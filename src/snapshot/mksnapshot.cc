// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <signal.h>
#include <stdio.h>

#include "include/libplatform/libplatform.h"
#include "src/assembler.h"
#include "src/base/platform/platform.h"
#include "src/flags.h"
#include "src/msan.h"
#include "src/snapshot/natives.h"
#include "src/snapshot/partial-serializer.h"
#include "src/snapshot/snapshot.h"
#include "src/snapshot/startup-serializer.h"

namespace {
class SnapshotWriter {
 public:
  SnapshotWriter()
      : snapshot_cpp_path_(nullptr), snapshot_blob_path_(nullptr) {}

#ifdef V8_EMBEDDED_BUILTINS
  void SetEmbeddedFile(const char* embedded_cpp_file) {
    embedded_cpp_path_ = embedded_cpp_file;
  }
#endif

  void SetSnapshotFile(const char* snapshot_cpp_file) {
    snapshot_cpp_path_ = snapshot_cpp_file;
  }

  void SetStartupBlobFile(const char* snapshot_blob_file) {
    snapshot_blob_path_ = snapshot_blob_file;
  }

  void WriteSnapshot(v8::StartupData blob) const {
    // TODO(crbug/633159): if we crash before the files have been fully created,
    // we end up with a corrupted snapshot file. The build step would succeed,
    // but the build target is unusable. Ideally we would write out temporary
    // files and only move them to the final destination as last step.
    i::Vector<const i::byte> blob_vector(
        reinterpret_cast<const i::byte*>(blob.data), blob.raw_size);
    MaybeWriteSnapshotFile(blob_vector);
    MaybeWriteStartupBlob(blob_vector);
  }

#ifdef V8_EMBEDDED_BUILTINS
  void WriteEmbedded(const i::EmbeddedData* blob) const {
    MaybeWriteEmbeddedFile(blob);
  }
#endif

 private:
  void MaybeWriteStartupBlob(const i::Vector<const i::byte>& blob) const {
    if (!snapshot_blob_path_) return;

    FILE* fp = GetFileDescriptorOrDie(snapshot_blob_path_);
    size_t written = fwrite(blob.begin(), 1, blob.length(), fp);
    fclose(fp);
    if (written != static_cast<size_t>(blob.length())) {
      i::PrintF("Writing snapshot file failed.. Aborting.\n");
      remove(snapshot_blob_path_);
      exit(1);
    }
  }

  void MaybeWriteSnapshotFile(const i::Vector<const i::byte>& blob) const {
    if (!snapshot_cpp_path_) return;

    FILE* fp = GetFileDescriptorOrDie(snapshot_cpp_path_);

    WriteSnapshotFilePrefix(fp);
    WriteSnapshotFileData(fp, blob);
    WriteSnapshotFileSuffix(fp);

    fclose(fp);
  }

  static void WriteSnapshotFilePrefix(FILE* fp) {
    fprintf(fp, "// Autogenerated snapshot file. Do not edit.\n\n");
    fprintf(fp, "#include \"src/v8.h\"\n");
    fprintf(fp, "#include \"src/base/platform/platform.h\"\n\n");
    fprintf(fp, "#include \"src/snapshot/snapshot.h\"\n\n");
    fprintf(fp, "namespace v8 {\n");
    fprintf(fp, "namespace internal {\n\n");
  }

  static void WriteSnapshotFileSuffix(FILE* fp) {
    fprintf(fp, "const v8::StartupData* Snapshot::DefaultSnapshotBlob() {\n");
    fprintf(fp, "  return &blob;\n");
    fprintf(fp, "}\n\n");
    fprintf(fp, "}  // namespace internal\n");
    fprintf(fp, "}  // namespace v8\n");
  }

  static void WriteSnapshotFileData(FILE* fp,
                                    const i::Vector<const i::byte>& blob) {
    fprintf(fp, "static const byte blob_data[] = {\n");
    WriteBinaryContentsAsCArray(fp, blob);
    fprintf(fp, "};\n");
    fprintf(fp, "static const int blob_size = %d;\n", blob.length());
    fprintf(fp, "static const v8::StartupData blob =\n");
    fprintf(fp, "{ (const char*) blob_data, blob_size };\n");
  }

