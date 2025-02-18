// #include "llvm/Transforms/IPO/InterProceduralGraph.h"
// #include "llvm/IR/Module.h"
// #include "llvm/IR/Function.h"
// #include "llvm/IR/Instructions.h"
// #include "llvm/Analysis/CallGraph.h"
// #include "llvm/Passes/PassBuilder.h"
// #include "llvm/Support/raw_ostream.h"
// #include "llvm/Support/FormatVariadic.h"
// #include "llvm/ADT/SCCIterator.h"
// #include "llvm/Support/Path.h"
// #include <map>
// #include <vector>
// #include <set>
// #include <iostream>
// #include <fstream>
// #include <list>
// #include <algorithm>
// #include <cstdlib>   // For rand() and srand()
// #include <ctime>     // For time()
// #include <string> 

// using namespace llvm;

// namespace {

// // Data structures for the graph
// struct Node {
//     std::string name;
//     llvm::BasicBlock* BB;
//     std::string FunctionName;

//     // Default constructor
//     Node() : name(""), BB(nullptr), FunctionName("") {}

//     // Parameterized constructor
//     Node(const std::string& n, llvm::BasicBlock* bb, const std::string& fn)
//         : name(n), BB(bb), FunctionName(fn) {}
// };

// struct Edge {
//     llvm::BasicBlock* from;
//     llvm::BasicBlock* to;
//     std::string edgeType;  // [Inter,Intra,return]

//     Edge() : from(nullptr), to(nullptr), edgeType("") {}

//     Edge(llvm::BasicBlock* from, llvm::BasicBlock* to, const std::string& edgeType)
//         : from(from), to(to), edgeType(edgeType) {}

//     // Overload the equality operator for comparison
//     bool operator==(const Edge& other) const {
//         return from == other.from && to == other.to;
//     }

//     // Overload the less-than operator for sorting and ordering in a set
//     bool operator<(const Edge& other) const {
//         if (from != other.from)
//             return from < other.from;
//         return to < other.to;
//     }
// };

// struct InterproceduralGraph {
//     std::map<llvm::BasicBlock*, Node> nodes;
//     std::vector<Edge> edges;
//     std::map<llvm::BasicBlock*, std::set<llvm::BasicBlock*>> returnBlockMap;
//     std::map<llvm::BasicBlock*, std::set<llvm::BasicBlock*>> exceptionBlockMap;
//     std::vector<llvm::BasicBlock*> keyList;      // Store keys
//     std::vector<llvm::BasicBlock*> valueList;    // Store all values from sets
//     std::vector<llvm::BasicBlock*> exceptionKeyList;      // Store keys
//     std::vector<llvm::BasicBlock*> exceptionValueList;    // Store all values from sets
//     std::vector<int> countList;                  // Store count of values per key

//     llvm::BasicBlock* getSig(const Node& node) {
//         return node.BB;
//     }

//     void addNode(const Node& node) {
//         llvm::BasicBlock* sig = getSig(node);
//         nodes[sig] = node;
//     }

//     void addEdge(const Edge& edge) {
//         // Check if the edge is already in the vector
//         if (std::find(edges.begin(), edges.end(), edge) == edges.end()) {
//             edges.push_back(edge);
//         }
//     }

// bool isFromSourceCode(const Function* F) {
//     if (DISubprogram* SP = F->getSubprogram()) {
//         StringRef directory = SP->getDirectory();
//         StringRef filename = SP->getFilename();
//         // Combine them to get full path
//         SmallString<256> fullPath;
//         sys::path::append(fullPath, directory, filename);
//         errs() << "  Full path: " << fullPath << "\n";
//         // Now you can check if this path is from your source
//         return !fullPath.empty() && !fullPath.starts_with("/usr/include") && !fullPath.starts_with("/usr/lib");
//     }
//     return false;
// }
    
// void findReturnEdges(CallGraph& CG) {
//     for (auto& nodePair : CG) {
//         const Function* caller = nodePair.first;
//         CallGraphNode* cgn = nodePair.second.get();
//         if (!caller) continue;

//         for (auto it = cgn->begin(); it != cgn->end(); ++it) {
//             CallGraphNode::CallRecord callRecord = *it;
//             Function* callee = callRecord.second->getFunction();
//             if (!callee || callee->isDeclaration()) continue;
//             if (!isFromSourceCode(callee))  {
//                 errs() << "this callee not from source Code" << "\n";
//                 continue;
//             }

