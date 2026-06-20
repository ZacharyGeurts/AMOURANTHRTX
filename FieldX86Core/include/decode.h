/****************************************************************************
*
* Field RTX x86 Core (libx86emu lineage, in-tree for Field Die host CPU)
* Not for separate distribution. Dual licensed with AMOURANTHRTX (GPL v3 / commercial).
*
* Description:
*   Header file for instruction decoding logic.
*
****************************************************************************/

#ifndef __X86EMU_DECODE_H
#define __X86EMU_DECODE_H

/*---------------------- Macros and type definitions ----------------------*/

/* Instruction Decoding */

#define OP_DECODE(a) \
  memcpy((emu)->x86.disasm_ptr, a, sizeof a - 1), \
  (emu)->x86.disasm_ptr += sizeof a - 1

#define SEGPREF_DECODE \
  memcpy((emu)->x86.disasm_ptr, (emu)->x86.decode_seg, 4), \
  (emu)->x86.disasm_ptr += (emu)->x86.default_seg ? 4 : 1

#define DECODE_HEX1(ofs) decode_hex1((emu), &(emu)->x86.disasm_ptr, ofs)
#define DECODE_HEX2(ofs) decode_hex2((emu), &(emu)->x86.disasm_ptr, ofs)
#define DECODE_HEX4(ofs) decode_hex4((emu), &(emu)->x86.disasm_ptr, ofs)
#define DECODE_HEX8(ofs) decode_hex8((emu), &(emu)->x86.disasm_ptr, ofs)
#define DECODE_HEX2S(ofs) decode_hex2s((emu), &(emu)->x86.disasm_ptr, ofs)
#define DECODE_HEX4S(ofs) decode_hex4s((emu), &(emu)->x86.disasm_ptr, ofs)
#define DECODE_HEX8S(ofs) decode_hex8s((emu), &(emu)->x86.disasm_ptr, ofs)
#define DECODE_HEX_ADDR(ofs) decode_hex_addr((emu), &(emu)->x86.disasm_ptr, ofs)

/*-------------------------- Function Prototypes --------------------------*/

#ifdef  __cplusplus
extern "C" {            			/* Use "C" linkage when in C++ mode */
#endif

void fetch_decode_modrm(x86emu_t *emu, int *mod, int *regh, int *regl);
u8 fetch_byte(x86emu_t *emu);
u16 fetch_word(x86emu_t *emu);
u32 fetch_long(x86emu_t *emu);
u8 fetch_data_byte(x86emu_t *emu, u32 offset);
u8 fetch_data_byte_abs(x86emu_t *emu, sel_t *seg, u32 offset);
u16 fetch_data_word(x86emu_t *emu, u32 offset);
u16 fetch_data_word_abs(x86emu_t *emu, sel_t *seg, u32 offset);
u32 fetch_data_long(x86emu_t *emu, u32 offset);
u32 fetch_data_long_abs(x86emu_t *emu, sel_t *seg, u32 offset);
I128_reg_t fetch_data_qlong(x86emu_t *emu, u32 ofs);
void store_data_byte(x86emu_t *emu, u32 offset, u8 val);
void store_data_byte_abs(x86emu_t *emu, sel_t *seg, u32 offset, u8 val);
void store_data_word(x86emu_t *emu, u32 offset, u16 val);
void store_data_word_abs(x86emu_t *emu, sel_t *seg, u32 offset, u16 val);
void store_data_long(x86emu_t *emu, u32 offset, u32 val);
void store_data_long_abs(x86emu_t *emu, sel_t *seg, u32 offset, u32 val);
void store_data_qlong(x86emu_t *emu, u32 ofs, I128_reg_t val);
u8 fetch_io_byte(x86emu_t *emu, u32 offset);
u16 fetch_io_word(x86emu_t *emu, u32 offset);
u32 fetch_io_long(x86emu_t *emu, u32 offset);
void store_io_byte(x86emu_t *emu, u32 port, u8 val);
void store_io_word(x86emu_t *emu, u32 port, u16 val);
void store_io_long(x86emu_t *emu, u32 port, u32 val);
u8* decode_rm_byte_register(x86emu_t *emu, int reg);
u16* decode_rm_word_register(x86emu_t *emu, int reg);
u32* decode_rm_long_register(x86emu_t *emu, int reg);
sel_t *decode_rm_seg_register(x86emu_t *emu, int reg);
I128_reg_t* decode_rm_sse_register(x86emu_t *emu, int reg);
u32 decode_rm00_address(x86emu_t *emu, int rm);
u32 decode_rm01_address(x86emu_t *emu, int rm);
u32 decode_rm10_address(x86emu_t *emu, int rm);
u32 decode_sib_address(x86emu_t *emu, int sib, int mod);
u32 decode_rm_address(x86emu_t *emu, int mod, int rl);

void decode_hex(x86emu_t *emu, char **p, u32 ofs);
void decode_hex1(x86emu_t *emu, char **p, u32 ofs);
void decode_hex2(x86emu_t *emu, char **p, u32 ofs);
void decode_hex4(x86emu_t *emu, char **p, u32 ofs);
void decode_hex8(x86emu_t *emu, char **p, u32 ofs);
void decode_hex32(x86emu_t *emu, char **p, I128_reg_t ofs);
void decode_hex_addr(x86emu_t *emu, char **p, u32 ofs);
void decode_hex2s(x86emu_t *emu, char **p, s32 ofs);
void decode_hex4s(x86emu_t *emu, char **p, s32 ofs);
void decode_hex8s(x86emu_t *emu, char **p, s32 ofs);

void decode_descriptor(x86emu_t *emu, descr_t *d, u32 dl, u32 dh);

unsigned emu_memio(x86emu_t *emu, u32 addr, u32 *val, unsigned type);

void emu_process_debug(x86emu_t *emu, unsigned start, unsigned len);


#ifdef  __cplusplus
}                       			/* End of "C" linkage for C++   	*/
#endif

#endif /* __X86EMU_DECODE_H */
