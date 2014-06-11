// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOOKUP_H_
#define V8_LOOKUP_H_

#include "src/factory.h"
#include "src/isolate.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

class LookupIterator V8_FINAL BASE_EMBEDDED {
 public:
  enum Type {
    CHECK_DERIVED      = 1 << 0,
    CHECK_INTERCEPTOR  = 1 << 1,
    CHECK_ACCESS_CHECK = 1 << 2,
    CHECK_OWN_REAL     = 0,
    CHECK_ALL          = CHECK_DERIVED | CHECK_INTERCEPTOR | CHECK_ACCESS_CHECK,
    SKIP_INTERCEPTOR   = CHECK_ALL ^ CHECK_INTERCEPTOR
  };

  enum State {
    NOT_FOUND,
    PROPERTY,
    INTERCEPTOR,
    ACCESS_CHECK,
    JSPROXY
  };

  enum PropertyType {
    DATA,
    ACCESSORS
  };

  enum PropertyEncoding {
    DICTIONARY,
    DESCRIPTOR
  };

  LookupIterator(Handle<Object> receiver,
                 Handle<Name> name,
                 Type type = CHECK_ALL)
      : type_(type),
        state_(NOT_FOUND),
        property_type_(DATA),
        property_encoding_(DESCRIPTOR),
        property_details_(NONE, NONEXISTENT, Representation::None()),
        isolate_(name->GetIsolate()),
        name_(name),
        maybe_receiver_(receiver),
        number_(DescriptorArray::kNotFound) {
    Handle<JSReceiver> origin = GetOrigin();
    holder_map_ = handle(origin->map());
    maybe_holder_ = origin;
    Next();
  }

  LookupIterator(Handle<Object> receiver,
                 Handle<Name> name,
                 Handle<JSReceiver> holder,
                 Type type = CHECK_ALL)
      : type_(type),
        state_(NOT_FOUND),
        property_type_(DATA),
        property_encoding_(DESCRIPTOR),
        property_details_(NONE, NONEXISTENT, Representation::None()),
        isolate_(name->GetIsolate()),
        name_(name),
        holder_map_(holder->map()),
        maybe_receiver_(receiver),
        maybe_holder_(holder),
        number_(DescriptorArray::kNotFound) {
    Next();
  }

  Isolate* isolate() const { return isolate_; }
  State state() const { return state_; }
  Handle<Name> name() const { return name_; }

  bool IsFound() const { return state_ != NOT_FOUND; }
  void Next();

  Heap* heap() const { return isolate_->heap(); }
  Factory* factory() const { return isolate_->factory(); }
  Handle<Object> GetReceiver() const {
    return Handle<Object>::cast(maybe_receiver_.ToHandleChecked());
  }
  Handle<JSObject> GetHolder() const {
    ASSERT(IsFound() && state_ != JSPROXY);
    return Handle<JSObject>::cast(maybe_holder_.ToHandleChecked());
  }
  Handle<JSReceiver> GetOrigin() const;

  /* ACCESS_CHECK */
  bool HasAccess(v8::AccessType access_type) const;

  /* PROPERTY */
  // HasProperty needs to be called before any of the other PROPERTY methods
  // below can be used. It ensures that we are able to provide a definite
  // answer, and loads extra information about the property.
  bool HasProperty();
  PropertyType property_type() const {
    ASSERT(has_property_);
    return property_type_;
  }
  PropertyDetails property_details() const {
    ASSERT(has_property_);
    return property_details_;
  }
  Handle<Object> GetAccessors() const;
  Handle<Object> GetDataValue() const;

  /* JSPROXY */

  Handle<JSProxy> GetJSProxy() const {
    return Handle<JSProxy>::cast(maybe_holder_.ToHandleChecked());
  }

 private:
  Handle<Map> GetReceiverMap() const;

  MUST_USE_RESULT bool NextHolder();
  void LookupInHolder();
  Handle<Object> FetchValue() const;

  bool IsBootstrapping() const;

  // Methods that fetch data from the holder ensure they always have a holder.
  // This means the receiver needs to be present as opposed to just the receiver
  // map. Other objects in the prototype chain are transitively guaranteed to be
  // present via the receiver map.
  bool is_guaranteed_to_have_holder() const {
    return !maybe_receiver_.is_null();
  }
  bool check_interceptor() const {
    return !IsBootstrapping() && (type_ & CHECK_INTERCEPTOR) != 0;
  }
  bool check_derived() const {
    return (type_ & CHECK_DERIVED) != 0;
  }
  bool check_access_check() const {
    return (type_ & CHECK_ACCESS_CHECK) != 0;
  }

  Type type_;
  State state_;
  bool has_property_;
  PropertyType property_type_;
  PropertyEncoding property_encoding_;
  PropertyDetails property_details_;
  Isolate* isolate_;
  Handle<Name> name_;
  Handle<Map> holder_map_;
  MaybeHandle<Object> maybe_receiver_;
  MaybeHandle<JSReceiver> maybe_holder_;

  int number_;
};


} }  // namespace v8::internal

#endif  // V8_LOOKUP_H_
