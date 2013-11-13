# Copyright 2011 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '../samples/samples.gyp:*',
        '../src/d8.gyp:d8',
        '../tools/lexer-shell.gyp:lexer-shell',
        '../test/cctest/cctest.gyp:*',
      ],
    }
  ]
}

