// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_INSTRUCTIONS_H_
#define V8_TORQUE_INSTRUCTIONS_H_

#include <memory>

#include "src/torque/ast.h"
#include "src/torque/source-positions.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

class Block;
class Builtin;
class ControlFlowGraph;
class Intrinsic;
class Macro;
class NamespaceConstant;
class RuntimeFunction;

// Instructions where all backends generate code the same way.
#define TORQUE_BACKEND_AGNOSTIC_INSTRUCTION_LIST(V) \
  V(PeekInstruction)                                \
  V(PokeInstruction)                                \
  V(DeleteRangeInstruction)

// Instructions where different backends may generate different code.
#define TORQUE_BACKEND_DEPENDENT_INSTRUCTION_LIST(V) \
  V(PushUninitializedInstruction)                    \
  V(PushBuiltinPointerInstruction)                   \
  V(LoadReferenceInstruction)                        \
  V(StoreReferenceInstruction)                       \
  V(LoadBitFieldInstruction)                         \
  V(StoreBitFieldInstruction)                        \
  V(CallCsaMacroInstruction)                         \
  V(CallIntrinsicInstruction)                        \
  V(NamespaceConstantInstruction)                    \
  V(CallCsaMacroAndBranchInstruction)                \
  V(CallBuiltinInstruction)                          \
  V(CallRuntimeInstruction)                          \
  V(CallBuiltinPointerInstruction)                   \
  V(BranchInstruction)                               \
  V(ConstexprBranchInstruction)                      \
  V(GotoInstruction)                                 \
  V(GotoExternalInstruction)                         \
  V(ReturnInstruction)                               \
  V(PrintConstantStringInstruction)                  \
  V(AbortInstruction)                                \
  V(UnsafeCastInstruction)

#define TORQUE_INSTRUCTION_LIST(V)            \
  TORQUE_BACKEND_AGNOSTIC_INSTRUCTION_LIST(V) \
  TORQUE_BACKEND_DEPENDENT_INSTRUCTION_LIST(V)

#define TORQUE_INSTRUCTION_BOILERPLATE()                                  \
  static const InstructionKind kKind;                                     \
  std::unique_ptr<InstructionBase> Clone() const override;                \
  void Assign(const InstructionBase& other) override;                     \
  void TypeInstruction(Stack<const Type*>* stack, ControlFlowGraph* cfg)  \
      const override;                                                     \
  void RecomputeDefinitionLocations(Stack<DefinitionLocation>* locations, \
                                    Worklist<Block*>* worklist)           \
      const override;

enum class InstructionKind {
#define ENUM_ITEM(name) k##name,
  TORQUE_INSTRUCTION_LIST(ENUM_ITEM)
#undef ENUM_ITEM
};

struct InstructionBase;

class DefinitionLocation {
 public:
  enum class Kind {
    kInvalid,
    kParameter,
    kPhi,
    kInstruction,
  };

  DefinitionLocation() : kind_(Kind::kInvalid), location_(nullptr), index_(0) {}

  static DefinitionLocation Parameter(std::size_t index) {
    return DefinitionLocation(Kind::kParameter, nullptr, index);
  }

  static DefinitionLocation Phi(const Block* block, std::size_t index) {
    return DefinitionLocation(Kind::kPhi, block, index);
  }

  static DefinitionLocation Instruction(const InstructionBase* instruction,
                                        std::size_t index = 0) {
    return DefinitionLocation(Kind::kInstruction, instruction, index);
  }

  Kind GetKind() const { return kind_; }
  bool IsValid() const { return kind_ != Kind::kInvalid; }
  bool IsParameter() const { return kind_ == Kind::kParameter; }
  bool IsPhi() const { return kind_ == Kind::kPhi; }
  bool IsInstruction() const { return kind_ == Kind::kInstruction; }

  std::size_t GetParameterIndex() const {
    DCHECK(IsParameter());
    return index_;
  }

  const Block* GetPhiBlock() const {
    DCHECK(IsPhi());
    return reinterpret_cast<const Block*>(location_);
  }

  bool IsPhiFromBlock(const Block* block) const {
    return IsPhi() && GetPhiBlock() == block;
  }

  std::size_t GetPhiIndex() const {
    DCHECK(IsPhi());
    return index_;
  }

  const InstructionBase* GetInstruction() const {
    DCHECK(IsInstruction());
    return reinterpret_cast<const InstructionBase*>(location_);
  }

  std::size_t GetInstructionIndex() const {
    DCHECK(IsInstruction());
    return index_;
  }

  bool operator==(const DefinitionLocation& other) const {
    if (kind_ != other.kind_) return false;
    if (location_ != other.location_) return false;
    return index_ == other.index_;
  }

  bool operator!=(const DefinitionLocation& other) const {
    return !operator==(other);
  }

