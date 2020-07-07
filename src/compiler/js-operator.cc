// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-operator.h"

#include <limits>

#include "src/base/lazy-instance.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/operator.h"
#include "src/handles/handles-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/template-objects.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

// Returns properties for the given binary op.
constexpr Operator::Properties BinopProperties(Operator::Opcode opcode) {
  CONSTEXPR_DCHECK(JSOperator::IsBinaryWithFeedback(opcode));
  return opcode == IrOpcode::kJSStrictEqual ? Operator::kPure
                                            : Operator::kNoProperties;
}

}  // namespace

namespace js_node_wrapper_utils {

TNode<Oddball> UndefinedConstant(JSGraph* jsgraph) {
  return TNode<Oddball>::UncheckedCast(jsgraph->UndefinedConstant());
}

}  // namespace js_node_wrapper_utils

FeedbackCellRef JSCreateClosureNode::GetFeedbackCellRefChecked(
    JSHeapBroker* broker) const {
  HeapObjectMatcher m(feedback_cell());
  CHECK(m.HasValue());
  return FeedbackCellRef(broker, m.Value());
}

std::ostream& operator<<(std::ostream& os, CallFrequency const& f) {
  if (f.IsUnknown()) return os << "unknown";
  return os << f.value();
}

std::ostream& operator<<(std::ostream& os,
                         ConstructForwardVarargsParameters const& p) {
  return os << p.arity() << ", " << p.start_index();
}

ConstructForwardVarargsParameters const& ConstructForwardVarargsParametersOf(
    Operator const* op) {
  DCHECK_EQ(IrOpcode::kJSConstructForwardVarargs, op->opcode());
  return OpParameter<ConstructForwardVarargsParameters>(op);
}

bool operator==(ConstructParameters const& lhs,
                ConstructParameters const& rhs) {
  return lhs.arity() == rhs.arity() && lhs.frequency() == rhs.frequency() &&
         lhs.feedback() == rhs.feedback();
}

bool operator!=(ConstructParameters const& lhs,
                ConstructParameters const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(ConstructParameters const& p) {
  return base::hash_combine(p.arity(), p.frequency(),
                            FeedbackSource::Hash()(p.feedback()));
}

std::ostream& operator<<(std::ostream& os, ConstructParameters const& p) {
  return os << p.arity() << ", " << p.frequency();
}

ConstructParameters const& ConstructParametersOf(Operator const* op) {
  DCHECK(op->opcode() == IrOpcode::kJSConstruct ||
         op->opcode() == IrOpcode::kJSConstructWithArrayLike ||
         op->opcode() == IrOpcode::kJSConstructWithSpread);
  return OpParameter<ConstructParameters>(op);
}

std::ostream& operator<<(std::ostream& os, CallParameters const& p) {
  return os << p.arity() << ", " << p.frequency() << ", " << p.convert_mode()
            << ", " << p.speculation_mode() << ", " << p.feedback_relation();
}

const CallParameters& CallParametersOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kJSCall ||
         op->opcode() == IrOpcode::kJSCallWithArrayLike ||
         op->opcode() == IrOpcode::kJSCallWithSpread);
  return OpParameter<CallParameters>(op);
}

std::ostream& operator<<(std::ostream& os,
                         CallForwardVarargsParameters const& p) {
  return os << p.arity() << ", " << p.start_index();
}

CallForwardVarargsParameters const& CallForwardVarargsParametersOf(
    Operator const* op) {
  DCHECK_EQ(IrOpcode::kJSCallForwardVarargs, op->opcode());
  return OpParameter<CallForwardVarargsParameters>(op);
}


bool operator==(CallRuntimeParameters const& lhs,
                CallRuntimeParameters const& rhs) {
  return lhs.id() == rhs.id() && lhs.arity() == rhs.arity();
}


bool operator!=(CallRuntimeParameters const& lhs,
                CallRuntimeParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(CallRuntimeParameters const& p) {
  return base::hash_combine(p.id(), p.arity());
}


std::ostream& operator<<(std::ostream& os, CallRuntimeParameters const& p) {
  return os << p.id() << ", " << p.arity();
}


const CallRuntimeParameters& CallRuntimeParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCallRuntime, op->opcode());
  return OpParameter<CallRuntimeParameters>(op);
}


ContextAccess::ContextAccess(size_t depth, size_t index, bool immutable)
    : immutable_(immutable),
      depth_(static_cast<uint16_t>(depth)),
      index_(static_cast<uint32_t>(index)) {
  DCHECK(depth <= std::numeric_limits<uint16_t>::max());
  DCHECK(index <= std::numeric_limits<uint32_t>::max());
}


bool operator==(ContextAccess const& lhs, ContextAccess const& rhs) {
  return lhs.depth() == rhs.depth() && lhs.index() == rhs.index() &&
         lhs.immutable() == rhs.immutable();
}