//             for (const BasicBlock& CBB : *caller) {
//                 for (const Instruction& I : CBB) {
//                     if (const CallBase* CB = dyn_cast<CallBase>(&I)) {
//                         if (CB->getCalledFunction() == callee) {
//                             // Create caller signature
//                             Node callerNode = Node(CBB.getName().str(), 
//                                                 const_cast<BasicBlock*>(&CBB), 
//                                                 caller->getName().str());
//                             BasicBlock* callersig = getSig(callerNode);

//                             if (const InvokeInst* invoke = dyn_cast<InvokeInst>(CB)) {
//                                 BasicBlock* unwindDest = invoke->getUnwindDest();
//                                 if (!unwindDest) continue;

//                                 // Get the catch block
//                                 const BasicBlock* catchBlock = unwindDest;
                                
//                                 // Look for landing pad instruction in catch block
//                                 for (const Instruction& UnwindInst : *catchBlock) {
//                                     if (const LandingPadInst* LP = dyn_cast<LandingPadInst>(&UnwindInst)) {
//                                         // Check all clauses in landing pad
//                                         for (unsigned i = 0; i < LP->getNumClauses(); ++i) {
//                                             if (LP->isCatch(i)) {
//                                                 // Create exception signature and add to map
//                                                 Node exceptionNode = Node(unwindDest->getName().str(),
//                                                                     unwindDest,
//                                                                     caller->getName().str());
//                                                 BasicBlock* exceptionsig = getSig(exceptionNode);
//                                                 exceptionBlockMap[callersig].insert(exceptionsig);
                                                
//                                                 errs() << "Found exception handler in function: " 
//                                                     << caller->getName() << "\n";
//                                                 errs() << "  Caller block: " << CBB.getName() << "\n";
//                                                 errs() << "  Exception block: " << unwindDest->getName() << "\n\n";
//                                                 break;
//                                             }
//                                         }
//                                     }
//                                 }
//                             }

//                             // Handle normal returns (unchanged)
//                             for (BasicBlock& calleeBB : *callee) {
//                                 for (Instruction& I : calleeBB) {
//                                     if (isa<ReturnInst>(&I)) {
//                                         Node calleeNode = Node(calleeBB.getName().str(),
//                                                             &calleeBB,
//                                                             callee->getName().str());
//                                         BasicBlock* calleesig = getSig(calleeNode);
//                                         returnBlockMap[callersig].insert(calleesig);
//                                     }
//                                 }
//                             }
//                         }
//                     }
//                 }
//             }
//         }
//     }
// }
// };

// void insertAddrListtoSection(Module& M,
//                            std::vector<llvm::BasicBlock*>& keyList,
//                            std::vector<llvm::BasicBlock*>& valueList,
//                            std::vector<int>& countList,
//                            std::vector<llvm::BasicBlock*>& exceptionKeyList,
//                            std::vector<llvm::BasicBlock*>& exceptionValueList) {
//     LLVMContext& Context = M.getContext();
//     Type* Int64Ty = Type::getInt64Ty(Context);
    
//     int counter = 0;
//     for (BasicBlock* BB : keyList) {
//         if (!BB || !BB->getParent()) continue;
        
//         Function* parentFunc = BB->getParent();
//         srand(time(NULL) + counter); // Seed with time + callerNum for more variation
//         int randomNum = rand() % 1000;  // Random number between 0-99
//         std::string varName = M.getModuleIdentifier() + "/" + std::to_string(randomNum) + "_caller_" + std::to_string(counter);
        
//         // Create a more stable reference using function pointer for entry blocks
//         Constant* addrValue = nullptr;
//         if (BB == &parentFunc->getEntryBlock()) {
//             addrValue = parentFunc;
//         } else {
//             // For non-entry blocks, use a more stable indirect reference
//             addrValue = BlockAddress::get(parentFunc, BB);
//         }
        
//         // Convert to integer using a stable bit pattern
//         Constant* intValue = ConstantExpr::getPtrToInt(addrValue, Int64Ty);

//         // Create global variable with proper alignment and linkage
//         auto* GV = new GlobalVariable(
//             M,
//             Int64Ty,
//             true,                           // isConstant
//             GlobalValue::ExternalLinkage,   // linkage
//             intValue,                       // initializer
//             varName                         // name
//         );
//         GV->setSection(".section_for_caller");
//         GV->setAlignment(Align(8));
//         counter++;
//     }

