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
// #include "llvm/Support/LLVM_DEBUG.h"

// using namespace llvm;

// #define LLVM_DEBUG_TYPE "x86-label-indirect-call"


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
// //        std::to_string(MI.getLLVM_DEBUGLoc().getLine())).str();

// //   // Add label reference to the call instruction
// //   BuildMI(MBB, MBBI, MI.getLLVM_DEBUGLoc(), TII->get(X86::LABEL))
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
#include "llvm/Target/TargetMachine.h"

using namespace llvm;

#define DEBUG_TYPE "x86-label-indirect-call"

namespace llvm {

char X86LabelIndirectCallTarget::ID = 0;

StringRef X86LabelIndirectCallTarget::getPassName() const {
  return "X86 Label Indirect Call Target Pass";
}

bool X86LabelIndirectCallTarget::runOnMachineFunction(MachineFunction &MF){
  FunctionInfo FuncInfo;
  const auto &CallSitesInfoMap = MF.getCallSitesInfo();  // Use '.' instead of '->'

  const TargetMachine &TM = MF.getTarget();  // Ensure TargetMachine is referenced correctly
  for (auto &MBB : MF) {
    for (auto &MI : MBB) {
      if (TM.Options.MatchIndirectCall && MI.isCall()) {
        LLVM_DEBUG(dbgs() << "Found call instruction: ");
        LLVM_DEBUG(MI.print(dbgs()));
        LLVM_DEBUG(dbgs() << "\n");

        const auto &CallSiteInfo = CallSitesInfoMap.find(&MI);
        if (CallSiteInfo != CallSitesInfoMap.end()) {
          LLVM_DEBUG(dbgs() << "  Found CallSiteInfo entry\n");
          
          if (auto *TypeId = CallSiteInfo->second.TypeId) {
            // Emit label
            uint64_t TypeIdVal = TypeId->getZExtValue();  // Can be used later if needed
            LLVM_DEBUG(dbgs() << "  TypeId value: 0x" << Twine::utohexstr(TypeIdVal) << "\n");
            StringRef LabelName = ("callsite_" + Twine(callsiteID)).str();
            // callsitetoTypeID[labelName].insert(TypeIdVal);
            MCSymbol *Label = MF.getContext().getOrCreateSymbol(LabelName);
            llvm::MachineInstr* MIptr = &MI;
            MIptr->setPreInstrSymbol(MF, Label);
            errs() <<"CallsiteID:" << callsiteID <<"\n";
            callsiteID ++;
          } else {
            LLVM_DEBUG(dbgs() << "  No TypeId found for this call site\n");
          }
        } else {
          LLVM_DEBUG(dbgs() << "  No CallSiteInfo found for this instruction\n");
        }
      }
    }
  }

  return true;
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
//        std::to_string(MI.getLLVM_DEBUGLoc().getLine())).str();

//   // Add label reference to the call instruction
//   BuildMI(MBB, MBBI, MI.getLLVM_DEBUGLoc(), TII->get(X86::LABEL))
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