/************************************************************
Copyright 1987 by Sun Microsystems, Inc. Mountain View, CA.

                    All Rights Reserved

Permission  to  use,  copy,  modify,  and  distribute   this
software  and  its documentation for any purpose and without
fee is hereby granted, provided that the above copyright no-
tice  appear  in all copies and that both that copyright no-
tice and this permission notice appear in  supporting  docu-
mentation,  and  that the names of Sun or MIT not be used in
advertising or publicity pertaining to distribution  of  the
software  without specific prior written permission. Sun and
M.I.T. make no representations about the suitability of this
software for any purpose. It is provided "as is" without any
express or implied warranty.

SUN DISCLAIMS ALL WARRANTIES WITH REGARD TO  THIS  SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FIT-
NESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SUN BE  LI-
ABLE  FOR  ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,  DATA  OR
PROFITS,  WHETHER  IN  AN  ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
THE USE OR PERFORMANCE OF THIS SOFTWARE.

********************************************************/

/* $XConsortium: cfbmskbits.h,v 4.19 91/07/05 10:53:09 rws Exp $ */

extern unsigned int cfbstarttab[];
extern unsigned int cfbendtab[];
extern unsigned int cfbstartpartial[];
extern unsigned int cfbendpartial[];
extern unsigned int cfbrmask[];
extern unsigned int cfbmask[];
extern unsigned int QuartetBitsTable[];
extern unsigned int QuartetPixelMaskTable[];


/*
 * ==========================================================================
 * Converted from mfb to support memory-mapped color framebuffer by smarks@sun, 
 * April-May 1987.
 *
 * The way I did the conversion was to consider each longword as an
 * array of four bytes instead of an array of 32 one-bit pixels.  So
 * getbits() and putbits() retain much the same calling sequence, but
 * they move bytes around instead of bits.  Of course, this entails the
 * removal of all of the one-bit-pixel dependencies from the other
 * files, but the major bit-hacking stuff should be covered here.
 *
 * I've created some new macros that make it easier to understand what's 
 * going on in the pixel calculations, and that make it easier to change the 
 * pixel size.
 *
 * name	    mfb	    cfb	    explanation
 * ----	    ---	    ---	    -----------
 * PPW	    32	    4	    pixels per word
 * PLST	    31	    3	    last pixel in a word (should be PPW-1)
 * PIM	    0x1f    0x03    pixel index mask (index within a word)
 * PWSH	    5	    2	    pixel-to-word shift
 * PSZ	    1	    8	    pixel size (bits)
 * PMSK	    0x01    0xFF    single-pixel mask
 *
 * Note that even with these new macros, there are still many dependencies 
 * in the code on the fact that there are 32 bits (not necessarily pixels) 
 * in each word.  These macros remove the dependency that 1 pixel == 1 bit.
 *
 * I have also added a new macro, PFILL, that takes one pixel and
 * replicates it throughout a word.  This macro definition is dependent
 * upon pixel and word size; it doesn't use macros like PPW and so
 * forth.  Examples: for monochrome, PFILL(1) => 0xffffffff, PFILL(0) =>
 * 0x00000000.  For 8-bit color, PFILL(0x5d) => 0x5d5d5d5d.  This macro
 * is used primarily for replicating a plane mask into a word.
 *
 * Color framebuffers operations also support the notion of a plane
 * mask.  This mask determines which planes of the framebuffer can be
 * altered; the others are left unchanged.  I have added another
 * parameter to the putbits and putbitsrop macros that is the plane
 * mask.
 * ==========================================================================
 */

#define PPW	4
#define PLST	3
#define PIM	0x03
#define PWSH	2
#define PSZ	8
#define PMSK	0xFF

