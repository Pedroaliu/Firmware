# Jixia Code Style and Formatting

## 1. Canonical formatter

Jixia C and C++ formatting is defined by the repository-root `.clang-format` file.

The selected brace style is attached braces:

```cpp
bool ready() {
    if (condition) {
        return true;
    }

    return false;
}
```

CLion should use the repository `.clang-format` rather than an IDE-only personal style. The project does not rely on each developer manually reproducing the formatting rules.

Assembly (`.S`) is intentionally excluded from automatic clang-format enforcement. Privileged entry code often uses hand-maintained layout to expose register ownership, trap state, and control-flow boundaries clearly.

## 2. Before committing

Stage the intended patch, then run:

```bash
git add <files>
bash scripts/pre-commit-check.sh
```

The pre-commit check performs:

```text
git diff --cached --check
    -> trailing whitespace / patch hygiene

scripts/check-format.sh --cached
    -> clang-format validation for staged C/C++ lines
```

If only formatting needs repair:

```bash
bash scripts/check-format.sh --cached --fix
```

Review the resulting diff and stage the repaired files again before committing.

## 3. Incremental adoption rule

Jixia adopted `.clang-format` after source already existed. To avoid a repository-wide mechanical rewrite mixed with architectural work, CI checks changed C/C++ lines rather than forcing every untouched legacy line to be reformatted immediately.

This gives the repository a monotonic rule:

```text
untouched historical code
    -> may retain old formatting temporarily

new or modified C/C++ lines
    -> must satisfy .clang-format
```

A later dedicated mechanical cleanup may run clang-format over the full tree if desired. Such a sweep should be isolated from functional changes.

## 4. CI enforcement

GitHub Actions determines the merge-base against `main` for milestone/feature branches and checks:

```text
git diff --check
changed-line clang-format
normal build/regressions
milestone acceptance tests
```

Formatting therefore has two layers:

```text
local pre-commit check
        ↓
GitHub CI gate
```

CI is the authoritative repository-wide backstop. Local checks exist to catch the same class of problem before a push.

## 5. Scope

Automatic clang-format enforcement covers:

```text
.c
.cc
.cpp
.cxx
.h
.hh
.hpp
.hxx
```

It does not currently format:

```text
.S
CMake
shell scripts
Markdown
```

Those files are still subject to `git diff --check` and normal review.