//     counter = 0;
//     int callerid = 0;
//     srand(time(NULL) + counter); 
//     int randomNum = rand()%1000;
//     int previousNum = rand() % 1000;
//     for (BasicBlock* BB : valueList) {
//         if (!BB || !BB->getParent()) continue;
    
//         Function* parentFunc = BB->getParent();
//         // the last digit of return varname  is the callerid
//         if(countList[counter]  == 1) {
//             srand(time(NULL) + counter); // Seed with time + callerNum for more variation
//             randomNum = rand() % 1000;  // Random number between 0-99
//             counter ++;
//             if(counter != 1) {
//             callerid += 1;
//             }
//         } else {
//             randomNum = previousNum + 1;
//             countList[counter] = countList[counter] - 1;
//         }
    
//         std::string varName = M.getModuleIdentifier() + "/" + std::to_string(randomNum) + "_return_" + std::to_string(callerid);
        
//         Constant* addrValue = nullptr;
//         if (BB == &parentFunc->getEntryBlock()) {
//             addrValue = parentFunc;
//         } else {
//             addrValue = BlockAddress::get(parentFunc, BB);
//         }
        
//         Constant* intValue = ConstantExpr::getPtrToInt(addrValue, Int64Ty);

//         auto* GV = new GlobalVariable(
//             M,
//             Int64Ty,
//             true,
//             GlobalValue::ExternalLinkage,
//             intValue,
//             varName
//         );
//         GV->setSection(".section_for_return");
//         GV->setAlignment(Align(8));
//         previousNum = randomNum;
//     }
//     counter = 0;
//     for (BasicBlock* BB : exceptionKeyList) {
//         if (!BB || !BB->getParent()) continue;
        
//         Function* parentFunc = BB->getParent();
//         srand(time(NULL) + counter); // Seed with time + callerNum for more variation
//         randomNum = rand() % 1000;  // Random number between 0-99
//         std::string varName = M.getModuleIdentifier() + "/" + std::to_string(randomNum) + "_invoker_" + std::to_string(counter);
        
//         // Create a more stable reference using function pointer for entry blocks
//         Constant* addrValue = nullptr;
//         if (BB == &parentFunc->getEntryBlock()) {
//             addrValue = parentFunc;
//         } else {
//             // For non-entry blocks, use a more stable indirect reference
//             addrValue = BlockAddress::get(parentFunc, BB);
//         }
        
//         // Convert to integer using a stable bit pattern
//         Constant* intValue = ConstantExpr::getPtrToInt(addrValue, Int64Ty);

//         // Create global variable with proper alignment and linkage
//         auto* GV = new GlobalVariable(
//             M,
//             Int64Ty,
//             true,                           // isConstant
//             GlobalValue::ExternalLinkage,   // linkage
//             intValue,                       // initializer
//             varName                         // name
//         );
//         GV->setSection(".section_for_invoker");
//         GV->setAlignment(Align(8));
//         counter++;
//     }
//         counter = 0;
//       for (BasicBlock* BB : exceptionValueList) {
//         if (!BB || !BB->getParent()) continue;

//         Function* parentFunc = BB->getParent();
//         // the last digit of return varname  is the callerid
//         srand(time(NULL) + counter); // Seed with time + callerNum for more variation
//         randomNum = rand() % 1000;  // Random number between 0-99
//         std::string varName = M.getModuleIdentifier() + "/" + std::to_string(randomNum) + "_exception_" + std::to_string(counter);
        
//         Constant* addrValue = nullptr;
//         if (BB == &parentFunc->getEntryBlock()) {
//             addrValue = parentFunc;
//         } else {
//             addrValue = BlockAddress::get(parentFunc, BB);
//         }
        
//         Constant* intValue = ConstantExpr::getPtrToInt(addrValue, Int64Ty);

//         auto* GV = new GlobalVariable(
//             M,
//             Int64Ty,
//             true,
//             GlobalValue::ExternalLinkage,
//             intValue,
//             varName
//         );
//         GV->setSection(".section_for_exception");
//         GV->setAlignment(Align(8));
//         counter ++;
//     }
// }