/* the following notes use the following conventions:
SCREEN LEFT				SCREEN RIGHT
in this file and maskbits.c, left and right refer to screen coordinates,
NOT bit numbering in registers.

cfbstarttab[n] 
	pixels[0,n-1] = 0's	pixels[n,PPW-1] = 1's
cfbendtab[n] =
	pixels[0,n-1] = 1's	pixels[n,PPW-1] = 0's

cfbstartpartial[], cfbendpartial[]
	these are used as accelerators for doing putbits and masking out
bits that are all contained between longword boudaries.  the extra
256 bytes of data seems a small price to pay -- code is smaller,
and narrow things (e.g. window borders) go faster.

the names may seem misleading; they are derived not from which end
of the word the bits are turned on, but at which end of a scanline
the table tends to be used.

look at the tables and macros to understand boundary conditions.
(careful readers will note that starttab[n] = ~endtab[n] for n != 0)

-----------------------------------------------------------------------
these two macros depend on the screen's bit ordering.
in both of them x is a screen position.  they are used to
combine bits collected from multiple longwords into a
single destination longword, and to unpack a single
source longword into multiple destinations.

SCRLEFT(dst, x)
	takes dst[x, PPW] and moves them to dst[0, PPW-x]
	the contents of the rest of dst are 0 ONLY IF
	dst is UNSIGNED.
	is cast as an unsigned.
	this is a right shift on the VAX, left shift on
	Sun and pc-rt.

SCRRIGHT(dst, x)
	takes dst[0,x] and moves them to dst[PPW-x, PPW]
	the contents of the rest of dst are 0 ONLY IF
	dst is UNSIGNED.
	this is a left shift on the VAX, right shift on
	Sun and pc-rt.


the remaining macros are cpu-independent; all bit order dependencies
are built into the tables and the two macros above.

maskbits(x, w, startmask, endmask, nlw)
	for a span of width w starting at position x, returns
a mask for ragged pixels at start, mask for ragged pixels at end,
and the number of whole longwords between the ends.

maskpartialbits(x, w, mask)
	works like maskbits(), except all the pixels are in the
	same longword (i.e. (x&0xPIM + w) <= PPW)

mask32bits(x, w, startmask, endmask, nlw)
	as maskbits, but does not calculate nlw.  it is used by
	cfbGlyphBlt to put down glyphs <= PPW bits wide.

getbits(psrc, x, w, dst)
	starting at position x in psrc (x < PPW), collect w
	pixels and put them in the screen left portion of dst.
	psrc is a longword pointer.  this may span longword boundaries.
	it special-cases fetching all w bits from one longword.

	+--------+--------+		+--------+
	|    | m |n|      |	==> 	| m |n|  |
	+--------+--------+		+--------+
	    x      x+w			0     w
	psrc     psrc+1			dst
			m = PPW - x
			n = w - m

	implementation:
	get m pixels, move to screen-left of dst, zeroing rest of dst;
	get n pixels from next word, move screen-right by m, zeroing
		 lower m pixels of word.
	OR the two things together.

putbits(src, x, w, pdst, planemask)
	starting at position x in pdst, put down the screen-leftmost
	w bits of src.  pdst is a longword pointer.  this may
	span longword boundaries.
	it special-cases putting all w bits into the same longword.

	+--------+			+--------+--------+
	| m |n|  |		==>	|    | m |n|      |
	+--------+			+--------+--------+
	0     w				     x     x+w
	dst				pdst     pdst+1
			m = PPW - x
			n = w - m

	implementation:
	get m pixels, shift screen-right by x, zero screen-leftmost x
		pixels; zero rightmost m bits of *pdst and OR in stuff
		from before the semicolon.
	shift src screen-left by m, zero bits n-32;
		zero leftmost n pixels of *(pdst+1) and OR in the
		stuff from before the semicolon.

putbitsrop(src, x, w, pdst, planemask, ROP)
	like putbits but calls DoRop with the rasterop ROP (see cfb.h for
	DoRop)

getleftbits(psrc, w, dst)
	get the leftmost w (w<=PPW) bits from *psrc and put them
	in dst.  this is used by the cfbGlyphBlt code for glyphs
	<=PPW bits wide.
*/

