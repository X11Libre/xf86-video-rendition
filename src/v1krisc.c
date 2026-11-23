#include "config.h"

#include "rendition.h"
#include "v1krisc.h"
#include "vos.h"



/*
 * defines
 */

/* RISC registers */
#define RISC_FLAG 37     /* RISC flags register */
#define RISC_SP   252    /* RISC stack pointer/scratch register */
#define RISC_IRA  253    /* RISC register */
#define RISC_RA   254    /* RISC program link/scratch register */
#define RISC_FP   255    /* RISC frame pointer/scratch register */

/* RISC opcodes used */
#define NOP_INSTR       0x00000000      /* really add immed with D=zero */
#define ADDI_OP         0x00
#define SPRI_OP         0x4f
#define ADD_OP          0x10
#define ANDN_OP         0x12
#define OR_OP           0x15
#define ADDIFI_OP       0x40
#define ADDSL8_OP       0x4b
#define LB_OP           0x70
#define LH_OP           0x71
#define LW_OP           0x72
#define LI_OP           0x76
#define LUI_OP          0x77
#define SB_OP           0x78
#define SH_OP           0x79
#define SW_OP           0x7a
#define JMP_OP          0x6c

/* defines for instruction composition */
#define INT_INSTR(op, d, s2, s1) \
 (((vu32)(op)<<24) | ((vu32)(d)<<16) | ((vu32)(s2)<<8) | ((vu32)(s1)&0xff))
#define STR_INSTR(op, off8, s2, s1) \
 (((vu32)(op)<<24) | (((vu32)(off8)&0xff)<<16) | ((vu32)(s2)<<8) | ((vu32)(s1)))
#define LD_INSTR(op, d, off8, s1) \
 (((vu32)(op)<<24) | ((vu32)(d)<<16) | (((vu32)(off8)&0xff)<<8) | ((vu32)(s1)))
#define LI_INSTR(op, d, immed16) \
 (((vu32)(op)<<24) | ((vu32)(d)<<16) | ((vu32)(immed16)&0xffff))
#define BR_INSTR(op, off16, s1) \
 (((vu32)(op)<<24) | (((vu32)(off16)&0xffff)<<8) | ((vu32)(s1)))
#define JMP_INSTR(op, addr24) \
 (((vu32)(op)<<24) | ((vu32)(addr24)))

/* some masks */
#define TYPEINSTR	0xf0000000
#define TYPEBRANCH	0x60000000
#define HALTBIT		0x00000008

#define VBIOS_DTNR	0x2000

#define READ_BYTE	0
#define READ_SHORT	1
#define READ_WORD	2

#define WRITE_BYTE	0
#define WRITE_SHORT	1
#define WRITE_WORD	2

#define VERITE_MAX_POLLS	100



/*
 * local function prototypes
 */

static void verite_iopoll(unsigned long port, vu32 data, vu32 mask);
static void verite_iopoll8(unsigned long port, vu8 data, vu8 mask);

static void risc_forcestep(unsigned long io_base, vu32 instruction);



/*
 * functions
 */

/*
 * void v1k_softreset(ScrnInfoPtr pScreenInfo)
 *
 * Soft Reset RISC.
 */
void
v1k_softreset(ScrnInfoPtr pScreenInfo)
{
  renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
  unsigned long io_base=pRendition->board.io_base;

  verite_out8(io_base+DEBUGREG, SOFTRESET|HOLDRISC);
  verite_out8(io_base+STATEINDEX, STATEINDEX_PC);
  verite_iopoll(io_base+STATEDATA, 0, 0xffffffff);
  verite_iopoll(io_base+STATEDATA, 0, 0xffffffff);
  verite_iopoll(io_base+STATEDATA, 0, 0xffffffff);

  verite_out8(io_base+DEBUGREG, HOLDRISC);
  verite_iopoll(io_base+STATEDATA, 0, 0);
  verite_iopoll(io_base+STATEDATA, 0, 0);
  verite_iopoll(io_base+STATEDATA, 0, 0);

  /* turn icache on */
  risc_forcestep(io_base, LI_INSTR(LI_OP, RISC_RA, ICACHE_ONOFF_MASK&0xffff));
  risc_forcestep(io_base, INT_INSTR(ADDIFI_OP, RISC_FLAG, RISC_RA,
                                    ICACHE_ONOFF_MASK>>16));
  /* clear any interrupts */
  verite_out8(io_base+INTR, 0xff);
  /* byte swap mode=word */
  verite_out8(io_base+MEMENDIAN, MEMENDIAN_NO);
}



/*
 * local functions
 */

/*
 * static void verite_iopoll(unsigned long port, vu32 data, vu32 mask)
 *
 * Loop on IO read until expected data is read or VERITE_MAX_POLLS is reached.
 */
static void
verite_iopoll(unsigned long port, vu32 data, vu32 mask)
{
  vu32 c;

  c=0;
  do {
    c++;
    if ((verite_in32(port)&mask) == (data&mask))
      break;
  } while (c <= VERITE_MAX_POLLS);
}



/*
 * static void verite_iopoll8(unsigned long port, vu8 data, vu8 mask)
 *
 * Loop on IO read until expected data is read or VERITE_MAX_POLLS is reached.
 */
static void
verite_iopoll8(unsigned long port, vu8 data, vu8 mask)
{
  vu32 c;

  c=0;
  do {
	c++;
	if ((verite_in8(port)&mask) == (data&mask))
	  break;
  } while (c <= VERITE_MAX_POLLS);
}



/*
 * static void risc_forcestep(unsigned long io_base, vu32 instruction)
 *
 * Single step RISC; force instruction; assumes RISC held.
 */
static void
risc_forcestep(unsigned long io_base, vu32 instruction)
{
  vu32 c;
  vu8 debugreg, stateindex;


  debugreg=verite_in8(io_base+DEBUGREG);
  stateindex=verite_in8(io_base+STATEINDEX);
  verite_out8(io_base+STATEINDEX, STATEINDEX_IR);
  verite_iopoll8(io_base+STATEINDEX, STATEINDEX_IR, 0xff);    /* wait */
  verite_out32(io_base+STATEDATA, instruction);               /* load instruction */
  verite_iopoll(io_base+STATEDATA, instruction, 0xffffffff);  /* wait */
  verite_out8(io_base+DEBUGREG, debugreg|HOLDRISC|STEPRISC);  /* step */
  verite_iopoll(io_base+STATEDATA, 0, 0);                     /* short pause */

  for (c=0; c<VERITE_MAX_POLLS; c++)
    if (HOLDRISC == (verite_in8(io_base+DEBUGREG) & (HOLDRISC|STEPRISC)))
      break;

  /* restore */
  verite_out8(io_base+STATEINDEX, stateindex);
}



/*
 * end of file v1krisc.c
 */
