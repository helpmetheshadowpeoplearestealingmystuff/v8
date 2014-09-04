#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
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

import re

SHA1_RE = re.compile('^[a-fA-F0-9]{40}$')
ROLL_DEPS_GIT_SVN_ID_RE = re.compile('^git-svn-id: .*@([0-9]+) .*$')

# Regular expression that matches a single commit footer line.
COMMIT_FOOTER_ENTRY_RE = re.compile(r'([^:]+):\s+(.+)')

# Footer metadata key for commit position.
COMMIT_POSITION_FOOTER_KEY = 'Cr-Commit-Position'

# Regular expression to parse a commit position
COMMIT_POSITION_RE = re.compile(r'(.+)@\{#(\d+)\}')

# Key for the 'git-svn' ID metadata commit footer entry.
GIT_SVN_ID_FOOTER_KEY = 'git-svn-id'

# e.g., git-svn-id: https://v8.googlecode.com/svn/trunk@23117
#     ce2b1a6d-e550-0410-aec6-3dcde31c8c00
GIT_SVN_ID_RE = re.compile(r'((?:\w+)://[^@]+)@(\d+)\s+(?:[a-zA-Z0-9\-]+)')


# Copied from bot_update.py.
def GetCommitMessageFooterMap(message):
  """Returns: (dict) A dictionary of commit message footer entries.
  """
  footers = {}

  # Extract the lines in the footer block.
  lines = []
  for line in message.strip().splitlines():
    line = line.strip()
    if len(line) == 0:
      del(lines[:])
      continue
    lines.append(line)

  # Parse the footer
  for line in lines:
    m = COMMIT_FOOTER_ENTRY_RE.match(line)
    if not m:
      # If any single line isn't valid, the entire footer is invalid.
      footers.clear()
      return footers
    footers[m.group(1)] = m.group(2).strip()
  return footers


class GitFailedException(Exception):
  pass


def Strip(f):
  def new_f(*args, **kwargs):
    return f(*args, **kwargs).strip()
  return new_f


def MakeArgs(l):
  """['-a', '', 'abc', ''] -> '-a abc'"""
  return " ".join(filter(None, l))


def Quoted(s):
  return "\"%s\"" % s


