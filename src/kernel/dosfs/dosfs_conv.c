/*
 *  Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 *  You can do anything you want with this software,
 *    just don't say you wrote it,
 *    and don't remove this notice.
 *
 *  This software is provided "as is".
 *
 *  The author supplies this software to be publicly
 *  redistributed on the understanding that the author
 *  is not responsible for the correct functioning of
 *  this software in any circumstances and is not liable
 *  for any damages caused by this software.
 *
 *  October 1992
 *
 * $Id: dosfs_conv.c,v 1.1 94/10/19 23:46:45 bill Exp $
 */

/*
 *  System include files.
 */
#include "sys/param.h"
#include "sys/time.h"
#include "kernel.h"	/* defines tz */

/*
 *  PCFS include files.
 */
#include "direntry.h"

/*
 *  Days in each month in a regular year.
 */
u_short regyear[] = {
	31,	28,	31,	30,	31,	30,
	31,	31,	30,	31,	30,	31
};

/*
 *  Days in each month in a leap year.
 */
u_short leapyear[] = {
	31,	29,	31,	30,	31,	30,
	31,	31,	30,	31,	30,	31
};

/*
 *  Variables used to remember parts of the last time
 *  conversion.  Maybe we can avoid a full conversion.
 */
u_long lasttime;
u_long lastday;
union dosdate lastddate;
union dostime lastdtime;

/*
 *  Convert the unix version of time to dos's idea of time
 *  to be used in file timestamps.
 *  The passed in unix time is assumed to be in GMT.
 */
void
unix2dostime(tvp, ddp, dtp)
	struct timeval *tvp;
	union dosdate *ddp;
	union dostime *dtp;
{
	u_long days;
	u_long inc;
	u_long year;
	u_long month;
	u_short *months;

/*
 *  If the time from the last conversion is the same
 *  as now, then skip the computations and use the
 *  saved result.
 */
	if (lasttime != tvp->tv_sec) {
		lasttime = tvp->tv_sec - (tz.tz_minuteswest * 60)
			/* +- daylight savings time correction */;
		lastdtime.dts.dt_2seconds = (lasttime % 60) >> 1;
		lastdtime.dts.dt_minutes  = (lasttime / 60) % 60;
		lastdtime.dts.dt_hours    = (lasttime / (60 * 60)) % 24;

/*
 *  If the number of days since 1970 is the same as the
 *  last time we did the computation then skip all this
 *  leap year and month stuff.
 */
		days = lasttime / (24 * 60 * 60);
		if (days != lastday) {
			lastday = days;
			for (year = 1970; ; year++) {
				inc = year & 0x03 ? 365 : 366;
				if (days < inc) break;
				days -= inc;
			}
			months = year & 0x03 ? regyear : leapyear;
			for (month = 0; month < 12; month++) {
				if (days < months[month]) break;
				days -= months[month];
			}
			lastddate.dds.dd_day = days + 1;
			lastddate.dds.dd_month = month+1;
/*
 *  Remember dos's idea of time is relative to 1980.
 *  unix's is relative to 1970.  If somehow we get a
 *  time before 1980 then don't give totally crazy
 *  results.
 */
			lastddate.dds.dd_year = year < 1980 ? 0 : year - 1980;
		}
	}
	dtp->dti = lastdtime.dti;
	ddp->ddi = lastddate.ddi;
}

/*
 *  The number of seconds between Jan 1, 1970 and
 *  Jan 1, 1980.
 *  In that interval there were 8 regular years and
 *  2 leap years.
 */
#define	SECONDSTO1980	(((8 * 365) + (2 * 366)) * (24 * 60 * 60))

union dosdate lastdosdate;
u_long lastseconds;

/*
 *  Convert from dos' idea of time to unix'.
 *  This will probably only be called from the
 *  stat(), and fstat() system calls
 *  and so probably need not be too efficient.
 */
void
dos2unixtime(ddp, dtp, tvp)
	union dosdate *ddp;
	union dostime *dtp;
	struct timeval *tvp;
{
	u_long seconds;
	u_long month;
	u_long yr;
	u_long days;
	u_short *months;

	seconds = (dtp->dts.dt_2seconds << 1) +
		  (dtp->dts.dt_minutes * 60) +
		  (dtp->dts.dt_hours * 60 * 60);
/*
 *  If the year, month, and day from the last conversion
 *  are the same then use the saved value.
 */
	if (lastdosdate.ddi != ddp->ddi) {
		lastdosdate.ddi = ddp->ddi;
		days = 0;
		for (yr = 0; yr < ddp->dds.dd_year; yr++) {
			days += yr & 0x03 ? 365 : 366;
		}
		months = yr & 0x03 ? regyear : leapyear;
/*
 *  Prevent going from 0 to 0xffffffff in the following
 *  loop.
 */
		if (ddp->dds.dd_month == 0) {
			printf("dos2unixtime(): month value out of range (%d)\n",
				ddp->dds.dd_month);
			ddp->dds.dd_month = 1;
		}
		for (month = 0; month < ddp->dds.dd_month-1; month++) {
			days += months[month];
		}
		days += ddp->dds.dd_day - 1;
		lastseconds = (days * 24 * 60 * 60) + SECONDSTO1980;
	}
	tvp->tv_sec = seconds + lastseconds + (tz.tz_minuteswest * 60)
		/* -+ daylight savings time correction */;
	tvp->tv_usec = 0;
}

