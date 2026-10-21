---
name: new-class
description: Scaffold a new C++ class with header, implementation file, unit test, and CMake registration following the project conventions. Use when adding a new class or component.
argument-hint: [ClassName] [module]
arguments: [class_name, module]
---

# New class

Create class `$class_name` in module `$module`.

1. Inspect the existing layout (where headers, sources and tests live, and how targets are defined in CMake) and follow it. If the module does not exist or the layout is unclear, ask before creating anything.
2. Derive the file base name by converting `$class_name` to snake_case.
3. Create the header, the implementation file, and the test file in the locations the existing layout uses. Each file type follows its rule in `.claude/rules/`.
4. Put the class in the project namespace, nested by module if existing code does so.
5. Start with the rule of zero. Add only the constructors and members that the request describes; leave a `// TODO` where behaviour is not specified rather than inventing it.
6. Write at least one test for each public member function that has defined behaviour.
7. Register the new source and test files explicitly in the relevant `CMakeLists.txt`.
8. Run `/verify` and report the files created.
