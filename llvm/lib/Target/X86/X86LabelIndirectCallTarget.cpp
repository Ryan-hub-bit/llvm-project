// //===- X86LabelIndirectCallTarget.cpp - Label Indirect Call Targets -*- C++ -*-===//
// //
// // This pass handles labeling of indirect call targets in X86 assembly
// //
// //===----------------------------------------------------------------------===//

// #include "X86LabelIndirectCallTarget.h"
// #include "X86.h"
// #include "X86InstrInfo.h"
// #include "X86Subtarget.h"
// #include "llvm/CodeGen/MachineInstrBuilder.h"
// #include "llvm/CodeGen/MachineModuleInfo.h"
// #include "llvm/IR/Function.h"
// #include "llvm/Support/Debug.h"

// using namespace llvm;

// #define DEBUG_TYPE "x86-label-indirect-call"


// namespace llvm{

// char X86LabelIndirectCallTarget::ID = 0;
// INITIALIZE_PASS(X86LabelIndirectCallTarget, "x86-label-indirect-call",
//                 "X86 Label Indirect Call Target Pass", false, false)

// }
// StringRef X86LabelIndirectCallTarget::getPassName() const {
//   return "X86 Label Indirect Call Target Pass";
// }

// bool X86LabelIndirectCallTarget::runOnMachineFunction(MachineFunction &MF) {
//   STI = &MF.getSubtarget<X86Subtarget>();
//   TII = STI->getInstrInfo();
//   bool Modified = false;

//   // for (auto &MBB : MF) {
//   //   for (auto MBBI = MBB.begin(); MBBI != MBB.end(); ++MBBI) {
//   //     if (MBBI->isCall() && MBBI->isIndirectBranch()) {
//   //       Modified |= processIndirectCall(MBB, MBBI);
//   //     }
//   // //   }
//   // }
//   errs() <<"in X86LabelIndirectCallTarget" <<"/n";

//   return Modified;
// }

// // bool X86LabelIndirectCallTarget::processIndirectCall(
// //     MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI) {
// //   MachineInstr &MI = *MBBI;
  
// //   // Skip if instruction is already properly labeled
// //   if (MI.getOperand(0).isSymbol())
// //     return false;

// //   // Create a new label for this indirect call
// //   MachineFunction *MF = MBB.getParent();
// //   const BasicBlock *BB = MBB.getBasicBlock();
  
// //   if (!BB)
// //     return false;

// //   // Generate a unique label name
// //   std::string LabelName = 
// //       ("__indirect_call_" + BB->getName() + "_" + 
// //        std::to_string(MI.getDebugLoc().getLine())).str();

// //   // Add label reference to the call instruction
// //   BuildMI(MBB, MBBI, MI.getDebugLoc(), TII->get(X86::LABEL))
// //       .addSym(MF->createExternalSymbolName(LabelName));

// //   return true;
// // }

// FunctionPass *llvm::createX86LabelIndirectCallTargetPass() {
//   return new X86LabelIndirectCallTarget();
// }

#include "X86LabelIndirectCallTarget.h"
#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "x86-label-indirect-call"

namespace llvm {

char X86LabelIndirectCallTarget::ID = 0;

StringRef X86LabelIndirectCallTarget::getPassName() const {
  return "X86 Label Indirect Call Target Pass";
}

bool X86LabelIndirectCallTarget::runOnMachineFunction(MachineFunction &MF) {
  STI = &MF.getSubtarget<X86Subtarget>();
  TII = STI->getInstrInfo();
  bool Modified = false;
   errs() <<"in X86LabelIndirectCallTarget" <<"\n";
  // for (auto &MBB : MF) {
  //   for (auto MBBI = MBB.begin(); MBBI != MBB.end(); ++MBBI) {
  //     if (MBBI->isCall() && MBBI->isIndirectBranch()) {
  //       Modified |= processIndirectCall(MBB, MBBI);
  //     }
  //   }
  // }

  return Modified;
}

// bool X86LabelIndirectCallTarget::processIndirectCall(
//     MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI) {
//   MachineInstr &MI = *MBBI;
  
//   // Skip if instruction is already properly labeled
//   if (MI.getOperand(0).isSymbol())
//     return false;

//   // Create a new label for this indirect call
//   MachineFunction *MF = MBB.getParent();
//   const BasicBlock *BB = MBB.getBasicBlock();
  
//   if (!BB)
//     return false;

//   // Generate a unique label name
//   std::string LabelName = 
//       ("__indirect_call_" + BB->getName() + "_" + 
//        std::to_string(MI.getDebugLoc().getLine())).str();

//   // Add label reference to the call instruction
//   BuildMI(MBB, MBBI, MI.getDebugLoc(), TII->get(X86::LABEL))
//       .addSym(MF->createExternalSymbolName(LabelName));

//   return true;
// }

FunctionPass *createX86LabelIndirectCallTargetPass() {
  return new X86LabelIndirectCallTarget();
}

void initializeX86LabelIndirectCallPass(PassRegistry &Registry) {
  RegisterPass<X86LabelIndirectCallTarget> X("x86-label-indirect-call", "X86 Label Indirect Call Target Pass", false, false);
}
}