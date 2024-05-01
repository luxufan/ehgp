#include "EHInfer.h"
#include <llvm/Analysis/LazyCallGraph.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Constants.h>
#include <llvm/ADT/Statistic.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Analysis/HeatUtils.h>
#include <llvm/ADT/SmallSet.h>
#include <llvm/Support/DOTGraphTraits.h>
#include <llvm/Support/GraphWriter.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Transforms/Utils/LowerInvoke.h>

using namespace llvm;
#define DEBUG_TYPE "ehinfer"

STATISTIC(NumDeadCatch, "Number of functions marked as nosync");
STATISTIC(NumCatch, "Number of functions marked as nosync");
STATISTIC(NumRethrow, "Number of functions marked as nosync");
STATISTIC(NumIndirect, "Number of functions marked as nosync");
STATISTIC(NumVirtualExType, "Number of functions marked as nosync");
STATISTIC(NumThrowType, "Number of functions marked as nosync");
STATISTIC(NumEmptyCleanup, "Number of functions marked as nosync");
STATISTIC(NumHandledVirtualCall, "Number of functions marked as nosync");
STATISTIC(NumNewLandingPad, "Number of functions marked as nosync");
STATISTIC(NumLowerInvoke, "Number of functions marked as nosync");
STATISTIC(NumDeadBlock, "Number of functions marked as nosync");

static unsigned NumMayThrow = 0;

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

public:
  EHGraphDOTInfo(CallBase *CxaThrow, VCallCandidatesAnalyzer &Analyzer) {
    EntryNode = CxaThrow->getFunction();
    Exception = CxaThrow->getArgOperand(1);
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

      auto HandleUser = [&](User *U) {
        if (auto *CB = dyn_cast<CallBase>(U)) {
          if (!canBeCaught(CB, Exception, Analyzer)) {
            Childs.insert(CB->getFunction());
            ToVisit.push_back(CB);
          }
        }
      };

      for_each(F->users(), HandleUser);

      SmallVector<User *, 5> Callers;
      if (Analyzer.getCallerCandidates(F, Callers))
        for_each(Callers, HandleUser);

    }
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
    std::string Label;
    char *DemangleName = itaniumDemangle(Node->getName());
    if (DemangleName)
      Label += DemangleName;
    else
      Label += Node->getName();

    Label = Label.substr(0, Label.find('('));

    if (Info->getEntryNode() == Node) {
      char *DemangledException = itaniumDemangle(Info->getException()->getName());
      Label += " THROWS: ";
      if (DemangledException)
        Label += DemangledException;
      else
        Label += Info->getException()->getName();
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
void doEHGraphDOTPrinting(Module &M, VCallCandidatesAnalyzer &Analyzer) {
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
    EHGraphDOTInfo GInfo(CB, Analyzer);
    if (!EC)
      WriteGraph(File, &GInfo);
    else
      errs() << "  error opening file for writing!\n";
  }
}
}

static void computeLandingPads(BasicBlock *BB, SmallPtrSetImpl<LandingPadInst *> &LPads) {
  DenseSet<BasicBlock *> Visited;
  SmallVector<BasicBlock *> WorkList = { BB };
  while (WorkList.size() != 0) {
    BasicBlock *Visiting = WorkList.pop_back_val();
    if (Visited.contains(Visiting))
      continue;
    if (auto *II = dyn_cast<LandingPadInst>(Visiting->getFirstNonPHI())) {
      LPads.insert(II);
      continue;
    }

    for (auto *Pred : predecessors(Visiting)) {
      WorkList.push_back(Pred);
    }
  }
}


bool EHInferPass::canBeCaught(LandingPadInst *LandingPad, Value *ExType) {
  // if (Rethrows.contains(LandingPad->getFunction()))
  //   return false;
  for (unsigned I = 0; I < LandingPad->getNumClauses(); I++) {
    if (canBeCaught(LandingPad->getClause(I), ExType)) {
      AliveClauses[LandingPad].insert(LandingPad->getClause(I));
      return true;
    }
  }
  return false;
}

bool EHInferPass::canBeCaught(Constant *Clause, Value *ExType) {
  if (Clause == ExType)
    return true;

  if (SubclassesMap.contains(Clause) && SubclassesMap[Clause].contains(ExType))
    return true;

  if (isa<ConstantPointerNull>(Clause))
    return true;

  return false;
}

static void getRaiseTypes(Value *RaiseOperand, DenseSet<GlobalVariable *> &Types) {
  if (auto *Type = dyn_cast<GlobalVariable>(RaiseOperand))
    Types.insert(Type);
  else if (auto *Phi = dyn_cast<PHINode>(RaiseOperand)) {
    for (unsigned I = 0; I < Phi->getNumIncomingValues(); I++) {
      getRaiseTypes(Phi->getIncomingValue(I), Types);
    }
  } else {
    llvm_unreachable("error");
  }
  return;
}


