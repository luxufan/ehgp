#ifndef EHINFER_H_
#define EHINFER_H_

#include <llvm/IR/PassManager.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LazyCallGraph.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/DenseSet.h>
#include "IndirectCallAnalysis.h"

#include <set>

using namespace llvm;

struct EHInferPass : PassInfoMixin<EHInferPass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
  using RaiseSet = std::set<Value *>;
  using RaiseMapType = DenseMap<Function *, RaiseSet>;
  DenseMap<Function *, RaiseSet> RaiseMap;
  DenseMap<Value *, DenseSet<Value *>> SubclassesMap;
  DenseMap<LandingPadInst *, DenseSet<Value *>> AliveClauses;
  DenseSet<Function *> Rethrows;
  DenseSet<Value *> ExTypes;
  std::set<Value *> MayThrow;
  DenseSet<Function *> ExternalThrow;
  VCallCandidatesAnalyzer *Analyzer;

  bool canBeCaught(Constant *Clause, Value *ExType);
  bool computeFixedPoint(LazyCallGraph::RefSCC &RefSCC, std::function<bool(Function &)>);
  bool canBeCaught(LandingPadInst *LandingPad, Value *ExType);
  bool inferRethrow(LazyCallGraph &CG);
  bool checkCheanup(Module &M);

  void collectMayThrow(CallInst *CI, RaiseSet &Set);
  void collectMayThrow(InvokeInst *CI, RaiseSet &Set);
  void collectMayThrow(Function *F, RaiseSet &Set, LandingPadInst *LandingPad = nullptr);

  void removeDeadCatchBlock(Module &M);

};


#endif // EHINFER_H_
