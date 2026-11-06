
<link rel="stylesheet" href="{{ '/assets/css/main.css' | relative_url }}">

# Try/Catch

[Back to Home](./index.md)

Errors are important to any programming language. C^4 has a few ways of handling them.

1. panic

Panic is a unrecoverable error, useable for things such as out-of-bounds indexing.

```qc
panic "my msg"
```

This crashes the program. It's also usefull for asserts.

```qc
void assert(bool condition, string msg) {
    if (!condition) panic msg;
}
```

2. try/catch

C^4 uses errors-as-values, meaning all errors are just normal values. Errors are just ANY type with a integer at field 0. That's unions, enums, and any custom type too.
The value is considered an error if the field 0 is != 1.

You use these rules with the two error handling primitives: `try`, and `catch`.

`try` can be used on a error value, and will pass the error through if it is a error and keeps going if not.


```qc
type MyError = int | Err; // Err is a error (not type 0)
type MyFun = double | Err;
MyFun func() {
    int x = try errorReturn(); // if it returns Err, it passes down and basically becomes return errorReturn(), if it doesn't int x gets the int.
    ...
}
```

`catch` can be used to handle the error, for example:

```qc
type MyError = ...
... {
    int x = func() catch 1; // defaults to 1 if error.
    x = myFunc() catch (e) { // captures error
        ...
        myValue; // sets x to this value
    }
}
```

Catch can either end with a expression stmt or terminate the function.
