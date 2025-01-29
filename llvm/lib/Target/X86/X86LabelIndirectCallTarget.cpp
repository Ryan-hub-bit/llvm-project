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

// FIXME: create a new label system to avoid the one instruction can only be labeled once 
// TailcallID
// IndirectCallID
// DirectCAll ID 
// JumpTableID
// JumpTableEntryID
// Function retID
// Function name
// Function signature
// label offset
#define DEBUG_TYPE "x86-label-indirect-call"

namespace llvm {

char X86LabelIndirectCallTarget::ID = 0;

// Initialize the list of values in the set
const std::set<uint16_t> X86LabelIndirectCallTarget::TailJumps = X86LabelIndirectCallTarget::initializeTailJumps();

// Helper function to populate the set
std::set<uint16_t> X86LabelIndirectCallTarget::initializeTailJumps() {
    return {
        4950, // TAILJMPm
        4951, // TAILJMPm64
        4952, // TAILJMPm64_REX
        4953, // TAILJMPr
        4954, // TAILJMPr64
        4955  // TAILJMPr64_REX
    };
}

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

bool X86LabelIndirectCallTarget::runOnMachineFunction(MachineFunction &MF) {
    const auto &CallSitesInfoMap = MF.getCallSitesInfo();  // Use '.' instead of '->'
    const TargetMachine &TM = MF.getTarget();  // Ensure TargetMachine is referenced correctly
    bool recordnext = false;
    uint64_t lastTypeId = 0;
     //logic for tail call and indirect call
    for (auto &MBB : MF) {
        for (auto &MI : MBB) {
            if(recordnext) {
                std::string nextLabel = "callsite_" + std::to_string(callsiteID) + "_next";
                typeIdtocallsitenext[lastTypeId].insert(nextLabel);
                MCSymbol *Label = MF.getContext().getOrCreateSymbol(nextLabel);
                llvm::MachineInstr* MIptr = &MI;
                MIptr->setPreInstrSymbol(MF, Label);
                errs() << "CallsiteID:" << callsiteID << "_next"<<"\n";
                recordnext = false;
            }
            if (TM.Options.MatchIndirectCall && MI.isCall()) {
                LLVM_DEBUG(MI.print(dbgs()));
                LLVM_DEBUG(dbgs() << "MI opcode:" << MI.getOpcode() << "\n");
                
                const auto &CallSiteInfo = CallSitesInfoMap.find(&MI);
                if (CallSiteInfo != CallSitesInfoMap.end()) {
                    // Generate labelName based on callsiteID
                    if (auto *TypeId = CallSiteInfo->second.TypeId) {
                        uint64_t TypeIdVal = TypeId->getZExtValue();  // Can be used later if needed
                        LLVM_DEBUG(dbgs() << "  TypeId value: 0x" << Twine::utohexstr(TypeIdVal) << "\n");
                        lastTypeId = TypeIdVal;
                        if (X86LabelIndirectCallTarget::TailJumps.count(MI.getOpcode())) {
                            std::string labelName = "tailcallsite_" + std::to_string(tailcallID);
                            callsitetoTypeID[labelName].insert(TypeIdVal);
                            MCSymbol *Label = MF.getContext().getOrCreateSymbol(labelName);
                            llvm::MachineInstr* MIptr = &MI;
                            MIptr->setPreInstrSymbol(MF, Label);
                            errs() << "tailcallID:" << tailcallID << "\n";
                            tailcallID++;
                        } else {
                            std::string labelName = "callsite_" + std::to_string(callsiteID);
                            callsitetoTypeID[labelName].insert(TypeIdVal);
                            MCSymbol *Label = MF.getContext().getOrCreateSymbol(labelName);
                            llvm::MachineInstr* MIptr = &MI;
                            MIptr->setPreInstrSymbol(MF, Label);
                            errs() << "CallsiteID:" << callsiteID << "\n";
                            callsiteID++;
                            recordnext = true;
                        }
                    }
                }
            }
        }
    }

    // Extract TypeId of current function, use it label the function begin address
    const Function &F = MF.getFunction();
    uint64_t FHash = llvm::MD5Hash(F.getName());
    bool IsIndirectTarget = !F.hasLocalLinkage() || 
                           F.hasAddressTaken(nullptr,
                                           /*IgnoreCallbackUses=*/true,
                                           /*IgnoreAssumeLikeCalls=*/true,
                                           /*IgnoreLLVMUsed=*/false);
    uint64_t TypeIdVal = 0;
    
    if (IsIndirectTarget) {
        TypeIdVal = extractNumericCGTypeId(F);
    }
    
    if (TypeIdVal != 0) {
        bool Inserted = TypeIdSet.insert(TypeIdVal).second;
        if (Inserted) {
            LLVM_DEBUG(dbgs() << "inserted" << "\n");
        }
    } else {
        LLVM_DEBUG(dbgs() << "func name: " << F.getName() << ": TypeIdVal == 0" << "\n");
    }

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
                if (&MBB == &MF.front()) {
                    MBB.begin()->setPreInstrSymbol(MF, Label);
                } else {
                    MBB.begin()->setPreInstrSymbol(MF, Label);
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
    errs() << "\n=== TypeID to callsite next Mapping ===\n";
    for (const auto &Entry : typeIdtocallsitenext) {
        errs() << Entry.first << ":";
        for (const std::string &callsite_next : Entry.second) {
            errs() << " " << callsite_next;
        }
        errs() << "\n";
    }
    return false;
}

FunctionPass *createX86LabelIndirectCallTargetPass() {
    return new X86LabelIndirectCallTarget();
}

void initializeX86LabelIndirectCallPass(PassRegistry &Registry) {
    RegisterPass<X86LabelIndirectCallTarget> X("x86-label-indirect-call", 
                                              "X86 Label Indirect Call Target Pass",
                                              false, false);
}

} // namespace llvm