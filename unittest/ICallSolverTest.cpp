#include "gtest/gtest.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/AsmParser/Parser.h>
#include "ICallSolver.h"

TEST(ICallSolverTest, Basic) {
StringRef Assembly = R"(
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-linux-gnu"

define dso_local void @_Z2f1v() {
entry:
  ret void
}

define dso_local void @_Z2f2v() {
entry:
  ret void
}

define dso_local void @_Z4testPFvvE(ptr noundef %foo) {
entry:
  call void %foo()
  ret void
}

define dso_local void @_Z5call1v() {
entry:
  call void @_Z4testPFvvE(ptr noundef @_Z2f1v)
  ret void
}

define dso_local void @_Z5call2v() {
entry:
  call void @_Z4testPFvvE(ptr noundef @_Z2f2v)
  ret void
}


)";
  LLVMContext Context;
  SMDiagnostic Error;

  std::unique_ptr<Module> M = parseAssemblyString(Assembly, Error, Context);
  ICallSolver Solver(*M);
  Solver.solve();

  Function *F = cast<Function>(M->getNamedValue("_Z4testPFvvE"));
  ValueLattice &VL = Solver.getValueStates(F->arg_begin());
  EXPECT_TRUE(VL.getFuncs().size() == 2);

  SmallVector<User *, 2> Users;
  Function *F1 = cast<Function>(M->getNamedValue("_Z2f1v"));
  EXPECT_TRUE(Solver.getCallSites(F1, Users));
  EXPECT_EQ(Users.size(), 1);

  Users.clear();
  Function *F2 = cast<Function>(M->getNamedValue("_Z2f2v"));
  EXPECT_TRUE(Solver.getCallSites(F2, Users));
  EXPECT_EQ(Users.size(), 1);
}
