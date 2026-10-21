---
name: debug-crash
description: Diagnose and fix a crash, hang, or sanitizer report in the C++ code. Use when a test or binary segfaults, aborts, hangs, or AddressSanitizer, UndefinedBehaviorSanitizer or ThreadSanitizer reports an error.
---

# Debug a crash

1. Reproduce with the smallest command possible (a single test via `ctest -R <name>` or one binary invocation). Record the exact command.
2. Make sure the reproduction runs in a sanitizer build (`build-asan/`). For suspected data races, configure a separate `build-tsan/` with `-fsanitize=thread`; TSan cannot be combined with ASan.
3. Read the full report. Work from the first error reported, not the last; later errors are often consequences.
4. For crashes without a sanitizer report, get a backtrace: `gdb -batch -ex run -ex bt --args <command>` (or `lldb --batch -o run -o bt -- <command>`).
5. State a hypothesis about the root cause in terms of lifetime, ownership, bounds, initialization, or concurrency, and point to the lines involved.
6. Write a regression test that fails for the current code before changing it.
7. Fix the root cause. Do not add null checks, catch-alls, or sanitizer suppressions that only hide the symptom.
8. Confirm the regression test passes, then run `/verify`.

Summarize the cause, the fix, and the test that now guards it.