bool operator!=(ContextAccess const& lhs, ContextAccess const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(ContextAccess const& access) {
  return base::hash_combine(access.depth(), access.index(), access.immutable());
}


std::ostream& operator<<(std::ostream& os, ContextAccess const& access) {
  return os << access.depth() << ", " << access.index() << ", "
            << access.immutable();
}


ContextAccess const& ContextAccessOf(Operator const* op) {
  DCHECK(op->opcode() == IrOpcode::kJSLoadContext ||
         op->opcode() == IrOpcode::kJSStoreContext);
  return OpParameter<ContextAccess>(op);
}

CreateFunctionContextParameters::CreateFunctionContextParameters(
    Handle<ScopeInfo> scope_info, int slot_count, ScopeType scope_type)
    : scope_info_(scope_info),
      slot_count_(slot_count),
      scope_type_(scope_type) {}

bool operator==(CreateFunctionContextParameters const& lhs,
                CreateFunctionContextParameters const& rhs) {
  return lhs.scope_info().location() == rhs.scope_info().location() &&
         lhs.slot_count() == rhs.slot_count() &&
         lhs.scope_type() == rhs.scope_type();
}

bool operator!=(CreateFunctionContextParameters const& lhs,
                CreateFunctionContextParameters const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(CreateFunctionContextParameters const& parameters) {
  return base::hash_combine(parameters.scope_info().location(),
                            parameters.slot_count(),
                            static_cast<int>(parameters.scope_type()));
}

std::ostream& operator<<(std::ostream& os,
                         CreateFunctionContextParameters const& parameters) {
  return os << parameters.slot_count() << ", " << parameters.scope_type();
}

CreateFunctionContextParameters const& CreateFunctionContextParametersOf(
    Operator const* op) {
  DCHECK_EQ(IrOpcode::kJSCreateFunctionContext, op->opcode());
  return OpParameter<CreateFunctionContextParameters>(op);
}

bool operator==(StoreNamedOwnParameters const& lhs,
                StoreNamedOwnParameters const& rhs) {
  return lhs.name().location() == rhs.name().location() &&
         lhs.feedback() == rhs.feedback();
}

bool operator!=(StoreNamedOwnParameters const& lhs,
                StoreNamedOwnParameters const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(StoreNamedOwnParameters const& p) {
  return base::hash_combine(p.name().location(),
                            FeedbackSource::Hash()(p.feedback()));
}

std::ostream& operator<<(std::ostream& os, StoreNamedOwnParameters const& p) {
  return os << Brief(*p.name());
}

StoreNamedOwnParameters const& StoreNamedOwnParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSStoreNamedOwn, op->opcode());
  return OpParameter<StoreNamedOwnParameters>(op);
}

bool operator==(FeedbackParameter const& lhs, FeedbackParameter const& rhs) {
  return lhs.feedback() == rhs.feedback();
}

bool operator!=(FeedbackParameter const& lhs, FeedbackParameter const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(FeedbackParameter const& p) {
  return FeedbackSource::Hash()(p.feedback());
}

std::ostream& operator<<(std::ostream& os, FeedbackParameter const& p) {
  return os << p.feedback();
}

FeedbackParameter const& FeedbackParameterOf(const Operator* op) {
  DCHECK(JSOperator::IsUnaryWithFeedback(op->opcode()) ||
         JSOperator::IsBinaryWithFeedback(op->opcode()) ||
         op->opcode() == IrOpcode::kJSCreateEmptyLiteralArray ||
         op->opcode() == IrOpcode::kJSInstanceOf ||
         op->opcode() == IrOpcode::kJSStoreDataPropertyInLiteral ||
         op->opcode() == IrOpcode::kJSStoreInArrayLiteral);
  return OpParameter<FeedbackParameter>(op);
}

bool operator==(NamedAccess const& lhs, NamedAccess const& rhs) {
  return lhs.name().location() == rhs.name().location() &&
         lhs.language_mode() == rhs.language_mode() &&
         lhs.feedback() == rhs.feedback();
}


bool operator!=(NamedAccess const& lhs, NamedAccess const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(NamedAccess const& p) {
  return base::hash_combine(p.name().location(), p.language_mode(),
                            FeedbackSource::Hash()(p.feedback()));
}


std::ostream& operator<<(std::ostream& os, NamedAccess const& p) {
  return os << Brief(*p.name()) << ", " << p.language_mode();
}


NamedAccess const& NamedAccessOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kJSLoadNamed ||
         op->opcode() == IrOpcode::kJSStoreNamed);
  return OpParameter<NamedAccess>(op);
}


std::ostream& operator<<(std::ostream& os, PropertyAccess const& p) {
  return os << p.language_mode() << ", " << p.feedback();
}


bool operator==(PropertyAccess const& lhs, PropertyAccess const& rhs) {
  return lhs.language_mode() == rhs.language_mode() &&
         lhs.feedback() == rhs.feedback();
}


bool operator!=(PropertyAccess const& lhs, PropertyAccess const& rhs) {
  return !(lhs == rhs);
}


PropertyAccess const& PropertyAccessOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kJSHasProperty ||
         op->opcode() == IrOpcode::kJSLoadProperty ||
         op->opcode() == IrOpcode::kJSStoreProperty);
  return OpParameter<PropertyAccess>(op);
}


size_t hash_value(PropertyAccess const& p) {
  return base::hash_combine(p.language_mode(),
                            FeedbackSource::Hash()(p.feedback()));
}


bool operator==(LoadGlobalParameters const& lhs,
                LoadGlobalParameters const& rhs) {
  return lhs.name().location() == rhs.name().location() &&
         lhs.feedback() == rhs.feedback() &&
         lhs.typeof_mode() == rhs.typeof_mode();
}


bool operator!=(LoadGlobalParameters const& lhs,
                LoadGlobalParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(LoadGlobalParameters const& p) {
  return base::hash_combine(p.name().location(), p.typeof_mode());
}


std::ostream& operator<<(std::ostream& os, LoadGlobalParameters const& p) {
  return os << Brief(*p.name()) << ", " << p.typeof_mode();
}


const LoadGlobalParameters& LoadGlobalParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSLoadGlobal, op->opcode());
  return OpParameter<LoadGlobalParameters>(op);
}


bool operator==(StoreGlobalParameters const& lhs,
                StoreGlobalParameters const& rhs) {
  return lhs.language_mode() == rhs.language_mode() &&
         lhs.name().location() == rhs.name().location() &&
         lhs.feedback() == rhs.feedback();
}


bool operator!=(StoreGlobalParameters const& lhs,
                StoreGlobalParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(StoreGlobalParameters const& p) {
  return base::hash_combine(p.language_mode(), p.name().location(),
                            FeedbackSource::Hash()(p.feedback()));
}


std::ostream& operator<<(std::ostream& os, StoreGlobalParameters const& p) {
  return os << p.language_mode() << ", " << Brief(*p.name());
}


const StoreGlobalParameters& StoreGlobalParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSStoreGlobal, op->opcode());
  return OpParameter<StoreGlobalParameters>(op);
}


CreateArgumentsType const& CreateArgumentsTypeOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCreateArguments, op->opcode());
  return OpParameter<CreateArgumentsType>(op);
}


bool operator==(CreateArrayParameters const& lhs,
                CreateArrayParameters const& rhs) {
  return lhs.arity() == rhs.arity() &&
         lhs.site().address() == rhs.site().address();
}


bool operator!=(CreateArrayParameters const& lhs,
                CreateArrayParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(CreateArrayParameters const& p) {
  return base::hash_combine(p.arity(), p.site().address());
}


std::ostream& operator<<(std::ostream& os, CreateArrayParameters const& p) {
  os << p.arity();
  Handle<AllocationSite> site;
  if (p.site().ToHandle(&site)) os << ", " << Brief(*site);
  return os;
}

const CreateArrayParameters& CreateArrayParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCreateArray, op->opcode());
  return OpParameter<CreateArrayParameters>(op);
}

bool operator==(CreateArrayIteratorParameters const& lhs,
                CreateArrayIteratorParameters const& rhs) {
  return lhs.kind() == rhs.kind();
}

bool operator!=(CreateArrayIteratorParameters const& lhs,
                CreateArrayIteratorParameters const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(CreateArrayIteratorParameters const& p) {
  return static_cast<size_t>(p.kind());
}

std::ostream& operator<<(std::ostream& os,
                         CreateArrayIteratorParameters const& p) {
  return os << p.kind();
}

const CreateArrayIteratorParameters& CreateArrayIteratorParametersOf(
    const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCreateArrayIterator, op->opcode());
  return OpParameter<CreateArrayIteratorParameters>(op);
}

