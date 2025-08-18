(define_automaton "alioth")

;; ---------------------------
;; Pipeline resources
;; ---------------------------
(define_cpu_unit "alioth_issue"  "alioth")  ;; 前端/发射资源（模拟取指/重定向停顿）
(define_cpu_unit "alioth_alu"    "alioth")  ;; 独立 ALU 执行资源
(define_cpu_unit "alioth_idiv0"  "alioth")
(define_cpu_unit "alioth_idiv1"  "alioth")
(define_cpu_unit "alioth_fpu"    "alioth")
(define_cpu_unit "alioth_wb_pipe" "alioth")
(define_cpu_unit "alioth_lsu_rd" "alioth")  ;; LSU 读取资源
(define_cpu_unit "alioth_lsu_wr" "alioth")  ;; LSU 写入资源

;; ---------------------------
;; Reservations
;; ---------------------------

;; ALU/逻辑/移位/CSR 等（1-cycle，首拍占用 issue）
(define_insn_reservation "alioth_alu" 1
  (and (eq_attr "tune" "alioth")
       (eq_attr "type"
         "unknown,const,arith,shift,slt,multi,auipc,nop,logical,move,bitmanip,rotate,min,max,minu,maxu,clz,ctz,atomic,condmove,mvpair,zicond"))
  "alioth_issue+alioth_alu+alioth_wb_pipe")

;; Load（两拍返回；第 2 拍写回）
(define_insn_reservation "alioth_load" 2
  (and (eq_attr "tune" "alioth")
       (eq_attr "type" "load"))
  "alioth_issue+alioth_lsu_rd,alioth_wb_pipe")

;; Store（1 拍；仅占用 issue 和 lsu 写入）
(define_insn_reservation "alioth_store" 1
  (and (eq_attr "tune" "alioth")
       (eq_attr "type" "store"))
  "alioth_issue+alioth_lsu_wr")

;; 单精度浮点读写（与整数一致）
(define_insn_reservation "alioth_fpload_sf" 2
  (and (eq_attr "tune" "alioth")
       (and (eq_attr "type" "fpload")
            (eq_attr "mode" "SF")))
  "alioth_issue+alioth_lsu_rd,alioth_wb_pipe")

(define_insn_reservation "alioth_fpstore_sf" 1
  (and (eq_attr "tune" "alioth")
       (and (eq_attr "type" "fpstore")
            (eq_attr "mode" "SF")))
  "alioth_issue+alioth_lsu_wr")

;; 双精度浮点读取（2 周期 lsu_rd + 1 周期 wb_pipe）
(define_insn_reservation "alioth_fpload_df" 3
  (and (eq_attr "tune" "alioth")
       (and (eq_attr "type" "fpload")
            (eq_attr "mode" "DF")))
  "alioth_issue+alioth_lsu_rd,nothing,alioth_wb_pipe")

;; 双精度浮点写入（2 周期 lsu_wr）
(define_insn_reservation "alioth_fpstore_df" 2
  (and (eq_attr "tune" "alioth")
       (and (eq_attr "type" "fpstore")
            (eq_attr "mode" "DF")))
  "alioth_issue+alioth_lsu_wr,alioth_lsu_wr")

;; ---- 控制转移：按重定向气泡建模 ----

;; 被预测覆盖的 branch：无额外停顿（仅 1 拍占用 issue）
(define_insn_reservation "alioth_branch_pred" 1
  (and (eq_attr "tune" "alioth")
       (and (eq_attr "type" "branch")
            (match_test "alioth_branch_predicted_p (insn)")))
  "alioth_issue")

;; 未被预测覆盖的 branch：4-cycle 前端停顿
(define_insn_reservation "alioth_branch_nopred" 4
  (and (eq_attr "tune" "alioth")
       (and (eq_attr "type" "branch")
            (match_test "!alioth_branch_predicted_p (insn)")))
  "alioth_issue,alioth_issue,alioth_issue,alioth_issue")

