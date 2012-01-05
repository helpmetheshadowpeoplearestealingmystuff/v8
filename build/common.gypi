# Copyright 2012 the V8 project authors. All rights reserved.
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

# Shared definitions for all V8-related targets.

{
  'variables': {
    'use_system_v8%': 0,
    'msvs_use_common_release': 0,
    'gcc_version%': 'unknown',
    'v8_compress_startup_data%': 'off',
    'v8_target_arch%': '<(target_arch)',

    # Setting 'v8_can_use_unaligned_accesses' to 'true' will allow the code
    # generated by V8 to do unaligned memory access, and setting it to 'false'
    # will ensure that the generated code will always do aligned memory
    # accesses. The default value of 'default' will try to determine the correct
    # setting. Note that for Intel architectures (ia32 and x64) unaligned memory
    # access is allowed for all CPUs.
    'v8_can_use_unaligned_accesses%': 'default',

    # Setting 'v8_can_use_vfp_instructions' to 'true' will enable use of ARM VFP
    # instructions in the V8 generated code. VFP instructions will be enabled
    # both for the snapshot and for the ARM target. Leaving the default value
    # of 'false' will avoid VFP instructions in the snapshot and use CPU feature
    # probing when running on the target.
    'v8_can_use_vfp_instructions%': 'false',

    # Similar to vfp but on MIPS.
    'v8_can_use_fpu_instructions%': 'true',

    # Setting v8_use_arm_eabi_hardfloat to true will turn on V8 support for ARM
    # EABI calling convention where double arguments are passed in VFP
    # registers. Note that the GCC flag '-mfloat-abi=hard' should be used as
    # well when compiling for the ARM target.
    'v8_use_arm_eabi_hardfloat%': 'false',

    # Similar to the ARM hard float ABI but on MIPS.
    'v8_use_mips_abi_hardfloat%': 'true',

    'v8_enable_debugger_support%': 1,

    'v8_enable_disassembler%': 0,

    'v8_object_print%': 0,

    'v8_enable_gdbjit%': 0,

    # Enable profiling support. Only required on Windows.
    'v8_enable_prof%': 0,

    # Chrome needs this definition unconditionally. For standalone V8 builds,
    # it's handled in build/standalone.gypi.
    'want_separate_host_toolset%': 1,

    'v8_use_snapshot%': 'true',
    'host_os%': '<(OS)',
    'v8_use_liveobjectlist%': 'false',
    'werror%': '-Werror',

    # For a shared library build, results in "libv8-<(soname_version).so".
    'soname_version%': '',
  },
  'target_defaults': {
    'conditions': [
      ['v8_enable_debugger_support==1', {
        'defines': ['ENABLE_DEBUGGER_SUPPORT',],
      }],
      ['v8_enable_disassembler==1', {
        'defines': ['ENABLE_DISASSEMBLER',],
      }],
      ['v8_object_print==1', {
        'defines': ['OBJECT_PRINT',],
      }],
      ['v8_enable_gdbjit==1', {
        'defines': ['ENABLE_GDB_JIT_INTERFACE',],
      }],
      ['OS!="mac"', {
        # TODO(mark): The OS!="mac" conditional is temporary. It can be
        # removed once the Mac Chromium build stops setting target_arch to
        # ia32 and instead sets it to mac. Other checks in this file for
        # OS=="mac" can be removed at that time as well. This can be cleaned
        # up once http://crbug.com/44205 is fixed.
        'conditions': [
          ['v8_target_arch=="arm"', {
            'defines': [
              'V8_TARGET_ARCH_ARM',
            ],
            'conditions': [
              [ 'v8_can_use_unaligned_accesses=="true"', {
                'defines': [
                  'CAN_USE_UNALIGNED_ACCESSES=1',
                ],
              }],
              [ 'v8_can_use_unaligned_accesses=="false"', {
                'defines': [
                  'CAN_USE_UNALIGNED_ACCESSES=0',
                ],
              }],
              [ 'v8_can_use_vfp_instructions=="true"', {
                'defines': [
                  'CAN_USE_VFP_INSTRUCTIONS',
                ],
              }],
              [ 'v8_use_arm_eabi_hardfloat=="true"', {
                'defines': [
                  'USE_EABI_HARDFLOAT=1',
                  'CAN_USE_VFP_INSTRUCTIONS',
                ],
                'cflags': [
                  '-mfloat-abi=hard',
                ],
              }, {
                'defines': [
                  'USE_EABI_HARDFLOAT=0',
                ],
              }],
              # The ARM assembler assumes the host is 32 bits,
              # so force building 32-bit host tools.
              ['host_arch=="x64" or OS=="android"', {
                'target_conditions': [
                  ['_toolset=="host"', {
                    'cflags': ['-m32'],
                    'ldflags': ['-m32'],
                  }],
                ],
              }],
            ],
          }],
          ['v8_target_arch=="ia32"', {
            'defines': [
              'V8_TARGET_ARCH_IA32',
            ],
          }],
          ['v8_target_arch=="mips"', {
            'defines': [
              'V8_TARGET_ARCH_MIPS',
            ],
            'conditions': [
              [ 'v8_can_use_fpu_instructions=="true"', {
                'defines': [
                  'CAN_USE_FPU_INSTRUCTIONS',
                ],
              }],
              [ 'v8_use_mips_abi_hardfloat=="true"', {
                'defines': [
                  '__mips_hard_float=1',
                  'CAN_USE_FPU_INSTRUCTIONS',
                ],
              }, {
                'defines': [
                  '__mips_soft_float=1'
                ],
              }],
              # The MIPS assembler assumes the host is 32 bits,
              # so force building 32-bit host tools.
              ['host_arch=="x64"', {
                'target_conditions': [
                  ['_toolset=="host"', {
                    'cflags': ['-m32'],
                    'ldflags': ['-m32'],
                  }],
                ],
              }],
            ],
          }],
          ['v8_target_arch=="x64"', {
            'defines': [
              'V8_TARGET_ARCH_X64',
            ],
          }],
        ],
      }],
      ['v8_use_liveobjectlist=="true"', {
        'defines': [
          'ENABLE_DEBUGGER_SUPPORT',
          'INSPECTOR',
          'OBJECT_PRINT',
          'LIVEOBJECTLIST',
        ],
      }],
      ['v8_compress_startup_data=="bz2"', {
        'defines': [
          'COMPRESS_STARTUP_DATA_BZ2',
        ],
      }],
      ['OS=="win" and v8_enable_prof==1', {
        'msvs_settings': {
          'VCLinkerTool': {
            'GenerateMapFile': 'true',
          },
        },
      }],
      ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris" \
         or OS=="netbsd"', {
        'conditions': [
          [ 'target_arch=="ia32"', {
            'cflags': [ '-m32' ],
            'ldflags': [ '-m32' ],
          }],
        ],
      }],
      ['OS=="solaris"', {
        'defines': [ '__C99FEATURES__=1' ],  # isinf() etc.
      }],
    ],
    'configurations': {
      'Debug': {
        'defines': [
          'DEBUG',
          'ENABLE_DISASSEMBLER',
          'V8_ENABLE_CHECKS',
          'OBJECT_PRINT',
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '0',

            'conditions': [
              ['OS=="win" and component=="shared_library"', {
                'RuntimeLibrary': '3',  # /MDd
              }, {
                'RuntimeLibrary': '1',  # /MTd
              }],
            ],
          },
          'VCLinkerTool': {
            'LinkIncremental': '2',
            # For future reference, the stack size needs to be increased
            # when building for Windows 64-bit, otherwise some test cases
            # can cause stack overflow.
            # 'StackReserveSize': '297152',
          },
        },
        'conditions': [
          ['OS=="freebsd" or OS=="openbsd"', {
            'cflags': [ '-I/usr/local/include' ],
          }],
          ['OS=="netbsd"', {
            'cflags': [ '-I/usr/pkg/include' ],
          }],
          ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="netbsd"', {
            'cflags': [ '-Wall', '<(werror)', '-W', '-Wno-unused-parameter',
                        '-Wnon-virtual-dtor' ],
          }],
        ],
      },
      'Release': {
        'conditions': [
          ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="netbsd"', {
            'cflags!': [
              '-O2',
              '-Os',
            ],
            'cflags': [
              '-fdata-sections',
              '-ffunction-sections',
              '-fomit-frame-pointer',
              '-O3',
            ],
            'conditions': [
              [ 'gcc_version==44', {
                'cflags': [
                  # Avoid crashes with gcc 4.4 in the v8 test suite.
                  '-fno-tree-vrp',
                ],
              }],
            ],
          }],
          ['OS=="freebsd" or OS=="openbsd"', {
            'cflags': [ '-I/usr/local/include' ],
          }],
          ['OS=="netbsd"', {
            'cflags': [ '-I/usr/pkg/include' ],
          }],
          ['OS=="mac"', {
            'xcode_settings': {
              'GCC_OPTIMIZATION_LEVEL': '3',  # -O3

              # -fstrict-aliasing.  Mainline gcc
              # enables this at -O2 and above,
              # but Apple gcc does not unless it
              # is specified explicitly.
              'GCC_STRICT_ALIASING': 'YES',
            },
          }],
          ['OS=="win"', {
            'msvs_configuration_attributes': {
              'OutputDirectory': '<(DEPTH)\\build\\$(ConfigurationName)',
              'IntermediateDirectory': '$(OutDir)\\obj\\$(ProjectName)',
              'CharacterSet': '1',
            },
            'msvs_settings': {
              'VCCLCompilerTool': {
                'Optimization': '2',
                'InlineFunctionExpansion': '2',
                'EnableIntrinsicFunctions': 'true',
                'FavorSizeOrSpeed': '0',
                'OmitFramePointers': 'true',
                'StringPooling': 'true',

                'conditions': [
                  ['OS=="win" and component=="shared_library"', {
                    'RuntimeLibrary': '2',  #/MD
                  }, {
                    'RuntimeLibrary': '0',  #/MT
                  }],
                ],
              },
              'VCLinkerTool': {
                'LinkIncremental': '1',
                'OptimizeReferences': '2',
                'OptimizeForWindows98': '1',
                'EnableCOMDATFolding': '2',
                # For future reference, the stack size needs to be
                # increased when building for Windows 64-bit, otherwise
                # some test cases can cause stack overflow.
                # 'StackReserveSize': '297152',
              },
            },
          }],
        ],
      },
    },
  },
}