bool operator==(CreateCollectionIteratorParameters const& lhs,
                CreateCollectionIteratorParameters const& rhs) {
  return lhs.collection_kind() == rhs.collection_kind() &&
         lhs.iteration_kind() == rhs.iteration_kind();
}

bool operator!=(CreateCollectionIteratorParameters const& lhs,
                CreateCollectionIteratorParameters const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(CreateCollectionIteratorParameters const& p) {
  return base::hash_combine(static_cast<size_t>(p.collection_kind()),
                            static_cast<size_t>(p.iteration_kind()));
}

std::ostream& operator<<(std::ostream& os,
                         CreateCollectionIteratorParameters const& p) {
  return os << p.collection_kind() << ", " << p.iteration_kind();
}

const CreateCollectionIteratorParameters& CreateCollectionIteratorParametersOf(
    const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCreateCollectionIterator, op->opcode());
  return OpParameter<CreateCollectionIteratorParameters>(op);
}

bool operator==(CreateBoundFunctionParameters const& lhs,
                CreateBoundFunctionParameters const& rhs) {
  return lhs.arity() == rhs.arity() &&
         lhs.map().location() == rhs.map().location();
}

bool operator!=(CreateBoundFunctionParameters const& lhs,
                CreateBoundFunctionParameters const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(CreateBoundFunctionParameters const& p) {
  return base::hash_combine(p.arity(), p.map().location());
}

std::ostream& operator<<(std::ostream& os,
                         CreateBoundFunctionParameters const& p) {
  os << p.arity();
  if (!p.map().is_null()) os << ", " << Brief(*p.map());
  return os;
}

const CreateBoundFunctionParameters& CreateBoundFunctionParametersOf(
    const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCreateBoundFunction, op->opcode());
  return OpParameter<CreateBoundFunctionParameters>(op);
}

bool operator==(GetTemplateObjectParameters const& lhs,
                GetTemplateObjectParameters const& rhs) {
  return lhs.description().location() == rhs.description().location() &&
         lhs.shared().location() == rhs.shared().location() &&
         lhs.feedback() == rhs.feedback();
}

bool operator!=(GetTemplateObjectParameters const& lhs,
                GetTemplateObjectParameters const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(GetTemplateObjectParameters const& p) {
  return base::hash_combine(p.description().location(), p.shared().location(),
                            FeedbackSource::Hash()(p.feedback()));
}

std::ostream& operator<<(std::ostream& os,
                         GetTemplateObjectParameters const& p) {
  return os << Brief(*p.description()) << ", " << Brief(*p.shared());
}

const GetTemplateObjectParameters& GetTemplateObjectParametersOf(
    const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kJSGetTemplateObject);
  return OpParameter<GetTemplateObjectParameters>(op);
}

bool operator==(CreateClosureParameters const& lhs,
                CreateClosureParameters const& rhs) {
  return lhs.allocation() == rhs.allocation() &&
         lhs.code().location() == rhs.code().location() &&
         lhs.shared_info().location() == rhs.shared_info().location();
}


bool operator!=(CreateClosureParameters const& lhs,
                CreateClosureParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(CreateClosureParameters const& p) {
  return base::hash_combine(p.allocation(), p.shared_info().location());
}


std::ostream& operator<<(std::ostream& os, CreateClosureParameters const& p) {
  return os << p.allocation() << ", " << Brief(*p.shared_info()) << ", "
            << Brief(*p.code());
}


const CreateClosureParameters& CreateClosureParametersOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSCreateClosure, op->opcode());
  return OpParameter<CreateClosureParameters>(op);
}


bool operator==(CreateLiteralParameters const& lhs,
                CreateLiteralParameters const& rhs) {
  return lhs.constant().location() == rhs.constant().location() &&
         lhs.feedback() == rhs.feedback() && lhs.length() == rhs.length() &&
         lhs.flags() == rhs.flags();
}


bool operator!=(CreateLiteralParameters const& lhs,
                CreateLiteralParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(CreateLiteralParameters const& p) {
  return base::hash_combine(p.constant().location(),
                            FeedbackSource::Hash()(p.feedback()), p.length(),
                            p.flags());
}


std::ostream& operator<<(std::ostream& os, CreateLiteralParameters const& p) {
  return os << Brief(*p.constant()) << ", " << p.length() << ", " << p.flags();
}


const CreateLiteralParameters& CreateLiteralParametersOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kJSCreateLiteralArray ||
         op->opcode() == IrOpcode::kJSCreateLiteralObject ||
         op->opcode() == IrOpcode::kJSCreateLiteralRegExp);
  return OpParameter<CreateLiteralParameters>(op);
}

bool operator==(CloneObjectParameters const& lhs,
                CloneObjectParameters const& rhs) {
  return lhs.feedback() == rhs.feedback() && lhs.flags() == rhs.flags();
}

bool operator!=(CloneObjectParameters const& lhs,
                CloneObjectParameters const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(CloneObjectParameters const& p) {
  return base::hash_combine(FeedbackSource::Hash()(p.feedback()), p.flags());
}

std::ostream& operator<<(std::ostream& os, CloneObjectParameters const& p) {
  return os << p.flags();
}

const CloneObjectParameters& CloneObjectParametersOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kJSCloneObject);
  return OpParameter<CloneObjectParameters>(op);
}

std::ostream& operator<<(std::ostream& os, GetIteratorParameters const& p) {
  return os << p.loadFeedback() << ", " << p.callFeedback();
}

bool operator==(GetIteratorParameters const& lhs,
                GetIteratorParameters const& rhs) {
  return lhs.loadFeedback() == rhs.loadFeedback() &&
         lhs.callFeedback() == rhs.callFeedback();
}

bool operator!=(GetIteratorParameters const& lhs,
                GetIteratorParameters const& rhs) {
  return !(lhs == rhs);
}

GetIteratorParameters const& GetIteratorParametersOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kJSGetIterator);
  return OpParameter<GetIteratorParameters>(op);
}

size_t hash_value(GetIteratorParameters const& p) {
  return base::hash_combine(FeedbackSource::Hash()(p.loadFeedback()),
                            FeedbackSource::Hash()(p.callFeedback()));
}

size_t hash_value(ForInMode mode) { return static_cast<uint8_t>(mode); }

std::ostream& operator<<(std::ostream& os, ForInMode mode) {
  switch (mode) {
    case ForInMode::kUseEnumCacheKeysAndIndices:
      return os << "UseEnumCacheKeysAndIndices";
    case ForInMode::kUseEnumCacheKeys:
      return os << "UseEnumCacheKeys";
    case ForInMode::kGeneric:
      return os << "Generic";
  }
  UNREACHABLE();
}

ForInMode ForInModeOf(Operator const* op) {
  DCHECK(op->opcode() == IrOpcode::kJSForInNext ||
         op->opcode() == IrOpcode::kJSForInPrepare);
  return OpParameter<ForInMode>(op);
}

