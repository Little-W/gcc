(define_automaton "alioth")

;; Alioth Core Pipeline Resource Definition
;; 单发射流水线，独立的 load/store，两个整数乘法单元，两个整数除法单元，浮点单元分杂项/乘加/除法

(define_cpu_unit "alioth_ex_pipe" "alioth")
(define_cpu_unit "alioth_imul0" "alioth")
(define_cpu_unit "alioth_imul1" "alioth")
(define_cpu_unit "alioth_idiv0" "alioth")
(define_cpu_unit "alioth_idiv1" "alioth")
(define_cpu_unit "alioth_fmisc" "alioth")
(define_cpu_unit "alioth_fmac" "alioth")
(define_cpu_unit "alioth_fdiv" "alioth")
(define_cpu_unit "alioth_wb_pipe" "alioth")

;; Shortcuts
(define_reservation "alioth_imul" "alioth_imul0|alioth_imul1")
(define_reservation "alioth_idiv" "alioth_idiv0|alioth_idiv1")

;; ALU/逻辑/移位等 (仅支持 IMDF)
(define_insn_reservation "alioth_alu" 1
(and (eq_attr "tune" "alioth")
(eq_attr "type" "unknown,const,arith,shift,slt,multi,auipc,nop,logical,move,bitmanip,rotate,min,max,minu,maxu,clz,ctz,atomic,condmove,mvpair,zicond"))
"alioth_ex_pipe+alioth_wb_pipe")

;; SFB ALU operations
(define_insn_reservation "alioth_sfb_alu" 2
(and (eq_attr "tune" "alioth")
(eq_attr "type" "sfb_alu"))
"alioth_ex_pipe+alioth_wb_pipe")

;; Load
(define_insn_reservation "alioth_load" 2
(and (eq_attr "tune" "alioth")
(eq_attr "type" "load,fpload"))
"alioth_ex_pipe,alioth_wb_pipe")

;; Store
(define_insn_reservation "alioth_store" 1
(and (eq_attr "tune" "alioth")
(eq_attr "type" "store,fpstore"))
"alioth_ex_pipe")

;; Branch
(define_insn_reservation "alioth_branch" 1
(and (eq_attr "tune" "alioth")
(eq_attr "type" "branch,jump,call,jalr,ret,trap"))
"alioth_ex_pipe")

;; Integer Multiply (7 cycles, not pipelined, 2 units)
(define_insn_reservation "alioth_imul" 7
(and (eq_attr "tune" "alioth")
(eq_attr "type" "imul"))
"alioth_ex_pipe+alioth_imul,alioth_imul*5,alioth_wb_pipe")

;; Integer Divide (按 generic.md 风格设置不同位宽的延迟)
(define_insn_reservation "alioth_idivsi" 34
(and (eq_attr "tune" "alioth")
     (and (eq_attr "type" "idiv")
          (eq_attr "mode" "SI")))
"alioth_ex_pipe+alioth_idiv,alioth_idiv*32,alioth_wb_pipe")

(define_insn_reservation "alioth_idivdi" 66
(and (eq_attr "tune" "alioth")
     (and (eq_attr "type" "idiv")
          (eq_attr "mode" "DI")))
"alioth_ex_pipe+alioth_idiv,alioth_idiv*64,alioth_wb_pipe")

;; Popcount and clmul
(define_insn_reservation "alioth_popcount" 2
(and (eq_attr "tune" "alioth")
(eq_attr "type" "cpop,clmul"))
"alioth_ex_pipe+alioth_wb_pipe")

;; Floating-point transfer/conversion/comparison
(define_insn_reservation "alioth_fmisc" 3
(and (eq_attr "tune" "alioth")
(eq_attr "type" "mfc,mtc,fcvt,fcvt_i2f,fcvt_f2i,fmove,fcmp"))
"alioth_ex_pipe+alioth_fmisc,alioth_wb_pipe")

;; Floating-point add/mul/fma - half precision
(define_insn_reservation "alioth_fmac_hf" 5
(and (eq_attr "tune" "alioth")
     (and (eq_attr "type" "fadd,fmul,fmadd")
          (eq_attr "mode" "HF")))
"alioth_ex_pipe+alioth_fmac,alioth_wb_pipe")

;; Floating-point add/mul/fma - single precision
(define_insn_reservation "alioth_fmac_sf" 5
(and (eq_attr "tune" "alioth")
     (and (eq_attr "type" "fadd,fmul,fmadd")
          (eq_attr "mode" "SF")))
"alioth_ex_pipe+alioth_fmac,alioth_wb_pipe")

;; Floating-point add/mul/fma - double precision
(define_insn_reservation "alioth_fmac_df" 7
(and (eq_attr "tune" "alioth")
     (and (eq_attr "type" "fadd,fmul,fmadd")
          (eq_attr "mode" "DF")))
"alioth_ex_pipe+alioth_fmac,alioth_wb_pipe")

;; Floating-point divide
(define_insn_reservation "alioth_fdiv" 20
(and (eq_attr "tune" "alioth")
(eq_attr "type" "fdiv"))
"alioth_ex_pipe+alioth_fdiv,alioth_fdiv*18,alioth_wb_pipe")

;; Floating-point sqrt
(define_insn_reservation "alioth_fsqrt" 25
(and (eq_attr "tune" "alioth")
(eq_attr "type" "fsqrt"))
"alioth_ex_pipe+alioth_fdiv,alioth_fdiv*23,alioth_wb_pipe")