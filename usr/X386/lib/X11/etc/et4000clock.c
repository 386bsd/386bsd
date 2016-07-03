/*
 * $XFree86: mit/server/ddx/x386/etc/et4000clock.c,v 1.3 1993/03/27 09:31:25 dawes Exp $
 *
 * This is a sample clock setting program.  It will not work with all
 * ET4000 cards.  To work correctly the clocks line in Xconfig must
 * have the values in the correct order.
 *
 * usage: et4000clock freq index
 *
 * This program ignores 'freq' and relies entirely on 'index'
 *
 * David Dawes  <dawes@physics.su.oz.au>
 * 19 December 1992
 */

#include <stdio.h>

/* The following inlines are from compiler.h in the XFree86 source dist */

#if defined(__386BSD__) || defined(MACH) || defined(MACH386) || defined(linux)
#define GCCUSESGAS
#endif

#ifdef __GNUC__
#ifdef GCCUSESGAS

static __inline__ void
outb(port, val)
short port;
char val;
{
   __asm__ __volatile__("outb %0,%1" : :"a" (val), "d" (port));
}

static __inline__ void
outw(port, val)
short port;
short val;
{
   __asm__ __volatile__("outw %0,%1" : :"a" (val), "d" (port));
}

static __inline__ unsigned int
inb(port)
short port;
{
   unsigned char ret;
   __asm__ __volatile__("inb %1,%0" :
       "=a" (ret) :
       "d" (port));
   return ret;
}

#else /* !GCCUSESGAS */

static __inline__ void
outb(port, val)
     short port;
     char val;
{
  __asm__ __volatile__("out%B0 (%1)" : :"a" (val), "d" (port));
}

static __inline__ void
outw(port, val)
     short port;
     short val;
{
  __asm__ __volatile__("out%W0 (%1)" : :"a" (val), "d" (port));
}

static __inline__ unsigned int
inb(port)
     short port;
{
  unsigned int ret;
  __asm__ __volatile__("in%B0 (%1)" :
                   "=a" (ret) :
                   "d" (port));
  return ret;
}

#endif /* GCCUSESGAS */
#else /* !__GNUC__ */

#if defined(__STDC__) && (__STDC__ == 1)
#define asm __asm
#endif

#ifdef SVR4
#include <sys/types.h>
#ifndef __USLC__
#define __USLC__
#endif
#endif

#include <sys/inline.h>
#endif /* __GNUC__ */


/* Now, the actual program */

main(argc, argv)

int argc;
char *argv[];

{
  int            index, vgaIOBase;
  unsigned char  tmp; 

  if (argc < 3)
  {
    fprintf(stderr, "usage: %s freq index\n", argv[0]);
    exit(1);
  }
  index = atoi(argv[2]);
  if (index < 0 || index > 7)
  {
    fprintf(stderr, "%s: Index %d out of range\n", argv[0], index);
    exit(2);
  }
  tmp = inb(0x3CC);
  vgaIOBase = (tmp & 0x01) ? 0x3D0 : 0x3B0;
  outb(0x3C2, (tmp & 0xF3) | ((index & 0x03) << 2));
  outw(vgaIOBase + 4, 0x34 | ((index & 0x04) << 7));
  fprintf(stderr, "%s: clock set to number %d\n", argv[0], index);
  exit(0);
}