#define CACHED_OP_LIST(V)                                                \
  V(ToLength, Operator::kNoProperties, 1, 1)                             \
  V(ToName, Operator::kNoProperties, 1, 1)                               \
  V(ToNumber, Operator::kNoProperties, 1, 1)                             \
  V(ToNumberConvertBigInt, Operator::kNoProperties, 1, 1)                \
  V(ToNumeric, Operator::kNoProperties, 1, 1)                            \
  V(ToObject, Operator::kFoldable, 1, 1)                                 \
  V(ToString, Operator::kNoProperties, 1, 1)                             \
  V(Create, Operator::kNoProperties, 2, 1)                               \
  V(CreateIterResultObject, Operator::kEliminatable, 2, 1)               \
  V(CreateStringIterator, Operator::kEliminatable, 1, 1)                 \
  V(CreateKeyValueArray, Operator::kEliminatable, 2, 1)                  \
  V(CreatePromise, Operator::kEliminatable, 0, 1)                        \
  V(CreateTypedArray, Operator::kNoProperties, 5, 1)                     \
  V(CreateObject, Operator::kNoProperties, 1, 1)                         \
  V(ObjectIsArray, Operator::kNoProperties, 1, 1)                        \
  V(HasInPrototypeChain, Operator::kNoProperties, 2, 1)                  \
  V(OrdinaryHasInstance, Operator::kNoProperties, 2, 1)                  \
  V(ForInEnumerate, Operator::kNoProperties, 1, 1)                       \
  V(AsyncFunctionEnter, Operator::kNoProperties, 2, 1)                   \
  V(AsyncFunctionReject, Operator::kNoDeopt | Operator::kNoThrow, 3, 1)  \
  V(AsyncFunctionResolve, Operator::kNoDeopt | Operator::kNoThrow, 3, 1) \
  V(LoadMessage, Operator::kNoThrow | Operator::kNoWrite, 0, 1)          \
  V(StoreMessage, Operator::kNoRead | Operator::kNoThrow, 1, 0)          \
  V(GeneratorRestoreContinuation, Operator::kNoThrow, 1, 1)              \
  V(GeneratorRestoreContext, Operator::kNoThrow, 1, 1)                   \
  V(GeneratorRestoreInputOrDebugPos, Operator::kNoThrow, 1, 1)           \
  V(Debugger, Operator::kNoProperties, 0, 0)                             \
  V(FulfillPromise, Operator::kNoDeopt | Operator::kNoThrow, 2, 1)       \
  V(PerformPromiseThen, Operator::kNoDeopt | Operator::kNoThrow, 4, 1)   \
  V(PromiseResolve, Operator::kNoProperties, 2, 1)                       \
  V(RejectPromise, Operator::kNoDeopt | Operator::kNoThrow, 3, 1)        \
  V(ResolvePromise, Operator::kNoDeopt | Operator::kNoThrow, 2, 1)       \
  V(GetSuperConstructor, Operator::kNoWrite, 1, 1)                       \
  V(ParseInt, Operator::kNoProperties, 2, 1)                             \
  V(RegExpTest, Operator::kNoProperties, 2, 1)

struct JSOperatorGlobalCache final {
#define CACHED_OP(Name, properties, value_input_count, value_output_count) \
  struct Name##Operator final : public Operator {                          \
    Name##Operator()                                                       \
        : Operator(IrOpcode::kJS##Name, properties, "JS" #Name,            \
                   value_input_count, Operator::ZeroIfPure(properties),    \
                   Operator::ZeroIfEliminatable(properties),               \
                   value_output_count, Operator::ZeroIfPure(properties),   \
                   Operator::ZeroIfNoThrow(properties)) {}                 \
  };                                                                       \
  Name##Operator k##Name##Operator;
  CACHED_OP_LIST(CACHED_OP)
#undef CACHED_OP
};

namespace {
DEFINE_LAZY_LEAKY_OBJECT_GETTER(JSOperatorGlobalCache, GetJSOperatorGlobalCache)
}

JSOperatorBuilder::JSOperatorBuilder(Zone* zone)
    : cache_(*GetJSOperatorGlobalCache()), zone_(zone) {}

#define CACHED_OP(Name, properties, value_input_count, value_output_count) \
  const Operator* JSOperatorBuilder::Name() {                              \
    return &cache_.k##Name##Operator;                                      \
  }
CACHED_OP_LIST(CACHED_OP)
#undef CACHED_OP

#define UNARY_OP(JSName, Name)                                                \
  const Operator* JSOperatorBuilder::Name(FeedbackSource const& feedback) {   \
    FeedbackParameter parameters(feedback);                                   \
    return new (zone()) Operator1<FeedbackParameter>(                         \
        IrOpcode::k##JSName, Operator::kNoProperties, #JSName, 2, 1, 1, 1, 1, \
        2, parameters);                                                       \
  }
JS_UNOP_WITH_FEEDBACK(UNARY_OP)
#undef UNARY_OP

