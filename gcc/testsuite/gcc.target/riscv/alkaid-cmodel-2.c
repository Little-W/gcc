/* { dg-do compile { target { rv64 } } } */
/* { dg-options "-march=rv64im -mabi=lp64 -mtune=alkaid -mcmodel=medlow" } */

#if !defined(__riscv_cmodel_medlow)
#error "__riscv_cmodel_medlow"
#endif

#if defined(__riscv_cmodel_medany)
#error "__riscv_cmodel_medany"
#endif

int foo (void) { return 0; }
