// Copyright 2013 the V8 project authors. All rights reserved.
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

#include <vector>

#include "cctest.h"
#include "types.h"
#include "utils/random-number-generator.h"

using namespace v8::internal;

// Testing auxiliaries (breaking the Type abstraction).
struct ZoneRep {
  typedef void* Struct;

  static bool IsStruct(Type* t, int tag) {
    return !IsBitset(t) && reinterpret_cast<intptr_t>(AsStruct(t)[0]) == tag;
  }
  static bool IsBitset(Type* t) { return reinterpret_cast<intptr_t>(t) & 1; }
  static bool IsClass(Type* t) { return IsStruct(t, 0); }
  static bool IsConstant(Type* t) { return IsStruct(t, 1); }
  static bool IsUnion(Type* t) { return IsStruct(t, 2); }

  static Struct* AsStruct(Type* t) {
    return reinterpret_cast<Struct*>(t);
  }
  static int AsBitset(Type* t) {
    return static_cast<int>(reinterpret_cast<intptr_t>(t) >> 1);
  }
  static Map* AsClass(Type* t) {
    return *static_cast<Map**>(AsStruct(t)[3]);
  }
  static Object* AsConstant(Type* t) {
    return *static_cast<Object**>(AsStruct(t)[3]);
  }
  static Struct* AsUnion(Type* t) {
    return AsStruct(t);
  }
  static int Length(Struct* structured) {
    return static_cast<int>(reinterpret_cast<intptr_t>(structured[1]));
  }

  static Zone* ToRegion(Zone* zone, Isolate* isolate) { return zone; }
};


struct HeapRep {
  typedef FixedArray Struct;

  static bool IsStruct(Handle<HeapType> t, int tag) {
    return t->IsFixedArray() && Smi::cast(AsStruct(t)->get(0))->value() == tag;
  }
  static bool IsBitset(Handle<HeapType> t) { return t->IsSmi(); }
  static bool IsClass(Handle<HeapType> t) { return t->IsMap(); }
  static bool IsConstant(Handle<HeapType> t) { return t->IsBox(); }
  static bool IsUnion(Handle<HeapType> t) { return IsStruct(t, 2); }

  static Struct* AsStruct(Handle<HeapType> t) { return FixedArray::cast(*t); }
  static int AsBitset(Handle<HeapType> t) { return Smi::cast(*t)->value(); }
  static Map* AsClass(Handle<HeapType> t) { return Map::cast(*t); }
  static Object* AsConstant(Handle<HeapType> t) {
    return Box::cast(*t)->value();
  }
  static Struct* AsUnion(Handle<HeapType> t) { return AsStruct(t); }
  static int Length(Struct* structured) { return structured->length() - 1; }

  static Isolate* ToRegion(Zone* zone, Isolate* isolate) { return isolate; }
};


template<class Type, class TypeHandle, class Region>
class Types {
 public:
  Types(Region* region, Isolate* isolate) : region_(region) {
    #define DECLARE_TYPE(name, value) \
      name = Type::name(region); \
      types.push_back(name);
    BITSET_TYPE_LIST(DECLARE_TYPE)
    #undef DECLARE_TYPE

    object_map = isolate->factory()->NewMap(JS_OBJECT_TYPE, 3 * kPointerSize);
    array_map = isolate->factory()->NewMap(JS_ARRAY_TYPE, 4 * kPointerSize);
    uninitialized_map = isolate->factory()->uninitialized_map();
    ObjectClass = Type::Class(object_map, region);
    ArrayClass = Type::Class(array_map, region);
    UninitializedClass = Type::Class(uninitialized_map, region);

    maps.push_back(object_map);
    maps.push_back(array_map);
    maps.push_back(uninitialized_map);
    for (MapVector::iterator it = maps.begin(); it != maps.end(); ++it) {
      types.push_back(Type::Class(*it, region));
    }

    smi = handle(Smi::FromInt(666), isolate);
    signed32 = isolate->factory()->NewHeapNumber(0x40000000);
    object1 = isolate->factory()->NewJSObjectFromMap(object_map);
    object2 = isolate->factory()->NewJSObjectFromMap(object_map);
    array = isolate->factory()->NewJSArray(20);
    uninitialized = isolate->factory()->uninitialized_value();
    SmiConstant = Type::Constant(smi, region);
    Signed32Constant = Type::Constant(signed32, region);
    ObjectConstant1 = Type::Constant(object1, region);
    ObjectConstant2 = Type::Constant(object2, region);
    ArrayConstant = Type::Constant(array, region);
    UninitializedConstant = Type::Constant(uninitialized, region);

    values.push_back(smi);
    values.push_back(signed32);
    values.push_back(object1);
    values.push_back(object2);
    values.push_back(array);
    values.push_back(uninitialized);
    for (ValueVector::iterator it = values.begin(); it != values.end(); ++it) {
      types.push_back(Type::Constant(*it, region));
    }

    for (int i = 0; i < 100; ++i) {
      types.push_back(Fuzz());
    }
  }

  #define DECLARE_TYPE(name, value) TypeHandle name;
  BITSET_TYPE_LIST(DECLARE_TYPE)
  #undef DECLARE_TYPE

  TypeHandle ObjectClass;
  TypeHandle ArrayClass;
  TypeHandle UninitializedClass;

  TypeHandle SmiConstant;
  TypeHandle Signed32Constant;
  TypeHandle ObjectConstant1;
  TypeHandle ObjectConstant2;
  TypeHandle ArrayConstant;
  TypeHandle UninitializedConstant;

  Handle<i::Map> object_map;
  Handle<i::Map> array_map;
  Handle<i::Map> uninitialized_map;

  Handle<i::Smi> smi;
  Handle<i::HeapNumber> signed32;
  Handle<i::JSObject> object1;
  Handle<i::JSObject> object2;
  Handle<i::JSArray> array;
  Handle<i::Oddball> uninitialized;

  typedef std::vector<TypeHandle> TypeVector;
  typedef std::vector<Handle<i::Map> > MapVector;
  typedef std::vector<Handle<i::Object> > ValueVector;
  TypeVector types;
  MapVector maps;
  ValueVector values;

  TypeHandle Of(Handle<i::Object> value) {
    return Type::Of(value, region_);
  }

  TypeHandle NowOf(Handle<i::Object> value) {
    return Type::NowOf(value, region_);
  }

  TypeHandle Constant(Handle<i::Object> value) {
    return Type::Constant(value, region_);
  }