  static void WriteBinaryContentsAsCArray(
      FILE* fp, const i::Vector<const i::byte>& blob) {
    for (int i = 0; i < blob.length(); i++) {
      if ((i & 0x1F) == 0x1F) fprintf(fp, "\n");
      if (i > 0) fprintf(fp, ",");
      fprintf(fp, "%u", static_cast<unsigned char>(blob.at(i)));
    }
    fprintf(fp, "\n");
  }

#ifdef V8_EMBEDDED_BUILTINS
  void MaybeWriteEmbeddedFile(const i::EmbeddedData* blob) const {
    if (embedded_cpp_path_ == nullptr) return;

    FILE* fp = GetFileDescriptorOrDie(embedded_cpp_path_);

    WriteEmbeddedFilePrefix(fp);
    WriteEmbeddedFileData(fp, blob);
    WriteEmbeddedFileSuffix(fp);

    fclose(fp);
  }

  static void WriteEmbeddedFilePrefix(FILE* fp) {
    fprintf(fp, "// Autogenerated file. Do not edit.\n\n");
    fprintf(fp, "#include <cstdint>\n\n");
    fprintf(fp, "#include \"src/snapshot/macros.h\"\n\n");
    fprintf(fp, "namespace v8 {\n");
    fprintf(fp, "namespace internal {\n\n");
    fprintf(fp, "namespace {\n\n");
  }

  static void WriteEmbeddedFileSuffix(FILE* fp) {
    fprintf(fp, "}  // namespace\n\n");
    fprintf(
        fp,
        "const uint8_t* DefaultEmbeddedBlob() { return v8_embedded_blob_; }\n");
    fprintf(fp,
            "uint32_t DefaultEmbeddedBlobSize() { return "
            "v8_embedded_blob_size_; }\n\n");
    fprintf(fp, "}  // namespace internal\n");
    fprintf(fp, "}  // namespace v8\n");
  }

  static void WriteEmbeddedFileData(FILE* fp, const i::EmbeddedData* blob) {
    // Note: On some platforms (observed on mac64), inserting labels into the
    // .byte stream causes the compiler to reorder symbols, invalidating stored
    // offsets.
    // We either need to avoid doing so, or stop relying on our own offset table
    // and directly reference symbols instead. But there is another complication
    // there since the chrome build process on mac verifies the order of symbols
    // present in the binary.
    // For now, the straight-forward solution seems to be to just emit a pure
    // .byte stream.
    fprintf(fp, "V8_EMBEDDED_TEXT_HEADER(v8_embedded_blob_)\n");
    WriteBinaryContentsAsByteDirective(fp, blob->data(), blob->size());
    fprintf(fp, "extern \"C\" const uint8_t v8_embedded_blob_[];\n");
    fprintf(fp, "static const uint32_t v8_embedded_blob_size_ = %d;\n\n",
            blob->size());
  }

  static void WriteBinaryContentsAsByteDirective(FILE* fp, const uint8_t* data,
                                                 uint32_t size) {
    static const int kTextWidth = 80;
    int current_line_length = 0;
    int printed_chars;

    fprintf(fp, "__asm__(\n");
    for (uint32_t i = 0; i < size; i++) {
      if (current_line_length == 0) {
        printed_chars = fprintf(fp, "%s", "  \".byte ");
        DCHECK_LT(0, printed_chars);
        current_line_length += printed_chars;
      } else {
        printed_chars = fprintf(fp, ",");
        DCHECK_EQ(1, printed_chars);
        current_line_length += printed_chars;
      }

      printed_chars = fprintf(fp, "0x%02x", data[i]);
      DCHECK_LT(0, printed_chars);
      current_line_length += printed_chars;

      if (current_line_length + strlen(",0xFF\\n\"") > kTextWidth) {
        fprintf(fp, "\\n\"\n");
        current_line_length = 0;
      }
    }

    if (current_line_length != 0) fprintf(fp, "\\n\"\n");
    fprintf(fp, ");\n");
  }
#endif

