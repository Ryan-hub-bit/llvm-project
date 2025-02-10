#include "llvm/Transforms/IPO/InterProceduralGraph.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/ADT/SCCIterator.h"
#include <map>
#include <vector>
#include <set>
#include <iostream>
#include <fstream>
#include <list>
#include <algorithm>

using namespace llvm;

namespace {

// Data structures for the graph
struct Node {
    std::string name;
    llvm::BasicBlock* BB;
    std::string FunctionName;

    // Default constructor
    Node() : name(""), BB(nullptr), FunctionName("") {}

    // Parameterized constructor
    Node(const std::string& n, llvm::BasicBlock* bb, const std::string& fn)
        : name(n), BB(bb), FunctionName(fn) {}
};

struct Edge {
    llvm::BasicBlock* from;
    llvm::BasicBlock* to;
    std::string edgeType;  // [Inter,Intra,return]

    Edge() : from(nullptr), to(nullptr), edgeType("") {}

    Edge(llvm::BasicBlock* from, llvm::BasicBlock* to, const std::string& edgeType)
        : from(from), to(to), edgeType(edgeType) {}

    // Overload the equality operator for comparison
    bool operator==(const Edge& other) const {
        return from == other.from && to == other.to;
    }

    // Overload the less-than operator for sorting and ordering in a set
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
    std::vector<llvm::BasicBlock*> keyList;      // Store keys
    std::vector<llvm::BasicBlock*> valueList;    // Store all values from sets
    std::vector<int> countList;                  // Store count of values per key

    llvm::BasicBlock* getSig(const Node& node) {
        return node.BB;
    }

    void addNode(const Node& node) {
        llvm::BasicBlock* sig = getSig(node);
        nodes[sig] = node;
    }

    void addEdge(const Edge& edge) {
        // Check if the edge is already in the vector
        if (std::find(edges.begin(), edges.end(), edge) == edges.end()) {
            edges.push_back(edge);
        }
    }

