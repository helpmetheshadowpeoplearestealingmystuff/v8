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

#ifndef V8_JSREGEXP_H_
#define V8_JSREGEXP_H_

namespace v8 { namespace internal {


class RegExpMacroAssembler;


class RegExpImpl {
 public:
  static inline bool UseNativeRegexp() {
#ifdef ARM
    return false;
#else
    return FLAG_regexp_native;
#endif
  }
  // Creates a regular expression literal in the old space.
  // This function calls the garbage collector if necessary.
  static Handle<Object> CreateRegExpLiteral(Handle<JSFunction> constructor,
                                            Handle<String> pattern,
                                            Handle<String> flags,
                                            bool* has_pending_exception);

  // Returns a string representation of a regular expression.
  // Implements RegExp.prototype.toString, see ECMA-262 section 15.10.6.4.
  // This function calls the garbage collector if necessary.
  static Handle<String> ToString(Handle<Object> value);

  // Parses the RegExp pattern and prepares the JSRegExp object with
  // generic data and choice of implementation - as well as what
  // the implementation wants to store in the data field.
  // Returns false if compilation fails.
  static Handle<Object> Compile(Handle<JSRegExp> re,
                                Handle<String> pattern,
                                Handle<String> flags);

  // See ECMA-262 section 15.10.6.2.
  // This function calls the garbage collector if necessary.
  static Handle<Object> Exec(Handle<JSRegExp> regexp,
                             Handle<String> subject,
                             int index,
                             Handle<JSArray> lastMatchInfo);

  // Call RegExp.prototyp.exec(string) in a loop.
  // Used by String.prototype.match and String.prototype.replace.
  // This function calls the garbage collector if necessary.
  static Handle<Object> ExecGlobal(Handle<JSRegExp> regexp,
                                   Handle<String> subject,
                                   Handle<JSArray> lastMatchInfo);

  // Prepares a JSRegExp object with Irregexp-specific data.
  static void IrregexpPrepare(Handle<JSRegExp> re,
                              Handle<String> pattern,
                              JSRegExp::Flags flags,
                              int capture_register_count);


  static void AtomCompile(Handle<JSRegExp> re,
                          Handle<String> pattern,
                          JSRegExp::Flags flags,
                          Handle<String> match_pattern);

  static Handle<Object> AtomExec(Handle<JSRegExp> regexp,
                                 Handle<String> subject,
                                 int index,
                                 Handle<JSArray> lastMatchInfo);

  // Execute an Irregexp bytecode pattern.
  // On a successful match, the result is a JSArray containing
  // captured positions. On a failure, the result is the null value.
  // Returns an empty handle in case of an exception.
  static Handle<Object> IrregexpExec(Handle<JSRegExp> regexp,
                                     Handle<String> subject,
                                     int index,
                                     Handle<JSArray> lastMatchInfo);

  // Offsets in the lastMatchInfo array.
  static const int kLastCaptureCount = 0;
  static const int kLastSubject = 1;
  static const int kLastInput = 2;
  static const int kFirstCapture = 3;
  static const int kLastMatchOverhead = 3;

  // Used to access the lastMatchInfo array.
  static int GetCapture(FixedArray* array, int index) {
    return Smi::cast(array->get(index + kFirstCapture))->value();
  }

  static void SetLastCaptureCount(FixedArray* array, int to) {
    array->set(kLastCaptureCount, Smi::FromInt(to));
  }

  static void SetLastSubject(FixedArray* array, String* to) {
    array->set(kLastSubject, to);
  }

  static void SetLastInput(FixedArray* array, String* to) {
    array->set(kLastInput, to);
  }

  static void SetCapture(FixedArray* array, int index, int to) {
    array->set(index + kFirstCapture, Smi::FromInt(to));
  }

  static int GetLastCaptureCount(FixedArray* array) {
    return Smi::cast(array->get(kLastCaptureCount))->value();
  }

  // For acting on the JSRegExp data FixedArray.
  static int IrregexpMaxRegisterCount(FixedArray* re);
  static void SetIrregexpMaxRegisterCount(FixedArray* re, int value);
  static int IrregexpNumberOfCaptures(FixedArray* re);
  static int IrregexpNumberOfRegisters(FixedArray* re);
  static ByteArray* IrregexpByteCode(FixedArray* re, bool is_ascii);
  static Code* IrregexpNativeCode(FixedArray* re, bool is_ascii);

 private:
  static String* last_ascii_string_;
  static String* two_byte_cached_string_;

  static bool EnsureCompiledIrregexp(Handle<JSRegExp> re, bool is_ascii);


  // Set the subject cache.  The previous string buffer is not deleted, so the
  // caller should ensure that it doesn't leak.
  static void SetSubjectCache(String* subject,
                              char* utf8_subject,
                              int uft8_length,
                              int character_position,
                              int utf8_position);

  // A one element cache of the last utf8_subject string and its length.  The
  // subject JS String object is cached in the heap.  We also cache a
  // translation between position and utf8 position.
  static char* utf8_subject_cache_;
  static int utf8_length_cache_;
  static int utf8_position_;
  static int character_position_;
};


class CharacterRange {
 public:
  CharacterRange() : from_(0), to_(0) { }
  // For compatibility with the CHECK_OK macro
  CharacterRange(void* null) { ASSERT_EQ(NULL, null); }  //NOLINT
  CharacterRange(uc16 from, uc16 to) : from_(from), to_(to) { }
  static void AddClassEscape(uc16 type, ZoneList<CharacterRange>* ranges);
  static Vector<const uc16> GetWordBounds();
  static inline CharacterRange Singleton(uc16 value) {
    return CharacterRange(value, value);
  }
  static inline CharacterRange Range(uc16 from, uc16 to) {
    ASSERT(from <= to);
    return CharacterRange(from, to);
  }
  static inline CharacterRange Everything() {
    return CharacterRange(0, 0xFFFF);
  }
  bool Contains(uc16 i) { return from_ <= i && i <= to_; }
  uc16 from() const { return from_; }
  void set_from(uc16 value) { from_ = value; }
  uc16 to() const { return to_; }
  void set_to(uc16 value) { to_ = value; }
  bool is_valid() { return from_ <= to_; }
  bool IsEverything(uc16 max) { return from_ == 0 && to_ >= max; }
  bool IsSingleton() { return (from_ == to_); }
  void AddCaseEquivalents(ZoneList<CharacterRange>* ranges);
  static void Split(ZoneList<CharacterRange>* base,
                    Vector<const uc16> overlay,
                    ZoneList<CharacterRange>** included,
                    ZoneList<CharacterRange>** excluded);