  TypeHandle Class(Handle<i::Map> map) {
    return Type::Class(map, region_);
  }

  TypeHandle Union(TypeHandle t1, TypeHandle t2) {
    return Type::Union(t1, t2, region_);
  }
  TypeHandle Intersect(TypeHandle t1, TypeHandle t2) {
    return Type::Intersect(t1, t2, region_);
  }

  template<class Type2, class TypeHandle2>
  TypeHandle Convert(TypeHandle2 t) {
    return Type::template Convert<Type2>(t, region_);
  }

  TypeHandle Fuzz(int depth = 5) {
    switch (rng_.NextInt(depth == 0 ? 3 : 20)) {
      case 0: {  // bitset
        int n = 0
        #define COUNT_BITSET_TYPES(type, value) + 1
        BITSET_TYPE_LIST(COUNT_BITSET_TYPES)
        #undef COUNT_BITSET_TYPES
        ;
        int i = rng_.NextInt(n);
        #define PICK_BITSET_TYPE(type, value) \
          if (i-- == 0) return Type::type(region_);
        BITSET_TYPE_LIST(PICK_BITSET_TYPE)
        #undef PICK_BITSET_TYPE
        UNREACHABLE();
      }
      case 1: {  // class
        int i = rng_.NextInt(static_cast<int>(maps.size()));
        return Type::Class(maps[i], region_);
      }
      case 2: {  // constant
        int i = rng_.NextInt(static_cast<int>(values.size()));
        return Type::Constant(values[i], region_);
      }
      default: {  // union
        int n = rng_.NextInt(10);
        TypeHandle type = None;
        for (int i = 0; i < n; ++i) {
          type = Type::Union(type, Fuzz(depth - 1), region_);
        }
        return type;
      }
    }
    UNREACHABLE();
  }

 private:
  Region* region_;
  RandomNumberGenerator rng_;
};


template<class Type, class TypeHandle, class Region, class Rep>
struct Tests : Rep {
  typedef Types<Type, TypeHandle, Region> TypesInstance;
  typedef typename TypesInstance::TypeVector::iterator TypeIterator;
  typedef typename TypesInstance::MapVector::iterator MapIterator;
  typedef typename TypesInstance::ValueVector::iterator ValueIterator;

  Isolate* isolate;
  HandleScope scope;
  Zone zone;
  TypesInstance T;

  Tests() :
      isolate(CcTest::i_isolate()),
      scope(isolate),
      zone(isolate),
      T(Rep::ToRegion(&zone, isolate), isolate) {
  }

  bool Equal(TypeHandle type1, TypeHandle type2) {
    return
        type1->Is(type2) && type2->Is(type1) &&
        Rep::IsBitset(type1) == Rep::IsBitset(type2) &&
        Rep::IsClass(type1) == Rep::IsClass(type2) &&
        Rep::IsConstant(type1) == Rep::IsConstant(type2) &&
        Rep::IsUnion(type1) == Rep::IsUnion(type2) &&
        type1->NumClasses() == type2->NumClasses() &&
        type1->NumConstants() == type2->NumConstants() &&
        (!Rep::IsBitset(type1) ||
          Rep::AsBitset(type1) == Rep::AsBitset(type2)) &&
        (!Rep::IsClass(type1) ||
          Rep::AsClass(type1) == Rep::AsClass(type2)) &&
        (!Rep::IsConstant(type1) ||
          Rep::AsConstant(type1) == Rep::AsConstant(type2)) &&
        (!Rep::IsUnion(type1) ||
          Rep::Length(Rep::AsUnion(type1)) == Rep::Length(Rep::AsUnion(type2)));
  }

  void CheckEqual(TypeHandle type1, TypeHandle type2) {
    CHECK(Equal(type1, type2));
  }

  void CheckSub(TypeHandle type1, TypeHandle type2) {
    CHECK(type1->Is(type2));
    CHECK(!type2->Is(type1));
    if (Rep::IsBitset(type1) && Rep::IsBitset(type2)) {
      CHECK_NE(Rep::AsBitset(type1), Rep::AsBitset(type2));
    }
  }

  void CheckUnordered(TypeHandle type1, TypeHandle type2) {
    CHECK(!type1->Is(type2));
    CHECK(!type2->Is(type1));
    if (Rep::IsBitset(type1) && Rep::IsBitset(type2)) {
      CHECK_NE(Rep::AsBitset(type1), Rep::AsBitset(type2));
    }
  }

  void CheckOverlap(TypeHandle type1, TypeHandle type2, TypeHandle mask) {
    CHECK(type1->Maybe(type2));
    CHECK(type2->Maybe(type1));
    if (Rep::IsBitset(type1) && Rep::IsBitset(type2)) {
      CHECK_NE(0,
          Rep::AsBitset(type1) & Rep::AsBitset(type2) & Rep::AsBitset(mask));
    }
  }

  void CheckDisjoint(TypeHandle type1, TypeHandle type2, TypeHandle mask) {
    CHECK(!type1->Is(type2));
    CHECK(!type2->Is(type1));
    CHECK(!type1->Maybe(type2));
    CHECK(!type2->Maybe(type1));
    if (Rep::IsBitset(type1) && Rep::IsBitset(type2)) {
      CHECK_EQ(0,
          Rep::AsBitset(type1) & Rep::AsBitset(type2) & Rep::AsBitset(mask));
    }
  }