#include	"X.h"
#include	"Xmd.h"
#include	"servermd.h"
#if	(BITMAP_BIT_ORDER == MSBFirst)
#define BitRight(lw,n)	((lw) >> (n))
#define BitLeft(lw,n)	((lw) << (n))
#else	/* (BITMAP_BIT_ORDER == LSBFirst) */
#define BitRight(lw,n)	((lw) << (n))
#define BitLeft(lw,n)	((lw) >> (n))
#endif	/* (BITMAP_BIT_ORDER == MSBFirst) */

#define SCRLEFT(lw, n)	BitLeft (lw, (n) * PSZ)
#define SCRRIGHT(lw, n)	BitRight(lw, (n) * PSZ)

/*
 * Note that the shift direction is independent of the byte ordering of the 
 * machine.  The following is portable code.
 */
#define PFILL(p) ( ((p)&PMSK)          | \
		   ((p)&PMSK) <<   PSZ | \
		   ((p)&PMSK) << 2*PSZ | \
		   ((p)&PMSK) << 3*PSZ )
#define PFILL2(p, pf) { \
    pf = (p) & PMSK; \
    pf |= (pf << PSZ); \
    pf |= (pf << 2*PSZ); \
}

/*
 * Reduced raster op - using precomputed values, perform the above
 * in three instructions
 */

#define DoRRop(dst, and, xor)	(((dst) & (and)) ^ (xor))

#define DoMaskRRop(dst, and, xor, mask) \
    (((dst) & ((and) | ~(mask))) ^ (xor & mask))

#define maskbits(x, w, startmask, endmask, nlw) \
    startmask = cfbstarttab[(x)&PIM]; \
    endmask = cfbendtab[((x)+(w)) & PIM]; \
    if (startmask) \
	nlw = (((w) - (PPW - ((x)&PIM))) >> PWSH); \
    else \
	nlw = (w) >> PWSH;

#define maskpartialbits(x, w, mask) \
    mask = cfbstartpartial[(x) & PIM] & cfbendpartial[((x) + (w)) & PIM];

#define mask32bits(x, w, startmask, endmask) \
    startmask = cfbstarttab[(x)&PIM]; \
    endmask = cfbendtab[((x)+(w)) & PIM];


#define getbits(psrc, x, w, dst) \
if ( ((x) + (w)) <= PPW) \
{ \
    dst = SCRLEFT(*(psrc), (x)); \
} \
else \
{ \
    int m; \
    m = PPW-(x); \
    dst = (SCRLEFT(*(psrc), (x)) & cfbendtab[m]) | \
	  (SCRRIGHT(*((psrc)+1), m) & cfbstarttab[m]); \
}


#define putbits(src, x, w, pdst, planemask) \
if ( ((x)+(w)) <= PPW) \
{ \
    unsigned long tmpmask; \
    maskpartialbits((x), (w), tmpmask); \
    tmpmask &= PFILL(planemask); \
    *(pdst) = (*(pdst) & ~tmpmask) | (SCRRIGHT(src, x) & tmpmask); \
} \
else \
{ \
    unsigned long m; \
    unsigned long n; \
    unsigned long pm = PFILL(planemask); \
    m = PPW-(x); \
    n = (w) - m; \
    *(pdst) = (*(pdst) & (cfbendtab[x] | ~pm)) | \
	(SCRRIGHT(src, x) & (cfbstarttab[x] & pm)); \
    *((pdst)+1) = (*((pdst)+1) & (cfbstarttab[n] | ~pm)) | \
	(SCRLEFT(src, m) & (cfbendtab[n] & pm)); \
}
#if defined(__GNUC__) && defined(mc68020)
#undef getbits
#define FASTGETBITS(psrc, x, w, dst) \
    asm ("bfextu %3{%1:%2},%0" \
	 : "=d" (dst) : "di" (x), "di" (w), "o" (*(char *)(psrc)))

