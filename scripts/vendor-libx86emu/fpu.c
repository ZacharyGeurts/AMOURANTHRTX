/****************************************************************************
*
* x87 FPU emulation for libx86emu
*
****************************************************************************/

#include "include/x86emu_int.h"
#include "include/fpu.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* FPU status word condition-code bits */
#define FPU_SW_C0  0x0100
#define FPU_SW_C1  0x0200
#define FPU_SW_C2  0x0400
#define FPU_SW_C3  0x0800
#define FPU_SW_TOP 0x3800
#define FPU_SW_TOP_SHIFT 11

/* FPU tag word: 2 bits per physical register */
#define FPU_TAG_VALID  0
#define FPU_TAG_ZERO   1
#define FPU_TAG_SPECIAL 2
#define FPU_TAG_EMPTY  3

typedef struct {
  double st[8];
  u16 control;
  u16 status;
  u16 tag;
  int top;
} x86emu_fpu_t;

static x86emu_fpu_t fpu;

static void fpu_sync_top(void)
{
  fpu.status &= ~FPU_SW_TOP;
  fpu.status |= (fpu.top & 7) << FPU_SW_TOP_SHIFT;
}

static int fpu_phys(int i)
{
  return (fpu.top + i) & 7;
}

static int fpu_tag_get(int phys)
{
  return (fpu.tag >> (phys * 2)) & 3;
}

static void fpu_tag_set(int phys, int val)
{
  fpu.tag &= ~(3u << (phys * 2));
  fpu.tag |= (val & 3) << (phys * 2);
}

static int fpu_is_empty(int i)
{
  return fpu_tag_get(fpu_phys(i)) == FPU_TAG_EMPTY;
}

static double fpu_get(int i)
{
  return fpu.st[fpu_phys(i)];
}

static void fpu_set(int i, double v)
{
  int p = fpu_phys(i);
  fpu.st[p] = v;
  if(v == 0.0)
    fpu_tag_set(p, FPU_TAG_ZERO);
  else if(!isfinite(v))
    fpu_tag_set(p, FPU_TAG_SPECIAL);
  else
    fpu_tag_set(p, FPU_TAG_VALID);
}

static void fpu_push(double v)
{
  fpu.top = (fpu.top - 1) & 7;
  fpu_set(0, v);
  fpu_sync_top();
}

static void fpu_pop(void)
{
  fpu_tag_set(fpu_phys(0), FPU_TAG_EMPTY);
  fpu.top = (fpu.top + 1) & 7;
  fpu_sync_top();
}

static void fpu_free(int i)
{
  fpu_tag_set(fpu_phys(i), FPU_TAG_EMPTY);
}

static void fpu_exchange(int i)
{
  int p0 = fpu_phys(0);
  int pi = fpu_phys(i);
  double t = fpu.st[p0];
  int tg0 = fpu_tag_get(p0);
  int tgi = fpu_tag_get(pi);
  fpu.st[p0] = fpu.st[pi];
  fpu.st[pi] = t;
  fpu_tag_set(p0, tgi);
  fpu_tag_set(pi, tg0);
}

static u32 fpu_load_m32(x86emu_t *emu, u32 addr)
{
  return fetch_data_long(emu, addr);
}

static u64 fpu_load_m64(x86emu_t *emu, u32 addr)
{
  return (u64) fetch_data_long(emu, addr) |
    ((u64) fetch_data_long(emu, addr + 4) << 32);
}

static double fpu_m32_to_d(u32 v)
{
  union { u32 u; float f; } u;
  u.u = v;
  return (double) u.f;
}

static double fpu_m64_to_d(u64 v)
{
  union { u64 u; double d; } u;
  u.u = v;
  return u.d;
}

static u32 fpu_d_to_m32(double v)
{
  union { u32 u; float f; } u;
  u.f = (float) v;
  return u.u;
}

static u64 fpu_d_to_m64(double v)
{
  union { u64 u; double d; } u;
  u.d = v;
  return u.u;
}

