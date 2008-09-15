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

#include <ctype.h>
#include <stdlib.h>

#include "v8.h"

#include "platform.h"

namespace v8 { namespace internal {

// Define all of our flags.
#define FLAG_MODE_DEFINE
#include "flags.defs"

// Define all of our flags default values.
#define FLAG_MODE_DEFINE_DEFAULTS
#include "flags.defs"

namespace {

// This structure represents a single entry in the flag system, with a pointer
// to the actual flag, default value, comment, etc.  This is designed to be POD
// initialized as to avoid requiring static constructors.
struct Flag {
  enum FlagType { TYPE_BOOL, TYPE_INT, TYPE_FLOAT, TYPE_STRING };

  FlagType type_;           // What type of flag, bool, int, or string.
  const char* name_;        // Name of the flag, ex "my_flag".
  void* valptr_;            // Pointer to the global flag variable.
  const void* defptr_;      // Pointer to the default value.
  const char* cmt_;         // A comment about the flags purpose.

  FlagType type() const { return type_; }

  const char* name() const { return name_; }

  const char* comment() const { return cmt_; }

  bool* bool_variable() const {
    ASSERT(type_ == TYPE_BOOL);
    return reinterpret_cast<bool*>(valptr_);
  }

  int* int_variable() const {
    ASSERT(type_ == TYPE_INT);
    return reinterpret_cast<int*>(valptr_);
  }

  double* float_variable() const {
    ASSERT(type_ == TYPE_FLOAT);
    return reinterpret_cast<double*>(valptr_);
  }

  const char** string_variable() const {
    ASSERT(type_ == TYPE_STRING);
    return reinterpret_cast<const char**>(valptr_);
  }

  bool bool_default() const {
    ASSERT(type_ == TYPE_BOOL);
    return *reinterpret_cast<const bool*>(defptr_);
  }

  int int_default() const {
    ASSERT(type_ == TYPE_INT);
    return *reinterpret_cast<const int*>(defptr_);
  }

  double float_default() const {
    ASSERT(type_ == TYPE_FLOAT);
    return *reinterpret_cast<const double*>(defptr_);
  }

  const char* string_default() const {
    ASSERT(type_ == TYPE_STRING);
    return *reinterpret_cast<const char* const *>(defptr_);
  }

  // Compare this flag's current value against the default.
  bool IsDefault() const {
    switch (type_) {
      case TYPE_BOOL:
        return *bool_variable() == bool_default();
      case TYPE_INT:
        return *int_variable() == int_default();
      case TYPE_FLOAT:
        return *float_variable() == float_default();
      case TYPE_STRING:
        const char* str1 = *string_variable();
        const char* str2 = string_default();
        if (str2 == NULL) return str1 == NULL;
        if (str1 == NULL) return str2 == NULL;
        return strcmp(str1, str2) == 0;
    }
    UNREACHABLE();
    return true;
  }

