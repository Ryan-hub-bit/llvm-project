# LLVM Static GT Generation

This repository is a research fork of the official
[LLVM project](https://github.com/llvm/llvm-project).

It extends Clang and the LLVM X86 backend to insert type-aware labels into
ELF binaries. These labels provide static ground truth for control-flow graph
analysis.

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

## Usage

Clone and build this LLVM fork:

```bash
git clone git@github.com:Ryan-hub-bit/llvm-project.git
cd llvm-project

cmake -S llvm -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_PROJECTS=clang \
  -DLLVM_TARGETS_TO_BUILD=X86

cmake --build build --target clang llvm-nm -- -j$(nproc)
```

Use the newly built Clang instead of the compiler currently selected in this
shell:

```bash
export LLVM_GT_BUILD="$(pwd)/build"
export PATH="$LLVM_GT_BUILD/bin:$PATH"
export CC="$LLVM_GT_BUILD/bin/clang"
export CXX="$LLVM_GT_BUILD/bin/clang++"

command -v clang
clang --version
```

Compile a single source file:

```bash
"$CC" -O2 -g example.c -o example
```

Compile an existing CMake project in a new build directory:

```bash
cmake -S /path/to/project -B /path/to/project-build-gt \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$CC" \
  -DCMAKE_CXX_COMPILER="$CXX"

cmake --build /path/to/project-build-gt -- -j$(nproc)
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
| `JTSourceID` | `0` | `0`: not a jump-table source; `1+`: jump-table source ID |
| `ITailCallID` | `0` | `0`: not an indirect tail call; `1+`: indirect tail-call site ID |
| `ICallSiteID` | `1` | `0`: not an indirect call; `1`: first call site; `2+`: later sites |
| `CalleeTypeID` | `54cb21d76569286d` | `0`: no callee type; nonzero hex: expected callee type hash |
| `DTailCallID` | `0` | `0`: not a direct tail call; `1+`: direct tail-call site ID |
| `t` | `t` | separator between source and target fields |
| `JTTargetID` | `0` | `0`: not an entry; `1+`: ID of the source jump table |
| `JTEntryID` | `0` | `0`: no entry; `1`: first entry; `2+`: later entries |
| `ReturnID` | `0` | `0`: not a return; `1`: first return; `2+`: later returns |
| `FunctionEntryID` | `0` | `0`: not a function entry; `1`: function entry |
| `FunctionHash` | `0` | `0`: no function hash; nonzero hex: function-name hash |
| `FunctionTypeID` | `type` | `type`: no function type; hex value: function type hash |

For ID fields, `0` always means absent or not applicable. Numbering starts at
`1`, so `1` identifies the first site or entry and larger values identify later
ones. `FunctionEntryID` is a flag: `0` means no and `1` means yes.

Example of an indirect-call label (`ICallSiteID=1`) and its instruction:

```text
main.c-0-0-1-54cb21d76569286d-0-t-0-0-0-0-0-type
401257: ff d0    call *%rax
```

## Static GT extraction

This repository only generates labels during compilation. For label parsing,
basic-block recovery, four-type Static GT extraction, and graph construction,
refer to [NeuJump](https://github.com/Ryan-hub-bit/neujump).
