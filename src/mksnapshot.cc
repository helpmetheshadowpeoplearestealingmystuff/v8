// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifdef COMPRESS_STARTUP_DATA_BZ2
#include <bzlib.h>
#endif
#include <signal.h>
#include <string>
#include <map>

#include "v8.h"

#include "bootstrapper.h"
#include "natives.h"
#include "platform.h"
#include "serialize.h"
#include "list.h"

// use explicit namespace to avoid clashing with types in namespace v8
namespace i = v8::internal;
using namespace v8;

static const unsigned int kMaxCounters = 256;

// A single counter in a counter collection.
class Counter {
 public:
  static const int kMaxNameSize = 64;
  int32_t* Bind(const char* name) {
    int i;
    for (i = 0; i < kMaxNameSize - 1 && name[i]; i++) {
      name_[i] = name[i];
    }
    name_[i] = '\0';
    return &counter_;
  }
 private:
  int32_t counter_;
  uint8_t name_[kMaxNameSize];
};


// A set of counters and associated information.  An instance of this
// class is stored directly in the memory-mapped counters file if
// the --save-counters options is used
class CounterCollection {
 public:
  CounterCollection() {
    magic_number_ = 0xDEADFACE;
    max_counters_ = kMaxCounters;
    max_name_size_ = Counter::kMaxNameSize;
    counters_in_use_ = 0;
  }
  Counter* GetNextCounter() {
    if (counters_in_use_ == kMaxCounters) return NULL;
    return &counters_[counters_in_use_++];
  }
 private:
  uint32_t magic_number_;
  uint32_t max_counters_;
  uint32_t max_name_size_;
  uint32_t counters_in_use_;
  Counter counters_[kMaxCounters];
};


// We statically allocate a set of local counters to be used if we
// don't want to store the stats in a memory-mapped file
static CounterCollection local_counters;


typedef std::map<std::string, int*> CounterMap;
typedef std::map<std::string, int*>::iterator CounterMapIterator;
static CounterMap counter_table_;


class Compressor {
 public:
  virtual ~Compressor() {}
  virtual bool Compress(i::Vector<char> input) = 0;
  virtual i::Vector<char>* output() = 0;
};


class PartialSnapshotSink : public i::SnapshotByteSink {
 public:
  PartialSnapshotSink() : data_(), raw_size_(-1) { }
  virtual ~PartialSnapshotSink() { data_.Free(); }
  virtual void Put(int byte, const char* description) {
    data_.Add(byte);
  }
  virtual int Position() { return data_.length(); }
  void Print(FILE* fp) {
    int length = Position();
    for (int j = 0; j < length; j++) {
      if ((j & 0x1f) == 0x1f) {
        fprintf(fp, "\n");
      }
      if (j != 0) {
        fprintf(fp, ",");
      }
      fprintf(fp, "%d", at(j));
    }
  }
  char at(int i) { return data_[i]; }
  bool Compress(Compressor* compressor) {
    ASSERT_EQ(-1, raw_size_);
    raw_size_ = data_.length();
    if (!compressor->Compress(data_.ToVector())) return false;
    data_.Clear();
    data_.AddAll(*compressor->output());
    return true;
  }
  int raw_size() { return raw_size_; }

 private:
  i::List<char> data_;
  int raw_size_;
};


class CppByteSink : public PartialSnapshotSink {
 public:
  explicit CppByteSink(const char* snapshot_file) {
    fp_ = i::OS::FOpen(snapshot_file, "wb");
    if (fp_ == NULL) {
      i::PrintF("Unable to write to snapshot file \"%s\"\n", snapshot_file);
      exit(1);
    }
    fprintf(fp_, "// Autogenerated snapshot file. Do not edit.\n\n");
    fprintf(fp_, "#include \"v8.h\"\n");
    fprintf(fp_, "#include \"platform.h\"\n\n");
    fprintf(fp_, "#include \"snapshot.h\"\n\n");
    fprintf(fp_, "namespace v8 {\nnamespace internal {\n\n");
    fprintf(fp_, "const byte Snapshot::data_[] = {");
  }

  virtual ~CppByteSink() {
    fprintf(fp_, "const int Snapshot::size_ = %d;\n", Position());
#ifdef COMPRESS_STARTUP_DATA_BZ2
    fprintf(fp_, "const byte* Snapshot::raw_data_ = NULL;\n");
    fprintf(fp_,
            "const int Snapshot::raw_size_ = %d;\n\n",
            raw_size());
#else
    fprintf(fp_,
            "const byte* Snapshot::raw_data_ = Snapshot::data_;\n");
    fprintf(fp_,
            "const int Snapshot::raw_size_ = Snapshot::size_;\n\n");
#endif
    fprintf(fp_, "} }  // namespace v8::internal\n");
    fclose(fp_);
  }

  void WriteSpaceUsed(
      int new_space_used,
      int pointer_space_used,
      int data_space_used,
      int code_space_used,
      int map_space_used,
      int cell_space_used,
      int large_space_used) {
    fprintf(fp_, "const int Snapshot::new_space_used_ = %d;\n", new_space_used);
    fprintf(fp_,
            "const int Snapshot::pointer_space_used_ = %d;\n",
            pointer_space_used);
    fprintf(fp_,
            "const int Snapshot::data_space_used_ = %d;\n",
            data_space_used);
    fprintf(fp_,
            "const int Snapshot::code_space_used_ = %d;\n",
            code_space_used);
    fprintf(fp_, "const int Snapshot::map_space_used_ = %d;\n", map_space_used);
    fprintf(fp_,
            "const int Snapshot::cell_space_used_ = %d;\n",
            cell_space_used);
    fprintf(fp_,
            "const int Snapshot::large_space_used_ = %d;\n",
            large_space_used);
  }

