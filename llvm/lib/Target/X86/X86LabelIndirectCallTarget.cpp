
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

/// Extracts a generalized numeric type identifier of a Function's type from
/// type metadata. Returns 0 if metadata cannot be found.
static uint64_t extractNumericCGTypeId(const Function &F) {
  SmallVector<MDNode *, 2> Types;
  F.getMetadata(LLVMContext::MD_type, Types);
  MDString *MDGeneralizedTypeId = nullptr;
  
  for (const auto &Type : Types) {
    if (Type->getNumOperands() == 2 && isa<MDString>(Type->getOperand(1))) {
      auto *TMDS = cast<MDString>(Type->getOperand(1));
      if (TMDS->getString().ends_with("generalized")) {
        MDGeneralizedTypeId = TMDS;
        break;
      }
    }
  }
  
  if (!MDGeneralizedTypeId) {
    errs() << "warning: can't find indirect target type id metadata "
           << "for " << F.getName() << "\n";
    return 0;  // Return 0 instead of nullptr
  }
  
  // Directly return the hash value
  return llvm::MD5Hash(MDGeneralizedTypeId->getString());
}


bool X86LabelIndirectCallTarget::runOnMachineFunction(MachineFunction &MF){
  FunctionInfo FuncInfo;
  const auto &CallSitesInfoMap = MF.getCallSitesInfo();  // Use '.' instead of '->'
  const TargetMachine &TM = MF.getTarget();  // Ensure TargetMachine is referenced correctly
  const Function &F = MF.getFunction();
  uint64_t FHash = llvm::MD5Hash(F.getName());
  LLVM_DEBUG(dbgs() <<F.getName() <<"\n");
  for (auto &MBB : MF) {
    for (auto &MI : MBB) {
      LLVM_DEBUG(dbgs() << MI <<"\n");
      if (MI.getOpcode() == X86::TCRETURNri64) {
            SmallString<64> TailJumpLabel;
            raw_svector_ostream(TailJumpLabel) << "tailcall_" << FHash << "_" << tailCallID;
            MCSymbol *TailJump = MF.getContext().getOrCreateSymbol(TailJumpLabel);
         if (&MBB == &MF.front()){
            MBB.begin()->setPostInstrSymbol(MF, TailJump);
            } else{
              MBB.begin()->setPreInstrSymbol(MF,TailJump);
            }
      }
      
      LLVM_DEBUG(dbgs() <<"MI is call ? " << MI.isCall() <<"\n");
      if (TM.Options.MatchIndirectCall && MI.isCall()) {
        const auto &CallSiteInfo = CallSitesInfoMap.find(&MI);

        if (CallSiteInfo != CallSitesInfoMap.end()) {
          LLVM_DEBUG(dbgs() << "in CallsiteInfo"<<"\n");
          if (auto *TypeId = CallSiteInfo->second.TypeId) {
             // Emit label
                uint64_t TypeIdVal = TypeId->getZExtValue();  // Can be used later if needed
                LLVM_DEBUG(dbgs() << " Indirect or tail call target TypeId value: 0x" << Twine::utohexstr(TypeIdVal) << "\n");
                  // Generate labelName based on callsiteID
                std::string labelName = "callsite_" + std::to_string(callsiteID);
                // Insert the TypeIdVal into the map under labelName
                callsitetoTypeID[labelName].insert(TypeIdVal);;
                MCSymbol *Label = MF.getContext().getOrCreateSymbol(labelName);
                llvm::MachineInstr* MIptr = &MI;
                MIptr->setPreInstrSymbol(MF, Label);
                errs() <<"CallsiteID:" << callsiteID <<"\n";
                callsiteID ++;
          }
          } 
        } 
      }
    }


  // Extract TypeId of current function, use it label the function begin address
bool IsIndirectTarget =
      !F.hasLocalLinkage() || F.hasAddressTaken(nullptr,
                                                /*IgnoreCallbackUses=*/true,
                                                /*IgnoreAssumeLikeCalls=*/true,
                                                /*IgnoreLLVMUsed=*/false);
uint64_t TypeIdVal = 0;
if(IsIndirectTarget) {
  TypeIdVal = extractNumericCGTypeId(F);
}
if (TypeIdVal != 0) {
    bool Inserted = TypeIdSet.insert(TypeIdVal).second;
    if(Inserted) {
      LLVM_DEBUG(dbgs()<<"inserted"<<"\n");
    }
    
} else{
  LLVM_DEBUG(dbgs() <<"func name: " << F.getName() << ": TypeIdVal == 0" << "\n");
}

  // Replace this part in your code:
unsigned ReturnCounter = 0;  // Counter for this function only
for (auto &MBB : MF) {
    if (!MBB.empty() && TypeIdVal != 0) {
        MachineBasicBlock::iterator Terminator = MBB.terminators().begin();
        if (Terminator != MBB.end() && Terminator->isReturn()) {
            SmallString<64> RetLabel;
            raw_svector_ostream(RetLabel) << "func_" << FHash << "_"
                                        << Twine::utohexstr(TypeIdVal) 
                                        << "_ret_" << ReturnCounter;
            
            MCSymbol *Label = MF.getContext().getOrCreateSymbol(RetLabel);
            if (&MBB == &MF.front()){
            MBB.begin()->setPostInstrSymbol(MF, Label);
            } else{
              MBB.begin()->setPreInstrSymbol(MF,Label);
            }
            ReturnCounter++;
            
            LLVM_DEBUG(dbgs() << "Added return label in non-entry block: " 
                             << RetLabel << "\n");
        }
    }
}
  return true;
}

bool X86LabelIndirectCallTarget::doFinalization(Module &M) {
  errs() << "\n=== Callsite to TypeID Mapping ===\n";
  for (const auto &Entry : callsitetoTypeID) {
    errs() << Entry.getKey() << ":";
    for (uint64_t TypeID : Entry.second) {
      errs() << " " << Twine::utohexstr(TypeID);
    }
    errs() << "\n";
  }
  return false;
}
FunctionPass *createX86LabelIndirectCallTargetPass() {
  return new X86LabelIndirectCallTarget();
}

void initializeX86LabelIndirectCallPass(PassRegistry &Registry) {
  RegisterPass<X86LabelIndirectCallTarget> X("x86-label-indirect-call", "X86 Label Indirect Call Target Pass", false, false);
}
}