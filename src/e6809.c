#include <stdio.h>
#include "e6809.h"
#include "vecx.h"

/* code assumptions:
 *  - it is assumed that an 'int' is at least 16 bits long.
 *  - a 16-bit register has valid bits only in the lower 16 bits and an
 *    8-bit register has valid bits only in the lower 8 bits. the upper
 *    may contain garbage!
 *  - all reading functions are assumed to return the requested data in
 *    the lower bits with the unused upper bits all set to zero.
 */

#define inline

enum {
  FLAG_E    = 0x80,
  FLAG_F    = 0x40,
  FLAG_H    = 0x20,
  FLAG_I    = 0x10,
  FLAG_N    = 0x08,
  FLAG_Z    = 0x04,
  FLAG_V    = 0x02,
  FLAG_C    = 0x01,
  IRQ_NORMAL  = 0,
  IRQ_SYNC  = 1,
  IRQ_CWAI  = 2
};

/* index registers */

static unsigned reg_x;
static unsigned reg_y;

/* user stack pointer */

static unsigned reg_u;

/* hardware stack pointer */

static unsigned reg_s;

/* program counter */

static unsigned reg_pc;

/* accumulators */

static unsigned reg_a;
static unsigned reg_b;

/* direct page register */

static unsigned reg_dp;

/* condition codes */

static unsigned reg_cc;

/* flag to see if interrupts should be handled (sync/cwai). */

static unsigned irq_status;

static unsigned *rptr_xyus[4] = {
  &reg_x,
  &reg_y,
  &reg_u,
  &reg_s
};

/* state */
void 
e6809_dump(e6809_save_t* a_state)
{
  a_state->reg_x      = reg_x;
  a_state->reg_y      = reg_y;
  a_state->reg_u      = reg_u;
  a_state->reg_s      = reg_s;
  a_state->reg_pc     = reg_pc;
  a_state->reg_a      = reg_a;
  a_state->reg_b      = reg_b;
  a_state->reg_dp     = reg_dp;
  a_state->reg_cc     = reg_cc;
  a_state->irq_status = irq_status;
}

void 
e6809_restore(e6809_save_t* a_state)
{
  reg_x     = a_state->reg_x;
  reg_y     = a_state->reg_y;
  reg_u     = a_state->reg_u;
  reg_s     = a_state->reg_s;
  reg_pc    = a_state->reg_pc;
  reg_a     = a_state->reg_a;
  reg_b     = a_state->reg_b;
  reg_dp    = a_state->reg_dp;
  reg_cc    = a_state->reg_cc;
  irq_status= a_state->irq_status;
}

/* user defined read and write functions */

extern unsigned char e6809_read8(unsigned short address);
extern void e6809_write8(unsigned short address, unsigned char data);
extern unsigned e6809_read16 (unsigned short address);

/* obtain a particular condition code. returns 0 or 1. */

# define get_cc(F)  ((reg_cc & (F)) != 0)

/* set a particular condition code to either 0 or 1.
 * value parameter must be either 0 or 1.
 */

static void set_cc (unsigned flag, unsigned value)
{
  reg_cc &= ~flag;
  reg_cc |= value * flag;
}

/* test carry */

static unsigned test_c (unsigned i0, unsigned i1,
                unsigned r, unsigned sub)
{
  unsigned flag;

  flag  = (i0 | i1) & ~r; /* one of the inputs is 1 and output is 0 */
  flag |= (i0 & i1);      /* both inputs are 1 */
  flag  = (flag >> 7) & 1;
  flag ^= sub; /* on a sub, carry is opposite the carry of an add */

  return flag;
}

/* test negative */

static unsigned test_n (unsigned r)
{
  return (r >> 7) & 1;
}

/* test for zero in lower 8 bits */

static unsigned test_z8 (unsigned r)
{
  unsigned flag;

  flag = ~r;
  flag = (flag >> 4) & (flag & 0xf);
  flag = (flag >> 2) & (flag & 0x3);
  flag = (flag >> 1) & (flag & 0x1);

  return flag;
}

/* test for zero in lower 16 bits */

static unsigned test_z16 (unsigned r)
{
  unsigned flag;

  flag = ~r;
  flag = (flag >> 8) & (flag & 0xff);
  flag = (flag >> 4) & (flag & 0xf);
  flag = (flag >> 2) & (flag & 0x3);
  flag = (flag >> 1) & (flag & 0x1);

  return flag;
}

/* overflow is set whenever the sign bits of the inputs are the same
 * but the sign bit of the result is not same as the sign bits of the
 * inputs.
 */

static unsigned test_v (unsigned i0, unsigned i1, unsigned r)
{
  unsigned flag;

  flag  = ~(i0 ^ i1); /* input sign bits are the same */
  flag &=  (i0 ^ r);  /* input sign and output sign not same */
  flag  = (flag >> 7) & 1;

  return flag;
}

static unsigned get_reg_d (void)
{
  return (reg_a << 8) | (reg_b & 0xff);
}

static void set_reg_d (unsigned value)
{
  reg_a = value >> 8;
  reg_b = value;
}

/* read a byte ... the returned value has the lower 8-bits set to the byte
 * while the upper bits are all zero.
 */

static unsigned char read8 (unsigned short address)
{
  return e6809_read8(address);
}

/* write a byte ... only the lower 8-bits of the unsigned data
 * is written. the upper bits are ignored.
 */

static void write8 (unsigned short address, unsigned char data)
{
  e6809_write8(address, data);
}

static unsigned read16 (unsigned short address)
{
# if 1
  return e6809_read16(address);
# else
  unsigned datahi, datalo;
  datahi = e6809_read8(address++);
  datalo = e6809_read8(address  );
  return (datahi << 8)|datalo;
# endif
}

static void write16 (unsigned short address, unsigned data)
{
  write8 (address, data >> 8);
  write8 (address + 1, data);
}

static void push8 (unsigned *sp, unsigned data)
{
  (*sp)--;
  write8 (*sp, data);
}

static unsigned pull8 (unsigned *sp)
{
  unsigned data;

  data = read8 (*sp);
  (*sp)++;

  return data;
}

static void push16 (unsigned *sp, unsigned data)
{
  push8 (sp, data);
  push8 (sp, data >> 8);
}

static unsigned pull16 (unsigned *sp)
{
  unsigned datahi, datalo;

  datahi = pull8 (sp);
  datalo = pull8 (sp);

  return (datahi << 8) | datalo;
}

/* read a byte from the address pointed to by the pc */

static unsigned pc_read8 (void)
{
  unsigned data;

  data = read8 (reg_pc);
  reg_pc++;

  return data;
}

/* read a word from the address pointed to by the pc */

static unsigned pc_read16 (void)
{
  unsigned data;

  data = read16 (reg_pc);
  reg_pc += 2;

  return data;
}

/* sign extend an 8-bit quantity into a 16-bit quantity */

static unsigned sign_extend (unsigned data)
{
  return (~(data & 0x80) + 1) | (data & 0xff);
}

/* direct addressing, upper byte of the address comes from
 * the direct page register, and the lower byte comes from the
 * instruction itself.
 */

static unsigned ea_direct (void)
{
  return (reg_dp << 8) | pc_read8 ();
}

/* extended addressing, address is obtained from 2 bytes following
 * the instruction.
 */

static unsigned ea_extended (void)
{
  return pc_read16 ();
}

/* indexed addressing */

static unsigned ea_indexed (unsigned *cycles)
{
  unsigned r, op, ea = 0;

  /* post byte */

  op = pc_read8 ();

  r = (op >> 5) & 3;

  switch (op) {
  case 0x00: case 0x01: case 0x02: case 0x03:
  case 0x04: case 0x05: case 0x06: case 0x07:
  case 0x08: case 0x09: case 0x0a: case 0x0b:
  case 0x0c: case 0x0d: case 0x0e: case 0x0f:
  case 0x20: case 0x21: case 0x22: case 0x23:
  case 0x24: case 0x25: case 0x26: case 0x27:
  case 0x28: case 0x29: case 0x2a: case 0x2b:
  case 0x2c: case 0x2d: case 0x2e: case 0x2f:
  case 0x40: case 0x41: case 0x42: case 0x43:
  case 0x44: case 0x45: case 0x46: case 0x47:
  case 0x48: case 0x49: case 0x4a: case 0x4b:
  case 0x4c: case 0x4d: case 0x4e: case 0x4f:
  case 0x60: case 0x61: case 0x62: case 0x63:
  case 0x64: case 0x65: case 0x66: case 0x67:
  case 0x68: case 0x69: case 0x6a: case 0x6b:
  case 0x6c: case 0x6d: case 0x6e: case 0x6f:
    /* R, +[0, 15] */

    ea = *rptr_xyus[r] + (op & 0xf);
    (*cycles)++;
    break;
  case 0x10: case 0x11: case 0x12: case 0x13:
  case 0x14: case 0x15: case 0x16: case 0x17:
  case 0x18: case 0x19: case 0x1a: case 0x1b:
  case 0x1c: case 0x1d: case 0x1e: case 0x1f:
  case 0x30: case 0x31: case 0x32: case 0x33:
  case 0x34: case 0x35: case 0x36: case 0x37:
  case 0x38: case 0x39: case 0x3a: case 0x3b:
  case 0x3c: case 0x3d: case 0x3e: case 0x3f:
  case 0x50: case 0x51: case 0x52: case 0x53:
  case 0x54: case 0x55: case 0x56: case 0x57:
  case 0x58: case 0x59: case 0x5a: case 0x5b:
  case 0x5c: case 0x5d: case 0x5e: case 0x5f:
  case 0x70: case 0x71: case 0x72: case 0x73:
  case 0x74: case 0x75: case 0x76: case 0x77:
  case 0x78: case 0x79: case 0x7a: case 0x7b:
  case 0x7c: case 0x7d: case 0x7e: case 0x7f:
    /* R, +[-16, -1] */

    ea = *rptr_xyus[r] + (op & 0xf) - 0x10;
    (*cycles)++;
    break;
  case 0x80: case 0x81:
  case 0xa0: case 0xa1:
  case 0xc0: case 0xc1:
  case 0xe0: case 0xe1:
    /* ,R+ / ,R++ */

    ea = *rptr_xyus[r];
    *rptr_xyus[r] += 1 + (op & 1);
    *cycles += 2 + (op & 1);
    break;
  case 0x90: case 0x91:
  case 0xb0: case 0xb1:
  case 0xd0: case 0xd1:
  case 0xf0: case 0xf1:
    /* [,R+] ??? / [,R++] */

    ea = read16 (*rptr_xyus[r]);
    *rptr_xyus[r] += 1 + (op & 1);
    *cycles += 5 + (op & 1);
    break;
  case 0x82: case 0x83:
  case 0xa2: case 0xa3:
  case 0xc2: case 0xc3:
  case 0xe2: case 0xe3:

    /* ,-R / ,--R */

    *rptr_xyus[r] -= 1 + (op & 1);
    ea = *rptr_xyus[r];
    *cycles += 2 + (op & 1);
    break;
  case 0x92: case 0x93:
  case 0xb2: case 0xb3:
  case 0xd2: case 0xd3:
  case 0xf2: case 0xf3:
    /* [,-R] ??? / [,--R] */

    *rptr_xyus[r] -= 1 + (op & 1);
    ea = read16 (*rptr_xyus[r]);
    *cycles += 5 + (op & 1);
    break;
  case 0x84: case 0xa4:
  case 0xc4: case 0xe4:
    /* ,R */

    ea = *rptr_xyus[r];
    break;
  case 0x94: case 0xb4:
  case 0xd4: case 0xf4:
    /* [,R] */

    ea = read16 (*rptr_xyus[r]);
    *cycles += 3;
    break;
  case 0x85: case 0xa5:
  case 0xc5: case 0xe5:
    /* B,R */

    ea = *rptr_xyus[r] + sign_extend (reg_b);
    *cycles += 1;
    break;
  case 0x95: case 0xb5:
  case 0xd5: case 0xf5:
    /* [B,R] */

    ea = read16 (*rptr_xyus[r] + sign_extend (reg_b));
    *cycles += 4;
    break;
  case 0x86: case 0xa6:
  case 0xc6: case 0xe6:
    /* A,R */

    ea = *rptr_xyus[r] + sign_extend (reg_a);
    *cycles += 1;
    break;
  case 0x96: case 0xb6:
  case 0xd6: case 0xf6:
    /* [A,R] */

    ea = read16 (*rptr_xyus[r] + sign_extend (reg_a));
    *cycles += 4;
    break;
  case 0x88: case 0xa8:
  case 0xc8: case 0xe8:
    /* byte,R */

    ea = *rptr_xyus[r] + sign_extend (pc_read8 ());
    *cycles += 1;
    break;
  case 0x98: case 0xb8:
  case 0xd8: case 0xf8:
    /* [byte,R] */

    ea = read16 (*rptr_xyus[r] + sign_extend (pc_read8 ()));
    *cycles += 4;
    break;
  case 0x89: case 0xa9:
  case 0xc9: case 0xe9:
    /* word,R */

    ea = *rptr_xyus[r] + pc_read16 ();
    *cycles += 4;
    break;
  case 0x99: case 0xb9:
  case 0xd9: case 0xf9:
    /* [word,R] */

    ea = read16 (*rptr_xyus[r] + pc_read16 ());
    *cycles += 7;
    break;
  case 0x8b: case 0xab:
  case 0xcb: case 0xeb:
    /* D,R */

    ea = *rptr_xyus[r] + get_reg_d ();
    *cycles += 4;
    break;
  case 0x9b: case 0xbb:
  case 0xdb: case 0xfb:
    /* [D,R] */

    ea = read16 (*rptr_xyus[r] + get_reg_d ());
    *cycles += 7;
    break;
  case 0x8c: case 0xac:
  case 0xcc: case 0xec:
    /* byte, PC */

    r = sign_extend (pc_read8 ());
    ea = reg_pc + r;
    *cycles += 1;
    break;
  case 0x9c: case 0xbc:
  case 0xdc: case 0xfc:
    /* [byte, PC] */

    r = sign_extend (pc_read8 ());
    ea = read16 (reg_pc + r);
    *cycles += 4;
    break;
  case 0x8d: case 0xad:
  case 0xcd: case 0xed:
    /* word, PC */

    r = pc_read16 ();
    ea = reg_pc + r;
    *cycles += 5;
    break;
  case 0x9d: case 0xbd:
  case 0xdd: case 0xfd:
    /* [word, PC] */

    r = pc_read16 ();
    ea = read16 (reg_pc + r);
    *cycles += 8;
    break;
  case 0x9f:
    /* [address] */

    ea = read16 (pc_read16 ());
    *cycles += 5;
    break;
  default:
    printf ("undefined post-byte\n");
    break;
  }

  return ea;
}

