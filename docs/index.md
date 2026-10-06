<link rel="stylesheet" href="{{ '/assets/css/main.css' | relative_url }}">

# Welcome To The Quartic-C Documentation!

```cpp
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

## Video

<div style="position: relative; padding-bottom: calc(51.0933% + 41px); height: 0; width: 100%; max-width: 900px; margin: 0 auto;">
  <iframe
    src="https://demo.arcade.software/YKKoQ8dvslZT7VIXJVgH?embed&embed_mobile=tab&embed_desktop=inline&show_copy_link=true"
    title="Run and Test C Code in Quartic C Playground"
    frameborder="0"
    loading="lazy"
    webkitallowfullscreen
    mozallowfullscreen
    allowfullscreen
    allow="clipboard-write"
    style="position: absolute; top: 0; left: 0; width: 100%; height: 100%; color-scheme: light;"
  ></iframe>
</div>

### Pages:

[Getting started](./getStart.md)

[Basics](./BasicSyntax.md)

[Control flow](./ControlFlow.md)

[Loops](./Loops.md)

[Functions](./Functions.md)

[Builtins](./BuiltInFunctions.md)

[Advanced function features](./AdvancedFunc.md)

[QBools](./QBools.md)

[Q Control Flow](./QFlow.md)

[User Types](./UserTypes.md)

[OOP](./OOP.md)

[Special Methods](./SpecialMethods.md)

[Inheritance](./inheritance.md)

[Namespaces](./namespaces.md)

[Multi File](./include.md)

[Conventions](./conventions.md)

[Variadics](./variadics.md)

[C Interop](./cinterop.md)

[Inline ASM](./asm.md)

[Manual Memory Managment](./MMM.md)

[Generics](./generics.md)

[Iterators](./iterators.md)

[Custom Runtimes](./runtimes.md)

[Concepts](./concepts.md)

[Defer](./defer.md)

[New Number Types](./newNums.md)

[Try/Catch](./trycatch.md)

[Tuples](./tuples.md)

## 'Philosophy'

- No hidden behavior
- No implicit magic (auto is highly discouraged)
- Errors should be as loud and early as possible, not console filling variant messes
- If something is complex, it is documented as complex

Quartic C favors clarity over convenience, and explicitness over brevity.

## Non-goals

- Quartic C is not trying to do 'hand-holding' or be beginner - friendly.  
- Quartic C does not hide memory costs
- Quartic C does not auto-correct ambiguous logic
- Quartic C does not _make_ ambiguous logic
- Quartic C does not _allow_ ambiguous logic
  `

## Versioning

QuarticC uses the following versioning scheme:
`cMa.Mo.MiP`
Where `c` is critical (massive additions, such as the compiler being added), `Ma` being major versions, tracking large collections of features, `Mo` being moderate versions, tracking collections of similar features, `Mi` being minor versions, which track individual feature milestones within the current moderate version's theme., and `P` being the patch version.
For the version
`x1.2.34`
`c` = `x`
`Ma` = `1`
`Mo` = `2`
`Mi` = `3`
`P` = `4`

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

### Legacy Versions

All legacy `v*` versions are deprecated. This includes the old release line up to `v12.0.0`.

### Current Versions

The current release line starts at `x0.15.0`.

Current `x*` versions are the non-deprecated versions going forward. For example, `x1.0.0` is part of the current version line and is distinct from the old legacy `v1.0.0`.`