static double fpu_load_m80(x86emu_t *emu, u32 addr)
{
  u64 mant;
  u16 expw;
  int sign, exp;
  double v;

  mant = (u64) fetch_data_long(emu, addr);
  mant |= (u64) fetch_data_long(emu, addr + 4) << 32;
  expw = fetch_data_word(emu, addr + 8);
  sign = (expw >> 15) & 1;
  exp = (expw & 0x7fff);

  if(exp == 0 && mant == 0)
    return sign ? -0.0 : 0.0;
  if(exp == 0x7fff) {
    if(mant == (u64)1 << 63)
      return sign ? -INFINITY : INFINITY;
    return NAN;
  }

  /* implicit integer bit */
  mant |= (u64)1 << 63;
  v = ldexp((double) mant / (double) ((u64)1 << 63), exp - 16383);
  return sign ? -v : v;
}

static void fpu_store_m80(x86emu_t *emu, u32 addr, double v)
{
  u16 expw = 0;
  u64 mant = 0;

  if(v == 0.0) {
    if(signbit(v))
      expw = 0x8000;
  }
  else if(isinf(v)) {
    expw = 0x7fff | (signbit(v) ? 0x8000 : 0);
    mant = (u64)1 << 63;
  }
  else if(isnan(v)) {
    expw = 0x7fff;
    mant = (u64)1 << 63;
  }
  else {
    int exp;
    double frac = frexp(v, &exp);
    if(signbit(v))
      expw = 0x8000;
    expw |= (u16) (exp + 16383);
    mant = (u64) (frac * (double) ((u64)1 << 63));
  }

  store_data_long(emu, addr, (u32) mant);
  store_data_long(emu, addr + 4, (u32) (mant >> 32));
  store_data_word(emu, addr + 8, expw);
}

static s16 fpu_load_m16(x86emu_t *emu, u32 addr)
{
  return (s16) fetch_data_word(emu, addr);
}

static s32 fpu_load_m32int(x86emu_t *emu, u32 addr)
{
  return (s32) fetch_data_long(emu, addr);
}

static void fpu_store_m16(x86emu_t *emu, u32 addr, s16 v)
{
  store_data_word(emu, addr, (u16) v);
}

static void fpu_store_m32int(x86emu_t *emu, u32 addr, s32 v)
{
  store_data_long(emu, addr, (u32) v);
}

static void fpu_compare(double a, double b)
{
  fpu.status &= ~(FPU_SW_C0 | FPU_SW_C2 | FPU_SW_C3);

  if(isnan(a) || isnan(b)) {
    fpu.status |= FPU_SW_C0 | FPU_SW_C3;
    return;
  }
  if(a > b) {
    /* C0=0 C3=0 */
  }
  else if(a < b) {
    fpu.status |= FPU_SW_C0;
  }
  else {
    fpu.status |= FPU_SW_C3;
  }
}

static void fpu_arith_bin(int op, double a, double b, double *res)
{
  switch(op) {
    case 0: *res = a + b; break;
    case 1: *res = a * b; break;
    case 4: *res = a - b; break;
    case 5: *res = b - a; break;
    case 6: *res = a / b; break;
    case 7: *res = b / a; break;
    default: *res = a; break;
  }
}

static void fpu_check(x86emu_t *emu)
{
  if(emu->x86.R_CR0 & CR0_EM)
    INTR_RAISE_UD(emu);
}

static u32 fpu_addr(x86emu_t *emu, int mod, int rm)
{
  return decode_rm_address(emu, mod, rm);
}

void x86emu_fpu_init(void)
{
  memset(&fpu, 0, sizeof fpu);
  fpu.control = 0x037f;
  fpu.tag = 0xffff;
  fpu.top = 0;
  fpu_sync_top();
}

void x86emu_fpu_emms(void)
{
  fpu.tag = 0xffff;
}

