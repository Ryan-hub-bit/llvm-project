#include "X86LabelIndirectCallTarget.h"
#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetMachine.h"

#include "X86.h"
#include "X86InstrInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/PassRegistry.h"
#include "llvm/Pass.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/ADT/Statistic.h"
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
// callsite_ID_next
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

static std::string initializeLabel() {
    std::string result = "s";
    
    // Add elements 2-5 (four zeros)
    for(int i = 0; i < 4; i++) {
        result += "-0";
    }
    
    // Add element 6 ('t')
    result += "-t";
    
    // Add elements 7-10 (four more zeros)
    for(int i = 0; i < 4; i++) {
        result += "-0";
    }
    
    // Add the final element
    result += "-type";
    
    return result;
}

bool X86LabelIndirectCallTarget::runOnMachineFunction(MachineFunction &MF) {

    // jump table label first
      Function &F = MF.getFunction();

    // LLVM_DEBUG(dbgs() << "Function address: " << FuncAddr << "\n");
    bool havejumptable = false;
    // Process jump tables
    MachineJumpTableInfo *JumpTableInfo = MF.getJumpTableInfo();
    if (JumpTableInfo) {
        havejumptable = true;
    
    }

    // bool Modified = false;
    //  LLVM_DEBUG(dbgs() << "Jump Table Size#" << JumpTableInfo->getJumpTables().size() << "\n");
    
    if(havejumptable) {
    for (unsigned JTIndex = 0; JTIndex < JumpTableInfo->getJumpTables().size(); ++JTIndex) {
    const MachineJumpTableEntry &JTEntry = JumpTableInfo->getJumpTables()[JTIndex];
    // LLVM_DEBUG(dbgs() << "FuncAddr:" << FuncAddr << "Jump Table #" << JTIndex << " contains " 
                    //   << JTEntry.MBBs.size() << " entries.\n");
    
    // Handle indirect jump instruction
    MachineInstr *indirectJumpInstr = traceIndirectJumps(MF, JTIndex, JumpTableInfo);
    if (indirectJumpInstr) {
        // Create label for indirect jump
        std::string LabelName =  std::to_string(RunCount) + "_IJUMP_" + std::to_string(JTIndex);
        MCSymbol *Label = MF.getContext().getOrCreateSymbol(LabelName);
        indirectJumpInstr->setPreInstrSymbol(MF, Label);
        if (MaxEntrySize < JTEntry.MBBs.size()) {
            MaxEntrySize = JTEntry.MBBs.size();
        }
        for (unsigned EntryIndex = 0; EntryIndex < JTEntry.MBBs.size(); ++EntryIndex) {
            MachineBasicBlock *TargetMBB = JTEntry.MBBs[EntryIndex];
            if (!TargetMBB->empty()) {
            std::string EntryLabelName = std::to_string(RunCount) + "_JTENTRY_" + std::to_string(JTIndex) + "_" + std::to_string(EntryIndex);
            MCSymbol *EntryLabel = MF.getContext().getOrCreateSymbol(EntryLabelName);
            
            // Set label only on first instruction
            MachineInstr &FirstInstr = TargetMBB->front();
            FirstInstr.setPreInstrSymbol(MF, EntryLabel);
            
            // LLVM_DEBUG(dbgs() << "Created label for jump table entry: " << EntryLabelName << "\n");
        }
        }
        RunCount ++;
    }
    }
    }

    const auto &CallSitesInfoMap = MF.getCallSitesInfo();  // Use '.' instead of '->'
    const TargetMachine &TM = MF.getTarget();  // Ensure TargetMachine is referenced correctly
    bool recordnext = false;
    uint64_t lastTypeId = 0;
     //logic for tail call and indirect call
    for (auto &MBB : MF) {
        for (auto &MI : MBB) {
            if(recordnext) {
                std::string nextLabel = "callsite_" + std::to_string(callsiteID - 1) + "_next";
                typeIdtocallsitenext[lastTypeId].insert(nextLabel);
                MCSymbol *Label = MF.getContext().getOrCreateSymbol(nextLabel);
                llvm::MachineInstr* MIptr = &MI;
                MIptr->setPreInstrSymbol(MF, Label);
                errs() << "CallsiteID:" << callsiteID  -1<< "_next"<<"\n";
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
    const Function &CF = MF.getFunction();
    uint64_t FHash = llvm::MD5Hash(CF.getName());
    bool IsIndirectTarget = !CF.hasLocalLinkage() || 
                           CF.hasAddressTaken(nullptr,
                                           /*IgnoreCallbackUses=*/true,
                                           /*IgnoreAssumeLikeCalls=*/true,
                                           /*IgnoreLLVMUsed=*/false);
    uint64_t TypeIdVal = 0;
    
    if (IsIndirectTarget) {
        TypeIdVal = extractNumericCGTypeId(CF);
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
                // if (&MBB == &MF.front()) {
                   MBB.begin()->setPreInstrSymbol(MF, Label);
              //  } else {
               //     MBB.begin()->setPreInstrSymbol(MF, Label);
                //}
                ReturnCounter++;
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

MachineInstr* X86LabelIndirectCallTarget::traceIndirectJumps(MachineFunction &MF,
                                                          unsigned JTIndex,
                                                          MachineJumpTableInfo *JumpTableInfo) {
    const MachineJumpTableEntry &JTEntry = JumpTableInfo->getJumpTables()[JTIndex]; 
    // LLVM_DEBUG(dbgs() << "Tracing indirect jumps:\n");
    for (auto &MBB : MF) {
        // LLVM_DEBUG(dbgs() << "  Checking BB: " << MBB.getName() << "\n");
        for (auto &MI : MBB) {
            // LLVM_DEBUG(dbgs() << "    Checking instruction: " << MI << "\n");
            if (MI.isIndirectBranch()) {
                // LLVM_DEBUG(dbgs() << "    Found indirect jump: " << MI << "\n");
                
                if (isJumpTableRelated(MI, JTEntry, MF)) {
                    // LLVM_DEBUG(dbgs() << "    This indirect jump is related to Jump Table #"
                            //    << JTIndex << "\n");
                    return &MI;
                } else {
                    // LLVM_DEBUG(dbgs() << "    Jump is not related to this jump table\n");
                }
            }
        }
    }
    
    // LLVM_DEBUG(dbgs() << "  No related indirect jump found\n");
    return nullptr;
}

bool X86LabelIndirectCallTarget::isJumpTableLoad(MachineInstr &MI, const MachineJumpTableEntry &JTEntry) {
    // LLVM_DEBUG(dbgs() << "\nAnalyzing potential jump table load instruction: " << MI << "\n");

    // First check memory operands for jump table metadata
    for (const MachineMemOperand *MMO : MI.memoperands()) {
        // LLVM_DEBUG(dbgs() << "  Checking memory operand flags: " << MMO->getFlags() << "\n");
        if (MMO->getValue()) {
            StringRef ValueName = MMO->getValue()->getName();
            // LLVM_DEBUG(dbgs() << "    Memory value name: '" << ValueName << "'\n");
            if (ValueName.contains("jump-table")) {
                // LLVM_DEBUG(dbgs() << "    Found jump table in memory value name\n");
                return true;
            }
        }

        // Check if this is a jump table load directly from memory operand comments
        if (MI.getDesc().mayLoad() && MI.hasOneMemOperand()) {
            // Look for jump table reference in the instruction's debug info or comments
            if (MI.getDebugLoc()) {
                std::string Comment;
                raw_string_ostream OS(Comment);
                MI.print(OS);
                if (Comment.find("jump-table") != std::string::npos) {
                    // LLVM_DEBUG(dbgs() << "    Found jump table reference in instruction comment\n");
                    return true;
                }
            }
        }
    }

    // Check for the MOVSX pattern
    if (MI.getOpcode() == X86::MOVSX64rm32) {
        // LLVM_DEBUG(dbgs() << "  Found MOVSX64rm32 instruction\n");
        Register BaseReg;
        
        // Find base register
        for (const MachineOperand &MO : MI.operands()) {
            if (MO.isReg() && MO.isUse()) {
                BaseReg = MO.getReg();
                // LLVM_DEBUG(dbgs() << "    Found base register: " << printReg(BaseReg, nullptr) << "\n");
                break;
            }
        }

        if (BaseReg) {
            // Look for preceding LEA
            MachineBasicBlock::iterator MBBI = MI;
            const MachineBasicBlock *MBB = MI.getParent();
            
            // LLVM_DEBUG(dbgs() << "    Looking for LEA defining register: " << printReg(BaseReg, nullptr) << "\n");
            
            while (MBBI != MBB->begin()) {
                --MBBI;
                // LLVM_DEBUG(dbgs() << "      Checking: " << *MBBI << "\n");
                
                if (MBBI->getOpcode() == X86::LEA64r) {
                    // LLVM_DEBUG(dbgs() << "      Found LEA64r\n");
                    
                    // Verify this LEA defines our base register
                    const MachineOperand &DefReg = MBBI->getOperand(0);
                    if (!DefReg.isReg() || DefReg.getReg() != BaseReg) {
                        // LLVM_DEBUG(dbgs() << "      LEA defines different register\n");
                        continue;
                    }
                    
                    // Check for jump table symbol
                    for (const MachineOperand &MO : MBBI->operands()) {
                        if (MO.isSymbol()) {
                            StringRef SymName = MO.getSymbolName();
                            // LLVM_DEBUG(dbgs() << "      Checking symbol: '" << SymName << "'\n");
                            if (SymName.contains("jump-table")) {
                                // LLVM_DEBUG(dbgs() << "      Found jump table symbol!\n");
                                return true;
                            }
                        }
                    }
                }
            }
            // LLVM_DEBUG(dbgs() << "    No matching LEA found\n");
        }
    }

    return false;
}

bool X86LabelIndirectCallTarget::isJumpTableRelated(MachineInstr &MI, 
                                              const MachineJumpTableEntry &JTEntry,
                                              MachineFunction &MF) {
    if (!MI.isIndirectBranch()) {
        // LLVM_DEBUG(dbgs() << "Not an indirect branch, skipping\n");
        return false;
    }

    // LLVM_DEBUG(dbgs() << "\nAnalyzing indirect jump: " << MI << "\n");

    // Get jump register
    Register JumpReg;
    for (const MachineOperand &MO : MI.operands()) {
        if (MO.isReg() && MO.isUse()) {
            JumpReg = MO.getReg();
            // LLVM_DEBUG(dbgs() << "Found jump register: " << printReg(JumpReg, nullptr) << "\n");
            break;
        }
    }

    if (!JumpReg) {
        // LLVM_DEBUG(dbgs() << "No jump register found\n");
        return false;
    }

    SmallVector<MachineInstr*, 8> Worklist;
    SmallPtrSet<MachineInstr*, 16> Visited;
    
    // LLVM_DEBUG(dbgs() << "Starting backward analysis from register " << printReg(JumpReg, nullptr) << "\n");

    for (MachineInstr &DefMI : MF.getRegInfo().def_instructions(JumpReg)) {
        Worklist.push_back(&DefMI);
        // LLVM_DEBUG(dbgs() << "Added to worklist: " << DefMI << "\n");
    }

    while (!Worklist.empty()) {
        MachineInstr *CurrMI = Worklist.pop_back_val();
        if (!Visited.insert(CurrMI).second) {
            // LLVM_DEBUG(dbgs() << "Already visited: " << *CurrMI << "\n");
            continue;
        }

        // LLVM_DEBUG(dbgs() << "Analyzing instruction: " << *CurrMI << "\n");

        if (isJumpTableLoad(*CurrMI, JTEntry)) {
            // LLVM_DEBUG(dbgs() << "Found jump table load!\n");
            return true;
        }

        if (CurrMI->getOpcode() == X86::ADD64rr) {
            // LLVM_DEBUG(dbgs() << "Found ADD64rr, checking operands\n");
            for (const MachineOperand &MO : CurrMI->operands()) {
                if (MO.isReg() && MO.isUse()) {
                    // LLVM_DEBUG(dbgs() << "Checking register operand: " << printReg(MO.getReg(), nullptr) << "\n");
                    for (MachineInstr &DefMI : MF.getRegInfo().def_instructions(MO.getReg())) {
                        if (isJumpTableLoad(DefMI, JTEntry)) {
                            // LLVM_DEBUG(dbgs() << "Found jump table load via ADD operand!\n");
                            return true;
                        }
                    }
                }
            }
        }

        // Add uses to worklist
        for (const MachineOperand &MO : CurrMI->operands()) {
            if (MO.isReg() && MO.isUse()) {
                // LLVM_DEBUG(dbgs() << "Adding definitions of register " << printReg(MO.getReg(), nullptr) << " to worklist\n");
                for (MachineInstr &DefMI : MF.getRegInfo().def_instructions(MO.getReg())) {
                    if (!Visited.count(&DefMI)) {
                        Worklist.push_back(&DefMI);
                        // LLVM_DEBUG(dbgs() << "Added to worklist: " << DefMI << "\n");
                    }
                }
            }
        }
    }

    // LLVM_DEBUG(dbgs() << "No jump table relation found\n");
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