  static const int kRangeCanonicalizeMax = 0x346;
  static const int kStartMarker = (1 << 24);
  static const int kPayloadMask = (1 << 24) - 1;

 private:
  uc16 from_;
  uc16 to_;
};


template <typename Node, class Callback>
static void DoForEach(Node* node, Callback* callback);


// A zone splay tree.  The config type parameter encapsulates the
// different configurations of a concrete splay tree:
//
//   typedef Key: the key type
//   typedef Value: the value type
//   static const kNoKey: the dummy key used when no key is set
//   static const kNoValue: the dummy value used to initialize nodes
//   int (Compare)(Key& a, Key& b) -> {-1, 0, 1}: comparison function
//
template <typename Config>
class ZoneSplayTree : public ZoneObject {
 public:
  typedef typename Config::Key Key;
  typedef typename Config::Value Value;

  class Locator;

  ZoneSplayTree() : root_(NULL) { }

  // Inserts the given key in this tree with the given value.  Returns
  // true if a node was inserted, otherwise false.  If found the locator
  // is enabled and provides access to the mapping for the key.
  bool Insert(const Key& key, Locator* locator);

  // Looks up the key in this tree and returns true if it was found,
  // otherwise false.  If the node is found the locator is enabled and
  // provides access to the mapping for the key.
  bool Find(const Key& key, Locator* locator);

  // Finds the mapping with the greatest key less than or equal to the
  // given key.
  bool FindGreatestLessThan(const Key& key, Locator* locator);

  // Find the mapping with the greatest key in this tree.
  bool FindGreatest(Locator* locator);

  // Finds the mapping with the least key greater than or equal to the
  // given key.
  bool FindLeastGreaterThan(const Key& key, Locator* locator);

  // Find the mapping with the least key in this tree.
  bool FindLeast(Locator* locator);

  // Remove the node with the given key from the tree.
  bool Remove(const Key& key);

  bool is_empty() { return root_ == NULL; }

  // Perform the splay operation for the given key. Moves the node with
  // the given key to the top of the tree.  If no node has the given
  // key, the last node on the search path is moved to the top of the
  // tree.
  void Splay(const Key& key);

  class Node : public ZoneObject {
   public:
    Node(const Key& key, const Value& value)
        : key_(key),
          value_(value),
          left_(NULL),
          right_(NULL) { }
    Key key() { return key_; }
    Value value() { return value_; }
    Node* left() { return left_; }
    Node* right() { return right_; }
   private:
    friend class ZoneSplayTree;
    friend class Locator;
    Key key_;
    Value value_;
    Node* left_;
    Node* right_;
  };

  // A locator provides access to a node in the tree without actually
  // exposing the node.
  class Locator {
   public:
    explicit Locator(Node* node) : node_(node) { }
    Locator() : node_(NULL) { }
    const Key& key() { return node_->key_; }
    Value& value() { return node_->value_; }
    void set_value(const Value& value) { node_->value_ = value; }
    inline void bind(Node* node) { node_ = node; }
   private:
    Node* node_;
  };

  template <class Callback>
  void ForEach(Callback* c) {
    DoForEach<typename ZoneSplayTree<Config>::Node, Callback>(root_, c);
  }

 private:
  Node* root_;
};


// A set of unsigned integers that behaves especially well on small
// integers (< 32).  May do zone-allocation.
class OutSet: public ZoneObject {
 public:
  OutSet() : first_(0), remaining_(NULL), successors_(NULL) { }
  OutSet* Extend(unsigned value);
  bool Get(unsigned value);
  static const unsigned kFirstLimit = 32;

 private:
  // Destructively set a value in this set.  In most cases you want
  // to use Extend instead to ensure that only one instance exists
  // that contains the same values.
  void Set(unsigned value);

  // The successors are a list of sets that contain the same values
  // as this set and the one more value that is not present in this
  // set.
  ZoneList<OutSet*>* successors() { return successors_; }

  OutSet(uint32_t first, ZoneList<unsigned>* remaining)
      : first_(first), remaining_(remaining), successors_(NULL) { }
  uint32_t first_;
  ZoneList<unsigned>* remaining_;
  ZoneList<OutSet*>* successors_;
  friend class Trace;
};


// A mapping from integers, specified as ranges, to a set of integers.
// Used for mapping character ranges to choices.
class DispatchTable : public ZoneObject {
 public:
  class Entry {
   public:
    Entry() : from_(0), to_(0), out_set_(NULL) { }
    Entry(uc16 from, uc16 to, OutSet* out_set)
        : from_(from), to_(to), out_set_(out_set) { }
    uc16 from() { return from_; }
    uc16 to() { return to_; }
    void set_to(uc16 value) { to_ = value; }
    void AddValue(int value) { out_set_ = out_set_->Extend(value); }
    OutSet* out_set() { return out_set_; }
   private:
    uc16 from_;
    uc16 to_;
    OutSet* out_set_;
  };