static void fpu_esc_mem(x86emu_t *emu, u8 esc, int mod, int reg, int rm)
{
  u32 addr;
  double a, b, r;
  s32 ival;

  if(mod == 3)
    return;

  addr = fpu_addr(emu, mod, rm);

  switch(esc) {
    case 0xD8: /* m32real */
      b = fpu_m32_to_d(fpu_load_m32(emu, addr));
      if(reg <= 1 || reg == 4 || reg == 5 || reg == 6 || reg == 7) {
        a = fpu_get(0);
        fpu_arith_bin(reg, a, b, &r);
        fpu_set(0, r);
      }
      else if(reg == 2) {
        fpu_compare(fpu_get(0), b);
      }
      else if(reg == 3) {
        fpu_compare(fpu_get(0), b);
        fpu_pop();
      }
      break;

    case 0xD9:
      switch(reg) {
        case 0:
          fpu_push(fpu_m32_to_d(fpu_load_m32(emu, addr)));
          break;
        case 1:
          store_data_long(emu, addr, fpu_d_to_m32(fpu_get(0)));
          break;
        case 2:
          store_data_long(emu, addr, fpu_d_to_m32(fpu_get(0)));
          fpu_pop();
          break;
        case 4:
          fpu.control = fetch_data_word(emu, addr);
          break;
        case 7:
          store_data_word(emu, addr, fpu.control);
          break;
      }
      break;

    case 0xDA: /* m16int */
      b = (double) fpu_load_m16(emu, addr);
      if(reg <= 1 || reg == 4 || reg == 5 || reg == 6 || reg == 7) {
        a = fpu_get(0);
        fpu_arith_bin(reg, a, b, &r);
        fpu_set(0, r);
      }
      else if(reg == 2) {
        fpu_compare(fpu_get(0), b);
      }
      else if(reg == 3) {
        fpu_compare(fpu_get(0), b);
        fpu_pop();
      }
      break;

    case 0xDB:
      switch(reg) {
        case 0:
          fpu_push((double) fpu_load_m16(emu, addr));
          break;
        case 2:
          ival = (s32) nearbyint(fpu_get(0));
          fpu_store_m16(emu, addr, (s16) ival);
          break;
        case 3:
          ival = (s32) nearbyint(fpu_get(0));
          fpu_store_m16(emu, addr, (s16) ival);
          fpu_pop();
          break;
        case 4:
          fpu_push(fpu_load_m80(emu, addr));
          break;
        case 5:
          fpu_store_m80(emu, addr, fpu_get(0));
          fpu_pop();
          break;
      }
      break;

    case 0xDC: /* m64real */
      b = fpu_m64_to_d(fpu_load_m64(emu, addr));
      if(reg <= 1 || reg == 4 || reg == 5 || reg == 6 || reg == 7) {
        a = fpu_get(0);
        fpu_arith_bin(reg, a, b, &r);
        fpu_set(0, r);
      }
      else if(reg == 2) {
        fpu_compare(fpu_get(0), b);
      }
      else if(reg == 3) {
        fpu_compare(fpu_get(0), b);
        fpu_pop();
      }
      break;

    case 0xDD:
      switch(reg) {
        case 0:
          fpu_push(fpu_m64_to_d(fpu_load_m64(emu, addr)));
          break;
        case 2:
          store_data_long(emu, addr, (u32) fpu_d_to_m64(fpu_get(0)));
          store_data_long(emu, addr + 4, (u32) (fpu_d_to_m64(fpu_get(0)) >> 32));
          break;
        case 3:
          store_data_long(emu, addr, (u32) fpu_d_to_m64(fpu_get(0)));
          store_data_long(emu, addr + 4, (u32) (fpu_d_to_m64(fpu_get(0)) >> 32));
          fpu_pop();
          break;
        case 7:
          store_data_word(emu, addr, fpu.status);
          break;
      }
      break;

    case 0xDE: /* m16int */
      b = (double) fpu_load_m16(emu, addr);
      if(reg <= 1 || reg == 4 || reg == 5 || reg == 6 || reg == 7) {
        a = fpu_get(0);
        fpu_arith_bin(reg, a, b, &r);
        fpu_set(0, r);
      }
      else if(reg == 2) {
        fpu_compare(fpu_get(0), b);
      }
      else if(reg == 3) {
        fpu_compare(fpu_get(0), b);
        fpu_pop();
      }
      break;

    case 0xDF:
      switch(reg) {
        case 0:
          fpu_push((double) fpu_load_m16(emu, addr));
          break;
        case 2:
          ival = (s32) nearbyint(fpu_get(0));
          fpu_store_m16(emu, addr, (s16) ival);
          break;
        case 3:
          ival = (s32) nearbyint(fpu_get(0));
          fpu_store_m16(emu, addr, (s16) ival);
          fpu_pop();
          break;
        case 5:
          fpu_push((double) fpu_load_m32int(emu, addr));
          break;
        case 7:
          ival = (s32) nearbyint(fpu_get(0));
          fpu_store_m32int(emu, addr, ival);
          fpu_pop();
          break;
      }
      break;
  }
}

