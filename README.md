# LLVM Static GT Generation Fork

This repository is an experimental research fork of the official
[LLVM monorepo](https://github.com/llvm/llvm-project). LLVM provides reusable
compiler and toolchain infrastructure; Clang is its C, C++, Objective-C, and
Objective-C++ frontend. The upstream documentation remains available at
[llvm.org/docs](https://llvm.org/docs/).

The fork adds static ground-truth (GT) generation for control-flow research on
x86-64 binaries. It augments Clang and the X86 backend with generalized type
metadata, machine-code labels, and optional ELF sections that describe direct
call returns and C++ exception edges.

The current `main` branch is the former `final_without_icswitch` branch. At
commit `4bec693cda57`, the old `-fmatch-indirect-call` switch has been removed:
indirect-call type metadata and X86 labeling are enabled by default.

## What this fork adds

### Generalized indirect-call type IDs

Clang emits a generalized `.generalized` type identifier for:

- externally visible or address-taken functions; and
- indirect C/C++ call sites.

The type string is hashed to a 64-bit value and carried into machine-level call
site information. This lets a downstream analysis associate an indirect call
site with functions that have a compatible generalized type.

### X86 control-flow labels

`X86LabelIndirectCallTarget` runs automatically in the X86 pre-emit pipeline.
It adds encoded symbols for:

- function entries and return targets;
- indirect-call and indirect-tail-call sites with their type IDs;
- direct tail calls; and
- jump-table dispatch sites and jump-table entries.

The labels contain the module identifier plus numeric fields for the relevant
call-site, jump-table, return, function-hash, and function-type information.
They can be inspected in an object file or executable with `llvm-nm`.

### Interprocedural GT sections

The new-PM module pass is registered as `inter-procedural-graph`. For direct
calls whose callee definition and debug information are available in the same
LLVM module, it emits address anchors into these ELF sections:

| Section | Meaning |
| --- | --- |
| `.section_for_caller` | Basic block containing a direct call |
| `.section_for_return` | Return block in the corresponding callee |
| `.section_for_invoker` | Block containing a potentially throwing `invoke` |
| `.section_for_exception` | Exception-handler block reached by that `invoke` |

The pass is registered with `opt`, but it is **not** inserted into Clang's
default optimization pipeline. Run it explicitly as shown below when these
sections are required.

## Build

The following minimal configuration builds Clang, `opt`, and the inspection
tools for X86:

```bash
cmake -S llvm -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_PROJECTS=clang \
  -DLLVM_TARGETS_TO_BUILD=X86 \
  -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_INCLUDE_TESTS=OFF \
  -DLLVM_INCLUDE_BENCHMARKS=OFF

cmake --build build --target clang opt llvm-readelf llvm-nm -- -j$(nproc)
```

This fork identifies itself as LLVM/Clang `20.0.0git` at commit
`4bec693cda57`.

## Generate Static GT

Compile with debug information, run the GT pass explicitly, and then lower the
transformed IR with this fork's Clang:

```bash
# C source
build/bin/clang -O0 -g -emit-llvm -c example.c -o example.bc
build/bin/opt -passes=inter-procedural-graph -S example.bc -o example.gt.ll
build/bin/clang -O0 -g example.gt.ll -o example

# Use build/bin/clang++ for C++ sources and final C++ linking.
```

Inspect the resulting GT sections and X86 labels:

```bash
build/bin/llvm-readelf --sections example | grep section_for_
build/bin/llvm-nm -a example | grep 'example.gt.ll-'
```

The X86 label pass also runs during a normal direct Clang compilation. The
explicit `opt` step is needed specifically for the four `.section_for_*`
sections.

## Extract NeuJump Static GT from instruction labels

The X86 instruction labels are the input to the downstream NeuJump analysis
pipeline; that extractor is maintained separately and is not bundled in this
LLVM source tree. Its workflow is:

1. read the encoded symbols from the linked ELF with `objdump -t`;
2. recover functions, instructions, basic blocks, and CFG edges with angr;
3. normalize labeled instruction addresses to graph basic-block nodes;
4. emit the four training tasks shown below; and
5. construct the compressed heterogeneous DGL graph and node lookup.

| NeuJump task | Generated resource | Relationship |
| --- | --- | --- |
| `ret` | `<binary>_ret.json` | callee return block to caller continuation |
| `jumptable` | `<binary>_correctjumptable.json` | dispatch block to switch target blocks |
| `tailcall` | `<binary>_itcbbtofunc.json` | indirect tail-call block to compatible functions |
| `indirectcall` | `<binary>_icallbbtocallee.json` | indirect-call block to compatible functions |

These four NeuJump task types are distinct from the four optional
`.section_for_*` categories emitted by `inter-procedural-graph`.

## Current scope and limitations

- The machine-level label generation is implemented in the X86 backend and has
  only been validated on x86-64 Linux/ELF.
- `-g` is required by `inter-procedural-graph`, because it filters callees using
  source paths from `DISubprogram` metadata.
- Direct-call return and exception relationships are module-local. For
  cross-translation-unit relationships, first combine bitcode or use an LTO
  workflow so callers and callee definitions are visible in one LLVM module.
- Indirect calls are represented by generalized type IDs and X86 call-site
  labels; the IR pass cannot resolve their concrete runtime targets.
- The generated IR globals include randomized numeric names. Treat the section
  contents and encoded addresses as the GT payload, not those random names as
  stable identifiers.
- This is a research fork and has not been validated against the full upstream
  LLVM test matrix.

## Validation snapshot

The current `main` commit was configured and built successfully on x86-64
Ubuntu using GCC 13, CMake 3.28, and Ninja 1.11. Three end-to-end fixtures were
then compiled through `clang -> opt -passes=inter-procedural-graph -> clang`:

| Fixture | Observed result |
| --- | --- |
| Direct calls with multiple returns | 2 caller anchors and 2 return anchors |
| Function pointer plus dense switch | indirect call-site TypeID plus jump-table source/entry labels |
| C++ `try`/`catch` | 2 caller, 2 return, 1 invoker, and 1 exception anchor |

All three executables linked, ran successfully, and contained the expected ELF
sections or encoded X86 symbols.

The instruction-label workflow was additionally validated end to end with the
downstream NeuJump extractor. One linked ELF contained a machine jump table, a
non-tail indirect call, and an optimized indirect tail call. Label parsing,
angr normalization, DGL graph construction, and the training-side GT loader
produced:

| NeuJump task | Loaded GT edges |
| --- | ---: |
| `ret` | 12 |
| `jumptable` | 7 |
| `tailcall` | 4 |
| `indirectcall` | 4 |

All 27 GT edges resolved to valid graph nodes. The resulting graph contained
64 code nodes, 11 data nodes, and 116 edges, and its compressed DGL artifact
loaded successfully.

## Upstream resources

- [LLVM project repository](https://github.com/llvm/llvm-project)
- [Getting Started with LLVM](https://llvm.org/docs/GettingStarted.html)
- [Contributing to LLVM](https://llvm.org/docs/Contributing.html)
- [LLVM code of conduct](https://llvm.org/docs/CodeOfConduct.html)

Changes specific to Static GT generation should be developed and reviewed in
this fork. General LLVM fixes should follow the upstream contribution process.
