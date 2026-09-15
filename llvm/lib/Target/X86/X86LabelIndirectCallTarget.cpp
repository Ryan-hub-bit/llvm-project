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
#include <sstream>
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/PassRegistry.h"
#include "llvm/Pass.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/ADT/Statistic.h"
#include <fstream>
#include <iostream> 
#include <sstream> // For std::stringstream




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

enum FunctionLabelKind : int {
    NoFunctionLabel = 0,
    IndirectTargetFunction = 1,
    SameTypeNonAddressTakenFunction = 2,
};

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
            if (TMDS->getString().ends_with(".generalized")) {
                MDGeneralizedTypeId = TMDS;
                break;
            }
        }
    }
    if (!MDGeneralizedTypeId) {
        return 0;  // Return 0 instead of nullptr
    }
    // Directly return the hash value
    return llvm::MD5Hash(MDGeneralizedTypeId->getString());
}

/// Extracts the generalized numeric type identifier attached to an indirect
/// call. Prefer the operand bundle emitted by Clang, but also accept the call
/// metadata form used by this tree.
static uint64_t extractNumericCGTypeId(const CallBase &CB) {
    if (!CB.isIndirectCall())
        return 0;

    auto HashGeneralizedTypeId = [](Metadata *MD) -> uint64_t {
        auto *TypeId = dyn_cast_or_null<MDString>(MD);
        if (!TypeId || !TypeId->getString().ends_with(".generalized"))
            return 0;
        return llvm::MD5Hash(TypeId->getString());
    };

    if (auto TypeBundle = CB.getOperandBundle(LLVMContext::OB_type)) {
        if (TypeBundle->Inputs.size() == 1) {
            if (auto *MAV =
                    dyn_cast<MetadataAsValue>(TypeBundle->Inputs.front().get()))
                if (uint64_t TypeId =
                        HashGeneralizedTypeId(MAV->getMetadata()))
                    return TypeId;
        }
    }

    if (MDNode *TypeMD = CB.getMetadata(LLVMContext::MD_type)) {
        for (const MDOperand &Operand : TypeMD->operands())
            if (uint64_t TypeId = HashGeneralizedTypeId(Operand.get()))
                return TypeId;
    }

    return 0;
}
// modifier-jumptableindex-tailcallID-callsiteID-calleeTypeID-DtailcallID-t-jumptableIndex-jumpEntryIndex-returnID-FunctionID-functionhash-functionTypeID
static const std::string& initializeLabel(StringRef moIdentifier) {
    static std::string result = moIdentifier.str();
    // Only initialize if this is the first call
    static bool initialized = false;
    if (!initialized) {
        // Add elements 2-5 (four zeros)
        for(int i = 0; i < 5; i++) {
            result += "-0";
        }
        // Add element 6 ('t')
        result += "-t";
        // Add elements 7-11 (four more zeros)
        for(int i = 0; i < 5; i++) {
            result += "-0";
        }
        // Add the final element
        result += "-type";
        initialized = true;
    }
    return result;
}

