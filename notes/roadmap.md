- Variadic Generics (`class VaradicTypes<...Tys> { Tys elems; ...`) (not decided on syntax) 
- Operator.
- Function pointers
- Private/Protected inheritance
- Constexpr
- Debug
- Cqb lib in stl. Ci and build.
- qcheck (QC Lint(qc linter. duh)
- qconform (QC Formatter)
- `__attributes__((...))`
- const params/returns and const correct methods
- Copy VS Move assignment (no move only, im not rust), to make things faster.
- more error stuff if i feel like it
- destructuring (eg `Pair {abc, def} = ...`, operator{} which is your destructure handler (multi return baby!) (maybe))
- tuples (like zig/rust tuples, .0 .1 .2 .3)
- user defined literals (macros baby!)
- user defined macro functions
- user defined metadata tags
- Compile-time assertions (assert, static_assert)
- Decltype (get type of expr)
- noexcept (funciton modifier) (same update as const on functions and stuff)
- change parent constructor and delegeating constructors
- dynamic field access
- has field, has method
- _generic- manal overloading
- rust like tags for enums, match.
- cpp contracts
- has_method, has_field, at
- rule usertypes so userdefined const equiveleants and stuff
- conversion operators (operator int, .... I already have _eval (to bool) and _repr (to string))
- operator co_await once i add co_await
- operator cast<T>
    `cast primitive operator overload
- operator imp_cast<T>
    implicit cast operator overload
- alloca builtin
- unreachable
- panic, assume
TOP PRIORITY:
1. Generics in header
2. Tuples + Destruturing
1. Function Pointers
1. Tagged enums, match (can only extract tags in match)
3. CQB
4. Other stuff (private/protected inheritance) + Variadic Generics, EVEN FANCIER OPERATOR OVERLOADS
5. Metadata
?likely? - likely marked
?unlikely? - marked unlikely
?inline? - pls inline >-<
?inline(always)? - always inline
?inline(never)? -  NEVER inline
?nodiscard? - warn if discard result
?noreturn? - optimize away post-call cleanup
?deprecated(msg)? - warning with this message when used
?fully_deprecated(msg)? - ERROR with this message when used
?errordiscard? - nodiscard but errors not warns
?consteval? - evaluate at compile time
?qc_ver_eq(version)? - errors if qc ver is != version
?qc_ver_lt(version)? - errors if qc ver is >= version
?qc_ver_gt(version)? - errors if qc ver is <= version
?qc_ver_lte(version)? - errors if qc ver is > version
?qc_ver_gte(version)? - errors if qc ver is < version
?since(ver)? - states the version of the package this is for
?until(ver)? - states when this will be deprecated
?experimental? - states that this tool is janky, subject to change, or flaky/expremintal
?unstable? - states this tool is janky, flaky, or unstable, but api won't change
?sentinel(value, msg, value, msg....)? - states this tool returns sentinel values with special meaning
6. Preproccessers
#line - somthing idk
7. User-Defined metadata
8. User-Defined macro _functions_ (the only good thing in rust)
9. User defined literals
10. Labels & goto/br
11. co_await