bool EHInferPass::checkCheanup(Module &M) {
  DenseSet<BasicBlock *> Visited;
  for (Function &F : M.functions()) {
    SmallVector<BasicBlock *, 10> DeletedBB;
    for (BasicBlock &BB : F) {
      if (auto *LandingPad = dyn_cast<LandingPadInst>(BB.getFirstNonPHI())) {
        if (LandingPad->getNumClauses() == 0 && LandingPad->isCleanup()) {
          if (BB.size() == 2) {
            if (auto *Br = dyn_cast<BranchInst>(BB.getTerminator())) {
              auto *TargetBB = Br->getSuccessor(0);
              if (Visited.contains(TargetBB))
                continue;
              Visited.insert(TargetBB);
              if (llvm::all_of(predecessors(TargetBB), [](BasicBlock *BB) {
                return BB->size() == 2 && isa<LandingPadInst>(BB->getFirstNonPHI());
              })) {
                bool Valid = true;
                bool HasPadValue = false;
                for (auto &I : *TargetBB) {
                  if (auto *Phi = dyn_cast<PHINode>(&I)) {
                    if (Phi->getType() != LandingPad->getType()) {
                      Valid = false;
                      break;
                    } else {
                      HasPadValue = true;
                    }
                  }
                }
                if (!Valid || !HasPadValue)
                  continue;
                auto *NewLandingPad = LandingPadInst::Create(LandingPad->getType(), 0);
                NewLandingPad->setCleanup(true);
                if (const Instruction *InsertPt = TargetBB->getFirstNonPHI()) {
                  NewLandingPad->insertBefore(TargetBB->getFirstNonPHI());
                } else
                  NewLandingPad->insertBefore(TargetBB->end());

                SmallVector<PHINode *, 1> ToDelete;
                for (auto &I : *TargetBB) {
                  if (auto *Phi = dyn_cast<PHINode>(&I)) {
                    if (Phi->getType() == LandingPad->getType()) {
                      Phi->replaceAllUsesWith(NewLandingPad);
                      ToDelete.push_back(Phi);
                    }
                  }
                }
                for (PHINode *Phi : ToDelete)
                  Phi->eraseFromParent();
                for (auto *Pred : predecessors(TargetBB)) {
                  Pred->replaceAllUsesWith(TargetBB);
                  DeletedBB.push_back(Pred);
                }

                NumEmptyCleanup++;
              }

            }

          }
        }
      }
    }

    for (auto *BB : DeletedBB) {
      BB->eraseFromParent();
    }
  }
  return true;
}

PreservedAnalyses EHInferPass::run(Module &M,
                                   ModuleAnalysisManager &AM) {

  auto &VCallAnalyzer = AM.getResult<VCallAnalysis>(M);
  Analyzer = &VCallAnalyzer;
  Analyzer->analyze();

  SmallVector<MDNode *> Types;
  for (GlobalVariable &GV : M.globals()) {
    StringRef Name = GV.getName();

    if (!Name.starts_with("_ZTV"))
      continue;

    Name.consume_front("_ZTV");

    if (auto PointIdx = Name.find('.'))
      Name = Name.substr(0, PointIdx);

    if (Value *TypeInfo = M.getNamedGlobal("_ZTI" + Name.str())) {
      if (MayThrow.count(TypeInfo)) {
        Types.clear();
        GV.getMetadata(LLVMContext::MD_type, Types);
        for (MDNode *Type : Types) {
          if (auto *TypeId = dyn_cast<MDString>(Type->getOperand(1).get())) {
            StringRef TypeIdStr = TypeId->getString();
            if (TypeIdStr.ends_with(".virtual"))
              continue;

            if (!TypeIdStr.consume_front("_ZTS"))
              llvm_unreachable("Type id string is not start with _ZTS");
            std::string BaseName = "_ZTI" + TypeIdStr.str();
            GlobalVariable *Base = M.getNamedGlobal("_ZTI" + TypeIdStr.str());
            SubclassesMap[Base].insert(TypeInfo);
            NumVirtualExType++;
          }
        }
      }
    }
  }

  doEHGraphDOTPrinting(M, *Analyzer);

  return PreservedAnalyses::all();
}

void EHInferPass::removeDeadCatchBlock(Module &M) {
  GlobalValue *TypeIdIntrinsic = M.getNamedValue("llvm.eh.typeid.for");
  for (auto *User : TypeIdIntrinsic->users()) {
    auto *CB = dyn_cast<CallBase>(User);
    if (!CB)
      continue;
    Value *TypeId = CB->getArgOperand(0);
    SmallPtrSet<LandingPadInst *, 3> LandingPads;
    computeLandingPads(CB->getParent(), LandingPads);
    if (!llvm::any_of(LandingPads, [TypeId](LandingPadInst *LPad) {
      for (unsigned I = 0; I < LPad->getNumClauses(); I++) {
        if (LPad->getClause(I) == TypeId)
          return true;
      }
      return false;
    })) {
      auto *ICmp = dyn_cast_or_null<ICmpInst>(CB->getUniqueUndroppableUser());
      if (ICmp) {
        ICmp->replaceAllUsesWith(ConstantInt::getBool(CB->getContext(), false));
        NumDeadBlock++;
      }
    }

  }

}

