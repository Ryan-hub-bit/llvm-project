//===- X86LabelIndirectCallTarget.h - Label Indirect Call Targets -*- C++ -*-===//
//
// This pass handles labeling of indirect call targets in X86 assembly
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_X86_X86LABELINDIRECTCALLTARGET_H
#define LLVM_LIB_TARGET_X86_X86LABELINDIRECTCALLTARGET_H

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/MC/MCSymbol.h"

namespace llvm {

class X86InstrInfo;
class X86Subtarget;

class X86LabelIndirectCallTarget : public MachineFunctionPass {
public:
  static char ID;
  X86LabelIndirectCallTarget() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override;
  bool runOnMachineFunction(MachineFunction &MF) override;
  /// Store symbols and type identifiers used to create call graph section
  /// entries related to a function.
  struct FunctionInfo {
    /// Numeric type identifier used in call graph section for indirect calls
    /// and targets.
    using CGTypeId = uint64_t;

    /// Enumeration of function kinds, and their mapping to function kind values
    /// stored in call graph section entries.
    /// Must match the enum in llvm/tools/llvm-objdump/llvm-objdump.cpp.
    enum FunctionKind {
      /// Function cannot be target to indirect calls.
      NOT_INDIRECT_TARGET = 0,

      /// Function may be target to indirect calls but its type id is unknown.
      INDIRECT_TARGET_UNKNOWN_TID = 1,

      /// Function may be target to indirect calls and its type id is known.
      INDIRECT_TARGET_KNOWN_TID = 2,
    };

    /// Map type identifiers to callsite labels. Labels are only for indirect
    /// calls and inclusive of all indirect calls of the function.
    SmallVector<std::pair<CGTypeId, MCSymbol *>> CallSiteLabels;
  };

private:
  const X86InstrInfo *TII = nullptr;
  const X86Subtarget *STI = nullptr;

  // bool processIndirectCall(MachineBasicBlock &MBB,
                          // MachineBasicBlock::iterator MBBI);
};

FunctionPass *createX86LabelIndirectCallTargetPass();

void initializeX86LabelIndirectCallPass(PassRegistry &Registry);
} // end namespace llvm

#endif // LLVM_LIB_TARGET_X86_X86LABELINDIRECTCALLTARGET_H