static void fpu_esc_d8_reg(x86emu_t *emu, int reg, int rm)
{
  double a, b, r;

  a = fpu_get(0);
  b = fpu_get(rm);
  if(reg <= 1 || reg == 4 || reg == 5 || reg == 6 || reg == 7) {
    fpu_arith_bin(reg, a, b, &r);
    fpu_set(0, r);
  }
  else if(reg == 2) {
    fpu_compare(a, b);
  }
  else if(reg == 3) {
    fpu_compare(a, b);
    fpu_pop();
  }
}

static void fpu_esc_dc_reg(x86emu_t *emu, int reg, int rm)
{
  double a, b, r;

  a = fpu_get(rm);
  b = fpu_get(0);
  /* ST(i), ST — reverse operand order from D8 */
  if(reg == 0 || reg == 1) {
    fpu_arith_bin(reg, a, b, &r);
    fpu_set(rm, r);
  }
  else if(reg == 4) {
    fpu_arith_bin(5, a, b, &r);
    fpu_set(rm, r);
  }
  else if(reg == 5) {
    fpu_arith_bin(4, a, b, &r);
    fpu_set(rm, r);
  }
  else if(reg == 6) {
    fpu_arith_bin(7, a, b, &r);
    fpu_set(rm, r);
  }
  else if(reg == 7) {
    fpu_arith_bin(6, a, b, &r);
    fpu_set(rm, r);
  }
  else if(reg == 2) {
    fpu_compare(a, b);
  }
  else if(reg == 3) {
    fpu_compare(a, b);
    fpu_pop();
  }
}

