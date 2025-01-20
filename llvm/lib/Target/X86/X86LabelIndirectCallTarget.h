//===- X86LabelIndirectCallTarget.h - Label Indirect Call Targets -*- C++ -*-===//
//
// This pass handles labeling of indirect call targets in X86 assembly
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_X86_X86LABELINDIRECTCALLTARGET_H
#define LLVM_LIB_TARGET_X86_X86LABELINDIRECTCALLTARGET_H

#include "llvm/CodeGen/MachineFunctionPass.h"

namespace llvm {

class X86InstrInfo;
class X86Subtarget;

class X86LabelIndirectCallTarget : public MachineFunctionPass {
public:
  static char ID;
  X86LabelIndirectCallTarget() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override;
  bool runOnMachineFunction(MachineFunction &MF) override;

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