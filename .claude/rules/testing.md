---
paths:
  - "tests/**/*"
  - "**/*_test.cpp"
---

# Test file rules

- Framework: GoogleTest (`GTest::gtest_main`). Register every test file explicitly in `tests/CMakeLists.txt`.
- Shared test helpers live in `tests/support/` in namespace `albedo::test_support`, e.g. `expectNear` for component-wise tolerance checks.
- Check precondition `assert`s with `EXPECT_DEBUG_DEATH` in a test suite named `<Type>DeathTest`.
- Check that something must not compile with a concept wrapping a `requires` expression plus `static_assert(!Concept<...>)`.

- Test files mirror the source tree: `src/parser/lexer.cpp` is tested in `tests/parser/lexer_test.cpp`.
- Test names describe behaviour in camelCase: `parsesEmptyInput`, `throwsOnInvalidHeader`.
- One behaviour per test, structured as arrange, act, assert.
- Test through the public interface only. Never use `#define private public` or friend declarations added for testing.
- Cover the edge cases: empty input, boundaries, invalid input, error and exception paths.
- Tests are deterministic and independent: no execution-order dependencies, no `sleep`, no real network access, fixed seeds for random data.
- File-system tests use a temporary directory and clean up after themselves.
- Compare floating-point values with a tolerance, never with `==`.
- Prefer small hand-written fakes over mocking frameworks unless the interface is large.
- Never delete, disable or weaken an existing test to make it pass. If a test looks wrong, say so and ask.
- Run the full suite under AddressSanitizer and UndefinedBehaviorSanitizer before reporting done.
