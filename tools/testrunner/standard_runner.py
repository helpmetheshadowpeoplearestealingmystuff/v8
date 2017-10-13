#!/usr/bin/env python
#
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from collections import OrderedDict
from os.path import getmtime, isdir, join
import itertools
import json
import multiprocessing
import optparse
import os
import platform
import random
import shlex
import subprocess
import sys
import time

# Adds testrunner to the path hence it has to be imported at the beggining.
import base_runner

from testrunner.local import execution
from testrunner.local import progress
from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.local import verbose
from testrunner.local.variants import ALL_VARIANTS
from testrunner.network import network_execution
from testrunner.objects import context


# Base dir of the v8 checkout to be used as cwd.
BASE_DIR = (
    os.path.dirname(
      os.path.dirname(
        os.path.dirname(
          os.path.abspath(__file__)))))

DEFAULT_OUT_GN = "out.gn"

ARCH_GUESS = utils.DefaultArch()

# Map of test name synonyms to lists of test suites. Should be ordered by
# expected runtimes (suites with slow test cases first). These groups are
# invoked in separate steps on the bots.
TEST_MAP = {
  # This needs to stay in sync with test/bot_default.isolate.
  "bot_default": [
    "debugger",
    "mjsunit",
    "cctest",
    "wasm-spec-tests",
    "inspector",
    "webkit",
    "mkgrokdump",
    "fuzzer",
    "message",
    "preparser",
    "intl",
    "unittests",
  ],
  # This needs to stay in sync with test/default.isolate.
  "default": [
    "debugger",
    "mjsunit",
    "cctest",
    "wasm-spec-tests",
    "inspector",
    "mkgrokdump",
    "fuzzer",
    "message",
    "preparser",
    "intl",
    "unittests",
  ],
  # This needs to stay in sync with test/optimize_for_size.isolate.
  "optimize_for_size": [
    "debugger",
    "mjsunit",
    "cctest",
    "inspector",
    "webkit",
    "intl",
  ],
  "unittests": [
    "unittests",
  ],
}

TIMEOUT_DEFAULT = 60

# Variants ordered by expected runtime (slowest first).
VARIANTS = ["default"]

MORE_VARIANTS = [
  "stress",
  "stress_incremental_marking",
  "nooptimization",
  "stress_asm_wasm",
  "wasm_traps",
]

EXHAUSTIVE_VARIANTS = MORE_VARIANTS + VARIANTS

VARIANT_ALIASES = {
  # The default for developer workstations.
  "dev": VARIANTS,
  # Additional variants, run on all bots.
  "more": MORE_VARIANTS,
  # TODO(machenbach): Deprecate this after the step is removed on infra side.
  # Additional variants, run on a subset of bots.
  "extra": [],
}

DEBUG_FLAGS = ["--nohard-abort", "--enable-slow-asserts", "--verify-heap"]
RELEASE_FLAGS = ["--nohard-abort"]

MODES = {
  "debug": {
    "flags": DEBUG_FLAGS,
    "timeout_scalefactor": 4,
    "status_mode": "debug",
    "execution_mode": "debug",
    "output_folder": "debug",
  },
  "optdebug": {
    "flags": DEBUG_FLAGS,
    "timeout_scalefactor": 4,
    "status_mode": "debug",
    "execution_mode": "debug",
    "output_folder": "optdebug",
  },
  "release": {
    "flags": RELEASE_FLAGS,
    "timeout_scalefactor": 1,
    "status_mode": "release",
    "execution_mode": "release",
    "output_folder": "release",
  },
  # Normal trybot release configuration. There, dchecks are always on which
  # implies debug is set. Hence, the status file needs to assume debug-like
  # behavior/timeouts.
  "tryrelease": {
    "flags": RELEASE_FLAGS,
    "timeout_scalefactor": 1,
    "status_mode": "debug",
    "execution_mode": "release",
    "output_folder": "release",
  },
  # This mode requires v8 to be compiled with dchecks and slow dchecks.
  "slowrelease": {
    "flags": RELEASE_FLAGS + ["--enable-slow-asserts"],
    "timeout_scalefactor": 2,
    "status_mode": "debug",
    "execution_mode": "release",
    "output_folder": "release",
  },
}

