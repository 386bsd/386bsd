/*
 * BDM2: Banked dumb monochrome driver
 * Pascal Haible 9/93, haible@izfm.uni-stuttgart.de
 *
 * bdm2/driver/sigma/sigmaPorts.h
 * I/O Port definitions for Sigma L-View and Sigma LaserView PLUS
 *
 * see bdm2/COPYRIGHT for copyright and disclaimers.
 */

/* Many thanks to Rich Murphey (rich@rice.edu) who sent me the docs */

/* $XFree86: mit/server/ddx/x386/bdm2/drivers/sigma/sigmaPorts.h,v 2.1 1993/09/10 08:11:46 dawes Exp $ */

/*
 * Name   r/w I/O  Bit Reset Description
 * -------------------------------------
 * Expanded Memory System Window Address Registers
 * EN1    r/w 0249 D7  0     EMS Frame Access Enable
 * W16    r/w 4249 D7  0     EMS Frame Pointer A16 (Location of 64k Frame)
 * W17    r/w 8249 D7  0     EMS Frame Pointer A17 (8000:0000 - E000:0000)
 * W18    r/w C249 D7  0     EMS Frame Pointer A18 (on 64k boundarys)
 * High Resolution Mode Control Registers
 * BLANK  r/w 0649 D7  0     HiRes Video Display Enable
 * ZOOM   r/w 4649 D7  0     Multiplane Write Enable
 * GR0    r/w 8649 D7  0     Gray 0
 * GR1    r/w C649 D7  0     Gray 1
 * Expanded Memory System Window Page Registers
 * BANK0  r/w 0248 D7  x     16k Page at EMS Frame Pointer +  0k
 * BANK1  r/w 4248 D7  x     16k Page at EMS Frame Pointer + 16k
 * BANK2  r/w 8248 D7  x     16k Page at EMS Frame Pointer + 32k
 * BANK3  r/w C248 D7  x     16k Page at EMS Frame Pointer + 48k
 * Emulation Control/Configuration Register
 * HIRES  r/w 0A48 D7  0     Emulation Display Disable
 * MONOEN ro  0A48 D6        Emulation Hardware Enable (Jumper)
 * Emulation Mode Control Register
 * BOLD   r/w 8A48 D1  0     Dither/Bold Text Intensity Representation
 * WOB    r/w 8A48 D0  0     Black Text on White Background/White Text on
 *                           Black Background
 */

#if defined(ACK_ASSEMBLER) || defined(C_STYLE_HEX_CONSTANTS)
#define SLV_EN1    0x0249
#define SLV_W16    0x4249
#define SLV_W17    0x8249
#define SLV_W18    0xC249
#define SLV_BLANK  0x0649
#define SLV_ZOOM   0x4649
#define SLV_GR0    0x8649
#define SLV_GR1    0xC649
#define SLV_BANK0  0x0248
#define SLV_BANK1  0x4248
#define SLV_BANK2  0x8248
#define SLV_BANK3  0xC248
#define SLV_HIRES  0x0A48
#define SLV_MONOEN 0x0A48
#define SLV_BOLD   0x8A48
#define SLV_WOB    0x8A48
#else
#define SLV_EN1    $0x0249
#define SLV_W16    $0x4249
#define SLV_W17    $0x8249
#define SLV_W18    $0xC249
#define SLV_BLANK  $0x0649
#define SLV_ZOOM   $0x4649
#define SLV_GR0    $0x8649
#define SLV_GR1    $0xC649
#define SLV_BANK0  $0x0248
#define SLV_BANK1  $0x4248
#define SLV_BANK2  $0x8248
#define SLV_BANK3  $0xC248
#define SLV_HIRES  $0x0A48
#define SLV_MONOEN $0x0A48
#define SLV_BOLD   $0x8A48
#define SLV_WOB    $0x8A48
#endif
