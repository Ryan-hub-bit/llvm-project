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
#include <map>
#include <set>
#include <string>


#include "MCTargetDesc/X86MCTargetDesc.h"
#include "X86.h"
#include "X86InstrInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include <sstream>

namespace llvm {

class X86InstrInfo;
class X86Subtarget;

class X86LabelIndirectCallTarget : public MachineFunctionPass {
public:
  static char ID;
  X86LabelIndirectCallTarget() : MachineFunctionPass(ID),callsiteID(1),tailcallID(1), RunCount(1), MaxEntrySize(0){}
  bool doFinalization(Module &M) override;
  StringRef getPassName() const override;
  bool runOnMachineFunction(MachineFunction &MF) override;
  static const std::set<uint16_t> TailJumps; // List of values to check against
  static std::set<uint16_t> initializeTailJumps();
  MachineInstr* traceIndirectJumps(MachineFunction &MF, unsigned JTIndex, 
                                  MachineJumpTableInfo *JumpTableInfo);
  bool isJumpTableRelated(MachineInstr &MI, const MachineJumpTableEntry &JTEntry, 
                         MachineFunction &MF);
  bool isJumpTableLoad(MachineInstr &MI, const MachineJumpTableEntry &JTEntry);
  bool isRegUsedInJumpTableLoad(Register Reg,MachineFunction &MF,
                                                    const MachineJumpTableEntry &JTEntry);


private:
    int callsiteID;
    int tailcallID;
    int RunCount;
    int MaxEntrySize;
    SmallSet<uint64_t, 16> TypeIdSet;  // Add this line
      // Map to store labelName -> set of TypeIdVal
  StringMap<SmallSet<uint64_t, 4>> callsitetoTypeID;
  std::map<uint64_t, std::set<std::string>> typeIdtocallsitenext;
  // bool processIndirectCall(MachineBasicBlock &MBB,
                          // MachineBasicBlock::iterator MBBI);
};

FunctionPass *createX86LabelIndirectCallTargetPass();

void initializeX86LabelIndirectCallPass(PassRegistry &Registry);
} // end namespace llvm

#endif // LLVM_LIB_TARGET_X86_X86LABELINDIRECTCALLTARGET_H