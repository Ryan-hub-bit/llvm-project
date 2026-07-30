# LLVM Static GT Generation

This repository is a research fork of the official
[LLVM project](https://github.com/llvm/llvm-project).

It extends Clang and the LLVM X86 backend to insert type-aware labels into
ELF binaries. These labels provide static ground truth for control-flow graph
analysis. Label generation is enabled by default and does not require the old
`-fmatch-indirect-call` switch.

## Collected edges

The labels support collecting four kinds of edges:

| Edge type | Relationship |
| --- | --- |
| `ret` | function return block to the block after a call |
| `jumptable` | jump-table dispatch block to its target blocks |
| `tailcall` | indirect tail-call block to compatible target functions |
| `indirectcall` | indirect-call block to compatible target functions |

Indirect-call and indirect-tail-call targets are matched using the type ID
encoded in the call-site and function-entry labels. Direct tail-call labels are
also emitted for return-path analysis.

## Build

```bash
cmake -S llvm -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_PROJECTS=clang \
  -DLLVM_TARGETS_TO_BUILD=X86

cmake --build build --target clang llvm-nm -- -j$(nproc)
```

Compile a program with the instrumented Clang:

```bash
build/bin/clang -O2 -g example.c -o example
```

## Label examples

Labels are stored as symbols in the generated object files and binaries:

```bash
build/bin/llvm-nm -a example | grep -- '-t-'
```

Example labels:

```text
main.c-1-0-0-0-0-t-0-0-0-0-0-type
main.c-0-0-1-54cb21d76569286d-0-t-0-0-0-0-0-type
tailcall.c-0-1-0-54cb21d76569286d-0-t-0-0-0-0-0-type
main.c-0-0-0-0-0-t-0-0-0-1-48c0a6bd1ef5201f-54cb21d76569286d
```

These examples represent a jump-table source, an indirect-call site, an
indirect-tail-call site, and a compatible function entry.

## Static GT extraction

This repository only generates labels during compilation. For label parsing,
basic-block recovery, four-type Static GT extraction, and graph construction,
refer to [NeuJump](https://github.com/Ryan-hub-bit/neujump).
