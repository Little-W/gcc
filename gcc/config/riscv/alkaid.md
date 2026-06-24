(define_automaton "alkaid")

;; The current Alkaid RTL is a single-dispatch core with a four-entry
;; commit-id scoreboard and a one-entry wait queue for limited out-of-order
;; issue.  Integer multiply and divide are single non-pipelined instances.
;; The RTL implements Zba/Zbs and selected Zbb-like ALU operations, but not
;; the complete Zbb extension.
(define_cpu_unit "alkaid_issue"   "alkaid")
(define_cpu_unit "alkaid_alu"     "alkaid")
(define_cpu_unit "alkaid_imul"    "alkaid")
(define_cpu_unit "alkaid_idiv"    "alkaid")
;; No hardware FPU exists in the current RTL; keep these as fallback costs for
;; externally supplied FP implementations or hand-selected -march values.
(define_cpu_unit "alkaid_fpu"     "alkaid")
(define_cpu_unit "alkaid_wb_pipe" "alkaid")
(define_cpu_unit "alkaid_lsu_rd"  "alkaid")
(define_cpu_unit "alkaid_lsu_wr"  "alkaid")

;; ALU, shifts, bitmanip, CSR-style integer results.
(define_insn_reservation "alkaid_alu" 1
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type"
         "unknown,const,arith,shift,slt,multi,auipc,nop,logical,move,bitmanip,rotate,min,max,minu,maxu,clz,ctz,atomic,condmove,mvpair,zicond"))
  "alkaid_issue+alkaid_alu+alkaid_wb_pipe")

;; TCM-hit load model.  AXI/uncached accesses are variable latency in RTL.
(define_insn_reservation "alkaid_load" 2
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "load"))
  "alkaid_issue+alkaid_lsu_rd,alkaid_wb_pipe")

;; Stores enter the LSU write path and can be buffered.
(define_insn_reservation "alkaid_store" 1
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "store"))
  "alkaid_issue+alkaid_lsu_wr")

;; Floating-point load/store fallback rules.
(define_insn_reservation "alkaid_fpload_sf" 2
  (and (eq_attr "tune" "alkaid")
       (and (eq_attr "type" "fpload")
            (eq_attr "mode" "SF")))
  "alkaid_issue+alkaid_lsu_rd,alkaid_wb_pipe")

(define_insn_reservation "alkaid_fpstore_sf" 1
  (and (eq_attr "tune" "alkaid")
       (and (eq_attr "type" "fpstore")
            (eq_attr "mode" "SF")))
  "alkaid_issue+alkaid_lsu_wr")

(define_insn_reservation "alkaid_fpload_df" 3
  (and (eq_attr "tune" "alkaid")
       (and (eq_attr "type" "fpload")
            (eq_attr "mode" "DF")))
  "alkaid_issue+alkaid_lsu_rd,nothing,alkaid_wb_pipe")

(define_insn_reservation "alkaid_fpstore_df" 2
  (and (eq_attr "tune" "alkaid")
       (and (eq_attr "type" "fpstore")
            (eq_attr "mode" "DF")))
  "alkaid_issue+alkaid_lsu_wr,alkaid_lsu_wr")

(define_insn_reservation "alkaid_fpload_generic" 2
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "fpload"))
  "alkaid_issue+alkaid_lsu_rd,alkaid_wb_pipe")

(define_insn_reservation "alkaid_fpstore_generic" 1
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "fpstore"))
  "alkaid_issue+alkaid_lsu_wr")

;; Branch costs model front-end redirect bubbles.  The RTL predictor combines
;; a static baseline with BHT/loop/correlation predictors; GCC can only model
;; the static side here.
(define_insn_reservation "alkaid_branch_pred" 1
  (and (eq_attr "tune" "alkaid")
       (and (eq_attr "type" "branch")
            (match_test "alkaid_branch_predicted_p (insn)")))
  "alkaid_issue")

