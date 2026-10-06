# QuarticC (C⁴/C^4)

### The 4th Evolution of C

```qc
struct Register {
    volatile addr_t value;
};

int main() {
    volatile addr_t *p = `mapped_ptr(0xB8000);
    *p = 0x0543056305690574;
    *(p + 1) = 0x0572056105750551;

    Register *reg = `malloc(sizeof Register);
    defer `free(reg);
    *reg = Register{};
    reg->value = 0xdeadbeefdeadbeef;

    return reg->value == 0xdeadbeefdeadbeef ? 0 : 1;
}
```

Docs are [here](https://youg-otricked.github.io/QuarticC/).
Book is [here](https://youg-otricked.github.io/QCBook/).

**More Powerful Than Explosives™**/j

## What is QuarticC?

C⁴ is a compiled, statically typed C-style systems programming language, focused on no global scope pollution and keeping code clean while still giving you minute control.
Whether your codebase is 100 or 100,000 lines long, code should still make sense next week, even to the person who wrote it.

---

# Video

Go to the [docs](https://youg-otricked.github.io/QuarticC/)

## Quick Start

Install the binaries from GitHub, use the [Package & Version Manager](https://github.com/Youg-Otricked/quartic-c-manager), or:

### Installation

#### Dependencies

You must have `clang` installed. (Obviously, it's a clang project)

---

After cloning the repo to your machine, run.

```bash
cd QuarticC
chmod +x install.sh
./install.sh
# Now, you can type
qc [flags] (filepath)
# to run a file
# or
qc [flags]
# to enter the REPL
```

---

# Sections

- [Roadmap](#feature-roadmap)
- [Manual Memory Management](#manual-memory-management)
- [Include System](#include-system)
- [Why QuarticC?](#why-quarticc)
- [Code Conventions](#conventions)
- [Type collections, Collection types](#type-collections-and-collection-types)
- [Systems-y stuff](#c-interop-and-inline-asm)
- [Generics and Iterators](#generics)
- [Misc.](#performance-comparison)

---

# Versioning Scheme

QuarticC uses the following versioning scheme:
`cMa.Mo.MiP`
, where `c` is critical, for massive additions, such as the compiler being added, `Ma` being major versions, tracking large collections of features, `Mo` being moderate versions, tracking collections of similar features, `Mi` being minor versions, which track individual feature milestones within the current moderate version's theme, and `P` being the patch version.
For the version
`x1.2.34`
`c` = `x`
`Ma` = `1`
`Mo` = `2`
`Mi` = `3`
`P` = `4`

P is omitted if it is 0.
Critical versions represent the largest generational milestones in QuarticC's development.

v = Interpreter
x = Compiler (Current)
f = Feature-complete compiler
s = Self-hosted compiler

Critical versions are intentionally rare and denote architectural milestones,
not language features.

Development toward future critical versions may begin before the current
critical version is complete. Multiple critical generations may therefore
be in development simultaneously.

Minor (Mi) is always a single decimal digit (0-9). Once a minor version reaches 9, the next release increments the moderate version instead.

Unlike semantic versioning, QuarticC versions describe the scale and category of language evolution rather than API compatibility.

# Development Status

Current Version: x1.2.0 = "Tuples"
Next Version: x1.2.1 = "Destructuring"

# Current Version Highlights

```text
Critical
└─ Added Compiler

Major
└─ N/A

Moderate
└─ Tuples

Minor
└─ N/A

