<link rel="stylesheet" href="{{ '/assets/css/main.css' | relative_url }}">

# Multi File

[Back to Home](./index.md)

## Including

Ok, time to info dump. Quartic C has a _**Very confusing**_ but _Very **Intresting** and **Very Powerful**_ include system.
In Quartic C, you include with the syntax:
```cpp
namespace Exported {
    #include <namespacename, path/to/file>
}
```

Let me break this down step-by-step. First, the line that actualy includes: `#include`.

In Quartic C you do not include files. instead, you include _namespaces_ FROM files. You cannot include code from the global scope in Quartic C, like the main function. You can only include namespaces from files.

Now, there are _Certanly_ at least 2 people saying _"Oh but what if we want to use the global scope instead of this namespace for X or Y reason, like DRY code?_". Quartic C has an answer for this. Introducing, the Exported namespace.

The exported namespace is a top level namespace _That's right **Top level**. That means that you **really should** put the exported namespace without nesting._ This is because it breaks all forms of nesting and dependancy logic (in the interpreter), so it should be the **First thing** in the file. Not the first top level thing: the **FIRST THING** What goes in the exported namespace, you may ask? The answer is 1 thing.
- Include statements.

Just to restate this:

The `Exported` namespace _**MUST**_:
- Be the _**Very First Thing** in the file other than comments.
- NOT be nested in any other namespaces

The `Exported` namespace should _**only** contain_:
- `#include` statements

It must be first because if you use a dependancy BEFORE it's incldued, then it won't be defined.
The exported namespace is _**Automatically merged with your exported namespace on include and run**_, and collisions are last-defined-wins (hence avoiding vars/functions in exported namespace)
The reason for include statments going in the exported namespace? Dependancys. Let's say we have 3 files.

`file-a.qc`
```cpp
namespace Exported {
}
namespace Thingy {
    // blah blah blah
}
```

`file-b.qc`
```cpp
namespace Exported {
    #include <Thingy, ./file-a.qc>
}
namespace OtherThingy {
    // blah blah blah
}
```

`file-c.qc`
```cpp
namespace Exported {
    #include <OtherThingy, ./file-b.qc>
}
// blah blah blah
```

See the problem that would happen if includes were not in the `Exported` namespace? Because `file-b` uses `file-a`, if you included `file-b` and it didn't have the include from `file-a` in it's exported namespace, it wouldn't be included, most likely **breaking** all the code in `file-b`, which is why we put it there. The resolve order in the above would be:

> Include stack is []
> See `file-c` `Exported` namespace
> In exported of `file-c` see include for `OtherThingy` of `./file-b`
> > Scan `file-b` for `OtherThingy` and `Exported` namespace
> > Merge the `file-b` exported namespace with `file-c` exported and save the include to a stack (include stack is now: [`OtherThingy` -> `file-b`])
> > In merged exported: see include for `Thingy` of `./file-a`.
> >> Scan `file-a` for `Thingy` and `Exported`
> >> Merge `Exported` and push `Thingy` to stack (stack is now: [`Thingy` -> `file-a`. `OtherThingy` -> `file-b`])
> >> Proccess Exported and see nothing new.
> >
> > Pop include stack and proccess `Thingy` Include from `file-a`. (include stack is now: [`OtherThingy` -> `file-b`])
>
> Pop include stack and proccess `OtherThingy` Include from `file-b`. (include stack is now: [])
> Include stack is now empty. No more includes. Runs code.

The list of namespaces would be in this order:
```cpp
namespace Exported (file-c)
namespace Exported (file-b)
namespace Exported (file-a)
namespace Thingy
namespace OtherThingy
file-c code
```
