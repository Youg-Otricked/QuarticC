<link rel="stylesheet" href="{{ '/assets/css/main.css' | relative_url }}">

# Generics

[Back to Home](./index.md)

QuarticC generics allow types and functions to work with multiple different types while keeping compile-time type safety.

Generics are available on:

- Enums
- Classes
- Structs
- Unions
- Type aliases
- Functions
- Methods
<details>
<summary>For the nerds:</summary>
<p>Generics are Monomorphised</p>
</details>


## Basic Generics

A generic type is declared using angle brackets with the type-names in the center:

```qc
class Box<T> {
    T value;

    Box(T value) {
        this->value = value;
    }
}

int main() {
    Box<int> box = Box<int>(123);
}
```

Another example:

```qc
class Pair<A, B> {
    A first;
    B second;

    Pair(A first, B second) {
        this->first = first;
        this->second = second;
    }
}

Pair<int, string> p = Pair<int, string>(123, "hello");
```

---

# Generic Constraints

Generic parameters can have constraints to restrict what types are allowed.

The syntax is:

```qc
<T([constraint]:[[!]<subconstraints>])>
```

For example:

```qc
class NumberBox<T(numeric:)> {
    T value;
}
```

This means `T` must be a numeric type.

Available built-in constraints include:

| Constraint  | Meaning               |
| ----------- | --------------------- |
| `usertype`  | Any user-defined type |
| `primitive` | Any primitive type    |
| `pointer`   | Any pointer type      |
| `numeric`   | Any numeric type      |

---

# Subconstraints

Generic constraints can also have _subconstraints_, which either restrict allowed types or exclude specific types.
Syntax:

```qc
<T(:!Type)>
```
The `!` means "not this type".

Example:

```qc
class NotInt<T(:!int)> {
    T value;
}
```

This allows any type except `int`.
Multiple types can be included or excluded using `|`.
Example:
```qc
<T(:!int|string)>
```
means:
> T cannot be int or string.

---

# Combining Constraints

Constraints and exclusions can be combined.
Example:

```qc
<T(numeric:!int|float)>
```

This means:

- `T` must be numeric
- `T` cannot be `int`
- `T` cannot be `float`

---

# Non-Type Generic Parameters

QuarticC also supports generic parameters that are values known at compile time.

Example:

```qc
class Array<T, int Size> {
    T data[Size];
}
```

`Size` is not a type. It is a compile-time integer parameter.

Usage:

```qc
Array<int, 32> numbers;
```

The compiler knows the size during compilation.

---

# Generic Functions

Functions can also use generics.

Example:

```qc
T max<T(numeric:)>(T a, T b) {
    if (a > b) {
        return a;
    }
    return b;
}
```

Usage:

```qc
int x = max<int>(10, 20);
double y = max<double>(1.5, 2.5);
```
Because QuarticC prioritizes explicitness, generic parameters are not inferred. You must provide them manually.

---

# Generic Methods

Methods can have generic parameters independently from their class.
Example:

```qc
class Printer {
    T print<T>(T value) {
        `qout("%s", value);
        return value;
    }
}
```

---

# Generic Type Aliases

Aliases can also use generics.
Example:

```qc
type Pointer<T> = T*;

Pointer<int> x;
```

This creates:

```qc
int* x;
```

---

# Why QuarticC Generics Are Different

QuarticC generics use constraint-based typing.
Other languages often express simple requirements using large collections of traits, concepts, or template checks.
For example, a numeric constraint in Rust may require listing many traits:

```rust
T: Add + Sub + Mul + Div + PartialOrd + Copy
```

In QuarticC:

```qc
T(numeric:)
```

expresses the intent directly.
The compiler understands what "numeric" means instead of requiring the programmer to manually describe every required operation.

---

# Generic Naming

By convention, generic parameters use short uppercase names:

```qc
class Array<T, int Size>
```

Common names:

| Name          | Meaning           |
| ------------- | ----------------- |
| `T`           | General type      |
| `A`, `B`      | Additional types  |
| `K`           | Key type          |
| `V`           | Value type        |
| `S`, `Size`   | Compile-time size |

However, generic parameter names are normal identifiers and follow the same rules as other names.
Specifically, generic parameters may use either the constant or usertype casing rules.

---

# Example

```qc
class Array<T, int S = 0> {
    T* data;
    int size;
    Array() {
        this->data = nullptr;
        this->size = 0;
    }
    T get(int index) {
        return this->data[index];
    }
}
int main() {
    Array<int, 10> numbers;
    Array<string> names;
}
```

# Comparison

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
