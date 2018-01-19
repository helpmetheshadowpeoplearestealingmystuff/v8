#!/usr/bin/env python
#
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import multiprocessing
import random
import shlex
import sys

# Adds testrunner to the path hence it has to be imported at the beggining.
import base_runner

from testrunner.local import progress
from testrunner.local import utils
from testrunner.objects import context

from testrunner.testproc import fuzzer
from testrunner.testproc.base import TestProcProducer
from testrunner.testproc.execution import ExecutionProc
from testrunner.testproc.filter import StatusFileFilterProc, NameFilterProc
from testrunner.testproc.loader import LoadProc
from testrunner.testproc.progress import ResultsTracker, TestsCounter


DEFAULT_SUITES = ["mjsunit", "webkit", "benchmarks"]
TIMEOUT_DEFAULT = 60

# Double the timeout for these:
SLOW_ARCHS = ["arm",
              "mipsel"]


class NumFuzzer(base_runner.BaseTestRunner):
  def __init__(self, *args, **kwargs):
    super(NumFuzzer, self).__init__(*args, **kwargs)

  def _add_parser_options(self, parser):
    parser.add_option("--command-prefix",
                      help="Prepended to each shell command used to run a test",
                      default="")
    parser.add_option("--dump-results-file", help="Dump maximum limit reached")
    parser.add_option("--extra-flags",
                      help="Additional flags to pass to each test command",
                      default="")
    parser.add_option("--isolates", help="Whether to test isolates",
                      default=False, action="store_true")
    parser.add_option("-j", help="The number of parallel tasks to run",
                      default=0, type="int")
    parser.add_option("-p", "--progress",
                      help=("The style of progress indicator"
                            " (verbose, dots, color, mono)"),
                      choices=progress.PROGRESS_INDICATORS.keys(),
                      default="mono")
    parser.add_option("-t", "--timeout", help="Timeout in seconds",
                      default= -1, type="int")
    parser.add_option("--random-seed", default=0,
                      help="Default seed for initializing random generator")
    parser.add_option("--fuzzer-random-seed", default=0,
                      help="Default seed for initializing fuzzer random "
                      "generator")

    parser.add_option("--stress-marking", default=0, type="int",
                      help="probability [0-10] of adding --stress-marking "
                           "flag to the test")
    parser.add_option("--stress-scavenge", default=0, type="int",
                      help="probability [0-10] of adding --stress-scavenge "
                           "flag to the test")
    parser.add_option("--stress-compaction", default=0, type="int",
                      help="probability [0-10] of adding --stress-compaction "
                           "flag to the test")
    parser.add_option("--stress-gc", default=0, type="int",
                      help="probability [0-10] of adding --random-gc-interval "
                           "flag to the test")

    parser.add_option("--tests-count", default=5, type="int",
                      help="Number of tests to generate from each base test")
    parser.add_option("--total-timeout-sec", default=0, type="int",
                      help="How long should fuzzer run. It overrides "
                           "--tests-count")
    return parser


  def _process_options(self, options):
    # Special processing of other options, sorted alphabetically.
    options.command_prefix = shlex.split(options.command_prefix)
    options.extra_flags = shlex.split(options.extra_flags)
    if options.j == 0:
      options.j = multiprocessing.cpu_count()
    while options.random_seed == 0:
      options.random_seed = random.SystemRandom().randint(-2147483648,
                                                          2147483647)
    while options.fuzzer_random_seed == 0:
      options.fuzzer_random_seed = random.SystemRandom().randint(-2147483648,
                                                                 2147483647)
    return True

  def _get_default_suite_names(self):
    return DEFAULT_SUITES

  def _do_execute(self, suites, args, options):
    print(">>> Running tests for %s.%s" % (self.build_config.arch,
                                           self.mode_name))

    ctx = self._create_context(options)
    tests = self._load_tests(options, suites, ctx)
    progress_indicator = progress.PROGRESS_INDICATORS[options.progress]()

    loader = LoadProc()
    fuzzer_rng = random.Random(options.fuzzer_random_seed)
    fuzzer_proc = fuzzer.FuzzerProc(
        fuzzer_rng,
        options.tests_count,
        self._create_fuzzer_configs(options),
        options.total_timeout_sec,
    )

    results = ResultsTracker()
    execproc = ExecutionProc(options.j, ctx)
    indicator = progress_indicator.ToProgressIndicatorProc()
    procs = [
      loader,
      NameFilterProc(args) if args else None,
      StatusFileFilterProc(None, None),
      self._create_shard_proc(options),
      fuzzer_proc,
      results,
      indicator,
      execproc,
    ]
    self._prepare_procs(procs)
    loader.load_tests(tests)
    execproc.start()

    indicator.finished()

    print '>>> %d tests ran' % results.total
    if results.failed:
      print '>>> %d tests failed' % results.failed

    if results.failed:
      return 1
    if results.remaining:
      return 2
    return 0

  def _create_context(self, options):
    # Populate context object.
    timeout = options.timeout
    if timeout == -1:
      # Simulators are slow, therefore allow a longer default timeout.
      if self.build_config.arch in SLOW_ARCHS:
        timeout = 2 * TIMEOUT_DEFAULT;
      else:
        timeout = TIMEOUT_DEFAULT;

    timeout *= self.mode_options.timeout_scalefactor
    ctx = context.Context(self.build_config.arch,
                          self.mode_options.execution_mode,
                          self.outdir,
                          self.mode_options.flags, options.verbose,
                          timeout, options.isolates,
                          options.command_prefix,
                          options.extra_flags,
                          False,  # Keep i18n on by default.
                          options.random_seed,
                          True,  # No sorting of test cases.
                          0,  # Don't rerun failing tests.
                          0,  # No use of a rerun-failing-tests maximum.
                          False,  # No no_harness mode.
                          False,  # Don't use perf data.
                          False)  # Coverage not supported.
    return ctx

  def _load_tests(self, options, suites, ctx):
    # Find available test suites and read test cases from them.
    variables = {
      "arch": self.build_config.arch,
      "asan": self.build_config.asan,
      "byteorder": sys.byteorder,
      "dcheck_always_on": self.build_config.dcheck_always_on,
      "deopt_fuzzer": False,
      "gc_fuzzer": True,
      "gc_stress": True,
      "gcov_coverage": self.build_config.gcov_coverage,
      "isolates": options.isolates,
      "mode": self.mode_options.status_mode,
      "msan": self.build_config.msan,
      "no_harness": False,
      "no_i18n": self.build_config.no_i18n,
      "no_snap": self.build_config.no_snap,
      "novfp3": False,
      "predictable": self.build_config.predictable,
      "simulator": utils.UseSimulator(self.build_config.arch),
      "simulator_run": False,
      "system": utils.GuessOS(),
      "tsan": self.build_config.tsan,
      "ubsan_vptr": self.build_config.ubsan_vptr,
    }

    tests = []
    for s in suites:
      s.ReadStatusFile(variables)
      s.ReadTestCases(ctx)
      tests += s.tests
    return tests

  def _prepare_procs(self, procs):
    procs = filter(None, procs)
    for i in xrange(0, len(procs) - 1):
      procs[i].connect_to(procs[i + 1])
    procs[0].setup()

  def _create_fuzzer_configs(self, options):
    fuzzers = []
    if options.stress_compaction:
      fuzzers.append(fuzzer.create_compaction_config(options.stress_compaction))
    if options.stress_marking:
      fuzzers.append(fuzzer.create_marking_config(options.stress_marking))
    if options.stress_scavenge:
      fuzzers.append(fuzzer.create_scavenge_config(options.stress_scavenge))
    if options.stress_gc:
      fuzzers.append(fuzzer.create_gc_interval_config(options.stress_gc))
    return fuzzers


if __name__ == '__main__':
  sys.exit(NumFuzzer().execute())