  class Config {
   public:
    typedef uc16 Key;
    typedef Entry Value;
    static const uc16 kNoKey;
    static const Entry kNoValue;
    static inline int Compare(uc16 a, uc16 b) {
      if (a == b)
        return 0;
      else if (a < b)
        return -1;
      else
        return 1;
    }
  };

  void AddRange(CharacterRange range, int value);
  OutSet* Get(uc16 value);
  void Dump();

  template <typename Callback>
  void ForEach(Callback* callback) { return tree()->ForEach(callback); }
 private:
  // There can't be a static empty set since it allocates its
  // successors in a zone and caches them.
  OutSet* empty() { return &empty_; }
  OutSet empty_;
  ZoneSplayTree<Config>* tree() { return &tree_; }
  ZoneSplayTree<Config> tree_;
};


#define FOR_EACH_NODE_TYPE(VISIT)                                    \
  VISIT(End)                                                         \
  VISIT(Action)                                                      \
  VISIT(Choice)                                                      \
  VISIT(BackReference)                                               \
  VISIT(Assertion)                                                   \
  VISIT(Text)


#define FOR_EACH_REG_EXP_TREE_TYPE(VISIT)                            \
  VISIT(Disjunction)                                                 \
  VISIT(Alternative)                                                 \
  VISIT(Assertion)                                                   \
  VISIT(CharacterClass)                                              \
  VISIT(Atom)                                                        \
  VISIT(Quantifier)                                                  \
  VISIT(Capture)                                                     \
  VISIT(Lookahead)                                                   \
  VISIT(BackReference)                                               \
  VISIT(Empty)                                                       \
  VISIT(Text)


#define FORWARD_DECLARE(Name) class RegExp##Name;
FOR_EACH_REG_EXP_TREE_TYPE(FORWARD_DECLARE)
#undef FORWARD_DECLARE


class TextElement {
 public:
  enum Type {UNINITIALIZED, ATOM, CHAR_CLASS};
  TextElement() : type(UNINITIALIZED) { }
  explicit TextElement(Type t) : type(t), cp_offset(-1) { }
  static TextElement Atom(RegExpAtom* atom);
  static TextElement CharClass(RegExpCharacterClass* char_class);
  int length();
  Type type;
  union {
    RegExpAtom* u_atom;
    RegExpCharacterClass* u_char_class;
  } data;
  int cp_offset;
};


class Trace;


struct NodeInfo {
  NodeInfo()
      : being_analyzed(false),
        been_analyzed(false),
        follows_word_interest(false),
        follows_newline_interest(false),
        follows_start_interest(false),
        at_end(false),
        visited(false) { }

  // Returns true if the interests and assumptions of this node
  // matches the given one.
  bool Matches(NodeInfo* that) {
    return (at_end == that->at_end) &&
           (follows_word_interest == that->follows_word_interest) &&
           (follows_newline_interest == that->follows_newline_interest) &&
           (follows_start_interest == that->follows_start_interest);
  }

  // Updates the interests of this node given the interests of the
  // node preceding it.
  void AddFromPreceding(NodeInfo* that) {
    at_end |= that->at_end;
    follows_word_interest |= that->follows_word_interest;
    follows_newline_interest |= that->follows_newline_interest;
    follows_start_interest |= that->follows_start_interest;
  }

  bool HasLookbehind() {
    return follows_word_interest ||
           follows_newline_interest ||
           follows_start_interest;
  }

  // Sets the interests of this node to include the interests of the
  // following node.
  void AddFromFollowing(NodeInfo* that) {
    follows_word_interest |= that->follows_word_interest;
    follows_newline_interest |= that->follows_newline_interest;
    follows_start_interest |= that->follows_start_interest;
  }

  void ResetCompilationState() {
    being_analyzed = false;
    been_analyzed = false;
  }

  bool being_analyzed: 1;
  bool been_analyzed: 1;

  // These bits are set of this node has to know what the preceding
  // character was.
  bool follows_word_interest: 1;
  bool follows_newline_interest: 1;
  bool follows_start_interest: 1;

  bool at_end: 1;
  bool visited: 1;
};


class SiblingList {
 public:
  SiblingList() : list_(NULL) { }
  int length() {
    return list_ == NULL ? 0 : list_->length();
  }
  void Ensure(RegExpNode* parent) {
    if (list_ == NULL) {
      list_ = new ZoneList<RegExpNode*>(2);
      list_->Add(parent);
    }
  }
  void Add(RegExpNode* node) { list_->Add(node); }
  RegExpNode* Get(int index) { return list_->at(index); }
 private:
  ZoneList<RegExpNode*>* list_;
};


// Details of a quick mask-compare check that can look ahead in the
// input stream.
class QuickCheckDetails {
 public:
  QuickCheckDetails()
      : characters_(0),
        mask_(0),
        value_(0),
        cannot_match_(false) { }
  explicit QuickCheckDetails(int characters)
      : characters_(characters),
        mask_(0),
        value_(0),
        cannot_match_(false) { }
  bool Rationalize(bool ascii);
  // Merge in the information from another branch of an alternation.
  void Merge(QuickCheckDetails* other, int from_index);
  // Advance the current position by some amount.
  void Advance(int by, bool ascii);
  void Clear();
  bool cannot_match() { return cannot_match_; }
  void set_cannot_match() { cannot_match_ = true; }
  struct Position {
    Position() : mask(0), value(0), determines_perfectly(false) { }
    uc16 mask;
    uc16 value;
    bool determines_perfectly;
  };
  int characters() { return characters_; }
  void set_characters(int characters) { characters_ = characters; }
  Position* positions(int index) {
    ASSERT(index >= 0);
    ASSERT(index < characters_);
    return positions_ + index;
  }
  uint32_t mask() { return mask_; }
  uint32_t value() { return value_; }