#define BINARY_OP(JSName, Name)                                               \
  const Operator* JSOperatorBuilder::Name(FeedbackSource const& feedback) {   \
    static constexpr auto kProperties = BinopProperties(IrOpcode::k##JSName); \
    FeedbackParameter parameters(feedback);                                   \
    return new (zone()) Operator1<FeedbackParameter>(                         \
        IrOpcode::k##JSName, kProperties, #JSName, 3, 1, 1, 1, 1,             \
        Operator::ZeroIfNoThrow(kProperties), parameters);                    \
  }
JS_BINOP_WITH_FEEDBACK(BINARY_OP)
#undef BINARY_OP

const Operator* JSOperatorBuilder::StoreDataPropertyInLiteral(
    const FeedbackSource& feedback) {
  static constexpr int kObject = 1;
  static constexpr int kName = 1;
  static constexpr int kValue = 1;
  static constexpr int kFlags = 1;
  static constexpr int kFeedbackVector = 1;
  static constexpr int kArity =
      kObject + kName + kValue + kFlags + kFeedbackVector;
  FeedbackParameter parameters(feedback);
  return new (zone()) Operator1<FeedbackParameter>(  // --
      IrOpcode::kJSStoreDataPropertyInLiteral,
      Operator::kNoThrow,              // opcode
      "JSStoreDataPropertyInLiteral",  // name
      kArity, 1, 1, 0, 1, 1,           // counts
      parameters);                     // parameter
}

const Operator* JSOperatorBuilder::StoreInArrayLiteral(
    const FeedbackSource& feedback) {
  static constexpr int kArray = 1;
  static constexpr int kIndex = 1;
  static constexpr int kValue = 1;
  static constexpr int kFeedbackVector = 1;
  static constexpr int kArity = kArray + kIndex + kValue + kFeedbackVector;
  FeedbackParameter parameters(feedback);
  return new (zone()) Operator1<FeedbackParameter>(  // --
      IrOpcode::kJSStoreInArrayLiteral,
      Operator::kNoThrow,       // opcode
      "JSStoreInArrayLiteral",  // name
      kArity, 1, 1, 0, 1, 1,    // counts
      parameters);              // parameter
}

const Operator* JSOperatorBuilder::CallForwardVarargs(size_t arity,
                                                      uint32_t start_index) {
  CallForwardVarargsParameters parameters(arity, start_index);
  return new (zone()) Operator1<CallForwardVarargsParameters>(   // --
      IrOpcode::kJSCallForwardVarargs, Operator::kNoProperties,  // opcode
      "JSCallForwardVarargs",                                    // name
      parameters.arity(), 1, 1, 1, 1, 2,                         // counts
      parameters);                                               // parameter
}

const Operator* JSOperatorBuilder::Call(
    size_t arity, CallFrequency const& frequency,
    FeedbackSource const& feedback, ConvertReceiverMode convert_mode,
    SpeculationMode speculation_mode, CallFeedbackRelation feedback_relation) {
  CallParameters parameters(arity, frequency, feedback, convert_mode,
                            speculation_mode, feedback_relation);
  return new (zone()) Operator1<CallParameters>(   // --
      IrOpcode::kJSCall, Operator::kNoProperties,  // opcode
      "JSCall",                                    // name
      parameters.arity(), 1, 1, 1, 1, 2,           // inputs/outputs
      parameters);                                 // parameter
}

const Operator* JSOperatorBuilder::CallWithArrayLike(
    const CallFrequency& frequency, const FeedbackSource& feedback,
    SpeculationMode speculation_mode, CallFeedbackRelation feedback_relation) {
  static constexpr int kTheArrayLikeObject = 1;
  CallParameters parameters(
      JSCallWithArrayLikeNode::ArityForArgc(kTheArrayLikeObject), frequency,
      feedback, ConvertReceiverMode::kAny, speculation_mode, feedback_relation);
  return new (zone()) Operator1<CallParameters>(                // --
      IrOpcode::kJSCallWithArrayLike, Operator::kNoProperties,  // opcode
      "JSCallWithArrayLike",                                    // name
      parameters.arity(), 1, 1, 1, 1, 2,                        // counts
      parameters);                                              // parameter
}

const Operator* JSOperatorBuilder::CallWithSpread(
    uint32_t arity, CallFrequency const& frequency,
    FeedbackSource const& feedback, SpeculationMode speculation_mode,
    CallFeedbackRelation feedback_relation) {
  DCHECK_IMPLIES(speculation_mode == SpeculationMode::kAllowSpeculation,
                 feedback.IsValid());
  CallParameters parameters(arity, frequency, feedback,
                            ConvertReceiverMode::kAny, speculation_mode,
                            feedback_relation);
  return new (zone()) Operator1<CallParameters>(             // --
      IrOpcode::kJSCallWithSpread, Operator::kNoProperties,  // opcode
      "JSCallWithSpread",                                    // name
      parameters.arity(), 1, 1, 1, 1, 2,                     // counts
      parameters);                                           // parameter
}

const Operator* JSOperatorBuilder::CallRuntime(Runtime::FunctionId id) {
  const Runtime::Function* f = Runtime::FunctionForId(id);
  return CallRuntime(f, f->nargs);
}


const Operator* JSOperatorBuilder::CallRuntime(Runtime::FunctionId id,
                                               size_t arity) {
  const Runtime::Function* f = Runtime::FunctionForId(id);
  return CallRuntime(f, arity);
}


const Operator* JSOperatorBuilder::CallRuntime(const Runtime::Function* f,
                                               size_t arity) {
  CallRuntimeParameters parameters(f->function_id, arity);
  DCHECK(f->nargs == -1 || f->nargs == static_cast<int>(parameters.arity()));
  return new (zone()) Operator1<CallRuntimeParameters>(   // --
      IrOpcode::kJSCallRuntime, Operator::kNoProperties,  // opcode
      "JSCallRuntime",                                    // name
      parameters.arity(), 1, 1, f->result_size, 1, 2,     // inputs/outputs
      parameters);                                        // parameter
}

const Operator* JSOperatorBuilder::ConstructForwardVarargs(
    size_t arity, uint32_t start_index) {
  ConstructForwardVarargsParameters parameters(arity, start_index);
  return new (zone()) Operator1<ConstructForwardVarargsParameters>(   // --
      IrOpcode::kJSConstructForwardVarargs, Operator::kNoProperties,  // opcode
      "JSConstructForwardVarargs",                                    // name
      parameters.arity(), 1, 1, 1, 1, 2,                              // counts
      parameters);  // parameter
}

// Note: frequency is taken by reference to work around a GCC bug
// on AIX (v8:8193).
const Operator* JSOperatorBuilder::Construct(uint32_t arity,
                                             CallFrequency const& frequency,
                                             FeedbackSource const& feedback) {
  ConstructParameters parameters(arity, frequency, feedback);
  return new (zone()) Operator1<ConstructParameters>(   // --
      IrOpcode::kJSConstruct, Operator::kNoProperties,  // opcode
      "JSConstruct",                                    // name
      parameters.arity(), 1, 1, 1, 1, 2,                // counts
      parameters);                                      // parameter
}

const Operator* JSOperatorBuilder::ConstructWithArrayLike(
    CallFrequency const& frequency, FeedbackSource const& feedback) {
  static constexpr int kTheArrayLikeObject = 1;
  ConstructParameters parameters(
      JSConstructWithArrayLikeNode::ArityForArgc(kTheArrayLikeObject),
      frequency, feedback);
  return new (zone()) Operator1<ConstructParameters>(  // --
      IrOpcode::kJSConstructWithArrayLike,             // opcode
      Operator::kNoProperties,                         // properties
      "JSConstructWithArrayLike",                      // name
      parameters.arity(), 1, 1, 1, 1, 2,               // counts
      parameters);                                     // parameter
}

const Operator* JSOperatorBuilder::ConstructWithSpread(
    uint32_t arity, CallFrequency const& frequency,
    FeedbackSource const& feedback) {
  ConstructParameters parameters(arity, frequency, feedback);
  return new (zone()) Operator1<ConstructParameters>(             // --
      IrOpcode::kJSConstructWithSpread, Operator::kNoProperties,  // opcode
      "JSConstructWithSpread",                                    // name
      parameters.arity(), 1, 1, 1, 1, 2,                          // counts
      parameters);                                                // parameter
}

const Operator* JSOperatorBuilder::LoadNamed(Handle<Name> name,
                                             const FeedbackSource& feedback) {
  static constexpr int kObject = 1;
  static constexpr int kFeedbackVector = 1;
  static constexpr int kArity = kObject + kFeedbackVector;
  NamedAccess access(LanguageMode::kSloppy, name, feedback);
  return new (zone()) Operator1<NamedAccess>(           // --
      IrOpcode::kJSLoadNamed, Operator::kNoProperties,  // opcode
      "JSLoadNamed",                                    // name
      kArity, 1, 1, 1, 1, 2,                            // counts
      access);                                          // parameter
}

const Operator* JSOperatorBuilder::LoadProperty(
    FeedbackSource const& feedback) {
  PropertyAccess access(LanguageMode::kSloppy, feedback);
  return new (zone()) Operator1<PropertyAccess>(           // --
      IrOpcode::kJSLoadProperty, Operator::kNoProperties,  // opcode
      "JSLoadProperty",                                    // name
      3, 1, 1, 1, 1, 2,                                    // counts
      access);                                             // parameter
}

const Operator* JSOperatorBuilder::GetIterator(
    FeedbackSource const& load_feedback, FeedbackSource const& call_feedback) {
  GetIteratorParameters access(load_feedback, call_feedback);
  return new (zone()) Operator1<GetIteratorParameters>(   // --
      IrOpcode::kJSGetIterator, Operator::kNoProperties,  // opcode
      "JSGetIterator",                                    // name
      2, 1, 1, 1, 1, 2,                                   // counts
      access);                                            // parameter
}

const Operator* JSOperatorBuilder::HasProperty(FeedbackSource const& feedback) {
  PropertyAccess access(LanguageMode::kSloppy, feedback);
  return new (zone()) Operator1<PropertyAccess>(          // --
      IrOpcode::kJSHasProperty, Operator::kNoProperties,  // opcode
      "JSHasProperty",                                    // name
      3, 1, 1, 1, 1, 2,                                   // counts
      access);                                            // parameter
}

const Operator* JSOperatorBuilder::ForInNext(ForInMode mode) {
  return new (zone()) Operator1<ForInMode>(             // --
      IrOpcode::kJSForInNext, Operator::kNoProperties,  // opcode
      "JSForInNext",                                    // name
      4, 1, 1, 1, 1, 2,                                 // counts
      mode);                                            // parameter
}

const Operator* JSOperatorBuilder::ForInPrepare(ForInMode mode) {
  return new (zone()) Operator1<ForInMode>(     // --
      IrOpcode::kJSForInPrepare,                // opcode
      Operator::kNoWrite | Operator::kNoThrow,  // flags
      "JSForInPrepare",                         // name
      1, 1, 1, 3, 1, 1,                         // counts
      mode);                                    // parameter
}

const Operator* JSOperatorBuilder::GeneratorStore(int register_count) {
  return new (zone()) Operator1<int>(                   // --
      IrOpcode::kJSGeneratorStore, Operator::kNoThrow,  // opcode
      "JSGeneratorStore",                               // name
      3 + register_count, 1, 1, 0, 1, 0,                // counts
      register_count);                                  // parameter
}

int RegisterCountOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kJSCreateAsyncFunctionObject, op->opcode());
  return OpParameter<int>(op);
}

int GeneratorStoreValueCountOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSGeneratorStore, op->opcode());
  return OpParameter<int>(op);
}

const Operator* JSOperatorBuilder::GeneratorRestoreRegister(int index) {
  return new (zone()) Operator1<int>(                             // --
      IrOpcode::kJSGeneratorRestoreRegister, Operator::kNoThrow,  // opcode
      "JSGeneratorRestoreRegister",                               // name
      1, 1, 1, 1, 1, 0,                                           // counts
      index);                                                     // parameter
}

int RestoreRegisterIndexOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kJSGeneratorRestoreRegister, op->opcode());
  return OpParameter<int>(op);
}

const Operator* JSOperatorBuilder::StoreNamed(LanguageMode language_mode,
                                              Handle<Name> name,
                                              FeedbackSource const& feedback) {
  static constexpr int kObject = 1;
  static constexpr int kValue = 1;
  static constexpr int kFeedbackVector = 1;
  static constexpr int kArity = kObject + kValue + kFeedbackVector;
  NamedAccess access(language_mode, name, feedback);
  return new (zone()) Operator1<NamedAccess>(            // --
      IrOpcode::kJSStoreNamed, Operator::kNoProperties,  // opcode
      "JSStoreNamed",                                    // name
      kArity, 1, 1, 0, 1, 2,                             // counts
      access);                                           // parameter
}

const Operator* JSOperatorBuilder::StoreProperty(
    LanguageMode language_mode, FeedbackSource const& feedback) {
  PropertyAccess access(language_mode, feedback);
  return new (zone()) Operator1<PropertyAccess>(            // --
      IrOpcode::kJSStoreProperty, Operator::kNoProperties,  // opcode
      "JSStoreProperty",                                    // name
      4, 1, 1, 0, 1, 2,                                     // counts
      access);                                              // parameter
}

const Operator* JSOperatorBuilder::StoreNamedOwn(
    Handle<Name> name, FeedbackSource const& feedback) {
  static constexpr int kObject = 1;
  static constexpr int kValue = 1;
  static constexpr int kFeedbackVector = 1;
  static constexpr int kArity = kObject + kValue + kFeedbackVector;
  StoreNamedOwnParameters parameters(name, feedback);
  return new (zone()) Operator1<StoreNamedOwnParameters>(   // --
      IrOpcode::kJSStoreNamedOwn, Operator::kNoProperties,  // opcode
      "JSStoreNamedOwn",                                    // name
      kArity, 1, 1, 0, 1, 2,                                // counts
      parameters);                                          // parameter
}

const Operator* JSOperatorBuilder::DeleteProperty() {
  return new (zone()) Operator(                              // --
      IrOpcode::kJSDeleteProperty, Operator::kNoProperties,  // opcode
      "JSDeleteProperty",                                    // name
      3, 1, 1, 1, 1, 2);                                     // counts
}

const Operator* JSOperatorBuilder::CreateGeneratorObject() {
  return new (zone()) Operator(                                     // --
      IrOpcode::kJSCreateGeneratorObject, Operator::kEliminatable,  // opcode
      "JSCreateGeneratorObject",                                    // name
      2, 1, 1, 1, 1, 0);                                            // counts
}

const Operator* JSOperatorBuilder::LoadGlobal(const Handle<Name>& name,
                                              const FeedbackSource& feedback,
                                              TypeofMode typeof_mode) {
  static constexpr int kFeedbackVector = 1;
  static constexpr int kArity = kFeedbackVector;
  LoadGlobalParameters parameters(name, feedback, typeof_mode);
  return new (zone()) Operator1<LoadGlobalParameters>(   // --
      IrOpcode::kJSLoadGlobal, Operator::kNoProperties,  // opcode
      "JSLoadGlobal",                                    // name
      kArity, 1, 1, 1, 1, 2,                             // counts
      parameters);                                       // parameter
}