  bool operator<(const DefinitionLocation& other) const {
    if (kind_ != other.kind_) {
      return static_cast<int>(kind_) < static_cast<int>(other.kind_);
    }
    if (location_ != other.location_) {
      return location_ < other.location_;
    }
    return index_ < other.index_;
  }

 private:
  DefinitionLocation(Kind kind, const void* location, std::size_t index)
      : kind_(kind), location_(location), index_(index) {}

  Kind kind_;
  const void* location_;
  std::size_t index_;
};

inline std::ostream& operator<<(std::ostream& stream,
                                const DefinitionLocation& loc) {
  switch (loc.GetKind()) {
    case DefinitionLocation::Kind::kInvalid:
      return stream << "DefinitionLocation::Invalid()";
    case DefinitionLocation::Kind::kParameter:
      return stream << "DefinitionLocation::Parameter("
                    << loc.GetParameterIndex() << ")";
    case DefinitionLocation::Kind::kPhi:
      return stream << "DefinitionLocation::Phi(" << std::hex
                    << loc.GetPhiBlock() << std::dec << ", "
                    << loc.GetPhiIndex() << ")";
    case DefinitionLocation::Kind::kInstruction:
      return stream << "DefinitionLocation::Instruction(" << std::hex
                    << loc.GetInstruction() << std::dec << ", "
                    << loc.GetInstructionIndex() << ")";
  }
}

struct InstructionBase {
  InstructionBase() : pos(CurrentSourcePosition::Get()) {}
  virtual std::unique_ptr<InstructionBase> Clone() const = 0;
  virtual void Assign(const InstructionBase& other) = 0;
  virtual ~InstructionBase() = default;

  virtual void TypeInstruction(Stack<const Type*>* stack,
                               ControlFlowGraph* cfg) const = 0;
  virtual void RecomputeDefinitionLocations(
      Stack<DefinitionLocation>* locations,
      Worklist<Block*>* worklist) const = 0;
  void InvalidateTransientTypes(Stack<const Type*>* stack) const;
  virtual bool IsBlockTerminator() const { return false; }
  virtual void AppendSuccessorBlocks(std::vector<Block*>* block_list) const {}

  SourcePosition pos;
};

class Instruction {
 public:
  template <class T>
  Instruction(T instr)  // NOLINT(runtime/explicit)
      : kind_(T::kKind), instruction_(new T(std::move(instr))) {}

  template <class T>
  T& Cast() {
    DCHECK(Is<T>());
    return static_cast<T&>(*instruction_);
  }

  template <class T>
  const T& Cast() const {
    DCHECK(Is<T>());
    return static_cast<const T&>(*instruction_);
  }

  template <class T>
  bool Is() const {
    return kind_ == T::kKind;
  }

  template <class T>
  T* DynamicCast() {
    if (Is<T>()) return &Cast<T>();
    return nullptr;
  }

  template <class T>
  const T* DynamicCast() const {
    if (Is<T>()) return &Cast<T>();
    return nullptr;
  }

  Instruction(const Instruction& other) V8_NOEXCEPT
      : kind_(other.kind_),
        instruction_(other.instruction_->Clone()) {}
  Instruction& operator=(const Instruction& other) V8_NOEXCEPT {
    if (kind_ == other.kind_) {
      instruction_->Assign(*other.instruction_);
    } else {
      kind_ = other.kind_;
      instruction_ = other.instruction_->Clone();
    }
    return *this;
  }

  InstructionKind kind() const { return kind_; }
  const char* Mnemonic() const {
    switch (kind()) {
#define ENUM_ITEM(name)          \
  case InstructionKind::k##name: \
    return #name;
      TORQUE_INSTRUCTION_LIST(ENUM_ITEM)
#undef ENUM_ITEM
      default:
        UNREACHABLE();
    }
  }
  void TypeInstruction(Stack<const Type*>* stack, ControlFlowGraph* cfg) const {
    return instruction_->TypeInstruction(stack, cfg);
  }
  void RecomputeDefinitionLocations(Stack<DefinitionLocation>* locations,
                                    Worklist<Block*>* worklist) const {
    instruction_->RecomputeDefinitionLocations(locations, worklist);
  }

  InstructionBase* operator->() { return instruction_.get(); }
  const InstructionBase* operator->() const { return instruction_.get(); }

 private:
  InstructionKind kind_;
  std::unique_ptr<InstructionBase> instruction_;
};

struct PeekInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()

  PeekInstruction(BottomOffset slot, base::Optional<const Type*> widened_type)
      : slot(slot), widened_type(widened_type) {}

  BottomOffset slot;
  base::Optional<const Type*> widened_type;
};

struct PokeInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()

  PokeInstruction(BottomOffset slot, base::Optional<const Type*> widened_type)
      : slot(slot), widened_type(widened_type) {}

  BottomOffset slot;
  base::Optional<const Type*> widened_type;
};