 private:
  // How many characters do we have quick check information from.  This is
  // the same for all branches of a choice node.
  int characters_;
  Position positions_[4];
  // These values are the condensate of the above array after Rationalize().
  uint32_t mask_;
  uint32_t value_;
  // If set to true, there is no way this quick check can match at all.
  // E.g., if it requires to be at the start of the input, and isn't.
  bool cannot_match_;
};


class RegExpNode: public ZoneObject {
 public:
  RegExpNode() : trace_count_(0) { }
  virtual ~RegExpNode();
  virtual void Accept(NodeVisitor* visitor) = 0;
  // Generates a goto to this node or actually generates the code at this point.
  virtual void Emit(RegExpCompiler* compiler, Trace* trace) = 0;
  // How many characters must this node consume at a minimum in order to
  // succeed.  If we have found at least 'still_to_find' characters that
  // must be consumed there is no need to ask any following nodes whether
  // they are sure to eat any more characters.
  virtual int EatsAtLeast(int still_to_find, int recursion_depth) = 0;
  // Emits some quick code that checks whether the preloaded characters match.
  // Falls through on certain failure, jumps to the label on possible success.
  // If the node cannot make a quick check it does nothing and returns false.
  bool EmitQuickCheck(RegExpCompiler* compiler,
                      Trace* trace,
                      bool preload_has_checked_bounds,
                      Label* on_possible_success,
                      QuickCheckDetails* details_return,
                      bool fall_through_on_failure);
  // For a given number of characters this returns a mask and a value.  The
  // next n characters are anded with the mask and compared with the value.
  // A comparison failure indicates the node cannot match the next n characters.
  // A comparison success indicates the node may match.
  virtual void GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int characters_filled_in,
                                    bool not_at_start) = 0;
  static const int kNodeIsTooComplexForGreedyLoops = -1;
  virtual int GreedyLoopTextLength() { return kNodeIsTooComplexForGreedyLoops; }
  Label* label() { return &label_; }
  // If non-generic code is generated for a node (ie the node is not at the
  // start of the trace) then it cannot be reused.  This variable sets a limit
  // on how often we allow that to happen before we insist on starting a new
  // trace and generating generic code for a node that can be reused by flushing
  // the deferred actions in the current trace and generating a goto.
  static const int kMaxCopiesCodeGenerated = 10;

  NodeInfo* info() { return &info_; }

  void AddSibling(RegExpNode* node) { siblings_.Add(node); }

  // Static version of EnsureSibling that expresses the fact that the
  // result has the same type as the input.
  template <class C>
  static C* EnsureSibling(C* node, NodeInfo* info, bool* cloned) {
    return static_cast<C*>(node->EnsureSibling(info, cloned));
  }

  SiblingList* siblings() { return &siblings_; }
  void set_siblings(SiblingList* other) { siblings_ = *other; }

 protected:
  enum LimitResult { DONE, CONTINUE };
  LimitResult LimitVersions(RegExpCompiler* compiler, Trace* trace);

  // Returns a sibling of this node whose interests and assumptions
  // match the ones in the given node info.  If no sibling exists NULL
  // is returned.
  RegExpNode* TryGetSibling(NodeInfo* info);

  // Returns a sibling of this node whose interests match the ones in
  // the given node info.  The info must not contain any assertions.
  // If no node exists a new one will be created by cloning the current
  // node.  The result will always be an instance of the same concrete
  // class as this node.
  RegExpNode* EnsureSibling(NodeInfo* info, bool* cloned);

  // Returns a clone of this node initialized using the copy constructor
  // of its concrete class.  Note that the node may have to be pre-
  // processed before it is on a usable state.
  virtual RegExpNode* Clone() = 0;

 private:
  Label label_;
  NodeInfo info_;
  SiblingList siblings_;
  // This variable keeps track of how many times code has been generated for
  // this node (in different traces).  We don't keep track of where the
  // generated code is located unless the code is generated at the start of
  // a trace, in which case it is generic and can be reused by flushing the
  // deferred operations in the current trace and generating a goto.
  int trace_count_;
};


// A simple closed interval.
class Interval {
 public:
  Interval() : from_(kNone), to_(kNone) { }
  Interval(int from, int to) : from_(from), to_(to) { }
  Interval Union(Interval that) {
    if (that.from_ == kNone)
      return *this;
    else if (from_ == kNone)
      return that;
    else
      return Interval(Min(from_, that.from_), Max(to_, that.to_));
  }
  bool Contains(int value) {
    return (from_ <= value) && (value <= to_);
  }
  bool is_empty() { return from_ == kNone; }
  int from() { return from_; }
  int to() { return to_; }
  static Interval Empty() { return Interval(); }
  static const int kNone = -1;
 private:
  int from_;
  int to_;
};


class SeqRegExpNode: public RegExpNode {
 public:
  explicit SeqRegExpNode(RegExpNode* on_success)
      : on_success_(on_success) { }
  RegExpNode* on_success() { return on_success_; }
  void set_on_success(RegExpNode* node) { on_success_ = node; }
 private:
  RegExpNode* on_success_;
};


class ActionNode: public SeqRegExpNode {
 public:
  enum Type {
    SET_REGISTER,
    INCREMENT_REGISTER,
    STORE_POSITION,
    BEGIN_SUBMATCH,
    POSITIVE_SUBMATCH_SUCCESS,
    EMPTY_MATCH_CHECK,
    CLEAR_CAPTURES
  };
  static ActionNode* SetRegister(int reg, int val, RegExpNode* on_success);
  static ActionNode* IncrementRegister(int reg, RegExpNode* on_success);
  static ActionNode* StorePosition(int reg,
                                   bool is_capture,
                                   RegExpNode* on_success);
  static ActionNode* ClearCaptures(Interval range, RegExpNode* on_success);
  static ActionNode* BeginSubmatch(int stack_pointer_reg,
                                   int position_reg,
                                   RegExpNode* on_success);
  static ActionNode* PositiveSubmatchSuccess(int stack_pointer_reg,
                                             int restore_reg,
                                             int clear_capture_count,
                                             int clear_capture_from,
                                             RegExpNode* on_success);
  static ActionNode* EmptyMatchCheck(int start_register,
                                     int repetition_register,
                                     int repetition_limit,
                                     RegExpNode* on_success);
  virtual void Accept(NodeVisitor* visitor);
  virtual void Emit(RegExpCompiler* compiler, Trace* trace);
  virtual int EatsAtLeast(int still_to_find, int recursion_depth);
  virtual void GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int filled_in,
                                    bool not_at_start) {
    return on_success()->GetQuickCheckDetails(
        details, compiler, filled_in, not_at_start);
  }
  Type type() { return type_; }
  // TODO(erikcorry): We should allow some action nodes in greedy loops.
  virtual int GreedyLoopTextLength() { return kNodeIsTooComplexForGreedyLoops; }
  virtual ActionNode* Clone() { return new ActionNode(*this); }