  static FILE* GetFileDescriptorOrDie(const char* filename) {
    FILE* fp = v8::base::OS::FOpen(filename, "wb");
    if (fp == nullptr) {
      i::PrintF("Unable to open file \"%s\" for writing.\n", filename);
      exit(1);
    }
    return fp;
  }

#ifdef V8_EMBEDDED_BUILTINS
  const char* embedded_cpp_path_ = nullptr;
#endif
  const char* snapshot_cpp_path_;
  const char* snapshot_blob_path_;
};

char* GetExtraCode(char* filename, const char* description) {
  if (filename == nullptr || strlen(filename) == 0) return nullptr;
  ::printf("Loading script for %s: %s\n", description, filename);
  FILE* file = v8::base::OS::FOpen(filename, "rb");
  if (file == nullptr) {
    fprintf(stderr, "Failed to open '%s': errno %d\n", filename, errno);
    exit(1);
  }
  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  rewind(file);
  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (size_t i = 0; i < size;) {
    size_t read = fread(&chars[i], 1, size - i, file);
    if (ferror(file)) {
      fprintf(stderr, "Failed to read '%s': errno %d\n", filename, errno);
      exit(1);
    }
    i += read;
  }
  fclose(file);
  return chars;
}

bool RunExtraCode(v8::Isolate* isolate, v8::Local<v8::Context> context,
                  const char* utf8_source, const char* name) {
  v8::base::ElapsedTimer timer;
  timer.Start();
  v8::Context::Scope context_scope(context);
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::String> source_string;
  if (!v8::String::NewFromUtf8(isolate, utf8_source, v8::NewStringType::kNormal)
           .ToLocal(&source_string)) {
    return false;
  }
  v8::Local<v8::String> resource_name =
      v8::String::NewFromUtf8(isolate, name, v8::NewStringType::kNormal)
          .ToLocalChecked();
  v8::ScriptOrigin origin(resource_name);
  v8::ScriptCompiler::Source source(source_string, origin);
  v8::Local<v8::Script> script;
  if (!v8::ScriptCompiler::Compile(context, &source).ToLocal(&script))
    return false;
  if (script->Run(context).IsEmpty()) return false;
  if (i::FLAG_profile_deserialization) {
    i::PrintF("Executing custom snapshot script %s took %0.3f ms\n", name,
              timer.Elapsed().InMillisecondsF());
  }
  timer.Stop();
  CHECK(!try_catch.HasCaught());
  return true;
}

v8::StartupData CreateSnapshotDataBlob(v8::SnapshotCreator* snapshot_creator,
                                       const char* script_source = NULL) {
  // Create a new isolate and a new context from scratch, optionally run
  // a script to embed, and serialize to create a snapshot blob.
  v8::StartupData result = {nullptr, 0};
  v8::base::ElapsedTimer timer;
  timer.Start();
  {
    v8::Isolate* isolate = snapshot_creator->GetIsolate();
    {
      v8::HandleScope scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      if (script_source != nullptr &&
          !RunExtraCode(isolate, context, script_source, "<embedded>")) {
        return result;
      }
      snapshot_creator->SetDefaultContext(context);
    }
    result = snapshot_creator->CreateBlob(
        v8::SnapshotCreator::FunctionCodeHandling::kClear);
  }

  if (i::FLAG_profile_deserialization) {
    i::PrintF("Creating snapshot took %0.3f ms\n",
              timer.Elapsed().InMillisecondsF());
  }
  timer.Stop();
  return result;
}

v8::StartupData WarmUpSnapshotDataBlob(v8::SnapshotCreator* snapshot_creator,
                                       const char* warmup_source) {
  CHECK_NOT_NULL(warmup_source);
  // Use following steps to create a warmed up snapshot blob from a cold one:
  //  - Create a new isolate from the cold snapshot.
  //  - Create a new context to run the warmup script. This will trigger
  //    compilation of executed functions.
  //  - Create a new context. This context will be unpolluted.
  //  - Serialize the isolate and the second context into a new snapshot blob.
  v8::StartupData result = {nullptr, 0};
  v8::base::ElapsedTimer timer;
  timer.Start();
  {
    v8::Isolate* isolate = snapshot_creator->GetIsolate();
    {
      v8::HandleScope scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      if (!RunExtraCode(isolate, context, warmup_source, "<warm-up>")) {
        return result;
      }
    }
    {
      v8::HandleScope handle_scope(isolate);
      isolate->ContextDisposedNotification(false);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      snapshot_creator->SetDefaultContext(context);
    }
    result = snapshot_creator->CreateBlob(
        v8::SnapshotCreator::FunctionCodeHandling::kKeep);
  }

  if (i::FLAG_profile_deserialization) {
    i::PrintF("Warming up snapshot took %0.3f ms\n",
              timer.Elapsed().InMillisecondsF());
  }
  timer.Stop();
  return result;
}

#ifdef V8_EMBEDDED_BUILTINS
void WriteEmbeddedFile(v8::SnapshotCreator* creator, SnapshotWriter* writer) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(creator->GetIsolate());
  isolate->PrepareEmbeddedBlobForSerialization();
  i::EmbeddedData embedded_blob = i::EmbeddedData::FromBlob(
      isolate->embedded_blob(), isolate->embedded_blob_size());
  writer->WriteEmbedded(&embedded_blob);
}
#endif  // V8_EMBEDDED_BUILTINS
}  // namespace