  void Bitset() {
    // None and Any are bitsets.
    CHECK(this->IsBitset(T.None));
    CHECK(this->IsBitset(T.Any));

    CHECK_EQ(0, this->AsBitset(T.None));
    CHECK_EQ(-1, this->AsBitset(T.Any));

    // Union(T1, T2) is bitset for bitsets T1,T2
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        TypeHandle type1 = *it1;
        TypeHandle type2 = *it2;
        CHECK(!(this->IsBitset(type1) && this->IsBitset(type2)) ||
              this->IsBitset(T.Union(type1, type2)));
      }
    }

    // Union(T1, T2) is bitset if T2 is bitset and T1->Is(T2)
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        TypeHandle type1 = *it1;
        TypeHandle type2 = *it2;
        CHECK(!(this->IsBitset(type2) && type1->Is(type2)) ||
              this->IsBitset(T.Union(type1, type2)));
      }
    }

    // Union(T1, T2) is bitwise disjunction for bitsets T1,T2
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        TypeHandle type1 = *it1;
        TypeHandle type2 = *it2;
        if (this->IsBitset(type1) && this->IsBitset(type2)) {
          CHECK_EQ(
              this->AsBitset(type1) | this->AsBitset(type2),
              this->AsBitset(T.Union(type1, type2)));
        }
      }
    }

    // Intersect(T1, T2) is bitwise conjunction for bitsets T1,T2
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        TypeHandle type1 = *it1;
        TypeHandle type2 = *it2;
        if (this->IsBitset(type1) && this->IsBitset(type2)) {
          CHECK_EQ(
              this->AsBitset(type1) & this->AsBitset(type2),
              this->AsBitset(T.Intersect(type1, type2)));
        }
      }
    }
  }

  void Class() {
    // Constructor
    for (MapIterator mt = T.maps.begin(); mt != T.maps.end(); ++mt) {
      Handle<i::Map> map = *mt;
      CHECK(this->IsClass(T.Class(map)));
    }

    // Map attribute
    for (MapIterator mt = T.maps.begin(); mt != T.maps.end(); ++mt) {
      Handle<i::Map> map = *mt;
      CHECK(*map == *T.Class(map)->AsClass());
    }

    // Functionality & Injectivity: Class(M1) = Class(M2) iff M1 = M2
    for (MapIterator mt1 = T.maps.begin(); mt1 != T.maps.end(); ++mt1) {
      for (MapIterator mt2 = T.maps.begin(); mt2 != T.maps.end(); ++mt2) {
        Handle<i::Map> map1 = *mt1;
        Handle<i::Map> map2 = *mt2;
        CHECK(Equal(T.Class(map1), T.Class(map2)) == (*map1 == *map2));
      }
    }
  }

  void Constant() {
    // Constructor
    for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
      Handle<i::Object> value = *vt;
      CHECK(this->IsConstant(T.Constant(value)));
    }

    // Value attribute
    for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
      Handle<i::Object> value = *vt;
      CHECK(*value == *T.Constant(value)->AsConstant());
    }

    // Functionality & Injectivity: Constant(V1) = Constant(v2) iff V1 = V2
    for (ValueIterator vt1 = T.values.begin(); vt1 != T.values.end(); ++vt1) {
      for (ValueIterator vt2 = T.values.begin(); vt2 != T.values.end(); ++vt2) {
        Handle<i::Object> val1 = *vt1;
        Handle<i::Object> val2 = *vt2;
        CHECK(Equal(T.Constant(val1), T.Constant(val2)) == (*val1 == *val2));
      }
    }
  }

  void Of() {
    // Constant(V)->Is(Of(V))
    for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
      Handle<i::Object> value = *vt;
      CHECK(T.Constant(value)->Is(T.Of(value)));
    }

    // Constant(V)->Is(T) iff Of(V)->Is(T) or T->Maybe(Constant(V))
    for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
      for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
        Handle<i::Object> value = *vt;
        TypeHandle type = *it;
        CHECK(T.Constant(value)->Is(type) ==
              (T.Of(value)->Is(type) || type->Maybe(T.Constant(value))));
      }
    }
  }

  void NowOf() {
    // Constant(V)->NowIs(NowOf(V))
    for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
      Handle<i::Object> value = *vt;
      CHECK(T.Constant(value)->NowIs(T.NowOf(value)));
    }

    // NowOf(V)->Is(Of(V))
    for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
      Handle<i::Object> value = *vt;
      CHECK(T.NowOf(value)->Is(T.Of(value)));
    }

    // Constant(V)->NowIs(T) iff NowOf(V)->NowIs(T) or T->Maybe(Constant(V))
    for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
      for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
        Handle<i::Object> value = *vt;
        TypeHandle type = *it;
        CHECK(T.Constant(value)->NowIs(type) ==
              (T.NowOf(value)->NowIs(type) || type->Maybe(T.Constant(value))));
      }
    }

    // Constant(V)->Is(T) implies NowOf(V)->Is(T) or T->Maybe(Constant(V))
    for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
      for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
        Handle<i::Object> value = *vt;
        TypeHandle type = *it;
        CHECK(!T.Constant(value)->Is(type) ||
              (T.NowOf(value)->Is(type) || type->Maybe(T.Constant(value))));
      }
    }
  }

  void Is() {
    // Least Element (Bottom): None->Is(T)
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      CHECK(T.None->Is(type));
    }

    // Greatest Element (Top): T->Is(Any)
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      CHECK(type->Is(T.Any));
    }

    // Bottom Uniqueness: T->Is(None) implies T = None
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      if (type->Is(T.None)) CheckEqual(type, T.None);
    }

    // Top Uniqueness: Any->Is(T) implies T = Any
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      if (T.Any->Is(type)) CheckEqual(type, T.Any);
    }

    // Reflexivity: T->Is(T)
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      CHECK(type->Is(type));
    }

    // Transitivity: T1->Is(T2) and T2->Is(T3) implies T1->Is(T3)
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        for (TypeIterator it3 = T.types.begin(); it3 != T.types.end(); ++it3) {
          TypeHandle type1 = *it1;
          TypeHandle type2 = *it2;
          TypeHandle type3 = *it3;
          CHECK(!(type1->Is(type2) && type2->Is(type3)) || type1->Is(type3));
        }
      }
    }

    // Antisymmetry: T1->Is(T2) and T2->Is(T1) iff T1 = T2
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        TypeHandle type1 = *it1;
        TypeHandle type2 = *it2;
        CHECK((type1->Is(type2) && type2->Is(type1)) == Equal(type1, type2));
      }
    }

    // Constant(V1)->Is(Constant(V2)) iff V1 = V2
    for (ValueIterator vt1 = T.values.begin(); vt1 != T.values.end(); ++vt1) {
      for (ValueIterator vt2 = T.values.begin(); vt2 != T.values.end(); ++vt2) {
        Handle<i::Object> val1 = *vt1;
        Handle<i::Object> val2 = *vt2;
        CHECK(T.Constant(val1)->Is(T.Constant(val2)) == (*val1 == *val2));
      }
    }

    // Class(M1)->Is(Class(M2)) iff M1 = M2
    for (MapIterator mt1 = T.maps.begin(); mt1 != T.maps.end(); ++mt1) {
      for (MapIterator mt2 = T.maps.begin(); mt2 != T.maps.end(); ++mt2) {
        Handle<i::Map> map1 = *mt1;
        Handle<i::Map> map2 = *mt2;
        CHECK(T.Class(map1)->Is(T.Class(map2)) == (*map1 == *map2));
      }
    }

    // Constant(V)->Is(Class(M)) never
    for (MapIterator mt = T.maps.begin(); mt != T.maps.end(); ++mt) {
      for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
        Handle<i::Map> map = *mt;
        Handle<i::Object> value = *vt;
        CHECK(!T.Constant(value)->Is(T.Class(map)));
      }
    }

    // Class(M)->Is(Constant(V)) never
    for (MapIterator mt = T.maps.begin(); mt != T.maps.end(); ++mt) {
      for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
        Handle<i::Map> map = *mt;
        Handle<i::Object> value = *vt;
        CHECK(!T.Class(map)->Is(T.Constant(value)));
      }
    }

    // Basic types
    CheckUnordered(T.Boolean, T.Null);
    CheckUnordered(T.Undefined, T.Null);
    CheckUnordered(T.Boolean, T.Undefined);

    CheckSub(T.SignedSmall, T.Number);
    CheckSub(T.Signed32, T.Number);
    CheckSub(T.Float, T.Number);
    CheckSub(T.SignedSmall, T.Signed32);
    CheckUnordered(T.SignedSmall, T.Float);
    CheckUnordered(T.Signed32, T.Float);

    CheckSub(T.UniqueName, T.Name);
    CheckSub(T.String, T.Name);
    CheckSub(T.InternalizedString, T.String);
    CheckSub(T.InternalizedString, T.UniqueName);
    CheckSub(T.InternalizedString, T.Name);
    CheckSub(T.Symbol, T.UniqueName);
    CheckSub(T.Symbol, T.Name);
    CheckUnordered(T.String, T.UniqueName);
    CheckUnordered(T.String, T.Symbol);
    CheckUnordered(T.InternalizedString, T.Symbol);

    CheckSub(T.Object, T.Receiver);
    CheckSub(T.Array, T.Object);
    CheckSub(T.Function, T.Object);
    CheckSub(T.Proxy, T.Receiver);
    CheckUnordered(T.Object, T.Proxy);
    CheckUnordered(T.Array, T.Function);

    // Structural types
    CheckSub(T.ObjectClass, T.Object);
    CheckSub(T.ArrayClass, T.Object);
    CheckSub(T.UninitializedClass, T.Internal);
    CheckUnordered(T.ObjectClass, T.ArrayClass);
    CheckUnordered(T.UninitializedClass, T.Null);
    CheckUnordered(T.UninitializedClass, T.Undefined);

    CheckSub(T.SmiConstant, T.SignedSmall);
    CheckSub(T.SmiConstant, T.Signed32);
    CheckSub(T.SmiConstant, T.Number);
    CheckSub(T.ObjectConstant1, T.Object);
    CheckSub(T.ObjectConstant2, T.Object);
    CheckSub(T.ArrayConstant, T.Object);
    CheckSub(T.ArrayConstant, T.Array);
    CheckSub(T.UninitializedConstant, T.Internal);
    CheckUnordered(T.ObjectConstant1, T.ObjectConstant2);
    CheckUnordered(T.ObjectConstant1, T.ArrayConstant);
    CheckUnordered(T.UninitializedConstant, T.Null);
    CheckUnordered(T.UninitializedConstant, T.Undefined);

    CheckUnordered(T.ObjectConstant1, T.ObjectClass);
    CheckUnordered(T.ObjectConstant2, T.ObjectClass);
    CheckUnordered(T.ObjectConstant1, T.ArrayClass);
    CheckUnordered(T.ObjectConstant2, T.ArrayClass);
    CheckUnordered(T.ArrayConstant, T.ObjectClass);
  }

  void NowIs() {
    // Least Element (Bottom): None->NowIs(T)
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      CHECK(T.None->NowIs(type));
    }

    // Greatest Element (Top): T->NowIs(Any)
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      CHECK(type->NowIs(T.Any));
    }

    // Bottom Uniqueness: T->NowIs(None) implies T = None
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      if (type->NowIs(T.None)) CheckEqual(type, T.None);
    }

    // Top Uniqueness: Any->NowIs(T) implies T = Any
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      if (T.Any->NowIs(type)) CheckEqual(type, T.Any);
    }

    // Reflexivity: T->NowIs(T)
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      CHECK(type->NowIs(type));
    }

    // Transitivity: T1->NowIs(T2) and T2->NowIs(T3) implies T1->NowIs(T3)
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        for (TypeIterator it3 = T.types.begin(); it3 != T.types.end(); ++it3) {
          TypeHandle type1 = *it1;
          TypeHandle type2 = *it2;
          TypeHandle type3 = *it3;
          CHECK(!(type1->NowIs(type2) && type2->NowIs(type3)) ||
                type1->NowIs(type3));
        }
      }
    }

    // Antisymmetry: T1->NowIs(T2) and T2->NowIs(T1) iff T1 = T2
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        TypeHandle type1 = *it1;
        TypeHandle type2 = *it2;
        CHECK((type1->NowIs(type2) && type2->NowIs(type1)) ==
              Equal(type1, type2));
      }
    }

    // T1->Is(T2) implies T1->NowIs(T2)
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        TypeHandle type1 = *it1;
        TypeHandle type2 = *it2;
        CHECK(!type1->Is(type2) || type1->NowIs(type2));
      }
    }

    // Constant(V1)->NowIs(Constant(V2)) iff V1 = V2
    for (ValueIterator vt1 = T.values.begin(); vt1 != T.values.end(); ++vt1) {
      for (ValueIterator vt2 = T.values.begin(); vt2 != T.values.end(); ++vt2) {
        Handle<i::Object> val1 = *vt1;
        Handle<i::Object> val2 = *vt2;
        CHECK(T.Constant(val1)->NowIs(T.Constant(val2)) == (*val1 == *val2));
      }
    }

    // Class(M1)->NowIs(Class(M2)) iff M1 = M2
    for (MapIterator mt1 = T.maps.begin(); mt1 != T.maps.end(); ++mt1) {
      for (MapIterator mt2 = T.maps.begin(); mt2 != T.maps.end(); ++mt2) {
        Handle<i::Map> map1 = *mt1;
        Handle<i::Map> map2 = *mt2;
        CHECK(T.Class(map1)->NowIs(T.Class(map2)) == (*map1 == *map2));
      }
    }

    // Constant(V)->NowIs(Class(M)) iff V has map M
    for (MapIterator mt = T.maps.begin(); mt != T.maps.end(); ++mt) {
      for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
        Handle<i::Map> map = *mt;
        Handle<i::Object> value = *vt;
        CHECK((value->IsHeapObject() &&
               i::HeapObject::cast(*value)->map() == *map)
              == T.Constant(value)->NowIs(T.Class(map)));
      }
    }

    // Class(M)->NowIs(Constant(V)) never
    for (MapIterator mt = T.maps.begin(); mt != T.maps.end(); ++mt) {
      for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
        Handle<i::Map> map = *mt;
        Handle<i::Object> value = *vt;
        CHECK(!T.Class(map)->NowIs(T.Constant(value)));
      }
    }
  }

  void Contains() {
    // T->Contains(V) iff Constant(V)->Is(T)
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
        TypeHandle type = *it;
        Handle<i::Object> value = *vt;
        CHECK(type->Contains(value) == T.Constant(value)->Is(type));
      }
    }

    // Of(V)->Is(T) implies T->Contains(V)
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
        TypeHandle type = *it;
        Handle<i::Object> value = *vt;
        CHECK(!T.Of(value)->Is(type) || type->Contains(value));
      }
    }
  }

  void NowContains() {
    // T->NowContains(V) iff Constant(V)->NowIs(T)
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
        TypeHandle type = *it;
        Handle<i::Object> value = *vt;
        CHECK(type->NowContains(value) == T.Constant(value)->NowIs(type));
      }
    }

    // T->Contains(V) implies T->NowContains(V)
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
        TypeHandle type = *it;
        Handle<i::Object> value = *vt;
        CHECK(!type->Contains(value) || type->NowContains(value));
      }
    }

    // NowOf(V)->Is(T) implies T->NowContains(V)
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
        TypeHandle type = *it;
        Handle<i::Object> value = *vt;
        CHECK(!T.NowOf(value)->NowIs(type) || type->NowContains(value));
      }
    }

    // NowOf(V)->NowIs(T) implies T->NowContains(V)
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
        TypeHandle type = *it;
        Handle<i::Object> value = *vt;
        CHECK(!T.NowOf(value)->NowIs(type) || type->NowContains(value));
      }
    }
  }

  void Maybe() {
    // T->Maybe(Any) iff T inhabited
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      CHECK(type->Maybe(T.Any) == type->IsInhabited());
    }

    // T->Maybe(None) never
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      CHECK(!type->Maybe(T.None));
    }

    // Reflexivity upto Inhabitation: T->Maybe(T) iff T inhabited
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      CHECK(type->Maybe(type) == type->IsInhabited());
    }

    // Symmetry: T1->Maybe(T2) iff T2->Maybe(T1)
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        TypeHandle type1 = *it1;
        TypeHandle type2 = *it2;
        CHECK(type1->Maybe(type2) == type2->Maybe(type1));
      }
    }

    // T1->Maybe(T2) implies T1, T2 inhabited
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        TypeHandle type1 = *it1;
        TypeHandle type2 = *it2;
        CHECK(!type1->Maybe(type2) ||
              (type1->IsInhabited() && type2->IsInhabited()));
      }
    }

    // T1->Maybe(T2) iff Intersect(T1, T2) inhabited
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        TypeHandle type1 = *it1;
        TypeHandle type2 = *it2;
        CHECK(type1->Maybe(type2) == T.Intersect(type1, type2)->IsInhabited());
      }
    }

    // T1->Is(T2) and T1 inhabited implies T1->Maybe(T2)
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        TypeHandle type1 = *it1;
        TypeHandle type2 = *it2;
        CHECK(!(type1->Is(type2) && type1->IsInhabited()) ||
              type1->Maybe(type2));
      }
    }

    // Constant(V1)->Maybe(Constant(V2)) iff V1 = V2
    for (ValueIterator vt1 = T.values.begin(); vt1 != T.values.end(); ++vt1) {
      for (ValueIterator vt2 = T.values.begin(); vt2 != T.values.end(); ++vt2) {
        Handle<i::Object> val1 = *vt1;
        Handle<i::Object> val2 = *vt2;
        CHECK(T.Constant(val1)->Maybe(T.Constant(val2)) == (*val1 == *val2));
      }
    }

    // Class(M1)->Maybe(Class(M2)) iff M1 = M2
    for (MapIterator mt1 = T.maps.begin(); mt1 != T.maps.end(); ++mt1) {
      for (MapIterator mt2 = T.maps.begin(); mt2 != T.maps.end(); ++mt2) {
        Handle<i::Map> map1 = *mt1;
        Handle<i::Map> map2 = *mt2;
        CHECK(T.Class(map1)->Maybe(T.Class(map2)) == (*map1 == *map2));
      }
    }

    // Constant(V)->Maybe(Class(M)) never
    for (MapIterator mt = T.maps.begin(); mt != T.maps.end(); ++mt) {
      for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
        Handle<i::Map> map = *mt;
        Handle<i::Object> value = *vt;
        CHECK(!T.Constant(value)->Maybe(T.Class(map)));
      }
    }

    // Class(M)->Maybe(Constant(V)) never
    for (MapIterator mt = T.maps.begin(); mt != T.maps.end(); ++mt) {
      for (ValueIterator vt = T.values.begin(); vt != T.values.end(); ++vt) {
        Handle<i::Map> map = *mt;
        Handle<i::Object> value = *vt;
        CHECK(!T.Class(map)->Maybe(T.Constant(value)));
      }
    }

    // Basic types
    CheckDisjoint(T.Boolean, T.Null, T.Semantic);
    CheckDisjoint(T.Undefined, T.Null, T.Semantic);
    CheckDisjoint(T.Boolean, T.Undefined, T.Semantic);

    CheckOverlap(T.SignedSmall, T.Number, T.Semantic);
    CheckOverlap(T.Float, T.Number, T.Semantic);
    CheckDisjoint(T.Signed32, T.Float, T.Semantic);

    CheckOverlap(T.UniqueName, T.Name, T.Semantic);
    CheckOverlap(T.String, T.Name, T.Semantic);
    CheckOverlap(T.InternalizedString, T.String, T.Semantic);
    CheckOverlap(T.InternalizedString, T.UniqueName, T.Semantic);
    CheckOverlap(T.InternalizedString, T.Name, T.Semantic);
    CheckOverlap(T.Symbol, T.UniqueName, T.Semantic);
    CheckOverlap(T.Symbol, T.Name, T.Semantic);
    CheckOverlap(T.String, T.UniqueName, T.Semantic);
    CheckDisjoint(T.String, T.Symbol, T.Semantic);
    CheckDisjoint(T.InternalizedString, T.Symbol, T.Semantic);

    CheckOverlap(T.Object, T.Receiver, T.Semantic);
    CheckOverlap(T.Array, T.Object, T.Semantic);
    CheckOverlap(T.Function, T.Object, T.Semantic);
    CheckOverlap(T.Proxy, T.Receiver, T.Semantic);
    CheckDisjoint(T.Object, T.Proxy, T.Semantic);
    CheckDisjoint(T.Array, T.Function, T.Semantic);

    // Structural types
    CheckOverlap(T.ObjectClass, T.Object, T.Semantic);
    CheckOverlap(T.ArrayClass, T.Object, T.Semantic);
    CheckOverlap(T.ObjectClass, T.ObjectClass, T.Semantic);
    CheckOverlap(T.ArrayClass, T.ArrayClass, T.Semantic);
    CheckDisjoint(T.ObjectClass, T.ArrayClass, T.Semantic);

    CheckOverlap(T.SmiConstant, T.SignedSmall, T.Semantic);
    CheckOverlap(T.SmiConstant, T.Signed32, T.Semantic);
    CheckOverlap(T.SmiConstant, T.Number, T.Semantic);
    CheckDisjoint(T.SmiConstant, T.Float, T.Semantic);
    CheckOverlap(T.ObjectConstant1, T.Object, T.Semantic);
    CheckOverlap(T.ObjectConstant2, T.Object, T.Semantic);
    CheckOverlap(T.ArrayConstant, T.Object, T.Semantic);
    CheckOverlap(T.ArrayConstant, T.Array, T.Semantic);
    CheckOverlap(T.ObjectConstant1, T.ObjectConstant1, T.Semantic);
    CheckDisjoint(T.ObjectConstant1, T.ObjectConstant2, T.Semantic);
    CheckDisjoint(T.ObjectConstant1, T.ArrayConstant, T.Semantic);

    CheckDisjoint(T.ObjectConstant1, T.ObjectClass, T.Semantic);
    CheckDisjoint(T.ObjectConstant2, T.ObjectClass, T.Semantic);
    CheckDisjoint(T.ObjectConstant1, T.ArrayClass, T.Semantic);
    CheckDisjoint(T.ObjectConstant2, T.ArrayClass, T.Semantic);
    CheckDisjoint(T.ArrayConstant, T.ObjectClass, T.Semantic);
  }

  void Union() {
    // Identity: Union(T, None) = T
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      CheckEqual(T.Union(type, T.None), type);
    }

    // Domination: Union(T, Any) = Any
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      CheckEqual(T.Union(type, T.Any), T.Any);
    }

    // Idempotence: Union(T, T) = T
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      CheckEqual(T.Union(type, type), type);
    }

    // Commutativity: Union(T1, T2) = Union(T2, T1)
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        TypeHandle type1 = *it1;
        TypeHandle type2 = *it2;
        CheckEqual(T.Union(type1, type2), T.Union(type2, type1));
      }
    }

    // Associativity: Union(T1, Union(T2, T3)) = Union(Union(T1, T2), T3)
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        for (TypeIterator it3 = T.types.begin(); it3 != T.types.end(); ++it3) {
          TypeHandle type1 = *it1;
          TypeHandle type2 = *it2;
          TypeHandle type3 = *it3;
          CheckEqual(
              T.Union(type1, T.Union(type2, type3)),
              T.Union(T.Union(type1, type2), type3));
        }
      }
    }

    // Meet: T1->Is(Union(T1, T2)) and T2->Is(Union(T1, T2))
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        TypeHandle type1 = *it1;
        TypeHandle type2 = *it2;
        CHECK(type1->Is(T.Union(type1, type2)));
        CHECK(type2->Is(T.Union(type1, type2)));
      }
    }

    // Upper Boundedness: T1->Is(T2) implies Union(T1, T2) = T2
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        TypeHandle type1 = *it1;
        TypeHandle type2 = *it2;
        if (type1->Is(type2)) CheckEqual(T.Union(type1, type2), type2);
      }
    }

    // Monotonicity: T1->Is(T2) implies Union(T1, T3)->Is(Union(T2, T3))
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        for (TypeIterator it3 = T.types.begin(); it3 != T.types.end(); ++it3) {
          TypeHandle type1 = *it1;
          TypeHandle type2 = *it2;
          TypeHandle type3 = *it3;
          CHECK(!type1->Is(type2) ||
                (T.Union(type1, type3)->Is(T.Union(type2, type3))));
        }
      }
    }

    // Monotonicity: T1->Is(T3) and T2->Is(T3) implies Union(T1, T2)->Is(T3)
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        for (TypeIterator it3 = T.types.begin(); it3 != T.types.end(); ++it3) {
          TypeHandle type1 = *it1;
          TypeHandle type2 = *it2;
          TypeHandle type3 = *it3;
          CHECK(!(type1->Is(type3) && type2->Is(type3)) ||
                T.Union(type1, type2)->Is(type3));
        }
      }
    }

    // Monotonicity: T1->Is(T2) or T1->Is(T3) implies T1->Is(Union(T2, T3))
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        for (TypeIterator it3 = T.types.begin(); it3 != T.types.end(); ++it3) {
          TypeHandle type1 = *it1;
          TypeHandle type2 = *it2;
          TypeHandle type3 = *it3;
          CHECK(!(type1->Is(type2) || type1->Is(type3)) ||
                type1->Is(T.Union(type2, type3)));
        }
      }
    }

    // Class-class
    CheckSub(T.Union(T.ObjectClass, T.ArrayClass), T.Object);
    CheckUnordered(T.Union(T.ObjectClass, T.ArrayClass), T.Array);
    CheckOverlap(T.Union(T.ObjectClass, T.ArrayClass), T.Array, T.Semantic);
    CheckDisjoint(T.Union(T.ObjectClass, T.ArrayClass), T.Number, T.Semantic);

    // Constant-constant
    CheckSub(T.Union(T.ObjectConstant1, T.ObjectConstant2), T.Object);
    CheckUnordered(T.Union(T.ObjectConstant1, T.ArrayConstant), T.Array);
    CheckUnordered(
        T.Union(T.ObjectConstant1, T.ObjectConstant2), T.ObjectClass);
    CheckOverlap(
        T.Union(T.ObjectConstant1, T.ArrayConstant), T.Array, T.Semantic);
    CheckDisjoint(
        T.Union(T.ObjectConstant1, T.ArrayConstant), T.Number, T.Semantic);
    CheckDisjoint(
        T.Union(T.ObjectConstant1, T.ArrayConstant), T.ObjectClass, T.Semantic);

    // Bitset-class
    CheckSub(
        T.Union(T.ObjectClass, T.SignedSmall), T.Union(T.Object, T.Number));
    CheckSub(T.Union(T.ObjectClass, T.Array), T.Object);
    CheckUnordered(T.Union(T.ObjectClass, T.String), T.Array);
    CheckOverlap(T.Union(T.ObjectClass, T.String), T.Object, T.Semantic);
    CheckDisjoint(T.Union(T.ObjectClass, T.String), T.Number, T.Semantic);

    // Bitset-constant
    CheckSub(
        T.Union(T.ObjectConstant1, T.Signed32), T.Union(T.Object, T.Number));
    CheckSub(T.Union(T.ObjectConstant1, T.Array), T.Object);
    CheckUnordered(T.Union(T.ObjectConstant1, T.String), T.Array);
    CheckOverlap(T.Union(T.ObjectConstant1, T.String), T.Object, T.Semantic);
    CheckDisjoint(T.Union(T.ObjectConstant1, T.String), T.Number, T.Semantic);

    // Class-constant
    CheckSub(T.Union(T.ObjectConstant1, T.ArrayClass), T.Object);
    CheckUnordered(T.ObjectClass, T.Union(T.ObjectConstant1, T.ArrayClass));
    CheckSub(
        T.Union(T.ObjectConstant1, T.ArrayClass), T.Union(T.Array, T.Object));
    CheckUnordered(T.Union(T.ObjectConstant1, T.ArrayClass), T.ArrayConstant);
    CheckDisjoint(
        T.Union(T.ObjectConstant1, T.ArrayClass), T.ObjectConstant2,
        T.Semantic);
    CheckDisjoint(
        T.Union(T.ObjectConstant1, T.ArrayClass), T.ObjectClass, T.Semantic);

    // Bitset-union
    CheckSub(
        T.Float,
        T.Union(T.Union(T.ArrayClass, T.ObjectConstant1), T.Number));
    CheckSub(
        T.Union(T.Union(T.ArrayClass, T.ObjectConstant1), T.Float),
        T.Union(T.ObjectConstant1, T.Union(T.Number, T.ArrayClass)));

    // Class-union
    CheckSub(
        T.Union(T.ObjectClass, T.Union(T.ObjectConstant1, T.ObjectClass)),
        T.Object);
    CheckEqual(
        T.Union(T.Union(T.ArrayClass, T.ObjectConstant2), T.ArrayClass),
        T.Union(T.ArrayClass, T.ObjectConstant2));

    // Constant-union
    CheckEqual(
        T.Union(
            T.ObjectConstant1, T.Union(T.ObjectConstant1, T.ObjectConstant2)),
        T.Union(T.ObjectConstant2, T.ObjectConstant1));
    CheckEqual(
        T.Union(
            T.Union(T.ArrayConstant, T.ObjectConstant2), T.ObjectConstant1),
        T.Union(
            T.ObjectConstant2, T.Union(T.ArrayConstant, T.ObjectConstant1)));

    // Union-union
    CheckEqual(
        T.Union(
            T.Union(T.ObjectConstant2, T.ObjectConstant1),
            T.Union(T.ObjectConstant1, T.ObjectConstant2)),
        T.Union(T.ObjectConstant2, T.ObjectConstant1));
    CheckEqual(
        T.Union(
            T.Union(T.Number, T.ArrayClass),
            T.Union(T.SignedSmall, T.Array)),
        T.Union(T.Number, T.Array));
  }

  void Intersect() {
    // Identity: Intersect(T, Any) = T
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      CheckEqual(T.Intersect(type, T.Any), type);
    }

    // Domination: Intersect(T, None) = None
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      CheckEqual(T.Intersect(type, T.None), T.None);
    }

    // Idempotence: Intersect(T, T) = T
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      CheckEqual(T.Intersect(type, type), type);
    }

    // Commutativity: Intersect(T1, T2) = Intersect(T2, T1)
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        TypeHandle type1 = *it1;
        TypeHandle type2 = *it2;
        CheckEqual(T.Intersect(type1, type2), T.Intersect(type2, type1));
      }
    }

    // Associativity:
    // Intersect(T1, Intersect(T2, T3)) = Intersect(Intersect(T1, T2), T3)
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        for (TypeIterator it3 = T.types.begin(); it3 != T.types.end(); ++it3) {
          TypeHandle type1 = *it1;
          TypeHandle type2 = *it2;
          TypeHandle type3 = *it3;
          CheckEqual(
              T.Intersect(type1, T.Intersect(type2, type3)),
              T.Intersect(T.Intersect(type1, type2), type3));
        }
      }
    }

    // Join: Intersect(T1, T2)->Is(T1) and Intersect(T1, T2)->Is(T2)
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        TypeHandle type1 = *it1;
        TypeHandle type2 = *it2;
        CHECK(T.Intersect(type1, type2)->Is(type1));
        CHECK(T.Intersect(type1, type2)->Is(type2));
      }
    }

    // Lower Boundedness: T1->Is(T2) implies Intersect(T1, T2) = T1
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        TypeHandle type1 = *it1;
        TypeHandle type2 = *it2;
        if (type1->Is(type2)) CheckEqual(T.Intersect(type1, type2), type1);
      }
    }

    // Monotonicity: T1->Is(T2) implies Intersect(T1, T3)->Is(Intersect(T2, T3))
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        for (TypeIterator it3 = T.types.begin(); it3 != T.types.end(); ++it3) {
          TypeHandle type1 = *it1;
          TypeHandle type2 = *it2;
          TypeHandle type3 = *it3;
          CHECK(!type1->Is(type2) ||
                (T.Intersect(type1, type3)->Is(T.Intersect(type2, type3))));
        }
      }
    }

    // Monotonicity: T1->Is(T2) implies Intersect(T1, T3)->Is(Intersect(T2, T3))
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        for (TypeIterator it3 = T.types.begin(); it3 != T.types.end(); ++it3) {
          TypeHandle type1 = *it1;
          TypeHandle type2 = *it2;
          TypeHandle type3 = *it3;
          CHECK(!type1->Is(type2) ||
                (T.Intersect(type1, type3)->Is(T.Intersect(type2, type3))));
        }
      }
    }

    // Monotonicity: T1->Is(T3) or T2->Is(T3) implies Intersect(T1, T2)->Is(T3)
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        for (TypeIterator it3 = T.types.begin(); it3 != T.types.end(); ++it3) {
          TypeHandle type1 = *it1;
          TypeHandle type2 = *it2;
          TypeHandle type3 = *it3;
          CHECK(!(type1->Is(type3) || type2->Is(type3)) ||
                T.Intersect(type1, type2)->Is(type3));
        }
      }
    }

    // Monotonicity: T1->Is(T2) and T1->Is(T3) implies T1->Is(Intersect(T2, T3))
    for (TypeIterator it1 = T.types.begin(); it1 != T.types.end(); ++it1) {
      for (TypeIterator it2 = T.types.begin(); it2 != T.types.end(); ++it2) {
        for (TypeIterator it3 = T.types.begin(); it3 != T.types.end(); ++it3) {
          TypeHandle type1 = *it1;
          TypeHandle type2 = *it2;
          TypeHandle type3 = *it3;
          CHECK(!(type1->Is(type2) && type1->Is(type3)) ||
                type1->Is(T.Intersect(type2, type3)));
        }
      }
    }

    // Bitset-class
    CheckEqual(T.Intersect(T.ObjectClass, T.Object), T.ObjectClass);
    CheckSub(T.Intersect(T.ObjectClass, T.Array), T.Representation);
    CheckSub(T.Intersect(T.ObjectClass, T.Number), T.Representation);

    // Class-constant
    CheckEqual(T.Intersect(T.ObjectConstant1, T.ObjectClass), T.None);
    CheckEqual(T.Intersect(T.ArrayClass, T.ObjectConstant2), T.None);

    // Bitset-union
    CheckEqual(
        T.Intersect(T.Object, T.Union(T.ObjectConstant1, T.ObjectClass)),
        T.Union(T.ObjectConstant1, T.ObjectClass));
    CheckEqual(
        T.Intersect(T.Union(T.ArrayClass, T.ObjectConstant1), T.Number),
        T.None);

    // Class-union
    CheckEqual(
        T.Intersect(T.ArrayClass, T.Union(T.ObjectConstant2, T.ArrayClass)),
        T.ArrayClass);
    CheckEqual(
        T.Intersect(T.ArrayClass, T.Union(T.Object, T.SmiConstant)),
        T.ArrayClass);
    CheckEqual(
        T.Intersect(T.Union(T.ObjectClass, T.ArrayConstant), T.ArrayClass),
        T.None);

    // Constant-union
    CheckEqual(
        T.Intersect(
            T.ObjectConstant1, T.Union(T.ObjectConstant1, T.ObjectConstant2)),
        T.ObjectConstant1);
    CheckEqual(
        T.Intersect(T.SmiConstant, T.Union(T.Number, T.ObjectConstant2)),
        T.SmiConstant);
    CheckEqual(
        T.Intersect(
            T.Union(T.ArrayConstant, T.ObjectClass), T.ObjectConstant1),
        T.None);

    // Union-union
    CheckEqual(
        T.Intersect(
            T.Union(T.Number, T.ArrayClass),
            T.Union(T.SignedSmall, T.Array)),
        T.Union(T.SignedSmall, T.ArrayClass));
    CheckEqual(
        T.Intersect(
            T.Union(T.Number, T.ObjectClass),
            T.Union(T.Signed32, T.Array)),
        T.Signed32);
    CheckEqual(
        T.Intersect(
            T.Union(T.ObjectConstant2, T.ObjectConstant1),
            T.Union(T.ObjectConstant1, T.ObjectConstant2)),
        T.Union(T.ObjectConstant2, T.ObjectConstant1));
    CheckEqual(
        T.Intersect(
            T.Union(
                T.Union(T.ObjectConstant2, T.ObjectConstant1), T.ArrayClass),
            T.Union(
                T.ObjectConstant1,
                T.Union(T.ArrayConstant, T.ObjectConstant2))),
        T.Union(T.ObjectConstant2, T.ObjectConstant1));
  }

  template<class Type2, class TypeHandle2, class Region2, class Rep2>
  void Convert() {
    Types<Type2, TypeHandle2, Region2> T2(
        Rep2::ToRegion(&zone, isolate), isolate);
    for (TypeIterator it = T.types.begin(); it != T.types.end(); ++it) {
      TypeHandle type = *it;
      CheckEqual(type,
                 T.template Convert<Type2>(T2.template Convert<Type>(type)));
    }
  }
};