// Preserve the top {preserved_slots} number of slots, and delete
// {deleted_slots} number or slots below.
struct DeleteRangeInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  explicit DeleteRangeInstruction(StackRange range) : range(range) {}

  StackRange range;
};

struct PushUninitializedInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  explicit PushUninitializedInstruction(const Type* type) : type(type) {}

  DefinitionLocation GetValueDefinition() const;

  const Type* type;
};

struct PushBuiltinPointerInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  PushBuiltinPointerInstruction(std::string external_name, const Type* type)
      : external_name(std::move(external_name)), type(type) {
    DCHECK(type->IsBuiltinPointerType());
  }

  DefinitionLocation GetValueDefinition() const;

  std::string external_name;
  const Type* type;
};

struct NamespaceConstantInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  explicit NamespaceConstantInstruction(NamespaceConstant* constant)
      : constant(constant) {}

  std::size_t GetValueDefinitionCount() const;
  DefinitionLocation GetValueDefinition(std::size_t index) const;

  NamespaceConstant* constant;
};

struct LoadReferenceInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  explicit LoadReferenceInstruction(const Type* type) : type(type) {}

  DefinitionLocation GetValueDefinition() const;

  const Type* type;
};

struct StoreReferenceInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  explicit StoreReferenceInstruction(const Type* type) : type(type) {}
  const Type* type;
};

// Pops a bitfield struct; pushes a bitfield value extracted from it.
struct LoadBitFieldInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  LoadBitFieldInstruction(const Type* bit_field_struct_type, BitField bit_field)
      : bit_field_struct_type(bit_field_struct_type),
        bit_field(std::move(bit_field)) {}

  DefinitionLocation GetValueDefinition() const;

  const Type* bit_field_struct_type;
  BitField bit_field;
};

// Pops a bitfield value and a bitfield struct; pushes a new bitfield struct
// containing the updated value.
struct StoreBitFieldInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  StoreBitFieldInstruction(const Type* bit_field_struct_type,
                           BitField bit_field, bool starts_as_zero)
      : bit_field_struct_type(bit_field_struct_type),
        bit_field(std::move(bit_field)),
        starts_as_zero(starts_as_zero) {}

  DefinitionLocation GetValueDefinition() const;

  const Type* bit_field_struct_type;
  BitField bit_field;
  // Allows skipping the mask step if we know the starting value is zero.
  bool starts_as_zero;
};

struct CallIntrinsicInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  CallIntrinsicInstruction(Intrinsic* intrinsic,
                           TypeVector specialization_types,
                           std::vector<std::string> constexpr_arguments)
      : intrinsic(intrinsic),
        specialization_types(std::move(specialization_types)),
        constexpr_arguments(constexpr_arguments) {}

  std::size_t GetValueDefinitionCount() const;
  DefinitionLocation GetValueDefinition(std::size_t index) const;

  Intrinsic* intrinsic;
  TypeVector specialization_types;
  std::vector<std::string> constexpr_arguments;
};

struct CallCsaMacroInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  CallCsaMacroInstruction(Macro* macro,
                          std::vector<std::string> constexpr_arguments,
                          base::Optional<Block*> catch_block)
      : macro(macro),
        constexpr_arguments(constexpr_arguments),
        catch_block(catch_block) {}
  void AppendSuccessorBlocks(std::vector<Block*>* block_list) const override {
    if (catch_block) block_list->push_back(*catch_block);
  }

  base::Optional<DefinitionLocation> GetExceptionObjectDefinition() const;
  std::size_t GetValueDefinitionCount() const;
  DefinitionLocation GetValueDefinition(std::size_t index) const;

  Macro* macro;
  std::vector<std::string> constexpr_arguments;
  base::Optional<Block*> catch_block;
};

struct CallCsaMacroAndBranchInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  CallCsaMacroAndBranchInstruction(Macro* macro,
                                   std::vector<std::string> constexpr_arguments,
                                   base::Optional<Block*> return_continuation,
                                   std::vector<Block*> label_blocks,
                                   base::Optional<Block*> catch_block)
      : macro(macro),
        constexpr_arguments(constexpr_arguments),
        return_continuation(return_continuation),
        label_blocks(label_blocks),
        catch_block(catch_block) {}
  bool IsBlockTerminator() const override { return true; }
  void AppendSuccessorBlocks(std::vector<Block*>* block_list) const override {
    if (catch_block) block_list->push_back(*catch_block);
    if (return_continuation) block_list->push_back(*return_continuation);
    for (Block* block : label_blocks) block_list->push_back(block);
  }

  std::size_t GetLabelCount() const;
  std::size_t GetLabelValueDefinitionCount(std::size_t label) const;
  DefinitionLocation GetLabelValueDefinition(std::size_t label,
                                             std::size_t index) const;
  std::size_t GetValueDefinitionCount() const;
  DefinitionLocation GetValueDefinition(std::size_t index) const;
  base::Optional<DefinitionLocation> GetExceptionObjectDefinition() const;

  Macro* macro;
  std::vector<std::string> constexpr_arguments;
  base::Optional<Block*> return_continuation;
  std::vector<Block*> label_blocks;
  base::Optional<Block*> catch_block;
};