 private:
  union {
    struct {
      int reg;
      int value;
    } u_store_register;
    struct {
      int reg;
    } u_increment_register;
    struct {
      int reg;
      bool is_capture;
    } u_position_register;
    struct {
      int stack_pointer_register;
      int current_position_register;
      int clear_register_count;
      int clear_register_from;
    } u_submatch;
    struct {
      int start_register;
      int repetition_register;
      int repetition_limit;
    } u_empty_match_check;
    struct {
      int range_from;
      int range_to;
    } u_clear_captures;
  } data_;
  ActionNode(Type type, RegExpNode* on_success)
      : SeqRegExpNode(on_success),
        type_(type) { }
  Type type_;
  friend class DotPrinter;
};


class TextNode: public SeqRegExpNode {
 public:
  TextNode(ZoneList<TextElement>* elms,
           RegExpNode* on_success)
      : SeqRegExpNode(on_success),
        elms_(elms) { }
  TextNode(RegExpCharacterClass* that,
           RegExpNode* on_success)
      : SeqRegExpNode(on_success),
        elms_(new ZoneList<TextElement>(1)) {
    elms_->Add(TextElement::CharClass(that));
  }
  virtual void Accept(NodeVisitor* visitor);
  virtual void Emit(RegExpCompiler* compiler, Trace* trace);
  virtual int EatsAtLeast(int still_to_find, int recursion_depth);
  virtual void GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int characters_filled_in,
                                    bool not_at_start);
  ZoneList<TextElement>* elements() { return elms_; }
  void MakeCaseIndependent();
  virtual int GreedyLoopTextLength();
  virtual TextNode* Clone() {
    TextNode* result = new TextNode(*this);
    result->CalculateOffsets();
    return result;
  }
  void CalculateOffsets();

 private:
  enum TextEmitPassType {
    NON_ASCII_MATCH,             // Check for characters that can't match.
    SIMPLE_CHARACTER_MATCH,      // Case-dependent single character check.
    NON_LETTER_CHARACTER_MATCH,  // Check characters that have no case equivs.
    CASE_CHARACTER_MATCH,        // Case-independent single character check.
    CHARACTER_CLASS_MATCH        // Character class.
  };
  static bool SkipPass(int pass, bool ignore_case);
  static const int kFirstRealPass = SIMPLE_CHARACTER_MATCH;
  static const int kLastPass = CHARACTER_CLASS_MATCH;
  void TextEmitPass(RegExpCompiler* compiler,
                    TextEmitPassType pass,
                    bool preloaded,
                    Trace* trace,
                    bool first_element_checked,
                    int* checked_up_to);
  int Length();
  ZoneList<TextElement>* elms_;
};


class AssertionNode: public SeqRegExpNode {
 public:
  enum AssertionNodeType {
    AT_END,
    AT_START,
    AT_BOUNDARY,
    AT_NON_BOUNDARY,
    AFTER_NEWLINE
  };
  static AssertionNode* AtEnd(RegExpNode* on_success) {
    return new AssertionNode(AT_END, on_success);
  }
  static AssertionNode* AtStart(RegExpNode* on_success) {
    return new AssertionNode(AT_START, on_success);
  }
  static AssertionNode* AtBoundary(RegExpNode* on_success) {
    return new AssertionNode(AT_BOUNDARY, on_success);
  }
  static AssertionNode* AtNonBoundary(RegExpNode* on_success) {
    return new AssertionNode(AT_NON_BOUNDARY, on_success);
  }
  static AssertionNode* AfterNewline(RegExpNode* on_success) {
    return new AssertionNode(AFTER_NEWLINE, on_success);
  }
  virtual void Accept(NodeVisitor* visitor);
  virtual void Emit(RegExpCompiler* compiler, Trace* trace);
  virtual int EatsAtLeast(int still_to_find, int recursion_depth);
  virtual void GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int filled_in,
                                    bool not_at_start);
  virtual AssertionNode* Clone() { return new AssertionNode(*this); }
  AssertionNodeType type() { return type_; }
 private:
  AssertionNode(AssertionNodeType t, RegExpNode* on_success)
      : SeqRegExpNode(on_success), type_(t) { }
  AssertionNodeType type_;
};


