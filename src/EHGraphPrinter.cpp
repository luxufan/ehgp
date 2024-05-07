#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Constants.h>
#include <llvm/Analysis/HeatUtils.h>
#include <llvm/ADT/SmallSet.h>
#include <llvm/Support/DOTGraphTraits.h>
#include <llvm/Support/GraphWriter.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "ICallSolver.h"
#include "EHGraphPrinter.h"
#include "IndirectCallAnalysis.h"
#include "util.h"

using namespace llvm;
#define DEBUG_TYPE "ehinfer"

static bool canBeCaught(CallBase *CB, Value *Exception, VCallCandidatesAnalyzer &Analyzer) {
  auto *II = dyn_cast<InvokeInst>(CB);
  if (!II)
    return false;

  auto *LPad = dyn_cast_if_present<LandingPadInst>(II->getUnwindDest()->getFirstNonPHI());
  if (!LPad)
    return false;

  for (unsigned I = 0; I < LPad->getNumClauses(); I++) {
    auto *Clause = LPad->getClause(I);
    if (isa<ConstantPointerNull>(Clause) || Clause == Exception ||
        Analyzer.derivedFrom(Clause, Exception))
      return true;
  }

  return false;
}

class EHGraphDOTInfo {
public:

  using ChildsSetType = DenseSet<Function *>;
  using ChildsMapType = DenseMap<Function *, ChildsSetType>;
  // This is static class variable since we need to use it
  // in GraphTraits::childs_begin
    static ChildsMapType ChildsMap;

private:
  Function *EntryNode;
  Value *Exception;
  Function *LeakNode;

public:
  EHGraphDOTInfo(CallBase *CxaThrow, Function *Leak, VCallCandidatesAnalyzer &Analyzer,
                 ICallSolver &Solver) {
    EntryNode = CxaThrow->getFunction();
    Exception = CxaThrow->getArgOperand(1);
    this->LeakNode = Leak;

    SmallVector<Function *, 32> Leafs;
    ChildsMap.clear();

    std::vector<CallBase *> ToVisit = {CxaThrow};
    DenseSet<User *> Visited;
    while (!ToVisit.empty()) {
      auto *Visiting = ToVisit.back();
      ToVisit.pop_back();
      if (Visited.contains(Visiting))
        continue;

      Visited.insert(Visiting);

      Function *F = Visiting->getFunction();
      auto &Childs = ChildsMap[Visiting->getFunction()];
      if (F->getName() == "main") {
        Childs.insert(LeakNode);
        Leafs.push_back(LeakNode);
        continue;
      }

      auto HandleUser = [&](User *U) {
        if (auto *CB = dyn_cast<CallBase>(U)) {
          Childs.insert(CB->getFunction());
          // Insert into ChildsMap to make sure this nodes get printed
          Leafs.push_back(CB->getFunction());
          if (!canBeCaught(CB, Exception, Analyzer)) {
            ToVisit.push_back(CB);
          }
        }
      };

      for_each(F->users(), HandleUser);

      SmallVector<User *, 5> Callers;

      if (Solver.getCallSites(F, Callers))
        for_each(Callers, HandleUser);
      else if (Analyzer.getCallerCandidates(F, Callers))
        for_each(Callers, HandleUser);

    }
    for_each(Leafs, [&](Function *F) {
        ChildsMap.getOrInsertDefault(F);
    });

  }

  Function *getEntryNode() { return EntryNode; }
  Value *getException() { return Exception; }
};

EHGraphDOTInfo::ChildsMapType EHGraphDOTInfo::ChildsMap;

template <>
struct GraphTraits<EHGraphDOTInfo *> {
  using NodeRef = Function *;
  static NodeRef getEntryNode(EHGraphDOTInfo *Info) {
    return Info->getEntryNode();
  }

  using ChildIteratorType = EHGraphDOTInfo::ChildsSetType::ConstIterator;
  static ChildIteratorType child_begin(NodeRef N) {
    return EHGraphDOTInfo::ChildsMap[N].begin();
  }

  static ChildIteratorType child_end(NodeRef N) {
    return EHGraphDOTInfo::ChildsMap[N].end();
  }

  typedef decltype(*std::declval<EHGraphDOTInfo::ChildsMapType::iterator>()) PairTy;

  static NodeRef getValue(PairTy Iter) {
    return Iter.getFirst();
  }

  typedef mapped_iterator<EHGraphDOTInfo::ChildsMapType::iterator, decltype(&getValue)> nodes_iterator;

