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

Label format:

```text
module-JTSourceID-ITailCallID-ICallSiteID-CalleeTypeID-DTailCallID-t-JTTargetID-JTEntryID-ReturnID-FunctionEntryID-FunctionHash-FunctionTypeID
```

The fields separated by `-` are:

| Field | Example value | Meaning |
| --- | --- | --- |
| `module` | `main.c` | source module or file name |
| `JTSourceID` | `0` | not a jump-table source |
| `ITailCallID` | `0` | not an indirect tail call |
| `ICallSiteID` | `1` | the first indirect-call site |
| `CalleeTypeID` | `54cb21d76569286d` | expected callee type hash |
| `DTailCallID` | `0` | not a direct tail call |
| `t` | `t` | separator between source and target fields |
| `JTTargetID` | `0` | not a jump-table entry |
| `JTEntryID` | `0` | no jump-table entry index |
| `ReturnID` | `0` | not a return label |
| `FunctionEntryID` | `0` | not a function-entry label |
| `FunctionHash` | `0` | no function hash attached |
| `FunctionTypeID` | `type` | default placeholder; no function type attached |

`0` means that a field does not apply to this label. IDs start from `1`;
type and function hashes are hexadecimal values. `type` is the default
placeholder when no function type is attached.

Example of an indirect-call label (`ICallSiteID=1`) and its instruction:

```text
main.c-0-0-1-54cb21d76569286d-0-t-0-0-0-0-0-type
401257: ff d0    call *%rax
```

## Static GT extraction

This repository only generates labels during compilation. For label parsing,
basic-block recovery, four-type Static GT extraction, and graph construction,
refer to [NeuJump](https://github.com/Ryan-hub-bit/neujump).
