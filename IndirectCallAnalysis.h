#ifndef INDIRECTCALLANALYSIS_H_
#define INDIRECTCALLANALYSIS_H_

#include <llvm/IR/PassManager.h>
#include <llvm/Pass.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/BasicAliasAnalysis.h>
#include <llvm/ADT/MapVector.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Analysis/OptimizationRemarkEmitter.h>
#include <llvm/IR/ModuleSummaryIndex.h>
#include <llvm/IR/Constants.h>
#include <llvm/ADT/DenseMap.h>
#include <set>
namespace llvm {

using AddressPoint = std::pair<GlobalValue *, uint64_t>;

class VCallCandidatesAnalyzer {

  DenseMap<Value *, DenseSet<Value *>> SubclassesMap;

  DenseMap<Function *, std::set<User *>> CallerMap;
  Module &M;

  DenseMap<StringRef, std::set<AddressPoint>> TypeIdCompatibleMap;

public:

  VCallCandidatesAnalyzer(Module &M) : M(M) {}

  void insertAddressPoint(StringRef TypeId, GlobalValue *VTable, uint64_t Offset) {
    TypeIdCompatibleMap.getOrInsertDefault(TypeId).insert(std::make_pair(VTable, Offset));
  }

  bool analyze();

  // Get virtual method candidates for specific typeid and Offset.
  // The TypeId is the argument of llvm.type.test intrinsic
  // Offset is the start from the begining of address point
  // Return false if can not analyze
  bool getVCallCandidates(CallBase *CB, SmallVectorImpl<Function  *> &Candidates);

  bool getCallerCandidates(Function *F, SmallVectorImpl<User *> &Callers);

  bool derivedFrom(Value *Base, Value *Defined);

};

class VCallAnalysis : public AnalysisInfoMixin<VCallAnalysis> {
  friend AnalysisInfoMixin<VCallAnalysis>;

  static AnalysisKey Key;

public:
  using Result = VCallCandidatesAnalyzer;
  VCallCandidatesAnalyzer run(Module &M, ModuleAnalysisManager &AM) {
    return VCallCandidatesAnalyzer(M);
  }

};
}


#endif // INDIRECTCALLANALYSIS_H_