// modifier-jumptableindex-tailcallID-callsiteID-calleeTypeID-DtailcallID-t-jumptableIndex-jumpEntryIndex-returnID-FunctionID-functionhash-functionTypeID
std::string modifyJumptableLabel(const std::string& originalStr, int value1) {
    std::vector<std::string> elements;
    std::string temp;
    
    // Split the original string by '-'
    for(char c : originalStr) {
        if(c == '-') {
            elements.push_back(temp);
            temp.clear();
        } else {
            temp += c;
        }
    }
    // Don't forget to add the last element
    elements.push_back(temp);
    
    // Modify only second and third elements
    if(elements.size() >= 2) {
        elements[1] = std::to_string(value1);
    }
    
    // Reconstruct the string
    std::string result = elements[0];
    for(size_t i = 1; i < elements.size(); i++) {
        result += "-" + elements[i];
    }
    
    return result;
}
// modifier-jumptableindex-tailcallID-callsiteID-calleeTypeID-DtailcallID-t-jumptableIndex-jumpEntryIndex-returnID-FunctionID-functionhash-functionTypeID
std::string modifyJumpEntry(const std::string& originalStr, int value1, int value2) {
    std::vector<std::string> elements;
    std::string temp;
    
    // Split the original string by '-'
    for(char c : originalStr) {
        if(c == '-') {
            elements.push_back(temp);
            temp.clear();
        } else {
            temp += c;
        }
    }
    // Don't forget to add the last element
    elements.push_back(temp);

    // Modify only seventh and eighth elements (index 6 and 7)
    if(elements.size() >= 9) {
        elements[7] = std::to_string(value1);
        elements[8] = std::to_string(value2);
    }
    
    // Reconstruct the string
    std::string result = elements[0];
    for(size_t i = 1; i < elements.size(); i++) {
        result += "-" + elements[i];
    }
    return result;
}
// modifier-jumptableindex-tailcallID-callsiteID-calleeTypeID-DtailcallID-t-jumptableIndex-jumpEntryIndex-returnID-FunctionID-functionhash-functionTypeID
std::string modifyTailcallSource(const std::string& originalStr, int value1, uint64_t value2) {
    std::vector<std::string> elements;
    std::string temp;
    
    // Split the original string by '-'
    for(char c : originalStr) {
        if(c == '-') {
            elements.push_back(temp);
            temp.clear();
        } else {
            temp += c;
        }
    }
    // Don't forget to add the last element
    elements.push_back(temp);
    
    // Modify elements at index 2 and 4
    if(elements.size() >= 5) {  // Changed from 8 to 5 since we now need at least 5 elements
        elements[2] = std::to_string(value1);
        std::stringstream ss;
        ss << std::hex << value2;
        elements[4] = ss.str();
    }
    
    // Reconstruct the string
    std::string result = elements[0];
    for(size_t i = 1; i < elements.size(); i++) {
        result += "-" + elements[i];
    }
    return result;
}

// modifier-jumptableindex-tailcallID-callsiteID-calleeTypeID-DtailcallID-t-jumptableIndex-jumpEntryIndex-returnID-FunctionID-functionhash-functionTypeID
std::string modifydirectTailcallSource(const std::string& originalStr, int value1) {
    std::vector<std::string> elements;
    std::string temp;
    
    // Split the original string by '-'
    for(char c : originalStr) {
        if(c == '-') {
            elements.push_back(temp);
            temp.clear();
        } else {
            temp += c;
        }
    }
    // Don't forget to add the last element
    elements.push_back(temp);
    
    // Modify elements at index 2 and 4
    if(elements.size() >= 6) {  // Changed from 8 to 5 since we now need at least 5 elements
        elements[5] = std::to_string(value1);
    }
    
    // Reconstruct the string
    std::string result = elements[0];
    for(size_t i = 1; i < elements.size(); i++) {
        result += "-" + elements[i];
    }
    return result;
}
// modifier-jumptableindex-tailcallID-callsiteID-calleeTypeID-DtailcallID-t-jumptableIndex-jumpEntryIndex-returnID-FunctionID-functionhash-functionTypeID
std::string modifyCallsiteSource(const std::string& originalStr, int value1, uint64_t value2) {
    std::vector<std::string> elements;
    std::string temp;
    
    // Split the original string by '-'
    for(char c : originalStr) {
        if(c == '-') {
            elements.push_back(temp);
            temp.clear();
        } else {
            temp += c;
        }
    }
    // Don't forget to add the last element
    elements.push_back(temp);
    
    // Modify elements at index 2 and 4
    if(elements.size() >= 5) {  // Changed from 8 to 5 since we now need at least 5 elements
        elements[3] = std::to_string(value1);
        // elements[4] = std::to_string(value2);
        std::stringstream ss;
        ss << std::hex << value2;
        elements[4] = ss.str();
    }
    
    // Reconstruct the string
    std::string result = elements[0];
    for(size_t i = 1; i < elements.size(); i++) {
        result += "-" + elements[i];
    }
    return result;
}
// modifier-jumptableindex-tailcallID-callsiteID-calleeTypeID-DtailcallID-t-jumptableIndex-jumpEntryIndex-returnID-FunctionID-functionhash-functionTypeID
std::string modifyReturnTarget(const std::string& originalStr, int value1, uint64_t value2, uint64_t value3) {
    std::vector<std::string> elements;
    std::string temp;
    
    for(char c : originalStr) {
        if(c == '-') {
            elements.push_back(temp);
            temp.clear();
        } else {
            temp += c;
        }
    }
    elements.push_back(temp);
    
    if(elements.size() >= 4) {
        size_t lastIndex = elements.size() - 1;
        elements[lastIndex-3] = std::to_string(value1);
        std::stringstream ss;
        ss << std::hex << value2;
        elements[lastIndex-1] = ss.str();
        std::stringstream sslast;
        sslast << std::hex << value3;
        elements[lastIndex] = sslast.str();
    }
    
    std::string result = elements[0];
    for(size_t i = 1; i < elements.size(); i++) {
        result += "-" + elements[i];
    }
    return result;
}
// modifier-jumptableindex-tailcallID-callsiteID-calleeTypeID-DtailcallID-t-jumptableIndex-jumpEntryIndex-returnID-FunctionID-functionhash-functionTypeID
std::string modifyFunctionStarting(const std::string& originalStr, int value1, uint64_t value2, uint64_t value3) {
    std::vector<std::string> elements;
    std::string temp;
    
    for(char c : originalStr) {
        if(c == '-') {
            elements.push_back(temp);
            temp.clear();
        } else {
            temp += c;
        }
    }
    elements.push_back(temp);
    
    if(elements.size() >= 4) {
        size_t lastIndex = elements.size() - 1;
        elements[lastIndex-2] = std::to_string(value1);
        std::stringstream ss;
        ss << std::hex << value2;
        elements[lastIndex-1] = ss.str();
        std::stringstream sslast;
        sslast << std::hex << value3;
        elements[lastIndex] = sslast.str();
    }
    
    std::string result = elements[0];
    for(size_t i = 1; i < elements.size(); i++) {
        result += "-" + elements[i];
    }
    return result;
}

