---
paths:
  - "**/*.{cpp,cc,cxx}"
---

# Implementation file rules

- The first include is the file's own header, followed by a blank line. This verifies the header is self-contained.
- Include order after that: other project headers, third-party headers, standard library headers, each group separated by a blank line and sorted alphabetically.
- One implementation file per header, with the same base name.
- Define functions in the same order they are declared in the header.
- Open the project namespace (`namespace project { ... }`) rather than qualifying each definition.
- File-local helpers and constants go in an anonymous namespace, not `static`.
- `using namespace` only at function scope, except `using namespace std::literals;`, which is allowed at file scope.
- No global mutable state. If shared state is unavoidable, wrap it in a function-local static and document the thread-safety guarantees.
- Keep functions under roughly 40 lines; extract named helpers instead of adding comments to explain long blocks.