class GitRecipesMixin(object):
  def GitIsWorkdirClean(self):
    return self.Git("status -s -uno").strip() == ""

  @Strip
  def GitBranch(self):
    return self.Git("branch")

  def GitCreateBranch(self, name, branch=""):
    assert name
    self.Git(MakeArgs(["checkout -b", name, branch]))

  def GitDeleteBranch(self, name):
    assert name
    self.Git(MakeArgs(["branch -D", name]))

  def GitReset(self, name):
    assert name
    self.Git(MakeArgs(["reset --hard", name]))

  def GitStash(self):
    self.Git(MakeArgs(["stash"]))

  def GitRemotes(self):
    return map(str.strip, self.Git(MakeArgs(["branch -r"])).splitlines())

  def GitCheckout(self, name):
    assert name
    self.Git(MakeArgs(["checkout -f", name]))

  def GitCheckoutFile(self, name, branch_or_hash):
    assert name
    assert branch_or_hash
    self.Git(MakeArgs(["checkout -f", branch_or_hash, "--", name]))

  def GitCheckoutFileSafe(self, name, branch_or_hash):
    try:
      self.GitCheckoutFile(name, branch_or_hash)
    except GitFailedException:  # pragma: no cover
      # The file doesn't exist in that revision.
      return False
    return True

  def GitChangedFiles(self, git_hash):
    assert git_hash
    try:
      files = self.Git(MakeArgs(["diff --name-only",
                                 git_hash,
                                 "%s^" % git_hash]))
      return map(str.strip, files.splitlines())
    except GitFailedException:  # pragma: no cover
      # Git fails using "^" at branch roots.
      return []


  @Strip
  def GitCurrentBranch(self):
    for line in self.Git("status -s -b -uno").strip().splitlines():
      match = re.match(r"^## (.+)", line)
      if match: return match.group(1)
    raise Exception("Couldn't find curent branch.")  # pragma: no cover

  @Strip
  def GitLog(self, n=0, format="", grep="", git_hash="", parent_hash="",
             branch="", reverse=False):
    assert not (git_hash and parent_hash)
    args = ["log"]
    if n > 0:
      args.append("-%d" % n)
    if format:
      args.append("--format=%s" % format)
    if grep:
      args.append("--grep=\"%s\"" % grep.replace("\"", "\\\""))
    if reverse:
      args.append("--reverse")
    if git_hash:
      args.append(git_hash)
    if parent_hash:
      args.append("%s^" % parent_hash)
    args.append(branch)
    return self.Git(MakeArgs(args))

  def GitGetPatch(self, git_hash):
    assert git_hash
    return self.Git(MakeArgs(["log", "-1", "-p", git_hash]))

  # TODO(machenbach): Unused? Remove.
  def GitAdd(self, name):
    assert name
    self.Git(MakeArgs(["add", Quoted(name)]))

  def GitApplyPatch(self, patch_file, reverse=False):
    assert patch_file
    args = ["apply --index --reject"]
    if reverse:
      args.append("--reverse")
    args.append(Quoted(patch_file))
    self.Git(MakeArgs(args))

  def GitUpload(self, reviewer="", author="", force=False, cq=False,
                bypass_hooks=False):
    args = ["cl upload --send-mail"]
    if author:
      args += ["--email", Quoted(author)]
    if reviewer:
      args += ["-r", Quoted(reviewer)]
    if force:
      args.append("-f")
    if cq:
      args.append("--use-commit-queue")
    if bypass_hooks:
      args.append("--bypass-hooks")
    # TODO(machenbach): Check output in forced mode. Verify that all required
    # base files were uploaded, if not retry.
    self.Git(MakeArgs(args), pipe=False)

  def GitCommit(self, message="", file_name="", author=None):
    assert message or file_name
    args = ["commit"]
    if file_name:
      args += ["-aF", Quoted(file_name)]
    if message:
      args += ["-am", Quoted(message)]
    if author:
      args += ["--author", "\"%s <%s>\"" % (author, author)]
    self.Git(MakeArgs(args))

  def GitPresubmit(self):
    self.Git("cl presubmit", "PRESUBMIT_TREE_CHECK=\"skip\"")

  def GitDCommit(self):
    self.Git("cl dcommit -f --bypass-hooks", retry_on=lambda x: x is None)

  def GitDiff(self, loc1, loc2):
    return self.Git(MakeArgs(["diff", loc1, loc2]))

  def GitPull(self):
    self.Git("pull")

  def GitFetchOrigin(self):
    self.Git("fetch origin")

  def GitConvertToSVNRevision(self, git_hash):
    result = self.Git(MakeArgs(["rev-list", "-n", "1", git_hash]))
    if not result or not SHA1_RE.match(result):
      raise GitFailedException("Git hash %s is unknown." % git_hash)
    log = self.GitLog(n=1, format="%B", git_hash=git_hash)
    for line in reversed(log.splitlines()):
      match = ROLL_DEPS_GIT_SVN_ID_RE.match(line.strip())
      if match:
        return match.group(1)
    raise GitFailedException("Couldn't convert %s to SVN." % git_hash)

  @Strip
  # Copied from bot_update.py and modified for svn-like numbers only.
  def GetCommitPositionNumber(self, git_hash):
    """Dumps the 'git' log for a specific revision and parses out the commit
    position number.

    If a commit position metadata key is found, its number will be returned.

    Otherwise, we will search for a 'git-svn' metadata entry. If one is found,
    its SVN revision value is returned.
    """
    git_log = self.GitLog(format='%B', n=1, git_hash=git_hash)
    footer_map = GetCommitMessageFooterMap(git_log)

    # Search for commit position metadata
    value = footer_map.get(COMMIT_POSITION_FOOTER_KEY)
    if value:
      match = COMMIT_POSITION_RE.match(value)
      if match:
        return match.group(2)

    # Extract the svn revision from 'git-svn' metadata
    value = footer_map.get(GIT_SVN_ID_FOOTER_KEY)
    if value:
      match = GIT_SVN_ID_RE.match(value)
      if match:
        return match.group(2)
    return None

  ### Git svn stuff

  def GitSVNFetch(self):
    self.Git("svn fetch")

  def GitSVNRebase(self):
    self.Git("svn rebase")

  # TODO(machenbach): Unused? Remove.
  @Strip
  def GitSVNLog(self):
    return self.Git("svn log -1 --oneline")

  @Strip
  def GitSVNFindGitHash(self, revision, branch=""):
    assert revision
    return self.Git(MakeArgs(["svn find-rev", "r%s" % revision, branch]))

  @Strip
  def GitSVNFindSVNRev(self, git_hash, branch=""):
    return self.Git(MakeArgs(["svn find-rev", git_hash, branch]))

  def GitSVNDCommit(self):
    return self.Git("svn dcommit 2>&1", retry_on=lambda x: x is None)

  def GitSVNTag(self, version):
    self.Git(("svn tag %s -m \"Tagging version %s\"" % (version, version)),
             retry_on=lambda x: x is None)
