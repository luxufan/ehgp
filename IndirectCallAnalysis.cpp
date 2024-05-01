#include "IndirectCallAnalysis.h"
#include <llvm/Analysis/ConstantFolding.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/ADT/Statistic.h>

#define DEBUG_TYPE "vcallanalysis"

using namespace llvm;

STATISTIC(NumVirtualCalls,   "Number of NoAlias results");
STATISTIC(NumNonVirtualCalls,   "Number of NoAlias results");
bool VCallCandidatesAnalyzer::derivedFrom(Value *Base, Value *Derived) {
  if (!SubclassesMap.contains(Base))
    return false;

  return SubclassesMap[Base].contains(Derived);
}

bool VCallCandidatesAnalyzer::analyze() {
  TypeIdCompatibleMap.clear();

  SmallVector<MDNode *, 8> Types;
  for (GlobalVariable &GV : M.globals()) {
    if (GV.getVCallVisibility() == GlobalObject::VCallVisibilityPublic)
      continue;
    Types.clear();
    GV.getMetadata(LLVMContext::MD_type, Types);
    StringRef GVName = GV.getName();
    if (!GVName.consume_front("_ZTV"))
      continue;

    if (auto Index = GVName.find('.'))
      GVName = GVName.substr(0, Index);

    Value *TypeInfo = M.getNamedGlobal("_ZTI" + GVName.str());

    for (MDNode *Type : Types) {
      auto TypeID = Type->getOperand(1).get();
      uint64_t Offset = cast<ConstantInt>(cast<ConstantAsMetadata>(Type->getOperand(0))->getValue())->getZExtValue();

      if (auto *TypeId = dyn_cast<MDString>(TypeID)) {
        StringRef TypeIdStr = TypeId->getString();
        if (TypeIdStr.ends_with(".virtual"))
          continue;

        insertAddressPoint(TypeIdStr, &GV, Offset);

        if (!TypeIdStr.consume_front("_ZTS"))
          llvm_unreachable("Type id string is not start with _ZTS");

        std::string BaseName = "_ZTI" + TypeIdStr.str();
        GlobalVariable *Base = M.getNamedGlobal("_ZTI" + TypeIdStr.str());
        SubclassesMap[Base].insert(TypeInfo);
      }
    }
  }

  for (auto &F : M.functions()) {
    for (auto &BB : F) {
      for (auto &II : BB) {
        if (auto *CB = dyn_cast<CallBase>(&II)) {
          SmallVector<Function *, 3> Candidates;
          if (getVCallCandidates(CB, Candidates)) {
            for (auto *Candidate : Candidates) {
              CallerMap[Candidate].insert(CB);
            }
          }
        }
      }
    }
  }

  return true;
}

AnalysisKey VCallAnalysis::Key;

bool VCallCandidatesAnalyzer::getVCallCandidates(CallBase *CB, SmallVectorImpl<Function *> &Candidates) {
  if (!TypeIdCompatibleMap.size())
    return false;

  Value *CalledOp = CB->getCalledOperand();
  auto *Ld = dyn_cast<LoadInst>(CalledOp);
  if (!Ld) {
    // NumNonVirtualCalls++;
    return false;
  }

  APInt Offset(64, 0);
  Value *UnderlyingObject = Ld->getPointerOperand()->stripAndAccumulateInBoundsConstantOffsets(CB->getModule()->getDataLayout(), Offset);
  StringRef TypeId;
  for (auto *User : UnderlyingObject->users()) {
    if (auto *TypeTest = dyn_cast<IntrinsicInst>(User)) {
      if (TypeTest->getIntrinsicID() == Intrinsic::type_test) {
        if (auto *MV = dyn_cast<MetadataAsValue>(TypeTest->getArgOperand(1))) {
          Metadata *MD = MV->getMetadata();
          if (auto *MDStr = dyn_cast<MDString>(MD)) {
            TypeId = MDStr->getString();
            NumVirtualCalls++;
          }
          break;
        }
      }
    }
  }
  if (TypeId.empty())
    NumNonVirtualCalls++;

  for (auto &AP : TypeIdCompatibleMap[TypeId]) {
    bool Overflow;
    if (auto *F = dyn_cast<Function>(ConstantFoldLoadFromConstPtr(AP.first, PointerType::get(M.getContext(), 0), Offset.uadd_ov(APInt(64, AP.second), Overflow), M.getDataLayout()))) {
      if (F->isDeclaration())
        return false;
      if (F->getName() != "__cxa_pure_virtual")
        Candidates.push_back(F);
    }
  }
  return !Candidates.empty();
}

bool VCallCandidatesAnalyzer::getCallerCandidates(Function *VF, SmallVectorImpl<User *> &Callers) {
  if (CallerMap.contains(VF)) {
    Callers.append(CallerMap[VF].begin(), CallerMap[VF].end());
    return true;
  }
  return false;
}
