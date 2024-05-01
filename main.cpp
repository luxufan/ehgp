#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/CodeGen/CommandFlags.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/IR/Verifier.h>

#include "IndirectCallAnalysis.h"
#include "EHGraphPrinter.h"

using namespace llvm;

static codegen::RegisterCodeGenFlags CFG;

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode file>"), cl::init("-"), cl::value_desc("filename"));

bool runPasses(Module &M) {
  ModuleAnalysisManager MAM;
  PassBuilder PB;
  MAM.registerPass([&] { return VCallAnalysis(); });
  MAM.registerPass([&] { return PassInstrumentationAnalysis(); });
  ModulePassManager MPM;
  MPM.addPass(EHInferPass());
  MPM.run(M, MAM);
  return 1;
}

int main(int argc, char *argv[]) {
  InitLLVM X(argc, argv);

  LLVMContext Context;
  SMDiagnostic Err;

  cl::ParseCommandLineOptions(argc, argv, "exception propagation graph printer\n");

  std::unique_ptr<Module> M;
  M = parseIRFile(InputFilename, Err, Context);
  if (!M) {
    Err.print(argv[0], errs());
    return 1;
  }

  if (verifyModule(*M, &errs())) {
    errs() << argv[0] << ": " << InputFilename
           << ": error: input module is broken!\n";
    return 1;
  }

  runPasses(*M);

  return 0;
}