#define getbits(psrc,x,w,dst) \
{ \
    FASTGETBITS(psrc, (x) * PSZ, (w) * PSZ, dst); \
    dst = SCRLEFT(dst,PPW-(w)); \
}

#define FASTPUTBITS(src, x, w, pdst) \
    asm ("bfins %3,%0{%1:%2}" \
	 : "=o" (*(char *)(pdst)) \
	 : "di" (x), "di" (w), "d" (src), "0" (*(char *) (pdst)))

#undef putbits
#define putbits(src, x, w, pdst, planemask) \
{ \
    if (planemask != PMSK) { \
        unsigned long _m, _pm; \
        FASTGETBITS(pdst, (x) * PSZ , (w) * PSZ, _m); \
        PFILL2(planemask, _pm); \
        _m &= (~_pm); \
        _m |= (SCRRIGHT(src, PPW-(w)) & _pm); \
        FASTPUTBITS(_m, (x) * PSZ, (w) * PSZ, pdst); \
    } else { \
        FASTPUTBITS(SCRRIGHT(src, PPW-(w)), (x) * PSZ, (w) * PSZ, pdst); \
    } \
}
    

#endif /* mc68020 */

#define putbitsrop(src, x, w, pdst, planemask, rop) \
if ( ((x)+(w)) <= PPW) \
{ \
    unsigned long tmpmask; \
    unsigned long t1, t2; \
    maskpartialbits((x), (w), tmpmask); \
    PFILL2(planemask, t1); \
    tmpmask &= t1; \
    t1 = SCRRIGHT((src), (x)); \
    DoRop(t2, rop, t1, *(pdst)); \
    *(pdst) = (*(pdst) & ~tmpmask) | (t2 & tmpmask); \
} \
else \
{ \
    unsigned long m; \
    unsigned long n; \
    unsigned long t1, t2; \
    unsigned long pm; \
    PFILL2(planemask, pm); \
    m = PPW-(x); \
    n = (w) - m; \
    t1 = SCRRIGHT((src), (x)); \
    DoRop(t2, rop, t1, *(pdst)); \
    *(pdst) = (*(pdst) & (cfbendtab[x] | ~pm)) | (t2 & (cfbstarttab[x] & pm));\
    t1 = SCRLEFT((src), m); \
    DoRop(t2, rop, t1, *((pdst) + 1)); \
    *((pdst)+1) = (*((pdst)+1) & (cfbstarttab[n] | ~pm)) | \
	(t2 & (cfbendtab[n] & pm)); \
}

/*
 * Use these macros only when you're using the MergeRop stuff
 * in ../mfb/mergerop.h
 */

/* useful only when not spanning destination longwords */
#define putbitsmropshort(src,x,w,pdst) {\
    unsigned long   _tmpmask; \
    unsigned long   _t1; \
    maskpartialbits ((x), (w), _tmpmask); \
    _t1 = SCRRIGHT((src), (x)); \
    *pdst = DoMaskMergeRop(_t1, *pdst, _tmpmask); \
}

/* useful only when spanning destination longwords */
#define putbitsmroplong(src,x,w,pdst) { \
    unsigned long   _startmask, _endmask; \
    int		    _m; \
    unsigned long   _t1; \
    _m = PPW - (x); \
    _startmask = cfbstarttab[x]; \
    _endmask = cfbendtab[(w) - _m]; \
    _t1 = SCRRIGHT((src), (x)); \
    pdst[0] = DoMaskMergeRop(_t1,pdst[0],_startmask); \
    _t1 = SCRLEFT ((src),_m); \
    pdst[1] = DoMaskMergeRop(_t1,pdst[1],_endmask); \
}

#define putbitsmrop(src,x,w,pdst) \
if ((x) + (w) <= PPW) {\
    putbitsmropshort(src,x,w,pdst); \
} else { \
    putbitsmroplong(src,x,w,pdst); \
}

#if GETLEFTBITS_ALIGNMENT == 1
#define getleftbits(psrc, w, dst)	dst = *((unsigned int *) psrc)
#endif /* GETLEFTBITS_ALIGNMENT == 1 */

