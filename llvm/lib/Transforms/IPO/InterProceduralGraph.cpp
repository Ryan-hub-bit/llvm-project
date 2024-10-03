#include "llvm/Transforms/IPO/InterProceduralGraph.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <vector>
#include <set>
#include <iostream>
#include <fstream>
#include <list>
#include <algorithm>
#include "llvm/Support/FormatVariadic.h"
using namespace llvm;


namespace {
        // Data structures for the graph
struct Node {
    std::string name;
    llvm::BasicBlock *BB;
    std::string FunctionName;

    // Default constructor
    Node() : name(""), BB(nullptr), FunctionName("") {}

    // Parameterized constructor
    Node(const std::string &n, llvm::BasicBlock *bb, const std::string &fn)
        : name(n), BB(bb), FunctionName(fn) {}
};

struct Edge {
    llvm::BasicBlock *from; //
    llvm::BasicBlock *to;
    std::string edgeType; // [Inter,Intra,return]
    Edge() : from(nullptr), to(nullptr), edgeType("") {}
    Edge(llvm::BasicBlock *from, llvm::BasicBlock *to, const std::string &edgeType)
        : from(from), to(to), edgeType(edgeType) {}


   // Overload the equality operator for comparison
    bool operator==(const Edge &other) const {
        return from == other.from && to == other.to;
    }

    // Overload the less-than operator for sorting and ordering in a set
    bool operator<(const Edge &other) const {
        if (from != other.from)
            return from < other.from;
        return to < other.to;
    }
};


struct InterproceduralGraph {
    std::map<llvm:: BasicBlock*, Node> nodes;
    std::vector<Edge> edges;
    std::list<llvm::BasicBlock*> intermadiateAddr;
    std::set<std::string> functionsWithIndirectCalls;
    std::list<llvm::BasicBlock*> addrPointer;
    llvm::BasicBlock* getSig(const Node &node) {
    return node.BB;
    }
    void addNode(const Node &node) {
        llvm::BasicBlock* sig = getSig(node);
        nodes[sig] = node;
    }

   void addEdge(const Edge &edge) {
        // Check if the edge is already in the vector
        if (std::find(edges.begin(), edges.end(), edge) == edges.end()) {
            edges.push_back(edge);
        }
    }

    void addIntraproceduralEdges(Function &F){
         for (BasicBlock &BB : F) {
            Node BBNode = Node(BB.getName().str(), &BB, F.getName().str());
            addNode(BBNode);
           if (std::find(intermadiateAddr.begin(), intermadiateAddr.end(), &BB) == intermadiateAddr.end()) {
            intermadiateAddr.push_back(&BB);
            addrPointer.push_back(&BB);
           }
            BasicBlock* BBSig = getSig(BBNode);
            // Iterate over each successor of the basic block
            for (BasicBlock *Succ : successors(&BB)) {
                Node SuccNode = Node(Succ->getName().str(), Succ, F.getName().str());
                addNode(SuccNode);
                // If the successor's address is not already in intermadiateAddr, add it
                if (std::find(intermadiateAddr.begin(), intermadiateAddr.end(), Succ) == intermadiateAddr.end()) {
                        intermadiateAddr.push_back(Succ);
                        addrPointer.push_back(Succ);
                }
                BasicBlock* succSig = getSig(SuccNode);
                Edge intraEdge = Edge(BBSig, succSig, "Intra");
                addEdge(intraEdge);
            }
        }
    }

    void addInterproceduralEdges(CallGraph &CG) {
        bool found = false;
        for (auto &nodePair : CG) {
            const Function *caller = nodePair.first;
            CallGraphNode *cgn = nodePair.second.get();
            if (caller){
            for (auto it = cgn->begin(); it != cgn->end(); ++it) {
                found = false;
                CallGraphNode::CallRecord callRecord = *it;
                Function *callee = callRecord.second->getFunction();
                if (callee) {
                        for (const BasicBlock &CBB : *caller) {
                                for (const Instruction &I : CBB) {
                                        if (const CallBase *CB = dyn_cast<CallBase>(&I)) {
                                        if (CB->getCalledFunction() == callee) {
                                                llvm::BasicBlock *CBBPtr = const_cast<llvm::BasicBlock*>(&CBB);
                                        Node callerNode = Node(CBB.getName().str(), CBBPtr, caller->getName().str());
                                        llvm::BasicBlock* callersig = getSig(callerNode);
                                        addNode(callerNode);
                                        if (std::find(intermadiateAddr.begin(), intermadiateAddr.end(), CBBPtr) == intermadiateAddr.end()){
                                        intermadiateAddr.push_back(CBBPtr);
                                        addrPointer.push_back(CBBPtr);
                                        }

                                        //addEdge(caller->getName().str(), callee->getName().str());
                                        if (callee && !callee->isDeclaration()) {
                                                for (BasicBlock &calleeBB : *callee){
                                                        for (Instruction &I : calleeBB){
                                                                if (isa<llvm::ReturnInst>(&I)) {
                                                                        found = true;

                                                Node calleeNode = Node(calleeBB.getName().str(), &calleeBB, callee->getName().str());
                                                BasicBlock* calleesig = getSig(calleeNode);
                                                addNode(calleeNode);
                                                if (std::find(intermadiateAddr.begin(), intermadiateAddr.end(), &calleeBB) == intermadiateAddr.end()){
                                                        intermadiateAddr.push_back(&calleeBB);
                                                        addrPointer.push_back(&calleeBB);
                                                }
                                                Edge interEdge = Edge(callersig, calleesig, "Inter");
                                                addEdge(interEdge);
                                                break;
                                                }
                                                if (found)
                                                break;
                                        }
                                        if (found)
                                        break;
                                }
                                if (found)
                                break;
                        }
                            }
                        }
                        if(found)
                        break;
                    }
                    if(found)
                    break;
                }
            }
            }
            }
        }
    }


