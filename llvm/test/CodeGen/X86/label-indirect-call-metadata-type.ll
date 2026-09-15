; RUN: llc < %s | FileCheck %s

target triple = "x86_64-unknown-linux-gnu"

; The local helper is not address-taken, but its type is used by the
; metadata-form indirect call below.  It should receive a kind-2 label.
define internal i32 @same_type_not_taken(i32 %x) !type !0 {
; CHECK-LABEL: same_type_not_taken:
; CHECK: "{{.*}}-2-{{[0-9a-f]+}}-b23cfe0c3eb850a1"
  ret i32 %x
}

; External linkage preserves the original kind-1 indirect-target label.
define i32 @target(i32 %x) !type !0 {
; CHECK-LABEL: target:
; CHECK: "{{.*}}-1-{{[0-9a-f]+}}-b23cfe0c3eb850a1"
  ret i32 %x
}

define i32 @caller(ptr %callee, i32 %x) {
; CHECK-LABEL: caller:
; CHECK: "{{.*}}-0-0-1-b23cfe0c3eb850a1-0-t-0-0-0-0-0-type":
  %result = call i32 %callee(i32 %x), !type !1
  ret i32 %result
}

!0 = !{i64 0, !"_ZTSFiiE.generalized"}
!1 = !{i64 0, !"_ZTSFiiE.generalized"}
