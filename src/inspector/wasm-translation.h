// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_WASMTRANSLATION_H_
#define V8_INSPECTOR_WASMTRANSLATION_H_

#include <unordered_map>

#include "include/v8.h"
#include "src/base/macros.h"
#include "src/inspector/string-16.h"

namespace v8_inspector {

// Forward declarations.
class V8Debugger;
class V8DebuggerScript;
struct ScriptBreakpoint;
namespace protocol {
namespace Debugger {
class Location;
}
}

class WasmTranslation {
 public:
  enum Mode { Raw, Disassemble };

  WasmTranslation(v8::Isolate* isolate, V8Debugger* debugger);
  ~WasmTranslation();

  // Set translation mode.
  void SetMode(Mode mode) { mode_ = mode; }

  // Make a wasm script known to the translation. Only locations referencing a
  // registered script will be translated by the Translate functions below.
  void AddScript(v8::Local<v8::Object> script_wrapper);

  // Clear all registered scripts.
  void Clear();

  // Translate a location as generated by V8 to a location that should be sent
  // over protocol.
  // Does nothing for locations referencing a script which was not registered
  // before via AddScript.
  // Line and column are 0-based.
  // The context group id specifies the context of the script.
  // If the script was registered and the respective wasm function was not seen
  // before, a new artificial script representing this function will be created
  // and made public to the frontend.
  // Returns true if the location was translated, false otherwise.
  bool TranslateWasmScriptLocationToProtocolLocation(String16* script_id,
                                                     int* line_number,
                                                     int* column_number,
                                                     int context_group_id);

  // Translate back from protocol locations (potentially referencing artificial
  // scripts for individual wasm functions) to locations that make sense to V8.
  // Does nothing if the location was not generated by the translate method
  // above.
  // Returns true if the location was translated, false otherwise.
  bool TranslateProtocolLocationToWasmScriptLocation(String16* script_id,
                                                     int* line_number,
                                                     int* column_number);

 private:
  class TranslatorImpl;
  friend class TranslatorImpl;

  void AddFakeScript(std::unique_ptr<V8DebuggerScript> fake_script,
                     TranslatorImpl* translator, int context_group_id);

  v8::Isolate* isolate_;
  V8Debugger* debugger_;
  std::unordered_map<int, std::unique_ptr<TranslatorImpl>> wasm_translators_;
  std::unordered_map<String16, TranslatorImpl*> fake_scripts_;
  Mode mode_;

  DISALLOW_COPY_AND_ASSIGN(WasmTranslation);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_WASMTRANSLATION_H_
