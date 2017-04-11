// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "src/globals.h"
#include "src/interpreter/bytecode-peephole-table.h"
#include "src/interpreter/bytecodes.h"

namespace v8 {
namespace internal {

namespace interpreter {

const char* ActionName(PeepholeAction action) {
  switch (action) {
#define CASE(Name)              \
  case PeepholeAction::k##Name: \
    return "PeepholeAction::k" #Name;
    PEEPHOLE_ACTION_LIST(CASE)
#undef CASE
    default:
      UNREACHABLE();
      return "";
  }
}

std::string BytecodeName(Bytecode bytecode) {
  return "Bytecode::k" + std::string(Bytecodes::ToString(bytecode));
}

class PeepholeActionTableWriter final {
 public:
  static const size_t kNumberOfBytecodes =
      static_cast<size_t>(Bytecode::kLast) + 1;
  typedef std::array<PeepholeActionAndData, kNumberOfBytecodes> Row;

  void BuildTable();
  void Write(std::ostream& os);

 private:
  static const char* kIndent;
  static const char* kNamespaceElements[];

  void WriteHeader(std::ostream& os);
  void WriteIncludeFiles(std::ostream& os);
  void WriteClassMethods(std::ostream& os);
  void WriteUniqueRows(std::ostream& os);
  void WriteRowMap(std::ostream& os);
  void WriteRow(std::ostream& os, size_t row_index);
  void WriteOpenNamespace(std::ostream& os);
  void WriteCloseNamespace(std::ostream& os);

  PeepholeActionAndData LookupActionAndData(Bytecode last, Bytecode current);
  void BuildRow(Bytecode last, Row* row);
  size_t HashRow(const Row* row);
  void InsertRow(size_t row_index, const Row* const row, size_t row_hash,
                 std::map<size_t, size_t>* hash_to_row_map);
  bool RowsEqual(const Row* const first, const Row* const second);

  std::vector<Row>* table() { return &table_; }

  // Table of unique rows.
  std::vector<Row> table_;

