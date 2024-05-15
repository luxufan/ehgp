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

@_ZTV1B = linkonce_odr hidden unnamed_addr constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr @_ZTI1B, ptr @_ZN1B1fEv] }, align 8, !type !0, !type !1, !type !2, !type !3, !vcall_visibility !4
@_ZTVN10__cxxabiv120__si_class_type_infoE = external global [0 x ptr]
@_ZTS1B = linkonce_odr hidden constant [3 x i8] c"1B\00", align 1
@_ZTVN10__cxxabiv117__class_type_infoE = external global [0 x ptr]
@_ZTS1A = linkonce_odr hidden constant [3 x i8] c"1A\00", align 1
@_ZTI1A = linkonce_odr hidden constant { ptr, ptr } { ptr getelementptr inbounds (ptr, ptr @_ZTVN10__cxxabiv117__class_type_infoE, i64 2), ptr @_ZTS1A }, align 8
@_ZTI1B = linkonce_odr hidden constant { ptr, ptr, ptr } { ptr getelementptr inbounds (ptr, ptr @_ZTVN10__cxxabiv120__si_class_type_infoE, i64 2), ptr @_ZTS1B, ptr @_ZTI1A }, align 8
@_ZTV1A = linkonce_odr hidden unnamed_addr constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr @_ZTI1A, ptr @__cxa_pure_virtual] }, align 8, !type !0, !type !1, !vcall_visibility !4
@_ZTV1C = linkonce_odr hidden unnamed_addr constant { [3 x ptr] } { [3 x ptr] [ptr null, ptr @_ZTI1C, ptr @_ZN1C1fEv] }, align 8, !type !0, !type !1, !type !5, !type !6, !vcall_visibility !4
@_ZTS1C = linkonce_odr hidden constant [3 x i8] c"1C\00", align 1
@_ZTI1C = linkonce_odr hidden constant { ptr, ptr, ptr } { ptr getelementptr inbounds (ptr, ptr @_ZTVN10__cxxabiv120__si_class_type_infoE, i64 2), ptr @_ZTS1C, ptr @_ZTI1A }, align 8

; Function Attrs: mustprogress noinline optnone uwtable
define hidden void @_Z4testP1A(ptr noundef %a) {
entry:
  %a.addr = alloca ptr, align 8
  store ptr %a, ptr %a.addr, align 8
  %0 = load ptr, ptr %a.addr, align 8
  %vtable = load ptr, ptr %0, align 8
  %1 = call i1 @llvm.type.test(ptr %vtable, metadata !"_ZTS1A")
  call void @llvm.assume(i1 %1)
  %vfn = getelementptr inbounds ptr, ptr %vtable, i64 0
  %2 = load ptr, ptr %vfn, align 8
  call void %2(ptr noundef nonnull align 8 dereferenceable(8) %0)
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare i1 @llvm.type.test(ptr, metadata)

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: write)
declare void @llvm.assume(i1 noundef)

define linkonce_odr hidden void @_ZN1B1fEv(ptr noundef nonnull align 8 dereferenceable(8) %this) unnamed_addr align 2 {
entry:
  %this.addr = alloca ptr, align 8
  store ptr %this, ptr %this.addr, align 8
  %this1 = load ptr, ptr %this.addr, align 8
  ret void
}

declare void @__cxa_pure_virtual() unnamed_addr

define linkonce_odr hidden void @_ZN1C1fEv(ptr noundef nonnull align 8 dereferenceable(8) %this) unnamed_addr align 2 {
entry:
  %this.addr = alloca ptr, align 8
  store ptr %this, ptr %this.addr, align 8
  %this1 = load ptr, ptr %this.addr, align 8
  ret void
}

!0 = !{i64 16, !"_ZTS1A"}
!1 = !{i64 16, !"_ZTSM1AFvvE.virtual"}
!2 = !{i64 16, !"_ZTS1B"}
!3 = !{i64 16, !"_ZTSM1BFvvE.virtual"}
!4 = !{i64 1}
!5 = !{i64 16, !"_ZTS1C"}
!6 = !{i64 16, !"_ZTSM1CFvvE.virtual"}

)";
  LLVMContext Context;
  SMDiagnostic Error;

  std::unique_ptr<Module> M = parseAssemblyString(Assembly, Error, Context);
  VCallCandidatesAnalyzer Analyzer(*M);
  Analyzer.analyze();

  Function *VFB = cast<Function>(M->getNamedValue("_ZN1B1fEv"));
  SmallVector<User *> Tmp;
  //FIXME: This is wrong. The VFB has potential caller.
  EXPECT_FALSE(Analyzer.getCallerCandidates(VFB, Tmp));
}
