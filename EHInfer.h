#ifndef EHINFER_H_
#define EHINFER_H_

#include <llvm/IR/PassManager.h>

using namespace llvm;

struct EHInferPass : PassInfoMixin<EHInferPass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};


#endif // EHINFER_H_