  void WritePartialSnapshot() {
    int length = partial_sink_.Position();
    fprintf(fp_, "};\n\n");
    fprintf(fp_, "const int Snapshot::context_size_ = %d;\n",  length);
#ifdef COMPRESS_STARTUP_DATA_BZ2
    fprintf(fp_,
            "const int Snapshot::context_raw_size_ = %d;\n",
            partial_sink_.raw_size());
#else
    fprintf(fp_,
            "const int Snapshot::context_raw_size_ = "
            "Snapshot::context_size_;\n");
#endif
    fprintf(fp_, "const byte Snapshot::context_data_[] = {\n");
    partial_sink_.Print(fp_);
    fprintf(fp_, "};\n\n");
#ifdef COMPRESS_STARTUP_DATA_BZ2
    fprintf(fp_, "const byte* Snapshot::context_raw_data_ = NULL;\n");
#else
    fprintf(fp_, "const byte* Snapshot::context_raw_data_ ="
            " Snapshot::context_data_;\n");
#endif
  }

  void WriteSnapshot() {
    Print(fp_);
  }

  PartialSnapshotSink* partial_sink() { return &partial_sink_; }

 private:
  FILE* fp_;
  PartialSnapshotSink partial_sink_;
};


#ifdef COMPRESS_STARTUP_DATA_BZ2
class BZip2Compressor : public Compressor {
 public:
  BZip2Compressor() : output_(NULL) {}
  virtual ~BZip2Compressor() {
    delete output_;
  }
  virtual bool Compress(i::Vector<char> input) {
    delete output_;
    output_ = new i::ScopedVector<char>((input.length() * 101) / 100 + 1000);
    unsigned int output_length_ = output_->length();
    int result = BZ2_bzBuffToBuffCompress(output_->start(), &output_length_,
                                          input.start(), input.length(),
                                          9, 1, 0);
    if (result == BZ_OK) {
      output_->Truncate(output_length_);
      return true;
    } else {
      fprintf(stderr, "bzlib error code: %d\n", result);
      return false;
    }
  }
  virtual i::Vector<char>* output() { return output_; }

 private:
  i::ScopedVector<char>* output_;
};


class BZip2Decompressor : public StartupDataDecompressor {
 public:
  virtual ~BZip2Decompressor() { }

 protected:
  virtual int DecompressData(char* raw_data,
                             int* raw_data_size,
                             const char* compressed_data,
                             int compressed_data_size) {
    ASSERT_EQ(StartupData::kBZip2,
              V8::GetCompressedStartupDataAlgorithm());
    unsigned int decompressed_size = *raw_data_size;
    int result =
        BZ2_bzBuffToBuffDecompress(raw_data,
                                   &decompressed_size,
                                   const_cast<char*>(compressed_data),
                                   compressed_data_size,
                                   0, 1);
    if (result == BZ_OK) {
      *raw_data_size = decompressed_size;
    }
    return result;
  }
};
#endif


int main(int argc, char** argv) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  // By default, log code create information in the snapshot.
  i::FLAG_log_code = true;
#endif
  // Print the usage if an error occurs when parsing the command line
  // flags or if the help flag is set.
  int result = i::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
  if (result > 0 || argc != 2 || i::FLAG_help) {
    ::printf("Usage: %s [flag] ... outfile\n", argv[0]);
    i::FlagList::PrintHelp();
    return !i::FLAG_help;
  }
#ifdef COMPRESS_STARTUP_DATA_BZ2
  BZip2Decompressor natives_decompressor;
  int bz2_result = natives_decompressor.Decompress();
  if (bz2_result != BZ_OK) {
    fprintf(stderr, "bzip error code: %d\n", bz2_result);
    exit(1);
  }
#endif
  i::Serializer::Enable();
  Persistent<Context> context = v8::Context::New();
  ASSERT(!context.IsEmpty());
  // Make sure all builtin scripts are cached.
  { HandleScope scope;
    for (int i = 0; i < i::Natives::GetBuiltinsCount(); i++) {
      i::Isolate::Current()->bootstrapper()->NativesSourceLookup(i);
    }
  }
  // If we don't do this then we end up with a stray root pointing at the
  // context even after we have disposed of the context.
  HEAP->CollectAllGarbage(true);
  i::Object* raw_context = *(v8::Utils::OpenHandle(*context));
  context.Dispose();
  CppByteSink sink(argv[1]);
  // This results in a somewhat smaller snapshot, probably because it gets rid
  // of some things that are cached between garbage collections.
  i::StartupSerializer ser(&sink);
  ser.SerializeStrongReferences();

  i::PartialSerializer partial_ser(&ser, sink.partial_sink());
  partial_ser.Serialize(&raw_context);

  ser.SerializeWeakReferences();

#ifdef COMPRESS_STARTUP_DATA_BZ2
  BZip2Compressor compressor;
  if (!sink.Compress(&compressor))
    return 1;
  if (!sink.partial_sink()->Compress(&compressor))
    return 1;
#endif
  sink.WriteSnapshot();
  sink.WritePartialSnapshot();

  sink.WriteSpaceUsed(
      partial_ser.CurrentAllocationAddress(i::NEW_SPACE),
      partial_ser.CurrentAllocationAddress(i::OLD_POINTER_SPACE),
      partial_ser.CurrentAllocationAddress(i::OLD_DATA_SPACE),
      partial_ser.CurrentAllocationAddress(i::CODE_SPACE),
      partial_ser.CurrentAllocationAddress(i::MAP_SPACE),
      partial_ser.CurrentAllocationAddress(i::CELL_SPACE),
      partial_ser.CurrentAllocationAddress(i::LO_SPACE));
  return 0;
}
