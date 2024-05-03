#include <llvm/IR/Function.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/InstVisitor.h>
#include "ICallSolver.h"

#include <set>

using namespace llvm;

bool ValueLattice::merge(ValueLattice &VL) {
  bool Changed = false;
  if (this->isTop())
    return false;

  if (VL.isTop()) {
    this->setTop();
    return true;
  }

  for (auto *F : VL.getFuncs()) {
    Changed |= this->addFunc(F);
  }

  return Changed;
}

void ICallSolver::visitCallBase(CallBase &CB) {
  auto *Callee = CB.getCalledFunction();
  if (!Callee) {
    ValueLattice &VL = getValueStates(CB.getCalledOperand());
    if (!VL.isUnknown() && !VL.isTop()) {
      for (auto *F : VL.getFuncs())
        CallSitesMap[F].insert(&CB);
    }
    return;
  }

  for (auto I : zip(Callee->args(), CB.args())) {
    auto &CBUse = std::get<1>(I);

    auto &ParaLattice = getValueStates(CBUse.get());
    if (ParaLattice.isUnknown())
      continue;

    auto &Arg = std::get<0>(I);
    auto &ArgLattice = getValueStates(&Arg);
    if (ArgLattice.merge(ParaLattice)) {
      llvm::for_each(Arg.users(), [&](User *U) {
        if (auto *I = dyn_cast<Instruction>(U))
          InstLists.push_back(I);
      });
    }
  }
}

void ICallSolver::solve() {
  for (auto &F : M.functions()) {
    getValueStates(&F).addFunc(&F);
    for (auto &BB : F)
      for (auto &I : BB)
        InstLists.push_back(&I);
  }

  while (!InstLists.empty()) {
    Instruction *I = dyn_cast<Instruction>(InstLists.pop_back_val());
    visit(*I);
  }
}

AnalysisKey ICallSolverAnalysis::Key;
