#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Performance runner for d8.

Call e.g. with tools/run-perf.py --arch ia32 some_suite.json

The suite json format is expected to be:
{
  "path": <relative path chunks to perf resources and main file>,
  "owners": [<list of email addresses of benchmark owners (required)>],
  "name": <optional suite name, file name is default>,
  "archs": [<architecture name for which this suite is run>, ...],
  "binary": <name of binary to run, default "d8">,
  "flags": [<flag to d8>, ...],
  "test_flags": [<flag to the test file>, ...],
  "run_count": <how often will this suite run (optional)>,
  "run_count_XXX": <how often will this suite run for arch XXX (optional)>,
  "timeout": <how long test is allowed to run>,
  "timeout_XXX": <how long test is allowed run run for arch XXX>,
  "retry_count": <how many times to retry failures (in addition to first try)",
  "retry_count_XXX": <how many times to retry failures for arch XXX>
  "resources": [<js file to be moved to android device>, ...]
  "main": <main js perf runner file>,
  "results_regexp": <optional regexp>,
  "results_processor": <optional python results processor script>,
  "units": <the unit specification for the performance dashboard>,
  "process_size": <flag - collect maximum memory used by the process>,
  "tests": [
    {
      "name": <name of the trace>,
      "results_regexp": <optional more specific regexp>,
      "results_processor": <optional python results processor script>,
      "units": <the unit specification for the performance dashboard>,
      "process_size": <flag - collect maximum memory used by the process>,
    }, ...
  ]
}

The tests field can also nest other suites in arbitrary depth. A suite
with a "main" file is a leaf suite that can contain one more level of
tests.

A suite's results_regexp is expected to have one string place holder
"%s" for the trace name. A trace's results_regexp overwrites suite
defaults.

