#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <cinttypes>

#include "wasm.hh"


auto get_export_global(wasm::vec<wasm::Extern*>& exports, size_t i) -> wasm::Global* {
  if (exports.size() <= i || !exports[i]->global()) {
    std::cout << "> Error accessing global export " << i << "!" << std::endl;
    exit(1);
  }
  return exports[i]->global();
}

auto get_export_func(const wasm::vec<wasm::Extern*>& exports, size_t i) -> const wasm::Func* {
  if (exports.size() <= i || !exports[i]->func()) {
    std::cout << "> Error accessing function export " << i << "!" << std::endl;
    exit(1);
  }
  return exports[i]->func();
}

template<class T, class U>
void check(T actual, U expected) {
  if (actual != expected) {
    std::cout << "> Error reading value, expected " << expected << ", got " << actual << std::endl;
    exit(1);
  }
}

auto call(const wasm::Func* func) -> wasm::Val {
  wasm::Val results[1];
  if (func->call(nullptr, results)) {
    std::cout << "> Error calling function!" << std::endl;
    exit(1);
  }
  return results[0].copy();
}

void call(const wasm::Func* func, wasm::Val&& arg) {
  wasm::Val args[1] = {std::move(arg)};
  if (func->call(args)) {
    std::cout << "> Error calling function!" << std::endl;
    exit(1);
  }
}


void run() {
  // Initialize.
  std::cout << "Initializing..." << std::endl;
  auto engine = wasm::Engine::make();
  auto store_ = wasm::Store::make(engine.get());
  auto store = store_.get();

  // Load binary.
  std::cout << "Loading binary..." << std::endl;
  std::ifstream file("global.wasm");
  file.seekg(0, std::ios_base::end);
  auto file_size = file.tellg();
  file.seekg(0);
  auto binary = wasm::vec<byte_t>::make_uninitialized(file_size);
  file.read(binary.get(), file_size);
  file.close();
  if (file.fail()) {
    std::cout << "> Error loading module!" << std::endl;
    return;
  }

  // Compile.
  std::cout << "Compiling module..." << std::endl;
  auto module = wasm::Module::make(store, binary);
  if (!module) {
    std::cout << "> Error compiling module!" << std::endl;
    return;
  }

  // Create external globals.
  std::cout << "Creating globals..." << std::endl;
  auto const_f32_type = wasm::GlobalType::make(
    wasm::ValType::make(wasm::F32), wasm::CONST);
  auto const_i64_type = wasm::GlobalType::make(
    wasm::ValType::make(wasm::I64), wasm::CONST);
  auto var_f32_type = wasm::GlobalType::make(
    wasm::ValType::make(wasm::F32), wasm::VAR);
  auto var_i64_type = wasm::GlobalType::make(
    wasm::ValType::make(wasm::I64), wasm::VAR);
  auto const_f32_import = wasm::Global::make(store, const_f32_type.get(), wasm::Val::f32(1));
  auto const_i64_import = wasm::Global::make(store, const_i64_type.get(), wasm::Val::i64(2));
  auto var_f32_import = wasm::Global::make(store, var_f32_type.get(), wasm::Val::f32(3));
  auto var_i64_import = wasm::Global::make(store, var_i64_type.get(), wasm::Val::i64(4));

  // Instantiate.
  std::cout << "Instantiating module..." << std::endl;
  wasm::Extern* imports[] = {
    const_f32_import.get(), const_i64_import.get(),
    var_f32_import.get(), var_i64_import.get()
  };
  auto instance = wasm::Instance::make(store, module.get(), imports);
  if (!instance) {
    std::cout << "> Error instantiating module!" << std::endl;
    return;
  }

  // Extract export.
  std::cout << "Extracting exports..." << std::endl;
  auto exports = instance->exports();
  size_t i = 0;
  auto const_f32_export = get_export_global(exports, i++);
  auto const_i64_export = get_export_global(exports, i++);
  auto var_f32_export = get_export_global(exports, i++);
  auto var_i64_export = get_export_global(exports, i++);
  auto get_const_f32_import = get_export_func(exports, i++);
  auto get_const_i64_import = get_export_func(exports, i++);
  auto get_var_f32_import = get_export_func(exports, i++);
  auto get_var_i64_import = get_export_func(exports, i++);
  auto get_const_f32_export = get_export_func(exports, i++);
  auto get_const_i64_export = get_export_func(exports, i++);
  auto get_var_f32_export = get_export_func(exports, i++);
  auto get_var_i64_export = get_export_func(exports, i++);
  auto set_var_f32_import = get_export_func(exports, i++);
  auto set_var_i64_import = get_export_func(exports, i++);
  auto set_var_f32_export = get_export_func(exports, i++);
  auto set_var_i64_export = get_export_func(exports, i++);

  // Try cloning.
  assert(var_f32_import->copy()->same(var_f32_import.get()));

  // Interact.
  std::cout << "Accessing globals..." << std::endl;

  // Check initial values.
  check(const_f32_import->get().f32(), 1);
  check(const_i64_import->get().i64(), 2);
  check(var_f32_import->get().f32(), 3);
  check(var_i64_import->get().i64(), 4);
  check(const_f32_export->get().f32(), 5);
  check(const_i64_export->get().i64(), 6);
  check(var_f32_export->get().f32(), 7);
  check(var_i64_export->get().i64(), 8);

  check(call(get_const_f32_import).f32(), 1);
  check(call(get_const_i64_import).i64(), 2);
  check(call(get_var_f32_import).f32(), 3);
  check(call(get_var_i64_import).i64(), 4);
  check(call(get_const_f32_export).f32(), 5);
  check(call(get_const_i64_export).i64(), 6);
  check(call(get_var_f32_export).f32(), 7);
  check(call(get_var_i64_export).i64(), 8);

  // Modify variables through API and check again.
  var_f32_import->set(wasm::Val::f32(33));
  var_i64_import->set(wasm::Val::i64(34));
  var_f32_export->set(wasm::Val::f32(37));
  var_i64_export->set(wasm::Val::i64(38));

  check(var_f32_import->get().f32(), 33);
  check(var_i64_import->get().i64(), 34);
  check(var_f32_export->get().f32(), 37);
  check(var_i64_export->get().i64(), 38);

  check(call(get_var_f32_import).f32(), 33);
  check(call(get_var_i64_import).i64(), 34);
  check(call(get_var_f32_export).f32(), 37);
  check(call(get_var_i64_export).i64(), 38);

  // Modify variables through calls and check again.
  call(set_var_f32_import, wasm::Val::f32(73));
  call(set_var_i64_import, wasm::Val::i64(74));
  call(set_var_f32_export, wasm::Val::f32(77));
  call(set_var_i64_export, wasm::Val::i64(78));

  check(var_f32_import->get().f32(), 73);
  check(var_i64_import->get().i64(), 74);
  check(var_f32_export->get().f32(), 77);
  check(var_i64_export->get().i64(), 78);

  check(call(get_var_f32_import).f32(), 73);
  check(call(get_var_i64_import).i64(), 74);
  check(call(get_var_f32_export).f32(), 77);
  check(call(get_var_i64_export).i64(), 78);

  // Shut down.
  std::cout << "Shutting down..." << std::endl;
}


int main(int argc, const char* argv[]) {
  run();
  std::cout << "Done." << std::endl;
  return 0;
}