/* instruction: neg
 * essentially (0 - data).
 */

unsigned inst_neg (unsigned data)
{
  unsigned i0, i1, r;

  i0 = 0;
  i1 = ~data;
  r = i0 + i1 + 1;

  set_cc (FLAG_H, test_c (i0 << 4, i1 << 4, r << 4, 0));
  set_cc (FLAG_N, test_n (r));
  set_cc (FLAG_Z, test_z8 (r));
  set_cc (FLAG_V, test_v (i0, i1, r));
  set_cc (FLAG_C, test_c (i0, i1, r, 1));

  return r;
}

/* instruction: com */

unsigned inst_com (unsigned data)
{
  unsigned r;

  r = ~data;
  
  set_cc (FLAG_N, test_n (r));
  set_cc (FLAG_Z, test_z8 (r));
  set_cc (FLAG_V, 0);
  set_cc (FLAG_C, 1);

  return r;
}

/* instruction: lsr
 * cannot be faked as an add or substract.
 */

unsigned inst_lsr (unsigned data)
{
  unsigned r;

  r = (data >> 1) & 0x7f;
  
  set_cc (FLAG_N, 0);
  set_cc (FLAG_Z, test_z8 (r));
  set_cc (FLAG_C, data & 1);

  return r;
}

/* instruction: ror
 * cannot be faked as an add or substract.
 */

unsigned inst_ror (unsigned data)
{
  unsigned r, c;

  c = get_cc (FLAG_C);
  r = ((data >> 1) & 0x7f) | (c << 7);
  
  set_cc (FLAG_N, test_n (r));
  set_cc (FLAG_Z, test_z8 (r));
  set_cc (FLAG_C, data & 1);

  return r;
}

/* instruction: asr
 * cannot be faked as an add or substract.
 */

unsigned inst_asr (unsigned data)
{
  unsigned r;

  r = ((data >> 1) & 0x7f) | (data & 0x80);
  
  set_cc (FLAG_N, test_n (r));
  set_cc (FLAG_Z, test_z8 (r));
  set_cc (FLAG_C, data & 1);

  return r;
}

/* instruction: asl
 * essentially (data + data). simple addition.
 */

unsigned inst_asl (unsigned data)
{
  unsigned i0, i1, r;

  i0 = data;
  i1 = data;
  r = i0 + i1;
  
  set_cc (FLAG_H, test_c (i0 << 4, i1 << 4, r << 4, 0));
  set_cc (FLAG_N, test_n (r));
  set_cc (FLAG_Z, test_z8 (r));
  set_cc (FLAG_V, test_v (i0, i1, r));
  set_cc (FLAG_C, test_c (i0, i1, r, 0));

  return r;
}

/* instruction: rol
 * essentially (data + data + carry). addition with carry.
 */

unsigned inst_rol (unsigned data)
{
  unsigned i0, i1, c, r;

  i0 = data;
  i1 = data;
  c = get_cc (FLAG_C);
  r = i0 + i1 + c;
  
  set_cc (FLAG_N, test_n (r));
  set_cc (FLAG_Z, test_z8 (r));
  set_cc (FLAG_V, test_v (i0, i1, r));
  set_cc (FLAG_C, test_c (i0, i1, r, 0));

  return r;
}

/* instruction: dec
 * essentially (data - 1).
 */

unsigned inst_dec (unsigned data)
{
  unsigned i0, i1, r;

  i0 = data;
  i1 = 0xff;
  r = i0 + i1;
  
  set_cc (FLAG_N, test_n (r));
  set_cc (FLAG_Z, test_z8 (r));
  set_cc (FLAG_V, test_v (i0, i1, r));

  return r;
}

/* instruction: inc
 * essentially (data + 1).
 */

unsigned inst_inc (unsigned data)
{
  unsigned i0, i1, r;

  i0 = data;
  i1 = 1;
  r = i0 + i1;
  
  set_cc (FLAG_N, test_n (r));
  set_cc (FLAG_Z, test_z8 (r));
  set_cc (FLAG_V, test_v (i0, i1, r));

  return r;
}

/* instruction: tst */

void inst_tst8 (unsigned data)
{
  set_cc (FLAG_N, test_n (data));
  set_cc (FLAG_Z, test_z8 (data));
  set_cc (FLAG_V, 0);
}

void inst_tst16 (unsigned data)
{
  set_cc (FLAG_N, test_n (data >> 8));
  set_cc (FLAG_Z, test_z16 (data));
  set_cc (FLAG_V, 0);
}

/* instruction: clr */

void inst_clr (void)
{
  set_cc (FLAG_N, 0);
  set_cc (FLAG_Z, 1);
  set_cc (FLAG_V, 0);
  set_cc (FLAG_C, 0);
}

/* instruction: suba/subb */

unsigned inst_sub8 (unsigned data0, unsigned data1)
{
  unsigned i0, i1, r;

  i0 = data0;
  i1 = ~data1;
  r = i0 + i1 + 1;
  
  set_cc (FLAG_H, test_c (i0 << 4, i1 << 4, r << 4, 0));
  set_cc (FLAG_N, test_n (r));
  set_cc (FLAG_Z, test_z8 (r));
  set_cc (FLAG_V, test_v (i0, i1, r));
  set_cc (FLAG_C, test_c (i0, i1, r, 1));

  return r;
}

/* instruction: sbca/sbcb/cmpa/cmpb.
 * only 8-bit version, 16-bit version not needed.
 */

unsigned inst_sbc (unsigned data0, unsigned data1)
{
  unsigned i0, i1, c, r;

  i0 = data0;
  i1 = ~data1;
  c = 1 - get_cc (FLAG_C);
  r = i0 + i1 + c;

  set_cc (FLAG_H, test_c (i0 << 4, i1 << 4, r << 4, 0));
  set_cc (FLAG_N, test_n (r));
  set_cc (FLAG_Z, test_z8 (r));
  set_cc (FLAG_V, test_v (i0, i1, r));
  set_cc (FLAG_C, test_c (i0, i1, r, 1));

  return r;
}

/* instruction: anda/andb/bita/bitb.
 * only 8-bit version, 16-bit version not needed.
 */

unsigned inst_and (unsigned data0, unsigned data1)
{
  unsigned r;

  r = data0 & data1;

  inst_tst8 (r);

  return r;
}

/* instruction: eora/eorb.
 * only 8-bit version, 16-bit version not needed.
 */

unsigned inst_eor (unsigned data0, unsigned data1)
{
  unsigned r;

  r = data0 ^ data1;

  inst_tst8 (r);

  return r;
}

/* instruction: adca/adcb
 * only 8-bit version, 16-bit version not needed.
 */

unsigned inst_adc (unsigned data0, unsigned data1)
{
  unsigned i0, i1, c, r;

  i0 = data0;
  i1 = data1;
  c = get_cc (FLAG_C);
  r = i0 + i1 + c;

  set_cc (FLAG_H, test_c (i0 << 4, i1 << 4, r << 4, 0));
  set_cc (FLAG_N, test_n (r));
  set_cc (FLAG_Z, test_z8 (r));
  set_cc (FLAG_V, test_v (i0, i1, r));
  set_cc (FLAG_C, test_c (i0, i1, r, 0));

  return r;
}

/* instruction: ora/orb.
 * only 8-bit version, 16-bit version not needed.
 */

unsigned inst_or (unsigned data0, unsigned data1)
{
  unsigned r;

  r = data0 | data1;

  inst_tst8 (r);

  return r;
}

/* instruction: adda/addb */

unsigned inst_add8 (unsigned data0, unsigned data1)
{
  unsigned i0, i1, r;

  i0 = data0;
  i1 = data1;
  r = i0 + i1;

  set_cc (FLAG_H, test_c (i0 << 4, i1 << 4, r << 4, 0));
  set_cc (FLAG_N, test_n (r));
  set_cc (FLAG_Z, test_z8 (r));
  set_cc (FLAG_V, test_v (i0, i1, r));
  set_cc (FLAG_C, test_c (i0, i1, r, 0));

  return r;
}

