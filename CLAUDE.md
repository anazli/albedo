# CLAUDE.md

Guidance for Claude when working in this repository.

## Project

<!-- One or two sentences on what the project does and its main components. -->
Personal C++ project. Language standard: **C++20** (no compiler extensions).

## Build and test

```bash
# Configure (Debug)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
cmake --build build -j

# Run tests
ctest --test-dir build --output-on-failure

# Format and lint
clang-format -i <files>
clang-tidy -p build <files>
```

Always build and run the tests after a change. Do not report a task as done if the build fails or tests are red.

## Guiding principles

- Follow the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines). When a rule here and the Guidelines conflict, this file wins (mainly naming).
- Prefer clarity over cleverness. Write code that states intent.
- Prefer the standard library over hand-rolled code or new dependencies. Ask before adding a third-party library.
- Make the smallest change that solves the problem; do not refactor unrelated code.
- Write cross platform code that runs on both linux and windows.

## Naming

| Element                         | Convention        | Example                       |
|---------------------------------|-------------------|-------------------------------|
| Functions and member functions  | camelCase         | `parseHeader()`, `getSize()`  |
| Local variables and parameters  | snake_case        | `buffer_size`, `file_path`    |
| Data members                    | snake_case        | `item_count`                  |
| Types (classes, structs, enums, concepts, aliases) | PascalCase | `TokenStream`, `Hashable` |
| Enumerators                     | PascalCase        | `Color::DarkRed`              |
| Constants and `constexpr` values| snake_case        | `max_retries`                 |
| Namespaces                      | snake_case        | `net::http`                   |
| Template parameters             | PascalCase        | `typename ValueType`          |
| Macros (avoid them)             | UPPER_SNAKE_CASE  | `PROJECT_ASSERT`              |
| Files                           | snake_case        | `token_stream.hpp`, `.cpp`    |

Do not use Hungarian notation or `m_` prefixes.

## Modern C++20 usage

Use these features where they improve clarity or safety:

- **Concepts** and `requires` clauses to constrain templates instead of SFINAE or `enable_if`.
- **Ranges** (`std::ranges::sort`, views, pipelines) instead of iterator-pair algorithms where readable.
- **`std::span`** for non-owning views of contiguous data; **`std::string_view`** for read-only strings.
- **`std::format`** for string formatting instead of `printf` or stream manipulators.
- **Three-way comparison** (`operator<=> = default`) and defaulted `operator==`.
- **`constexpr` / `consteval` / `constinit`** to move work to compile time where sensible.
- **Designated initializers** for aggregates.
- **`std::jthread`** and `std::stop_token` instead of raw `std::thread`.
- **`[[nodiscard]]`**, `[[likely]]`/`[[unlikely]]` (only when measured), `[[maybe_unused]]`.
- `std::optional`, `std::variant`, `std::expected`-style results are preferred to sentinel values.
- Use `enum class`, never plain `enum`.
- Use `auto` when the type is obvious or verbose; spell out types when it aids readability.
- Use structured bindings and range-based `for` loops.

Not yet: C++20 modules and coroutines, unless a task explicitly requires them (tooling support varies). Do not use C++23 features (`std::print`, `std::expected`, deducing `this`).

## Resource and memory management

- RAII for every resource. No naked `new`/`delete`, `malloc`/`free`.
- Ownership: `std::unique_ptr` by default, `std::shared_ptr` only for genuinely shared ownership. Create with `std::make_unique` / `std::make_shared`.
- Raw pointers and references are non-owning only. Prefer references when null is not valid.
- Follow the rule of zero; if a type must manage a resource, follow the rule of five.
- Prefer `std::vector` and `std::array` over C arrays.

## Correctness and safety

- `const` by default: variables, member functions, parameters passed by reference.
- Pass cheap types by value, others by `const&`; use `&&` only for sink parameters or forwarding.
- Initialize every variable at declaration; prefer brace initialization.
- Mark single-argument constructors `explicit`.
- Mark overrides with `override` (or `final`); never repeat `virtual` with it.
- No C-style casts; use `static_cast` etc. Avoid `reinterpret_cast` and `const_cast`.
- Use `std::size_t` / signed types consistently; avoid signed/unsigned mixing.
- Avoid global mutable state.
- No `using namespace` in headers.

## Error handling

- Use exceptions for errors that cannot be handled locally; use `std::optional` / `std::variant` for expected failure paths.
- Mark functions `noexcept` when they cannot throw (destructors, move operations, swaps).
- Never ignore a returned error; use `[[nodiscard]]` on functions whose result must be checked.
- Use `assert` / `static_assert` for invariants and preconditions.

## Formatting and tooling

- Code is formatted by `.clang-format`; do not hand-format against it.
- `clang-tidy` checks (`cppcoreguidelines-*`, `modernize-*`, `bugprone-*`, `performance-*`, `readability-*`) should pass on changed code.
- Compile cleanly with `-Wall -Wextra -Wpedantic -Werror` (or `/W4 /WX` on MSVC).
- Debug builds should run with AddressSanitizer and UndefinedBehaviorSanitizer enabled.

## Testing

- Every new feature or bug fix comes with tests. Test-file conventions are in `.claude/rules/testing.md`.

## File-specific rules

Rules for headers, implementation files, tests and CMake live in `.claude/rules/` and load automatically when matching files are read or edited.

## Comments and documentation

- Comment *why*, not *what*. Remove dead or commented-out code.
- Document public APIs with brief Doxygen-style comments (`///`).
- Explain non-obvious decisions and trade-offs where they are made.

## Working rules for Claude

- Read the surrounding code before editing and match its conventions.
- Never hardcode credentials, tokens, or machine-specific paths.
- Flag security-relevant changes (input parsing, file and network I/O, memory handling) in your summary.
- Every C++/CMake change is reviewed by the read-only `cpp-reviewer` agent (`.claude/agents/cpp-reviewer.md`) once build and tests pass, and before it is committed. Hooks enforce this: the Stop hook requests the review, and `git commit` is refused while the change differs from what was last reviewed. Pass the reviewer the `git diff HEAD` output and new file paths, since it cannot run git. Fix blocking findings, fix should-fix findings unless there is a concrete reason not to, and report all findings and the verdict.
- If a requirement is unclear, ask instead of guessing.
- Keep commits focused; do not commit, push, or modify CI configuration without being asked.
