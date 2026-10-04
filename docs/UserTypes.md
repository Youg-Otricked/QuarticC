<link rel="stylesheet" href="{{ '/assets/css/main.css' | relative_url }}">

# User Types

[Back to Home](./index.md)

### Types of User Types:

|////|Use|Syntax|Pro/Con|
|----|---|------|-------|
|Struct|Storing a group of variables|struct name { field-type field-name; ...};|Very useful and simple|
|Union |Defines a set of types or literals that a variable of this type can be|type union-name = type-orliteral1 \| type-orliteral2.....;|Amazingly powerful for much more than just making a variable of multiple types|
|Alias |Aliases a type to a name|type alias-name = type-to-alias;|Simple, just aliasing a name|
|Enum  |Stores a collection of name -> literal mappings|enum name = { fieldone = value; .....};|Can do something similar with unions, however enums are more usefull and explicit for this use.|

### Syntax And examples

Struct syntax is:
```cpp
struct struct-name {
    field-type field-name;
    ...
}
```

For Example
```cpp
struct Point {
    int x;
    int y;
    int z;
}
```

Union syntax is very simple. It is as follows
```cpp
type union-name = type1 | type2.....
```

For Example
```cpp
type Number = int | double | float | "1";
```

Alias syntax is

```cpp
type name = thing;
```

So
```cpp
type IntAlias = int;
```

And Enum syntax is

```cpp
enum name {
    name1 = value1;
    name2 = value2;... //If you don't set the values it will auto increment from 0 -> ...
};
```

So an example would be

```cpp
enum Status {
    ERROR = 1;
    TIMEOUT = 2;
    OK = 0;
};
```

Enums can also have _tags_, special fields of each member of the enum, such as

```cpp
enum Optional {
    Value(string);
    None;
};
```

Then you can extract the tags in a _match_ (a special kind of switch without fallthrough that requires every possible case be covered).

```cpp
Optional s = Optional.Value("Hello");
match (s) {
    Value(val) => `qout("%s", val);
    None => unreachable;
}
```