/* instruction: addd */

unsigned inst_add16 (unsigned data0, unsigned data1)
{
  unsigned i0, i1, r;

  i0 = data0;
  i1 = data1;
  r = i0 + i1;

  set_cc (FLAG_N, test_n (r >> 8));
  set_cc (FLAG_Z, test_z16 (r));
  set_cc (FLAG_V, test_v (i0 >> 8, i1 >> 8, r >> 8));
  set_cc (FLAG_C, test_c (i0 >> 8, i1 >> 8, r >> 8, 0));

  return r;
}

/* instruction: subd */

unsigned inst_sub16 (unsigned data0, unsigned data1)
{
  unsigned i0, i1, r;

  i0 = data0;
  i1 = ~data1;
  r = i0 + i1 + 1;

  set_cc (FLAG_N, test_n (r >> 8));
  set_cc (FLAG_Z, test_z16 (r));
  set_cc (FLAG_V, test_v (i0 >> 8, i1 >> 8, r >> 8));
  set_cc (FLAG_C, test_c (i0 >> 8, i1 >> 8, r >> 8, 1));

  return r;
}

/* instruction: 8-bit offset branch */

void inst_bra8 (unsigned test, unsigned op, unsigned *cycles)
{
  unsigned offset, mask;

  offset = pc_read8 ();

  /* trying to avoid an if statement */

  mask = (test ^ (op & 1)) - 1; /* 0xffff when taken, 0 when not taken */
  reg_pc += sign_extend (offset) & mask;

  *cycles += 3;
}

/* instruction: 16-bit offset branch */

void inst_bra16 (unsigned test, unsigned op, unsigned *cycles)
{
  unsigned offset, mask;

  offset = pc_read16 ();

  /* trying to avoid an if statement */

  mask = (test ^ (op & 1)) - 1; /* 0xffff when taken, 0 when not taken */
  reg_pc += offset & mask;

  *cycles += 5 - mask;
}

/* instruction: pshs/pshu */

void inst_psh (unsigned op, unsigned *sp,
             unsigned data, unsigned *cycles)
{
  if (op & 0x80) {
    push16 (sp, reg_pc);
    *cycles += 2;
  }

  if (op & 0x40) {
    /* either s or u */
    push16 (sp, data);
    *cycles += 2;
  }

  if (op & 0x20) {
    push16 (sp, reg_y);
    *cycles += 2;
  }

  if (op & 0x10) {
    push16 (sp, reg_x);
    *cycles += 2;
  }

  if (op & 0x08) {
    push8 (sp, reg_dp);
    *cycles += 1;
  }

  if (op & 0x04) {
    push8 (sp, reg_b);
    *cycles += 1;
  }

  if (op & 0x02) {
    push8 (sp, reg_a);
    *cycles += 1;
  }

  if (op & 0x01) {
    push8 (sp, reg_cc);
    *cycles += 1;
  }
}

/* instruction: puls/pulu */

void inst_pul (unsigned op, unsigned *sp, unsigned *osp,
             unsigned *cycles)
{
  if (op & 0x01) {
    reg_cc = pull8 (sp);
    *cycles += 1;
  }

  if (op & 0x02) {
    reg_a = pull8 (sp);
    *cycles += 1;
  }

  if (op & 0x04) {
    reg_b = pull8 (sp);
    *cycles += 1;
  }

  if (op & 0x08) {
    reg_dp = pull8 (sp);
    *cycles += 1;
  }

  if (op & 0x10) {
    reg_x = pull16 (sp);
    *cycles += 2;
  }

  if (op & 0x20) {
    reg_y = pull16 (sp);
    *cycles += 2;
  }

  if (op & 0x40) {
    /* either s or u */
    *osp = pull16 (sp);
    *cycles += 2;
  }

  if (op & 0x80) {
    reg_pc = pull16 (sp);
    *cycles += 2;
  }
}

unsigned exgtfr_read (unsigned reg)
{
  unsigned data;

  switch (reg) {
  case 0x0:
    data = get_reg_d ();
    break;
  case 0x1:
    data = reg_x;
    break;
  case 0x2:
    data = reg_y;
    break;
  case 0x3:
    data = reg_u;
    break;
  case 0x4:
    data = reg_s;
    break;
  case 0x5:
    data = reg_pc;
    break;
  case 0x8:
    data = 0xff00 | reg_a;
    break;
  case 0x9:
    data = 0xff00 | reg_b;
    break;
  case 0xa:
    data = 0xff00 | reg_cc;
    break;
  case 0xb:
    data = 0xff00 | reg_dp;
    break;
  default:
    data = 0xffff;
    printf ("illegal exgtfr reg %.1x\n", reg);
    break;
  }

  return data;
}

void exgtfr_write (unsigned reg, unsigned data)
{
  switch (reg) {
  case 0x0:
    set_reg_d (data);
    break;
  case 0x1:
    reg_x = data;
    break;
  case 0x2:
    reg_y = data;
    break;
  case 0x3:
    reg_u = data;
    break;
  case 0x4:
    reg_s = data;
    break;
  case 0x5:
    reg_pc = data;
    break;
  case 0x8:
    reg_a = data;
    break;
  case 0x9:
    reg_b = data;
    break;
  case 0xa:
    reg_cc = data;
    break;
  case 0xb:
    reg_dp = data;
    break;
  default:
    printf ("illegal exgtfr reg %.1x\n", reg);
    break;
  }
}

/* instruction: exg */

void inst_exg (void)
{
  unsigned op, tmp;

  op = pc_read8 ();

  tmp = exgtfr_read (op & 0xf);
  exgtfr_write (op & 0xf, exgtfr_read (op >> 4));
  exgtfr_write (op >> 4, tmp);
}

/* instruction: tfr */

void inst_tfr (void)
{
  unsigned op;

  op = pc_read8 ();

  exgtfr_write (op & 0xf, exgtfr_read (op >> 4));
}

/* reset the 6809 */

void e6809_reset (void)
{
  reg_x = 0;
  reg_y = 0;
  reg_u = 0;
  reg_s = 0;

  reg_a = 0;
  reg_b = 0;

  reg_dp = 0;

  reg_cc = FLAG_I | FLAG_F;
  irq_status = IRQ_NORMAL;

  reg_pc = read16 (0xfffe);
}

/* execute a single instruction or handle interrupts and return */

static unsigned 
e6809_irq_i(unsigned irq_i)
{
  unsigned cycles = 0;
  if (irq_i) {
    if (get_cc (FLAG_I) == 0) {
      if (irq_status != IRQ_CWAI) {
        set_cc (FLAG_E, 1);
        inst_psh (0xff, &reg_s, reg_u, &cycles);
      }

      set_cc (FLAG_I, 1);

      reg_pc = read16 (0xfff8);
      irq_status = IRQ_NORMAL;
      cycles += 7;
    } else {
      if (irq_status == IRQ_SYNC) {
        irq_status = IRQ_NORMAL;
      }
    }
  }

  if (irq_status != IRQ_NORMAL) {
    cycles++;
  }
  return cycles;
}

