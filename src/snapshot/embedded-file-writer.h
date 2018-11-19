// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_EMBEDDED_FILE_WRITER_H_
#define V8_SNAPSHOT_EMBEDDED_FILE_WRITER_H_

#include <cstdio>
#include <cstring>

#include "src/globals.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

enum DataDirective {
  kByte,
  kLong,
  kQuad,
  kOcta,
};

static constexpr char kDefaultEmbeddedVariant[] = "Default";

// The platform-dependent logic for emitting assembly code for the generated
// embedded.S file.
class PlatformDependentEmbeddedFileWriter final {
 public:
  void SetFile(FILE* fp) { fp_ = fp; }

  void SectionText();
  void SectionData();
  void SectionRoData();

  void AlignToCodeAlignment();

  void DeclareUint32(const char* name, uint32_t value);
  void DeclarePointerToSymbol(const char* name, const char* target);

  void DeclareLabel(const char* name);

  void DeclareFunctionBegin(const char* name);
  void DeclareFunctionEnd(const char* name);

  // Returns the number of printed characters.
  int HexLiteral(uint64_t value);

  void Comment(const char* string);
  void Newline() { fprintf(fp_, "\n"); }

  void FilePrologue();
  void FileEpilogue();

  int IndentedDataDirective(DataDirective directive);

  FILE* fp() const { return fp_; }

 private:
  void DeclareSymbolGlobal(const char* name);

  static const char* DirectiveAsString(DataDirective directive);

 private:
  FILE* fp_ = nullptr;
};

// Generates the embedded.S file which is later compiled into the final v8
// binary. Its contents are exported through two symbols:
//
// v8_<variant>_embedded_blob_ (intptr_t):
//     a pointer to the start of the embedded blob.
// v8_<variant>_embedded_blob_size_ (uint32_t):
//     size of the embedded blob in bytes.
//
// The variant is usually "Default" but can be modified in multisnapshot builds.
class EmbeddedFileWriter {
 public:
  void SetEmbeddedFile(const char* embedded_cpp_file) {
    embedded_cpp_path_ = embedded_cpp_file;
  }

  void SetEmbeddedVariant(const char* embedded_variant) {
    embedded_variant_ = embedded_variant;
  }

  void WriteEmbedded(const i::EmbeddedData* blob) const {
    MaybeWriteEmbeddedFile(blob);
  }

 private:
  void MaybeWriteEmbeddedFile(const i::EmbeddedData* blob) const {
    if (embedded_cpp_path_ == nullptr) return;

    FILE* fp = GetFileDescriptorOrDie(embedded_cpp_path_);

    PlatformDependentEmbeddedFileWriter writer;
    writer.SetFile(fp);

    WriteFilePrologue(&writer);
    WriteMetadataSection(&writer, blob);
    WriteInstructionStreams(&writer, blob);
    WriteFileEpilogue(&writer, blob);

    fclose(fp);
  }

  static FILE* GetFileDescriptorOrDie(const char* filename) {
    FILE* fp = v8::base::OS::FOpen(filename, "wb");
    if (fp == nullptr) {
      i::PrintF("Unable to open file \"%s\" for writing.\n", filename);
      exit(1);
    }
    return fp;
  }

  static void WriteFilePrologue(PlatformDependentEmbeddedFileWriter* w) {
    w->Comment("Autogenerated file. Do not edit.");
    w->Newline();
    w->FilePrologue();
  }

  // Fairly arbitrary but should fit all symbol names.
  static constexpr int kTemporaryStringLength = 256;

  void WriteMetadataSection(PlatformDependentEmbeddedFileWriter* w,
                            const i::EmbeddedData* blob) const {
    char embedded_blob_data_symbol[kTemporaryStringLength];
    i::SNPrintF(i::Vector<char>(embedded_blob_data_symbol),
                "v8_%s_embedded_blob_data_", embedded_variant_);

    w->Comment("The embedded blob starts here. Metadata comes first, followed");
    w->Comment("by builtin instruction streams.");
    w->SectionText();
    w->AlignToCodeAlignment();
    w->DeclareLabel(embedded_blob_data_symbol);

    WriteBinaryContentsAsInlineAssembly(w, blob->data(),
                                        i::EmbeddedData::RawDataOffset());
  }

  void WriteInstructionStreams(PlatformDependentEmbeddedFileWriter* w,
                               const i::EmbeddedData* blob) const {
    const bool is_default_variant =
        std::strcmp(embedded_variant_, kDefaultEmbeddedVariant) == 0;

    for (int i = 0; i < i::Builtins::builtin_count; i++) {
      if (!blob->ContainsBuiltin(i)) continue;

      char builtin_symbol[kTemporaryStringLength];
      if (is_default_variant) {
        // Create nicer symbol names for the default mode.
        i::SNPrintF(i::Vector<char>(builtin_symbol), "Builtins_%s",
                    i::Builtins::name(i));
      } else {
        i::SNPrintF(i::Vector<char>(builtin_symbol), "%s_Builtins_%s",
                    embedded_variant_, i::Builtins::name(i));
      }

      // Labels created here will show up in backtraces. We check in
      // Isolate::SetEmbeddedBlob that the blob layout remains unchanged, i.e.
      // that labels do not insert bytes into the middle of the blob byte
      // stream.
      w->DeclareFunctionBegin(builtin_symbol);
      WriteBinaryContentsAsInlineAssembly(
          w,
          reinterpret_cast<const uint8_t*>(blob->InstructionStartOfBuiltin(i)),
          blob->PaddedInstructionSizeOfBuiltin(i));
      w->DeclareFunctionEnd(builtin_symbol);
    }
    w->Newline();
  }