    void findReturnEdges(CallGraph& CG) {
        for (auto& nodePair : CG) {
            const Function* caller = nodePair.first;
            CallGraphNode* cgn = nodePair.second.get();
            if (!caller) continue;

            for (auto it = cgn->begin(); it != cgn->end(); ++it) {
                bool foundCall = false;
                CallGraphNode::CallRecord callRecord = *it;
                Function* callee = callRecord.second->getFunction();
                if (!callee || callee->isDeclaration()) continue;

                for (const BasicBlock& CBB : *caller) {
                    if (foundCall) break;
                    for (const Instruction& I : CBB) {
                        if (foundCall) break;
                        if (const CallBase* CB = dyn_cast<CallBase>(&I)) {
                            if (CB->getCalledFunction() == callee) {
                                foundCall = true;
                                llvm::BasicBlock* CBBPtr = const_cast<llvm::BasicBlock*>(&CBB);
                                Node callerNode = Node(CBB.getName().str(), CBBPtr, caller->getName().str());
                                llvm::BasicBlock* callersig = getSig(callerNode);
                                addNode(callerNode);

                                if (const InvokeInst* invoke = dyn_cast<InvokeInst>(CB)) {
                                    BasicBlock* unwindDest = invoke->getUnwindDest();
                                    if (unwindDest) {
                                        Node exceptionNode = Node(unwindDest->getName().str(), unwindDest, caller->getName().str());
                                        BasicBlock* exceptionsig = getSig(exceptionNode);
                                        addNode(exceptionNode);

                                        outs() << "get exception case:" << "\n";
                                        returnBlockMap[callersig].insert(exceptionsig);
                                        Edge exceptionEdge = Edge(exceptionsig, callersig, "Exception");
                                        addEdge(exceptionEdge);
                                    }
                                }

                                for (BasicBlock& calleeBB : *callee) {
                                    for (Instruction& I : calleeBB) {
                                        if (isa<llvm::ReturnInst>(&I)) {
                                            Node calleeNode = Node(calleeBB.getName().str(), &calleeBB, callee->getName().str());
                                            BasicBlock* calleesig = getSig(calleeNode);
                                            addNode(calleeNode);

                                            returnBlockMap[callersig].insert(calleesig);
                                            Edge returnEdge = Edge(calleesig, callersig, "return");
                                            addEdge(returnEdge);
                                        }
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

void insertAddrListtoSection(Module& M,
                           std::vector<llvm::BasicBlock*>& keyList,
                           std::vector<llvm::BasicBlock*>& valueList,
                           std::vector<int>& countList) {
    LLVMContext& Context = M.getContext();
    Type* Int64Ty = Type::getInt64Ty(Context);
    
    int counter = 0;  // Counter for unique global variable names
    for (BasicBlock* addr : keyList) {
        outs() << "Processing keyList basic block at address " << addr << "\n";

        BlockAddress* blockAddr = BlockAddress::get(addr->getParent(), addr);
        BasicBlock* block = blockAddr->getBasicBlock();
        Value* addrValue = nullptr;
        Function* parentFunction = block->getParent();

        if (block == &parentFunction->getEntryBlock()) {
            addrValue = ConstantExpr::getPtrToInt(parentFunction, Int64Ty);
        } else {
            addrValue = ConstantExpr::getPtrToInt(blockAddr, Int64Ty);
        }

        std::string varName = "caller" + std::to_string(counter) + "_" + std::to_string(countList[counter]);

        GlobalVariable* MyVariable = new GlobalVariable(
            M,                              // Module
            Int64Ty,                        // Type
            true,                           // IsConstant
            GlobalValue::ExternalLinkage,   // Linkage
            cast<Constant>(addrValue),      // Initializer
            varName,                        // Name
            nullptr,                        // InsertBefore
            GlobalValue::NotThreadLocal,    // Thread Local
            0,                             // AddressSpace
            true                           // Constant
        );
        counter++;
        MyVariable->setSection(".section_for_caller");
    }

    int newCounter = 0;
    for (BasicBlock* addr : valueList) {
        outs() << "Processing valuelist basic block at address " << addr << "\n";

        BlockAddress* blockAddr = BlockAddress::get(addr->getParent(), addr);
        BasicBlock* block = blockAddr->getBasicBlock();
        Value* addrValue = nullptr;
        Function* parentFunction = block->getParent();

        if (block == &parentFunction->getEntryBlock()) {
            addrValue = ConstantExpr::getPtrToInt(parentFunction, Int64Ty);
        } else {
            addrValue = ConstantExpr::getPtrToInt(blockAddr, Int64Ty);
        }

        std::string varName = "return_" + std::to_string(newCounter);

        GlobalVariable* MyVariable = new GlobalVariable(
            M,                              // Module
            Int64Ty,                        // Type
            true,                           // IsConstant
            GlobalValue::ExternalLinkage,   // Linkage
            cast<Constant>(addrValue),      // Initializer
            varName,                        // Name
            nullptr,                        // InsertBefore
            GlobalValue::NotThreadLocal,    // Thread Local
            0,                             // AddressSpace
            true                           // Constant
        );
        newCounter++;
        MyVariable->setSection(".section_for_return_and_exception");
    }
}

// Helper function that does the actual graph construction and output
void interproceduralGraphImpl(Module& M, CallGraph& CG) {
    InterproceduralGraph IPG;

    IPG.findReturnEdges(CG);
    // Convert map to lists
    for (const auto& pair : IPG.returnBlockMap) {
        IPG.keyList.push_back(pair.first);
        IPG.countList.push_back(pair.second.size());
        for (const auto& value : pair.second) {
            IPG.valueList.push_back(value);
        }
    }

    insertAddrListtoSection(M, IPG.keyList, IPG.valueList, IPG.countList);
    IPG.returnBlockMap.clear();
    IPG.keyList.clear();
    IPG.valueList.clear();
    IPG.countList.clear();
}

}  // end anonymous namespace

PreservedAnalyses InterproceduralGraphPass::run(Module& M, ModuleAnalysisManager& AM) {
    // Get the CallGraph from the AnalysisManager
    CallGraph& CG = AM.getResult<CallGraphAnalysis>(M);

    // Call the helper function to build and output the graph
    interproceduralGraphImpl(M, CG);

    // Indicate that no analyses are preserved after this pass runs
    return PreservedAnalyses::all();
}