const Operator* JSOperatorBuilder::StoreGlobal(LanguageMode language_mode,
                                               const Handle<Name>& name,
                                               const FeedbackSource& feedback) {
  static constexpr int kValue = 1;
  static constexpr int kFeedbackVector = 1;
  static constexpr int kArity = kValue + kFeedbackVector;
  StoreGlobalParameters parameters(language_mode, feedback, name);
  return new (zone()) Operator1<StoreGlobalParameters>(   // --
      IrOpcode::kJSStoreGlobal, Operator::kNoProperties,  // opcode
      "JSStoreGlobal",                                    // name
      kArity, 1, 1, 0, 1, 2,                              // counts
      parameters);                                        // parameter
}

const Operator* JSOperatorBuilder::HasContextExtension(size_t depth) {
  return new (zone()) Operator1<size_t>(        // --
      IrOpcode::kJSHasContextExtension,         // opcode
      Operator::kNoWrite | Operator::kNoThrow,  // flags
      "JSHasContextExtension",                  // name
      0, 1, 0, 1, 1, 0,                         // counts
      depth);                                   // parameter
}

const Operator* JSOperatorBuilder::LoadContext(size_t depth, size_t index,
                                               bool immutable) {
  ContextAccess access(depth, index, immutable);
  return new (zone()) Operator1<ContextAccess>(  // --
      IrOpcode::kJSLoadContext,                  // opcode
      Operator::kNoWrite | Operator::kNoThrow,   // flags
      "JSLoadContext",                           // name
      0, 1, 0, 1, 1, 0,                          // counts
      access);                                   // parameter
}


const Operator* JSOperatorBuilder::StoreContext(size_t depth, size_t index) {
  ContextAccess access(depth, index, false);
  return new (zone()) Operator1<ContextAccess>(  // --
      IrOpcode::kJSStoreContext,                 // opcode
      Operator::kNoRead | Operator::kNoThrow,    // flags
      "JSStoreContext",                          // name
      1, 1, 1, 0, 1, 0,                          // counts
      access);                                   // parameter
}

const Operator* JSOperatorBuilder::LoadModule(int32_t cell_index) {
  return new (zone()) Operator1<int32_t>(       // --
      IrOpcode::kJSLoadModule,                  // opcode
      Operator::kNoWrite | Operator::kNoThrow,  // flags
      "JSLoadModule",                           // name
      1, 1, 1, 1, 1, 0,                         // counts
      cell_index);                              // parameter
}

const Operator* JSOperatorBuilder::StoreModule(int32_t cell_index) {
  return new (zone()) Operator1<int32_t>(      // --
      IrOpcode::kJSStoreModule,                // opcode
      Operator::kNoRead | Operator::kNoThrow,  // flags
      "JSStoreModule",                         // name
      2, 1, 1, 0, 1, 0,                        // counts
      cell_index);                             // parameter
}

const Operator* JSOperatorBuilder::CreateArguments(CreateArgumentsType type) {
  return new (zone()) Operator1<CreateArgumentsType>(         // --
      IrOpcode::kJSCreateArguments, Operator::kEliminatable,  // opcode
      "JSCreateArguments",                                    // name
      1, 1, 0, 1, 1, 0,                                       // counts
      type);                                                  // parameter
}

const Operator* JSOperatorBuilder::CreateArray(
    size_t arity, MaybeHandle<AllocationSite> site) {
  // constructor, new_target, arg1, ..., argN
  int const value_input_count = static_cast<int>(arity) + 2;
  CreateArrayParameters parameters(arity, site);
  return new (zone()) Operator1<CreateArrayParameters>(   // --
      IrOpcode::kJSCreateArray, Operator::kNoProperties,  // opcode
      "JSCreateArray",                                    // name
      value_input_count, 1, 1, 1, 1, 2,                   // counts
      parameters);                                        // parameter
}

const Operator* JSOperatorBuilder::CreateArrayIterator(IterationKind kind) {
  CreateArrayIteratorParameters parameters(kind);
  return new (zone()) Operator1<CreateArrayIteratorParameters>(   // --
      IrOpcode::kJSCreateArrayIterator, Operator::kEliminatable,  // opcode
      "JSCreateArrayIterator",                                    // name
      1, 1, 1, 1, 1, 0,                                           // counts
      parameters);                                                // parameter
}

const Operator* JSOperatorBuilder::CreateAsyncFunctionObject(
    int register_count) {
  return new (zone()) Operator1<int>(          // --
      IrOpcode::kJSCreateAsyncFunctionObject,  // opcode
      Operator::kEliminatable,                 // flags
      "JSCreateAsyncFunctionObject",           // name
      3, 1, 1, 1, 1, 0,                        // counts
      register_count);                         // parameter
}

const Operator* JSOperatorBuilder::CreateCollectionIterator(
    CollectionKind collection_kind, IterationKind iteration_kind) {
  CreateCollectionIteratorParameters parameters(collection_kind,
                                                iteration_kind);
  return new (zone()) Operator1<CreateCollectionIteratorParameters>(
      IrOpcode::kJSCreateCollectionIterator, Operator::kEliminatable,
      "JSCreateCollectionIterator", 1, 1, 1, 1, 1, 0, parameters);
}

const Operator* JSOperatorBuilder::CreateBoundFunction(size_t arity,
                                                       Handle<Map> map) {
  // bound_target_function, bound_this, arg1, ..., argN
  int const value_input_count = static_cast<int>(arity) + 2;
  CreateBoundFunctionParameters parameters(arity, map);
  return new (zone()) Operator1<CreateBoundFunctionParameters>(   // --
      IrOpcode::kJSCreateBoundFunction, Operator::kEliminatable,  // opcode
      "JSCreateBoundFunction",                                    // name
      value_input_count, 1, 1, 1, 1, 0,                           // counts
      parameters);                                                // parameter
}

const Operator* JSOperatorBuilder::CreateClosure(
    Handle<SharedFunctionInfo> shared_info, Handle<Code> code,
    AllocationType allocation) {
  static constexpr int kFeedbackCell = 1;
  static constexpr int kArity = kFeedbackCell;
  CreateClosureParameters parameters(shared_info, code, allocation);
  return new (zone()) Operator1<CreateClosureParameters>(   // --
      IrOpcode::kJSCreateClosure, Operator::kEliminatable,  // opcode
      "JSCreateClosure",                                    // name
      kArity, 1, 1, 1, 1, 0,                                // counts
      parameters);                                          // parameter
}