void EHInferPass::collectMayThrow(CallInst *CI, RaiseSet &Exceptions) {
  if (CI->hasFnAttr(Attribute::NoUnwind))
    return;
  if (auto *Callee = CI->getCalledFunction()) {
    if (Callee->getName() == "__cxa_throw") {
      DenseSet<GlobalVariable *> Types;
      getRaiseTypes(CI->getOperand(1), Types);
      for (auto *Type : Types) {
        Exceptions.insert(Type);
      }
      return;
    }
    if (Callee->isDeclaration())
      ExternalThrow.insert(CI->getParent()->getParent());
    collectMayThrow(Callee, Exceptions);
  } else {
    NumIndirect++;
    SmallVector<Function *, 4> VirtualFunctions;
    if (Analyzer->getVCallCandidates(CI, VirtualFunctions)) {
      NumHandledVirtualCall++;
      for (auto *F : VirtualFunctions) {
        collectMayThrow(F, Exceptions);
      }
    } else {
      Exceptions = MayThrow;
    }
  }
}

void EHInferPass::collectMayThrow(InvokeInst *Invoke, RaiseSet &Exceptions) {
  LandingPadInst *LandingPad = dyn_cast<LandingPadInst>(Invoke->getUnwindDest()->getFirstNonPHI());
  if (auto *Callee = dyn_cast<Function>(Invoke->getCalledOperand())) {
    if (Callee->getName() == "__cxa_throw") {
      DenseSet<GlobalVariable *> Types;
      getRaiseTypes(Invoke->getOperand(1), Types);
      for (auto *Type : Types) {
        if (!canBeCaught(LandingPad, Type))
          Exceptions.insert(Type);
      }
      return;
    }
    if (Callee->isDeclaration()) {
      assert(!Callee->hasInternalLinkage());
      ExternalThrow.insert(Invoke->getParent()->getParent());
    }
    collectMayThrow(Callee, Exceptions, LandingPad);

  } else {
    NumIndirect++;
    SmallVector<Function *, 4> VirtualFunctions;
    if (Analyzer->getVCallCandidates(Invoke, VirtualFunctions)) {
      NumHandledVirtualCall++;
      for (auto *F : VirtualFunctions) {
        collectMayThrow(F, Exceptions, LandingPad);
      }

    } else {
      Exceptions = MayThrow;
    }
  }

}
void EHInferPass::collectMayThrow(Function *F, RaiseSet &Exceptions, LandingPadInst *LandingPad) {
  if (!RaiseMap.contains(F))
    return;

  for (Value *Exception : RaiseMap[F]) {
    if (!LandingPad || !canBeCaught(LandingPad, Exception))
      Exceptions.insert(Exception);
  }
}

bool EHInferPass::computeFixedPoint(LazyCallGraph::RefSCC &RefSCC, std::function<bool(Function &)> TransferFunction) {
  SmallVector<Function *> WorkList;

  for (LazyCallGraph::SCC &SCC : RefSCC) {
    for (LazyCallGraph::Node &Node : SCC) {
      WorkList.push_back(&Node.getFunction());
    }
  }

  while (!WorkList.empty()) {
    Function *F = WorkList.pop_back_val();
    if (TransferFunction(*F)) {
      for (auto *User : F->users())
        if (auto *I = dyn_cast<Instruction>(User)) {
          WorkList.push_back(I->getFunction());
        }
    }
  }
  return true;
}

bool EHInferPass::inferRethrow(LazyCallGraph &CG) {
  auto TransferFn = [this](Function &F) {
    if (Rethrows.contains(&F))
      return false;
    bool Change = false;
    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        if (auto *CB = dyn_cast<CallBase>(&I)) {
          if (CB->hasFnAttr(Attribute::NoUnwind))
            continue;
          auto *Callee = CB->getCalledFunction();
          if (Callee) {
            if (Callee->getName() == "__cxa_throw")
              continue;

            if (!Callee->isDeclaration() && Callee->getName() != "__cxa_rethrow" && !Rethrows.contains(Callee))
              continue;

          }
          Rethrows.insert(&F);
          NumRethrow++;
          Change = true;
        }
      }
    }
    return Change;
  };
  for (LazyCallGraph::RefSCC &RefSCC : CG.postorder_ref_sccs()) {
    computeFixedPoint(RefSCC, TransferFn);
  }
  return false;
}