  // Mapping of row index to unique row index.
  std::array<size_t, kNumberOfBytecodes> row_map_;
};

const char* PeepholeActionTableWriter::kIndent = "  ";
const char* PeepholeActionTableWriter::kNamespaceElements[] = {"v8", "internal",
                                                               "interpreter"};

// static
PeepholeActionAndData PeepholeActionTableWriter::LookupActionAndData(
    Bytecode last, Bytecode current) {
  // If there is no last bytecode to optimize against, store the incoming
  // bytecode or for jumps emit incoming bytecode immediately.
  if (last == Bytecode::kIllegal) {
    if (Bytecodes::IsJump(current)) {
      return {PeepholeAction::kUpdateLastJumpAction, Bytecode::kIllegal};
    } else if (current == Bytecode::kNop) {
      return {PeepholeAction::kUpdateLastIfSourceInfoPresentAction,
              Bytecode::kIllegal};
    } else {
      return {PeepholeAction::kUpdateLastAction, Bytecode::kIllegal};
    }
  }

  // No matches, take the default action.
  if (Bytecodes::IsJump(current)) {
    return {PeepholeAction::kDefaultJumpAction, Bytecode::kIllegal};
  } else {
    return {PeepholeAction::kDefaultAction, Bytecode::kIllegal};
  }
}

void PeepholeActionTableWriter::Write(std::ostream& os) {
  WriteHeader(os);
  WriteIncludeFiles(os);
  WriteOpenNamespace(os);
  WriteUniqueRows(os);
  WriteRowMap(os);
  WriteClassMethods(os);
  WriteCloseNamespace(os);
}

void PeepholeActionTableWriter::WriteHeader(std::ostream& os) {
  os << "// Copyright 2016 the V8 project authors. All rights reserved.\n"
     << "// Use of this source code is governed by a BSD-style license that\n"
     << "// can be found in the LICENSE file.\n\n"
     << "// Autogenerated by " __FILE__ ". Do not edit.\n\n";
}

void PeepholeActionTableWriter::WriteIncludeFiles(std::ostream& os) {
  os << "#include \"src/interpreter/bytecode-peephole-table.h\"\n\n";
}

void PeepholeActionTableWriter::WriteUniqueRows(std::ostream& os) {
  os << "const PeepholeActionAndData PeepholeActionTable::row_data_["
     << table_.size() << "][" << kNumberOfBytecodes << "] = {\n";
  for (size_t i = 0; i < table_.size(); ++i) {
    os << "{\n";
    WriteRow(os, i);
    os << "},\n";
  }
  os << "};\n\n";
}

void PeepholeActionTableWriter::WriteRowMap(std::ostream& os) {
  os << "const PeepholeActionAndData* const PeepholeActionTable::row_["
     << kNumberOfBytecodes << "] = {\n";
  for (size_t i = 0; i < kNumberOfBytecodes; ++i) {
    os << kIndent << " PeepholeActionTable::row_data_[" << row_map_[i]
       << "], \n";
  }
  os << "};\n\n";
}

void PeepholeActionTableWriter::WriteRow(std::ostream& os, size_t row_index) {
  const Row row = table_.at(row_index);
  for (PeepholeActionAndData action_data : row) {
    os << kIndent << "{" << ActionName(action_data.action) << ","
       << BytecodeName(action_data.bytecode) << "},\n";
  }
}

void PeepholeActionTableWriter::WriteOpenNamespace(std::ostream& os) {
  for (auto element : kNamespaceElements) {
    os << "namespace " << element << " {\n";
  }
  os << "\n";
}

void PeepholeActionTableWriter::WriteCloseNamespace(std::ostream& os) {
  for (auto element : kNamespaceElements) {
    os << "}  // namespace " << element << "\n";
  }
}

void PeepholeActionTableWriter::WriteClassMethods(std::ostream& os) {
  os << "// static\n"
     << "const PeepholeActionAndData*\n"
     << "PeepholeActionTable::Lookup(Bytecode last, Bytecode current) {\n"
     << kIndent
     << "return &row_[Bytecodes::ToByte(last)][Bytecodes::ToByte(current)];\n"
     << "}\n\n";
}

void PeepholeActionTableWriter::BuildTable() {
  std::map<size_t, size_t> hash_to_row_map;
  Row row;
  for (size_t i = 0; i < kNumberOfBytecodes; ++i) {
    uint8_t byte_value = static_cast<uint8_t>(i);
    Bytecode last = Bytecodes::FromByte(byte_value);
    BuildRow(last, &row);
    size_t row_hash = HashRow(&row);
    InsertRow(i, &row, row_hash, &hash_to_row_map);
  }
}

void PeepholeActionTableWriter::BuildRow(Bytecode last, Row* row) {
  for (size_t i = 0; i < kNumberOfBytecodes; ++i) {
    uint8_t byte_value = static_cast<uint8_t>(i);
    Bytecode current = Bytecodes::FromByte(byte_value);
    PeepholeActionAndData action_data = LookupActionAndData(last, current);
    row->at(i) = action_data;
  }
}

// static
bool PeepholeActionTableWriter::RowsEqual(const Row* const first,
                                          const Row* const second) {
  return memcmp(first, second, sizeof(*first)) == 0;
}

// static
void PeepholeActionTableWriter::InsertRow(
    size_t row_index, const Row* const row, size_t row_hash,
    std::map<size_t, size_t>* hash_to_row_map) {
  // Insert row if no existing row matches, otherwise use existing row.
  auto iter = hash_to_row_map->find(row_hash);
  if (iter == hash_to_row_map->end()) {
    row_map_[row_index] = table()->size();
    table()->push_back(*row);
  } else {
    row_map_[row_index] = iter->second;

    // If the following DCHECK fails, the HashRow() is not adequate.
    DCHECK(RowsEqual(&table()->at(iter->second), row));
  }
}

// static
size_t PeepholeActionTableWriter::HashRow(const Row* row) {
  static const size_t kHashShift = 3;
  std::size_t result = (1u << 31) - 1u;
  const uint8_t* raw_data = reinterpret_cast<const uint8_t*>(row);
  for (size_t i = 0; i < sizeof(*row); ++i) {
    size_t top_bits = result >> (kBitsPerByte * sizeof(size_t) - kHashShift);
    result = (result << kHashShift) ^ top_bits ^ raw_data[i];
  }
  return result;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

int main(int argc, const char* argv[]) {
  CHECK_EQ(argc, 2);

  std::ofstream ofs(argv[1], std::ofstream::trunc);
  v8::internal::interpreter::PeepholeActionTableWriter writer;
  writer.BuildTable();
  writer.Write(ofs);
  ofs.flush();
  ofs.close();

  return 0;
}