class BackReferenceNode: public SeqRegExpNode {
 public:
  BackReferenceNode(int start_reg,
                    int end_reg,
                    RegExpNode* on_success)
      : SeqRegExpNode(on_success),
        start_reg_(start_reg),
        end_reg_(end_reg) { }
  virtual void Accept(NodeVisitor* visitor);
  int start_register() { return start_reg_; }
  int end_register() { return end_reg_; }
  virtual void Emit(RegExpCompiler* compiler, Trace* trace);
  virtual int EatsAtLeast(int still_to_find, int recursion_depth);
  virtual void GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int characters_filled_in,
                                    bool not_at_start) {
    return;
  }
  virtual BackReferenceNode* Clone() { return new BackReferenceNode(*this); }

 private:
  int start_reg_;
  int end_reg_;
};


class EndNode: public RegExpNode {
 public:
  enum Action { ACCEPT, BACKTRACK, NEGATIVE_SUBMATCH_SUCCESS };
  explicit EndNode(Action action) : action_(action) { }
  virtual void Accept(NodeVisitor* visitor);
  virtual void Emit(RegExpCompiler* compiler, Trace* trace);
  virtual int EatsAtLeast(int still_to_find, int recursion_depth) { return 0; }
  virtual void GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int characters_filled_in,
                                    bool not_at_start) {
    // Returning 0 from EatsAtLeast should ensure we never get here.
    UNREACHABLE();
  }
  virtual EndNode* Clone() { return new EndNode(*this); }

 private:
  Action action_;
};


class NegativeSubmatchSuccess: public EndNode {
 public:
  NegativeSubmatchSuccess(int stack_pointer_reg,
                          int position_reg,
                          int clear_capture_count,
                          int clear_capture_start)
      : EndNode(NEGATIVE_SUBMATCH_SUCCESS),
        stack_pointer_register_(stack_pointer_reg),
        current_position_register_(position_reg),
        clear_capture_count_(clear_capture_count),
        clear_capture_start_(clear_capture_start) { }
  virtual void Emit(RegExpCompiler* compiler, Trace* trace);

 private:
  int stack_pointer_register_;
  int current_position_register_;
  int clear_capture_count_;
  int clear_capture_start_;
};


class Guard: public ZoneObject {
 public:
  enum Relation { LT, GEQ };
  Guard(int reg, Relation op, int value)
      : reg_(reg),
        op_(op),
        value_(value) { }
  int reg() { return reg_; }
  Relation op() { return op_; }
  int value() { return value_; }

 private:
  int reg_;
  Relation op_;
  int value_;
};


class GuardedAlternative {
 public:
  explicit GuardedAlternative(RegExpNode* node) : node_(node), guards_(NULL) { }
  void AddGuard(Guard* guard);
  RegExpNode* node() { return node_; }
  void set_node(RegExpNode* node) { node_ = node; }
  ZoneList<Guard*>* guards() { return guards_; }

 private:
  RegExpNode* node_;
  ZoneList<Guard*>* guards_;
};


class AlternativeGeneration;


class ChoiceNode: public RegExpNode {
 public:
  explicit ChoiceNode(int expected_size)
      : alternatives_(new ZoneList<GuardedAlternative>(expected_size)),
        table_(NULL),
        not_at_start_(false),
        being_calculated_(false) { }
  virtual void Accept(NodeVisitor* visitor);
  void AddAlternative(GuardedAlternative node) { alternatives()->Add(node); }
  ZoneList<GuardedAlternative>* alternatives() { return alternatives_; }
  DispatchTable* GetTable(bool ignore_case);
  virtual void Emit(RegExpCompiler* compiler, Trace* trace);
  virtual int EatsAtLeast(int still_to_find, int recursion_depth);
  int EatsAtLeastHelper(int still_to_find,
                        int recursion_depth,
                        RegExpNode* ignore_this_node);
  virtual void GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int characters_filled_in,
                                    bool not_at_start);
  virtual ChoiceNode* Clone() { return new ChoiceNode(*this); }

  bool being_calculated() { return being_calculated_; }
  bool not_at_start() { return not_at_start_; }
  void set_not_at_start() { not_at_start_ = true; }
  void set_being_calculated(bool b) { being_calculated_ = b; }
  virtual bool try_to_emit_quick_check_for_alternative(int i) { return true; }

 protected:
  int GreedyLoopTextLength(GuardedAlternative* alternative);
  ZoneList<GuardedAlternative>* alternatives_;

 private:
  friend class DispatchTableConstructor;
  friend class Analysis;
  void GenerateGuard(RegExpMacroAssembler* macro_assembler,
                     Guard* guard,
                     Trace* trace);
  int CalculatePreloadCharacters(RegExpCompiler* compiler);
  void EmitOutOfLineContinuation(RegExpCompiler* compiler,
                                 Trace* trace,
                                 GuardedAlternative alternative,
                                 AlternativeGeneration* alt_gen,
                                 int preload_characters,
                                 bool next_expects_preload);
  DispatchTable* table_;
  // If true, this node is never checked at the start of the input.
  // Allows a new trace to start with at_start() set to false.
  bool not_at_start_;
  bool being_calculated_;
};


class NegativeLookaheadChoiceNode: public ChoiceNode {
 public:
  explicit NegativeLookaheadChoiceNode(GuardedAlternative this_must_fail,
                                       GuardedAlternative then_do_this)
      : ChoiceNode(2) {
    AddAlternative(this_must_fail);
    AddAlternative(then_do_this);
  }
  virtual int EatsAtLeast(int still_to_find, int recursion_depth);
  virtual void GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int characters_filled_in,
                                    bool not_at_start);
  // For a negative lookahead we don't emit the quick check for the
  // alternative that is expected to fail.  This is because quick check code
  // starts by loading enough characters for the alternative that takes fewest
  // characters, but on a negative lookahead the negative branch did not take
  // part in that calculation (EatsAtLeast) so the assumptions don't hold.
  virtual bool try_to_emit_quick_check_for_alternative(int i) { return i != 0; }
};


