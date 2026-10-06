- VTables not needing in-decl-order
- Including C Headers (which means adding macro functions (c style) and type def (for compatiblity))
- const pointers
- const params
- const stuff correctness
- const methods fore const obhects
- static & inline functions
- Upgrade Include System
- Variadic Generics (`class VaradicTypes<...Tys> { Tys elems; ...`) (not decided on syntax) 
- Operator.
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
- user defined literals (macros baby!)
- user defined macro functions
- user defined metadata tags
- Compile-time assertions (assert, static_assert)
- Decltype (get type of expr)
- basically eextesion to make C^4 verilog output
- noexcept (funciton modifier) (same update as const on functions and stuff)
- change parent constructor and delegeating constructors
- dynamic field access
- has field, has method
- _generic- manal overloading
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
- real u64 (ai64u_t (always i64 unsigned type)), u32 (unsigned int), u16 (unsigned short int) types
- true i64 (ai64_t (always i64 type))
- panic, assume
- reflection, ^^ changes purposes
- _ unsued variable
- comptime-trycatch (catches compiel verrors)
TOP PRIORITY:
1. VTables allowed to not be in decl order.
1. Direct C header includes (and thus, ocnst correctness)
2. CQB
3. Other stuff (private/protected inheritance) + Variadic Generics, EVEN FANCIER OPERATOR OVERLOADS
4. Metadata
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
5. Preproccessers
#line - somthing idk
6. User-Defined metadata
7. User-Defined macro _functions_ (the only good thing in rust)
8. User defined literals
9. Labels & goto/br
10. co_await
11. rest of featuers im forgetting
12. self host
13. Direct _C++_ header includes (and thus, the rest of const stuff), but not all features (obviously, i don't hate myself).
