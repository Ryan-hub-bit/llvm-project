; RUN: llc < %s | FileCheck %s

target triple = "x86_64-unknown-linux-gnu"

; This local function is not address-taken and precedes the indirect caller.
; It gets function kind 2 because its generalized type is used by the call,
; while the existing indirect-target function kind remains 1.
define internal i32 @same_type_not_taken(i32 %x) !type !0 {
; CHECK-LABEL: same_type_not_taken:
; CHECK: "{{.*}}-2-{{[0-9a-f]+}}-b23cfe0c3eb850a1":
  ret i32 %x
}

; A non-address-taken function with a different type must remain unlabeled.
define internal void @different_type() !type !1 {
; CHECK-LABEL: different_type:
; CHECK-NOT: "{{.*}}-{{[12]}}-{{[0-9a-f]+}}-{{[0-9a-f]+}}":
; CHECK: retq
  ret void
}

define i32 @caller(ptr %callee, i32 %x) !type !2 {
; CHECK-LABEL: caller:
; CHECK: "{{.*}}-1-{{[0-9a-f]+}}-{{[0-9a-f]+}}":
  %result = call i32 %callee(i32 %x)
      [ "type"(metadata !"_ZTSFiiE.generalized") ]
  ret i32 %result
}

!0 = !{i64 0, !"_ZTSFiiE.generalized"}
!1 = !{i64 0, !"_ZTSFvvE.generalized"}
!2 = !{i64 0, !"_ZTSFiPviE.generalized"}