#define getglyphbits(psrc, x, w, dst) \
{ \
    dst = BitLeft((unsigned) *(psrc), (x)); \
    if ( ((x) + (w)) > 32) \
	dst |= (BitRight((unsigned) *((psrc)+1), 32-(x))); \
}
#if GETLEFTBITS_ALIGNMENT == 2
#define getleftbits(psrc, w, dst) \
    { \
	if ( ((int)(psrc)) & 0x01 ) \
		getglyphbits( ((unsigned int *)(((char *)(psrc))-1)), 8, (w), (dst) ); \
	else \
		dst = *((unsigned int *) psrc); \
    }
#endif /* GETLEFTBITS_ALIGNMENT == 2 */

#if GETLEFTBITS_ALIGNMENT == 4
#define getleftbits(psrc, w, dst) \
    { \
	int off, off_b; \
	off_b = (off = ( ((int)(psrc)) & 0x03)) << 3; \
	getglyphbits( \
		(unsigned int *)( ((char *)(psrc)) - off), \
		(off_b), (w), (dst) \
	       ); \
    }
#endif /* GETLEFTBITS_ALIGNMENT == 4 */

#if (PPW*PSZ==32)
#define GET_VALID_BITS_FROM_LONG(l) (l)
#else
#define GET_VALID_BITS_FROM_LONG(l) ((l)&((1L<<(PPW*PSZ))-1))
#endif

/*
 * getstipplepixels( psrcstip, x, w, ones, psrcpix, destpix )
 *
 * Converts bits to pixels in a reasonable way.  Takes w (1 <= w <= 4)
 * bits from *psrcstip, starting at bit x; call this a quartet of bits.
 * Then, takes the pixels from *psrcpix corresponding to the one-bits (if
 * ones is TRUE) or the zero-bits (if ones is FALSE) of the quartet
 * and puts these pixels into destpix.
 *
 * Example:
 *
 *      getstipplepixels( &(0x08192A3B), 17, 4, 1, &(0x4C5D6E7F), dest )
 *
 * 0x08192A3B = 0000 1000 0001 1001 0010 1010 0011 1011
 *
 * This will take 4 bits starting at bit 17, so the quartet is 0x5 = 0101.
 * It will take pixels from 0x4C5D6E7F corresponding to the one-bits in this
 * quartet, so dest = 0x005D007F.
 *
 * XXX Works with both byte order.
 * XXX This works for all values of x and w within a doubleword.
 */
#if (BITMAP_BIT_ORDER == MSBFirst)
#define getstipplepixels( psrcstip, x, w, ones, psrcpix, destpix ) \
{ \
    unsigned int q; \
    int m; \
    if ((m = ((x) - ((PPW*PSZ)-4))) > 0) { \
        q = (*(psrcstip)) << m; \
	if ( (x)+(w) > (PPW*PSZ) ) \
	    q |= GET_VALID_BITS_FROM_LONG(*((psrcstip)+1)) >> ((PPW*PSZ)-m); \
    } \
    else \
        q = (*(psrcstip)) >> -m; \
    q = QuartetBitsTable[(w)] & ((ones) ? q : ~q); \
    *(destpix) = (*(psrcpix)) & QuartetPixelMaskTable[q]; \
}
#else /* BITMAP_BIT_ORDER == LSB */
#define getstipplepixels( psrcstip, xt, w, ones, psrcpix, destpix ) \
{ \
    unsigned int q; \
    q = GET_VALID_BITS_FROM_LONG(*(psrcstip)) >> (xt); \
    if ( ((xt)+(w)) > (PPW*PSZ) ) \
        q |= (*((psrcstip)+1)) << ((PPW*PSZ)-(xt)); \
    q = QuartetBitsTable[(w)] & ((ones) ? q : ~q); \
    *(destpix) = (*(psrcpix)) & QuartetPixelMaskTable[q]; \
}
#endif