;; jal：需要写回 rd（PC+4），并造成 1-cycle 前端停顿
(define_insn_reservation "alioth_jump_wb_jal" 2
  (and (eq_attr "tune" "alioth")
       (eq_attr "type" "jump"))
  "alioth_issue+alioth_alu,alioth_wb_pipe")

;; jalr / call：需要写回，且 4-cycle 前端停顿
(define_insn_reservation "alioth_jump_wb_jalr" 4
  (and (eq_attr "tune" "alioth")
       (eq_attr "type" "jalr,call"))
  "alioth_issue+alioth_alu,alioth_wb_pipe+alioth_issue,alioth_issue,alioth_issue")

;; ret：与 jalr 类似，4-cycle 前端停顿+写回
(define_insn_reservation "alioth_ret" 4
  (and (eq_attr "tune" "alioth")
       (eq_attr "type" "ret"))
  "alioth_issue+alioth_alu,alioth_wb_pipe+alioth_issue,alioth_issue,alioth_issue")

;; trap（ecall/ebreak）：与未预测分支类似，7-cycle 前端停顿
(define_insn_reservation "alioth_trap" 7
  (and (eq_attr "tune" "alioth")
       (eq_attr "type" "trap"))
  "alioth_issue,alioth_issue,alioth_issue,alioth_issue,alioth_issue,alioth_issue,alioth_issue")

;; Integer Multiply（4-cycle，可流水；首拍占 issue）
(define_insn_reservation "alioth_imul" 4
  (and (eq_attr "tune" "alioth")
       (eq_attr "type" "imul"))
  "alioth_issue,nothing*2,alioth_wb_pipe")

;; Integer Divide (32-bit)（两单元互斥；首拍占 issue）
(define_insn_reservation "alioth_idivsi" 36
  (and (eq_attr "tune" "alioth")
       (and (eq_attr "type" "idiv")
            (eq_attr "mode" "SI")))
  "alioth_issue,(alioth_idiv0*34|alioth_idiv1*34),alioth_wb_pipe")

;; 浮点类（首拍占 issue）
(define_insn_reservation "alioth_fmisc" 3
  (and (eq_attr "tune" "alioth")
       (eq_attr "type" "mfc,mtc,fmove,fcmp"))
  "alioth_issue+alioth_fpu,alioth_fpu,alioth_wb_pipe")

(define_insn_reservation "alioth_fadd" 9
  (and (eq_attr "tune" "alioth")
       (eq_attr "type" "fadd"))
  "alioth_issue+alioth_fpu,alioth_fpu*7,alioth_wb_pipe")

(define_insn_reservation "alioth_fdiv" 29
  (and (eq_attr "tune" "alioth")
       (eq_attr "type" "fdiv"))
  "alioth_issue+alioth_fpu,alioth_fpu*27,alioth_wb_pipe")

(define_insn_reservation "alioth_fsqrt" 35
  (and (eq_attr "tune" "alioth")
       (eq_attr "type" "fsqrt"))
  "alioth_issue+alioth_fpu,alioth_fpu*33,alioth_wb_pipe")

(define_insn_reservation "alioth_fmul" 11
  (and (eq_attr "tune" "alioth")
       (eq_attr "type" "fmul,fmadd"))
  "alioth_issue+alioth_fpu,alioth_fpu*9,alioth_wb_pipe")

(define_insn_reservation "alioth_fcvt_f2i" 6
  (and (eq_attr "tune" "alioth")
       (eq_attr "type" "fcvt_f2i"))
  "alioth_issue+alioth_fpu,alioth_fpu*4,alioth_wb_pipe")

(define_insn_reservation "alioth_fcvt_2f" 7
  (and (eq_attr "tune" "alioth")
       (eq_attr "type" "fcvt_i2f,fcvt"))
  "alioth_issue+alioth_fpu,alioth_fpu*5,alioth_wb_pipe")
