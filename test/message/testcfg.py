# Copyright 2008 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import itertools
import os
import re

from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.objects import testcase


FLAGS_PATTERN = re.compile(r"//\s+Flags:(.*)")
INVALID_FLAGS = ["--enable-slow-asserts"]
MODULE_PATTERN = re.compile(r"^// MODULE$", flags=re.MULTILINE)


class MessageTestSuite(testsuite.TestSuite):
  def __init__(self, name, root):
    super(MessageTestSuite, self).__init__(name, root)

  def ListTests(self, context):
    tests = []
    for dirname, dirs, files in os.walk(self.root):
      for dotted in [x for x in dirs if x.startswith('.')]:
        dirs.remove(dotted)
      dirs.sort()
      files.sort()
      for filename in files:
        if filename.endswith(".js"):
          fullpath = os.path.join(dirname, filename)
          relpath = fullpath[len(self.root) + 1 : -3]
          testname = relpath.replace(os.path.sep, "/")
          test = testcase.TestCase(self, testname)
          tests.append(test)
    return tests

  def CreateVariantGenerator(self, variants):
    return super(MessageTestSuite, self).CreateVariantGenerator(
        variants + ["preparser"])

  def GetFlagsForTestCase(self, testcase, context):
    source = self.GetSourceForTest(testcase)
    result = []
    flags_match = re.findall(FLAGS_PATTERN, source)
    for match in flags_match:
      result += match.strip().split()
    result += context.mode_flags
    if MODULE_PATTERN.search(source):
      result.append("--module")
    result = [x for x in result if x not in INVALID_FLAGS]
    result.append(os.path.join(self.root, testcase.path + ".js"))
    return testcase.flags + result

  def GetSourceForTest(self, testcase):
    filename = os.path.join(self.root, testcase.path + self.suffix())
    with open(filename) as f:
      return f.read()

  def _IgnoreLine(self, string):
    """Ignore empty lines, valgrind output, Android output."""
    if not string: return True
    if not string.strip(): return True
    return (string.startswith("==") or string.startswith("**") or
            string.startswith("ANDROID"))

  def _GetExpectedFail(self, testcase):
    path = testcase.path
    while path:
      (head, tail) = os.path.split(path)
      if tail == "fail":
        return True
      path = head
    return False

  def IsFailureOutput(self, testcase):
    output = testcase.output
    testpath = testcase.path
    expected_fail = self._GetExpectedFail(testcase)
    fail = testcase.output.exit_code != 0
    if expected_fail != fail:
      return True
    expected_path = os.path.join(self.root, testpath + ".out")
    expected_lines = []
    # Can't use utils.ReadLinesFrom() here because it strips whitespace.
    with open(expected_path) as f:
      for line in f:
        if line.startswith("#") or not line.strip(): continue
        expected_lines.append(line)
    raw_lines = output.stdout.splitlines()
    actual_lines = [ s for s in raw_lines if not self._IgnoreLine(s) ]
    env = { "basename": os.path.basename(testpath + ".js") }
    if len(expected_lines) != len(actual_lines):
      return True
    for (expected, actual) in itertools.izip_longest(
        expected_lines, actual_lines, fillvalue=''):
      pattern = re.escape(expected.rstrip() % env)
      pattern = pattern.replace("\\*", ".*")
      pattern = pattern.replace("\\{NUMBER\\}", "\d(?:\.\d*)?")
      pattern = "^%s$" % pattern
      if not re.match(pattern, actual):
        return True
    return False

  def StripOutputForTransmit(self, testcase):
    pass


def GetSuite(name, root):
  return MessageTestSuite(name, root)