class LoopChoiceNode: public ChoiceNode {
 public:
  explicit LoopChoiceNode(bool body_can_be_zero_length)
      : ChoiceNode(2),
        loop_node_(NULL),
        continue_node_(NULL),
        body_can_be_zero_length_(body_can_be_zero_length) { }
  void AddLoopAlternative(GuardedAlternative alt);
  void AddContinueAlternative(GuardedAlternative alt);
  virtual void Emit(RegExpCompiler* compiler, Trace* trace);
  virtual int EatsAtLeast(int still_to_find, int recursion_depth);
  virtual void GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int characters_filled_in,
                                    bool not_at_start);
  virtual LoopChoiceNode* Clone() { return new LoopChoiceNode(*this); }
  RegExpNode* loop_node() { return loop_node_; }
  RegExpNode* continue_node() { return continue_node_; }
  bool body_can_be_zero_length() { return body_can_be_zero_length_; }
  virtual void Accept(NodeVisitor* visitor);

 private:
  // AddAlternative is made private for loop nodes because alternatives
  // should not be added freely, we need to keep track of which node
  // goes back to the node itself.
  void AddAlternative(GuardedAlternative node) {
    ChoiceNode::AddAlternative(node);
  }

  RegExpNode* loop_node_;
  RegExpNode* continue_node_;
  bool body_can_be_zero_length_;
};


// There are many ways to generate code for a node.  This class encapsulates
// the current way we should be generating.  In other words it encapsulates
// the current state of the code generator.  The effect of this is that we
// generate code for paths that the matcher can take through the regular
// expression.  A given node in the regexp can be code-generated several times
// as it can be part of several traces.  For example for the regexp:
// /foo(bar|ip)baz/ the code to match baz will be generated twice, once as part
// of the foo-bar-baz trace and once as part of the foo-ip-baz trace.  The code
// to match foo is generated only once (the traces have a common prefix).  The
// code to store the capture is deferred and generated (twice) after the places
// where baz has been matched.
class Trace {
 public:
  // A value for a property that is either known to be true, know to be false,
  // or not known.
  enum TriBool {
    UNKNOWN = -1, FALSE = 0, TRUE = 1
  };

  class DeferredAction {
   public:
    DeferredAction(ActionNode::Type type, int reg)
        : type_(type), reg_(reg), next_(NULL) { }
    DeferredAction* next() { return next_; }
    bool Mentions(int reg);
    int reg() { return reg_; }
    ActionNode::Type type() { return type_; }
   private:
    ActionNode::Type type_;
    int reg_;
    DeferredAction* next_;
    friend class Trace;
  };

  class DeferredCapture : public DeferredAction {
   public:
    DeferredCapture(int reg, bool is_capture, Trace* trace)
        : DeferredAction(ActionNode::STORE_POSITION, reg),
          cp_offset_(trace->cp_offset()),
          is_capture_(is_capture) { }
    int cp_offset() { return cp_offset_; }
    bool is_capture() { return is_capture_; }
   private:
    int cp_offset_;
    bool is_capture_;
    void set_cp_offset(int cp_offset) { cp_offset_ = cp_offset; }
  };

  class DeferredSetRegister : public DeferredAction {
   public:
    DeferredSetRegister(int reg, int value)
        : DeferredAction(ActionNode::SET_REGISTER, reg),
          value_(value) { }
    int value() { return value_; }
   private:
    int value_;
  };

  class DeferredClearCaptures : public DeferredAction {
   public:
    explicit DeferredClearCaptures(Interval range)
        : DeferredAction(ActionNode::CLEAR_CAPTURES, -1),
          range_(range) { }
    Interval range() { return range_; }
   private:
    Interval range_;
  };

  class DeferredIncrementRegister : public DeferredAction {
   public:
    explicit DeferredIncrementRegister(int reg)
        : DeferredAction(ActionNode::INCREMENT_REGISTER, reg) { }
  };

  Trace()
      : cp_offset_(0),
        actions_(NULL),
        backtrack_(NULL),
        stop_node_(NULL),
        loop_label_(NULL),
        characters_preloaded_(0),
        bound_checked_up_to_(0),
        flush_budget_(100),
        at_start_(UNKNOWN) { }