// // Helper function that does the actual graph construction and output
// void interproceduralGraphImpl(Module& M, CallGraph& CG) {
//     InterproceduralGraph IPG;
//     // errs() << "in interProceduralGraph" <<"\n";
//     IPG.findReturnEdges(CG);
//     // Convert map to lists
//     for (const auto& pair : IPG.returnBlockMap) {
//         IPG.keyList.push_back(pair.first);
//         IPG.countList.push_back(pair.second.size());
//         for (const auto& value : pair.second) {
//             IPG.valueList.push_back(value);
//         }
//     }
//      for (const auto& pair : IPG.exceptionBlockMap) {
//         IPG.exceptionKeyList.push_back(pair.first);
//         for (const auto& value : pair.second) {
//             IPG.exceptionValueList.push_back(value);
//         }
//     }


//     insertAddrListtoSection(M, IPG.keyList, IPG.valueList, IPG.countList, IPG.exceptionKeyList, IPG.exceptionValueList);

//     IPG.returnBlockMap.clear();
//     IPG.keyList.clear();
//     IPG.valueList.clear();
//     IPG.countList.clear();
// }

// }  // end anonymous namespace

// PreservedAnalyses InterproceduralGraphPass::run(Module& M, ModuleAnalysisManager& AM) {
//     // Get the CallGraph from the AnalysisManager
//     CallGraph& CG = AM.getResult<CallGraphAnalysis>(M);

//     // Call the helper function to build and output the graph
//     interproceduralGraphImpl(M, CG);

//     // Indicate that no analyses are preserved after this pass runs
//     return PreservedAnalyses::all();
// }

#include "llvm/Transforms/IPO/InterProceduralGraph.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/Support/Path.h"
#include <map>
#include <vector>
#include <set>
#include <iostream>
#include <fstream>
#include <list>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <string>

using namespace llvm;

namespace {

struct Node {
    std::string name;
    llvm::BasicBlock* BB;
    std::string FunctionName;

    Node() : name(""), BB(nullptr), FunctionName("") {}

    Node(const std::string& n, llvm::BasicBlock* bb, const std::string& fn)
        : name(n)
        , BB(bb)
        , FunctionName(fn) {}
};

struct Edge {
    llvm::BasicBlock* from;
    llvm::BasicBlock* to;
    std::string edgeType;

    Edge() : from(nullptr), to(nullptr), edgeType("") {}

    Edge(llvm::BasicBlock* from, llvm::BasicBlock* to, const std::string& edgeType)
        : from(from)
        , to(to)
        , edgeType(edgeType) {}

    bool operator==(const Edge& other) const {
        return from == other.from && to == other.to;
    }

    bool operator<(const Edge& other) const {
        if (from != other.from)
            return from < other.from;
        return to < other.to;
    }
};

struct InterproceduralGraph {
    std::map<llvm::BasicBlock*, Node> nodes;
    std::vector<Edge> edges;
    std::map<llvm::BasicBlock*, std::set<llvm::BasicBlock*>> returnBlockMap;
    std::map<llvm::BasicBlock*, std::set<llvm::BasicBlock*>> exceptionBlockMap;
    std::vector<llvm::BasicBlock*> keyList;
    std::vector<llvm::BasicBlock*> valueList;
    std::vector<llvm::BasicBlock*> exceptionKeyList;
    std::vector<llvm::BasicBlock*> exceptionValueList;
    std::vector<int> countList;

    llvm::BasicBlock* getSig(const Node& node) {
        return node.BB;
    }

    void addNode(const Node& node) {
        llvm::BasicBlock* sig = getSig(node);
        nodes[sig] = node;
    }

    void addEdge(const Edge& edge) {
        if (std::find(edges.begin(), edges.end(), edge) == edges.end()) {
            edges.push_back(edge);
        }
    }

    bool isFromSourceCode(const Function* F) {
        if (DISubprogram* SP = F->getSubprogram()) {
            StringRef directory = SP->getDirectory();
            StringRef filename = SP->getFilename();
            SmallString<256> fullPath;
            sys::path::append(fullPath, directory, filename);
            // errs() << "  Full path: " << fullPath << "\n";
            return !fullPath.empty() && 
                   !fullPath.starts_with("/usr/include") && 
                   !fullPath.starts_with("/usr/lib");
        }
        return false;
    }