  void WriteFileEpilogue(PlatformDependentEmbeddedFileWriter* w,
                         const i::EmbeddedData* blob) const {
    {
      char embedded_blob_data_symbol[kTemporaryStringLength];
      i::SNPrintF(i::Vector<char>(embedded_blob_data_symbol),
                  "v8_%s_embedded_blob_data_", embedded_variant_);

      char embedded_blob_symbol[kTemporaryStringLength];
      i::SNPrintF(i::Vector<char>(embedded_blob_symbol), "v8_%s_embedded_blob_",
                  embedded_variant_);

      w->Comment("Pointer to the beginning of the embedded blob.");
      w->SectionData();
      w->DeclarePointerToSymbol(embedded_blob_symbol,
                                embedded_blob_data_symbol);
      w->Newline();
    }

    {
      char embedded_blob_size_symbol[kTemporaryStringLength];
      i::SNPrintF(i::Vector<char>(embedded_blob_size_symbol),
                  "v8_%s_embedded_blob_size_", embedded_variant_);

      w->Comment("The size of the embedded blob in bytes.");
      w->SectionRoData();
      w->DeclareUint32(embedded_blob_size_symbol, blob->size());
      w->Newline();
    }

    w->FileEpilogue();
  }

#ifdef V8_OS_WIN
  // Windows MASM doesn't have an .octa directive, use QWORDs instead.
  // Note: MASM *really* does not like large data streams. It takes over 5
  // minutes to assemble the ~350K lines of embedded.S produced when using
  // BYTE directives in a debug build. QWORD produces roughly 120KLOC and
  // reduces assembly time to ~40 seconds. Still terrible, but much better
  // than before.
  // TODO(v8:8475): Use nasm or yasm instead of MASM.

  static constexpr DataDirective kByteChunkDirective = kQuad;
  static constexpr int kByteChunkSize = 8;

  static int WriteByteChunk(PlatformDependentEmbeddedFileWriter* w,
                            int current_line_length, const uint8_t* data) {
    const uint64_t* quad_ptr = reinterpret_cast<const uint64_t*>(data);
    return current_line_length + w->HexLiteral(*quad_ptr);
  }
#else  // V8_OS_WIN
  static constexpr DataDirective kByteChunkDirective = kOcta;
  static constexpr int kByteChunkSize = 16;

  static int WriteByteChunk(PlatformDependentEmbeddedFileWriter* w,
                            int current_line_length, const uint8_t* data) {
    const uint64_t* quad_ptr1 = reinterpret_cast<const uint64_t*>(data);
    const uint64_t* quad_ptr2 = reinterpret_cast<const uint64_t*>(data + 8);

#ifdef V8_TARGET_BIG_ENDIAN
    uint64_t part1 = *quad_ptr1;
    uint64_t part2 = *quad_ptr2;
#else
    uint64_t part1 = *quad_ptr2;
    uint64_t part2 = *quad_ptr1;
#endif  // V8_TARGET_BIG_ENDIAN

    if (part1 != 0) {
      current_line_length +=
          fprintf(w->fp(), "0x%" PRIx64 "%016" PRIx64, part1, part2);
    } else {
      current_line_length += fprintf(w->fp(), "0x%" PRIx64, part2);
    }
    return current_line_length;
  }
#endif  // V8_OS_WIN

  static int WriteDirectiveOrSeparator(PlatformDependentEmbeddedFileWriter* w,
                                       int current_line_length,
                                       DataDirective directive) {
    int printed_chars;
    if (current_line_length == 0) {
      printed_chars = w->IndentedDataDirective(directive);
      DCHECK_LT(0, printed_chars);
    } else {
      printed_chars = fprintf(w->fp(), ",");
      DCHECK_EQ(1, printed_chars);
    }
    return current_line_length + printed_chars;
  }

  static int WriteLineEndIfNeeded(PlatformDependentEmbeddedFileWriter* w,
                                  int current_line_length, int write_size) {
    static const int kTextWidth = 100;
    // Check if adding ',0xFF...FF\n"' would force a line wrap. This doesn't use
    // the actual size of the string to be written to determine this so it's
    // more conservative than strictly needed.
    if (current_line_length + strlen(",0x") + write_size * 2 > kTextWidth) {
      fprintf(w->fp(), "\n");
      return 0;
    } else {
      return current_line_length;
    }
  }

  static void WriteBinaryContentsAsInlineAssembly(
      PlatformDependentEmbeddedFileWriter* w, const uint8_t* data,
      uint32_t size) {
    int current_line_length = 0;

    uint32_t i = 0;

    // Begin by writing out byte chunks.
    for (; i <= size - kByteChunkSize; i += kByteChunkSize) {
      current_line_length = WriteDirectiveOrSeparator(w, current_line_length,
                                                      kByteChunkDirective);
      current_line_length = WriteByteChunk(w, current_line_length, data + i);
      current_line_length =
          WriteLineEndIfNeeded(w, current_line_length, kByteChunkSize);
    }
    if (current_line_length != 0) w->Newline();
    current_line_length = 0;

    // Write any trailing bytes one-by-one.
    for (; i < size; i++) {
      current_line_length =
          WriteDirectiveOrSeparator(w, current_line_length, kByte);
      current_line_length += w->HexLiteral(data[i]);
      current_line_length = WriteLineEndIfNeeded(w, current_line_length, 1);
    }
    if (current_line_length != 0) w->Newline();
  }

  const char* embedded_cpp_path_ = nullptr;
  const char* embedded_variant_ = kDefaultEmbeddedVariant;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_EMBEDDED_FILE_WRITER_H_
