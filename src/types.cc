// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "types.h"

#include "string-stream.h"
#include "types-inl.h"

namespace v8 {
namespace internal {

template<class Config>
int TypeImpl<Config>::NumClasses() {
  if (this->IsClass()) {
    return 1;
  } else if (this->IsUnion()) {
    StructHandle unioned = this->AsUnion();
    int result = 0;
    for (int i = 0; i < Config::struct_length(unioned); ++i) {
      if (Config::struct_get(unioned, i)->IsClass()) ++result;
    }
    return result;
  } else {
    return 0;
  }
}


template<class Config>
int TypeImpl<Config>::NumConstants() {
  if (this->IsConstant()) {
    return 1;
  } else if (this->IsUnion()) {
    StructHandle unioned = this->AsUnion();
    int result = 0;
    for (int i = 0; i < Config::struct_length(unioned); ++i) {
      if (Config::struct_get(unioned, i)->IsConstant()) ++result;
    }
    return result;
  } else {
    return 0;
  }
}


template<class Config> template<class T>
typename TypeImpl<Config>::TypeHandle
TypeImpl<Config>::Iterator<T>::get_type() {
  ASSERT(!Done());
  return type_->IsUnion()
      ? Config::struct_get(type_->AsUnion(), index_) : type_;
}


// C++ cannot specialise nested templates, so we have to go through this
// contortion with an auxiliary template to simulate it.
template<class Config, class T>
struct TypeImplIteratorAux {
  static bool matches(typename TypeImpl<Config>::TypeHandle type);
  static i::Handle<T> current(typename TypeImpl<Config>::TypeHandle type);
};

template<class Config>
struct TypeImplIteratorAux<Config, i::Map> {
  static bool matches(typename TypeImpl<Config>::TypeHandle type) {
    return type->IsClass();
  }
  static i::Handle<i::Map> current(typename TypeImpl<Config>::TypeHandle type) {
    return type->AsClass();
  }
};

template<class Config>
struct TypeImplIteratorAux<Config, i::Object> {
  static bool matches(typename TypeImpl<Config>::TypeHandle type) {
    return type->IsConstant();
  }
  static i::Handle<i::Object> current(
      typename TypeImpl<Config>::TypeHandle type) {
    return type->AsConstant();
  }
};

template<class Config> template<class T>
bool TypeImpl<Config>::Iterator<T>::matches(TypeHandle type) {
  return TypeImplIteratorAux<Config, T>::matches(type);
}

template<class Config> template<class T>
i::Handle<T> TypeImpl<Config>::Iterator<T>::Current() {
  return TypeImplIteratorAux<Config, T>::current(get_type());
}


template<class Config> template<class T>
void TypeImpl<Config>::Iterator<T>::Advance() {
  ++index_;
  if (type_->IsUnion()) {
    StructHandle unioned = type_->AsUnion();
    for (; index_ < Config::struct_length(unioned); ++index_) {
      if (matches(Config::struct_get(unioned, index_))) return;
    }
  } else if (index_ == 0 && matches(type_)) {
    return;
  }
  index_ = -1;
}


// Get the smallest bitset subsuming this type.
template<class Config>
int TypeImpl<Config>::LubBitset() {
  if (this->IsBitset()) {
    return this->AsBitset();
  } else if (this->IsUnion()) {
    StructHandle unioned = this->AsUnion();
    int bitset = kNone;
    for (int i = 0; i < Config::struct_length(unioned); ++i) {
      bitset |= Config::struct_get(unioned, i)->LubBitset();
    }
    return bitset;
  } else if (this->IsClass()) {
    int bitset = Config::lub_bitset(this);
    return bitset ? bitset : LubBitset(*this->AsClass());
  } else {
    int bitset = Config::lub_bitset(this);
    return bitset ? bitset : LubBitset(*this->AsConstant());
  }
}


template<class Config>
int TypeImpl<Config>::LubBitset(i::Object* value) {
  if (value->IsSmi()) return kSignedSmall & kTaggedInt;
  i::Map* map = i::HeapObject::cast(value)->map();
  if (map->instance_type() == HEAP_NUMBER_TYPE) {
    int32_t i;
    uint32_t u;
    return kTaggedPtr & (
        value->ToInt32(&i) ? (Smi::IsValid(i) ? kSignedSmall : kOtherSigned32) :
        value->ToUint32(&u) ? kUnsigned32 : kFloat);
  }
  return LubBitset(map);
}


template<class Config>
int TypeImpl<Config>::LubBitset(i::Map* map) {
  switch (map->instance_type()) {
    case STRING_TYPE:
    case ASCII_STRING_TYPE:
    case CONS_STRING_TYPE:
    case CONS_ASCII_STRING_TYPE:
    case SLICED_STRING_TYPE:
    case SLICED_ASCII_STRING_TYPE:
    case EXTERNAL_STRING_TYPE:
    case EXTERNAL_ASCII_STRING_TYPE:
    case EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE:
    case SHORT_EXTERNAL_STRING_TYPE:
    case SHORT_EXTERNAL_ASCII_STRING_TYPE:
    case SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE:
    case INTERNALIZED_STRING_TYPE:
    case ASCII_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE:
    case SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE:
    case SHORT_EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE:
    case SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE:
      return kString;
    case SYMBOL_TYPE:
      return kSymbol;
    case ODDBALL_TYPE: {
      Heap* heap = map->GetHeap();
      if (map == heap->undefined_map()) return kUndefined;
      if (map == heap->the_hole_map()) return kAny;  // TODO(rossberg): kNone?
      if (map == heap->null_map()) return kNull;
      if (map == heap->boolean_map()) return kBoolean;
      ASSERT(map == heap->uninitialized_map() ||
             map == heap->no_interceptor_result_sentinel_map() ||
             map == heap->termination_exception_map() ||
             map == heap->arguments_marker_map());
      return kInternal & kTaggedPtr;
    }
    case HEAP_NUMBER_TYPE:
      return kFloat & kTaggedPtr;
    case JS_VALUE_TYPE:
    case JS_DATE_TYPE:
    case JS_OBJECT_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
    case JS_MODULE_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_BUILTINS_OBJECT_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_ARRAY_BUFFER_TYPE:
    case JS_TYPED_ARRAY_TYPE:
    case JS_DATA_VIEW_TYPE:
    case JS_SET_TYPE:
    case JS_MAP_TYPE:
    case JS_WEAK_MAP_TYPE:
    case JS_WEAK_SET_TYPE:
      if (map->is_undetectable()) return kUndetectable;
      return kOtherObject;
    case JS_ARRAY_TYPE:
      return kArray;
    case JS_FUNCTION_TYPE:
      return kFunction;
    case JS_REGEXP_TYPE:
      return kRegExp;
    case JS_PROXY_TYPE:
    case JS_FUNCTION_PROXY_TYPE:
      return kProxy;
    case MAP_TYPE:
      // When compiling stub templates, the meta map is used as a place holder
      // for the actual map with which the template is later instantiated.
      // We treat it as a kind of type variable whose upper bound is Any.
      // TODO(rossberg): for caching of CompareNilIC stubs to work correctly,
      // we must exclude Undetectable here. This makes no sense, really,
      // because it means that the template isn't actually parametric.
      // Also, it doesn't apply elsewhere. 8-(
      // We ought to find a cleaner solution for compiling stubs parameterised
      // over type or class variables, esp ones with bounds...
      return kDetectable;
    case DECLARED_ACCESSOR_INFO_TYPE:
    case EXECUTABLE_ACCESSOR_INFO_TYPE:
    case ACCESSOR_PAIR_TYPE:
    case FIXED_ARRAY_TYPE:
      return kInternal & kTaggedPtr;
    default:
      UNREACHABLE();
      return kNone;
  }
}


// Get the largest bitset subsumed by this type.
template<class Config>
int TypeImpl<Config>::GlbBitset() {
  if (this->IsBitset()) {
    return this->AsBitset();
  } else if (this->IsUnion()) {
    // All but the first are non-bitsets and thus would yield kNone anyway.
    return Config::struct_get(this->AsUnion(), 0)->GlbBitset();
  } else {
    return kNone;
  }
}


// Most precise _current_ type of a value (usually its class).
template<class Config>
typename TypeImpl<Config>::TypeHandle TypeImpl<Config>::NowOf(
    i::Object* value, Region* region) {
  if (value->IsSmi() ||
      i::HeapObject::cast(value)->map()->instance_type() == HEAP_NUMBER_TYPE) {
    return Of(value, region);
  }
  return Class(i::handle(i::HeapObject::cast(value)->map()), region);
}


// Check this <= that.
template<class Config>
bool TypeImpl<Config>::SlowIs(TypeImpl* that) {
  // Fast path for bitsets.
  if (this->IsNone()) return true;
  if (that->IsBitset()) {
    return (this->LubBitset() | that->AsBitset()) == that->AsBitset();
  }

  if (that->IsClass()) {
    return this->IsClass() && *this->AsClass() == *that->AsClass();
  }
  if (that->IsConstant()) {
    return this->IsConstant() && *this->AsConstant() == *that->AsConstant();
  }

  // (T1 \/ ... \/ Tn) <= T  <=>  (T1 <= T) /\ ... /\ (Tn <= T)
  if (this->IsUnion()) {
    StructHandle unioned = this->AsUnion();
    for (int i = 0; i < Config::struct_length(unioned); ++i) {
      TypeHandle this_i = Config::struct_get(unioned, i);
      if (!this_i->Is(that)) return false;
    }
    return true;
  }

  // T <= (T1 \/ ... \/ Tn)  <=>  (T <= T1) \/ ... \/ (T <= Tn)
  // (iff T is not a union)
  ASSERT(!this->IsUnion());
  if (that->IsUnion()) {
    StructHandle unioned = that->AsUnion();
    for (int i = 0; i < Config::struct_length(unioned); ++i) {
      TypeHandle that_i = Config::struct_get(unioned, i);
      if (this->Is(that_i)) return true;
      if (this->IsBitset()) break;  // Fast fail, only first field is a bitset.
    }
    return false;
  }

  return false;
}


template<class Config>
bool TypeImpl<Config>::NowIs(TypeImpl* that) {
  // TODO(rossberg): this is incorrect for
  //   Union(Constant(V), T)->NowIs(Class(M))
  // but fuzzing does not cover that!
  DisallowHeapAllocation no_allocation;
  if (this->IsConstant()) {
    i::Object* object = *this->AsConstant();
    if (object->IsHeapObject()) {
      i::Map* map = i::HeapObject::cast(object)->map();
      for (Iterator<i::Map> it = that->Classes(); !it.Done(); it.Advance()) {
        if (*it.Current() == map) return true;
      }
    }
  }
  return this->Is(that);
}


// Check this overlaps that.
template<class Config>
bool TypeImpl<Config>::Maybe(TypeImpl* that) {
  // (T1 \/ ... \/ Tn) overlaps T <=> (T1 overlaps T) \/ ... \/ (Tn overlaps T)
  if (this->IsUnion()) {
    StructHandle unioned = this->AsUnion();
    for (int i = 0; i < Config::struct_length(unioned); ++i) {
      TypeHandle this_i = Config::struct_get(unioned, i);
      if (this_i->Maybe(that)) return true;
    }
    return false;
  }

  // T overlaps (T1 \/ ... \/ Tn) <=> (T overlaps T1) \/ ... \/ (T overlaps Tn)
  if (that->IsUnion()) {
    StructHandle unioned = that->AsUnion();
    for (int i = 0; i < Config::struct_length(unioned); ++i) {
      TypeHandle that_i = Config::struct_get(unioned, i);
      if (this->Maybe(that_i)) return true;
    }
    return false;
  }

  ASSERT(!this->IsUnion() && !that->IsUnion());
  if (this->IsBitset()) {
    return IsInhabited(this->AsBitset() & that->LubBitset());
  }
  if (that->IsBitset()) {
    return IsInhabited(this->LubBitset() & that->AsBitset());
  }

  if (this->IsClass()) {
    return that->IsClass() && *this->AsClass() == *that->AsClass();
  }
  if (this->IsConstant()) {
    return that->IsConstant() && *this->AsConstant() == *that->AsConstant();
  }

  return false;
}


template<class Config>
bool TypeImpl<Config>::Contains(i::Object* value) {
  for (Iterator<i::Object> it = this->Constants(); !it.Done(); it.Advance()) {
    if (*it.Current() == value) return true;
  }
  return Config::from_bitset(LubBitset(value))->Is(this);
}


template<class Config>
bool TypeImpl<Config>::InUnion(StructHandle unioned, int current_size) {
  ASSERT(!this->IsUnion());
  for (int i = 0; i < current_size; ++i) {
    TypeHandle type = Config::struct_get(unioned, i);
    if (this->Is(type)) return true;
  }
  return false;
}


// Get non-bitsets from this which are not subsumed by union, store at result,
// starting at index. Returns updated index.
template<class Config>
int TypeImpl<Config>::ExtendUnion(
    StructHandle result, TypeHandle type, int current_size) {
  int old_size = current_size;
  if (type->IsClass() || type->IsConstant()) {
    if (!type->InUnion(result, old_size)) {
      Config::struct_set(result, current_size++, type);
    }
  } else if (type->IsUnion()) {
    StructHandle unioned = type->AsUnion();
    for (int i = 0; i < Config::struct_length(unioned); ++i) {
      TypeHandle type = Config::struct_get(unioned, i);
      ASSERT(i == 0 ||
             !(type->IsBitset() || type->Is(Config::struct_get(unioned, 0))));
      if (!type->IsBitset() && !type->InUnion(result, old_size)) {
        Config::struct_set(result, current_size++, type);
      }
    }
  }
  return current_size;
}


// Union is O(1) on simple bit unions, but O(n*m) on structured unions.
template<class Config>
typename TypeImpl<Config>::TypeHandle TypeImpl<Config>::Union(
    TypeHandle type1, TypeHandle type2, Region* region) {
  // Fast case: bit sets.
  if (type1->IsBitset() && type2->IsBitset()) {
    return Config::from_bitset(type1->AsBitset() | type2->AsBitset(), region);
  }

  // Fast case: top or bottom types.
  if (type1->IsAny() || type2->IsNone()) return type1;
  if (type2->IsAny() || type1->IsNone()) return type2;

  // Semi-fast case: Unioned objects are neither involved nor produced.
  if (!(type1->IsUnion() || type2->IsUnion())) {
    if (type1->Is(type2)) return type2;
    if (type2->Is(type1)) return type1;
  }

  // Slow case: may need to produce a Unioned object.
  int size = 0;
  if (!type1->IsBitset()) {
    size += (type1->IsUnion() ? Config::struct_length(type1->AsUnion()) : 1);
  }
  if (!type2->IsBitset()) {
    size += (type2->IsUnion() ? Config::struct_length(type2->AsUnion()) : 1);
  }
  int bitset = type1->GlbBitset() | type2->GlbBitset();
  if (bitset != kNone) ++size;
  ASSERT(size >= 1);
  StructHandle unioned = Config::struct_create(kUnionTag, size, region);

  size = 0;
  if (bitset != kNone) {
    Config::struct_set(unioned, size++, Config::from_bitset(bitset, region));
  }
  size = ExtendUnion(unioned, type1, size);
  size = ExtendUnion(unioned, type2, size);

  if (size == 1) {
    return Config::struct_get(unioned, 0);
  } else {
    Config::struct_shrink(unioned, size);
    return Config::from_struct(unioned);
  }
}


// Get non-bitsets from type which are also in other, store at result,
// starting at index. Returns updated index.
template<class Config>
int TypeImpl<Config>::ExtendIntersection(
    StructHandle result, TypeHandle type, TypeHandle other, int current_size) {
  int old_size = current_size;
  if (type->IsClass() || type->IsConstant()) {
    if (type->Is(other) && !type->InUnion(result, old_size)) {
      Config::struct_set(result, current_size++, type);
    }
  } else if (type->IsUnion()) {
    StructHandle unioned = type->AsUnion();
    for (int i = 0; i < Config::struct_length(unioned); ++i) {
      TypeHandle type = Config::struct_get(unioned, i);
      ASSERT(i == 0 ||
             !(type->IsBitset() || type->Is(Config::struct_get(unioned, 0))));
      if (!type->IsBitset() && type->Is(other) &&
          !type->InUnion(result, old_size)) {
        Config::struct_set(result, current_size++, type);
      }
    }
  }
  return current_size;
}


// Intersection is O(1) on simple bit unions, but O(n*m) on structured unions.
// TODO(rossberg): Should we use object sets somehow? Is it worth it?
template<class Config>
typename TypeImpl<Config>::TypeHandle TypeImpl<Config>::Intersect(
    TypeHandle type1, TypeHandle type2, Region* region) {
  // Fast case: bit sets.
  if (type1->IsBitset() && type2->IsBitset()) {
    return Config::from_bitset(type1->AsBitset() & type2->AsBitset(), region);
  }

  // Fast case: top or bottom types.
  if (type1->IsNone() || type2->IsAny()) return type1;
  if (type2->IsNone() || type1->IsAny()) return type2;

  // Semi-fast case: Unioned objects are neither involved nor produced.
  if (!(type1->IsUnion() || type2->IsUnion())) {
    if (type1->Is(type2)) return type1;
    if (type2->Is(type1)) return type2;
  }

  // Slow case: may need to produce a Unioned object.
  int size = 0;
  if (!type1->IsBitset()) {
    size += (type1->IsUnion() ? Config::struct_length(type1->AsUnion()) : 1);
  }
  if (!type2->IsBitset()) {
    size += (type2->IsUnion() ? Config::struct_length(type2->AsUnion()) : 1);
  }
  int bitset = type1->GlbBitset() & type2->GlbBitset();
  if (bitset != kNone) ++size;
  ASSERT(size >= 1);
  StructHandle unioned = Config::struct_create(kUnionTag, size, region);

  size = 0;
  if (bitset != kNone) {
    Config::struct_set(unioned, size++, Config::from_bitset(bitset, region));
  }
  size = ExtendIntersection(unioned, type1, type2, size);
  size = ExtendIntersection(unioned, type2, type1, size);

  if (size == 0) {
    return None(region);
  } else if (size == 1) {
    return Config::struct_get(unioned, 0);
  } else {
    Config::struct_shrink(unioned, size);
    return Config::from_struct(unioned);
  }
}


template<class Config>
template<class OtherType>
typename TypeImpl<Config>::TypeHandle TypeImpl<Config>::Convert(
    typename OtherType::TypeHandle type, Region* region) {
  if (type->IsBitset()) {
    return Config::from_bitset(type->AsBitset(), region);
  } else if (type->IsClass()) {
    return Config::from_class(type->AsClass(), type->LubBitset(), region);
  } else if (type->IsConstant()) {
    return Config::from_constant(type->AsConstant(), type->LubBitset(), region);
  } else {
    ASSERT(type->IsUnion());
    typename OtherType::StructHandle unioned = type->AsUnion();
    int length = OtherType::StructLength(unioned);
    StructHandle new_unioned = Config::struct_create(kUnionTag, length, region);
    for (int i = 0; i < length; ++i) {
      Config::struct_set(new_unioned, i,
          Convert<OtherType>(OtherType::StructGet(unioned, i), region));
    }
    return Config::from_struct(new_unioned);
  }
}


// TODO(rossberg): this does not belong here.
Representation Representation::FromType(Type* type) {
  if (type->Is(Type::None())) return Representation::None();
  if (type->Is(Type::SignedSmall())) return Representation::Smi();
  if (type->Is(Type::Signed32())) return Representation::Integer32();
  if (type->Is(Type::Number())) return Representation::Double();
  return Representation::Tagged();
}


template<class Config>
void TypeImpl<Config>::TypePrint(PrintDimension dim) {
  TypePrint(stdout, dim);
  PrintF(stdout, "\n");
  Flush(stdout);
}


template<class Config>
const char* TypeImpl<Config>::bitset_name(int bitset) {
  switch (bitset) {
    case kAny & kRepresentation: return "Any";
    #define PRINT_COMPOSED_TYPE(type, value) \
    case k##type & kRepresentation: return #type;
    REPRESENTATION_BITSET_TYPE_LIST(PRINT_COMPOSED_TYPE)
    #undef PRINT_COMPOSED_TYPE

    #define PRINT_COMPOSED_TYPE(type, value) \
    case k##type & kSemantic: return #type;
    SEMANTIC_BITSET_TYPE_LIST(PRINT_COMPOSED_TYPE)
    #undef PRINT_COMPOSED_TYPE

    default:
      return NULL;
  }
}


template<class Config>
void TypeImpl<Config>::BitsetTypePrint(FILE* out, int bitset) {
  const char* name = bitset_name(bitset);
  if (name != NULL) {
    PrintF(out, "%s", name);
  } else {
    static const int named_bitsets[] = {
      #define BITSET_CONSTANT(type, value) k##type & kRepresentation,
      REPRESENTATION_BITSET_TYPE_LIST(BITSET_CONSTANT)
      #undef BITSET_CONSTANT

      #define BITSET_CONSTANT(type, value) k##type & kSemantic,
      SEMANTIC_BITSET_TYPE_LIST(BITSET_CONSTANT)
      #undef BITSET_CONSTANT
    };

    bool is_first = true;
    PrintF(out, "(");
    for (int i(ARRAY_SIZE(named_bitsets) - 1); bitset != 0 && i >= 0; --i) {
      int subset = named_bitsets[i];
      if ((bitset & subset) == subset) {
        if (!is_first) PrintF(out, " | ");
        is_first = false;
        PrintF(out, "%s", bitset_name(subset));
        bitset -= subset;
      }
    }
    ASSERT(bitset == 0);
    PrintF(out, ")");
  }
}


template<class Config>
void TypeImpl<Config>::TypePrint(FILE* out, PrintDimension dim) {
  if (this->IsBitset()) {
    int bitset = this->AsBitset();
    switch (dim) {
      case BOTH_DIMS:
        BitsetTypePrint(out, bitset & kSemantic);
        PrintF(out, "/");
        BitsetTypePrint(out, bitset & kRepresentation);
        break;
      case SEMANTIC_DIM:
        BitsetTypePrint(out, bitset & kSemantic);
        break;
      case REPRESENTATION_DIM:
        BitsetTypePrint(out, bitset & kRepresentation);
        break;
    }
  } else if (this->IsConstant()) {
    PrintF(out, "Constant(%p : ", static_cast<void*>(*this->AsConstant()));
    Config::from_bitset(this->LubBitset())->TypePrint(out, dim);
    PrintF(out, ")");
  } else if (this->IsClass()) {
    PrintF(out, "Class(%p < ", static_cast<void*>(*this->AsClass()));
    Config::from_bitset(this->LubBitset())->TypePrint(out, dim);
    PrintF(out, ")");
  } else if (this->IsUnion()) {
    PrintF(out, "(");
    StructHandle unioned = this->AsUnion();
    for (int i = 0; i < Config::struct_length(unioned); ++i) {
      TypeHandle type_i = Config::struct_get(unioned, i);
      if (i > 0) PrintF(out, " | ");
      type_i->TypePrint(out, dim);
    }
    PrintF(out, ")");
  }
}


template class TypeImpl<ZoneTypeConfig>;
template class TypeImpl<ZoneTypeConfig>::Iterator<i::Map>;
template class TypeImpl<ZoneTypeConfig>::Iterator<i::Object>;

template class TypeImpl<HeapTypeConfig>;
template class TypeImpl<HeapTypeConfig>::Iterator<i::Map>;
template class TypeImpl<HeapTypeConfig>::Iterator<i::Object>;

template TypeImpl<ZoneTypeConfig>::TypeHandle
  TypeImpl<ZoneTypeConfig>::Convert<HeapType>(
    TypeImpl<HeapTypeConfig>::TypeHandle, TypeImpl<ZoneTypeConfig>::Region*);
template TypeImpl<HeapTypeConfig>::TypeHandle
  TypeImpl<HeapTypeConfig>::Convert<Type>(
    TypeImpl<ZoneTypeConfig>::TypeHandle, TypeImpl<HeapTypeConfig>::Region*);

} }  // namespace v8::internal