typedef Tests<Type, Type*, Zone, ZoneRep> ZoneTests;
typedef Tests<HeapType, Handle<HeapType>, Isolate, HeapRep> HeapTests;


TEST(Bitset) {
  CcTest::InitializeVM();
  ZoneTests().Bitset();
  HeapTests().Bitset();
}


TEST(Class) {
  CcTest::InitializeVM();
  ZoneTests().Class();
  HeapTests().Class();
}


TEST(Constant) {
  CcTest::InitializeVM();
  ZoneTests().Constant();
  HeapTests().Constant();
}


TEST(Of) {
  CcTest::InitializeVM();
  ZoneTests().Of();
  HeapTests().Of();
}


TEST(NowOf) {
  CcTest::InitializeVM();
  ZoneTests().NowOf();
  HeapTests().NowOf();
}


TEST(Is) {
  CcTest::InitializeVM();
  ZoneTests().Is();
  HeapTests().Is();
}


TEST(NowIs) {
  CcTest::InitializeVM();
  ZoneTests().NowIs();
  HeapTests().NowIs();
}


TEST(Contains) {
  CcTest::InitializeVM();
  ZoneTests().Contains();
  HeapTests().Contains();
}


TEST(NowContains) {
  CcTest::InitializeVM();
  ZoneTests().NowContains();
  HeapTests().NowContains();
}


TEST(Maybe) {
  CcTest::InitializeVM();
  ZoneTests().Maybe();
  HeapTests().Maybe();
}


TEST(Union) {
  CcTest::InitializeVM();
  ZoneTests().Union();
  HeapTests().Union();
}


TEST(Intersect) {
  CcTest::InitializeVM();
  ZoneTests().Intersect();
  HeapTests().Intersect();
}


TEST(Convert) {
  CcTest::InitializeVM();
  ZoneTests().Convert<HeapType, Handle<HeapType>, Isolate, HeapRep>();
  HeapTests().Convert<Type, Type*, Zone, ZoneRep>();
}