struct CallBuiltinInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override { return is_tailcall; }
  CallBuiltinInstruction(bool is_tailcall, Builtin* builtin, size_t argc,
                         base::Optional<Block*> catch_block)
      : is_tailcall(is_tailcall),
        builtin(builtin),
        argc(argc),
        catch_block(catch_block) {}
  void AppendSuccessorBlocks(std::vector<Block*>* block_list) const override {
    if (catch_block) block_list->push_back(*catch_block);
  }

  std::size_t GetValueDefinitionCount() const;
  DefinitionLocation GetValueDefinition(std::size_t index) const;
  base::Optional<DefinitionLocation> GetExceptionObjectDefinition() const;

  bool is_tailcall;
  Builtin* builtin;
  size_t argc;
  base::Optional<Block*> catch_block;
};

struct CallBuiltinPointerInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override { return is_tailcall; }
  CallBuiltinPointerInstruction(bool is_tailcall,
                                const BuiltinPointerType* type, size_t argc)
      : is_tailcall(is_tailcall), type(type), argc(argc) {}

  std::size_t GetValueDefinitionCount() const;
  DefinitionLocation GetValueDefinition(std::size_t index) const;

  bool is_tailcall;
  const BuiltinPointerType* type;
  size_t argc;
};

struct CallRuntimeInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override;

  CallRuntimeInstruction(bool is_tailcall, RuntimeFunction* runtime_function,
                         size_t argc, base::Optional<Block*> catch_block)
      : is_tailcall(is_tailcall),
        runtime_function(runtime_function),
        argc(argc),
        catch_block(catch_block) {}
  void AppendSuccessorBlocks(std::vector<Block*>* block_list) const override {
    if (catch_block) block_list->push_back(*catch_block);
  }

  std::size_t GetValueDefinitionCount() const;
  DefinitionLocation GetValueDefinition(std::size_t index) const;
  base::Optional<DefinitionLocation> GetExceptionObjectDefinition() const;

  bool is_tailcall;
  RuntimeFunction* runtime_function;
  size_t argc;
  base::Optional<Block*> catch_block;
};

struct BranchInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override { return true; }
  void AppendSuccessorBlocks(std::vector<Block*>* block_list) const override {
    block_list->push_back(if_true);
    block_list->push_back(if_false);
  }

  BranchInstruction(Block* if_true, Block* if_false)
      : if_true(if_true), if_false(if_false) {}

  Block* if_true;
  Block* if_false;
};

struct ConstexprBranchInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override { return true; }
  void AppendSuccessorBlocks(std::vector<Block*>* block_list) const override {
    block_list->push_back(if_true);
    block_list->push_back(if_false);
  }

  ConstexprBranchInstruction(std::string condition, Block* if_true,
                             Block* if_false)
      : condition(condition), if_true(if_true), if_false(if_false) {}

  std::string condition;
  Block* if_true;
  Block* if_false;
};

struct GotoInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override { return true; }
  void AppendSuccessorBlocks(std::vector<Block*>* block_list) const override {
    block_list->push_back(destination);
  }

  explicit GotoInstruction(Block* destination) : destination(destination) {}

  Block* destination;
};

struct GotoExternalInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override { return true; }

  GotoExternalInstruction(std::string destination,
                          std::vector<std::string> variable_names)
      : destination(std::move(destination)),
        variable_names(std::move(variable_names)) {}

  std::string destination;
  std::vector<std::string> variable_names;
};

struct ReturnInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  explicit ReturnInstruction(size_t count) : count(count) {}
  bool IsBlockTerminator() const override { return true; }

  size_t count;  // How many values to return.
};

struct PrintConstantStringInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  explicit PrintConstantStringInstruction(std::string message)
      : message(std::move(message)) {}

  std::string message;
};

struct AbortInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  enum class Kind { kDebugBreak, kUnreachable, kAssertionFailure };
  bool IsBlockTerminator() const override { return kind != Kind::kDebugBreak; }
  explicit AbortInstruction(Kind kind, std::string message = "")
      : kind(kind), message(std::move(message)) {}

  Kind kind;
  std::string message;
};

struct UnsafeCastInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  explicit UnsafeCastInstruction(const Type* destination_type)
      : destination_type(destination_type) {}

  DefinitionLocation GetValueDefinition() const;

  const Type* destination_type;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_INSTRUCTIONS_H_
