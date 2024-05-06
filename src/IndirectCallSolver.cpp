#include <llvm/IR/Function.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/InstVisitor.h>
#include "ICallSolver.h"

#include <set>

using namespace llvm;

void ICallSolver::solve() {
  for (auto &F : M.functions()) {
    for (auto &BB : F)
      for (auto &I : BB) {
        if (auto *CB = dyn_cast<CallBase>(&I))
          if (MDNode *MD = CB->getMetadata(LLVMContext::MD_callees)) {
            for (const auto &Op : MD->operands())
              if (Function *Callee = mdconst::dyn_extract_or_null<Function>(Op))
                CallSitesMap[Callee].insert(CB);
          }
      }
  }
}


AnalysisKey ICallSolverAnalysis::Key;
