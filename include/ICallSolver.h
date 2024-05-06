#ifndef ICALLSOLVER_H_
#define ICALLSOLVER_H_

#include <llvm/IR/Function.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/IR/PassManager.h>

#include <set>
using namespace llvm;

class ICallSolver : public InstVisitor<ICallSolver> {

  Module &M;
  DenseMap<Function *, std::set<User *>> CallSitesMap;

public:
  void solve();
  ICallSolver(Module &M) : M(M) {}
  bool getCallSites(Function *F, SmallVectorImpl<User *> &Users) {
    if (CallSitesMap.contains(F)) {
      Users.append(CallSitesMap[F].begin(), CallSitesMap[F].end());
      return true;
    }
    return false;
  }
};

class ICallSolverAnalysis : public AnalysisInfoMixin<ICallSolverAnalysis> {
  friend AnalysisInfoMixin<ICallSolverAnalysis>;

  static AnalysisKey Key;

public:
  using Result = ICallSolver;
  ICallSolver run(Module &M, ModuleAnalysisManager &MAM) {
    return ICallSolver(M);
  }
};

#endif // ICALLSOLVER_H_