Patch
└─ N/A
```

# Recent Deprecations / Breaking Changes

These are deprecations in the past 3 moderate versions (`x1.0.* -> x1.2.*`)

Jagged Arrays

# Upcoming Deprecations:

## Feature Roadmap

| Category            | Feature                                                                | Status  |
| ------------------- | ---------------------------------------------------------------------- | ------- |
| **Core Logic**      | Variables & Types (`int`, `float`, `double`, `string`, `char`, `bool`) | Done    |
|                     | Constants (`const`) & `auto` Inference                                 | Done    |
|                     | Long and Short types                                                   | Done    |
| **Operators**       | Standard Math & Expressions (includes `#^` power operator)             | Done    |
|                     | Control Flow (`if/else`, `switch`, `while`, `for`, `foreach`)          | Done    |
| **Functions**       | Functions with Default Parameters                                      | Done    |
|                     | Lambdas & Higher-Order Functions                                       | Done    |
|                     | **Native Multi-Return Values**                                         | Done    |
| **Data Structures** | Arrays                                                                 | Done    |
|                     | Spread Syntax for Arrays (`@`)                                         | Done    |
|                     | Enums                                                                  | Done    |
| **Advanced**        | Structs, Classes, & Namespaces                                         | Done    |
|                     | Union Types (TypeScript-style)                                         | Done    |
|                     | Advanced OOP & Operator Overloading                                    | Done    |
|                     | Manual Memory Management                                               | Done    |
| **System**          | F-Strings (Python-style interpolation)                                 | Done    |
|                     | Random Number Generation                                               | Done    |
|                     | Stdlib Part 1 & 2                                                      | Done    |
| **Future**          | Stdlib Part 3                                                          | Planned |
|                     | Inline ASM                                                             | Done    |
|                     | Generics                                                               | Done    |
|                     | Classes                                                                | Done    |
|                     | Structs                                                                | Done    |
|                     | Unions                                                                 | Done    |
|                     | Functions & Methods                                                    | Done    |
|                     | Variadic Generic Arguments                                             | Planned |
|                     | `restrict`, `out`, `inout`, `volatile`                                 | Done    |
|                     | Extern                                                                 | Done    |
|                     | Bitwise Logic                                                          | Done    |
|                     | Really fancy operator overloads                                        | Done    |
|                     | Try/Catch and `throw`                                                  | Done    |
|                     | Error message quality and helpfulness upgrade                          | Done    |

See the full list of remaining features in the [roadmap](https://github.com/Youg-Otricked/QuarticC/blob/master/notes/roadmap.md).

# Contributing

If you want to contribute to C^4, fork the repository, make your changes, and open a pull request.

## Commit Syntax

Commits should follow this format:

```text
category(thing): message
```

Available categories:

```text
improve   Improvements to existing functionality
feat      New functionality or language features
clean     Code cleanup, refactoring, or technical debt
errors    Changes to diagnostics, warnings, or error handling / adding more errors, warnings, notes, help, suggestions, etc
fix       Bug fixes
```

For example:

```text
feat(parser, compiler): add modifier support
errors(warnings, diagnostics, help, suggestions): add help suggestions to warnings
clean(compiler): simplify type checking
fix(codegen): handle missing returns in main
improve(lexer): reduce tokenization overhead
```

If your commit is about multiple catagorys, comma-seperate.

```text
errors(warnings),feat(cli-flags): added warnings & -W flags
```

But at least make your commits sensical. Unlike mine

```commit
FINALLY IM DONE WHY DID THAT TAKE 5 DAYS AAGHAGAHGAH IT WORKS
```

Please don't.
If you forgot something in a previous commit, i like to use

```text
oops(README): forgot to re-add readme.md
```

---

## License

MIT License - See [LICENSE](./LICENSE.txt) for details

---

### i2 Primitive Type

```cpp
qbool qb = both;
qif (qb && qtrue /* evaluates to both */) {
    `qout("True path");   // Executes
} qelse {
    `qout("False path");
}
```

---

---

## Manual Memory Management

```cpp
int main() {
    int* ptr = `malloc(sizeof "int");
    *ptr = 42;
    `qout("%p", ptr);
    `free(ptr);
}
```

## Include system

```cpp
namespace Exported {
/*
Exported namespaces are merged during include resolution.
See the include-system docs for full details.
*/
    #include <Math, std> // std is an alias for ~/.qc/lib/stdlib.qc. This line imports the Math namespace from the standard library.
}

int main() {
    return Math::Max(1234, 432); // Using the math namespace.
}
```

Namespaces can also declare dependencies on other namespaces in the same file
using `#depends`, ensuring includers automatically see required types even if
they only explicitly included one namespace from that file:

```cpp
#depends(x: y)
namespace x { /* uses y::Something internally */ }
namespace y { /* ... */ }
```

