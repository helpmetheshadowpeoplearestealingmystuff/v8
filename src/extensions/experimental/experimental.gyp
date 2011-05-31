# Copyright 2011 the V8 project authors. All rights reserved.
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

{
  'variables': {
    # TODO(cira): Find out how to pass this value for arbitrary embedder.
    # Chromium sets it in common.gypi and does force include of that file for
    # all sub projects.
    'icu_src_dir%': '../../../../third_party/icu',
  },
  'targets': [
    {
      'target_name': 'i18n_api',
      'type': 'static_library',
      'sources': [
        'break-iterator.cc',
        'break-iterator.h',
        'collator.cc',
        'collator.h',
        'i18n-extension.cc',
        'i18n-extension.h',
        'i18n-locale.cc',
        'i18n-locale.h',
        'i18n-natives.h',
        'i18n-utils.cc',
        'i18n-utils.h',
        'language-matcher.cc',
        'language-matcher.h',
        '<(SHARED_INTERMEDIATE_DIR)/i18n-js.cc',
      ],
      'include_dirs': [
        '<(icu_src_dir)/public/common',
        # v8/ is root for all includes.
        '../../..'
      ],
      'dependencies': [
        '<(icu_src_dir)/icu.gyp:*',
        'js2c_i18n#host',
        '../../../tools/gyp/v8.gyp:v8',
      ],
      'direct_dependent_settings': {
        # Adds -Iv8 for embedders.
        'include_dirs': [
          '../../..'
        ],
      },
    },
    {
      'target_name': 'js2c_i18n',
      'type': 'none',
      'toolsets': ['host'],
      'variables': {
        'library_files': [
          'i18n.js'
        ],
      },
      'actions': [
        {
          'action_name': 'js2c_i18n',
          'inputs': [
            'i18n-js2c.py',
            '<@(library_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/i18n-js.cc',
          ],
          'action': [
            'python',
            'i18n-js2c.py',
            '<@(_outputs)',
            '<@(library_files)'
          ],
        },
      ],
    },
  ],  # targets
}