  static nodes_iterator nodes_begin(EHGraphDOTInfo *Info) {
    return nodes_iterator(EHGraphDOTInfo::ChildsMap.begin(), &getValue);
  }

  static nodes_iterator nodes_end(EHGraphDOTInfo *Info) {
    return nodes_iterator(EHGraphDOTInfo::ChildsMap.end(), &getValue);
  }

};

template <>
struct DOTGraphTraits<EHGraphDOTInfo *> : public DefaultDOTGraphTraits {
  DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}

  static std::string getGraphName(EHGraphDOTInfo *CGInfo) {
    return CGInfo->getException()->getName().str() + " propagation graph: " +
      CGInfo->getEntryNode()->getName().str();
  }

  static bool isNodeHidden(Function *Node, EHGraphDOTInfo *Info) {
    return false;
  }

  std::string getNodeLabel(Function *Node, EHGraphDOTInfo *Info) {
    std::string Label = getDemangledName(Node->getName());

    Label = Label.substr(0, Label.find('('));

    if (Info->getEntryNode() == Node) {
      Label += " THROWS: ";
      std::string ExceptionName = getDemangledName(Info->getException()->getName());
      Label += ExceptionName;
    }
    return Label;
  }

  typedef decltype(*std::declval<EHGraphDOTInfo::ChildsMapType::iterator>()) PairTy;

  static Function *getValue(PairTy Iter) {
    return Iter.getFirst();
  }

  typedef mapped_iterator<EHGraphDOTInfo::ChildsMapType::iterator, decltype(&getValue)> nodes_iterator;

  std::string getEdgeAttributes(Function *Node, typename GraphTraits<EHGraphDOTInfo *>::ChildIteratorType I,
                                EHGraphDOTInfo *Info) {
    return "";
  }

  std::string getNodeAttributes(Function *Node, EHGraphDOTInfo *Info) {
    bool VMethod = llvm::any_of(Node->users(), [](auto *User) {
      if (auto *VInit = dyn_cast<ConstantAggregate>(User)) {
        return true;
      }
      return false;
    });
    std::string Attrs;
    uint64_t ColorValue = 0;
    if (Node == Info->getEntryNode())
      ColorValue = 99;
    else if (VMethod)
      ColorValue = 33;
    else if (!Node->hasInternalLinkage())
      ColorValue = 66;

    std::string NodeColor = getHeatColor(ColorValue, 100);
    std::string EdgeColor = getHeatColor(0);
    Attrs = "color=\"" + EdgeColor + "ff\", style=filled, fillcolor=\"" +
            NodeColor + "80\"";
    return Attrs;
  }

  static bool renderGraphFromBottomUp() { return true; }

};

namespace {
void doEHGraphDOTPrinting(Module &M, VCallCandidatesAnalyzer &Analyzer, ICallSolver &Solver) {
  Function *LeakNode = Function::Create(FunctionType::get(Type::getVoidTy(M.getContext()), false), GlobalValue::ExternalLinkage, "LEAK");
  Function *CxaThrow = cast_if_present<Function>(M.getNamedValue("__cxa_throw"));
  if (!CxaThrow)
    return;

  std::string Filename;
  unsigned I = 0;
  for (auto *U : CxaThrow->users()) {
    auto *CB = dyn_cast<CallBase>(U);
    if (!CB)
      continue;
    Filename = std::string(M.getModuleIdentifier() + "." + utostr(I++) + ".dot");
    errs() << "Writing '" << Filename << "'...\n";
    std::error_code EC;
    raw_fd_ostream File(Filename, EC, sys::fs::OF_Text);
    EHGraphDOTInfo GInfo(CB, LeakNode, Analyzer, Solver);
    errs() << getDemangledName(GInfo.getEntryNode()->getName()) << " throw " << getDemangledName(GInfo.getException()->getName()) << "\n";
    errs() << "Number of nodes: " << GInfo.ChildsMap.size() << "\n";
    if (!EC)
      WriteGraph(File, &GInfo);
    else
      errs() << "  error opening file for writing!\n";
  }
}
}

PreservedAnalyses EHGraphPrinterPass::run(Module &M,
                                   ModuleAnalysisManager &AM) {

  auto &VCallAnalyzer = AM.getResult<VCallAnalysis>(M);
  VCallAnalyzer.analyze();
  auto &ICallSolver = AM.getResult<ICallSolverAnalysis>(M);
  ICallSolver.solve();

  doEHGraphDOTPrinting(M, VCallAnalyzer, ICallSolver);

  return PreservedAnalyses::all();
}