Want to learn more? Check out the [docs for it](https://youg-otricked.github.io/QuarticC/include.html).

---

## Why QuarticC?

| **Feature**                 | **C++**              | **Zig**              | **Rust**       | **QuarticC**           | **C**                           |
| --------------------------- | -------------------- | -------------------- | -------------- | ---------------------- | ------------------------------- |
| **Comp + Run**              | Medium               | Fast                 | Medium-Slow    | Medium-Fast            | Fastest                         |
| **Compile Time (relative)** | Slow                 | Fast (Direct-to-ASM) | (Really) Slow  | Medium                 | Fastest                         |
| **Runtime**                 | Fast                 | Fast                 | Fast           | Fast                   | Fast                            |
| **Memory safety**           | Manual               | GPA                  | Borrow checker | Manual                 | Manual                          |
| **Multi-return**            | Structs              | Tuples               | Tuples         | **Native** (Or tuples) | Structs                         |
| **Generics**                | Templates + Concepts | Type as Argument     | Trait Based    | Constraint-Based       | No (Macro Hell does not count.) |
| **Memory Control**          | Yes                  | Yes                  | Ehhh           | Yes                    | Yes                             |

Based on the last reliable benchmark results, QuarticC showed performance in the same general range as C++, while offering a similar set of quality-of-life improvements found in languages such as Zig.According to most recent benchmarks, in tested cases C^4 runs either at a similar or faster speed than C++, with equal or faster compiles.

---

## Conventions

QuarticC has unusual naming conventions:

| **Type**                               | **Convention**         | **Why?**                                                                                                                      |
| -------------------------------------- | ---------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| **Variables**                          | `snake_case`           | It's familiar to Python devs who changed their ways, C++, C, Zig, Go, and Rust devs.                                          |
| **Functions**                          | `camelCase`            | It allows for instant knowledge between if an identifier is a var, or function (lambdas use var casing, not function casing)  |
| **User Types**                         | `PascalCase`           | It is common across basically every programming language.                                                                     |
| **Constants**                          | `SCREAMING_SNAKE_CASE` | Same as above.                                                                                                                |
| **Private Member Variables**           | `__snake_case`         | Variable case prepended with __. Most underscores.                                                                            |
| **Protected Member Variables**         | `_snake_case`          | Less underscores.                                                                                                             |
| **Protected Methods**                  | `__camelCase`          | Unique casing, more underscores.                                                                                              |
| **Private Methods**                    | `camel_Snake_Case`     | Function casing, more underscores.                                                                                            |
| **Namespaces**                         | `PascalCase`           | Same as user types.                                                                                                           |
| **Namespaces Not Meant For Inclusion** | `Pascal_Snake_Case`    | Unique casing style, more underscores, you have to be trying to include this.                                                 |
| **Global Scope Functions**             | `camel_Snake_Case`     | Unique casing style, more underscores, similarity to private methods is intentional, because global scope cannot be included. |
| **Methods Used By Compiler**           | `_camelCase`           | Different from everything else. (these methods are iterators and stuff. Methods you define and compiler uses)                 |
| **Compiler Reserved**                  | `_qc_, __qc_ and qc_`  | Unique, hard to use accidently                                                                                                |
| **Compiler Intrinsics**                | \` + `snake_case`      | Unique, impossible to use accidently                                                                                          |

Max line size is around 120 _relative to your starting indentation_, tabs or spaces, lf newlines, comments are `//`, doc comments are `///`, and top-level doc comments are `//!`. File paths are unquoted, everything other than main should go in a namespace when applicable, and namespaces should fit the following rules:

1. Namespaces should do one thing well, similar to the UNIX philosophy,
2. Namespaces should have either:

        1. one type or group of tightly related types such as bigints and their core helpers,
        2. above + namespaces containing extra helpers
        3. helper functions / utility functions (think a `Math` namespace with log, cos...)
        4. OR anything if directly mapping  C/C++/Zig/Rust code to C^4

3. Types in namespaces should have short names: The namespace should have the longer name
   e.g.

```qc
namespace Array {
    class Arr<T, int S = 0> {
        ...
    }
}
```

Pointer asterisks bind to the type rather than the variable. The final * belongs to the declarator, unless it's a function return type. Then it's all on the type.

```
int** *x;
int* ptr_add(int *p) ...
```

Files are `kebab-case`. This is optional.
QuarticC naming conventions are designed to make code readable without requiring the reader to inspect library code. Names should provide immediate context.
My rule: `RTFM` once, not `RTMSCE5S` (Read The Manual and Source Code Every 5 Seconds), and these conventions make things hard to forget or mis-type, unlike C/C++ where every library uses entirely different conventions.

Example:

```
namespace Network {
    class Client {
        string server_name;

        void connectToServer() {
            ...
        }
    }
}
namespace Not_Embezzeling { // Intentionally formatted as a non-inclusion namespace.
    // Sure, you may not want to type all of that. That means your users absolutely don't.
    ..
}
```

## Ideals

QuarticC follows four core rules:

- Forced Cleanliness: QuarticC is designed to reward readable code. Language features should make the obvious solution the clean solution. Clean is not defined as 'Convenient for language', it means what it should be. Clean is not a "pythonic" equivalent; it is self-explanatory.
- Your Memory, Your Problem: QuarticC does not prevent dangerous code. It expects the programmer to understand the consequences. If you want, you can write a segfault handler with a segfault in it. QuarticC is strongly typed, but union types are designed to be ergonomic rather than restrictive. Unlike Rust or TypeScript, QuarticC does not force exhaustive narrowing before every union operation. I will give you a loaded shotgun. If you blow your leg off, don't blame the gunsmith.
- No Hiding: QuarticC is an explicit language, so your code does what it looks like it does. Nothing is hidden inside the parser while pretending to be stdlib, nothing is hidden away in some back catacomb. If it is an intrinsic, it says it is.
- No Excessive Syntax: No capture lists on lambdas, no templates, no infinite <>, no Rust "bird droppings", no ! and @ everywhere.

## Compiler Architecture

QuarticC uses a classic multi-pass compilation pipeline:

1. **Lexical Analysis / Preprocessing:** Custom lexer converts text to tokens in one loop.
2. **AST Parsing:** Recursive descent parser generating a strongly typed Abstract Syntax Tree.
3. **Type Checking & Semantic Analysis:** Resolves user-defined types, namespaces, and TypeScript-style union types. The unique thing is that this pass is _merged with the compilation/codegen phase_
4. **Intermediate Representation (IR):** Generates LLVM IR. Target-aware pointer arithmetic is achieved dynamically via target-specific DataLayout queries.
5. **Codegen:** Emits native platform object files (`.o`) or WebAssembly binaries via LLVM's target machines.

---

## Multi Return

```cpp
int, string GetStatus() {
    return 200, "Success";
}
int main() {
    int code, string alias = GetStatus();
    `qout("%s", f"Code: {code}, AKA: {alias}\n");
    return 0;
}
```

## Type collections and collection types.

Union types and aliases use the exact same syntax.

```cpp
int main() {
    // Union type. No narrowing. Compiled to LLVM. I SPENT HOURS ON THIS.
    type IdT = int | string;
    IdT id = 101;
    id = "A101"; // Valid

    // Alias
    type UserID = int;
    UserID myId = 5;
}
```

Unions require exactly 0 narrowing.

### Tagged Enums

Enums can have tags.

```qc
enum MyEnum {
    OK(int);
    NONE;
}
```

You construct enums like this:

```qc
MyEnum.OK(123);
MyEnum.NONE;
```

Normally, you can only access the descriminant of a enum. To access the tag, you must use a `match` (yes, C⁴ has both `switch` and `match`)

```qc
match (value) {
    MyEnum.OK(x) => ...
    ...
}
```

(You can also match on integers)

### Tuples

Tuples use the Zig-style syntax.

```qc
(int, int) makeCoordinate(int x, int y) {
    return (x, y);
}
```

This means you can use both tuples and raw values for multireturn, or make functions to unpack tuples.

```qc
int, int unpackCoordinate((int, int) coord) {
    return coord.0, coord.1;
}
```

---

## C interop and inline ASM

Extern "C" is the only supported extern style, and thus no string is needed to say where you are externing to.

```cpp
extern:
int add(int a, int b) {
    return a + b;
}
:extern
```

Extern is only for _externalizing_ api. To use _foreign_ code, you must put it in a `foreign` block.

```cpp
foreign:
int do_some_c_stuff(int x);
:foreign
```

---

The QuarticC inline ASM syntax is like a simplified version of the GCC syntax:

```cpp
inline(R"(
mov rax, 1
mov rdi, 1
mov rsi, $0r ; argument 1 (Hello, World!)
mov rdx, $1r ; argument 2 (14)
syscall
)"/* your inline asm string */, "Hello, World", 14, "~{rax,rdi,rsi,rdx}" /* clobbers */);
```

You can also use AT&T ASM syntax by making sure the first 5 characters of your ASM string are

```asm
; ATT
```

## Storage & Argument Specifiers

QuarticC has 4 special (non-`const`) storage modifiers.

1. `volatile`
   Volatile means the compiler won't optimize it. It can be used before variable declarations, or on function definitions. On methods, it must go after the access modifier and `final`, and before the return type.

```
volatile void infinite_time() {
    while(true) {
    }
    return;
}
```

2. `restrict`

`restrict` tells the compiler that this pointer is the exclusive access path to the referenced memory. Other unrelated pointers must not access the same memory in a way that violates the restrict contract. This allows more aggressive optimization.

Example:

```qc
void doSomePointer(int *restrict ptr, int *other) {
    *ptr = 10;
    *other = 20; // Undefined behavior if other points to the same memory as ptr
}
```

3. `out`
   Out tells the compiler that this parameter is write-only, and this memory address will not be copied. It also only exists for optimization purposes.

```qc
void writeOnly(out int p) {
    p = 123; // OK
    int x = p; // ILLEGAL. Breaks contract.
    int *x = &p; // ILLEGAL also.
}
```

4. `inout`
   Inout tells the compiler that this parameter will be read and written from, but its address will not be copied.

```qc
void rw(inout int p) {
    p = 123; // OK
    int x = p; // OK
    int *x = &p; // NOT OK
}
```

## Variadic arguments

QuarticC variadic arguments look like this:

```cpp
int add_all(...args) {
    int res = 0;
    while (!`is_empty(args)) {
        res += `next(args, "int");
    }
    return res;
}
```

`args` is a variadic argument (hence the `...`), which can be passed any number of parameters.
`is_empty` is a runtime compiler intrinsic that takes a variadic arg-list and returns whether it is empty.
`next` is a runtime compiler intrinsic that takes a variadic arg-list and the expected type and returns the next element in the arg list cast to that type.

You may use C-Style variadic arguments too, but _only in foreign blocks_.

```cpp
foreign:
void printf(char* fmt, ...);
:foreign
```

## Bitwise logic

QuarticC has all the standard bitwise logic operators; however, it has a non-standard XOR and Right-Shift token.
The Bitwise XOR operator in C^4 is `$`. It is `$` because ^ and ^^ are already used tokens, and C^4 avoids repeating tokens to improve quick readability.
The same logic applies for right-shift: `|>` is the right-shift token, because it allows the parser to immediately determine:

```
Node<Vector<X>> // Is this RSHIFT or ending generics?
```

This is easy to determine (because it would be illegal for rshift to be there); however, using this non-normal rshift operator allows it to be instant because:

```
Node<Vector<X>> // This is unrelated to RSHIFT
```

QuarticC also has a special operator for _logical_ right shift: :>. This is because it allows avoiding constantly casting between signed and unsigned integer types, unlike C++'s "arithmetic if signed, logical if unsigned"

2 more unique bitwise things QuarticC does:

1. Built-in rotations: `<<<` and `|>>` and `LROT` and `RROT`, allowing 1 instruction rotations instead of 6+.
2. - and - have lower priority than shifts. `3 << 3 + 2 == 26`. This is because the shift expression `l << r` is equivalent to `l * 2 ^ r`. If r was 2 + 2, that would be `l * 2 ^ 2 + 2`, which would be l * 4 + 2.

## Inheritance

All methods are virtual. If you want to use polymorphism, inherited methods MUST be defined in the same order as the parent class, and new methods must be defined after.

## Generics

QuarticC generics have simple syntax:

```
class C<T> {
    T x;
    C() {
    }
}
int main() {
    C<int> thing = C();
}
```

Generics are allowed on `struct`s, `class`es, `concept`s, unions, `enum`s, aliases, functions, and methods.
The unique thing about QuarticC's generics is its constraint system:
The constraint system follows this syntax:

```
<T([constraint]:[[!]<[typename]'|'...>]>
```

Main constraints are as follows:

```
usertype: non-primitive type
primitive: primitive type
pointer: any pointer type
numeric: any numeric type
```

So

```
<T(numeric:)>
```

Can be any numeric type.
The subconstraint system is like this:

```
<T(:!int|string)>
```

This means "T can be any type other than int or string".
The ! means not anything in this set, and the type|type means these types.
QuarticC also has non-type generic parameters.

```
<int S> // S is a non-type generic parameter (a compile time int)
```

In Rust,

```
<T(numeric:)>
```

Would be

```
<T: std::ops::Add<Output = T> + std::ops::Sub<Output = T> + std::ops::Mul<Output = T> + std::ops::Div<Output = T> + PartialOrd + Copy>
```

And in C++, it would be

```
template <typename T>
requires std::is_arithmetic_v<T>
T
```

or in old SFINAE C++

```
template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
T
```

And in Zig:

```
...(comptime T: type, ...) ... {
    switch (@typeInfo(T)) {
        .Int, .Float => ...,
        else => @compileError("T must be numeric"),
    }
}
```

## Iterators

Iterators work as follows:
The class you want to iterate must define the following method:

```qc
MyIterator _begin() // Returns an iterator to the beginning of the class

```

`_end` is optional, and returns an iterator to the end of the class.
The iterator itself MUST define the following 2 methods:

```qc
bool _atEnd() // Returns true if there is nothing left to iterate
T _next() // Returns the current value then moves to the next
```

and may define the following recommended extra methods:

```qc
bool _atStart() // Returns true if the iterator is currently at index 0
T _prev() // Returns the element at current index -- and decrements current index
void _moveTo(whateveryouwant idx) // sets index to idx
MyIterator<U> _map<U>(fn(T elem) -> U) // preforms a operation on every element
MyClass<T> _collect() // returns the class that this iterator iterates
MyIterator<T> _filter(fn(T elem) -> bool) // returns a iterator of only the elements that are true for that lambda
void _inval_destroy() // invalidates the collection this iterator iterates
MyIterator<T> _take(int n) // returns a iterator to the first n elements
MyIterator<T> _drop(int n) // returns a iterator skipping the first n elements
```

Example:

```qc
class ArrayIterator<T> {
    T* data;
    int size;
    int current_index;
    ArrayIterator(T* data, int size, bool is_end) {
        this.data = data;
        this.size = size;
        this.current_index = `ternary(is_end, size, 0);
    }
    bool _atEnd() {
        return this.size <= this.current_index;
    }
    T _next() {
        if (!this._atEnd()) {
            return this.data[this.current_index++];
        }
        return this.data[this.current_index];
    }
    bool _atStart() {
        return this.current_index <= 0;
    }
    T _prev() {
        if (!this._atStart()) {
            return this.data[--this.current_index];
        }
        return this.data[this.current_index];
    }
    void _moveTo(int index) {
        if (index >= this.size) {
            index = this.size - 1;
        } else if (index < 0) {
            index = 0;
        }
        this.current_index = index;
    }
}
class Array<T, int S = 0> {
    T* data;
    int size;
    Array() {
        this.data = nullptr;
        this.size = 0;
    }
    void operator[]=(T* data, int length) {
        if (length > S) {
            this.size = length;
        } else {
            this.size = S;
        }
        this.data = `malloc(sizeof "T" * this.size);
        for (int i = 0; i < this.size; i++) {
            this.data[i] = data[i];
        }
    }
    T operator[](int index) {
        return this.data[index];
    }
    ArrayIterator<T> _begin() {
        return ArrayIterator<T>(this.data, this.size, false);
    }
    ArrayIterator<T> _end() {
        return ArrayIterator<T>(this.data, this.size, true);
    }
}
```

The standard library collections have iterators in the structure of this:

```qc
namespace Array
    namespace Iterator {
        class It {
            ...
        }
    }
    class Arr {
        Iterator::It _begin() {
            ...
        }
        ...
    }
}
```

## Try/Catch and Error Returns

C^4 supports both `try`/`catch` exception handling and multireturn/unions for error handling. You are encouraged to use both, or combine them where appropriate. Just document if you throw or not.
Example:

```qc
int main() {
    try {
        throw 123;
    } catch (int e) {
        ...
    }
}
```

Or for union-based:

```qc
struct myerror {
    ...
}
type MyResult = int | myerror;
MyResult myThing() {
    return myerror{...};
}
```

Nested try/catch blocks are not allowed becuase they just obfuscate what your code is doing without adding any new functionality.

## Defer

C^4 has `defer`, like Go and Zig.
Unlike Go's `defer`, C^4 `defer` does **not** participate in stack unwinding. Deferred code is not automatically executed when an exception propagates out of a scope.
This behavior is intentional: making `defer` participate in stack unwinding would introduce additional runtime overhead.
`defer` should not be considered a destructor mechanism. It is a scope-exit convenience feature, not RAII.

## Concepts

C^4's concepts are like a combination of Typescript interfaces, Rust traits, and C++ concepts.

```qc
concept Printable {
    1_of {
        void print();
        void print(Self self);
    }
    default {
class:
        void print() {
            `qout("Printing. . .");
        }
else:
        void print(Self self) {
            `qout ("Printing. . .");
        }
    }
}
class PDF {
    string data;
    PDF(string data) {
        this.data = data;
    }
    void print() {
        `qout("%s", this.data);
    }
}
class ASCII {
    string data;
    ASCII(string data) {
        this.data = data;
    }
}
struct Paper {
    string data;
}
type File = PDF | ASCII;
/// Print would print PDFs data
PDF proves Printable;
/// Print would print "Printing. . ."
ASCII proves Printable;
/// Print would print the paper's data
Paper proves Printable with_proof { // with_proof block to add additional definitions, like impl in Rust
    void print(Self self) {
        `qout("%s", self.data);
    }
}
/// Would print "Printing. . ."
File proves Printable;
```

The `Self` arguments are explicit for non-class methods added through concepts. This is to emphasize _structs are not classes, and should stick to being POD_. Concepts only allow methods on structs because it's better for DRY than having 50 functions with similar names.

## Simple File Example

```qc
namespace Exported {
    #include<Vector, std>
}
int main() {
    Vector::Vec<int> my_vec = [1, 2, 3];
    my_vec.push(123);
    `qout("%i", my_vec[2]);
    return 0;
}
```

---

## Self Hosted Runtime

There is currently an unknown bug with the self-hosted runtime.

# Benchmarks

All benchmarks were executed on Linux (`x86_64`) using `/usr/bin/time` across **500 runs per optimization tier** (4,000 total benchmark runs). Workload: identical compute-heavy arithmetic and bitwise algorithm global state.
Src can be found in the bench<...> files in the project root.

---

## 1. Executive Summary

| Category                        | Winner     | Advantage                                              |
| :------------------------------ | :--------- | :----------------------------------------------------- |
| Compilation Speed               | C^4 (`qc`) | ~1.85x faster (up to 47.3% lower latency)              |
| Compiler Memory (Peak RSS)      | C^4 (`qc`) | 31.6% less RAM (~60 MB leaner)                         |
| Compiler OS Page Faults         | C^4 (`qc`) | 68.4% fewer page faults (~10,000 fewer faults/compile) |
| Unoptimized Runtime (`-O0`)     | C^4 (`qc`) | 2.0x faster (50.0% reduction in execution time)        |
| Optimized Runtime (`-O1`–`-O3`) | Tie        | Exact Parity (Both clock identical 30.00 ms)           |
| Binary Runtime Memory           | C^4 (`qc`) | 44.7% less memory (2.11 MB vs 3.81 MB)                 |

## 2. Compilation Performance (500 Runs Averaged)

### Speed & CPU Overhead

| Opt Level | Compiler   | Elapsed Time | User CPU Time | Sys CPU Time | CPU Util | Relative Speed        |
| :-------- | :--------- | :----------- | :------------ | :----------- | :------- | :-------------------- |
| `-O0`     | Clang C++  | 301.54 ms    | 271.60 ms     | 23.82 ms     | 99.0%    | 1.00x                 |
|           | C^4 (`qc`) | 159.02 ms    | 95.40 ms      | 8.50 ms      | 68.7%    | 1.90x faster (-47.3%) |
| `-O1`     | Clang C++  | 313.14 ms    | 282.86 ms     | 23.64 ms     | 99.0%    | 1.00x                 |
|           | C^4 (`qc`) | 168.74 ms    | 48.28 ms      | 11.22 ms     | 39.0%    | 1.86x faster (-46.1%) |
| `-O2`     | Clang C++  | 314.12 ms    | 283.80 ms     | 24.12 ms     | 99.0%    | 1.00x                 |
|           | C^4 (`qc`) | 169.04 ms    | 49.26 ms      | 11.40 ms     | 39.5%    | 1.86x faster (-46.2%) |
| `-O3`     | Clang C++  | 315.72 ms    | 284.98 ms     | 23.88 ms     | 99.0%    | 1.00x                 |
|           | C^4 (`qc`) | 171.54 ms    | 55.04 ms      | 11.78 ms     | 42.6%    | 1.84x faster (-45.7%) |

> _Note: At `-O3`, C^4 uses 5.2x less User CPU time (55.04 ms vs 284.98 ms) by eliminating C++ preprocessor and template instantiation overhead._

### Memory & System Overhead

| Opt Level | Compiler   | Peak RSS (MB)          | Minor Page Faults | Major Faults | I/O (Out) |
| :-------- | :--------- | :--------------------- | :---------------- | :----------- | :-------- |
| `-O0`     | Clang C++  | 177.84 MB (182,110 KB) | 14,777.7          | 0            | 40.0      |
|           | C^4 (`qc`) | 129.70 MB (132,818 KB) | 5,080.1           | 0            | 416.0     |
|           | _Delta_    | _-27.1% RAM_           | _-65.6% faults_   | —            | —         |
| `-O1`     | Clang C++  | 177.97 MB (182,239 KB) | 14,802.2          | 0            | 40.0      |
|           | C^4 (`qc`) | 128.79 MB (131,886 KB) | 4,625.0           | 0            | 72.0      |
|           | _Delta_    | _-27.6% RAM_           | _-68.8% faults_   | —            | —         |
| `-O2`     | Clang C++  | 177.97 MB (182,244 KB) | 14,835.1          | 0            | 40.0      |
|           | C^4 (`qc`) | 128.75 MB (131,841 KB) | 4,627.8           | 0            | 72.0      |
|           | _Delta_    | _-27.6% RAM_           | _-68.8% faults_   | —            | —         |
| `-O3`     | Clang C++  | 188.09 MB (192,601 KB) | 14,635.2          | 0            | 40.0      |
|           | C^4 (`qc`) | 128.73 MB (131,819 KB) | 4,620.6           | 0            | 72.0      |
|           | _Delta_    | -31.6% RAM (-59.4 MB)  | -68.4% faults     | —            | —         |

---

## 3. Binary Runtime & Efficiency (500 Runs Averaged)

### Execution Time

| Opt Level | Clang (C++) Binary | C^4 Binary | Speedup / Delta              |
| :-------- | :----------------- | :--------- | :--------------------------- |
| `-O0`     | 60.00 ms           | 30.00 ms   | 2.00x faster (+100% speedup) |
| `-O1`     | 30.00 ms           | 30.00 ms   | Parity (1.00x)               |
| `-O2`     | 30.00 ms           | 30.00 ms   | Parity (1.00x)               |
| `-O3`     | 30.00 ms           | 30.00 ms   | Parity (1.00x)               |

### Runtime Memory & Footprint

| Opt Level | Binary     | Peak RSS (KB)   | Peak RSS (MB) | Minor Faults    |
| :-------- | :--------- | :-------------- | :------------ | :-------------- |
| `-O0`     | Clang C++  | 3,891.34 KB     | 3.80 MB       | 150.9           |
|           | C^4 (`qc`) | 2,334.83 KB     | 2.28 MB       | 97.9            |
|           | _Delta_    | _-39.9% memory_ | —             | _-35.1% faults_ |
| `-O1`     | Clang C++  | 3,892.55 KB     | 3.80 MB       | 150.7           |
|           | C^4 (`qc`) | 2,156.53 KB     | 2.11 MB       | 90.6            |
|           | _Delta_    | _-44.6% memory_ | —             | _-39.9% faults_ |
| `-O2`     | Clang C++  | 3,898.58 KB     | 3.81 MB       | 150.8           |
|           | C^4 (`qc`) |

## Known Limitations

&& and || do not short-circuit on unions.
Volatile does not exist on structs or class fields, and does not work on property accesses. In general, volatile is not 100%.

## Standard Library Namespaces:

```
Vector
List
Array
AdvQBool
Math
Utils
OSInterop
```

Made by [Luca Fazio](https://github.com/Youg-Otricked)
