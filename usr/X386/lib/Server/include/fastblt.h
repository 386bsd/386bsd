/*
 * Fast bitblt macros for certain hardware.  If your machine has an addressing
 * mode of small constant + register, you'll probably want this magic specific
 * code.  It's 25% faster for the R2000.  I haven't studied the Sparc
 * instruction set, but I suspect it also has this addressing mode.  Also,
 * unrolling the loop by 32 is possibly excessive for mfb. The number of times
 * the loop is actually looped through is pretty small.
 */

/*
 * WARNING:  These macros make *a lot* of assumptions about
 * the environment they are invoked in.  Plenty of implicit
 * arguments, lots of side effects.  Don't use them casually.
 */

#define SwitchOdd(n) case n: BodyOdd(n)
#define SwitchEven(n) case n: BodyEven(n)

/* to allow mfb and cfb to share code... */
#ifndef BitRight
#define BitRight(a,b) SCRRIGHT(a,b)
#define BitLeft(a,b) SCRLEFT(a,b)
#endif

#ifdef LARGE_INSTRUCTION_CACHE
#define UNROLL 8
#define PackedLoop \
    switch (nl & (UNROLL-1)) { \
    SwitchOdd( 7) SwitchEven( 6) SwitchOdd( 5) SwitchEven( 4) \
    SwitchOdd( 3) SwitchEven( 2) SwitchOdd( 1) \
    } \
    while ((nl -= UNROLL) >= 0) { \
	LoopReset \
	BodyEven( 8) \
    	BodyOdd( 7) BodyEven( 6) BodyOdd( 5) BodyEven( 4) \
    	BodyOdd( 3) BodyEven( 2) BodyOdd( 1) \
    }
#else
#define UNROLL 4
#define PackedLoop \
    switch (nl & (UNROLL-1)) { \
    SwitchOdd( 3) SwitchEven( 2) SwitchOdd( 1) \
    } \
    while ((nl -= UNROLL) >= 0) { \
	LoopReset \
    	BodyEven( 4) \
    	BodyOdd( 3) BodyEven( 2) BodyOdd( 1) \
    }
#endif

#define DuffL(counter,label,body) \
    switch (counter & 3) { \
    label: \
        body \
    case 3: \
	body \
    case 2: \
	body \
    case 1: \
	body \
    case 0: \
	if ((counter -= 4) >= 0) \
	    goto label; \
    }