static 
unsigned e6809_op10()
{
  __label__ 
l_0x00, l_0x01, l_0x02, l_0x03, l_0x04, l_0x05, l_0x06, l_0x07, l_0x08, l_0x09,
l_0x0a, l_0x0b, l_0x0c, l_0x0d, l_0x0e, l_0x0f, l_0x10, l_0x11, l_0x12, l_0x13,
l_0x14, l_0x15, l_0x16, l_0x17, l_0x18, l_0x19, l_0x1a, l_0x1b, l_0x1c, l_0x1d,
l_0x1e, l_0x1f, l_0x20, l_0x21, l_0x22, l_0x23, l_0x24, l_0x25, l_0x26, l_0x27,
l_0x28, l_0x29, l_0x2a, l_0x2b, l_0x2c, l_0x2d, l_0x2e, l_0x2f, l_0x30, l_0x31,
l_0x32, l_0x33, l_0x34, l_0x35, l_0x36, l_0x37, l_0x38, l_0x39, l_0x3a, l_0x3b,
l_0x3c, l_0x3d, l_0x3e, l_0x3f, l_0x40, l_0x41, l_0x42, l_0x43, l_0x44, l_0x45,
l_0x46, l_0x47, l_0x48, l_0x49, l_0x4a, l_0x4b, l_0x4c, l_0x4d, l_0x4e, l_0x4f,
l_0x50, l_0x51, l_0x52, l_0x53, l_0x54, l_0x55, l_0x56, l_0x57, l_0x58, l_0x59,
l_0x5a, l_0x5b, l_0x5c, l_0x5d, l_0x5e, l_0x5f, l_0x60, l_0x61, l_0x62, l_0x63,
l_0x64, l_0x65, l_0x66, l_0x67, l_0x68, l_0x69, l_0x6a, l_0x6b, l_0x6c, l_0x6d,
l_0x6e, l_0x6f, l_0x70, l_0x71, l_0x72, l_0x73, l_0x74, l_0x75, l_0x76, l_0x77,
l_0x78, l_0x79, l_0x7a, l_0x7b, l_0x7c, l_0x7d, l_0x7e, l_0x7f, l_0x80, l_0x81,
l_0x82, l_0x83, l_0x84, l_0x85, l_0x86, l_0x87, l_0x88, l_0x89, l_0x8a, l_0x8b,
l_0x8c, l_0x8d, l_0x8e, l_0x8f, l_0x90, l_0x91, l_0x92, l_0x93, l_0x94, l_0x95,
l_0x96, l_0x97, l_0x98, l_0x99, l_0x9a, l_0x9b, l_0x9c, l_0x9d, l_0x9e, l_0x9f,
l_0xa0, l_0xa1, l_0xa2, l_0xa3, l_0xa4, l_0xa5, l_0xa6, l_0xa7, l_0xa8, l_0xa9,
l_0xaa, l_0xab, l_0xac, l_0xad, l_0xae, l_0xaf, l_0xb0, l_0xb1, l_0xb2, l_0xb3,
l_0xb4, l_0xb5, l_0xb6, l_0xb7, l_0xb8, l_0xb9, l_0xba, l_0xbb, l_0xbc, l_0xbd,
l_0xbe, l_0xbf, l_0xc0, l_0xc1, l_0xc2, l_0xc3, l_0xc4, l_0xc5, l_0xc6, l_0xc7,
l_0xc8, l_0xc9, l_0xca, l_0xcb, l_0xcc, l_0xcd, l_0xce, l_0xcf, l_0xd0, l_0xd1,
l_0xd2, l_0xd3, l_0xd4, l_0xd5, l_0xd6, l_0xd7, l_0xd8, l_0xd9, l_0xda, l_0xdb,
l_0xdc, l_0xdd, l_0xde, l_0xdf, l_0xe0, l_0xe1, l_0xe2, l_0xe3, l_0xe4, l_0xe5,
l_0xe6, l_0xe7, l_0xe8, l_0xe9, l_0xea, l_0xeb, l_0xec, l_0xed, l_0xee, l_0xef,
l_0xf0, l_0xf1, l_0xf2, l_0xf3, l_0xf4, l_0xf5, l_0xf6, l_0xf7, l_0xf8, l_0xf9,
l_0xfa, l_0xfb, l_0xfc, l_0xfd, l_0xfe, l_0xff;

  static const void* const a_jump_table[256] = {
&&l_0x00, &&l_0x01, &&l_0x02, &&l_0x03, &&l_0x04, &&l_0x05, &&l_0x06, &&l_0x07,
&&l_0x08, &&l_0x09, &&l_0x0a, &&l_0x0b, &&l_0x0c, &&l_0x0d, &&l_0x0e, &&l_0x0f,
&&l_0x10, &&l_0x11, &&l_0x12, &&l_0x13, &&l_0x14, &&l_0x15, &&l_0x16, &&l_0x17,
&&l_0x18, &&l_0x19, &&l_0x1a, &&l_0x1b, &&l_0x1c, &&l_0x1d, &&l_0x1e, &&l_0x1f,
&&l_0x20, &&l_0x21, &&l_0x22, &&l_0x23, &&l_0x24, &&l_0x25, &&l_0x26, &&l_0x27,
&&l_0x28, &&l_0x29, &&l_0x2a, &&l_0x2b, &&l_0x2c, &&l_0x2d, &&l_0x2e, &&l_0x2f,
&&l_0x30, &&l_0x31, &&l_0x32, &&l_0x33, &&l_0x34, &&l_0x35, &&l_0x36, &&l_0x37,
&&l_0x38, &&l_0x39, &&l_0x3a, &&l_0x3b, &&l_0x3c, &&l_0x3d, &&l_0x3e, &&l_0x3f,
&&l_0x40, &&l_0x41, &&l_0x42, &&l_0x43, &&l_0x44, &&l_0x45, &&l_0x46, &&l_0x47,
&&l_0x48, &&l_0x49, &&l_0x4a, &&l_0x4b, &&l_0x4c, &&l_0x4d, &&l_0x4e, &&l_0x4f,
&&l_0x50, &&l_0x51, &&l_0x52, &&l_0x53, &&l_0x54, &&l_0x55, &&l_0x56, &&l_0x57,
&&l_0x58, &&l_0x59, &&l_0x5a, &&l_0x5b, &&l_0x5c, &&l_0x5d, &&l_0x5e, &&l_0x5f,
&&l_0x60, &&l_0x61, &&l_0x62, &&l_0x63, &&l_0x64, &&l_0x65, &&l_0x66, &&l_0x67,
&&l_0x68, &&l_0x69, &&l_0x6a, &&l_0x6b, &&l_0x6c, &&l_0x6d, &&l_0x6e, &&l_0x6f,
&&l_0x70, &&l_0x71, &&l_0x72, &&l_0x73, &&l_0x74, &&l_0x75, &&l_0x76, &&l_0x77,
&&l_0x78, &&l_0x79, &&l_0x7a, &&l_0x7b, &&l_0x7c, &&l_0x7d, &&l_0x7e, &&l_0x7f,
&&l_0x80, &&l_0x81, &&l_0x82, &&l_0x83, &&l_0x84, &&l_0x85, &&l_0x86, &&l_0x87,
&&l_0x88, &&l_0x89, &&l_0x8a, &&l_0x8b, &&l_0x8c, &&l_0x8d, &&l_0x8e, &&l_0x8f,
&&l_0x90, &&l_0x91, &&l_0x92, &&l_0x93, &&l_0x94, &&l_0x95, &&l_0x96, &&l_0x97,
&&l_0x98, &&l_0x99, &&l_0x9a, &&l_0x9b, &&l_0x9c, &&l_0x9d, &&l_0x9e, &&l_0x9f,
&&l_0xa0, &&l_0xa1, &&l_0xa2, &&l_0xa3, &&l_0xa4, &&l_0xa5, &&l_0xa6, &&l_0xa7,
&&l_0xa8, &&l_0xa9, &&l_0xaa, &&l_0xab, &&l_0xac, &&l_0xad, &&l_0xae, &&l_0xaf,
&&l_0xb0, &&l_0xb1, &&l_0xb2, &&l_0xb3, &&l_0xb4, &&l_0xb5, &&l_0xb6, &&l_0xb7,
&&l_0xb8, &&l_0xb9, &&l_0xba, &&l_0xbb, &&l_0xbc, &&l_0xbd, &&l_0xbe, &&l_0xbf,
&&l_0xc0, &&l_0xc1, &&l_0xc2, &&l_0xc3, &&l_0xc4, &&l_0xc5, &&l_0xc6, &&l_0xc7,
&&l_0xc8, &&l_0xc9, &&l_0xca, &&l_0xcb, &&l_0xcc, &&l_0xcd, &&l_0xce, &&l_0xcf,
&&l_0xd0, &&l_0xd1, &&l_0xd2, &&l_0xd3, &&l_0xd4, &&l_0xd5, &&l_0xd6, &&l_0xd7,
&&l_0xd8, &&l_0xd9, &&l_0xda, &&l_0xdb, &&l_0xdc, &&l_0xdd, &&l_0xde, &&l_0xdf,
&&l_0xe0, &&l_0xe1, &&l_0xe2, &&l_0xe3, &&l_0xe4, &&l_0xe5, &&l_0xe6, &&l_0xe7,
&&l_0xe8, &&l_0xe9, &&l_0xea, &&l_0xeb, &&l_0xec, &&l_0xed, &&l_0xee, &&l_0xef,
&&l_0xf0, &&l_0xf1, &&l_0xf2, &&l_0xf3, &&l_0xf4, &&l_0xf5, &&l_0xf6, &&l_0xf7,
&&l_0xf8, &&l_0xf9, &&l_0xfa, &&l_0xfb, &&l_0xfc, &&l_0xfd, &&l_0xfe, &&l_0xff
};

  unsigned ea;
  unsigned cycles = 0;
  unsigned char op = pc_read8 ();
  goto *a_jump_table[op];

  //switch (op) {
    /* lbra */
    l_0x20:
    /* lbrn */
    l_0x21:
      inst_bra16 (0, op, &cycles);
    return cycles;
    /* lbhi */
    l_0x22:
    /* lbls */
    l_0x23:
      inst_bra16 (get_cc (FLAG_C) | get_cc (FLAG_Z), op, &cycles);
    return cycles;
    /* lbhs/lbcc */
    l_0x24:
    /* lblo/lbcs */
    l_0x25:
      inst_bra16 (get_cc (FLAG_C), op, &cycles);
    return cycles;
    /* lbne */
    l_0x26:
    /* lbeq */
    l_0x27:
      inst_bra16 (get_cc (FLAG_Z), op, &cycles);
    return cycles;
    /* lbvc */
    l_0x28:
    /* lbvs */
    l_0x29:
      inst_bra16 (get_cc (FLAG_V), op, &cycles);
    return cycles;
    /* lbpl */
    l_0x2a:
    /* lbmi */
    l_0x2b:
      inst_bra16 (get_cc (FLAG_N), op, &cycles);
    return cycles;
    /* lbge */
    l_0x2c:
    /* lblt */
    l_0x2d:
      inst_bra16 (get_cc (FLAG_N) ^ get_cc (FLAG_V), op, &cycles);
    return cycles;
    /* lbgt */
    l_0x2e:
    /* lble */
    l_0x2f:
      inst_bra16 (get_cc (FLAG_Z) |
            (get_cc (FLAG_N) ^ get_cc (FLAG_V)), op, &cycles);
    return cycles;
    /* cmpd */
    l_0x83:
      inst_sub16 (get_reg_d (), pc_read16 ());
      cycles += 5;
    return cycles;
    l_0x93:
      ea = ea_direct ();
      inst_sub16 (get_reg_d (), read16 (ea));
      cycles += 7;
    return cycles;
    l_0xa3:
      ea = ea_indexed (&cycles);
      inst_sub16 (get_reg_d (), read16 (ea));
      cycles += 7;
    return cycles;
    l_0xb3:
      ea = ea_extended ();
      inst_sub16 (get_reg_d (), read16 (ea));
      cycles += 8;
    return cycles;
    /* cmpy */
    l_0x8c:
      inst_sub16 (reg_y, pc_read16 ());
      cycles += 5;
    return cycles;
    l_0x9c:
      ea = ea_direct ();
      inst_sub16 (reg_y, read16 (ea));
      cycles += 7;
    return cycles;
    l_0xac:
      ea = ea_indexed (&cycles);
      inst_sub16 (reg_y, read16 (ea));
      cycles += 7;
    return cycles;
    l_0xbc:
      ea = ea_extended ();
      inst_sub16 (reg_y, read16 (ea));
      cycles += 8;
    return cycles;
    /* ldy */
    l_0x8e:
      reg_y = pc_read16 ();
      inst_tst16 (reg_y);
      cycles += 4;
    return cycles;
    l_0x9e: 
      ea = ea_direct ();
      reg_y = read16 (ea);
      inst_tst16 (reg_y);
      cycles += 6;
    return cycles;
    l_0xae:
      ea = ea_indexed (&cycles);
      reg_y = read16 (ea);
      inst_tst16 (reg_y);
      cycles += 6;
    return cycles;
    l_0xbe:
      ea = ea_extended ();
      reg_y = read16 (ea);
      inst_tst16 (reg_y);
      cycles += 7;
    return cycles;
    /* sty */
    l_0x9f:
      ea = ea_direct ();
      write16 (ea, reg_y);
      inst_tst16 (reg_y);
      cycles += 6;
    return cycles;
    l_0xaf:
      ea = ea_indexed (&cycles);
      write16 (ea, reg_y);
      inst_tst16 (reg_y);
      cycles += 6;
    return cycles;
    l_0xbf:
      ea = ea_extended ();
      write16 (ea, reg_y);
      inst_tst16 (reg_y);
      cycles += 7;
    return cycles;
    /* lds */
    l_0xce:
      reg_s = pc_read16 ();
      inst_tst16 (reg_s);
      cycles += 4;
    return cycles;
    l_0xde: 
      ea = ea_direct ();
      reg_s = read16 (ea);
      inst_tst16 (reg_s);
      cycles += 6;
    return cycles;
    l_0xee:
      ea = ea_indexed (&cycles);
      reg_s = read16 (ea);
      inst_tst16 (reg_s);
      cycles += 6;
    return cycles;
    l_0xfe:
      ea = ea_extended ();
      reg_s = read16 (ea);
      inst_tst16 (reg_s);
      cycles += 7;
    return cycles;
    /* sts */
    l_0xdf:
      ea = ea_direct ();
      write16 (ea, reg_s);
      inst_tst16 (reg_s);
      cycles += 6;
    return cycles;
    l_0xef:
      ea = ea_indexed (&cycles);
      write16 (ea, reg_s);
      inst_tst16 (reg_s);
      cycles += 6;
    return cycles;
    l_0xff:
      ea = ea_extended ();
      write16 (ea, reg_s);
      inst_tst16 (reg_s);
      cycles += 7;
    return cycles;
    /* swi2 */
    l_0x3f:
      set_cc (FLAG_E, 1);
      inst_psh (0xff, &reg_s, reg_u, &cycles);
        reg_pc = read16 (0xfff4);
      cycles += 8;
    return cycles;
    l_0xfd:
    l_0xfc:
    l_0xfb:
    l_0xfa:
    l_0xf9:
    l_0xf8:
    l_0xf7:
    l_0xf6:
    l_0xf5:
    l_0xf4:
    l_0xf3:
    l_0xf2:
    l_0xf1:
    l_0xf0:
    l_0xed:
    l_0xec:
    l_0xeb:
    l_0xea:
    l_0xe9:
    l_0xe8:
    l_0xe7:
    l_0xe6:
    l_0xe5:
    l_0xe4:
    l_0xe3:
    l_0xe2:
    l_0xe1:
    l_0xe0:
    l_0xdd:
    l_0xdc:
    l_0xdb:
    l_0xda:
    l_0xd9:
    l_0xd8:
    l_0xd7:
    l_0xd6:
    l_0xd5:
    l_0xd4:
    l_0xd3:
    l_0xd2:
    l_0xd1:
    l_0xd0:
    l_0xcf:
    l_0xcd:
    l_0xcc:
    l_0xcb:
    l_0xca:
    l_0xc9:
    l_0xc8:
    l_0xc7:
    l_0xc6:
    l_0xc5:
    l_0xc4:
    l_0xc3:
    l_0xc2:
    l_0xc1:
    l_0xc0:
    l_0xbd:
    l_0xbb:
    l_0xba:
    l_0xb9:
    l_0xb8:
    l_0xb7:
    l_0xb6:
    l_0xb5:
    l_0xb4:
    l_0xb2:
    l_0xb1:
    l_0xb0:
    l_0xad:
    l_0xab:
    l_0xaa:
    l_0xa9:
    l_0xa8:
    l_0xa7:
    l_0xa6:
    l_0xa5:
    l_0xa4:
    l_0xa2:
    l_0xa1:
    l_0xa0:
    l_0x9d:
    l_0x9b:
    l_0x9a:
    l_0x99:
    l_0x98:
    l_0x97:
    l_0x96:
    l_0x95:
    l_0x94:
    l_0x92:
    l_0x91:
    l_0x90:
    l_0x8f:
    l_0x8d:
    l_0x8b:
    l_0x8a:
    l_0x89:
    l_0x88:
    l_0x87:
    l_0x86:
    l_0x85:
    l_0x84:
    l_0x82:
    l_0x81:
    l_0x80:
    l_0x7f:
    l_0x7e:
    l_0x7d:
    l_0x7c:
    l_0x7b:
    l_0x7a:
    l_0x79:
    l_0x78:
    l_0x77:
    l_0x76:
    l_0x75:
    l_0x74:
    l_0x73:
    l_0x72:
    l_0x71:
    l_0x70:
    l_0x6f:
    l_0x6e:
    l_0x6d:
    l_0x6c:
    l_0x6b:
    l_0x6a:
    l_0x69:
    l_0x68:
    l_0x67:
    l_0x66:
    l_0x65:
    l_0x64:
    l_0x63:
    l_0x62:
    l_0x61:
    l_0x60:
    l_0x5f:
    l_0x5e:
    l_0x5d:
    l_0x5c:
    l_0x5b:
    l_0x5a:
    l_0x59:
    l_0x58:
    l_0x57:
    l_0x56:
    l_0x55:
    l_0x54:
    l_0x53:
    l_0x52:
    l_0x51:
    l_0x50:
    l_0x4f:
    l_0x4e:
    l_0x4d:
    l_0x4c:
    l_0x4b:
    l_0x4a:
    l_0x49:
    l_0x48:
    l_0x47:
    l_0x46:
    l_0x45:
    l_0x44:
    l_0x43:
    l_0x42:
    l_0x41:
    l_0x40:
    l_0x3e:
    l_0x3d:
    l_0x3c:
    l_0x3b:
    l_0x3a:
    l_0x39:
    l_0x38:
    l_0x37:
    l_0x36:
    l_0x35:
    l_0x34:
    l_0x33:
    l_0x32:
    l_0x31:
    l_0x30:
    l_0x1f:
    l_0x1e:
    l_0x1d:
    l_0x1c:
    l_0x1b:
    l_0x1a:
    l_0x19:
    l_0x18:
    l_0x17:
    l_0x16:
    l_0x15:
    l_0x14:
    l_0x13:
    l_0x12:
    l_0x11:
    l_0x10:
    l_0x0f:
    l_0x0e:
    l_0x0d:
    l_0x0c:
    l_0x0b:
    l_0x0a:
    l_0x09:
    l_0x08:
    l_0x07:
    l_0x06:
    l_0x05:
    l_0x04:
    l_0x03:
    l_0x02:
    l_0x01:
    l_0x00:
# if 0
    default:
      printf ("unknown page-1 op code: %.2x\n", op);
    return cycles;
# endif
    //}
  return cycles;
}

