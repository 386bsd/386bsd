/* $XFree86: mit/server/ddx/mfb/mfbcustom.h,v 1.5 1993/03/27 09:00:41 dawes Exp $ */

/*
 * This file customizes the monochrome frame bufer library.
 * This will be the same old mfb as we know from mit distribution.
 */

#define MAPRW(p) ((unsigned int*)(p))
#define MAPR(p)  MAPRW(p)
#define MAPW(p)  MAPRW(p)

/* Some dummy defines for bank-related stuff */
#define CHECKSCREEN(x)		FALSE
#define SETRWF(f,x)		/**/
#define CHECKRWOF(f,x)		/**/
#define CHECKRWUF(f,x)		/**/
#define BANK_FLAG(a)		/**/
#define BANK_FLAG_BOTH(a,b)	/**/
#define SETR(a)			/**/
#define SETW(a)			/**/
#define SETRW(a)		/**/
#define CHECKRO(a)		/**/
#define CHECKWO(a)		/**/
#define CHECKRWO(a)		/**/
#define CHECKRU(a)		/**/
#define CHECKWU(a)		/**/
#define CHECKRWU(a)		/**/
#define NEXTR(a)		/**/
#define NEXTW(a)		/**/
#define PREVR(a)		/**/
#define PREVW(a)		/**/
#define SAVE_BANK()		/**/
#define RESTORE_BANK()		/**/
#define PUSHR()			/**/
#define POPR()			/**/
#define CHECKRWONEXT(a)		/**/
#define CHECKRWD(a,b,c)		/**/

