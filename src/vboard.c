#include "config.h"

#include "rendition.h"
#include "v1krisc.h"
#include "vboard.h"
#include "vos.h"

/*
 * functions
 */
int
verite_resetboard(ScrnInfoPtr pScreenInfo)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    vu16 iob=pRendition->board.io_base;
    vu8 memendian=verite_in8(iob+MEMENDIAN);
    vu32 crtcctl = verite_in32(iob+CRTCCTL);

    v1k_softreset(pScreenInfo);
    verite_out8(iob+MEMENDIAN, memendian);
    verite_out32(iob+CRTCCTL, crtcctl);

    return 0;
}

int
verite_getmemorysize(ScrnInfoPtr pScreenInfo)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);

#define PATTERN  0xf5faaf5f
#define START    0x12345678
#define ONEMEG   (1024L*1024L)
    vu32 offset;
    vu32 pattern;
    vu32 start;
    vu8 memendian;
#ifdef XSERVER
    vu8 modereg;

    modereg=verite_in8(pRendition->board.io_base+MODEREG);
    verite_out8(pRendition->board.io_base+MODEREG, NATIVE_MODE);
#endif

    /* no byte swapping */
    memendian=verite_in8(pRendition->board.io_base+MEMENDIAN);
    verite_out8(pRendition->board.io_base+MEMENDIAN, MEMENDIAN_NO);

    /* it looks like the v1000 wraps the memory; but for I'm not sure,
     * let's test also for non-writable offsets */
    start=verite_read_memory32(pRendition->board.vmem_base, 0);
    verite_write_memory32(pRendition->board.vmem_base, 0, START);
    for (offset=ONEMEG; offset<16*ONEMEG; offset+=ONEMEG) {
#ifdef DEBUG
        ErrorF( "Testing %d MB: ", offset/ONEMEG);
#endif
        pattern=verite_read_memory32(pRendition->board.vmem_base, offset);
        if (START == pattern) {
#ifdef DEBUG
            ErrorF( "Back at the beginning\n");
#endif
            break;
        }

        pattern^=PATTERN;
        verite_write_memory32(pRendition->board.vmem_base, offset, pattern);

#ifdef DEBUG
        ErrorF( "%x <-> %x\n", (int)pattern,
                    (int)verite_read_memory32(pRendition->board.vmem_base, offset));
#endif

        if (pattern != verite_read_memory32(pRendition->board.vmem_base, offset)) {
            offset-=ONEMEG;
            break;
        }
        verite_write_memory32(pRendition->board.vmem_base, offset, pattern^PATTERN);
    }
    verite_write_memory32(pRendition->board.vmem_base, 0, start);

    if (16*ONEMEG <= offset)
        pRendition->board.mem_size=4*ONEMEG;
    else
	    pRendition->board.mem_size=offset;

    /* restore default byte swapping */
    verite_out8(pRendition->board.io_base+MEMENDIAN, memendian);

#ifdef XSERVER
    verite_out8(pRendition->board.io_base+MODEREG, modereg);
#endif

    return pRendition->board.mem_size;
#undef PATTERN
#undef ONEMEG
}

/*
 * end of file vboard.c
 */