    void findReturnEdges(CallGraph& CG) {
        for (auto& nodePair : CG) {
            const Function* caller = nodePair.first;
            CallGraphNode* cgn = nodePair.second.get();
            if (!caller) continue;

            for (auto it = cgn->begin(); it != cgn->end(); ++it) {
                CallGraphNode::CallRecord callRecord = *it;
                Function* callee = callRecord.second->getFunction();
                if (!callee || callee->isDeclaration()) continue;
                if (!isFromSourceCode(callee)) {
                    // errs() << "this callee not from source Code" << "\n";
                    continue;
                }

                for (const BasicBlock& CBB : *caller) {
                    for (const Instruction& I : CBB) {
                        if (const CallBase* CB = dyn_cast<CallBase>(&I)) {
                            if (CB->getCalledFunction() == callee) {
                                Node callerNode = Node(
                                    CBB.getName().str(),
                                    const_cast<BasicBlock*>(&CBB),
                                    caller->getName().str()
                                );
                                BasicBlock* callersig = getSig(callerNode);

                                if (const InvokeInst* invoke = dyn_cast<InvokeInst>(CB)) {
                                    BasicBlock* unwindDest = invoke->getUnwindDest();
                                    if (!unwindDest) continue;

                                    const BasicBlock* catchBlock = unwindDest;
                                    for (const Instruction& UnwindInst : *catchBlock) {
                                        if (const LandingPadInst* LP = dyn_cast<LandingPadInst>(&UnwindInst)) {
                                            for (unsigned i = 0; i < LP->getNumClauses(); ++i) {
                                                if (LP->isCatch(i)) {
                                                    Node exceptionNode = Node(
                                                        unwindDest->getName().str(),
                                                        unwindDest,
                                                        caller->getName().str()
                                                    );
                                                    BasicBlock* exceptionsig = getSig(exceptionNode);
                                                    exceptionBlockMap[callersig].insert(exceptionsig);

                                                    // errs() << "Found exception handler in function: "
                                                    //       << caller->getName() << "\n"
                                                    //       << "  Caller block: " << CBB.getName() << "\n"
                                                    //       << "  Exception block: " << unwindDest->getName() << "\n\n";
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }

                                // for (BasicBlock& calleeBB : *callee) {
                                //     for (Instruction& I : calleeBB) {
                                //         if (isa<ReturnInst>(I)) {
                                //             Node calleeNode = Node(
                                //                 calleeBB.getName().str(),
                                //                 &calleeBB,
                                //                 callee->getName().str()
                                //             );
                                //             BasicBlock* calleesig = getSig(calleeNode);
                                //             returnBlockMap[callersig].insert(calleesig);
                                //         }
                                //     }
                                // }
                                for (BasicBlock& calleeBB : *callee) {
                                    if (isa<ReturnInst>(calleeBB.getTerminator())) {
                                        Node calleeNode = Node(
                                            calleeBB.getName().str(),
                                            &calleeBB,
                                            callee->getName().str()
                                        );
                                        BasicBlock* calleesig = getSig(calleeNode);
                                        returnBlockMap[callersig].insert(calleesig);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
};

void insertAddrListtoSection(
    Module& M,
    std::vector<llvm::BasicBlock*>& keyList,
    std::vector<llvm::BasicBlock*>& valueList,
    std::vector<int>& countList,
    std::vector<llvm::BasicBlock*>& exceptionKeyList,
    std::vector<llvm::BasicBlock*>& exceptionValueList
) {
    LLVMContext& Context = M.getContext();
    Type* Int64Ty = Type::getInt64Ty(Context);

    int counter = 0;
    for (BasicBlock* BB : keyList) {
        if (!BB || !BB->getParent()) continue;

        Function* parentFunc = BB->getParent();
        srand(time(NULL) + counter);
        int randomNum = rand() % 1000;
        std::string varName = M.getModuleIdentifier() + "/" + 
                             std::to_string(randomNum) + "_caller_" + 
                             std::to_string(counter);

        Constant* addrValue = nullptr;
        if (BB == &parentFunc->getEntryBlock()) {
            addrValue = parentFunc;
        } else {
            addrValue = BlockAddress::get(parentFunc, BB);
        }

        Constant* intValue = ConstantExpr::getPtrToInt(addrValue, Int64Ty);

        auto* GV = new GlobalVariable(
            M,
            Int64Ty,
            true,
            GlobalValue::ExternalLinkage,
            intValue,
            varName
        );
        GV->setSection(".section_for_caller");
        GV->setAlignment(Align(8));
        counter++;
    }

    counter = 0;
    int callerid = 0;
    srand(time(NULL) + counter);
    int randomNum = rand() % 1000;
    int previousNum = rand() % 1000;

    for (BasicBlock* BB : valueList) {
        if (!BB || !BB->getParent()) continue;

        Function* parentFunc = BB->getParent();
        if (countList[counter] == 1) {
            srand(time(NULL) + counter);
            randomNum = rand() % 1000;
            counter++;
            if (counter != 1) {
                callerid += 1;
            }
        } else {
            randomNum = previousNum + 1;
            countList[counter] = countList[counter] - 1;
        }

        std::string varName = M.getModuleIdentifier() + "/" + 
                             std::to_string(randomNum) + "_return_" + 
                             std::to_string(callerid);

        Constant* addrValue = nullptr;
        if (BB == &parentFunc->getEntryBlock()) {
            addrValue = parentFunc;
        } else {
            addrValue = BlockAddress::get(parentFunc, BB);
        }

        Constant* intValue = ConstantExpr::getPtrToInt(addrValue, Int64Ty);

        auto* GV = new GlobalVariable(
            M,
            Int64Ty,
            true,
            GlobalValue::ExternalLinkage,
            intValue,
            varName
        );
        GV->setSection(".section_for_return");
        GV->setAlignment(Align(8));
        previousNum = randomNum;
    }

    counter = 0;
    for (BasicBlock* BB : exceptionKeyList) {
        if (!BB || !BB->getParent()) continue;

        Function* parentFunc = BB->getParent();
        srand(time(NULL) + counter);
        randomNum = rand() % 1000;
        std::string varName = M.getModuleIdentifier() + "/" + 
                             std::to_string(randomNum) + "_invoker_" + 
                             std::to_string(counter);

        Constant* addrValue = nullptr;
        if (BB == &parentFunc->getEntryBlock()) {
            addrValue = parentFunc;
        } else {
            addrValue = BlockAddress::get(parentFunc, BB);
        }

        Constant* intValue = ConstantExpr::getPtrToInt(addrValue, Int64Ty);

        auto* GV = new GlobalVariable(
            M,
            Int64Ty,
            true,
            GlobalValue::ExternalLinkage,
            intValue,
            varName
        );
        GV->setSection(".section_for_invoker");
        GV->setAlignment(Align(8));
        counter++;
    }

    counter = 0;
    for (BasicBlock* BB : exceptionValueList) {
        if (!BB || !BB->getParent()) continue;

        Function* parentFunc = BB->getParent();
        srand(time(NULL) + counter);
        randomNum = rand() % 1000;
        std::string varName = M.getModuleIdentifier() + "/" + 
                             std::to_string(randomNum) + "_exception_" + 
                             std::to_string(counter);

        Constant* addrValue = nullptr;
        if (BB == &parentFunc->getEntryBlock()) {
            addrValue = parentFunc;
        } else {
            addrValue = BlockAddress::get(parentFunc, BB);
        }

        Constant* intValue = ConstantExpr::getPtrToInt(addrValue, Int64Ty);

        auto* GV = new GlobalVariable(
            M,
            Int64Ty,
            true,
            GlobalValue::ExternalLinkage,
            intValue,
            varName
        );
        GV->setSection(".section_for_exception");
        GV->setAlignment(Align(8));
        counter++;
    }
}

void interproceduralGraphImpl(Module& M, CallGraph& CG) {
    InterproceduralGraph IPG;
    IPG.findReturnEdges(CG);

    for (const auto& pair : IPG.returnBlockMap) {
        IPG.keyList.push_back(pair.first);
        IPG.countList.push_back(pair.second.size());
        for (const auto& value : pair.second) {
            IPG.valueList.push_back(value);
        }
    }

    for (const auto& pair : IPG.exceptionBlockMap) {
        IPG.exceptionKeyList.push_back(pair.first);
        for (const auto& value : pair.second) {
            IPG.exceptionValueList.push_back(value);
        }
    }

    insertAddrListtoSection(
        M,
        IPG.keyList,
        IPG.valueList,
        IPG.countList,
        IPG.exceptionKeyList,
        IPG.exceptionValueList
    );

    IPG.returnBlockMap.clear();
    IPG.keyList.clear();
    IPG.valueList.clear();
    IPG.countList.clear();
    IPG.exceptionKeyList.clear();
    IPG.exceptionValueList.clear();
}

} // end anonymous namespace

PreservedAnalyses InterproceduralGraphPass::run(Module& M, ModuleAnalysisManager& AM) {
    CallGraph& CG = AM.getResult<CallGraphAnalysis>(M);
    interproceduralGraphImpl(M, CG);
    return PreservedAnalyses::all();
}