    std::set<std::string> getPossibleIndirectTargets(const std::string &FuncName) {
        return std::set<std::string>();
    }

 void outputGraph() {
        // errs() << "Nodes:\n";
        // for (const auto &NodePair : nodes) {
        //     errs() << "  " << NodePair.first << "\n";
        // }

        // errs() << "Edges:\n";
        // for (const auto &Edge : edges) {
        //     errs() << "  " << Edge.from << " -> " << Edge.to << ":" <<Edge.edgeType <<"\n";
        // }
        return;
    }
bool writeGraph() {
        // Create an output file stream object to write to a file
    std::ofstream outFile("/home/ryan/llvm-project/build2/test/intermadiateAddress.txt");

    // Check if the file opened successfully
    if (!outFile) {
        return false;
    }

    // Write the list elements to the file
    for (llvm::BasicBlock* addr : intermadiateAddr) {
        std::string bbAddress = llvm::formatv("{0}", static_cast<const void*>(addr)).str();
        outFile << bbAddress << std::endl;  // Each value on a new line
    }
    // Close the file
    outFile.close();
    return true;
}

};


void insertAddrListtoSection(Module &M, std::list<BasicBlock*> addrs) {
    LLVMContext &Context = M.getContext();
    Type *Int64Ty = Type::getInt64Ty(Context);
    int counter = 0;  // Counter for unique global variable names

    for (BasicBlock* addr : addrs) {
        outs() << "Processing basic block at address " << addr << "\n";

        // Get a Constant that represents the address of the basic block
        BlockAddress *blockAddr = BlockAddress::get(addr->getParent(), addr);
            // Get the BasicBlock pointed to by the BlockAddress
        BasicBlock *block = blockAddr->getBasicBlock();

        // Get the parent function of the BasicBlock
        Value *addrValue = nullptr;
        Function *parentFunction = block->getParent();
        if  (block == &parentFunction->getEntryBlock()){
            // Convert the function address to an integer
        Value *addrValue = ConstantExpr::getPtrToInt(parentFunction, Int64Ty);
        } else{
        // Convert the block address to an integer
        Value *addrValue = ConstantExpr::getPtrToInt(blockAddr, Int64Ty);
        }
        
        // Create a unique name for the global variable
        std::string varName = "myVariable_" + std::to_string(counter);

        // Create a global variable with the integer value
        GlobalVariable *MyVariable = new GlobalVariable(
            M,                       // Module
            Int64Ty,                // Type
            true,                   // IsConstant (true for global constant)
            GlobalValue::ExternalLinkage, // Linkage
            cast<Constant>(addrValue), // Initializer
            varName,                // Name
            nullptr,                // InsertBefore
            GlobalValue::NotThreadLocal, // Thread Local
            0,                      // AddressSpace
            true                    // Constant
        );

        // Set the section where the global variable should be placed
        MyVariable->setSection(".section_for_address");

        // Increment the counter for the next variable
        counter++;
    }
}

// Helper function that does the actual graph construction and output
void interproceduralGraphImpl(Module &M, CallGraph &CG) {
    InterproceduralGraph IPG;

    for (Function &F : M) {
        IPG.addIntraproceduralEdges(F);
    }
    IPG.addInterproceduralEdges(CG);
//     IPG.addIndirectCallEdges();
    IPG.outputGraph();
    if (!IPG.writeGraph()){
         errs() << "Error: Could not open the file for writing!" << "\n";
    }
    insertAddrListtoSection(M,IPG.intermadiateAddr);
}

} // end anonymous namespace


PreservedAnalyses InterproceduralGraphPass::run(Module &M, ModuleAnalysisManager &AM) {
        // Get the CallGraph from the AnalysisManager
        CallGraph &CG = AM.getResult<CallGraphAnalysis>(M);

        // Call the helper function to build and output the graph
        interproceduralGraphImpl(M, CG);

        // Indicate that no analyses are preserved after this pass runs
        return PreservedAnalyses::all();
    }
