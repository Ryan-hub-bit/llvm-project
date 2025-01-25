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
#include "llvm/ADT/SmallSet.h"  // To use SmallSet or std::set for storing TypeIdVal
#include "llvm/ADT/StringMap.h"  // To use StringMap for storing labelName -> TypeI

namespace llvm {

class X86InstrInfo;
class X86Subtarget;

class X86LabelIndirectCallTarget : public MachineFunctionPass {
public:
  static char ID;
  X86LabelIndirectCallTarget() : MachineFunctionPass(ID),callsiteID(0),tailcallID(0) {}
  bool doFinalization(Module &M) override;
  StringRef getPassName() const override;
  bool runOnMachineFunction(MachineFunction &MF) override;
  static const std::set<uint16_t> TailJumps; // List of values to check against
  static std::set<uint16_t> initializeTailJumps();


private:
    int callsiteID;
    int tailcallID;
    SmallSet<uint64_t, 16> TypeIdSet;  // Add this line
      // Map to store labelName -> set of TypeIdVal
  StringMap<SmallSet<uint64_t, 4>> callsitetoTypeID;
  // bool processIndirectCall(MachineBasicBlock &MBB,
                          // MachineBasicBlock::iterator MBBI);
};

FunctionPass *createX86LabelIndirectCallTargetPass();

void initializeX86LabelIndirectCallPass(PassRegistry &Registry);
} // end namespace llvm

#endif // LLVM_LIB_TARGET_X86_X86LABELINDIRECTCALLTARGET_H