static void fpu_esc_d9_reg(x86emu_t *emu, int reg, int rm)
{
  int exp;
  double v, frac;

  switch(reg) {
    case 0:
      fpu_push(fpu_get(rm));
      break;
    case 1:
      fpu_exchange(rm);
      break;
    case 2:
      /* FNOP */
      break;
    case 4:
      switch(rm) {
        case 0: fpu_set(0, -fpu_get(0)); break;          /* FCHS */
        case 1: fpu_set(0, fabs(fpu_get(0))); break;     /* FABS */
        case 3: fpu_compare(fpu_get(0), 0.0); break;     /* FTST */
        case 4: /* FXAM */ break;
        default: break;
      }
      break;
    case 5: {
      static const double constants[] = {
        1.0, log10(2.0), log2(M_E), M_PI, log10(M_E), log(2.0), 0.0
      };
      if(rm <= 6)
        fpu_set(0, constants[rm]);
      break;
    }
    case 6:
      switch(rm) {
        case 0: fpu_set(0, fmod(fpu_get(0), fpu_get(1))); break; /* FPREM */
        case 1: fpu_set(0, fpu_get(0) * log1p(fpu_get(1))); break; /* FYL2XP1 */
        case 2: { /* FPTAN */
          v = tan(fpu_get(0));
          fpu_set(0, v);
          fpu_push(1.0);
          break;
        }
        case 3: { /* FPATAN */
          v = atan2(fpu_get(1), fpu_get(0));
          fpu_pop();
          fpu_set(0, v);
          break;
        }
        case 4: { /* FXTRACT */
          frac = frexp(fpu_get(0), &exp);
          fpu_set(0, (double) exp);
          fpu_push(frac);
          break;
        }
        case 5: fpu_set(0, fmod(fpu_get(0), fpu_get(1))); break; /* FPREM1 */
        case 6: fpu.top = (fpu.top - 1) & 7; fpu_sync_top(); break; /* FDECSTP */
        case 7: fpu.top = (fpu.top + 1) & 7; fpu_sync_top(); break; /* FINCSTP */
      }
      break;
    case 7:
      switch(rm) {
        case 0: fpu_set(0, fmod(fpu_get(0), fpu_get(1))); break; /* FPREM */
        case 1: fpu_set(0, fpu_get(0) * log1p(fpu_get(1))); break; /* FYL2XP1 */
        case 2: fpu_set(0, sqrt(fpu_get(0))); break;              /* FSQRT */
        case 3: { /* FSINCOS */
          v = sin(fpu_get(0));
          fpu_set(0, cos(fpu_get(0)));
          fpu_push(v);
          break;
        }
        case 4: fpu_set(0, nearbyint(fpu_get(0))); break;         /* FRNDINT */
        case 5: fpu_set(0, fpu_get(0) * pow(2.0, trunc(fpu_get(1)))); break; /* FSCALE */
        case 6: fpu_set(0, sin(fpu_get(0))); break;               /* FSIN */
        case 7: fpu_set(0, cos(fpu_get(0))); break;               /* FCOS */
      }
      break;
  }
}

static void fpu_esc_da_reg(x86emu_t *emu, int reg, int rm)
{
  (void) emu;
  if(reg == 5) {
    /* FUCOMIPP */
    fpu_compare(fpu_get(0), fpu_get(rm));
    fpu_pop();
    fpu_pop();
  }
}

static void fpu_esc_db_reg(x86emu_t *emu, int reg, int rm)
{
  (void) emu;
  if(reg == 0 || reg == 1 || reg == 2 || reg == 3) {
    /* FCMOVcc: approximate as unconditional for Watcom DOOM */
    fpu_set(0, fpu_get(rm));
  }
  else if(reg == 4) {
    switch(rm) {
      case 2: fpu.status &= ~0x7f00; break; /* FNCLEX */
      case 3: x86emu_fpu_init(); break;   /* FNINIT */
      default: break;
    }
  }
  else if(reg == 5) {
    /* FUCOMI ST, ST(i) */
    fpu_compare(fpu_get(0), fpu_get(rm));
  }
  else if(reg == 6) {
    /* FCOMI ST, ST(i) */
    fpu_compare(fpu_get(0), fpu_get(rm));
  }
}

static void fpu_esc_dd_reg(x86emu_t *emu, int reg, int rm)
{
  switch(reg) {
    case 0:
      fpu_free(rm);
      break;
    case 1:
      fpu_exchange(rm);
      break;
    case 2:
      /* FST ST(i) */
      break;
    case 3:
      fpu_pop();
      break;
    case 4:
      fpu_compare(fpu_get(0), fpu_get(rm));
      break;
    case 5:
      fpu_compare(fpu_get(0), fpu_get(rm));
      fpu_pop();
      break;
    case 7:
      /* FNSTSW AX alias */
      emu->x86.R_AX = (emu->x86.R_AX & 0xffff0000u) | fpu.status;
      break;
  }
}

