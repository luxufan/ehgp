#ifndef ICALLSOLVER_H_
#define ICALLSOLVER_H_

#include <llvm/IR/Function.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/IR/PassManager.h>

#include <set>
using namespace llvm;

class ValueLattice {
  std::set<Function *> Funcs;
  bool Top = false;
  bool Unknown = true;

public:
  bool merge(ValueLattice &VL);

  const auto &getFuncs() const { return Funcs; }
  void setTop() { Top = true; }
  bool isTop() { return Top; }
  bool isUnknown() { return Unknown; }

  bool addFunc(Function *F) {
    Unknown = false;
    return Funcs.insert(F).second;
  }

};

class ICallSolver : public InstVisitor<ICallSolver> {

  Module &M;
  DenseMap<Value *, ValueLattice> ValueStates;

  SmallVector<Value *, 64> InstLists;
  DenseSet<Function *> TrackedFuncs;

  DenseMap<Function *, std::set<User *>> CallSitesMap;


public:
  void visitCallBase(CallBase &CB);

  void solve();
  ICallSolver(Module &M) : M(M) {}
  ValueLattice &getValueStates(Value *V) {
    return ValueStates[V];
  }

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