#ifdef STATS
long e6809_stats[256];
void
e6809_dump_stats() {
  int i;
  for (i = 0; i < 256; i++) {
    if (e6809_stats[i]) {
      fprintf(stdout, "[%x] = %ld\n", i, e6809_stats[i] );
    }
  }
}
#endif

unsigned e6809_sstep (unsigned irq_i)
{
  __label__ 
l_0x00, l_0x01, l_0x02, l_0x03, l_0x04, l_0x05, l_0x06, l_0x07, l_0x08, l_0x09,
l_0x0a, l_0x0b, l_0x0c, l_0x0d, l_0x0e, l_0x0f, l_0x10, l_0x11, l_0x12, l_0x13,
l_0x14, l_0x15, l_0x16, l_0x17, l_0x18, l_0x19, l_0x1a, l_0x1b, l_0x1c, l_0x1d,
l_0x1e, l_0x1f, l_0x20, l_0x21, l_0x22, l_0x23, l_0x24, l_0x25, l_0x26, l_0x27,
l_0x28, l_0x29, l_0x2a, l_0x2b, l_0x2c, l_0x2d, l_0x2e, l_0x2f, l_0x30, l_0x31,
l_0x32, l_0x33, l_0x34, l_0x35, l_0x36, l_0x37, l_0x38, l_0x39, l_0x3a, l_0x3b,
l_0x3c, l_0x3d, l_0x3e, l_0x3f, l_0x40, l_0x41, l_0x42, l_0x43, l_0x44, l_0x45,
l_0x46, l_0x47, l_0x48, l_0x49, l_0x4a, l_0x4b, l_0x4c, l_0x4d, l_0x4e, l_0x4f,
l_0x50, l_0x51, l_0x52, l_0x53, l_0x54, l_0x55, l_0x56, l_0x57, l_0x58, l_0x59,
l_0x5a, l_0x5b, l_0x5c, l_0x5d, l_0x5e, l_0x5f, l_0x60, l_0x61, l_0x62, l_0x63,
l_0x64, l_0x65, l_0x66, l_0x67, l_0x68, l_0x69, l_0x6a, l_0x6b, l_0x6c, l_0x6d,
l_0x6e, l_0x6f, l_0x70, l_0x71, l_0x72, l_0x73, l_0x74, l_0x75, l_0x76, l_0x77,
l_0x78, l_0x79, l_0x7a, l_0x7b, l_0x7c, l_0x7d, l_0x7e, l_0x7f, l_0x80, l_0x81,
l_0x82, l_0x83, l_0x84, l_0x85, l_0x86, l_0x87, l_0x88, l_0x89, l_0x8a, l_0x8b,
l_0x8c, l_0x8d, l_0x8e, l_0x8f, l_0x90, l_0x91, l_0x92, l_0x93, l_0x94, l_0x95,
l_0x96, l_0x97, l_0x98, l_0x99, l_0x9a, l_0x9b, l_0x9c, l_0x9d, l_0x9e, l_0x9f,
l_0xa0, l_0xa1, l_0xa2, l_0xa3, l_0xa4, l_0xa5, l_0xa6, l_0xa7, l_0xa8, l_0xa9,
l_0xaa, l_0xab, l_0xac, l_0xad, l_0xae, l_0xaf, l_0xb0, l_0xb1, l_0xb2, l_0xb3,
l_0xb4, l_0xb5, l_0xb6, l_0xb7, l_0xb8, l_0xb9, l_0xba, l_0xbb, l_0xbc, l_0xbd,
l_0xbe, l_0xbf, l_0xc0, l_0xc1, l_0xc2, l_0xc3, l_0xc4, l_0xc5, l_0xc6, l_0xc7,
l_0xc8, l_0xc9, l_0xca, l_0xcb, l_0xcc, l_0xcd, l_0xce, l_0xcf, l_0xd0, l_0xd1,
l_0xd2, l_0xd3, l_0xd4, l_0xd5, l_0xd6, l_0xd7, l_0xd8, l_0xd9, l_0xda, l_0xdb,
l_0xdc, l_0xdd, l_0xde, l_0xdf, l_0xe0, l_0xe1, l_0xe2, l_0xe3, l_0xe4, l_0xe5,
l_0xe6, l_0xe7, l_0xe8, l_0xe9, l_0xea, l_0xeb, l_0xec, l_0xed, l_0xee, l_0xef,
l_0xf0, l_0xf1, l_0xf2, l_0xf3, l_0xf4, l_0xf5, l_0xf6, l_0xf7, l_0xf8, l_0xf9,
l_0xfa, l_0xfb, l_0xfc, l_0xfd, l_0xfe, l_0xff;

  static const void* const a_jump_table[256] = {
&&l_0x00, &&l_0x01, &&l_0x02, &&l_0x03, &&l_0x04, &&l_0x05, &&l_0x06, &&l_0x07,
&&l_0x08, &&l_0x09, &&l_0x0a, &&l_0x0b, &&l_0x0c, &&l_0x0d, &&l_0x0e, &&l_0x0f,
&&l_0x10, &&l_0x11, &&l_0x12, &&l_0x13, &&l_0x14, &&l_0x15, &&l_0x16, &&l_0x17,
&&l_0x18, &&l_0x19, &&l_0x1a, &&l_0x1b, &&l_0x1c, &&l_0x1d, &&l_0x1e, &&l_0x1f,
&&l_0x20, &&l_0x21, &&l_0x22, &&l_0x23, &&l_0x24, &&l_0x25, &&l_0x26, &&l_0x27,
&&l_0x28, &&l_0x29, &&l_0x2a, &&l_0x2b, &&l_0x2c, &&l_0x2d, &&l_0x2e, &&l_0x2f,
&&l_0x30, &&l_0x31, &&l_0x32, &&l_0x33, &&l_0x34, &&l_0x35, &&l_0x36, &&l_0x37,
&&l_0x38, &&l_0x39, &&l_0x3a, &&l_0x3b, &&l_0x3c, &&l_0x3d, &&l_0x3e, &&l_0x3f,
&&l_0x40, &&l_0x41, &&l_0x42, &&l_0x43, &&l_0x44, &&l_0x45, &&l_0x46, &&l_0x47,
&&l_0x48, &&l_0x49, &&l_0x4a, &&l_0x4b, &&l_0x4c, &&l_0x4d, &&l_0x4e, &&l_0x4f,
&&l_0x50, &&l_0x51, &&l_0x52, &&l_0x53, &&l_0x54, &&l_0x55, &&l_0x56, &&l_0x57,
&&l_0x58, &&l_0x59, &&l_0x5a, &&l_0x5b, &&l_0x5c, &&l_0x5d, &&l_0x5e, &&l_0x5f,
&&l_0x60, &&l_0x61, &&l_0x62, &&l_0x63, &&l_0x64, &&l_0x65, &&l_0x66, &&l_0x67,
&&l_0x68, &&l_0x69, &&l_0x6a, &&l_0x6b, &&l_0x6c, &&l_0x6d, &&l_0x6e, &&l_0x6f,
&&l_0x70, &&l_0x71, &&l_0x72, &&l_0x73, &&l_0x74, &&l_0x75, &&l_0x76, &&l_0x77,
&&l_0x78, &&l_0x79, &&l_0x7a, &&l_0x7b, &&l_0x7c, &&l_0x7d, &&l_0x7e, &&l_0x7f,
&&l_0x80, &&l_0x81, &&l_0x82, &&l_0x83, &&l_0x84, &&l_0x85, &&l_0x86, &&l_0x87,
&&l_0x88, &&l_0x89, &&l_0x8a, &&l_0x8b, &&l_0x8c, &&l_0x8d, &&l_0x8e, &&l_0x8f,
&&l_0x90, &&l_0x91, &&l_0x92, &&l_0x93, &&l_0x94, &&l_0x95, &&l_0x96, &&l_0x97,
&&l_0x98, &&l_0x99, &&l_0x9a, &&l_0x9b, &&l_0x9c, &&l_0x9d, &&l_0x9e, &&l_0x9f,
&&l_0xa0, &&l_0xa1, &&l_0xa2, &&l_0xa3, &&l_0xa4, &&l_0xa5, &&l_0xa6, &&l_0xa7,
&&l_0xa8, &&l_0xa9, &&l_0xaa, &&l_0xab, &&l_0xac, &&l_0xad, &&l_0xae, &&l_0xaf,
&&l_0xb0, &&l_0xb1, &&l_0xb2, &&l_0xb3, &&l_0xb4, &&l_0xb5, &&l_0xb6, &&l_0xb7,
&&l_0xb8, &&l_0xb9, &&l_0xba, &&l_0xbb, &&l_0xbc, &&l_0xbd, &&l_0xbe, &&l_0xbf,
&&l_0xc0, &&l_0xc1, &&l_0xc2, &&l_0xc3, &&l_0xc4, &&l_0xc5, &&l_0xc6, &&l_0xc7,
&&l_0xc8, &&l_0xc9, &&l_0xca, &&l_0xcb, &&l_0xcc, &&l_0xcd, &&l_0xce, &&l_0xcf,
&&l_0xd0, &&l_0xd1, &&l_0xd2, &&l_0xd3, &&l_0xd4, &&l_0xd5, &&l_0xd6, &&l_0xd7,
&&l_0xd8, &&l_0xd9, &&l_0xda, &&l_0xdb, &&l_0xdc, &&l_0xdd, &&l_0xde, &&l_0xdf,
&&l_0xe0, &&l_0xe1, &&l_0xe2, &&l_0xe3, &&l_0xe4, &&l_0xe5, &&l_0xe6, &&l_0xe7,
&&l_0xe8, &&l_0xe9, &&l_0xea, &&l_0xeb, &&l_0xec, &&l_0xed, &&l_0xee, &&l_0xef,
&&l_0xf0, &&l_0xf1, &&l_0xf2, &&l_0xf3, &&l_0xf4, &&l_0xf5, &&l_0xf6, &&l_0xf7,
&&l_0xf8, &&l_0xf9, &&l_0xfa, &&l_0xfb, &&l_0xfc, &&l_0xfd, &&l_0xfe, &&l_0xff
};

  unsigned char op;
  unsigned ea, i0, i1, r;
  unsigned cycles = 0;

  if (irq_i) { 
   cycles = e6809_irq_i(irq_i);
  }

  op = pc_read8 ();
#ifdef STATS
  e6809_stats[op]++;
#endif
  goto *a_jump_table[op];

  //switch (op) {
  /* page 0 instructions */

  /* bne */
  l_0x26:
  /* beq */
  l_0x27:
    inst_bra8 (get_cc (FLAG_Z), op, &cycles);
  return cycles;
  l_0x95:
    ea = ea_direct ();
    inst_and (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  /* sta */
  l_0x97:
    ea = ea_direct ();
    write8 (ea, reg_a);
    inst_tst8 (reg_a);
    cycles += 4;
  return cycles;
  l_0xa6:
    ea = ea_indexed (&cycles);
    reg_a = read8 (ea);
    inst_tst8 (reg_a);
    cycles += 4;
  return cycles;
  /* neg, nega, negb */
  l_0x00:
    ea = ea_direct ();
    r = inst_neg (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x40:
    reg_a = inst_neg (reg_a);
    cycles += 2;
  return cycles;
  l_0x50:
    reg_b = inst_neg (reg_b);
    cycles += 2;
  return cycles;
  l_0x60:
    ea = ea_indexed (&cycles);
    r = inst_neg (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x70:
    ea = ea_extended ();
    r = inst_neg (read8 (ea));
    write8 (ea, r);
    cycles += 7;
  return cycles;
  /* com, coma, comb */
  l_0x03:
    ea = ea_direct ();
    r = inst_com (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x43:
    reg_a = inst_com (reg_a);
    cycles += 2;
  return cycles;
  l_0x53:
    reg_b = inst_com (reg_b);
    cycles += 2;
  return cycles;
  l_0x63:
    ea = ea_indexed (&cycles);
    r = inst_com (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x73:
    ea = ea_extended ();
    r = inst_com (read8 (ea));
    write8 (ea, r);
    cycles += 7;
  return cycles;
  /* lsr, lsra, lsrb */
  l_0x04:
    ea = ea_direct ();
    r = inst_lsr (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x44:
    reg_a = inst_lsr (reg_a);
    cycles += 2;
  return cycles;
  l_0x54:
    reg_b = inst_lsr (reg_b);
    cycles += 2;
  return cycles;
  l_0x64:
    ea = ea_indexed (&cycles);
    r = inst_lsr (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x74:
    ea = ea_extended ();
    r = inst_lsr (read8 (ea));
    write8 (ea, r);
    cycles += 7;
  return cycles;
  /* ror, rora, rorb */
  l_0x06:
    ea = ea_direct ();
    r = inst_ror (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x46:
    reg_a = inst_ror (reg_a);
    cycles += 2;
  return cycles;
  l_0x56:
    reg_b = inst_ror (reg_b);
    cycles += 2;
  return cycles;
  l_0x66:
    ea = ea_indexed (&cycles);
    r = inst_ror (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x76:
    ea = ea_extended ();
    r = inst_ror (read8 (ea));
    write8 (ea, r);
    cycles += 7;
  return cycles;
  /* asr, asra, asrb */
  l_0x07:
    ea = ea_direct ();
    r = inst_asr (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x47:
    reg_a = inst_asr (reg_a);
    cycles += 2;
  return cycles;
  l_0x57:
    reg_b = inst_asr (reg_b);
    cycles += 2;
  return cycles;
  l_0x67:
    ea = ea_indexed (&cycles);
    r = inst_asr (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x77:
    ea = ea_extended ();
    r = inst_asr (read8 (ea));
    write8 (ea, r);
    cycles += 7;
  return cycles;
  /* asl, asla, aslb */
  l_0x08:
    ea = ea_direct ();
    r = inst_asl (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x48:
    reg_a = inst_asl (reg_a);
    cycles += 2;
  return cycles;
  l_0x58:
    reg_b = inst_asl (reg_b);
    cycles += 2;
  return cycles;
  l_0x68:
    ea = ea_indexed (&cycles);
    r = inst_asl (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x78:
    ea = ea_extended ();
    r = inst_asl (read8 (ea));
    write8 (ea, r);
    cycles += 7;
  return cycles;
  /* rol, rola, rolb */
  l_0x09:
    ea = ea_direct ();
    r = inst_rol (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x49:
    reg_a = inst_rol (reg_a);
    cycles += 2;
  return cycles;
  l_0x59:
    reg_b = inst_rol (reg_b);
    cycles += 2;
  return cycles;
  l_0x69:
    ea = ea_indexed (&cycles);
    r = inst_rol (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x79:
    ea = ea_extended ();
    r = inst_rol (read8 (ea));
    write8 (ea, r);
    cycles += 7;
  return cycles;
  /* dec, deca, decb */
  l_0x0a:
    ea = ea_direct ();
    r = inst_dec (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x4a:
    reg_a = inst_dec (reg_a);
    cycles += 2;
  return cycles;
  l_0x5a:
    reg_b = inst_dec (reg_b);
    cycles += 2;
  return cycles;
  l_0x6a:
    ea = ea_indexed (&cycles);
    r = inst_dec (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x7a:
    ea = ea_extended ();
    r = inst_dec (read8 (ea));
    write8 (ea, r);
    cycles += 7;
  return cycles;
  /* inc, inca, incb */
  l_0x0c:
    ea = ea_direct ();
    r = inst_inc (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x4c:
    reg_a = inst_inc (reg_a);
    cycles += 2;
  return cycles;
  l_0x5c:
    reg_b = inst_inc (reg_b);
    cycles += 2;
  return cycles;
  l_0x6c:
    ea = ea_indexed (&cycles);
    r = inst_inc (read8 (ea));
    write8 (ea, r);
    cycles += 6;
  return cycles;
  l_0x7c:
    ea = ea_extended ();
    r = inst_inc (read8 (ea));
    write8 (ea, r);
    cycles += 7;
  return cycles;
  /* tst, tsta, tstb */
  l_0x0d:
    ea = ea_direct ();
    inst_tst8 (read8 (ea));
    cycles += 6;
  return cycles;
  l_0x4d:
    inst_tst8 (reg_a);
    cycles += 2;
  return cycles;
  l_0x5d:
    inst_tst8 (reg_b);
    cycles += 2;
  return cycles;
  l_0x6d:
    ea = ea_indexed (&cycles);
    inst_tst8 (read8 (ea));
    cycles += 6;
  return cycles;
  l_0x7d:
    ea = ea_extended ();
    inst_tst8 (read8 (ea));
    cycles += 7;
  return cycles;
  /* jmp */
  l_0x0e:
    reg_pc = ea_direct ();
    cycles += 3;
  return cycles;
  l_0x6e:
    reg_pc = ea_indexed (&cycles);
    cycles += 3;
  return cycles;
  l_0x7e:
    reg_pc = ea_extended ();
    cycles += 4;
  return cycles;
  /* clr */
  l_0x0f:
    ea = ea_direct ();
    inst_clr ();
    write8 (ea, 0);
    cycles += 6;
  return cycles;
  l_0x4f:
    inst_clr ();
    reg_a = 0;
    cycles += 2;
  return cycles;
  l_0x5f:
    inst_clr ();
    reg_b = 0;
    cycles += 2;
  return cycles;
  l_0x6f:
    ea = ea_indexed (&cycles);
    inst_clr ();
    write8 (ea, 0);
    cycles += 6;
  return cycles;
  l_0x7f:
    ea = ea_extended ();
    inst_clr ();
    write8 (ea, 0);
    cycles += 7;
  return cycles;
  /* suba */
  l_0x80:
    reg_a = inst_sub8 (reg_a, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0x90:
    ea = ea_direct ();
    reg_a = inst_sub8 (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xa0:
    ea = ea_indexed (&cycles);
    reg_a = inst_sub8 (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xb0:
    ea = ea_extended ();
    reg_a = inst_sub8 (reg_a, read8 (ea));
    cycles += 5;
  return cycles;
  /* subb */
  l_0xc0:
    reg_b = inst_sub8 (reg_b, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0xd0:
    ea = ea_direct ();
    reg_b = inst_sub8 (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xe0:
    ea = ea_indexed (&cycles);
    reg_b = inst_sub8 (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xf0:
    ea = ea_extended ();
    reg_b = inst_sub8 (reg_b, read8 (ea));
    cycles += 5;
  return cycles;
  /* cmpa */
  l_0x81:
    inst_sub8 (reg_a, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0x91:
    ea = ea_direct ();
    inst_sub8 (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xa1:
    ea = ea_indexed (&cycles);
    inst_sub8 (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xb1:
    ea = ea_extended ();
    inst_sub8 (reg_a, read8 (ea));
    cycles += 5;
  return cycles;
  /* cmpb */
  l_0xc1:
    inst_sub8 (reg_b, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0xd1:
    ea = ea_direct ();
    inst_sub8 (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xe1:
    ea = ea_indexed (&cycles);
    inst_sub8 (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xf1:
    ea = ea_extended ();
    inst_sub8 (reg_b, read8 (ea));
    cycles += 5;
  return cycles;
  /* sbca */
  l_0x82:
    reg_a = inst_sbc (reg_a, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0x92:
    ea = ea_direct ();
    reg_a = inst_sbc (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xa2:
    ea = ea_indexed (&cycles);
    reg_a = inst_sbc (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xb2:
    ea = ea_extended ();
    reg_a = inst_sbc (reg_a, read8 (ea));
    cycles += 5;
  return cycles;
  /* sbcb */
  l_0xc2:
    reg_b = inst_sbc (reg_b, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0xd2:
    ea = ea_direct ();
    reg_b = inst_sbc (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xe2:
    ea = ea_indexed (&cycles);
    reg_b = inst_sbc (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xf2:
    ea = ea_extended ();
    reg_b = inst_sbc (reg_b, read8 (ea));
    cycles += 5;
  return cycles;
  /* anda */
  l_0x84:
    reg_a = inst_and (reg_a, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0x94:
    ea = ea_direct ();
    reg_a = inst_and (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xa4:
    ea = ea_indexed (&cycles);
    reg_a = inst_and (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xb4:
    ea = ea_extended ();
    reg_a = inst_and (reg_a, read8 (ea));
    cycles += 5;
  return cycles;
  /* andb */
  l_0xc4:
    reg_b = inst_and (reg_b, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0xd4:
    ea = ea_direct ();
    reg_b = inst_and (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xe4:
    ea = ea_indexed (&cycles);
    reg_b = inst_and (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xf4:
    ea = ea_extended ();
    reg_b = inst_and (reg_b, read8 (ea));
    cycles += 5;
  return cycles;
  /* bita */
  l_0x85:
    inst_and (reg_a, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0xa5:
    ea = ea_indexed (&cycles);
    inst_and (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xb5:
    ea = ea_extended ();
    inst_and (reg_a, read8 (ea));
    cycles += 5;
  return cycles;
  /* bitb */
  l_0xc5:
    inst_and (reg_b, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0xd5:
    ea = ea_direct ();
    inst_and (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xe5:
    ea = ea_indexed (&cycles);
    inst_and (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xf5:
    ea = ea_extended ();
    inst_and (reg_b, read8 (ea));
    cycles += 5;
  return cycles;
  /* lda */
  l_0x86:
    reg_a = pc_read8 ();
    inst_tst8 (reg_a);
    cycles += 2;
  return cycles;
  l_0x96:
    ea = ea_direct ();
    reg_a = read8 (ea);
    inst_tst8 (reg_a);
    cycles += 4;
  return cycles;
  l_0xb6:
    ea = ea_extended ();
    reg_a = read8 (ea);
    inst_tst8 (reg_a);
    cycles += 5;
  return cycles;
  /* ldb */
  l_0xc6:
    reg_b = pc_read8 ();
    inst_tst8 (reg_b);
    cycles += 2;
  return cycles;
  l_0xd6:
    ea = ea_direct ();
    reg_b = read8 (ea);
    inst_tst8 (reg_b);
    cycles += 4;
  return cycles;
  l_0xe6:
    ea = ea_indexed (&cycles);
    reg_b = read8 (ea);
    inst_tst8 (reg_b);
    cycles += 4;
  return cycles;
  l_0xf6:
    ea = ea_extended ();
    reg_b = read8 (ea);
    inst_tst8 (reg_b);
    cycles += 5;
  return cycles;
  /* sta */
  l_0xa7:
    ea = ea_indexed (&cycles);
    write8 (ea, reg_a);
    inst_tst8 (reg_a);
    cycles += 4;
  return cycles;
  l_0xb7:
    ea = ea_extended ();
    write8 (ea, reg_a);
    inst_tst8 (reg_a);
    cycles += 5;
  return cycles;
  /* stb */
  l_0xd7:
    ea = ea_direct ();
    write8 (ea, reg_b);
    inst_tst8 (reg_b);
    cycles += 4;
  return cycles;
  l_0xe7:
    ea = ea_indexed (&cycles);
    write8 (ea, reg_b);
    inst_tst8 (reg_b);
    cycles += 4;
  return cycles;
  l_0xf7:
    ea = ea_extended ();
    write8 (ea, reg_b);
    inst_tst8 (reg_b);
    cycles += 5;
  return cycles;
  /* eora */
  l_0x88:
    reg_a = inst_eor (reg_a, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0x98:
    ea = ea_direct ();
    reg_a = inst_eor (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xa8:
    ea = ea_indexed (&cycles);
    reg_a = inst_eor (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xb8:
    ea = ea_extended ();
    reg_a = inst_eor (reg_a, read8 (ea));
    cycles += 5;
  return cycles;
  /* eorb */
  l_0xc8:
    reg_b = inst_eor (reg_b, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0xd8:
    ea = ea_direct ();
    reg_b = inst_eor (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xe8:
    ea = ea_indexed (&cycles);
    reg_b = inst_eor (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xf8:
    ea = ea_extended ();
    reg_b = inst_eor (reg_b, read8 (ea));
    cycles += 5;
  return cycles;
  /* adca */
  l_0x89:
    reg_a = inst_adc (reg_a, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0x99:
    ea = ea_direct ();
    reg_a = inst_adc (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xa9:
    ea = ea_indexed (&cycles);
    reg_a = inst_adc (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xb9:
    ea = ea_extended ();
    reg_a = inst_adc (reg_a, read8 (ea));
    cycles += 5;
  return cycles;
  /* adcb */
  l_0xc9:
    reg_b = inst_adc (reg_b, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0xd9:
    ea = ea_direct ();
    reg_b = inst_adc (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xe9:
    ea = ea_indexed (&cycles);
    reg_b = inst_adc (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xf9:
    ea = ea_extended ();
    reg_b = inst_adc (reg_b, read8 (ea));
    cycles += 5;
  return cycles;
  /* ora */
  l_0x8a:
    reg_a = inst_or (reg_a, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0x9a:
    ea = ea_direct ();
    reg_a = inst_or (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xaa:
    ea = ea_indexed (&cycles);
    reg_a = inst_or (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xba:
    ea = ea_extended ();
    reg_a = inst_or (reg_a, read8 (ea));
    cycles += 5;
  return cycles;
  /* orb */
  l_0xca:
    reg_b = inst_or (reg_b, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0xda:
    ea = ea_direct ();
    reg_b = inst_or (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xea:
    ea = ea_indexed (&cycles);
    reg_b = inst_or (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xfa:
    ea = ea_extended ();
    reg_b = inst_or (reg_b, read8 (ea));
    cycles += 5;
  return cycles;
  /* adda */
  l_0x8b:
    reg_a = inst_add8 (reg_a, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0x9b:
    ea = ea_direct ();
    reg_a = inst_add8 (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xab:
    ea = ea_indexed (&cycles);
    reg_a = inst_add8 (reg_a, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xbb:
    ea = ea_extended ();
    reg_a = inst_add8 (reg_a, read8 (ea));
    cycles += 5;
  return cycles;
  /* addb */
  l_0xcb:
    reg_b = inst_add8 (reg_b, pc_read8 ());
    cycles += 2;
  return cycles;
  l_0xdb:
    ea = ea_direct ();
    reg_b = inst_add8 (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xeb:
    ea = ea_indexed (&cycles);
    reg_b = inst_add8 (reg_b, read8 (ea));
    cycles += 4;
  return cycles;
  l_0xfb:
    ea = ea_extended ();
    reg_b = inst_add8 (reg_b, read8 (ea));
    cycles += 5;
  return cycles;
  /* subd */
  l_0x83:
    set_reg_d (inst_sub16 (get_reg_d (), pc_read16 ()));
    cycles += 4;
  return cycles;
  l_0x93:
    ea = ea_direct ();
    set_reg_d (inst_sub16 (get_reg_d (), read16 (ea)));
    cycles += 6;
  return cycles;
  l_0xa3:
    ea = ea_indexed (&cycles);
    set_reg_d (inst_sub16 (get_reg_d (), read16 (ea)));
    cycles += 6;
  return cycles;
  l_0xb3:
    ea = ea_extended ();
    set_reg_d (inst_sub16 (get_reg_d (), read16 (ea)));
    cycles += 7;
  return cycles;
  /* cmpx */
  l_0x8c:
    inst_sub16 (reg_x, pc_read16 ());
    cycles += 4;
  return cycles;
  l_0x9c:
    ea = ea_direct ();
    inst_sub16 (reg_x, read16 (ea));
    cycles += 6;
  return cycles;
  l_0xac:
    ea = ea_indexed (&cycles);
    inst_sub16 (reg_x, read16 (ea));
    cycles += 6;
  return cycles;
  l_0xbc:
    ea = ea_extended ();
    inst_sub16 (reg_x, read16 (ea));
    cycles += 7;
  return cycles;
  /* ldx */
  l_0x8e:
    reg_x = pc_read16 ();
    inst_tst16 (reg_x);
    cycles += 3;
  return cycles;
  l_0x9e: 
    ea = ea_direct ();
    reg_x = read16 (ea);
    inst_tst16 (reg_x);
    cycles += 5;
  return cycles;
  l_0xae:
    ea = ea_indexed (&cycles);
    reg_x = read16 (ea);
    inst_tst16 (reg_x);
    cycles += 5;
  return cycles;
  l_0xbe:
    ea = ea_extended ();
    reg_x = read16 (ea);
    inst_tst16 (reg_x);
    cycles += 6;
  return cycles;
  /* ldu */
  l_0xce:
    reg_u = pc_read16 ();
    inst_tst16 (reg_u);
    cycles += 3;
  return cycles;
  l_0xde: 
    ea = ea_direct ();
    reg_u = read16 (ea);
    inst_tst16 (reg_u);
    cycles += 5;
  return cycles;
  l_0xee:
    ea = ea_indexed (&cycles);
    reg_u = read16 (ea);
    inst_tst16 (reg_u);
    cycles += 5;
  return cycles;
  l_0xfe:
    ea = ea_extended ();
    reg_u = read16 (ea);
    inst_tst16 (reg_u);
    cycles += 6;
  return cycles;
  /* stx */
  l_0x9f:
    ea = ea_direct ();
    write16 (ea, reg_x);
    inst_tst16 (reg_x);
    cycles += 5;
  return cycles;
  l_0xaf:
    ea = ea_indexed (&cycles);
    write16 (ea, reg_x);
    inst_tst16 (reg_x);
    cycles += 5;
  return cycles;
  l_0xbf:
    ea = ea_extended ();
    write16 (ea, reg_x);
    inst_tst16 (reg_x);
    cycles += 6;
  return cycles;
  /* stu */
  l_0xdf:
    ea = ea_direct ();
    write16 (ea, reg_u);
    inst_tst16 (reg_u);
    cycles += 5;
  return cycles;
  l_0xef:
    ea = ea_indexed (&cycles);
    write16 (ea, reg_u);
    inst_tst16 (reg_u);
    cycles += 5;
  return cycles;
  l_0xff:
    ea = ea_extended ();
    write16 (ea, reg_u);
    inst_tst16 (reg_u);
    cycles += 6;
  return cycles;
  /* addd */
  l_0xc3:
    set_reg_d (inst_add16 (get_reg_d (), pc_read16 ()));
    cycles += 4;
  return cycles;
  l_0xd3:
    ea = ea_direct ();
    set_reg_d (inst_add16 (get_reg_d (), read16 (ea)));
    cycles += 6;
  return cycles;
  l_0xe3:
    ea = ea_indexed (&cycles);
    set_reg_d (inst_add16 (get_reg_d (), read16 (ea)));
    cycles += 6;
  return cycles;
  l_0xf3:
    ea = ea_extended ();
    set_reg_d (inst_add16 (get_reg_d (), read16 (ea)));
    cycles += 7;
  return cycles;
  /* ldd */
  l_0xcc:
    set_reg_d (pc_read16 ());
    inst_tst16 (get_reg_d ());
    cycles += 3;
  return cycles;
  l_0xdc: 
    ea = ea_direct ();
    set_reg_d (read16 (ea));
    inst_tst16 (get_reg_d ());
    cycles += 5;
  return cycles;
  l_0xec:
    ea = ea_indexed (&cycles);
    set_reg_d (read16 (ea));
    inst_tst16 (get_reg_d ());
    cycles += 5;
  return cycles;
  l_0xfc:
    ea = ea_extended ();
    set_reg_d (read16 (ea));
    inst_tst16 (get_reg_d ());
    cycles += 6;
  return cycles;
  /* std */
  l_0xdd:
    ea = ea_direct ();
    write16 (ea, get_reg_d ());
    inst_tst16 (get_reg_d ());
    cycles += 5;
  return cycles;
  l_0xed:
    ea = ea_indexed (&cycles);
    write16 (ea, get_reg_d ());
    inst_tst16 (get_reg_d ());
    cycles += 5;
  return cycles;
  l_0xfd:
    ea = ea_extended ();
    write16 (ea, get_reg_d ());
    inst_tst16 (get_reg_d ());
    cycles += 6;
  return cycles;
  /* nop */
  l_0x12:
    cycles += 2;
  return cycles;
  /* mul */
  l_0x3d:
    r = (reg_a & 0xff) * (reg_b & 0xff);
    set_reg_d (r);

    set_cc (FLAG_Z, test_z16 (r));
    set_cc (FLAG_C, (r >> 7) & 1);
    
    cycles += 11;
  return cycles;
  /* bra */
  l_0x20:
  /* brn */
  l_0x21:
    inst_bra8 (0, op, &cycles);
  return cycles;
  /* bhi */
  l_0x22:
  /* bls */
  l_0x23:
    inst_bra8 (get_cc (FLAG_C) | get_cc (FLAG_Z), op, &cycles);
  return cycles;
  /* bhs/bcc */
  l_0x24:
  /* blo/bcs */
  l_0x25:
    inst_bra8 (get_cc (FLAG_C), op, &cycles);
  return cycles;
  /* bvc */
  l_0x28:
  /* bvs */
  l_0x29:
    inst_bra8 (get_cc (FLAG_V), op, &cycles);
  return cycles;
  /* bpl */
  l_0x2a:
  /* bmi */
  l_0x2b:
    inst_bra8 (get_cc (FLAG_N), op, &cycles);
  return cycles;
  /* bge */
  l_0x2c:
  /* blt */
  l_0x2d:
    inst_bra8 (get_cc (FLAG_N) ^ get_cc (FLAG_V), op, &cycles);
  return cycles;
  /* bgt */
  l_0x2e:
  /* ble */
  l_0x2f:
    inst_bra8 (get_cc (FLAG_Z) |
           (get_cc (FLAG_N) ^ get_cc (FLAG_V)), op, &cycles);
  return cycles;
  /* lbra */
  l_0x16:
    r = pc_read16 ();
    reg_pc += r;
    cycles += 5;
  return cycles;
  /* lbsr */
  l_0x17:
    r = pc_read16 ();
    push16 (&reg_s, reg_pc);
    reg_pc += r;
    cycles += 9;
  return cycles;
  /* bsr */
  l_0x8d:
    r = pc_read8 ();
    push16 (&reg_s, reg_pc);
    reg_pc += sign_extend (r);
    cycles += 7;
  return cycles;
  /* jsr */
  l_0x9d:
    ea = ea_direct ();
    push16 (&reg_s, reg_pc);
    reg_pc = ea;
    cycles += 7;
  return cycles;
  l_0xad:
    ea = ea_indexed (&cycles);
    push16 (&reg_s, reg_pc);
    reg_pc = ea;
    cycles += 7;
  return cycles;
  l_0xbd:
    ea = ea_extended ();
    push16 (&reg_s, reg_pc);
    reg_pc = ea;
    cycles += 8;
  return cycles;
  /* leax */
  l_0x30:
    reg_x = ea_indexed (&cycles);
    set_cc (FLAG_Z, test_z16 (reg_x));
    cycles += 4;
  return cycles;
  /* leay */
  l_0x31:
    reg_y = ea_indexed (&cycles);
    set_cc (FLAG_Z, test_z16 (reg_y));
    cycles += 4;
  return cycles;
  /* leas */
  l_0x32:
    reg_s = ea_indexed (&cycles);
    cycles += 4;
  return cycles;
  /* leau */
  l_0x33:
    reg_u = ea_indexed (&cycles);
    cycles += 4;
  return cycles;
  /* pshs */
  l_0x34:
    inst_psh (pc_read8 (), &reg_s, reg_u, &cycles);
    cycles += 5;
  return cycles;
  /* puls */
  l_0x35:
    inst_pul (pc_read8 (), &reg_s, &reg_u, &cycles);
    cycles += 5;
  return cycles;
  /* pshu */
  l_0x36:
    inst_psh (pc_read8 (), &reg_u, reg_s, &cycles);
    cycles += 5;
  return cycles;
  /* pulu */
  l_0x37:
    inst_pul (pc_read8 (), &reg_u, &reg_s, &cycles);
    cycles += 5;
  return cycles;
  /* rts */
  l_0x39:
    reg_pc = pull16 (&reg_s);
    cycles += 5;
  return cycles;
  /* abx */
  l_0x3a:
    reg_x += reg_b & 0xff;
    cycles += 3;
  return cycles;
  /* orcc */
  l_0x1a:
    reg_cc |= pc_read8 ();
    cycles += 3;
  return cycles;
  /* andcc */
  l_0x1c:
    reg_cc &= pc_read8 ();
    cycles += 3;
  return cycles;
  /* sex */
  l_0x1d:
    set_reg_d (sign_extend (reg_b));
    set_cc (FLAG_N, test_n (reg_a));
    set_cc (FLAG_Z, test_z16 (get_reg_d ()));
    cycles += 2;
  return cycles;
  /* exg */
  l_0x1e:
    inst_exg ();
    cycles += 8;
  return cycles;
  /* tfr */
  l_0x1f:
    inst_tfr ();
    cycles += 6;
  return cycles;
  /* rti */
  l_0x3b:
    if (get_cc (FLAG_E)) {
      inst_pul (0xff, &reg_s, &reg_u, &cycles);
    } else {
      inst_pul (0x81, &reg_s, &reg_u, &cycles);
    }

    cycles += 3;
  return cycles;
  /* swi */
  l_0x3f:
    set_cc (FLAG_E, 1);
    inst_psh (0xff, &reg_s, reg_u, &cycles);
    set_cc (FLAG_I, 1);
    set_cc (FLAG_F, 1);
        reg_pc = read16 (0xfffa);
        cycles += 7;
  return cycles;
  /* sync */
  l_0x13:
    irq_status = IRQ_SYNC;
    cycles += 2;
  return cycles;
  /* daa */
  l_0x19:
    i0 = reg_a;
    i1 = 0;

    if ((reg_a & 0x0f) > 0x09 || get_cc (FLAG_H) == 1) {
      i1 |= 0x06;
    }

    if ((reg_a & 0xf0) > 0x80 && (reg_a & 0x0f) > 0x09) {
      i1 |= 0x60;
    }

    if ((reg_a & 0xf0) > 0x90 || get_cc (FLAG_C) == 1) {
      i1 |= 0x60;
    }

    reg_a = i0 + i1;

    set_cc (FLAG_N, test_n (reg_a));
    set_cc (FLAG_Z, test_z8 (reg_a));
    set_cc (FLAG_V, 0);
    set_cc (FLAG_C, test_c (i0, i1, reg_a, 0));
    cycles += 2;
  return cycles;
  /* cwai */
  l_0x3c:
    reg_cc &= pc_read8 ();
    set_cc (FLAG_E, 1);
    inst_psh (0xff, &reg_s, reg_u, &cycles);
    irq_status = IRQ_CWAI;
    cycles += 4;
  return cycles;

  /* page 1 instructions */

  l_0x10:
    cycles += e6809_op10(); 
  return cycles;

  /* page 2 instructions */

  l_0x11:
    op = pc_read8 ();

    switch (op) {
    /* cmpu */
    case 0x83:
      inst_sub16 (reg_u, pc_read16 ());
      cycles += 5;
    break;
    case 0x93:
      ea = ea_direct ();
      inst_sub16 (reg_u, read16 (ea));
      cycles += 7;
    break;
    case 0xa3:
      ea = ea_indexed (&cycles);
      inst_sub16 (reg_u, read16 (ea));
      cycles += 7;
    break;
    case 0xb3:
      ea = ea_extended ();
      inst_sub16 (reg_u, read16 (ea));
      cycles += 8;
    break;
    /* cmps */
    case 0x8c:
      inst_sub16 (reg_s, pc_read16 ());
      cycles += 5;
    break;
    case 0x9c:
      ea = ea_direct ();
      inst_sub16 (reg_s, read16 (ea));
      cycles += 7;
    break;
    case 0xac:
      ea = ea_indexed (&cycles);
      inst_sub16 (reg_s, read16 (ea));
      cycles += 7;
    break;
    case 0xbc:
      ea = ea_extended ();
      inst_sub16 (reg_s, read16 (ea));
      cycles += 8;
    break;
    /* swi3 */
    case 0x3f:
      set_cc (FLAG_E, 1);
      inst_psh (0xff, &reg_s, reg_u, &cycles);
        reg_pc = read16 (0xfff2);
      cycles += 8;
    break;
    default:
      printf ("unknown page-2 op code: %.2x\n", op);
    break;
    }
  l_0xcf:
  l_0xcd:
  l_0xc7:
  l_0x8f:
  l_0x87:
  l_0x7b:
  l_0x75:
  l_0x72:
  l_0x71:
  l_0x6b:
  l_0x65:
  l_0x62:
  l_0x61:
  l_0x5e:
  l_0x5b:
  l_0x55:
  l_0x52:
  l_0x51:
  l_0x4e:
  l_0x4b:
  l_0x45:
  l_0x42:
  l_0x41:
  l_0x3e:
  l_0x38:
  l_0x1b:
  l_0x18:
  l_0x15:
  l_0x14:
  l_0x0b:
  l_0x05:
  l_0x02:
  l_0x01:
  
  //}
  return cycles;
}

