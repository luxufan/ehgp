#ifndef EHINFER_H_
#define EHINFER_H_

#include <llvm/IR/PassManager.h>
#include <llvm/IR/Instructions.h>
#include "util.h"
#include "IndirectCallAnalysis.h"

using namespace llvm;

class CurrentException {

  Value *Exception;
  CallBase *ThrowSite;

public:
  CurrentException(CallBase *CB) : ThrowSite(CB) {
    if (CB->getCalledFunction()->getName() == "__cxa_throw")
      Exception = CB->getArgOperand(1);
    else
      Exception = nullptr;
  }

  std::string getExceptionName() {
    return Exception ? getDemangledName(Exception->getName()) : "Potential external exception throwed by " + getDemangledName(ThrowSite->getCalledFunction()->getName());
  }

  bool externalException() { return !Exception; }

  CallBase *getThrowSite() { return ThrowSite; }

  Value *getExceptionValue() { return Exception; }

};

class ICallSolver;

class EHGraphDOTInfo {
public:

  using ChildsSetType = DenseSet<Function *>;
  using ChildsMapType = DenseMap<Function *, ChildsSetType>;

  // This is static class variable since we need to use it
  // in GraphTraits::childs_begin
  static ChildsMapType ChildsMap;

private:
  Function *EntryNode;
  CurrentException *Exception;
  Function *LeakNode;
  bool Leaked = false;

  void buildChildsMap(Function *, VCallCandidatesAnalyzer &, ICallSolver &);


public:
  EHGraphDOTInfo(CurrentException *Exception, Function *Leak, VCallCandidatesAnalyzer &Analyzer,
                 ICallSolver &Solver) : Exception(Exception), LeakNode(Leak) {
    EntryNode = Exception->getThrowSite()->getFunction();
    buildChildsMap(Leak, Analyzer, Solver);
  }

  Function *getEntryNode() { return EntryNode; }
  CurrentException *getCurrentException() { return Exception; }
  bool isLeaked() { return Leaked; }
};

struct EHGraphPrinterPass : PassInfoMixin<EHGraphPrinterPass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};


#endif // EHINFER_H_
