/****************************************************************************
*
* Field RTX x86 Core (libx86emu lineage, in-tree for Field Die host CPU)
* Not for separate distribution. Dual licensed with AMOURANTHRTX (GPL v3 / commercial).
*
* Description:
*   Header file for operand decoding functions.
*
****************************************************************************/

#ifndef __X86EMU_OPS_H
#define __X86EMU_OPS_H

extern void (*x86emu_optab[0x100])(x86emu_t *emu, u8 op1);
extern void (*x86emu_optab2[0x100])(x86emu_t *emu, u8 op2);

void decode_cond(x86emu_t *emu, int type);


#endif /* __X86EMU_OPS_H */