  // Set a flag back to it's default value.
  void Reset() {
    switch (type_) {
      case TYPE_BOOL:
        *bool_variable() = bool_default();
        break;
      case TYPE_INT:
        *int_variable() = int_default();
        break;
      case TYPE_FLOAT:
        *float_variable() = float_default();
        break;
      case TYPE_STRING:
        *string_variable() = string_default();
        break;
    }
  }
};

Flag flags[] = {
#define FLAG_MODE_META
#include "flags.defs"
};

const size_t num_flags = sizeof(flags) / sizeof(*flags);

}  // namespace


static const char* Type2String(Flag::FlagType type) {
  switch (type) {
    case Flag::TYPE_BOOL: return "bool";
    case Flag::TYPE_INT: return "int";
    case Flag::TYPE_FLOAT: return "float";
    case Flag::TYPE_STRING: return "string";
  }
  UNREACHABLE();
  return NULL;
}


static char* ToString(Flag* flag) {
  Vector<char> value;
  switch (flag->type()) {
    case Flag::TYPE_BOOL:
      value = Vector<char>::New(6);
      OS::SNPrintF(value, "%s", (*flag->bool_variable() ? "true" : "false"));
      break;
    case Flag::TYPE_INT:
      value = Vector<char>::New(12);
      OS::SNPrintF(value, "%d", *flag->int_variable());
      break;
    case Flag::TYPE_FLOAT:
      value = Vector<char>::New(20);
      OS::SNPrintF(value, "%f", *flag->float_variable());
      break;
    case Flag::TYPE_STRING:
      const char* str = *flag->string_variable();
      if (str) {
        int length = strlen(str) + 1;
        value = Vector<char>::New(length);
        OS::SNPrintF(value, "%s", str);
      } else {
        value = Vector<char>::New(5);
        OS::SNPrintF(value, "NULL");
      }
      break;
  }
  ASSERT(!value.is_empty());
  return value.start();
}


// static
List<char *>* FlagList::argv() {
  List<char *>* args = new List<char*>(8);
  for (size_t i = 0; i < num_flags; ++i) {
    Flag* f = &flags[i];
    if (!f->IsDefault()) {
      Vector<char> cmdline_flag;
      if (f->type() != Flag::TYPE_BOOL || *(f->bool_variable())) {
        int length = strlen(f->name()) + 2 + 1;
        cmdline_flag = Vector<char>::New(length);
        OS::SNPrintF(cmdline_flag, "--%s", f->name());
      } else {
        int length = strlen(f->name()) + 4 + 1;
        cmdline_flag = Vector<char>::New(length);
        OS::SNPrintF(cmdline_flag, "--no%s", f->name());
      }
      args->Add(cmdline_flag.start());
      if (f->type() != Flag::TYPE_BOOL) {
        args->Add(ToString(f));
      }
    }
  }

  return args;
}


// Helper function to parse flags: Takes an argument arg and splits it into
// a flag name and flag value (or NULL if they are missing). is_bool is set
// if the arg started with "-no" or "--no". The buffer may be used to NUL-
// terminate the name, it must be large enough to hold any possible name.
static void SplitArgument(const char* arg,
                          char* buffer,
                          int buffer_size,
                          const char** name,
                          const char** value,
                          bool* is_bool) {
  *name = NULL;
  *value = NULL;
  *is_bool = false;

  if (*arg == '-') {
    // find the begin of the flag name
    arg++;  // remove 1st '-'
    if (*arg == '-')
      arg++;  // remove 2nd '-'
    if (arg[0] == 'n' && arg[1] == 'o') {
      arg += 2;  // remove "no"
      *is_bool = true;
    }
    *name = arg;

    // find the end of the flag name
    while (*arg != '\0' && *arg != '=')
      arg++;

    // get the value if any
    if (*arg == '=') {
      // make a copy so we can NUL-terminate flag name
      int n = arg - *name;
      CHECK(n < buffer_size);  // buffer is too small
      memcpy(buffer, *name, n);
      buffer[n] = '\0';
      *name = buffer;
      // get the value
      *value = arg + 1;
    }
  }
}


inline char NormalizeChar(char ch) {
  return ch == '_' ? '-' : ch;
}


static bool EqualNames(const char* a, const char* b) {
  for (int i = 0; NormalizeChar(a[i]) == NormalizeChar(b[i]); i++) {
    if (a[i] == '\0') {
      return true;
    }
  }
  return false;
}


static Flag* FindFlag(const char* name) {
  for (size_t i = 0; i < num_flags; ++i) {
    if (EqualNames(name, flags[i].name()))
      return &flags[i];
  }
  return NULL;
}


// static
int FlagList::SetFlagsFromCommandLine(int* argc,
                                      char** argv,
                                      bool remove_flags) {
  // parse arguments
  for (int i = 1; i < *argc;) {
    int j = i;  // j > 0
    const char* arg = argv[i++];

    // split arg into flag components
    char buffer[1*KB];
    const char* name;
    const char* value;
    bool is_bool;
    SplitArgument(arg, buffer, sizeof buffer, &name, &value, &is_bool);

    if (name != NULL) {
      // lookup the flag
      Flag* flag = FindFlag(name);
      if (flag == NULL) {
        if (remove_flags) {
          // We don't recognize this flag but since we're removing
          // the flags we recognize we assume that the remaining flags
          // will be processed somewhere else so this flag might make
          // sense there.
          continue;
        } else {
          fprintf(stderr, "Error: unrecognized flag %s\n", arg);
          return j;
        }
      }

      // if we still need a flag value, use the next argument if available
      if (flag->type() != Flag::TYPE_BOOL && value == NULL) {
        if (i < *argc) {
          value = argv[i++];
        } else {
          fprintf(stderr, "Error: missing value for flag %s of type %s\n",
                  arg, Type2String(flag->type()));
          return j;
        }
      }

      // set the flag
      char* endp = const_cast<char*>("");  // *endp is only read
      switch (flag->type()) {
        case Flag::TYPE_BOOL:
          *flag->bool_variable() = !is_bool;
          break;
        case Flag::TYPE_INT:
          *flag->int_variable() = strtol(value, &endp, 10);  // NOLINT
          break;
        case Flag::TYPE_FLOAT:
          *flag->float_variable() = strtod(value, &endp);
          break;
        case Flag::TYPE_STRING:
          *flag->string_variable() = value;
          break;
      }

      // handle errors
      if ((flag->type() == Flag::TYPE_BOOL && value != NULL) ||
          (flag->type() != Flag::TYPE_BOOL && is_bool) ||
          *endp != '\0') {
        fprintf(stderr, "Error: illegal value for flag %s of type %s\n",
                arg, Type2String(flag->type()));
        return j;
      }

      // remove the flag & value from the command
      if (remove_flags)
        while (j < i)
          argv[j++] = NULL;
    }
  }

  // shrink the argument list
  if (remove_flags) {
    int j = 1;
    for (int i = 1; i < *argc; i++) {
      if (argv[i] != NULL)
        argv[j++] = argv[i];
    }
    *argc = j;
  }

  // parsed all flags successfully
  return 0;
}


static char* SkipWhiteSpace(char* p) {
  while (*p != '\0' && isspace(*p) != 0) p++;
  return p;
}


static char* SkipBlackSpace(char* p) {
  while (*p != '\0' && isspace(*p) == 0) p++;
  return p;
}


// static
int FlagList::SetFlagsFromString(const char* str, int len) {
  // make a 0-terminated copy of str
  char* copy0 = NewArray<char>(len + 1);
  memcpy(copy0, str, len);
  copy0[len] = '\0';

  // strip leading white space
  char* copy = SkipWhiteSpace(copy0);

  // count the number of 'arguments'
  int argc = 1;  // be compatible with SetFlagsFromCommandLine()
  for (char* p = copy; *p != '\0'; argc++) {
    p = SkipBlackSpace(p);
    p = SkipWhiteSpace(p);
  }

  // allocate argument array
  char** argv = NewArray<char*>(argc);

  // split the flags string into arguments
  argc = 1;  // be compatible with SetFlagsFromCommandLine()
  for (char* p = copy; *p != '\0'; argc++) {
    argv[argc] = p;
    p = SkipBlackSpace(p);
    if (*p != '\0') *p++ = '\0';  // 0-terminate argument
    p = SkipWhiteSpace(p);
  }

  // set the flags
  int result = SetFlagsFromCommandLine(&argc, argv, false);

  // cleanup
  DeleteArray(argv);
  // don't delete copy0 since the substrings
  // may be pointed to by FLAG variables!
  // (this is a memory leak, but it's minor since this
  // code is only used for debugging, or perhaps once
  // during initialization).

  return result;
}


// static
void FlagList::ResetAllFlags() {
  for (size_t i = 0; i < num_flags; ++i) {
    flags[i].Reset();
  }
}


// static
void FlagList::PrintHelp() {
  for (size_t i = 0; i < num_flags; ++i) {
    Flag* f = &flags[i];
    char* value = ToString(f);
    printf("  --%s (%s)  type: %s  default: %s\n",
           f->name(), f->comment(), Type2String(f->type()), value);
    DeleteArray(value);
  }
}

} }  // namespace v8::internal
