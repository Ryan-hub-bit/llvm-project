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
using namespace llvm;


namespace {
	// Data structures for the graph
struct Node {
    std::string name;
    std::string address;
    std::string FunctionName;

Node() : name(""), address(""), FunctionName("") {}
Node(const std::string &n, const std::string &a, const std::string &fn)
        : name(n), address(a), FunctionName(fn) {}
};

struct Edge {
    std::string from; // node.name + "_" + node.address
    std::string to;
    std::string edgeType; // Inter or Intra
    Edge() : from(""), to(""), edgeType("") {}
    Edge(const std::string &n, const std::string &a, const std::string &fn)
        : from(n), to(a), edgeType(fn) {}

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
    std::map<std::string, Node> nodes;
    std::vector<Edge> edges;
    std::list<std::string> intermadiateAddr;
    std::set<std::string> functionsWithIndirectCalls;
    std::list<llvm::BlockAddress*> addrPointer;
    std::string getSig(Node node){
	return node.name + "_" + node.address;
    }
    void addNode(Node &node) {
	std::string sig = getSig(node);
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
            // Print the name of the basic block
            outs() << "BasicBlock: " << BB.getName() << "\n";
	    BlockAddress *BBAddress = BlockAddress::get(&F, &BB);
	    std::string str;
    	    llvm::raw_string_ostream rso(str);
	    rso << BBAddress;
	    Node BBNode = Node(BB.getName().str(), str, F.getName().str());
	    addNode(BBNode);
	    if (intermadiateAddr.end() == std::find(intermadiateAddr.begin(), intermadiateAddr.end(), str)){
		intermadiateAddr.push_back(str);
		addrPointer.push_back(BBAddress);
	    }
	    std::string BBSig = getSig(BBNode);
            // Iterate over each successor of the basic block
            for (BasicBlock *Succ : successors(&BB)) {
                outs() << "  Successor: " << Succ->getName() << "\n";
		BlockAddress *SuccAddress = BlockAddress::get(&F, Succ);
		std::string succstr;
    	    	llvm::raw_string_ostream rso2(succstr);
	    	rso2 << SuccAddress;
		rso2.flush();
	    	Node SuccNode = Node(Succ->getName().str(), succstr, F.getName().str());
		addNode(SuccNode);
		if (intermadiateAddr.end() == std::find(intermadiateAddr.begin(), intermadiateAddr.end(), succstr)){
		intermadiateAddr.push_back(succstr);
		addrPointer.push_back(SuccAddress);
		}
		std::string succSig = getSig(SuccNode);
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
                                	// Found the call, now get the basic block address
                                	// const void *CBBAddr = &CBB;
					// llvm::BlockAddress* CBBAddress = const_cast<llvm::BlockAddress*>(static_cast<const llvm::BlockAddress*>(CBBAddr));
					// llvm::BlockAddress *CBBAddress = BlockAddress::get(caller, const_cast<BasicBlock*>(&CBB));
					llvm::BlockAddress *CBBAddress = llvm::BlockAddress::get(const_cast<llvm::Function*>(caller), const_cast<llvm::BasicBlock*>(&CBB));

					std::string callerStr;
    	    				llvm::raw_string_ostream rso3(callerStr);
	    				rso3 << CBBAddress;
					Node callerNode = Node(CBB.getName().str(), rso3.str(), caller->getName().str());
					std::string callersig = getSig(callerNode);
					addNode(callerNode);
					    if (intermadiateAddr.end() == std::find(intermadiateAddr.begin(), intermadiateAddr.end(), callerStr)){
					intermadiateAddr.push_back(callerStr);
					addrPointer.push_back(CBBAddress);
	    					}
                                	//addEdge(caller->getName().str(), callee->getName().str());
 					if (callee && !callee->isDeclaration()) {
						for (BasicBlock &calleeBB : *callee){
							for (Instruction &I : calleeBB){
								if (isa<llvm::ReturnInst>(&I)) {
									found = true;
            								BlockAddress *RetAddress = BlockAddress::get(callee, &calleeBB);

						std::string retStr;
    	    					llvm::raw_string_ostream rso4(retStr);
	    					rso4 << RetAddress;
						Node calleeNode = Node(calleeBB.getName().str(), retStr, callee->getName().str());
						std::string calleesig = getSig(calleeNode);
						addNode(calleeNode);
						  if (intermadiateAddr.end() == std::find(intermadiateAddr.begin(), intermadiateAddr.end(), retStr)){
						intermadiateAddr.push_back(retStr);
						addrPointer.push_back(RetAddress);
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
    for (std::string value : intermadiateAddr) {
        outFile << value << std::endl;  // Each value on a new line
    }

    // Close the file
    outFile.close();
    return true;
}

};

void insertAddrListtoSection(Module &M, std::list<llvm::BlockAddress*> addrs) {
	LLVMContext &Context = M.getContext();
    	IRBuilder<> Builder(Context);

	// Create a pointer type and an integer type
    	//Type *IntPtrTy = Type::getInt8Ty(Context)->getPointerTo();
    	Type *Int64Ty = Type::getInt64Ty(Context);

    	// Assume each string in the list corresponds to a unique pointer
    	for (const auto &addr : addrs) {
	outs() << "addr " << addr << "\n";
	llvm::Type *addrType = addr->getType();
	if (!addrType->isPointerTy()) {
    	outs() << "Error: addr is not a pointer type.\n";
    	return ; // Or handle the error as appropriate
	}
        //llvm::Constant *AddrConstant = llvm::ConstantExpr::getPtrToInt(addr, Int64Ty);
	Value *result = Builder.CreatePtrToInt(addr, Type::getInt64Ty(Context));
              // llvm::errs() << "result:" << result << "\n";
              Constant *resultConstant = dyn_cast<Constant>(result);
              if (!resultConstant) {
               llvm::errs() << "convert failed"<< "\n";
        }

        // Create a global variable with the constant address
        llvm::GlobalVariable *MyVariable = new llvm::GlobalVariable(
            M,                     // Module
            Int64Ty,               // Type
            false,                 // IsConstant
            llvm::GlobalValue::ExternalLinkage, // Linkage
            resultConstant,          // Initializer
            "myVariable",          // Name
            nullptr,               // InsertBefore
            llvm::GlobalValue::NotThreadLocal, // Thread Local
            0,                     // AddressSpace
            true                   // Constant
        );
        MyVariable->setSection(".section_for_address");
	}
	return;
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
    insertAddrListtoSection(M,IPG.addrPointer);
}

} // end anonymous namespace


PreservedAnalyses InterproceduralGraphPass::run(Module &M, ModuleAnalysisManager &AM) {
        // Get the CallGraph from the AnalysisManager
        CallGraph &CG = AM.getResult<CallGraphAnalysis>(M);

        // Call the helper function to build and output the graph
        interproceduralGraphImpl(M, CG);

        // Indicate that no analyses are preserved after this pass runs
        return PreservedAnalyses::none();
    }