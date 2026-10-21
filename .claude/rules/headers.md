---
paths:
  - "**/*.{h,hpp,hxx,ipp}"
---

# Header file rules

- Start every header with `#pragma once`.
- Headers must be self-contained: they compile on their own and include everything they use.
- Keep includes minimal. Prefer forward declarations over includes when only pointers or references to a type are needed. Never include `<iostream>` in a header.
- No `using namespace` and no namespace aliases at namespace scope.
- No anonymous namespaces and no `static` free functions or variables in headers.
- Namespace-scope constants are `inline constexpr`.
- Only declarations, templates, concepts, `constexpr`/`inline` functions, and trivial one-line member functions belong here. Everything else goes in the matching `.cpp`.
- Put implementation details that must live in a header (template helpers) in a nested `detail` namespace.
- Class layout order: `public`, then `protected`, then `private`. Within each section: types and aliases, constructors and destructor, special members, operators, member functions, data members. Exception: plain aggregates (all-public structs used with designated initializers, such as `Vector3` or `Rgb`) list their data members first, since the members are their interface.
- Mark functions `[[nodiscard]]`, `noexcept`, `const` and `explicit` on the declaration.
- Explicitly `= default` or `= delete` special members when the rule of zero does not apply.
- Document every public type and function with a brief `///` comment stating purpose, preconditions, and what it throws.
- File name matches the main type in snake_case: `TokenStream` lives in `token_stream.hpp`.