static void fpu_esc_de_reg(x86emu_t *emu, int reg, int rm)
{
  double a, b, r;

  if(reg <= 1) {
    a = fpu_get(rm);
    b = fpu_get(0);
    fpu_arith_bin(reg, a, b, &r);
    fpu_set(rm, r);
    fpu_pop();
  }
  else if(reg == 2) {
    fpu_compare(fpu_get(0), fpu_get(rm));
    fpu_pop();
  }
  else if(reg == 3 && rm == 1) {
    /* FCOMPP */
    fpu_compare(fpu_get(0), fpu_get(1));
    fpu_pop();
    fpu_pop();
  }
  else if(reg == 4) {
    a = fpu_get(rm);
    b = fpu_get(0);
    fpu_arith_bin(5, a, b, &r);
    fpu_set(rm, r);
    fpu_pop();
  }
  else if(reg == 5) {
    a = fpu_get(rm);
    b = fpu_get(0);
    fpu_arith_bin(4, a, b, &r);
    fpu_set(rm, r);
    fpu_pop();
  }
  else if(reg == 6) {
    a = fpu_get(rm);
    b = fpu_get(0);
    fpu_arith_bin(7, a, b, &r);
    fpu_set(rm, r);
    fpu_pop();
  }
  else if(reg == 7) {
    a = fpu_get(rm);
    b = fpu_get(0);
    fpu_arith_bin(6, a, b, &r);
    fpu_set(rm, r);
    fpu_pop();
  }
}

static void fpu_esc_df_reg(x86emu_t *emu, int reg, int rm)
{
  if(reg == 4 && rm == 0) {
    emu->x86.R_AX = (emu->x86.R_AX & 0xffff0000u) | fpu.status;
  }
  else if(reg == 3) {
    fpu_pop();
  }
}

static void fpu_dispatch(x86emu_t *emu, u8 esc)
{
  int mod, reg, rm;

  fpu_check(emu);
  fetch_decode_modrm(emu, &mod, &reg, &rm);

  if(mod != 3) {
    fpu_esc_mem(emu, esc, mod, reg, rm);
    return;
  }

  switch(esc) {
    case 0xD8: fpu_esc_d8_reg(emu, reg, rm); break;
    case 0xD9:
      if(reg == 3 && rm <= 7) {
        /* FSTP ST(i) alias */
        fpu_pop();
      }
      else {
        fpu_esc_d9_reg(emu, reg, rm);
      }
      break;
    case 0xDA: fpu_esc_da_reg(emu, reg, rm); break;
    case 0xDB: fpu_esc_db_reg(emu, reg, rm); break;
    case 0xDC: fpu_esc_dc_reg(emu, reg, rm); break;
    case 0xDD: fpu_esc_dd_reg(emu, reg, rm); break;
    case 0xDE: fpu_esc_de_reg(emu, reg, rm); break;
    case 0xDF: fpu_esc_df_reg(emu, reg, rm); break;
  }
}

void x86emuOp_esc_coprocess_d8(x86emu_t *emu, u8 op1)
{
  OP_DECODE("fpu d8 ");
  fpu_dispatch(emu, op1);
}

void x86emuOp_esc_coprocess_d9(x86emu_t *emu, u8 op1)
{
  OP_DECODE("fpu d9 ");
  fpu_dispatch(emu, op1);
}

void x86emuOp_esc_coprocess_da(x86emu_t *emu, u8 op1)
{
  OP_DECODE("fpu da ");
  fpu_dispatch(emu, op1);
}

void x86emuOp_esc_coprocess_db(x86emu_t *emu, u8 op1)
{
  OP_DECODE("fpu db ");
  fpu_dispatch(emu, op1);
}

void x86emuOp_esc_coprocess_dc(x86emu_t *emu, u8 op1)
{
  OP_DECODE("fpu dc ");
  fpu_dispatch(emu, op1);
}

void x86emuOp_esc_coprocess_dd(x86emu_t *emu, u8 op1)
{
  OP_DECODE("fpu dd ");
  fpu_dispatch(emu, op1);
}

void x86emuOp_esc_coprocess_de(x86emu_t *emu, u8 op1)
{
  OP_DECODE("fpu de ");
  fpu_dispatch(emu, op1);
}

void x86emuOp_esc_coprocess_df(x86emu_t *emu, u8 op1)
{
  OP_DECODE("fpu df ");
  fpu_dispatch(emu, op1);
}