/*
 *  Cheezy macros to do case detection and conversion
 *  for the ascii character set.  DOESN'T work for ebcdic.
 */
#define	isupper(c)	(c >= 'A'  &&  c <= 'Z')
#define	islower(c)	(c >= 'a'  &&  c <= 'z')
#define	toupper(c)	(c & ~' ')
#define	tolower(c)	(c | ' ')

/*
 *  DOS filenames are made of 2 parts, the name part and
 *  the extension part.  The name part is 8 characters
 *  long and the extension part is 3 characters long.  They
 *  may contain trailing blanks if the name or extension
 *  are not long enough to fill their respective fields.
 */

/*
 *  Convert a DOS filename to a unix filename.
 *  And, return the number of characters in the
 *  resulting unix filename excluding the terminating
 *  null.
 */
int
dos2unixfn(dn, un)
	u_char dn[11];
	u_char *un;
{
	int i;
	int ni;
	int ei;
	int thislong = 0;
	u_char c;
	u_char *origun = un;

/*
 *  Find the last character in the name portion
 *  of the dos filename.
 */
	for (ni = 7; ni >= 0; ni--)
		if (dn[ni] != ' ') break;

/*
 *  Find the last character in the extension
 *  portion of the filename.
 */
	for (ei = 10; ei >= 8; ei--)
		if (dn[ei] != ' ') break;

/*
 *  Copy the name portion into the unix filename
 *  string.
 *  NOTE: DOS filenames are usually kept in upper
 *  case.  To make it more unixy we convert all
 *  DOS filenames to lower case.  Some may like
 *  this, some may not.
 */
	for (i = 0; i <= ni; i++) {
		c = dn[i];
		*un++ = isupper(c) ? tolower(c) : c;
		thislong++;
	}

/*
 *  Now, if there is an extension then put in a period
 *  and copy in the extension.
 */
	if (ei >= 8) {
		*un++ = '.';
		thislong++;
		for (i = 8; i <= ei; i++) {
			c = dn[i];
			*un++ = isupper(c) ? tolower(c) : c;
			thislong++;
		}
	}
	*un++ = 0;

/*
 *  If first char of the filename is SLOT_E5 (0x05), then
 *  the real first char of the filename should be 0xe5.
 *  But, they couldn't just have a 0xe5 mean 0xe5 because
 *  that is used to mean a freed directory slot.
 *  Another dos quirk.
 */
	if (*origun == SLOT_E5)
		*origun = 0xe5;

	return thislong;
}

/*
 *  Convert a unix filename to a DOS filename.
 *  This function does not ensure that valid
 *  characters for a dos filename are supplied.
 */
void
unix2dosfn(un, dn, unlen)
	u_char *un;
	u_char dn[11];
	int unlen;
{
	int i;
	u_char c;

/*
 *  Fill the dos filename string with blanks.
 *  These are DOS's pad characters.
 */
	for (i = 0; i <= 10; i++)
		dn[i] = ' ';

/*
 *  The filenames "." and ".." are handled specially,
 *  since they don't follow dos filename rules.
 */
	if (un[0] == '.'  &&  un[1] == '\0') {
		dn[0] = '.';
		return;
	}
	if (un[0] == '.'  &&  un[1] == '.'  &&  un[2] == '\0') {
		dn[0] = '.';
		dn[1] = '.';
		return;
	}

/*
 *  Copy the unix filename into the dos filename string
 *  upto the end of string, a '.', or 8 characters.
 *  Whichever happens first stops us.
 *  This forms the name portion of the dos filename.
 *  Fold to upper case.
 */
	for (i = 0; i <= 7  &&  unlen  &&  (c = *un)  &&  c != '.'; i++) {
		dn[i] = islower(c) ? toupper(c) : c;
		un++;
		unlen--;
	}

/*
 *  If the first char of the filename is 0xe5, then translate
 *  it to 0x05.  This is because 0xe5 is the marker for a
 *  deleted directory slot.  I guess this means you can't
 *  have filenames that start with 0x05.  I suppose we should
 *  check for this and doing something about it.
 */
	if (dn[0] == SLOT_DELETED)
		dn[0] = SLOT_E5;

/*
 *  Strip any further characters up to a '.' or the
 *  end of the string.
 */
	while (unlen  &&  (c = *un)  &&  c != '.') {
		un++;
		unlen--;
	}

/*
 *  If we stopped on a '.', then get past it.
 */
	if (c == '.') un++;

/*
 *  Copy in the extension part of the name, if any.
 *  Force to upper case.
 *  Note that the extension is allowed to contain '.'s.
 *  Filenames in this form are probably inaccessable
 *  under dos.
 */
	for (i = 8; i <= 10  &&  unlen  &&  (c = *un); i++) {
		dn[i] = islower(c) ? toupper(c) : c;
		un++;
		unlen--;
	}
}

/*
 *  Get rid of these macros before someone discovers
 *  we are using such hideous things.
 */
#undef	isupper
#undef	islower
#undef	toupper
#undef	tolower
