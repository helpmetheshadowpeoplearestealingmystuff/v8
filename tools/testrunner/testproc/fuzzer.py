# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import namedtuple
import time

from . import base


class FuzzerConfig(object):
  def __init__(self, probability, analyzer, fuzzer):
    """
    Args:
      probability: of choosing this fuzzer (0; 10]
      analyzer: instance of Analyzer class, can be None if no analysis is needed
      fuzzer: instance of Fuzzer class
    """
    assert probability > 0 and probability <= 10

    self.probability = probability
    self.analyzer = analyzer
    self.fuzzer = fuzzer


class Analyzer(object):
  def get_analysis_flags(self):
    raise NotImplementedError()

  def do_analysis(self, result):
    raise NotImplementedError()


class Fuzzer(object):
  def create_flags_generator(self, test, analysis_value):
    raise NotImplementedError()


# TODO(majeski): Allow multiple subtests to run at once.
class FuzzerProc(base.TestProcProducer):
  def __init__(self, rng, count, fuzzers, fuzz_duration_sec=0):
    """
    Args:
      rng: random number generator used to select flags and values for them
      count: number of tests to generate based on each base test
      fuzzers: list of FuzzerConfig instances
      fuzz_duration_sec: how long it should run, overrides count
    """
    super(FuzzerProc, self).__init__('Fuzzer')

    self._rng = rng
    self._count = count
    self._fuzzer_configs = fuzzers
    self._fuzz_duration_sec = fuzz_duration_sec
    self._gens = {}

    self._start_time = None
    self._stop = False

  def setup(self, requirement=base.DROP_RESULT):
    # Fuzzer is optimized to not store the results
    assert requirement == base.DROP_RESULT
    super(FuzzerProc, self).setup(requirement)

  def _next_test(self, test):
    if not self._start_time:
      self._start_time = time.time()

    analysis_flags = []
    for fuzzer_config in self._fuzzer_configs:
      if fuzzer_config.analyzer:
        analysis_flags += fuzzer_config.analyzer.get_analysis_flags()

    if analysis_flags:
      analysis_flags = list(set(analysis_flags))
      subtest = self._create_subtest(test, 'analysis', flags=analysis_flags,
                                     keep_output=True)
      self._send_test(subtest)
      return

    self._gens[test.procid] = self._create_gen(test)
    self._try_send_next_test(test)

  def _result_for(self, test, subtest, result):
    if self._fuzz_duration_sec and not self._stop:
      if int(time.time() - self._start_time) > self._fuzz_duration_sec:
        print '>>> Stopping fuzzing'
        self._stop = True

    if result is not None:
      # Analysis phase, for fuzzing we drop the result.
      if result.has_unexpected_output:
        self._send_result(test, None)
        return
      self._gens[test.procid] = self._create_gen(test, result)

    self._try_send_next_test(test)

  def _create_gen(self, test, analysis_result=None):
    # It will be called with analysis_result==None only when there is no
    # analysis phase at all, so no fuzzer has it's own analyzer.
    gens = []
    indexes = []
    for i, fuzzer_config in enumerate(self._fuzzer_configs):
      analysis_value = None
      if fuzzer_config.analyzer:
        analysis_value = fuzzer_config.analyzer.do_analysis(analysis_result)
        if not analysis_value:
          # Skip fuzzer for this test since it doesn't have analysis data
          continue
      p = fuzzer_config.probability
      flag_gen = fuzzer_config.fuzzer.create_flags_generator(test,
                                                             analysis_value)
      indexes += [len(gens)] * p
      gens.append((p, flag_gen))

    if not gens:
      # No fuzzers for this test, skip it
      return

    i = 0
    while not self._stop or i < self._count:
      main_index = self._rng.choice(indexes)
      _, main_gen = gens[main_index]

      flags = next(main_gen)
      for index, (p, gen) in enumerate(gens):
        if index == main_index:
          continue
        if self._rng.randint(1, 10) <= p:
          flags += next(gen)

      flags.append('--fuzzer-random-seed=%s' % self._next_seed())
      yield self._create_subtest(test, str(i), flags=flags)

      i += 1

  def _try_send_next_test(self, test):
    if not self._stop:
      for subtest in self._gens[test.procid]:
        self._send_test(subtest)
        return

    del self._gens[test.procid]
    self._send_result(test, None)

  def _next_seed(self):
    seed = None
    while not seed:
      seed = self._rng.randint(-2147483648, 2147483647)
    return seed


def create_scavenge_config(probability):
  return FuzzerConfig(probability, ScavengeAnalyzer(), ScavengeFuzzer())

def create_marking_config(probability):
  return FuzzerConfig(probability, MarkingAnalyzer(), MarkingFuzzer())

def create_gc_interval_config(probability):
  return FuzzerConfig(probability, GcIntervalAnalyzer(), GcIntervalFuzzer())

def create_compaction_config(probability):
  return FuzzerConfig(probability, None, CompactionFuzzer())


class ScavengeAnalyzer(Analyzer):
  def get_analysis_flags(self):
    return ['--fuzzer-gc-analysis']

  def do_analysis(self, result):
    for line in reversed(result.output.stdout.splitlines()):
      if line.startswith('### Maximum new space size reached = '):
        return int(float(line.split()[7]))


class ScavengeFuzzer(Fuzzer):
  def create_flags_generator(self, test, analysis_value):
    while True:
      yield ['--stress-scavenge=%d' % analysis_value]


class MarkingAnalyzer(Analyzer):
  def get_analysis_flags(self):
    return ['--fuzzer-gc-analysis']

  def do_analysis(self, result):
    for line in reversed(result.output.stdout.splitlines()):
      if line.startswith('### Maximum marking limit reached = '):
        return int(float(line.split()[6]))


class MarkingFuzzer(Fuzzer):
  def create_flags_generator(self, test, analysis_value):
    while True:
      yield ['--stress-marking=%d' % analysis_value]


class GcIntervalAnalyzer(Analyzer):
  def get_analysis_flags(self):
    return ['--fuzzer-gc-analysis']

  def do_analysis(self, result):
    for line in reversed(result.output.stdout.splitlines()):
      if line.startswith('### Allocations = '):
        return int(float(line.split()[3][:-1]))


class GcIntervalFuzzer(Fuzzer):
  def create_flags_generator(self, test, analysis_value):
    value = analysis_value / 10
    while True:
      yield ['--random-gc-interval=%d' % value]


class CompactionFuzzer(Fuzzer):
  def create_flags_generator(self, test, analysis_value):
    while True:
      yield ['--stress-compaction-random']
