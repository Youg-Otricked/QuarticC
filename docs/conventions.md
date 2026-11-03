<link rel="stylesheet" href="{{ '/assets/css/main.css' | relative_url }}">

# Conventions

[Back to Home](./index.md)

QuarticC has rather unique naming conventions:

| **Type**                               | **Convention**         | **Why?**                                                                                                                     |
| -------------------------------------- | ---------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| **Variables**                          | `snake_case`           | It's familiar to C++, C, Zig, Go, and Rust devs.                                                                             |
| **Functions**                          | `camelCase`            | It allows for instant knowledge between if an identifier is a var, or function (lambdas use var casing, not function casing) |
| **User Types**                         | `PascalCase`           | It is common across basically every programming language.                                                                    |
| **Constants**                          | `SCREAMING_SNAKE_CASE` | Same as above.                                                                                                               |
| **Private Member Variables**           | `__snake_case`         | Variable case prepended with __. Most underscores.                                                                           |
| **Protected Member Variables**         | `_snake_case`          | Less underscores.                                                                                                            |
| **Protected Methods**                  | `__camelCase`          | Unique casing, more underscores.                                                                                             |
| **Private Methods**                    | `camel_Snake_Case`     | Function casing, more underscores.                                                                                           |
| **Namespaces**                         | `PascalCase`           | Same as user types.                                                                                                          |
| **Namespaces Not Meant For Inclusion** | `Pascal_Snake_Case`    | Unique casing style, more underscores, you have to be trying to include this.                                                |
| **Global Scope Functions**             | `camel_Snake_Case`     | Unique casing style, more underscores, similarity to private methods is intentional, because global scope cannot be included.|
| **Methods Used By Compiler**           | `_camelCase`           | Different from everything else. (Special methods recognized by the compiler (for example iterator methods).)                 | 
| **Compiler Reserved**                  | `_qc_, __qc_ and qc_`  | Unique, hard to use accidently                                                                                               |
| **Compiler Intrinsics**                | \` + `snake_case`      | Clearly distinguishes compiler intrinsics from user-defined functions.                                                       |

General formatting recommendations:

- Maximum line length: approximately 120 characters relative to the current indentation.
- Tabs or spaces are both acceptable.
- Use LF line endings.
- `//` for comments.
- `///` for documentation comments.
- `//!` for file-level documentation.
- File paths are written without quotes.
- Place everything except `main` inside a namespace when practical.
- `//?` for end-of-file faq/implementation notes

A namespace should generally contain one of the following:
1. Namespaces should do one thing well, similar to the UNIX philosophy, 
2. Namespaces should have either:
        1. one type (or group of TIGHTLY related types, eg bigints) and their core helpers,
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
Pointer asterisks bind to the type rather than the variable. The final * belongs to the declarator, unless its a function return type. Then its all on the type.
```
int** *x;
int* ptr_add(int *p) ...
```
Files are `kebab-case` (optional, sometimes I dont follow this)
QuarticC naming conventions are designed to make code readable without requiring the reader to inspect library code. Names should provide immediate context.

## Philosophy

> RTFM once, not RTMSCE5S (Read The Manual and Source Code Every 5 Seconds).

Names should provide enough context that readers rarely need to inspect library implementations to understand their role.

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
namespace Nothing_Illegal_I_Promise { // Intentionally formatted as a non-inclusion namespace.
    // If users don't want to type this, they probably shouldn't be including it.
    ...
}
```

