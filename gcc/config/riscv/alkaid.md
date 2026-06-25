(define_automaton "alkaid")

;; The current Alkaid RTL is a single-dispatch core with a four-entry
;; commit-id scoreboard and a one-entry wait queue for limited out-of-order
;; issue.  Integer multiply and divide are single non-pipelined instances;
;; their RV64 implementations are iterative.
;; The RTL implements Zba/Zbs and selected Zbb-like ALU operations, but not
;; the complete Zbb extension.
;;
;; Latencies below distinguish normal register-file availability from explicit
;; bypass paths:
;; - ALU results can feed ALU/branch/MUL/store-data one cycle after issue, but
;;   load/store address generation sees them through the registered AGU
;;   forward path.
;; - Load data can feed ALU/branch/store-data on the live LSU bypass.  A later
;;   load/store address use waits for the registered AGU forward path.
;; - MUL results can feed ALU/branch/MUL/store-data on the live MUL bypass;
;;   memory addresses wait for normal writeback.
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
(define_insn_reservation "alkaid_alu" 2
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type"
         "unknown,const,arith,shift,slt,multi,auipc,nop,logical,move,bitmanip,rotate,min,max,minu,maxu,clz,ctz,atomic,condmove,mvpair,zicond,sfb_alu"))
  "alkaid_issue+alkaid_alu,alkaid_wb_pipe")

;; RV64/Zbb population count is modeled as an ALU-style scalar operation so
;; full-B -march strings do not leave the DFA without a reservation.
(define_insn_reservation "alkaid_cpop" 2
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "cpop"))
  "alkaid_issue+alkaid_alu,alkaid_wb_pipe")

;; TCM-hit load model.  AXI/uncached accesses are variable latency in RTL.
(define_insn_reservation "alkaid_load" 3
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "load"))
  "alkaid_issue+alkaid_lsu_rd,nothing,alkaid_wb_pipe")

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
;; SImode MULW/RV32 low forms use the FAST_RESULT path; DImode RV64 full-width
;; multiplies iterate over all 16 partial products before RESULT.
(define_insn_reservation "alkaid_imulsi" 3
  (and (eq_attr "tune" "alkaid")
       (and (eq_attr "type" "imul")
            (eq_attr "mode" "SI")))
  "alkaid_issue+alkaid_imul,alkaid_imul+alkaid_wb_pipe")

(define_insn_reservation "alkaid_imuldi" 18
  (and (eq_attr "tune" "alkaid")
       (and (eq_attr "type" "imul")
            (eq_attr "mode" "DI")))
  "alkaid_issue+alkaid_imul,alkaid_imul*16,alkaid_imul+alkaid_wb_pipe")

(define_insn_reservation "alkaid_imul" 18
  (and (eq_attr "tune" "alkaid")
       (and (eq_attr "type" "imul")
            (not (eq_attr "mode" "SI,DI"))))
  "alkaid_issue+alkaid_imul,alkaid_imul*16,alkaid_imul+alkaid_wb_pipe")

;; Carry-less multiply appears as a separate GCC scheduling type when the full
;; B extension is selected.  Model it on the RV64 iterative multiply resource.
(define_insn_reservation "alkaid_clmul" 18
  (and (eq_attr "tune" "alkaid")
       (eq_attr "type" "clmul"))
  "alkaid_issue+alkaid_imul,alkaid_imul*16,alkaid_imul+alkaid_wb_pipe")

;; Integer divide is a single non-pipelined restoring divider.  RV32 performs
;; one step per quotient bit.  RV64 performs low/high half steps per quotient
;; bit, so full-width operations take roughly twice as many cycles.  GCC marks
;; RV64 word-divide patterns as DImode too, so this is deliberately
;; conservative for DIVW/REMW.
(define_insn_reservation "alkaid_idivsi" 34
  (and (eq_attr "tune" "alkaid")
       (and (eq_attr "type" "idiv")
            (eq_attr "mode" "SI")))
  "alkaid_issue+alkaid_idiv,alkaid_idiv*32,alkaid_idiv+alkaid_wb_pipe")

(define_insn_reservation "alkaid_idivdi" 130
  (and (eq_attr "tune" "alkaid")
       (and (eq_attr "type" "idiv")
            (eq_attr "mode" "DI")))
  "alkaid_issue+alkaid_idiv,alkaid_idiv*128,alkaid_idiv+alkaid_wb_pipe")

;; Explicit bypasses modeled from dispatch.sv/hdu.sv:
;; - ALU bank bypass feeds ALU/BJP/MUL consumers and store data.
;; - LSU live bypass feeds ALU/BJP consumers and store data, not MUL.
;; - MUL live bypass feeds ALU/BJP/MUL consumers and store data.
;; Store-address and load-address dependencies intentionally use the default
;; reservation latency rather than the store-data bypass.
(define_bypass 1 "alkaid_alu"
  "alkaid_alu,alkaid_branch_pred,alkaid_branch_nopred,
   alkaid_jump_wb_jalr,alkaid_ret,alkaid_cpop,
   alkaid_imulsi,alkaid_imuldi,alkaid_imul,alkaid_clmul")

(define_bypass 1 "alkaid_cpop"
  "alkaid_alu,alkaid_branch_pred,alkaid_branch_nopred,
   alkaid_jump_wb_jalr,alkaid_ret,alkaid_cpop,
   alkaid_imulsi,alkaid_imuldi,alkaid_imul,alkaid_clmul")

(define_bypass 1 "alkaid_alu"
  "alkaid_store" "riscv_store_data_bypass_p")

(define_bypass 2 "alkaid_load"
  "alkaid_alu,alkaid_branch_pred,alkaid_branch_nopred,
   alkaid_jump_wb_jalr,alkaid_ret,alkaid_cpop")

(define_bypass 2 "alkaid_load"
  "alkaid_store" "riscv_store_data_bypass_p")

(define_bypass 2 "alkaid_imulsi"
  "alkaid_alu,alkaid_branch_pred,alkaid_branch_nopred,
   alkaid_jump_wb_jalr,alkaid_ret,alkaid_cpop,
   alkaid_imulsi,alkaid_imuldi,alkaid_imul,alkaid_clmul")

(define_bypass 2 "alkaid_imulsi"
  "alkaid_store" "riscv_store_data_bypass_p")

(define_bypass 17 "alkaid_imuldi,alkaid_imul,alkaid_clmul"
  "alkaid_alu,alkaid_branch_pred,alkaid_branch_nopred,
   alkaid_jump_wb_jalr,alkaid_ret,alkaid_cpop,
   alkaid_imulsi,alkaid_imuldi,alkaid_imul,alkaid_clmul")

(define_bypass 17 "alkaid_imuldi,alkaid_imul,alkaid_clmul"
  "alkaid_store" "riscv_store_data_bypass_p")

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