const Operator* JSOperatorBuilder::CreateLiteralArray(
    Handle<ArrayBoilerplateDescription> description,
    FeedbackSource const& feedback, int literal_flags, int number_of_elements) {
  CreateLiteralParameters parameters(description, feedback, number_of_elements,
                                     literal_flags);
  return new (zone()) Operator1<CreateLiteralParameters>(  // --
      IrOpcode::kJSCreateLiteralArray,                     // opcode
      Operator::kNoProperties,                             // properties
      "JSCreateLiteralArray",                              // name
      1, 1, 1, 1, 1, 2,                                    // counts
      parameters);                                         // parameter
}

const Operator* JSOperatorBuilder::CreateEmptyLiteralArray(
    FeedbackSource const& feedback) {
  static constexpr int kFeedbackVector = 1;
  static constexpr int kArity = kFeedbackVector;
  FeedbackParameter parameters(feedback);
  return new (zone()) Operator1<FeedbackParameter>(  // --
      IrOpcode::kJSCreateEmptyLiteralArray,          // opcode
      Operator::kEliminatable,                       // properties
      "JSCreateEmptyLiteralArray",                   // name
      kArity, 1, 1, 1, 1, 0,                         // counts
      parameters);                                   // parameter
}

const Operator* JSOperatorBuilder::CreateArrayFromIterable() {
  return new (zone()) Operator(              // --
      IrOpcode::kJSCreateArrayFromIterable,  // opcode
      Operator::kNoProperties,               // properties
      "JSCreateArrayFromIterable",           // name
      1, 1, 1, 1, 1, 2);                     // counts
}

const Operator* JSOperatorBuilder::CreateLiteralObject(
    Handle<ObjectBoilerplateDescription> constant_properties,
    FeedbackSource const& feedback, int literal_flags,
    int number_of_properties) {
  CreateLiteralParameters parameters(constant_properties, feedback,
                                     number_of_properties, literal_flags);
  return new (zone()) Operator1<CreateLiteralParameters>(  // --
      IrOpcode::kJSCreateLiteralObject,                    // opcode
      Operator::kNoProperties,                             // properties
      "JSCreateLiteralObject",                             // name
      1, 1, 1, 1, 1, 2,                                    // counts
      parameters);                                         // parameter
}

const Operator* JSOperatorBuilder::GetTemplateObject(
    Handle<TemplateObjectDescription> description,
    Handle<SharedFunctionInfo> shared, FeedbackSource const& feedback) {
  GetTemplateObjectParameters parameters(description, shared, feedback);
  return new (zone()) Operator1<GetTemplateObjectParameters>(  // --
      IrOpcode::kJSGetTemplateObject,                          // opcode
      Operator::kEliminatable,                                 // properties
      "JSGetTemplateObject",                                   // name
      1, 1, 1, 1, 1, 0,                                        // counts
      parameters);                                             // parameter
}

const Operator* JSOperatorBuilder::CloneObject(FeedbackSource const& feedback,
                                               int literal_flags) {
  CloneObjectParameters parameters(feedback, literal_flags);
  return new (zone()) Operator1<CloneObjectParameters>(  // --
      IrOpcode::kJSCloneObject,                          // opcode
      Operator::kNoProperties,                           // properties
      "JSCloneObject",                                   // name
      2, 1, 1, 1, 1, 2,                                  // counts
      parameters);                                       // parameter
}

const Operator* JSOperatorBuilder::StackCheck(StackCheckKind kind) {
  return new (zone()) Operator1<StackCheckKind>(  // --
      IrOpcode::kJSStackCheck,                    // opcode
      Operator::kNoWrite,                         // properties
      "JSStackCheck",                             // name
      0, 1, 1, 0, 1, 2,                           // counts
      kind);                                      // parameter
}

const Operator* JSOperatorBuilder::CreateEmptyLiteralObject() {
  return new (zone()) Operator(               // --
      IrOpcode::kJSCreateEmptyLiteralObject,  // opcode
      Operator::kNoProperties,                // properties
      "JSCreateEmptyLiteralObject",           // name
      0, 1, 1, 1, 1, 2);                      // counts
}

const Operator* JSOperatorBuilder::CreateLiteralRegExp(
    Handle<String> constant_pattern, FeedbackSource const& feedback,
    int literal_flags) {
  CreateLiteralParameters parameters(constant_pattern, feedback, -1,
                                     literal_flags);
  return new (zone()) Operator1<CreateLiteralParameters>(  // --
      IrOpcode::kJSCreateLiteralRegExp,                    // opcode
      Operator::kNoProperties,                             // properties
      "JSCreateLiteralRegExp",                             // name
      1, 1, 1, 1, 1, 2,                                    // counts
      parameters);                                         // parameter
}

const Operator* JSOperatorBuilder::CreateFunctionContext(
    Handle<ScopeInfo> scope_info, int slot_count, ScopeType scope_type) {
  CreateFunctionContextParameters parameters(scope_info, slot_count,
                                             scope_type);
  return new (zone()) Operator1<CreateFunctionContextParameters>(   // --
      IrOpcode::kJSCreateFunctionContext, Operator::kNoProperties,  // opcode
      "JSCreateFunctionContext",                                    // name
      0, 1, 1, 1, 1, 2,                                             // counts
      parameters);                                                  // parameter
}

const Operator* JSOperatorBuilder::CreateCatchContext(
    const Handle<ScopeInfo>& scope_info) {
  return new (zone()) Operator1<Handle<ScopeInfo>>(
      IrOpcode::kJSCreateCatchContext, Operator::kNoProperties,  // opcode
      "JSCreateCatchContext",                                    // name
      1, 1, 1, 1, 1, 2,                                          // counts
      scope_info);                                               // parameter
}

const Operator* JSOperatorBuilder::CreateWithContext(
    const Handle<ScopeInfo>& scope_info) {
  return new (zone()) Operator1<Handle<ScopeInfo>>(
      IrOpcode::kJSCreateWithContext, Operator::kNoProperties,  // opcode
      "JSCreateWithContext",                                    // name
      1, 1, 1, 1, 1, 2,                                         // counts
      scope_info);                                              // parameter
}

const Operator* JSOperatorBuilder::CreateBlockContext(
    const Handle<ScopeInfo>& scope_info) {
  return new (zone()) Operator1<Handle<ScopeInfo>>(              // --
      IrOpcode::kJSCreateBlockContext, Operator::kNoProperties,  // opcode
      "JSCreateBlockContext",                                    // name
      0, 1, 1, 1, 1, 2,                                          // counts
      scope_info);                                               // parameter
}

Handle<ScopeInfo> ScopeInfoOf(const Operator* op) {
  DCHECK(IrOpcode::kJSCreateBlockContext == op->opcode() ||
         IrOpcode::kJSCreateWithContext == op->opcode() ||
         IrOpcode::kJSCreateCatchContext == op->opcode());
  return OpParameter<Handle<ScopeInfo>>(op);
}

#undef CACHED_OP_LIST

}  // namespace compiler
}  // namespace internal
}  // namespace v8