bool X86LabelIndirectCallTarget::doInitialization(Module &M) {
    IndirectCallTypeIds.clear();

    // Collect callsite types before visiting any MachineFunction. Otherwise a
    // same-typed function would only be found when its machine function happens
    // to be emitted after the caller.
    for (const Function &F : M) {
        for (const BasicBlock &BB : F) {
            for (const Instruction &I : BB) {
                const auto *CB = dyn_cast<CallBase>(&I);
                if (!CB)
                    continue;
                if (uint64_t TypeId = extractNumericCGTypeId(*CB))
                    IndirectCallTypeIds.insert(TypeId);
            }
        }
    }

    return false;
}

bool X86LabelIndirectCallTarget::runOnMachineFunction(MachineFunction &MF) {

    // jump table label first
    Function &F = MF.getFunction();
    StringRef modulestr = F.getParent()->getModuleIdentifier();
    std::string moIdentifierstr = modulestr.str();


    // Replace '-' with '.'
    size_t pos = 0;
    while ((pos = moIdentifierstr.find('-', pos)) != std::string::npos) {
        moIdentifierstr[pos] = '.';
        pos++;
    }

    // Replace '_' with '.'
    pos = 0;
    while ((pos = moIdentifierstr.find('_', pos)) != std::string::npos) {
        moIdentifierstr[pos] = '.';
        pos++;
    }

    // Convert back to StringRef if needed
    StringRef moIdentifier(moIdentifierstr);

    //direct tail call 
    for (MachineBasicBlock &MBB : MF) {
        for ( MachineInstr &MI : MBB) {
             unsigned Opc = MI.getOpcode();
                if(MI.isCall()) {
                    if (Opc == X86::TCRETURNdi || Opc == X86::TAILJMPd64 || Opc == X86::TAILJMPd_CC){
                        // errs() << "opc:" << Opc<<"\n";
                        llvm::MachineInstr* MIptr = &MI;
                             if(MIptr->getPreInstrSymbol()) {
                                MCSymbol *Label = MIptr->getPreInstrSymbol();
                                std::string labelName = Label->getName().str();  // Store it somewhere permanent
                                const std::string& labelRef = labelName;
                                std::string modifiedLabel = modifydirectTailcallSource(labelRef, DtailcallID);
                                MCSymbol *newLabel = MF.getContext().getOrCreateSymbol(modifiedLabel);
                                MIptr->setPreInstrSymbol(MF, newLabel);
                             } else {
                                const std::string& labelName = initializeLabel(moIdentifier);
                                std::string modifiedLabel = modifydirectTailcallSource(labelName,DtailcallID);
                                // errs()<< "direct tailcall modifiedLabel:" << modifiedLabel <<"\n";
                                MCSymbol *Label = MF.getContext().getOrCreateSymbol(modifiedLabel);
                                MIptr->setPreInstrSymbol(MF, Label);
                             }
                             DtailcallID ++;
                    }
                }
                
            }
        }
    MachineJumpTableInfo *JTI = MF.getJumpTableInfo();
    if (JTI) {
    // Map to store JumpTable Index -> Source BB mapping
    std::map<unsigned, MachineBasicBlock*> JumpTableSources;

    // Scan once to find all jump table sources
    // for (MachineBasicBlock &MBB : MF) {
    //     for ( MachineInstr &MI : MBB) {
    //         for (const MachineOperand &MO : MI.operands()) {
    //             if (MO.isJTI()) {
    //                 unsigned JTIndex = MO.getIndex();

    //                 JumpTableSources[JTIndex] = MI.getParent();
    //             }
    //         }
    //     }
    // }
    for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
        for (const MachineOperand &MO : MI.operands()) {
            if (MO.isJTI()) {
                unsigned JTIndex = MO.getIndex();
                MachineBasicBlock *ParentBlock = MI.getParent();
                
                // Check if this block's terminator is an indirect branch
                if (!ParentBlock->empty()) {
                    MachineInstr &TermInstr = ParentBlock->instr_back();
                    if (TermInstr.isIndirectBranch()) {
                        // Only assign as jump table source if it has an indirect branch terminator
                        JumpTableSources[JTIndex] = ParentBlock;
                    }
                }
            }
        }
    }
}

    


    // errs() << "JTSource:" << JumpTableSources.size() <<"\n";
    // errs() << "size:" << JTI->getJumpTables().size() <<"\n";
        // Now you have all jump tables' sources
    for (unsigned JTIndex = 0; JTIndex < JTI->getJumpTables().size(); JTIndex++) {
        if (auto *SourceBB = JumpTableSources[JTIndex]) {
            // errs() << "sourceBB:" <<JTIndex << SourceBB <<"\n";
            // errs() << "Jump Table " << JTIndex << " source block: " << SourceBB->getNumber() << "\n";
            // You can also get targets:
            if(!SourceBB -> empty()){
                MachineInstr &FirstInstr = SourceBB->front();
                    if(FirstInstr.getPreInstrSymbol()) {  // Check if first instruction has a symbol
                        MCSymbol *Label = FirstInstr.getPreInstrSymbol();
                        std::string labelN = Label->getName().str();
                        const std::string& labelR = labelN;
                        std::string modifiedLabel = modifyJumptableLabel(labelR, Runcount);
                        // errs()<< "modifiedLabel:" << modifiedLabel <<"\n";
                        MCSymbol *newLabel = MF.getContext().getOrCreateSymbol(modifiedLabel);
                        FirstInstr.setPreInstrSymbol(MF, newLabel);
                    } else {
                        const std::string& labelName = initializeLabel(moIdentifier);
                        std::string modifiedLabel = modifyJumptableLabel(labelName, Runcount);
                        // errs()<< "jumptableLabel:" << modifiedLabel <<"\n";
                        MCSymbol *Label = MF.getContext().getOrCreateSymbol(modifiedLabel);
                        FirstInstr.setPreInstrSymbol(MF, Label);
                    }

            }
            const MachineJumpTableEntry &JTE = JTI->getJumpTables()[JTIndex];
            for (unsigned EntryIndex = 0; EntryIndex < JTE.MBBs.size(); EntryIndex++) {
                            MachineBasicBlock *TargetMBB = JTE.MBBs[EntryIndex];
                            if (!TargetMBB->empty()) {
                                MachineInstr &FirstInstr = TargetMBB->front();
                                if(FirstInstr.getPreInstrSymbol()) {
                                    MCSymbol *Label = FirstInstr.getPreInstrSymbol();
                                    std::string labelName = Label->getName().str();
                                    const std::string& labelRef = labelName;
                                    std::string modifiedLabel = modifyJumpEntry(labelRef, Runcount, EntryIndex + 1);
                                    // errs()<< "modifiedLabel:" << modifiedLabel <<"\n";
                                    MCSymbol *newLabel = MF.getContext().getOrCreateSymbol(modifiedLabel);
                                    FirstInstr.setPreInstrSymbol(MF, newLabel);
                                } else {
                                    const std::string& labelName = initializeLabel(moIdentifier);
                                    std::string modifiedLabel = modifyJumpEntry(labelName, Runcount, EntryIndex + 1);
                                    MCSymbol *Label = MF.getContext().getOrCreateSymbol(modifiedLabel);
                                    FirstInstr.setPreInstrSymbol(MF, Label);
                                }
                            }
                 }
        }
        Runcount ++;
    }
}

    const auto &CallSitesInfoMap = MF.getCallSitesInfo();  // Use '.' instead of '->'
    const TargetMachine &TM = MF.getTarget();  // Ensure TargetMachine is referenced correctly
    // bool recordnext = false;
    // uint64_t lastTypeId = 0;
     //logic for tail call and indirect call
    for (auto &MBB : MF) {
        for (auto &MI : MBB) {
            if (MI.isCall()) {
                const auto &CallSiteInfo = CallSitesInfoMap.find(&MI);
                if (CallSiteInfo != CallSitesInfoMap.end()) {
                    // Generate labelName based on callsiteID
                    if (auto *TypeId = CallSiteInfo->second.TypeId) {
                        uint64_t TypeIdVal = TypeId->getZExtValue();  // Can be used later if needed
                        // lastTypeId = TypeIdVal;
                        if (X86LabelIndirectCallTarget::TailJumps.count(MI.getOpcode())) {
                            std::string labelName = "tailcallsite_" + std::to_string(tailcallID);
                            callsitetoTypeID[labelName].insert(TypeIdVal);
                            
                            // MCSymbol *Label = MF.getContext().getOrCreateSymbol(labelName);
                            llvm::MachineInstr* MIptr = &MI;
                             if(MIptr->getPreInstrSymbol()) {
                                MCSymbol *Label = MIptr->getPreInstrSymbol();
                                std::string labelName = Label->getName().str();  // Store it somewhere permanent
                                const std::string& labelRef = labelName;
                                std::string modifiedLabel = modifyTailcallSource(labelRef, tailcallID, TypeIdVal);
                                MCSymbol *newLabel = MF.getContext().getOrCreateSymbol(modifiedLabel);
                                MIptr->setPreInstrSymbol(MF, newLabel);
                            // MIptr->setPreInstrSymbol(MF, Label);
                            // errs() << "tailcallID:" << tailcallID << "\n";
                             } else {
                                const std::string& labelName = initializeLabel(moIdentifier);
                                std::string modifiedLabel = modifyTailcallSource(labelName, tailcallID, TypeIdVal);
                                errs()<< "tailcall modifiedLabel:" << modifiedLabel <<"\n";
                                MCSymbol *Label = MF.getContext().getOrCreateSymbol(modifiedLabel);
                                MIptr->setPreInstrSymbol(MF, Label);
                             }
                            tailcallID++;
                        } else {
                            std::string labelName = "callsite_" + std::to_string(callsiteID);
                            callsitetoTypeID[labelName].insert(TypeIdVal);
                             llvm::MachineInstr* MIptr = &MI;
                             if(MIptr->getPreInstrSymbol()) {
                                MCSymbol *Label = MIptr->getPreInstrSymbol();
                                std::string labelName = Label->getName().str();  // Store it somewhere permanent
                                const std::string& labelRef = labelName;
                                std::string modifiedLabel = modifyCallsiteSource(labelRef, callsiteID, TypeIdVal);
                                MCSymbol *newLabel = MF.getContext().getOrCreateSymbol(modifiedLabel);
                                MIptr->setPreInstrSymbol(MF, newLabel);
                             } else {
                                const std::string& labelName = initializeLabel(moIdentifier);
                                std::string modifiedLabel = modifyCallsiteSource(labelName, callsiteID, TypeIdVal);
                                errs()<< "indirect call modifiedLabel:" << modifiedLabel <<"\n";
                                MCSymbol *Label = MF.getContext().getOrCreateSymbol(modifiedLabel);
                                MIptr->setPreInstrSymbol(MF, Label);
                             }
                             callsiteID++;
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
    uint64_t FunctionTypeId = extractNumericCGTypeId(CF);
    bool HasIndirectCallType =
        FunctionTypeId != 0 && IndirectCallTypeIds.count(FunctionTypeId);
    uint64_t TypeIdVal = 0;
    FunctionLabelKind FunctionKind = NoFunctionLabel;

    if (IsIndirectTarget) {
        // Preserve the original indirect-target label kind.
        TypeIdVal = FunctionTypeId;
        FunctionKind = IndirectTargetFunction;
    } else if (HasIndirectCallType) {
        // A local, non-address-taken function whose type is used by an
        // indirect call. Keep it distinguishable from a real indirect target.
        TypeIdVal = FunctionTypeId;
        FunctionKind = SameTypeNonAddressTakenFunction;
    }

    // First, process the function entry point
    if (!MF.empty() && TypeIdVal != 0) {
        MachineBasicBlock &EntryMBB = MF.front();
        if (!EntryMBB.empty()) {
            auto MI = EntryMBB.begin();
            llvm::MachineInstr* MIptr = &*MI;
            
            if (MIptr->getPreInstrSymbol()) {
                MCSymbol *Label = MIptr->getPreInstrSymbol();
                std::string labelName = Label->getName().str();
                const std::string& labelRef = labelName;
                std::string modifiedLabel = modifyFunctionStarting(labelRef, FunctionKind, FHash, TypeIdVal);
                MCSymbol *newLabel = MF.getContext().getOrCreateSymbol(modifiedLabel);
                MIptr->setPreInstrSymbol(MF, newLabel);
            } else {
                const std::string& labelName = initializeLabel(moIdentifier);
                std::string modifiedLabel = modifyFunctionStarting(labelName, FunctionKind, FHash, TypeIdVal);
                // errs() << "Function entry label: " << modifiedLabel << "\n";
                MCSymbol *Label = MF.getContext().getOrCreateSymbol(modifiedLabel);
                MIptr->setPreInstrSymbol(MF, Label);
            }
        }
    }

    unsigned ReturnCounter = 1;  // Counter for this function only
    for (auto &MBB : MF) {
        if (!MBB.empty() && TypeIdVal != 0) {
            MachineBasicBlock::iterator Terminator = MBB.terminators().begin();
            if (Terminator != MBB.end() && Terminator->isReturn()) {
                // assert(MBB.begin() == MI && "Not at block start");
                auto MI = MBB.begin();
                llvm::MachineInstr* MIptr = &*MI;
                 if(MIptr->getPreInstrSymbol()) {
                        MCSymbol *Label = MIptr->getPreInstrSymbol();
                        std::string labelName = Label->getName().str();  // Store it somewhere permanent
                        const std::string& labelRef = labelName;
                        std::string modifiedLabel = modifyReturnTarget(labelRef, ReturnCounter, FHash, TypeIdVal);
                        MCSymbol *newLabel = MF.getContext().getOrCreateSymbol(modifiedLabel);
                        MIptr->setPreInstrSymbol(MF, newLabel);
                        } else {
                        const std::string& labelName = initializeLabel(moIdentifier);
                        std::string modifiedLabel = modifyReturnTarget(labelName, ReturnCounter, FHash, TypeIdVal);
                        errs()<< "modifiedLabel:" << modifiedLabel <<"\n";
                        MCSymbol *Label = MF.getContext().getOrCreateSymbol(modifiedLabel);
                        MIptr->setPreInstrSymbol(MF, Label);
                    }
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


FunctionPass *createX86LabelIndirectCallTargetPass() {
    return new X86LabelIndirectCallTarget();
}

void initializeX86LabelIndirectCallPass(PassRegistry &Registry) {
    RegisterPass<X86LabelIndirectCallTarget> X("x86-label-indirect-call", 
                                              "X86 Label Indirect Call Target Pass",
                                              false, false);
}

} // namespace llvm
