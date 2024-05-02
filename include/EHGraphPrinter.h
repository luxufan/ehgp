#ifndef EHINFER_H_
#define EHINFER_H_

#include <llvm/IR/PassManager.h>

using namespace llvm;

struct EHGraphPrinterPass : PassInfoMixin<EHGraphPrinterPass> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};


#endif // EHINFER_H_