  // End the trace.  This involves flushing the deferred actions in the trace
  // and pushing a backtrack location onto the backtrack stack.  Once this is
  // done we can start a new trace or go to one that has already been
  // generated.
  void Flush(RegExpCompiler* compiler, RegExpNode* successor);
  int cp_offset() { return cp_offset_; }
  DeferredAction* actions() { return actions_; }
  // A trivial trace is one that has no deferred actions or other state that
  // affects the assumptions used when generating code.  There is no recorded
  // backtrack location in a trivial trace, so with a trivial trace we will
  // generate code that, on a failure to match, gets the backtrack location
  // from the backtrack stack rather than using a direct jump instruction.  We
  // always start code generation with a trivial trace and non-trivial traces
  // are created as we emit code for nodes or add to the list of deferred
  // actions in the trace.  The location of the code generated for a node using
  // a trivial trace is recorded in a label in the node so that gotos can be
  // generated to that code.
  bool is_trivial() {
    return backtrack_ == NULL &&
           actions_ == NULL &&
           cp_offset_ == 0 &&
           characters_preloaded_ == 0 &&
           bound_checked_up_to_ == 0 &&
           quick_check_performed_.characters() == 0 &&
           at_start_ == UNKNOWN;
  }
  TriBool at_start() { return at_start_; }
  void set_at_start(bool at_start) { at_start_ = at_start ? TRUE : FALSE; }
  Label* backtrack() { return backtrack_; }
  Label* loop_label() { return loop_label_; }
  RegExpNode* stop_node() { return stop_node_; }
  int characters_preloaded() { return characters_preloaded_; }
  int bound_checked_up_to() { return bound_checked_up_to_; }
  int flush_budget() { return flush_budget_; }
  QuickCheckDetails* quick_check_performed() { return &quick_check_performed_; }
  bool mentions_reg(int reg);
  // Returns true if a deferred position store exists to the specified
  // register and stores the offset in the out-parameter.  Otherwise
  // returns false.
  bool GetStoredPosition(int reg, int* cp_offset);
  // These set methods and AdvanceCurrentPositionInTrace should be used only on
  // new traces - the intention is that traces are immutable after creation.
  void add_action(DeferredAction* new_action) {
    ASSERT(new_action->next_ == NULL);
    new_action->next_ = actions_;
    actions_ = new_action;
  }
  void set_backtrack(Label* backtrack) { backtrack_ = backtrack; }
  void set_stop_node(RegExpNode* node) { stop_node_ = node; }
  void set_loop_label(Label* label) { loop_label_ = label; }
  void set_characters_preloaded(int cpre) { characters_preloaded_ = cpre; }
  void set_bound_checked_up_to(int to) { bound_checked_up_to_ = to; }
  void set_flush_budget(int to) { flush_budget_ = to; }
  void set_quick_check_performed(QuickCheckDetails* d) {
    quick_check_performed_ = *d;
  }
  void InvalidateCurrentCharacter();
  void AdvanceCurrentPositionInTrace(int by, RegExpCompiler* compiler);
 private:
  int FindAffectedRegisters(OutSet* affected_registers);
  void PerformDeferredActions(RegExpMacroAssembler* macro,
                               int max_register,
                               OutSet& affected_registers,
                               OutSet* registers_to_pop,
                               OutSet* registers_to_clear);
  void RestoreAffectedRegisters(RegExpMacroAssembler* macro,
                                int max_register,
                                OutSet& registers_to_pop,
                                OutSet& registers_to_clear);
  int cp_offset_;
  DeferredAction* actions_;
  Label* backtrack_;
  RegExpNode* stop_node_;
  Label* loop_label_;
  int characters_preloaded_;
  int bound_checked_up_to_;
  QuickCheckDetails quick_check_performed_;
  int flush_budget_;
  TriBool at_start_;
};


class NodeVisitor {
 public:
  virtual ~NodeVisitor() { }
#define DECLARE_VISIT(Type)                                          \
  virtual void Visit##Type(Type##Node* that) = 0;
FOR_EACH_NODE_TYPE(DECLARE_VISIT)
#undef DECLARE_VISIT
  virtual void VisitLoopChoice(LoopChoiceNode* that) { VisitChoice(that); }
};


// Node visitor used to add the start set of the alternatives to the
// dispatch table of a choice node.
class DispatchTableConstructor: public NodeVisitor {
 public:
  DispatchTableConstructor(DispatchTable* table, bool ignore_case)
      : table_(table),
        choice_index_(-1),
        ignore_case_(ignore_case) { }

  void BuildTable(ChoiceNode* node);

  void AddRange(CharacterRange range) {
    table()->AddRange(range, choice_index_);
  }

  void AddInverse(ZoneList<CharacterRange>* ranges);

#define DECLARE_VISIT(Type)                                          \
  virtual void Visit##Type(Type##Node* that);
FOR_EACH_NODE_TYPE(DECLARE_VISIT)
#undef DECLARE_VISIT

  DispatchTable* table() { return table_; }
  void set_choice_index(int value) { choice_index_ = value; }

 protected:
  DispatchTable* table_;
  int choice_index_;
  bool ignore_case_;
};


// Assertion propagation moves information about assertions such as
// \b to the affected nodes.  For instance, in /.\b./ information must
// be propagated to the first '.' that whatever follows needs to know
// if it matched a word or a non-word, and to the second '.' that it
// has to check if it succeeds a word or non-word.  In this case the
// result will be something like:
//
//   +-------+        +------------+
//   |   .   |        |      .     |
//   +-------+  --->  +------------+
//   | word? |        | check word |
//   +-------+        +------------+
class Analysis: public NodeVisitor {
 public:
  explicit Analysis(bool ignore_case)
      : ignore_case_(ignore_case) { }
  void EnsureAnalyzed(RegExpNode* node);

#define DECLARE_VISIT(Type)                                          \
  virtual void Visit##Type(Type##Node* that);
FOR_EACH_NODE_TYPE(DECLARE_VISIT)
#undef DECLARE_VISIT
  virtual void VisitLoopChoice(LoopChoiceNode* that);

 private:
  bool ignore_case_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Analysis);
};


struct RegExpCompileData {
  RegExpCompileData()
    : tree(NULL),
      node(NULL),
      simple(true),
      contains_anchor(false),
      capture_count(0) { }
  RegExpTree* tree;
  RegExpNode* node;
  bool simple;
  bool contains_anchor;
  Handle<String> error;
  int capture_count;
};


class RegExpEngine: public AllStatic {
 public:
  struct CompilationResult {
    explicit CompilationResult(const char* error_message)
        : error_message(error_message),
          code(Heap::the_hole_value()),
          num_registers(0) {}
    CompilationResult(Object* code, int registers)
      : error_message(NULL),
        code(code),
        num_registers(registers) {}
    const char* error_message;
    Object* code;
    int num_registers;
  };

  static CompilationResult Compile(RegExpCompileData* input,
                                   bool ignore_case,
                                   bool multiline,
                                   Handle<String> pattern,
                                   bool is_ascii);

  static void DotPrint(const char* label, RegExpNode* node, bool ignore_case);
};


} }  // namespace v8::internal

#endif  // V8_JSREGEXP_H_
