#include "gtest/gtest.h"
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/AsmParser/Parser.h>
#include "IndirectCallAnalysis.h"
#include "ICallSolver.h"
#include "EHGraphPrinter.h"

using namespace llvm;

TEST(NestTryCatch, Basic) {
StringRef Assembly = R"(
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-linux-gnu"

$_ZTS1A = comdat any

$_ZTI1A = comdat any

$_ZTS1B = comdat any

$_ZTI1B = comdat any

@_ZTVN10__cxxabiv117__class_type_infoE = external global [0 x ptr]
@_ZTS1A = linkonce_odr dso_local constant [3 x i8] c"1A\00", comdat, align 1
@_ZTI1A = linkonce_odr dso_local constant { ptr, ptr } { ptr getelementptr inbounds (ptr, ptr @_ZTVN10__cxxabiv117__class_type_infoE, i64 2), ptr @_ZTS1A }, comdat, align 8
@_ZTS1B = linkonce_odr dso_local constant [3 x i8] c"1B\00", comdat, align 1
@_ZTI1B = linkonce_odr dso_local constant { ptr, ptr } { ptr getelementptr inbounds (ptr, ptr @_ZTVN10__cxxabiv117__class_type_infoE, i64 2), ptr @_ZTS1B }, comdat, align 8

; Function Attrs: mustprogress noinline noreturn uwtable
define dso_local void @_Z7throw_av() local_unnamed_addr {
  %1 = tail call ptr @__cxa_allocate_exception(i64 1)
  tail call void @__cxa_throw(ptr %1, ptr nonnull @_ZTI1A, ptr null)
  unreachable
}

declare ptr @__cxa_allocate_exception(i64) local_unnamed_addr

declare void @__cxa_throw(ptr, ptr, ptr) local_unnamed_addr

; Function Attrs: mustprogress noinline uwtable
define dso_local void @_Z4testv() local_unnamed_addr #1 personality ptr @__gxx_personality_v0 {
  invoke void @_Z7throw_av()
          to label %9 unwind label %1

1:                                                ; preds = %0
  %2 = landingpad { ptr, i32 }
          catch ptr @_ZTI1B
          catch ptr @_ZTI1A
  %3 = extractvalue { ptr, i32 } %2, 1
  %4 = tail call i32 @llvm.eh.typeid.for(ptr nonnull @_ZTI1B)
  %5 = icmp eq i32 %3, %4
  br i1 %5, label %10, label %6

6:                                                ; preds = %1
  %7 = tail call i32 @llvm.eh.typeid.for(ptr nonnull @_ZTI1A)
  %8 = icmp eq i32 %3, %7
  br i1 %8, label %10, label %13

9:                                                ; preds = %0
  unreachable

10:                                               ; preds = %6, %1
  %11 = extractvalue { ptr, i32 } %2, 0
  %12 = tail call ptr @__cxa_begin_catch(ptr %11)
  tail call void @__cxa_end_catch()
  ret void

13:                                               ; preds = %6
  resume { ptr, i32 } %2
}

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nofree nosync nounwind memory(none)
declare i32 @llvm.eh.typeid.for(ptr)

declare ptr @__cxa_begin_catch(ptr) local_unnamed_addr

declare void @__cxa_end_catch() local_unnamed_addr

; Function Attrs: mustprogress uwtable
define dso_local void @_Z9call_testv() {
  tail call void @_Z4testv()
  ret void
}
)";
  LLVMContext Context;
  SMDiagnostic Error;
  std::unique_ptr<Module> M = parseAssemblyString(Assembly, Error, Context);
  VCallCandidatesAnalyzer Analyzer(*M);
  ICallSolver Solver(*M);
  Value *CxaThrow = M->getNamedValue("__cxa_throw");
  auto CurException = std::make_unique<CurrentException>(cast<CallBase>(CxaThrow->getUniqueUndroppableUser()));
  EHGraphDOTInfo GInfo(CurException.get(), nullptr, Analyzer, Solver);

  // Make sure the exception is not propagated to test funtions
  Function *CallTest = cast<Function>(M->getNamedValue("_Z9call_testv"));
  EXPECT_TRUE(!EHGraphDOTInfo::ChildsMap.contains(CallTest));

}
