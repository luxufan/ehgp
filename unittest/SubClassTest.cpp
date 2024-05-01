#include "gtest/gtest.h"
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/AsmParser/Parser.h>
#include "IndirectCallAnalysis.h"

using namespace llvm;

TEST(SubClassTest, Basic) {
StringRef Assembly = R"(
target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-linux-gnu"

@_ZTV2B1 = linkonce_odr hidden unnamed_addr constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr @_ZTI2B1, ptr @_ZN2B13fooEv] }, align 8, !type !0, !type !1, !type !2, !type !3, !vcall_visibility !4
@_ZTVN10__cxxabiv120__si_class_type_infoE = external global [0 x ptr]
@_ZTS2B1 = linkonce_odr hidden constant [4 x i8] c"2B1\00", align 1
@_ZTVN10__cxxabiv117__class_type_infoE = external global [0 x ptr]
@_ZTS1A = linkonce_odr hidden constant [3 x i8] c"1A\00", align 1
@_ZTI1A = linkonce_odr hidden constant { ptr, ptr } { ptr getelementptr inbounds (ptr, ptr @_ZTVN10__cxxabiv117__class_type_infoE, i64 2), ptr @_ZTS1A }, align 8
@_ZTI2B1 = linkonce_odr hidden constant { ptr, ptr, ptr } { ptr getelementptr inbounds (ptr, ptr @_ZTVN10__cxxabiv120__si_class_type_infoE, i64 2), ptr @_ZTS2B1, ptr @_ZTI1A }, align 8
@_ZTV2B2 = linkonce_odr hidden unnamed_addr constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr @_ZTI2B2, ptr @_ZN2B23fooEv] }, align 8, !type !0, !type !1, !type !5, !type !6, !vcall_visibility !4
@_ZTS2B2 = linkonce_odr hidden constant [4 x i8] c"2B2\00", align 1
@_ZTI2B2 = linkonce_odr hidden constant { ptr, ptr, ptr } { ptr getelementptr inbounds (ptr, ptr @_ZTVN10__cxxabiv120__si_class_type_infoE, i64 2), ptr @_ZTS2B2, ptr @_ZTI1A }, align 8

; Function Attrs: mustprogress noinline uwtable
define hidden void @_Z5vcallP1A(ptr %a) {
entry:
  %vtable = load ptr, ptr %a, align 8
  %0 = tail call i1 @llvm.type.test(ptr %vtable, metadata !"_ZTS1A")
  tail call void @llvm.assume(i1 %0)
  %1 = load ptr, ptr %vtable, align 8
  tail call void %1(ptr %a)
  ret void
}

declare i1 @llvm.type.test(ptr, metadata)

declare void @llvm.assume(i1 noundef)

define linkonce_odr hidden void @_ZN2B13fooEv(ptr %this) {
entry:
  ret void
}

define linkonce_odr hidden void @_ZN2B23fooEv(ptr %this) {
entry:
  ret void
}
!0 = !{i64 16, !"_ZTS1A"}
!1 = !{i64 16, !"_ZTSM1AFvvE.virtual"}
!2 = !{i64 16, !"_ZTS2B1"}
!3 = !{i64 16, !"_ZTSM2B1FvvE.virtual"}
!4 = !{i64 1}
!5 = !{i64 16, !"_ZTS2B2"}
!6 = !{i64 16, !"_ZTSM2B2FvvE.virtual"}

)";
  LLVMContext Context;
  SMDiagnostic Error;

  std::unique_ptr<Module> M = parseAssemblyString(Assembly, Error, Context);
  VCallCandidatesAnalyzer Analyzer(*M);
  Analyzer.analyze();

  Value *A = M->getNamedGlobal("_ZTI1A");
  Value *B1 = M->getNamedGlobal("_ZTI2B1");
  Value *B2 = M->getNamedGlobal("_ZTI2B2");

  EXPECT_TRUE(Analyzer.derivedFrom(A, B1));
  EXPECT_TRUE(Analyzer.derivedFrom(A, B2));
}
