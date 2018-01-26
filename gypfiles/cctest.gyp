# Copyright 2012 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'v8_code': 1,
    'generated_file': '<(SHARED_INTERMEDIATE_DIR)/resources.cc',
    'cctest_sources': [
      '../test/cctest/compiler/c-signature.h',
      '../test/cctest/compiler/call-tester.h',
      '../test/cctest/compiler/codegen-tester.cc',
      '../test/cctest/compiler/codegen-tester.h',
      '../test/cctest/compiler/code-assembler-tester.h',
      '../test/cctest/compiler/function-tester.cc',
      '../test/cctest/compiler/function-tester.h',
      '../test/cctest/compiler/graph-builder-tester.h',
      '../test/cctest/compiler/test-basic-block-profiler.cc',
      '../test/cctest/compiler/test-branch-combine.cc',
      '../test/cctest/compiler/test-run-unwinding-info.cc',
      '../test/cctest/compiler/test-gap-resolver.cc',
      '../test/cctest/compiler/test-graph-visualizer.cc',
      '../test/cctest/compiler/test-code-generator.cc',
      '../test/cctest/compiler/test-code-assembler.cc',
      '../test/cctest/compiler/test-instruction.cc',
      '../test/cctest/compiler/test-js-context-specialization.cc',
      '../test/cctest/compiler/test-js-constant-cache.cc',
      '../test/cctest/compiler/test-js-typed-lowering.cc',
      '../test/cctest/compiler/test-jump-threading.cc',
      '../test/cctest/compiler/test-linkage.cc',
      '../test/cctest/compiler/test-loop-analysis.cc',
      '../test/cctest/compiler/test-machine-operator-reducer.cc',
      '../test/cctest/compiler/test-multiple-return.cc',
      '../test/cctest/compiler/test-node.cc',
      '../test/cctest/compiler/test-operator.cc',
      '../test/cctest/compiler/test-representation-change.cc',
      '../test/cctest/compiler/test-run-bytecode-graph-builder.cc',
      '../test/cctest/compiler/test-run-calls-to-external-references.cc',
      '../test/cctest/compiler/test-run-deopt.cc',
      '../test/cctest/compiler/test-run-intrinsics.cc',
      '../test/cctest/compiler/test-run-jsbranches.cc',
      '../test/cctest/compiler/test-run-jscalls.cc',
      '../test/cctest/compiler/test-run-jsexceptions.cc',
      '../test/cctest/compiler/test-run-jsobjects.cc',
      '../test/cctest/compiler/test-run-jsops.cc',
      '../test/cctest/compiler/test-run-load-store.cc',
      '../test/cctest/compiler/test-run-machops.cc',
      '../test/cctest/compiler/test-run-native-calls.cc',
      '../test/cctest/compiler/test-run-retpoline.cc',
      '../test/cctest/compiler/test-run-stackcheck.cc',
      '../test/cctest/compiler/test-run-stubs.cc',
      '../test/cctest/compiler/test-run-tail-calls.cc',
      '../test/cctest/compiler/test-run-variables.cc',
      '../test/cctest/compiler/test-run-wasm-machops.cc',
      '../test/cctest/compiler/value-helper.cc',
      '../test/cctest/compiler/value-helper.h',
      '../test/cctest/cctest.cc',
      '../test/cctest/cctest.h',
      '../test/cctest/expression-type-collector-macros.h',
      '../test/cctest/gay-fixed.cc',
      '../test/cctest/gay-fixed.h',
      '../test/cctest/gay-precision.cc',
      '../test/cctest/gay-precision.h',
      '../test/cctest/gay-shortest.cc',
      '../test/cctest/gay-shortest.h',
      '../test/cctest/heap/heap-tester.h',
      '../test/cctest/heap/heap-utils.cc',
      '../test/cctest/heap/heap-utils.h',
      '../test/cctest/heap/test-alloc.cc',
      '../test/cctest/heap/test-array-buffer-tracker.cc',
      '../test/cctest/heap/test-compaction.cc',
      '../test/cctest/heap/test-concurrent-marking.cc',
      '../test/cctest/heap/test-embedder-tracing.cc',
      '../test/cctest/heap/test-heap.cc',
      '../test/cctest/heap/test-incremental-marking.cc',
      '../test/cctest/heap/test-invalidated-slots.cc',
      '../test/cctest/heap/test-lab.cc',
      '../test/cctest/heap/test-mark-compact.cc',
      '../test/cctest/heap/test-page-promotion.cc',
      '../test/cctest/heap/test-spaces.cc',
      '../test/cctest/interpreter/interpreter-tester.cc',
      '../test/cctest/interpreter/interpreter-tester.h',
      '../test/cctest/interpreter/source-position-matcher.cc',
      '../test/cctest/interpreter/source-position-matcher.h',
      '../test/cctest/interpreter/test-bytecode-generator.cc',
      '../test/cctest/interpreter/test-interpreter.cc',
      '../test/cctest/interpreter/test-interpreter-intrinsics.cc',
      '../test/cctest/interpreter/test-source-positions.cc',
      '../test/cctest/interpreter/bytecode-expectations-printer.cc',
      '../test/cctest/interpreter/bytecode-expectations-printer.h',
      '../test/cctest/libplatform/test-tracing.cc',
      '../test/cctest/libsampler/test-sampler.cc',
      '../test/cctest/parsing/test-parse-decision.cc',
      '../test/cctest/parsing/test-preparser.cc',
      '../test/cctest/parsing/test-scanner-streams.cc',
      '../test/cctest/parsing/test-scanner.cc',
      '../test/cctest/print-extension.cc',
      '../test/cctest/print-extension.h',
      '../test/cctest/profiler-extension.cc',
      '../test/cctest/profiler-extension.h',
      '../test/cctest/scope-test-helper.h',
      '../test/cctest/setup-isolate-for-tests.cc',
      '../test/cctest/setup-isolate-for-tests.h',
      '../test/cctest/test-access-checks.cc',
      '../test/cctest/test-accessor-assembler.cc',
      '../test/cctest/test-accessors.cc',
      '../test/cctest/test-allocation.cc',
      '../test/cctest/test-api.cc',
      '../test/cctest/test-api.h',
      '../test/cctest/test-api-accessors.cc',
      '../test/cctest/test-api-interceptors.cc',
      '../test/cctest/test-array-list.cc',
      '../test/cctest/test-atomicops.cc',
      '../test/cctest/test-bignum.cc',
      '../test/cctest/test-bignum-dtoa.cc',
      '../test/cctest/test-bit-vector.cc',
      '../test/cctest/test-circular-queue.cc',
      '../test/cctest/test-code-layout.cc',
      '../test/cctest/test-code-stub-assembler.cc',
      '../test/cctest/test-compiler.cc',
      '../test/cctest/test-constantpool.cc',
      '../test/cctest/test-conversions.cc',
      '../test/cctest/test-cpu-profiler.cc',
      '../test/cctest/test-date.cc',
      '../test/cctest/test-debug.cc',
      '../test/cctest/test-decls.cc',
      '../test/cctest/test-deoptimization.cc',
      '../test/cctest/test-dictionary.cc',
      '../test/cctest/test-diy-fp.cc',
      '../test/cctest/test-double.cc',
      '../test/cctest/test-dtoa.cc',
      '../test/cctest/test-elements-kind.cc',
      '../test/cctest/test-fast-dtoa.cc',
      '../test/cctest/test-feedback-vector.cc',
      '../test/cctest/test-feedback-vector.h',
      '../test/cctest/test-field-type-tracking.cc',
      '../test/cctest/test-fixed-dtoa.cc',
      '../test/cctest/test-flags.cc',
      '../test/cctest/test-func-name-inference.cc',
      '../test/cctest/test-global-handles.cc',
      '../test/cctest/test-global-object.cc',
      '../test/cctest/test-hashcode.cc',
      '../test/cctest/test-hashmap.cc',
      '../test/cctest/test-heap-profiler.cc',
      '../test/cctest/test-identity-map.cc',
      '../test/cctest/test-intl.cc',
      '../test/cctest/test-inobject-slack-tracking.cc',
      '../test/cctest/test-isolate-independent-builtins.cc',
      '../test/cctest/test-liveedit.cc',
      '../test/cctest/test-lockers.cc',
      '../test/cctest/test-log.cc',
      '../test/cctest/test-managed.cc',
      '../test/cctest/test-mementos.cc',
      '../test/cctest/test-modules.cc',
      '../test/cctest/test-object.cc',
      '../test/cctest/test-orderedhashtable.cc',
      '../test/cctest/test-parsing.cc',
      '../test/cctest/test-platform.cc',
      '../test/cctest/test-profile-generator.cc',
      '../test/cctest/test-random-number-generator.cc',
      '../test/cctest/test-regexp.cc',
      '../test/cctest/test-representation.cc',
      '../test/cctest/test-sampler-api.cc',
      '../test/cctest/test-serialize.cc',
      '../test/cctest/test-strings.cc',
      '../test/cctest/test-symbols.cc',
      '../test/cctest/test-strtod.cc',
      '../test/cctest/test-thread-termination.cc',
      '../test/cctest/test-threads.cc',
      '../test/cctest/test-trace-event.cc',
      '../test/cctest/test-traced-value.cc',
      '../test/cctest/test-transitions.cc',
      '../test/cctest/test-transitions.h',
      '../test/cctest/test-typedarrays.cc',
      '../test/cctest/test-types.cc',
      '../test/cctest/test-unbound-queue.cc',
      '../test/cctest/test-unboxed-doubles.cc',
      '../test/cctest/test-unscopables-hidden-prototype.cc',
      '../test/cctest/test-usecounters.cc',
      '../test/cctest/test-utils.cc',
      '../test/cctest/test-version.cc',
      '../test/cctest/test-weakmaps.cc',
      '../test/cctest/test-weaksets.cc',
      '../test/cctest/trace-extension.cc',
      '../test/cctest/trace-extension.h',
      '../test/cctest/types-fuzz.h',
      '../test/cctest/unicode-helpers.h',
      '../test/cctest/wasm/test-c-wasm-entry.cc',
      '../test/cctest/wasm/test-streaming-compilation.cc',
      '../test/cctest/wasm/test-run-wasm.cc',
      '../test/cctest/wasm/test-run-wasm-64.cc',
      '../test/cctest/wasm/test-run-wasm-asmjs.cc',
      '../test/cctest/wasm/test-run-wasm-atomics.cc',
      '../test/cctest/wasm/test-run-wasm-interpreter.cc',
      '../test/cctest/wasm/test-run-wasm-js.cc',
      '../test/cctest/wasm/test-run-wasm-module.cc',
      '../test/cctest/wasm/test-run-wasm-relocation.cc',
      '../test/cctest/wasm/test-run-wasm-sign-extension.cc',
      '../test/cctest/wasm/test-run-wasm-simd.cc',
      '../test/cctest/wasm/test-wasm-breakpoints.cc',
      "../test/cctest/wasm/test-wasm-codegen.cc",
      '../test/cctest/wasm/test-wasm-interpreter-entry.cc',
      '../test/cctest/wasm/test-wasm-stack.cc',
      '../test/cctest/wasm/test-wasm-trap-position.cc',
      '../test/cctest/wasm/wasm-run-utils.cc',
      '../test/cctest/wasm/wasm-run-utils.h',
    ],
    'cctest_sources_ia32': [
      '../test/cctest/test-assembler-ia32.cc',
      '../test/cctest/test-code-stubs.cc',
      '../test/cctest/test-code-stubs.h',
      '../test/cctest/test-code-stubs-ia32.cc',
      '../test/cctest/test-disasm-ia32.cc',
      '../test/cctest/test-log-stack-tracer.cc',
      '../test/cctest/test-run-wasm-relocation-ia32.cc',
    ],
    'cctest_sources_x64': [
      '../test/cctest/test-assembler-x64.cc',
      '../test/cctest/test-code-stubs.cc',
      '../test/cctest/test-code-stubs.h',
      '../test/cctest/test-code-stubs-x64.cc',
      '../test/cctest/test-disasm-x64.cc',
      '../test/cctest/test-macro-assembler-x64.cc',
      '../test/cctest/test-log-stack-tracer.cc',
      '../test/cctest/test-run-wasm-relocation-x64.cc',
    ],
    'cctest_sources_arm': [
      '../test/cctest/assembler-helper-arm.cc',
      '../test/cctest/assembler-helper-arm.h',
      '../test/cctest/test-assembler-arm.cc',
      '../test/cctest/test-code-stubs.cc',
      '../test/cctest/test-code-stubs.h',
      '../test/cctest/test-code-stubs-arm.cc',
      '../test/cctest/test-disasm-arm.cc',
      '../test/cctest/test-macro-assembler-arm.cc',
      '../test/cctest/test-run-wasm-relocation-arm.cc',
      '../test/cctest/test-sync-primitives-arm.cc',
    ],
    'cctest_sources_arm64': [
      '../test/cctest/test-utils-arm64.cc',
      '../test/cctest/test-utils-arm64.h',
      '../test/cctest/test-assembler-arm64.cc',
      '../test/cctest/test-code-stubs.cc',
      '../test/cctest/test-code-stubs.h',
      '../test/cctest/test-code-stubs-arm64.cc',
      '../test/cctest/test-disasm-arm64.cc',
      '../test/cctest/test-fuzz-arm64.cc',
      '../test/cctest/test-javascript-arm64.cc',
      '../test/cctest/test-js-arm64-variables.cc',
      '../test/cctest/test-run-wasm-relocation-arm64.cc',
      '../test/cctest/test-sync-primitives-arm64.cc',
    ],
    'cctest_sources_s390': [
      '../test/cctest/test-assembler-s390.cc',
      '../test/cctest/test-code-stubs.cc',
      '../test/cctest/test-code-stubs.h',
      '../test/cctest/test-disasm-s390.cc',
    ],
    'cctest_sources_ppc': [
      '../test/cctest/test-assembler-ppc.cc',
      '../test/cctest/test-code-stubs.cc',
      '../test/cctest/test-code-stubs.h',
      '../test/cctest/test-disasm-ppc.cc',
    ],
    'cctest_sources_mips': [
      '../test/cctest/test-assembler-mips.cc',
      '../test/cctest/test-code-stubs.cc',
      '../test/cctest/test-code-stubs.h',
      '../test/cctest/test-code-stubs-mips.cc',
      '../test/cctest/test-disasm-mips.cc',
      '../test/cctest/test-macro-assembler-mips.cc',
    ],
    'cctest_sources_mipsel': [
      '../test/cctest/test-assembler-mips.cc',
      '../test/cctest/test-code-stubs.cc',
      '../test/cctest/test-code-stubs.h',
      '../test/cctest/test-code-stubs-mips.cc',
      '../test/cctest/test-disasm-mips.cc',
      '../test/cctest/test-macro-assembler-mips.cc',
    ],
    'cctest_sources_mips64': [
      '../test/cctest/test-assembler-mips64.cc',
      '../test/cctest/test-code-stubs.cc',
      '../test/cctest/test-code-stubs.h',
      '../test/cctest/test-code-stubs-mips64.cc',
      '../test/cctest/test-disasm-mips64.cc',
      '../test/cctest/test-macro-assembler-mips64.cc',
    ],
    'cctest_sources_mips64el': [
      '../test/cctest/test-assembler-mips64.cc',
      '../test/cctest/test-code-stubs.cc',
      '../test/cctest/test-code-stubs.h',
      '../test/cctest/test-code-stubs-mips64.cc',
      '../test/cctest/test-disasm-mips64.cc',
      '../test/cctest/test-macro-assembler-mips64.cc',
    ],
  },
  'includes': ['toolchain.gypi', 'features.gypi'],
  'targets': [
    {
      'target_name': 'cctest',
      'type': 'executable',
      'dependencies': [
        'resources',
        'v8.gyp:v8_libbase',
        'v8.gyp:v8_libplatform',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../test/common/wasm/flag-utils.h',
        '../test/common/wasm/test-signatures.h',
        '../test/common/wasm/wasm-macro-gen.h',
        '../test/common/wasm/wasm-module-runner.cc',
        '../test/common/wasm/wasm-module-runner.h',
        '<@(cctest_sources)',
        '<(generated_file)',
      ],
      'conditions': [
        ['v8_target_arch=="ia32"', {
          'sources': [
            '<@(cctest_sources_ia32)',
          ],
        }],
        ['v8_target_arch=="x64"', {
          'sources': [
            '<@(cctest_sources_x64)',
          ],
        }],
        ['v8_target_arch=="arm"', {
          'sources': [
            '<@(cctest_sources_arm)',
          ],
        }],
        ['v8_target_arch=="arm64"', {
          'sources': [
            '<@(cctest_sources_arm64)',
          ],
        }],
        ['v8_target_arch=="s390"', {
          'sources': [
            '<@(cctest_sources_s390)',
          ],
        }],
        ['v8_target_arch=="s390x"', {
          'sources': [
            '<@(cctest_sources_s390)',
          ],
        }],
        ['v8_target_arch=="ppc"', {
          'sources': [
            '<@(cctest_sources_ppc)',
          ],
        }],
        ['v8_target_arch=="ppc64"', {
          'sources': [
            '<@(cctest_sources_ppc)',
          ],
        }],
        ['v8_target_arch=="mips"', {
          'sources': [
            '<@(cctest_sources_mips)',
          ],
        }],
        ['v8_target_arch=="mipsel"', {
          'sources': [
            '<@(cctest_sources_mipsel)',
          ],
        }],
        ['v8_target_arch=="mips64"', {
          'sources': [
            '<@(cctest_sources_mips64)',
          ],
        }],
        ['v8_target_arch=="mips64el"', {
          'sources': [
            '<@(cctest_sources_mips64el)',
          ],
        }],
        [ 'OS=="win"', {
          'msvs_settings': {
            'VCCLCompilerTool': {
              # MSVS wants this for gay-{precision,shortest}.cc.
              'AdditionalOptions': ['/bigobj'],
            },
          },
        }],
        ['v8_target_arch=="ppc" or v8_target_arch=="ppc64" \
          or v8_target_arch=="arm" or v8_target_arch=="arm64" \
          or v8_target_arch=="s390" or v8_target_arch=="s390x" \
          or v8_target_arch=="mips" or v8_target_arch=="mips64" \
          or v8_target_arch=="mipsel" or v8_target_arch=="mips64el"', {
          # disable fmadd/fmsub so that expected results match generated code in
          # RunFloat64MulAndFloat64Add1 and friends.
          'cflags': ['-ffp-contract=off'],
        }],
        ['OS=="aix"', {
          'ldflags': [ '-Wl,-bbigtoc' ],
        }],
        ['component=="shared_library"', {
          # cctest can't be built against a shared library, so we need to
          # depend on the underlying static target in that case.
          'dependencies': ['v8.gyp:v8_maybe_snapshot'],
          'defines': [ 'BUILDING_V8_SHARED', ]
        }, {
          'dependencies': ['v8.gyp:v8'],
        }],
        ['v8_use_snapshot=="true"', {
          'dependencies': ['v8.gyp:v8_initializers'],
        }],
      ],
    },
    {
      'target_name': 'resources',
      'type': 'none',
      'variables': {
        'file_list': [
           '../tools/splaytree.js',
           '../tools/codemap.js',
           '../tools/csvparser.js',
           '../tools/consarray.js',
           '../tools/profile.js',
           '../tools/profile_view.js',
           '../tools/arguments.js',
           '../tools/logreader.js',
           '../test/cctest/log-eq-of-logging-and-traversal.js',
        ],
      },
      'actions': [
        {
          'action_name': 'js2c',
          'inputs': [
            '../tools/js2c.py',
            '<@(file_list)',
          ],
          'outputs': [
            '<(generated_file)',
          ],
          'action': [
            'python',
            '../tools/js2c.py',
            '<@(_outputs)',
            'TEST',  # type
            '<@(file_list)',
          ],
        }
      ],
    },
    {
      'target_name': 'generate-bytecode-expectations',
      'type': 'executable',
      'dependencies': [
        'v8.gyp:v8',
        'v8.gyp:v8_libbase',
        'v8.gyp:v8_libplatform',
      ],
      'include_dirs+': [
        '..',
      ],
      'sources': [
        '../test/cctest/interpreter/bytecode-expectations-printer.cc',
        '../test/cctest/interpreter/bytecode-expectations-printer.h',
        '../test/cctest/interpreter/generate-bytecode-expectations.cc',
      ],
    },
  ],
  'conditions': [
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'cctest_exe_run',
          'type': 'none',
          'dependencies': [
            'cctest',
          ],
          'includes': [
            'isolate.gypi',
          ],
          'sources': [
            '../test/cctest/cctest_exe.isolate',
          ],
        },
        {
          'target_name': 'cctest_run',
          'type': 'none',
          'dependencies': [
            'cctest_exe_run',
          ],
          'includes': [
            'isolate.gypi',
          ],
          'sources': [
            '../test/cctest/cctest.isolate',
          ],
        },
      ],
    }],
  ],
}
