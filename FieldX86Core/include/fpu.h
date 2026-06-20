/****************************************************************************
*
* Field RTX x86 Core (libx86emu lineage, in-tree for Field Die host CPU)
* Not for separate distribution. Dual licensed with AMOURANTHRTX (GPL v3 / commercial).
*
* Description:
*   x87 FPU emulation.
*
****************************************************************************/

#ifndef __X86EMU_FPU_H
#define __X86EMU_FPU_H

#include "x86emu.h"

void x86emu_fpu_init(void);
void x86emu_fpu_emms(void);

void x86emuOp_esc_coprocess_d8(x86emu_t *emu, u8 op1);
void x86emuOp_esc_coprocess_d9(x86emu_t *emu, u8 op1);
void x86emuOp_esc_coprocess_da(x86emu_t *emu, u8 op1);
void x86emuOp_esc_coprocess_db(x86emu_t *emu, u8 op1);
void x86emuOp_esc_coprocess_dc(x86emu_t *emu, u8 op1);
void x86emuOp_esc_coprocess_dd(x86emu_t *emu, u8 op1);
void x86emuOp_esc_coprocess_de(x86emu_t *emu, u8 op1);
void x86emuOp_esc_coprocess_df(x86emu_t *emu, u8 op1);

#endif /* __X86EMU_FPU_H */