GC_STRESS_FLAGS = ["--gc-interval=500", "--stress-compaction",
                   "--concurrent-recompilation-queue-length=64",
                   "--concurrent-recompilation-delay=500",
                   "--concurrent-recompilation"]

SUPPORTED_ARCHS = ["android_arm",
                   "android_arm64",
                   "android_ia32",
                   "android_x64",
                   "arm",
                   "ia32",
                   "mips",
                   "mipsel",
                   "mips64",
                   "mips64el",
                   "s390",
                   "s390x",
                   "ppc",
                   "ppc64",
                   "x64",
                   "x32",
                   "arm64"]
# Double the timeout for these:
SLOW_ARCHS = ["android_arm",
              "android_arm64",
              "android_ia32",
              "android_x64",
              "arm",
              "mips",
              "mipsel",
              "mips64",
              "mips64el",
              "s390",
              "s390x",
              "arm64"]


class StandardTestRunner(base_runner.BaseTestRunner):
    def __init__(self):
        super(StandardTestRunner, self).__init__()

    def _do_execute(self):
      # Use the v8 root as cwd as some test cases use "load" with relative
      # paths.
      os.chdir(BASE_DIR)

      parser = self._build_options()
      (options, args) = parser.parse_args()
      if not self._process_options(options):
        parser.print_help()
        return 1
      self._setup_env(options)

      if options.swarming:
        # Swarming doesn't print how isolated commands are called. Lets make
        # this less cryptic by printing it ourselves.
        print ' '.join(sys.argv)

      exit_code = 0

      suite_paths = utils.GetSuitePaths(join(BASE_DIR, "test"))

      # Use default tests if no test configuration was provided at the cmd line.
      if len(args) == 0:
        args = ["default"]

      # Expand arguments with grouped tests. The args should reflect the list
      # of suites as otherwise filters would break.
      def ExpandTestGroups(name):
        if name in TEST_MAP:
          return [suite for suite in TEST_MAP[name]]
        else:
          return [name]
      args = reduce(lambda x, y: x + y,
            [ExpandTestGroups(arg) for arg in args],
            [])

      args_suites = OrderedDict() # Used as set
      for arg in args:
        args_suites[arg.split('/')[0]] = True
      suite_paths = [ s for s in args_suites if s in suite_paths ]

      suites = []
      for root in suite_paths:
        suite = testsuite.TestSuite.LoadTestSuite(
            os.path.join(BASE_DIR, "test", root))
        if suite:
          suites.append(suite)

      for s in suites:
        s.PrepareSources()

      for (arch, mode) in options.arch_and_mode:
        try:
          code = self._execute(arch, mode, args, options, suites)
        except KeyboardInterrupt:
          return 2
        exit_code = exit_code or code
      return exit_code

    def _build_options(self):
      result = optparse.OptionParser()
      result.usage = '%prog [options] [tests]'
      result.description = """TESTS: %s""" % (TEST_MAP["default"])
      result.add_option("--arch",
                        help=("The architecture to run tests for, "
                              "'auto' or 'native' for auto-detect: %s" %
                              SUPPORTED_ARCHS))
      result.add_option("--arch-and-mode",
                        help="Architecture and mode in the format 'arch.mode'")
      result.add_option("--asan",
                        help="Regard test expectations for ASAN",
                        default=False, action="store_true")
      result.add_option("--sancov-dir",
                        help="Directory where to collect coverage data")
      result.add_option("--cfi-vptr",
                        help="Run tests with UBSAN cfi_vptr option.",
                        default=False, action="store_true")
      result.add_option("--buildbot",
                        help="Adapt to path structure used on buildbots",
                        default=False, action="store_true")
      result.add_option("--dcheck-always-on",
                        help="Indicates that V8 was compiled with DCHECKs"
                        " enabled",
                        default=False, action="store_true")
      result.add_option("--novfp3",
                        help="Indicates that V8 was compiled without VFP3"
                        " support",
                        default=False, action="store_true")
      result.add_option("--cat", help="Print the source of the tests",
                        default=False, action="store_true")
      result.add_option("--slow-tests",
                        help="Regard slow tests (run|skip|dontcare)",
                        default="dontcare")
      result.add_option("--pass-fail-tests",
                        help="Regard pass|fail tests (run|skip|dontcare)",
                        default="dontcare")
      result.add_option("--gc-stress",
                        help="Switch on GC stress mode",
                        default=False, action="store_true")
      result.add_option("--gcov-coverage",
                        help="Uses executables instrumented for gcov coverage",
                        default=False, action="store_true")
      result.add_option("--command-prefix",
                        help="Prepended to each shell command used to run a"
                        " test",
                        default="")
      result.add_option("--extra-flags",
                        help="Additional flags to pass to each test command",
                        action="append", default=[])
      result.add_option("--isolates", help="Whether to test isolates",
                        default=False, action="store_true")
      result.add_option("-j", help="The number of parallel tasks to run",
                        default=0, type="int")
      result.add_option("-m", "--mode",
                        help="The test modes in which to run (comma-separated,"
                        " uppercase for ninja and buildbot builds): %s"
                        % MODES.keys())
      result.add_option("--no-harness", "--noharness",
                        help="Run without test harness of a given suite",
                        default=False, action="store_true")
      result.add_option("--no-i18n", "--noi18n",
                        help="Skip internationalization tests",
                        default=False, action="store_true")
      result.add_option("--network", help="Distribute tests on the network",
                        default=False, dest="network", action="store_true")
      result.add_option("--no-network", "--nonetwork",
                        help="Don't distribute tests on the network",
                        dest="network", action="store_false")
      result.add_option("--no-presubmit", "--nopresubmit",
                        help='Skip presubmit checks (deprecated)',
                        default=False, dest="no_presubmit", action="store_true")
      result.add_option("--no-snap", "--nosnap",
                        help='Test a build compiled without snapshot.',
                        default=False, dest="no_snap", action="store_true")
      result.add_option("--no-sorting", "--nosorting",
                        help="Don't sort tests according to duration of last"
                        " run.",
                        default=False, dest="no_sorting", action="store_true")
      result.add_option("--no-variants", "--novariants",
                        help="Don't run any testing variants",
                        default=False, dest="no_variants", action="store_true")
      result.add_option("--variants",
                        help="Comma-separated list of testing variants;"
                        " default: \"%s\"" % ",".join(VARIANTS))
      result.add_option("--exhaustive-variants",
                        default=False, action="store_true",
                        help="Use exhaustive set of default variants:"
                        " \"%s\"" % ",".join(EXHAUSTIVE_VARIANTS))
      result.add_option("--outdir", help="Base directory with compile output",
                        default="out")
      result.add_option("--gn", help="Scan out.gn for the last built"
                        " configuration",
                        default=False, action="store_true")
      result.add_option("--predictable",
                        help="Compare output of several reruns of each test",
                        default=False, action="store_true")
      result.add_option("-p", "--progress",
                        help=("The style of progress indicator"
                              " (verbose, dots, color, mono)"),
                        choices=progress.PROGRESS_INDICATORS.keys(),
                        default="mono")
      result.add_option("--quickcheck", default=False, action="store_true",
                        help=("Quick check mode (skip slow tests)"))
      result.add_option("--report", help="Print a summary of the tests to be"
                        " run",
                        default=False, action="store_true")
      result.add_option("--json-test-results",
                        help="Path to a file for storing json results.")
      result.add_option("--flakiness-results",
                        help="Path to a file for storing flakiness json.")
      result.add_option("--rerun-failures-count",
                        help=("Number of times to rerun each failing test case."
                              " Very slow tests will be rerun only once."),
                        default=0, type="int")
      result.add_option("--rerun-failures-max",
                        help="Maximum number of failing test cases to rerun.",
                        default=100, type="int")
      result.add_option("--shard-count",
                        help="Split testsuites into this number of shards",
                        default=1, type="int")
      result.add_option("--shard-run",
                        help="Run this shard from the split up tests.",
                        default=1, type="int")
      result.add_option("--shell", help="DEPRECATED! use --shell-dir",
                        default="")
      result.add_option("--shell-dir", help="Directory containing executables",
                        default="")
      result.add_option("--dont-skip-slow-simulator-tests",
                        help="Don't skip more slow tests when using a"
                        " simulator.",
                        default=False, action="store_true",
                        dest="dont_skip_simulator_slow_tests")
      result.add_option("--swarming",
                        help="Indicates running test driver on swarming.",
                        default=False, action="store_true")
      result.add_option("--time", help="Print timing information after running",
                        default=False, action="store_true")
      result.add_option("-t", "--timeout", help="Timeout in seconds",
                        default=TIMEOUT_DEFAULT, type="int")
      result.add_option("--tsan",
                        help="Regard test expectations for TSAN",
                        default=False, action="store_true")
      result.add_option("-v", "--verbose", help="Verbose output",
                        default=False, action="store_true")
      result.add_option("--valgrind", help="Run tests through valgrind",
                        default=False, action="store_true")
      result.add_option("--warn-unused", help="Report unused rules",
                        default=False, action="store_true")
      result.add_option("--junitout", help="File name of the JUnit output")
      result.add_option("--junittestsuite",
                        help="The testsuite name in the JUnit output file",
                        default="v8tests")
      result.add_option("--random-seed", default=0, dest="random_seed",
                        help="Default seed for initializing random generator",
                        type=int)
      result.add_option("--random-seed-stress-count", default=1, type="int",
                        dest="random_seed_stress_count",
                        help="Number of runs with different random seeds")
      result.add_option("--ubsan-vptr",
                        help="Regard test expectations for UBSanVptr",
                        default=False, action="store_true")
      result.add_option("--msan",
                        help="Regard test expectations for UBSanVptr",
                        default=False, action="store_true")
      return result

    def _process_options(self, options):
      global VARIANTS

      # First try to auto-detect configurations based on the build if GN was
      # used. This can't be overridden by cmd-line arguments.
      options.auto_detect = False
      if options.gn:
        gn_out_dir = os.path.join(BASE_DIR, DEFAULT_OUT_GN)
        latest_timestamp = -1
        latest_config = None
        for gn_config in os.listdir(gn_out_dir):
          gn_config_dir = os.path.join(gn_out_dir, gn_config)
          if not isdir(gn_config_dir):
            continue
          if os.path.getmtime(gn_config_dir) > latest_timestamp:
            latest_timestamp = os.path.getmtime(gn_config_dir)
            latest_config = gn_config
        if latest_config:
          print(">>> Latest GN build found is %s" % latest_config)
          options.outdir = os.path.join(DEFAULT_OUT_GN, latest_config)

      if options.buildbot:
        build_config_path = os.path.join(
            BASE_DIR, options.outdir, options.mode, "v8_build_config.json")
      else:
        build_config_path = os.path.join(
            BASE_DIR, options.outdir, "v8_build_config.json")

      # Auto-detect test configurations based on the build (GN only).
      if os.path.exists(build_config_path):
        try:
          with open(build_config_path) as f:
            build_config = json.load(f)
        except Exception:
          print ("%s exists but contains invalid json. "
                 "Is your build up-to-date?" % build_config_path)
          return False
        options.auto_detect = True

        # In auto-detect mode the outdir is always where we found the build
        # config.
        # This ensures that we'll also take the build products from there.
        options.outdir = os.path.dirname(build_config_path)
        options.arch_and_mode = None
        if options.mode:
          # In auto-detect mode we don't use the mode for more path-magic.
          # Therefore transform the buildbot mode here to fit to the GN build
          # config.
          options.mode = self._buildbot_to_v8_mode(options.mode)

        # In V8 land, GN's x86 is called ia32.
        if build_config["v8_target_cpu"] == "x86":
          build_config["v8_target_cpu"] = "ia32"

        # Update options based on the build config. Sanity check that we're not
        # trying to use inconsistent options.
        for param, value in (
            ('arch', build_config["v8_target_cpu"]),
            ('asan', build_config["is_asan"]),
            ('dcheck_always_on', build_config["dcheck_always_on"]),
            ('gcov_coverage', build_config["is_gcov_coverage"]),
            ('mode', 'debug' if build_config["is_debug"] else 'release'),
            ('msan', build_config["is_msan"]),
            ('no_i18n', not build_config["v8_enable_i18n_support"]),
            ('no_snap', not build_config["v8_use_snapshot"]),
            ('tsan', build_config["is_tsan"]),
            ('ubsan_vptr', build_config["is_ubsan_vptr"])):
          cmd_line_value = getattr(options, param)
          if (cmd_line_value not in [None, True, False] and
              cmd_line_value != value):
            # TODO(machenbach): This is for string options only. Requires
            # options  to not have default values. We should make this more
            # modular and implement it in our own version of the option parser.
            print "Attempted to set %s to %s, while build is %s." % (
                param, cmd_line_value, value)
            return False
          if cmd_line_value == True and value == False:
            print "Attempted to turn on %s, but it's not available." % (
                param)
            return False
          if cmd_line_value != value:
            print ">>> Auto-detected %s=%s" % (param, value)
          setattr(options, param, value)

      else:
        # Non-GN build without auto-detect. Set default values for missing
        # parameters.
        if not options.mode:
          options.mode = "release,debug"
        if not options.arch:
          options.arch = "ia32,x64,arm"

      # Architecture and mode related stuff.
      if options.arch_and_mode:
        options.arch_and_mode = [arch_and_mode.split(".")
            for arch_and_mode in options.arch_and_mode.split(",")]
        options.arch = ",".join([tokens[0] for tokens in options.arch_and_mode])
        options.mode = ",".join([tokens[1] for tokens in options.arch_and_mode])
      options.mode = options.mode.split(",")
      for mode in options.mode:
        if not self._buildbot_to_v8_mode(mode) in MODES:
          print "Unknown mode %s" % mode
          return False
      if options.arch in ["auto", "native"]:
        options.arch = ARCH_GUESS
      options.arch = options.arch.split(",")
      for arch in options.arch:
        if not arch in SUPPORTED_ARCHS:
          print "Unknown architecture %s" % arch
          return False

      # Store the final configuration in arch_and_mode list. Don't overwrite
      # predefined arch_and_mode since it is more expressive than arch and mode.
      if not options.arch_and_mode:
        options.arch_and_mode = itertools.product(options.arch, options.mode)

      # Special processing of other options, sorted alphabetically.

      if options.buildbot:
        options.network = False
      if options.command_prefix and options.network:
        print("Specifying --command-prefix disables network distribution, "
              "running tests locally.")
        options.network = False
      options.command_prefix = shlex.split(options.command_prefix)
      options.extra_flags = sum(map(shlex.split, options.extra_flags), [])

      if options.gc_stress:
        options.extra_flags += GC_STRESS_FLAGS

      if options.asan:
        options.extra_flags.append("--invoke-weak-callbacks")
        options.extra_flags.append("--omit-quit")

      if options.novfp3:
        options.extra_flags.append("--noenable-vfp3")

      if options.exhaustive_variants:
        # This is used on many bots. It includes a larger set of default
        # variants.
        # Other options for manipulating variants still apply afterwards.
        VARIANTS = EXHAUSTIVE_VARIANTS

      # TODO(machenbach): Figure out how to test a bigger subset of variants on
      # msan.
      if options.msan:
        VARIANTS = ["default"]

      if options.j == 0:
        options.j = multiprocessing.cpu_count()

      if options.random_seed_stress_count <= 1 and options.random_seed == 0:
        options.random_seed = self._random_seed()

      def excl(*args):
        """Returns true if zero or one of multiple arguments are true."""
        return reduce(lambda x, y: x + y, args) <= 1

      if not excl(options.no_variants, bool(options.variants)):
        print("Use only one of --no-variants or --variants.")
        return False
      if options.quickcheck:
        VARIANTS = ["default", "stress"]
        options.slow_tests = "skip"
        options.pass_fail_tests = "skip"
      if options.no_variants:
        VARIANTS = ["default"]
      if options.variants:
        VARIANTS = options.variants.split(",")

        # Resolve variant aliases.
        VARIANTS = reduce(
            list.__add__,
            (VARIANT_ALIASES.get(v, [v]) for v in VARIANTS),
            [],
        )

        if not set(VARIANTS).issubset(ALL_VARIANTS):
          print "All variants must be in %s" % str(ALL_VARIANTS)
          return False
      if options.predictable:
        VARIANTS = ["default"]
        options.extra_flags.append("--predictable")
        options.extra_flags.append("--verify_predictable")
        options.extra_flags.append("--no-inline-new")

      # Dedupe.
      VARIANTS = list(set(VARIANTS))

      if not options.shell_dir:
        if options.shell:
          print "Warning: --shell is deprecated, use --shell-dir instead."
          options.shell_dir = os.path.dirname(options.shell)
      if options.valgrind:
        run_valgrind = os.path.join("tools", "run-valgrind.py")
        # This is OK for distributed running, so we don't need to disable
        # network.
        options.command_prefix = (["python", "-u", run_valgrind] +
                                  options.command_prefix)
      def CheckTestMode(name, option):
        if not option in ["run", "skip", "dontcare"]:
          print "Unknown %s mode %s" % (name, option)
          return False
        return True
      if not CheckTestMode("slow test", options.slow_tests):
        return False
      if not CheckTestMode("pass|fail test", options.pass_fail_tests):
        return False
      if options.no_i18n:
        TEST_MAP["bot_default"].remove("intl")
        TEST_MAP["default"].remove("intl")
      return True

    def _setup_env(self, options):
      """Setup additional environment variables."""

      # Many tests assume an English interface.
      os.environ['LANG'] = 'en_US.UTF-8'

      symbolizer = 'external_symbolizer_path=%s' % (
          os.path.join(
              BASE_DIR, 'third_party', 'llvm-build', 'Release+Asserts', 'bin',
              'llvm-symbolizer',
          )
      )

      if options.asan:
        asan_options = [symbolizer, "allow_user_segv_handler=1"]
        if not utils.GuessOS() == 'macos':
          # LSAN is not available on mac.
          asan_options.append('detect_leaks=1')
        os.environ['ASAN_OPTIONS'] = ":".join(asan_options)

      if options.sancov_dir:
        assert os.path.exists(options.sancov_dir)
        os.environ['ASAN_OPTIONS'] = ":".join([
          'coverage=1',
          'coverage_dir=%s' % options.sancov_dir,
          symbolizer,
          "allow_user_segv_handler=1",
        ])

      if options.cfi_vptr:
        os.environ['UBSAN_OPTIONS'] = ":".join([
          'print_stacktrace=1',
          'print_summary=1',
          'symbolize=1',
          symbolizer,
        ])

      if options.ubsan_vptr:
        os.environ['UBSAN_OPTIONS'] = ":".join([
          'print_stacktrace=1',
          symbolizer,
        ])

      if options.msan:
        os.environ['MSAN_OPTIONS'] = symbolizer

      if options.tsan:
        suppressions_file = os.path.join(
            BASE_DIR, 'tools', 'sanitizers', 'tsan_suppressions.txt')
        os.environ['TSAN_OPTIONS'] = " ".join([
          symbolizer,
          'suppressions=%s' % suppressions_file,
          'exit_code=0',
          'report_thread_leaks=0',
          'history_size=7',
          'report_destroy_locked=0',
        ])

    def _random_seed(self):
      seed = 0
      while not seed:
        seed = random.SystemRandom().randint(-2147483648, 2147483647)
      return seed

    def _execute(self, arch, mode, args, options, suites):
      print(">>> Running tests for %s.%s" % (arch, mode))

      shell_dir = options.shell_dir
      if not shell_dir:
        if options.auto_detect:
          # If an output dir with a build was passed, test directly in that
          # directory.
          shell_dir = os.path.join(BASE_DIR, options.outdir)
        elif options.buildbot:
          # TODO(machenbach): Get rid of different output folder location on
          # buildbot. Currently this is capitalized Release and Debug.
          shell_dir = os.path.join(BASE_DIR, options.outdir, mode)
          mode = self._buildbot_to_v8_mode(mode)
        else:
          shell_dir = os.path.join(
              BASE_DIR,
              options.outdir,
              "%s.%s" % (arch, MODES[mode]["output_folder"]),
          )
      if not os.path.exists(shell_dir):
          raise Exception('Could not find shell_dir: "%s"' % shell_dir)

      # Populate context object.
      mode_flags = MODES[mode]["flags"]

      # Simulators are slow, therefore allow a longer timeout.
      if arch in SLOW_ARCHS:
        options.timeout *= 2

      options.timeout *= MODES[mode]["timeout_scalefactor"]

      if options.predictable:
        # Predictable mode is slower.
        options.timeout *= 2

      ctx = context.Context(arch, MODES[mode]["execution_mode"], shell_dir,
                            mode_flags, options.verbose,
                            options.timeout,
                            options.isolates,
                            options.command_prefix,
                            options.extra_flags,
                            options.no_i18n,
                            options.random_seed,
                            options.no_sorting,
                            options.rerun_failures_count,
                            options.rerun_failures_max,
                            options.predictable,
                            options.no_harness,
                            use_perf_data=not options.swarming,
                            sancov_dir=options.sancov_dir)

      # TODO(all): Combine "simulator" and "simulator_run".
      # TODO(machenbach): In GN we can derive simulator run from
      # target_arch != v8_target_arch in the dumped build config.
      simulator_run = not options.dont_skip_simulator_slow_tests and \
          arch in ['arm64', 'arm', 'mipsel', 'mips', 'mips64', 'mips64el', \
                  'ppc', 'ppc64', 's390', 's390x'] and \
          bool(ARCH_GUESS) and arch != ARCH_GUESS
      # Find available test suites and read test cases from them.
      variables = {
        "arch": arch,
        "asan": options.asan,
        "deopt_fuzzer": False,
        "gc_stress": options.gc_stress,
        "gcov_coverage": options.gcov_coverage,
        "isolates": options.isolates,
        "mode": MODES[mode]["status_mode"],
        "no_i18n": options.no_i18n,
        "no_snap": options.no_snap,
        "simulator_run": simulator_run,
        "simulator": utils.UseSimulator(arch),
        "system": utils.GuessOS(),
        "tsan": options.tsan,
        "msan": options.msan,
        "dcheck_always_on": options.dcheck_always_on,
        "novfp3": options.novfp3,
        "predictable": options.predictable,
        "byteorder": sys.byteorder,
        "no_harness": options.no_harness,
        "ubsan_vptr": options.ubsan_vptr,
      }
      all_tests = []
      num_tests = 0
      for s in suites:
        s.ReadStatusFile(variables)
        s.ReadTestCases(ctx)
        if len(args) > 0:
          s.FilterTestCasesByArgs(args)
        all_tests += s.tests

        # First filtering by status applying the generic rules (independent of
        # variants).
        s.FilterTestCasesByStatus(options.warn_unused, options.slow_tests,
                                  options.pass_fail_tests)

        if options.cat:
          verbose.PrintTestSource(s.tests)
          continue
        variant_gen = s.CreateVariantGenerator(VARIANTS)
        variant_tests = [ t.CopyAddingFlags(v, flags)
                          for t in s.tests
                          for v in variant_gen.FilterVariantsByTest(t)
                          for flags in variant_gen.GetFlagSets(t, v) ]

        if options.random_seed_stress_count > 1:
          # Duplicate test for random seed stress mode.
          def iter_seed_flags():
            for _ in range(0, options.random_seed_stress_count):
              # Use given random seed for all runs (set by default in
              # execution.py) or a new random seed if none is specified.
              if options.random_seed:
                yield []
              else:
                yield ["--random-seed=%d" % self._random_seed()]
          s.tests = [
            t.CopyAddingFlags(t.variant, flags)
            for t in variant_tests
            for flags in iter_seed_flags()
          ]
        else:
          s.tests = variant_tests

        # Second filtering by status applying the variant-dependent rules.
        s.FilterTestCasesByStatus(options.warn_unused, options.slow_tests,
                                  options.pass_fail_tests, variants=True)

        s.tests = self._shard_tests(s.tests, options)
        num_tests += len(s.tests)

      if options.cat:
        return 0  # We're done here.

      if options.report:
        verbose.PrintReport(all_tests)

      # Run the tests, either locally or distributed on the network.
      start_time = time.time()
      progress_indicator = progress.IndicatorNotifier()
      progress_indicator.Register(
        progress.PROGRESS_INDICATORS[options.progress]())
      if options.junitout:
        progress_indicator.Register(progress.JUnitTestProgressIndicator(
            options.junitout, options.junittestsuite))
      if options.json_test_results:
        progress_indicator.Register(progress.JsonTestProgressIndicator(
            options.json_test_results, arch, MODES[mode]["execution_mode"],
            ctx.random_seed))
      if options.flakiness_results:
        progress_indicator.Register(progress.FlakinessTestProgressIndicator(
            options.flakiness_results))

      run_networked = options.network
      if not run_networked:
        if options.verbose:
          print("Network distribution disabled, running tests locally.")
      elif utils.GuessOS() != "linux":
        print("Network distribution is only supported on Linux, sorry!")
        run_networked = False
      peers = []
      if run_networked:
        peers = network_execution.GetPeers()
        if not peers:
          print("No connection to distribution server; running tests locally.")
          run_networked = False
        elif len(peers) == 1:
          print("No other peers on the network; running tests locally.")
          run_networked = False
        elif num_tests <= 100:
          print("Less than 100 tests, running them locally.")
          run_networked = False

      if run_networked:
        runner = network_execution.NetworkedRunner(suites, progress_indicator,
                                                  ctx, peers, BASE_DIR)
      else:
        runner = execution.Runner(suites, progress_indicator, ctx)

      exit_code = runner.Run(options.j)
      overall_duration = time.time() - start_time

      if options.time:
        verbose.PrintTestDurations(suites, overall_duration)

      if num_tests == 0:
        print("Warning: no tests were run!")

      if exit_code == 1 and options.json_test_results:
        print("Force exit code 0 after failures. Json test results file "
              "generated with failure information.")
        exit_code = 0

      if options.sancov_dir:
        # If tests ran with sanitizer coverage, merge coverage files in the end.
        try:
          print "Merging sancov files."
          subprocess.check_call([
            sys.executable,
            join(BASE_DIR, "tools", "sanitizers", "sancov_merger.py"),
            "--coverage-dir=%s" % options.sancov_dir])
        except:
          print >> sys.stderr, "Error: Merging sancov files failed."
          exit_code = 1

      return exit_code

    def _buildbot_to_v8_mode(self, config):
      """Convert buildbot build configs to configs understood by the v8 runner.

      V8 configs are always lower case and without the additional _x64 suffix
      for 64 bit builds on windows with ninja.
      """
      mode = config[:-4] if config.endswith('_x64') else config
      return mode.lower()

    def _shard_tests(self, tests, options):
      # Read gtest shard configuration from environment (e.g. set by swarming).
      # If none is present, use values passed on the command line.
      shard_count = int(
        os.environ.get('GTEST_TOTAL_SHARDS', options.shard_count))
      shard_run = os.environ.get('GTEST_SHARD_INDEX')
      if shard_run is not None:
        # The v8 shard_run starts at 1, while GTEST_SHARD_INDEX starts at 0.
        shard_run = int(shard_run) + 1
      else:
        shard_run = options.shard_run

      if options.shard_count > 1:
        # Log if a value was passed on the cmd line and it differs from the
        # environment variables.
        if options.shard_count != shard_count:
          print("shard_count from cmd line differs from environment variable "
                "GTEST_TOTAL_SHARDS")
        if options.shard_run > 1 and options.shard_run != shard_run:
          print("shard_run from cmd line differs from environment variable "
                "GTEST_SHARD_INDEX")

      if shard_count < 2:
        return tests
      if shard_run < 1 or shard_run > shard_count:
        print "shard-run not a valid number, should be in [1:shard-count]"
        print "defaulting back to running all tests"
        return tests
      count = 0
      shard = []
      for test in tests:
        if count % shard_count == shard_run - 1:
          shard.append(test)
        count += 1
      return shard


if __name__ == '__main__':
  sys.exit(StandardTestRunner().execute())