(define_insn_reservation "alkaid_branch_nopred" 4
  (and (eq_attr "tune" "alkaid")
       (and (eq_attr "type" "branch")
            (match_test "!alkaid_branch_predicted_p (insn)")))
  "alkaid_issue,alkaid_issue,alkaid_issue,alkaid_issue")

(define_insn_reservation "alkaid_jump_wb_jal" 2
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "jump"))
  "alkaid_issue+alkaid_alu,alkaid_wb_pipe")

(define_insn_reservation "alkaid_jump_wb_jalr" 4
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "jalr,call"))
  "alkaid_issue+alkaid_alu,alkaid_wb_pipe+alkaid_issue,alkaid_issue,alkaid_issue")

(define_insn_reservation "alkaid_ret" 4
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "ret"))
  "alkaid_issue+alkaid_alu,alkaid_wb_pipe+alkaid_issue,alkaid_issue,alkaid_issue")

(define_insn_reservation "alkaid_trap" 7
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "trap"))
  "alkaid_issue,alkaid_issue,alkaid_issue,alkaid_issue,alkaid_issue,alkaid_issue,alkaid_issue")

;; Integer multiply is a single non-pipelined 16x16 segmented multiplier.
;; RV64 full-width multiplies take the ROW+RESULT path; MULW/low RV32 forms
;; can complete earlier, but GCC schedules the common imul type at 3 cycles.
(define_insn_reservation "alkaid_imul" 3
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "imul"))
  "alkaid_issue+alkaid_imul,alkaid_imul,alkaid_imul+alkaid_wb_pipe")

;; Integer divide is a single non-pipelined restoring divider.  Non-zero word
;; operations take START + 32 CALC + END cycles; full RV64 operations take
;; START + 64 CALC + END.  Divide-by-zero is a fast path that is not modeled.
(define_insn_reservation "alkaid_idivsi" 34
  (and (eq_attr "tune" "alkaid")
       (and (eq_attr "type" "idiv")
            (eq_attr "mode" "SI")))
  "alkaid_issue+alkaid_idiv,alkaid_idiv*32,alkaid_idiv+alkaid_wb_pipe")

(define_insn_reservation "alkaid_idivdi" 66
  (and (eq_attr "tune" "alkaid")
       (and (eq_attr "type" "idiv")
            (eq_attr "mode" "DI")))
  "alkaid_issue+alkaid_idiv,alkaid_idiv*64,alkaid_idiv+alkaid_wb_pipe")

;; Floating-point fallback rules.  The present Alkaid RTL does not decode F/D.
(define_insn_reservation "alkaid_fmisc" 3
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "mfc,mtc,fmove,fcmp"))
  "alkaid_issue+alkaid_fpu,alkaid_fpu,alkaid_wb_pipe")

(define_insn_reservation "alkaid_fadd" 9
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "fadd"))
  "alkaid_issue+alkaid_fpu,alkaid_fpu*7,alkaid_wb_pipe")

(define_insn_reservation "alkaid_fdiv" 29
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "fdiv"))
  "alkaid_issue+alkaid_fpu,alkaid_fpu*27,alkaid_wb_pipe")

(define_insn_reservation "alkaid_fsqrt" 35
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "fsqrt"))
  "alkaid_issue+alkaid_fpu,alkaid_fpu*33,alkaid_wb_pipe")

(define_insn_reservation "alkaid_fmul" 11
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "fmul,fmadd"))
  "alkaid_issue+alkaid_fpu,alkaid_fpu*9,alkaid_wb_pipe")

(define_insn_reservation "alkaid_fcvt_f2i" 6
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "fcvt_f2i"))
  "alkaid_issue+alkaid_fpu,alkaid_fpu*4,alkaid_wb_pipe")

(define_insn_reservation "alkaid_fcvt_2f" 7
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "fcvt_i2f,fcvt"))
  "alkaid_issue+alkaid_fpu,alkaid_fpu*5,alkaid_wb_pipe")