A suite's results_processor may point to an optional python script. If
specified, it is called after running the tests (with a path relative to the
suite level's path). It is expected to read the measurement's output text
on stdin and print the processed output to stdout.

The results_regexp will be applied to the processed output.

A suite without "tests" is considered a performance test itself.

Full example (suite with one runner):
{
  "path": ["."],
  "owners": ["username@chromium.org"],
  "flags": ["--expose-gc"],
  "test_flags": ["5"],
  "archs": ["ia32", "x64"],
  "run_count": 5,
  "run_count_ia32": 3,
  "main": "run.js",
  "results_regexp": "^%s: (.+)$",
  "units": "score",
  "tests": [
    {"name": "Richards"},
    {"name": "DeltaBlue"},
    {"name": "NavierStokes",
     "results_regexp": "^NavierStokes: (.+)$"}
  ]
}

Full example (suite with several runners):
{
  "path": ["."],
  "owners": ["username@chromium.org", "otherowner@google.com"],
  "flags": ["--expose-gc"],
  "archs": ["ia32", "x64"],
  "run_count": 5,
  "units": "score",
  "tests": [
    {"name": "Richards",
     "path": ["richards"],
     "main": "run.js",
     "run_count": 3,
     "results_regexp": "^Richards: (.+)$"},
    {"name": "NavierStokes",
     "path": ["navier_stokes"],
     "main": "run.js",
     "results_regexp": "^NavierStokes: (.+)$"}
  ]
}

Path pieces are concatenated. D8 is always run with the suite's path as cwd.

The test flags are passed to the js test file after '--'.
"""

# for py2/py3 compatibility
from __future__ import print_function
from functools import reduce

from collections import OrderedDict
import copy
import json
import logging
import math
import argparse
import os
import re
import subprocess
import sys
import time
import traceback

from testrunner.local import android
from testrunner.local import command
from testrunner.local import utils
from testrunner.objects.output import Output, NULL_OUTPUT

try:
  basestring       # Python 2
except NameError:  # Python 3
  basestring = str

SUPPORTED_ARCHS = ['arm',
                   'ia32',
                   'mips',
                   'mipsel',
                   'x64',
                   'arm64']

GENERIC_RESULTS_RE = re.compile(r'^RESULT ([^:]+): ([^=]+)= ([^ ]+) ([^ ]*)$')
RESULT_STDDEV_RE = re.compile(r'^\{([^\}]+)\}$')
RESULT_LIST_RE = re.compile(r'^\[([^\]]+)\]$')
TOOLS_BASE = os.path.abspath(os.path.dirname(__file__))
INFRA_FAILURE_RETCODE = 87


def GeometricMean(values):
  """Returns the geometric mean of a list of values.

  The mean is calculated using log to avoid overflow.
  """
  values = map(float, values)
  return str(math.exp(sum(map(math.log, values)) / len(values)))


class Results(object):
  """Place holder for result traces."""
  def __init__(self, traces=None, errors=None):
    self.traces = traces or []
    self.errors = errors or []
    # TODO(sergiyb): Deprecate self.timeouts/near_timeouts and compute them in
    # the recipe based on self.runnable_durations. Also cleanup RunnableConfig
    # by removing has_timeouts/has_near_timeouts there.
    self.timeouts = []
    self.near_timeouts = []  # > 90% of the max runtime
    self.runnable_durations = []

  def ToDict(self):
    return {
        'traces': self.traces,
        'errors': self.errors,
        'timeouts': self.timeouts,
        'near_timeouts': self.near_timeouts,
        'runnable_durations': self.runnable_durations,
    }

  def WriteToFile(self, file_name):
    with open(file_name, 'w') as f:
      f.write(json.dumps(self.ToDict()))

  def __add__(self, other):
    self.traces += other.traces
    self.errors += other.errors
    self.timeouts += other.timeouts
    self.near_timeouts += other.near_timeouts
    self.runnable_durations += other.runnable_durations
    return self

  def __str__(self):  # pragma: no cover
    return str(self.ToDict())


class Measurement(object):
  """Represents a series of results of one trace.

  The results are from repetitive runs of the same executable. They are
  gathered by repeated calls to ConsumeOutput.
  """
  def __init__(self, graphs, units, results_regexp, stddev_regexp):
    self.name = '/'.join(graphs)
    self.graphs = graphs
    self.units = units
    self.results_regexp = results_regexp
    self.stddev_regexp = stddev_regexp
    self.results = []
    self.errors = []
    self.stddev = ''

  def ConsumeOutput(self, output):
    try:
      result = re.search(self.results_regexp, output.stdout, re.M).group(1)
      self.results.append(str(float(result)))
    except ValueError:
      self.errors.append('Regexp "%s" returned a non-numeric for test %s.'
                         % (self.results_regexp, self.name))
    except:
      self.errors.append('Regexp "%s" did not match for test %s.'
                         % (self.results_regexp, self.name))

    try:
      if self.stddev_regexp and self.stddev:
        self.errors.append('Test %s should only run once since a stddev '
                           'is provided by the test.' % self.name)
      if self.stddev_regexp:
        self.stddev = re.search(
            self.stddev_regexp, output.stdout, re.M).group(1)
    except:
      self.errors.append('Regexp "%s" did not match for test %s.'
                         % (self.stddev_regexp, self.name))

  def GetResults(self):
    return Results([{
      'graphs': self.graphs,
      'units': self.units,
      'results': self.results,
      'stddev': self.stddev,
    }], self.errors)


class NullMeasurement(object):
  """Null object to avoid having extra logic for configurations that don't
  require secondary run, e.g. CI bots.
  """
  def ConsumeOutput(self, output):
    pass

  def GetResults(self):
    return Results()


def Unzip(iterable):
  left = []
  right = []
  for l, r in iterable:
    left.append(l)
    right.append(r)
  return lambda: iter(left), lambda: iter(right)


def RunResultsProcessor(results_processor, output, count):
  # Dummy pass through for null-runs.
  if output.stdout is None:
    return output

  # We assume the results processor is relative to the suite.
  assert os.path.exists(results_processor)
  p = subprocess.Popen(
      [sys.executable, results_processor],
      stdin=subprocess.PIPE,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
  )
  new_output = copy.copy(output)
  new_output.stdout, _ = p.communicate(input=output.stdout)
  logging.info('>>> Processed stdout (#%d):\n%s', count, output.stdout)
  return new_output


def AccumulateResults(
    graph_names, trace_configs, output_iter, perform_measurement, calc_total):
  """Iterates over the output of multiple benchmark reruns and accumulates
  results for a configured list of traces.

  Args:
    graph_names: List of names that configure the base path of the traces. E.g.
                 ['v8', 'Octane'].
    trace_configs: List of 'TraceConfig' instances. Each trace config defines
                   how to perform a measurement.
    output_iter: Iterator over the output of each test run.
    perform_measurement: Whether to actually run tests and perform measurements.
                         This is needed so that we reuse this script for both CI
                         and trybot, but want to ignore second run on CI without
                         having to spread this logic throughout the script.
    calc_total: Boolean flag to specify the calculation of a summary trace.
  Returns: A 'Results' object.
  """
  measurements = [
    trace.CreateMeasurement(perform_measurement) for trace in trace_configs]
  for output in output_iter():
    for measurement in measurements:
      measurement.ConsumeOutput(output)

  res = reduce(lambda r, m: r + m.GetResults(), measurements, Results())

  if not res.traces or not calc_total:
    return res

  # Assume all traces have the same structure.
  if len(set(map(lambda t: len(t['results']), res.traces))) != 1:
    res.errors.append('Not all traces have the same number of results.')
    return res

  # Calculate the geometric means for all traces. Above we made sure that
  # there is at least one trace and that the number of results is the same
  # for each trace.
  n_results = len(res.traces[0]['results'])
  total_results = [GeometricMean(t['results'][i] for t in res.traces)
                   for i in range(0, n_results)]
  res.traces.append({
    'graphs': graph_names + ['Total'],
    'units': res.traces[0]['units'],
    'results': total_results,
    'stddev': '',
  })
  return res


class Node(object):
  """Represents a node in the suite tree structure."""
  def __init__(self, *args):
    self._children = []

  def AppendChild(self, child):
    self._children.append(child)


class DefaultSentinel(Node):
  """Fake parent node with all default values."""
  def __init__(self, binary = 'd8'):
    super(DefaultSentinel, self).__init__()
    self.binary = binary
    self.run_count = 10
    self.timeout = 60
    self.retry_count = 0
    self.path = []
    self.graphs = []
    self.flags = []
    self.test_flags = []
    self.process_size = False
    self.resources = []
    self.results_processor = None
    self.results_regexp = None
    self.stddev_regexp = None
    self.units = 'score'
    self.total = False
    self.owners = []


class GraphConfig(Node):
  """Represents a suite definition.

  Can either be a leaf or an inner node that provides default values.
  """
  def __init__(self, suite, parent, arch):
    super(GraphConfig, self).__init__()
    self._suite = suite

    assert isinstance(suite.get('path', []), list)
    assert isinstance(suite.get('owners', []), list)
    assert isinstance(suite['name'], basestring)
    assert isinstance(suite.get('flags', []), list)
    assert isinstance(suite.get('test_flags', []), list)
    assert isinstance(suite.get('resources', []), list)

    # Accumulated values.
    self.path = parent.path[:] + suite.get('path', [])
    self.graphs = parent.graphs[:] + [suite['name']]
    self.flags = parent.flags[:] + suite.get('flags', [])
    self.test_flags = parent.test_flags[:] + suite.get('test_flags', [])
    self.owners = parent.owners[:] + suite.get('owners', [])

    # Values independent of parent node.
    self.resources = suite.get('resources', [])

    # Descrete values (with parent defaults).
    self.binary = suite.get('binary', parent.binary)
    self.run_count = suite.get('run_count', parent.run_count)
    self.run_count = suite.get('run_count_%s' % arch, self.run_count)
    self.retry_count = suite.get('retry_count', parent.retry_count)
    self.retry_count = suite.get('retry_count_%s' % arch, self.retry_count)
    self.timeout = suite.get('timeout', parent.timeout)
    self.timeout = suite.get('timeout_%s' % arch, self.timeout)
    self.units = suite.get('units', parent.units)
    self.total = suite.get('total', parent.total)
    self.results_processor = suite.get(
        'results_processor', parent.results_processor)
    self.process_size = suite.get('process_size', parent.process_size)

    # A regular expression for results. If the parent graph provides a
    # regexp and the current suite has none, a string place holder for the
    # suite name is expected.
    # TODO(machenbach): Currently that makes only sense for the leaf level.
    # Multiple place holders for multiple levels are not supported.
    if parent.results_regexp:
      regexp_default = parent.results_regexp % re.escape(suite['name'])
    else:
      regexp_default = None
    self.results_regexp = suite.get('results_regexp', regexp_default)

    # A similar regular expression for the standard deviation (optional).
    if parent.stddev_regexp:
      stddev_default = parent.stddev_regexp % re.escape(suite['name'])
    else:
      stddev_default = None
    self.stddev_regexp = suite.get('stddev_regexp', stddev_default)


class TraceConfig(GraphConfig):
  """Represents a leaf in the suite tree structure."""
  def __init__(self, suite, parent, arch):
    super(TraceConfig, self).__init__(suite, parent, arch)
    assert self.results_regexp
    assert self.owners

  def CreateMeasurement(self, perform_measurement):
    if not perform_measurement:
      return NullMeasurement()

    return Measurement(
        self.graphs,
        self.units,
        self.results_regexp,
        self.stddev_regexp,
    )


class RunnableConfig(GraphConfig):
  """Represents a runnable suite definition (i.e. has a main file).
  """
  def __init__(self, suite, parent, arch):
    super(RunnableConfig, self).__init__(suite, parent, arch)
    self.has_timeouts = False
    self.has_near_timeouts = False

  @property
  def main(self):
    return self._suite.get('main', '')

  def PostProcess(self, outputs_iter):
    if self.results_processor:
      def it():
        for i, output in enumerate(outputs_iter()):
          yield RunResultsProcessor(self.results_processor, output, i + 1)
      return it
    else:
      return outputs_iter

  def ChangeCWD(self, suite_path):
    """Changes the cwd to to path defined in the current graph.

    The tests are supposed to be relative to the suite configuration.
    """
    suite_dir = os.path.abspath(os.path.dirname(suite_path))
    bench_dir = os.path.normpath(os.path.join(*self.path))
    os.chdir(os.path.join(suite_dir, bench_dir))

  def GetCommandFlags(self, extra_flags=None):
    suffix = ['--'] + self.test_flags if self.test_flags else []
    return self.flags + (extra_flags or []) + [self.main] + suffix

  def GetCommand(self, cmd_prefix, shell_dir, extra_flags=None):
    # TODO(machenbach): This requires +.exe if run on windows.
    extra_flags = extra_flags or []
    if self.binary != 'd8' and '--prof' in extra_flags:
      logging.info('Profiler supported only on a benchmark run with d8')

    if self.process_size:
      cmd_prefix = ['/usr/bin/time', '--format=MaxMemory: %MKB'] + cmd_prefix
    if self.binary.endswith('.py'):
      # Copy cmd_prefix instead of update (+=).
      cmd_prefix = cmd_prefix + [sys.executable]

    return command.Command(
        cmd_prefix=cmd_prefix,
        shell=os.path.join(shell_dir, self.binary),
        args=self.GetCommandFlags(extra_flags=extra_flags),
        timeout=self.timeout or 60)

  def Run(self, runner, trybot):
    """Iterates over several runs and handles the output for all traces."""
    output, output_secondary = Unzip(runner())
    return (
        AccumulateResults(
            self.graphs,
            self._children,
            output_iter=self.PostProcess(output),
            perform_measurement=True,
            calc_total=self.total,
        ),
        AccumulateResults(
            self.graphs,
            self._children,
            output_iter=self.PostProcess(output_secondary),
            perform_measurement=trybot,  # only run second time on trybots
            calc_total=self.total,
        ),
    )


class RunnableTraceConfig(TraceConfig, RunnableConfig):
  """Represents a runnable suite definition that is a leaf."""
  def __init__(self, suite, parent, arch):
    super(RunnableTraceConfig, self).__init__(suite, parent, arch)

  def Run(self, runner, trybot):
    """Iterates over several runs and handles the output."""
    measurement = self.CreateMeasurement(perform_measurement=True)
    measurement_secondary = self.CreateMeasurement(perform_measurement=trybot)
    for output, output_secondary in runner():
      measurement.ConsumeOutput(output)
      measurement_secondary.ConsumeOutput(output_secondary)
    return (
        measurement.GetResults(),
        measurement_secondary.GetResults(),
    )


def MakeGraphConfig(suite, arch, parent):
  """Factory method for making graph configuration objects."""
  if isinstance(parent, RunnableConfig):
    # Below a runnable can only be traces.
    return TraceConfig(suite, parent, arch)
  elif suite.get('main') is not None:
    # A main file makes this graph runnable. Empty strings are accepted.
    if suite.get('tests'):
      # This graph has subgraphs (traces).
      return RunnableConfig(suite, parent, arch)
    else:
      # This graph has no subgraphs, it's a leaf.
      return RunnableTraceConfig(suite, parent, arch)
  elif suite.get('tests'):
    # This is neither a leaf nor a runnable.
    return GraphConfig(suite, parent, arch)
  else:  # pragma: no cover
    raise Exception('Invalid suite configuration.')


def BuildGraphConfigs(suite, arch, parent):
  """Builds a tree structure of graph objects that corresponds to the suite
  configuration.
  """

  # TODO(machenbach): Implement notion of cpu type?
  if arch not in suite.get('archs', SUPPORTED_ARCHS):
    return None

  graph = MakeGraphConfig(suite, arch, parent)
  for subsuite in suite.get('tests', []):
    BuildGraphConfigs(subsuite, arch, graph)
  parent.AppendChild(graph)
  return graph


def FlattenRunnables(node, node_cb):
  """Generator that traverses the tree structure and iterates over all
  runnables.
  """
  node_cb(node)
  if isinstance(node, RunnableConfig):
    yield node
  elif isinstance(node, Node):
    for child in node._children:
      for result in FlattenRunnables(child, node_cb):
        yield result
  else:  # pragma: no cover
    raise Exception('Invalid suite configuration.')


class Platform(object):
  def __init__(self, args):
    self.shell_dir = args.shell_dir
    self.shell_dir_secondary = args.shell_dir_secondary
    self.extra_flags = args.extra_flags.split()
    self.args = args

  @staticmethod
  def ReadBuildConfig(args):
    config_path = os.path.join(args.shell_dir, 'v8_build_config.json')
    if not os.path.isfile(config_path):
      return {}
    with open(config_path) as f:
      return json.load(f)

  @staticmethod
  def GetPlatform(args):
    if Platform.ReadBuildConfig(args).get('is_android', False):
      return AndroidPlatform(args)
    else:
      return DesktopPlatform(args)

  def _Run(self, runnable, count, secondary=False):
    raise NotImplementedError()  # pragma: no cover

  def _LoggedRun(self, runnable, count, secondary=False):
    suffix = ' - secondary' if secondary else ''
    title = '>>> %%s (#%d)%s:' % ((count + 1), suffix)
    try:
      output = self._Run(runnable, count, secondary)
    except OSError:
      logging.exception(title % 'OSError')
      raise
    if output.duration > 0.9 * runnable.timeout:
      runnable.has_near_timeouts = True
    if output.stdout:
      logging.info(title % 'Stdout' + '\n%s', output.stdout)
    if output.stderr:  # pragma: no cover
      # Print stderr for debugging.
      logging.info(title % 'Stderr' + '\n%s', output.stderr)
    if output.timed_out:
      runnable.has_timeouts = True
      logging.warning('>>> Test timed out after %ss.', runnable.timeout)
    if output.exit_code != 0:
      logging.warning('>>> Test crashed with exit code %d.', output.exit_code)
    return output

  def Run(self, runnable, count):
    """Execute the benchmark's main file.

    If args.shell_dir_secondary is specified, the benchmark is run twice, e.g.
    with and without patch.
    Args:
      runnable: A Runnable benchmark instance.
      count: The number of this (repeated) run.
    Returns: A tuple with the two benchmark outputs. The latter will be None if
             args.shell_dir_secondary was not specified.
    """
    output = self._LoggedRun(runnable, count, secondary=False)
    if self.shell_dir_secondary:
      return output, self._LoggedRun(runnable, count, secondary=True)
    else:
      return output, NULL_OUTPUT


class DesktopPlatform(Platform):
  def __init__(self, args):
    super(DesktopPlatform, self).__init__(args)
    self.command_prefix = []

    # Setup command class to OS specific version.
    command.setup(utils.GuessOS(), args.device)

    if args.prioritize or args.affinitize != None:
      self.command_prefix = ['schedtool']
      if args.prioritize:
        self.command_prefix += ['-n', '-20']
      if args.affinitize != None:
      # schedtool expects a bit pattern when setting affinity, where each
      # bit set to '1' corresponds to a core where the process may run on.
      # First bit corresponds to CPU 0. Since the 'affinitize' parameter is
      # a core number, we need to map to said bit pattern.
        cpu = int(args.affinitize)
        core = 1 << cpu
        self.command_prefix += ['-a', ('0x%x' % core)]
      self.command_prefix += ['-e']

  def PreExecution(self):
    pass

  def PostExecution(self):
    pass

  def PreTests(self, node, path):
    if isinstance(node, RunnableConfig):
      node.ChangeCWD(path)

  def _Run(self, runnable, count, secondary=False):
    shell_dir = self.shell_dir_secondary if secondary else self.shell_dir
    cmd = runnable.GetCommand(self.command_prefix, shell_dir, self.extra_flags)
    output = cmd.execute()

    if output.IsSuccess() and '--prof' in self.extra_flags:
      os_prefix = {'linux': 'linux', 'macos': 'mac'}.get(utils.GuessOS())
      if os_prefix:
        tick_tools = os.path.join(TOOLS_BASE, '%s-tick-processor' % os_prefix)
        subprocess.check_call(tick_tools + ' --only-summary', shell=True)
      else:  # pragma: no cover
        logging.warning(
            'Profiler option currently supported on Linux and Mac OS.')

    # /usr/bin/time outputs to stderr
    if runnable.process_size:
      output.stdout += output.stderr
    return output


class AndroidPlatform(Platform):  # pragma: no cover

  def __init__(self, args):
    super(AndroidPlatform, self).__init__(args)
    self.driver = android.android_driver(args.device)

  def PreExecution(self):
    self.driver.set_high_perf_mode()

  def PostExecution(self):
    self.driver.set_default_perf_mode()
    self.driver.tear_down()

  def PreTests(self, node, path):
    if isinstance(node, RunnableConfig):
      node.ChangeCWD(path)
    suite_dir = os.path.abspath(os.path.dirname(path))
    if node.path:
      bench_rel = os.path.normpath(os.path.join(*node.path))
      bench_abs = os.path.join(suite_dir, bench_rel)
    else:
      bench_rel = '.'
      bench_abs = suite_dir

    self.driver.push_executable(self.shell_dir, 'bin', node.binary)
    if self.shell_dir_secondary:
      self.driver.push_executable(
          self.shell_dir_secondary, 'bin_secondary', node.binary)

    if isinstance(node, RunnableConfig):
      self.driver.push_file(bench_abs, node.main, bench_rel)
    for resource in node.resources:
      self.driver.push_file(bench_abs, resource, bench_rel)

  def _Run(self, runnable, count, secondary=False):
    target_dir = 'bin_secondary' if secondary else 'bin'
    self.driver.drop_ram_caches()

    # Relative path to benchmark directory.
    if runnable.path:
      bench_rel = os.path.normpath(os.path.join(*runnable.path))
    else:
      bench_rel = '.'

    logcat_file = None
    if self.args.dump_logcats_to:
      runnable_name = '-'.join(runnable.graphs)
      logcat_file = os.path.join(
          self.args.dump_logcats_to, 'logcat-%s-#%d%s.log' % (
            runnable_name, count + 1, '-secondary' if secondary else ''))
      logging.debug('Dumping logcat into %s', logcat_file)

    output = Output()
    start = time.time()
    try:
      output.stdout = self.driver.run(
          target_dir=target_dir,
          binary=runnable.binary,
          args=runnable.GetCommandFlags(self.extra_flags),
          rel_path=bench_rel,
          timeout=runnable.timeout,
          logcat_file=logcat_file,
      )
    except android.CommandFailedException as e:
      output.stdout = e.output
      output.exit_code = e.status
    except android.TimeoutException as e:
      output.stdout = e.output
      output.timed_out = True
    if runnable.process_size:
      output.stdout += 'MaxMemory: Unsupported'
    output.duration = time.time() - start
    return output

class CustomMachineConfiguration:
  def __init__(self, disable_aslr = False, governor = None):
    self.aslr_backup = None
    self.governor_backup = None
    self.disable_aslr = disable_aslr
    self.governor = governor

  def __enter__(self):
    if self.disable_aslr:
      self.aslr_backup = CustomMachineConfiguration.GetASLR()
      CustomMachineConfiguration.SetASLR(0)
    if self.governor != None:
      self.governor_backup = CustomMachineConfiguration.GetCPUGovernor()
      CustomMachineConfiguration.SetCPUGovernor(self.governor)
    return self

  def __exit__(self, type, value, traceback):
    if self.aslr_backup != None:
      CustomMachineConfiguration.SetASLR(self.aslr_backup)
    if self.governor_backup != None:
      CustomMachineConfiguration.SetCPUGovernor(self.governor_backup)

  @staticmethod
  def GetASLR():
    try:
      with open('/proc/sys/kernel/randomize_va_space', 'r') as f:
        return int(f.readline().strip())
    except Exception:
      logging.exception('Failed to get current ASLR settings.')
      raise

  @staticmethod
  def SetASLR(value):
    try:
      with open('/proc/sys/kernel/randomize_va_space', 'w') as f:
        f.write(str(value))
    except Exception:
      logging.exception(
          'Failed to update ASLR to %s. Are we running under sudo?', value)
      raise

    new_value = CustomMachineConfiguration.GetASLR()
    if value != new_value:
      raise Exception('Present value is %s' % new_value)

  @staticmethod
  def GetCPUCoresRange():
    try:
      with open('/sys/devices/system/cpu/present', 'r') as f:
        indexes = f.readline()
        r = map(int, indexes.split('-'))
        if len(r) == 1:
          return range(r[0], r[0] + 1)
        return range(r[0], r[1] + 1)
    except Exception:
      logging.exception('Failed to retrieve number of CPUs.')
      raise

  @staticmethod
  def GetCPUPathForId(cpu_index):
    ret = '/sys/devices/system/cpu/cpu'
    ret += str(cpu_index)
    ret += '/cpufreq/scaling_governor'
    return ret

  @staticmethod
  def GetCPUGovernor():
    try:
      cpu_indices = CustomMachineConfiguration.GetCPUCoresRange()
      ret = None
      for cpu_index in cpu_indices:
        cpu_device = CustomMachineConfiguration.GetCPUPathForId(cpu_index)
        with open(cpu_device, 'r') as f:
          # We assume the governors of all CPUs are set to the same value
          val = f.readline().strip()
          if ret == None:
            ret = val
          elif ret != val:
            raise Exception('CPU cores have differing governor settings')
      return ret
    except Exception:
      logging.exception('Failed to get the current CPU governor. Is the CPU '
                        'governor disabled? Check BIOS.')
      raise

  @staticmethod
  def SetCPUGovernor(value):
    try:
      cpu_indices = CustomMachineConfiguration.GetCPUCoresRange()
      for cpu_index in cpu_indices:
        cpu_device = CustomMachineConfiguration.GetCPUPathForId(cpu_index)
        with open(cpu_device, 'w') as f:
          f.write(value)

    except Exception:
      logging.exception('Failed to change CPU governor to %s. Are we '
                        'running under sudo?', value)
      raise

    cur_value = CustomMachineConfiguration.GetCPUGovernor()
    if cur_value != value:
      raise Exception('Could not set CPU governor. Present value is %s'
                      % cur_value )

def Main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--arch',
                      help='The architecture to run tests for. Pass "auto" '
                      'to auto-detect.', default='x64',
                      choices=SUPPORTED_ARCHS + ['auto'])
  parser.add_argument('--buildbot',
                      help='Adapt to path structure used on buildbots and adds '
                      'timestamps/level to all logged status messages',
                      default=False, action='store_true')
  parser.add_argument('-d', '--device',
                      help='The device ID to run Android tests on. If not '
                      'given it will be autodetected.')
  parser.add_argument('--extra-flags',
                      help='Additional flags to pass to the test executable',
                      default='')
  parser.add_argument('--json-test-results',
                      help='Path to a file for storing json results.')
  parser.add_argument('--json-test-results-secondary',
                      help='Path to a file for storing json results from run '
                      'without patch or for reference build run.')
  parser.add_argument('--outdir', help='Base directory with compile output',
                      default='out')
  parser.add_argument('--outdir-secondary',
                      help='Base directory with compile output without patch '
                      'or for reference build')
  parser.add_argument('--binary-override-path',
                      help='JavaScript engine binary. By default, d8 under '
                      'architecture-specific build dir. '
                      'Not supported in conjunction with outdir-secondary.')
  parser.add_argument('--prioritize',
                      help='Raise the priority to nice -20 for the '
                      'benchmarking process.Requires Linux, schedtool, and '
                      'sudo privileges.', default=False, action='store_true')
  parser.add_argument('--affinitize',
                      help='Run benchmarking process on the specified core. '
                      'For example: --affinitize=0 will run the benchmark '
                      'process on core 0. --affinitize=3 will run the '
                      'benchmark process on core 3. Requires Linux, schedtool, '
                      'and sudo privileges.', default=None)
  parser.add_argument('--noaslr',
                      help='Disable ASLR for the duration of the benchmarked '
                      'process. Requires Linux and sudo privileges.',
                      default=False, action='store_true')
  parser.add_argument('--cpu-governor',
                      help='Set cpu governor to specified policy for the '
                      'duration of the benchmarked process. Typical options: '
                      '"powersave" for more stable results, or "performance" '
                      'for shorter completion time of suite, with potentially '
                      'more noise in results.')
  parser.add_argument('--filter',
                      help='Only run the benchmarks beginning with this '
                      'string. For example: '
                      '--filter=JSTests/TypedArrays/ will run only TypedArray '
                      'benchmarks from the JSTests suite.',
                      default='')
  parser.add_argument('--dump-logcats-to',
                      help='Writes logcat output from each test into specified '
                      'directory. Only supported for android targets.')
  parser.add_argument("--run-count", type=int, default=0,
                      help="Override the run count specified by the test "
                           "suite. The default 0 uses the suite's config.")
  parser.add_argument('suite', nargs='+', help='Path to the suite config file.')

  try:
    args = parser.parse_args(argv)
  except SystemExit:
    return INFRA_FAILURE_RETCODE

  logging.basicConfig(
      level=logging.INFO, format='%(asctime)s %(levelname)-8s  %(message)s')

  if args.arch == 'auto':  # pragma: no cover
    args.arch = utils.DefaultArch()
    if args.arch not in SUPPORTED_ARCHS:
      logging.error(
          'Auto-detected architecture "%s" is not supported.', args.arch)
      return INFRA_FAILURE_RETCODE

  if (args.json_test_results_secondary and
      not args.outdir_secondary):  # pragma: no cover
    logging.error('For writing secondary json test results, a secondary outdir '
                  'patch must be specified.')
    return INFRA_FAILURE_RETCODE

  workspace = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

  if args.buildbot:
    build_config = 'Release'
  else:
    build_config = '%s.release' % args.arch

  if args.binary_override_path == None:
    args.shell_dir = os.path.join(workspace, args.outdir, build_config)
    default_binary_name = 'd8'
  else:
    if not os.path.isfile(args.binary_override_path):
      logging.error('binary-override-path must be a file name')
      return INFRA_FAILURE_RETCODE
    if args.outdir_secondary:
      logging.error('specify either binary-override-path or outdir-secondary')
      return INFRA_FAILURE_RETCODE
    args.shell_dir = os.path.abspath(
        os.path.dirname(args.binary_override_path))
    default_binary_name = os.path.basename(args.binary_override_path)

  if args.outdir_secondary:
    args.shell_dir_secondary = os.path.join(
        workspace, args.outdir_secondary, build_config)
  else:
    args.shell_dir_secondary = None

  if args.json_test_results:
    args.json_test_results = os.path.abspath(args.json_test_results)

  if args.json_test_results_secondary:
    args.json_test_results_secondary = os.path.abspath(
        args.json_test_results_secondary)

  # Ensure all arguments have absolute path before we start changing current
  # directory.
  args.suite = map(os.path.abspath, args.suite)

  prev_aslr = None
  prev_cpu_gov = None
  platform = Platform.GetPlatform(args)

  results = Results()
  results_secondary = Results()
  # We use list here to allow modification in nested function below.
  have_failed_tests = [False]
  with CustomMachineConfiguration(governor = args.cpu_governor,
                                  disable_aslr = args.noaslr) as conf:
    for path in args.suite:
      if not os.path.exists(path):  # pragma: no cover
        results.errors.append('Configuration file %s does not exist.' % path)
        continue

      with open(path) as f:
        suite = json.loads(f.read())

      # If no name is given, default to the file name without .json.
      suite.setdefault('name', os.path.splitext(os.path.basename(path))[0])

      # Setup things common to one test suite.
      platform.PreExecution()

      # Build the graph/trace tree structure.
      default_parent = DefaultSentinel(default_binary_name)
      root = BuildGraphConfigs(suite, args.arch, default_parent)

      # Callback to be called on each node on traversal.
      def NodeCB(node):
        platform.PreTests(node, path)

      # Traverse graph/trace tree and iterate over all runnables.
      for runnable in FlattenRunnables(root, NodeCB):
        runnable_name = '/'.join(runnable.graphs)
        if (not runnable_name.startswith(args.filter) and
            runnable_name + '/' != args.filter):
          continue
        logging.info('>>> Running suite: %s', runnable_name)
        durations = []
        durations_secondary = []

        def Runner():
          """Output generator that reruns several times."""
          for i in range(0, max(1, args.run_count or runnable.run_count)):
            attempts_left = runnable.retry_count + 1
            while attempts_left:
              output, output_secondary = platform.Run(runnable, i)
              if output.IsSuccess() and output_secondary.IsSuccess():
                durations.append(output.duration)
                if output_secondary is not NULL_OUTPUT:
                  durations_secondary.append(output_secondary.duration)
                yield output, output_secondary
                break
              attempts_left -= 1
              if not attempts_left:  # ignore failures until last attempt
                have_failed_tests[0] = True
              else:
                logging.info('>>> Retrying suite: %s', runnable_name)

        # Let runnable iterate over all runs and handle output.
        result, result_secondary = runnable.Run(
          Runner, trybot=args.shell_dir_secondary)
        results += result
        results_secondary += result_secondary
        if runnable.has_timeouts:
          results.timeouts.append(runnable_name)
        if runnable.has_near_timeouts:
          results.near_timeouts.append(runnable_name)
        results.runnable_durations.append({
          'graphs': runnable.graphs,
          'durations': durations,
          'timeout': runnable.timeout,
        })
        if durations_secondary:
          results_secondary.runnable_durations.append({
            'graphs': runnable.graphs,
            'durations': durations_secondary,
            'timeout': runnable.timeout,
          })

      platform.PostExecution()

    if args.json_test_results:
      results.WriteToFile(args.json_test_results)
    else:  # pragma: no cover
      print(results)

  if args.json_test_results_secondary:
    results_secondary.WriteToFile(args.json_test_results_secondary)
  else:  # pragma: no cover
    print(results_secondary)

  if results.errors or have_failed_tests[0]:
    return 1

  return 0


def MainWrapper():
  try:
    return Main(sys.argv[1:])
  except:
    # Log uncaptured exceptions and report infra failure to the caller.
    traceback.print_exc()
    return INFRA_FAILURE_RETCODE


if __name__ == '__main__':  # pragma: no cover
  sys.exit(MainWrapper())