int main(int argc, char** argv) {
  // Make mksnapshot runs predictable to create reproducible snapshots.
  i::FLAG_predictable = true;

  // Print the usage if an error occurs when parsing the command line
  // flags or if the help flag is set.
  int result = i::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
  if (result > 0 || (argc > 3) || i::FLAG_help) {
    ::printf("Usage: %s --startup_src=... --startup_blob=... [extras]\n",
             argv[0]);
    i::FlagList::PrintHelp();
    return !i::FLAG_help;
  }

  i::CpuFeatures::Probe(true);
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();

  {
    SnapshotWriter writer;
    if (i::FLAG_startup_src) writer.SetSnapshotFile(i::FLAG_startup_src);
    if (i::FLAG_startup_blob) writer.SetStartupBlobFile(i::FLAG_startup_blob);
#ifdef V8_EMBEDDED_BUILTINS
    if (i::FLAG_embedded_src) writer.SetEmbeddedFile(i::FLAG_embedded_src);
#endif

    std::unique_ptr<char> embed_script(
        GetExtraCode(argc >= 2 ? argv[1] : nullptr, "embedding"));
    std::unique_ptr<char> warmup_script(
        GetExtraCode(argc >= 3 ? argv[2] : nullptr, "warm up"));

    v8::StartupData blob;
    {
      v8::SnapshotCreator snapshot_creator;
#ifdef V8_EMBEDDED_BUILTINS
      // This process is a bit tricky since we might go on to make a second
      // snapshot if a warmup script is passed. In that case, create the first
      // snapshot without off-heap trampolines and only move code off-heap for
      // the warmed-up snapshot.
      if (!warmup_script) WriteEmbeddedFile(&snapshot_creator, &writer);
#endif
      blob = CreateSnapshotDataBlob(&snapshot_creator, embed_script.get());
    }

    if (warmup_script) {
      CHECK(blob.raw_size > 0 && blob.data != nullptr);
      v8::StartupData cold = blob;
      v8::SnapshotCreator snapshot_creator(nullptr, &cold);
#ifdef V8_EMBEDDED_BUILTINS
      WriteEmbeddedFile(&snapshot_creator, &writer);
#endif
      blob = WarmUpSnapshotDataBlob(&snapshot_creator, warmup_script.get());
      delete[] cold.data;
    }

    CHECK(blob.data);
    writer.WriteSnapshot(blob);
    delete[] blob.data;
  }

  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  return 0;
}
