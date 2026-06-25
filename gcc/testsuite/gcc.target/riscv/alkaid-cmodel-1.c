/* { dg-do compile { target { rv64 } } } */
/* { dg-options "-mcpu=alkaid -mabi=lp64" } */

#if !defined(__riscv_cmodel_medany)
#error "__riscv_cmodel_medany"
#endif

#if defined(__riscv_cmodel_medlow)
#error "__riscv_cmodel_medlow"
#endif

int foo (void) { return 0; }
