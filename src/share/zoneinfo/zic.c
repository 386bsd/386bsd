/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Arthur David Olson of the National Cancer Institute.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)zic.c	5.3 (Berkeley) 4/20/91";
#endif /* not lint */

#ifdef notdef
static char	elsieid[] = "@(#)zic.c	4.12";
#endif

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/stat.h>
#include <time.h>
#include <tzfile.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif /* !defined TRUE */

struct rule {
	const char *	r_filename;
	int		r_linenum;
	const char *	r_name;

	int		r_loyear;	/* for example, 1986 */
	int		r_hiyear;	/* for example, 1986 */
	const char *	r_yrtype;

	int		r_month;	/* 0..11 */

	int		r_dycode;	/* see below */
	int		r_dayofmonth;
	int		r_wday;

	long		r_tod;		/* time from midnight */
	int		r_todisstd;	/* above is standard time if TRUE */
					/* or wall clock time if FALSE */
	long		r_stdoff;	/* offset from standard time */
	const char *	r_abbrvar;	/* variable part of abbreviation */

	int		r_todo;		/* a rule to do (used in outzone) */
	time_t		r_temp;		/* used in outzone */
};

/*
**	r_dycode		r_dayofmonth	r_wday
*/

#define DC_DOM		0	/* 1..31 */	/* unused */
#define DC_DOWGEQ	1	/* 1..31 */	/* 0..6 (Sun..Sat) */
#define DC_DOWLEQ	2	/* 1..31 */	/* 0..6 (Sun..Sat) */

struct zone {
	const char *	z_filename;
	int		z_linenum;

	const char *	z_name;
	long		z_gmtoff;
	const char *	z_rule;
	const char *	z_format;

	long		z_stdoff;

	struct rule *	z_rules;
	int		z_nrules;

	struct rule	z_untilrule;
	time_t		z_untiltime;
};

extern char *	icatalloc __P((char * old, const char * new));
extern char *	icpyalloc __P((const char * string));
extern void	ifree __P((char * p));
extern char *	imalloc __P((int n));
extern char *	irealloc __P((char * old, int n));
extern int	link __P((const char * fromname, const char * toname));
extern char *	optarg;
extern int	optind;
extern void	perror __P((const char * string));
extern char *	scheck __P((const char * string, const char * format));
static void	addtt __P((time_t starttime, int type));
static int	addtype
		    __P((long gmtoff, const char * abbr, int isdst,
		    int ttisstd));
static void	addleap __P((time_t t, int positive, int rolling));
static void	adjleap __P((void));
static void	associate __P((void));
static int	ciequal __P((const char * ap, const char * bp));
static void	convert __P((long val, char * buf));
static void	dolink __P((const char * fromfile, const char * tofile));
static void	eat __P((const char * name, int num));
static void	eats __P((const char * name, int num,
		    const char * rname, int rnum));
static long	eitol __P((int i));
static void	error __P((const char * message));
static char **	getfields __P((char * buf));
static long	gethms __P((char * string, const char * errstrng,
		    int signable));
static void	infile __P((const char * filename));
static void	inleap __P((char ** fields, int nfields));
static void	inlink __P((char ** fields, int nfields));
static void	inrule __P((char ** fields, int nfields));
static int	inzcont __P((char ** fields, int nfields));
static int	inzone __P((char ** fields, int nfields));
static int	inzsub __P((char ** fields, int nfields, int iscont));
static int	itsabbr __P((const char * abbr, const char * word));
static int	itsdir __P((const char * name));
static int	lowerit __P((int c));
static char *	memcheck __P((char * tocheck));
static int	mkdirs __P((char * filename));
static void	newabbr __P((const char * abbr));
static long	oadd __P((long t1, long t2));
static void	outzone __P((const struct zone * zp, int ntzones));
static void	puttzcode __P((long code, FILE * fp));
static int	rcomp __P((const void *leftp, const void *rightp));
static time_t	rpytime __P((const struct rule * rp, int wantedy));
static void	rulesub __P((struct rule * rp, char * loyearp, char * hiyearp,
		char * typep, char * monthp, char * dayp, char * timep));
static void	setboundaries __P((void));
static time_t	tadd __P((time_t t1, long t2));
static void	usage __P((void));
static void	writezone __P((const char * name));
static int	yearistype __P((int year, const char * type));

static int		charcnt;
static int		errors;
static const char *	filename;
static int		leapcnt;
static int		linenum;
static time_t		max_time;
static int		max_year;
static time_t		min_time;
static int		min_year;
static int		noise;
static const char *	rfilename;
static int		rlinenum;
static const char *	progname;
static int		timecnt;
static int		typecnt;
static int		tt_signed;

/*
** Line codes.
*/

#define LC_RULE		0
#define LC_ZONE		1
#define LC_LINK		2
#define LC_LEAP		3

/*
** Which fields are which on a Zone line.
*/

#define ZF_NAME		1
#define ZF_GMTOFF	2
#define ZF_RULE		3
#define ZF_FORMAT	4
#define ZF_TILYEAR	5
#define ZF_TILMONTH	6
#define ZF_TILDAY	7
#define ZF_TILTIME	8
#define ZONE_MINFIELDS	5
#define ZONE_MAXFIELDS	9

/*
** Which fields are which on a Zone continuation line.
*/

#define ZFC_GMTOFF	0
#define ZFC_RULE	1
#define ZFC_FORMAT	2
#define ZFC_TILYEAR	3
#define ZFC_TILMONTH	4
#define ZFC_TILDAY	5
#define ZFC_TILTIME	6
#define ZONEC_MINFIELDS	3
#define ZONEC_MAXFIELDS	7

/*
** Which files are which on a Rule line.
*/

#define RF_NAME		1
#define RF_LOYEAR	2
#define RF_HIYEAR	3
#define RF_COMMAND	4
#define RF_MONTH	5
#define RF_DAY		6
#define RF_TOD		7
#define RF_STDOFF	8
#define RF_ABBRVAR	9
#define RULE_FIELDS	10

/*
** Which fields are which on a Link line.
*/

#define LF_FROM		1
#define LF_TO		2
#define LINK_FIELDS	3

/*
** Which fields are which on a Leap line.
*/

#define LP_YEAR		1
#define LP_MONTH	2
#define LP_DAY		3
#define LP_TIME		4
#define LP_CORR		5
#define LP_ROLL		6
#define LEAP_FIELDS	7

/*
** Year synonyms.
*/

#define YR_MINIMUM	0
#define YR_MAXIMUM	1
#define YR_ONLY		2

static struct rule *	rules;
static int		nrules;	/* number of rules */

static struct zone *	zones;
static int		nzones;	/* number of zones */

struct link {
	const char *	l_filename;
	int		l_linenum;
	const char *	l_from;
	const char *	l_to;
};

static struct link *	links;
static int		nlinks;

struct lookup {
	const char *	l_word;
	const int	l_value;
};

static struct lookup const *	byword __P((const char * string,
					const struct lookup * lp));

static struct lookup const	line_codes[] = {
	"Rule",		LC_RULE,
	"Zone",		LC_ZONE,
	"Link",		LC_LINK,
	"Leap",		LC_LEAP,
	NULL,		0
};

static struct lookup const	mon_names[] = {
	"January",	TM_JANUARY,
	"February",	TM_FEBRUARY,
	"March",	TM_MARCH,
	"April",	TM_APRIL,
	"May",		TM_MAY,
	"June",		TM_JUNE,
	"July",		TM_JULY,
	"August",	TM_AUGUST,
	"September",	TM_SEPTEMBER,
	"October",	TM_OCTOBER,
	"November",	TM_NOVEMBER,
	"December",	TM_DECEMBER,
	NULL,		0
};

static struct lookup const	wday_names[] = {
	"Sunday",	TM_SUNDAY,
	"Monday",	TM_MONDAY,
	"Tuesday",	TM_TUESDAY,
	"Wednesday",	TM_WEDNESDAY,
	"Thursday",	TM_THURSDAY,
	"Friday",	TM_FRIDAY,
	"Saturday",	TM_SATURDAY,
	NULL,		0
};

static struct lookup const	lasts[] = {
	"last-Sunday",		TM_SUNDAY,
	"last-Monday",		TM_MONDAY,
	"last-Tuesday",		TM_TUESDAY,
	"last-Wednesday",	TM_WEDNESDAY,
	"last-Thursday",	TM_THURSDAY,
	"last-Friday",		TM_FRIDAY,
	"last-Saturday",	TM_SATURDAY,
	NULL,			0
};

static struct lookup const	begin_years[] = {
	"minimum",		YR_MINIMUM,
	"maximum",		YR_MAXIMUM,
	NULL,			0
};

static struct lookup const	end_years[] = {
	"minimum",		YR_MINIMUM,
	"maximum",		YR_MAXIMUM,
	"only",			YR_ONLY,
	NULL,			0
};

static struct lookup const	leap_types[] = {
	"Rolling",		TRUE,
	"Stationary",		FALSE,
	NULL,			0
};

static const int	len_months[2][MONSPERYEAR] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static const int	len_years[2] = {
	DAYSPERNYEAR, DAYSPERLYEAR
};

static time_t		ats[TZ_MAX_TIMES];
static unsigned char	types[TZ_MAX_TIMES];
static long		gmtoffs[TZ_MAX_TYPES];
static char		isdsts[TZ_MAX_TYPES];
static char		abbrinds[TZ_MAX_TYPES];
static char		ttisstds[TZ_MAX_TYPES];
static char		chars[TZ_MAX_CHARS];
static time_t		trans[TZ_MAX_LEAPS];
static long		corr[TZ_MAX_LEAPS];
static char		roll[TZ_MAX_LEAPS];

/*
** Memory allocation.
*/

static char *
memcheck(ptr)
char * const	ptr;
{
	if (ptr == NULL) {
		(void) perror(progname);
		(void) exit(EXIT_FAILURE);
	}
	return ptr;
}

#define emalloc(size)		memcheck(imalloc(size))
#define erealloc(ptr, size)	memcheck(irealloc(ptr, size))
#define ecpyalloc(ptr)		memcheck(icpyalloc(ptr))
#define ecatalloc(oldp, newp)	memcheck(icatalloc(oldp, newp))

/*
** Error handling.
*/

static void
eats(name, num, rname, rnum)
const char * const	name;
const int		num;
const char * const	rname;
const int		rnum;
{
	filename = name;
	linenum = num;
	rfilename = rname;
	rlinenum = rnum;
}

static void
eat(name, num)
const char * const	name;
const int		num;
{
	eats(name, num, (char *) NULL, -1);
}

static void
error(string)
const char * const	string;
{
	/*
	** Match the format of "cc" to allow sh users to
	** 	zic ... 2>&1 | error -t "*" -v
	** on BSD systems.
	*/
	(void) fprintf(stderr, "\"%s\", line %d: %s",
		filename, linenum, string);
	if (rfilename != NULL)
		(void) fprintf(stderr, " (rule from \"%s\", line %d)",
			rfilename, rlinenum);
	(void) fprintf(stderr, "\n");
	++errors;
}

static void
usage()
{
	(void) fprintf(stderr,
"%s: usage is %s [ -s ] [ -v ] [ -l localtime ] [ -p posixrules ] [ -d directory ]\n\
\t[ -L leapseconds ] [ filename ... ]\n",
		progname, progname);
	(void) exit(EXIT_FAILURE);
}

static const char *	psxrules = NULL;
static const char *	lcltime = NULL;
static const char *	directory = NULL;
static const char *	leapsec = NULL;
static int		sflag = FALSE;

int
main(argc, argv)
int	argc;
char *	argv[];
{
	register int	i, j;
	register int	c;

	(void) umask(umask(022) | 022);
	progname = argv[0];
	while ((c = getopt(argc, argv, "d:l:p:L:vs")) != EOF)
		switch (c) {
			default:
				usage();
			case 'd':
				if (directory == NULL)
					directory = optarg;
				else {
					(void) fprintf(stderr,
"%s: More than one -d option specified\n",
						progname);
					(void) exit(EXIT_FAILURE);
				}
				break;
			case 'l':
				if (lcltime == NULL)
					lcltime = optarg;
				else {
					(void) fprintf(stderr,
"%s: More than one -l option specified\n",
						progname);
					(void) exit(EXIT_FAILURE);
				}
				break;
			case 'p':
				if (psxrules == NULL)
					psxrules = optarg;
				else {
					(void) fprintf(stderr,
"%s: More than one -p option specified\n",
						progname);
					(void) exit(EXIT_FAILURE);
				}
				break;
			case 'L':
				if (leapsec == NULL)
					leapsec = optarg;
				else {
					(void) fprintf(stderr,
"%s: More than one -L option specified\n",
						progname);
					(void) exit(EXIT_FAILURE);
				}
				break;
			case 'v':
				noise = TRUE;
				break;
			case 's':
				sflag = TRUE;
				break;
		}
	if (optind == argc - 1 && strcmp(argv[optind], "=") == 0)
		usage();	/* usage message by request */
	if (directory == NULL)
		directory = TZDIR;

	setboundaries();

	if (optind < argc && leapsec != NULL) {
		infile(leapsec);
		adjleap();
	}

	zones = (struct zone *) emalloc(0);
	rules = (struct rule *) emalloc(0);
	links = (struct link *) emalloc(0);
	for (i = optind; i < argc; ++i)
		infile(argv[i]);
	if (errors)
		(void) exit(EXIT_FAILURE);
	associate();
	for (i = 0; i < nzones; i = j) {
		/*
		** Find the next non-continuation zone entry.
		*/
		for (j = i + 1; j < nzones && zones[j].z_name == NULL; ++j)
			;
		outzone(&zones[i], j - i);
	}
	/*
	** Make links.
	*/
	for (i = 0; i < nlinks; ++i)
		dolink(links[i].l_from, links[i].l_to);
	if (lcltime != NULL)
		dolink(lcltime, TZDEFAULT);
	if (psxrules != NULL)
		dolink(psxrules, TZDEFRULES);
	return (errors == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void
dolink(fromfile, tofile)
const char * const	fromfile;
const char * const	tofile;
{
	register char *	fromname;
	register char *	toname;

	fromname = ecpyalloc(directory);
	fromname = ecatalloc(fromname, "/");
	fromname = ecatalloc(fromname, fromfile);
	toname = ecpyalloc(directory);
	toname = ecatalloc(toname, "/");
	toname = ecatalloc(toname, tofile);
	/*
	** We get to be careful here since
	** there's a fair chance of root running us.
	*/
	if (!itsdir(toname))
		(void) remove(toname);
	if (link(fromname, toname) != 0) {
		(void) fprintf(stderr, "%s: Can't link from %s to ",
			progname, fromname);
		(void) perror(toname);
		(void) exit(EXIT_FAILURE);
	}
	ifree(fromname);
	ifree(toname);
}

static void
setboundaries()
{
	register time_t	bit;

	for (bit = 1; bit > 0; bit <<= 1)
		;
	if (bit == 0) {		/* time_t is an unsigned type */
		tt_signed = FALSE;
		min_time = 0;
		max_time = ~(time_t) 0;
		if (sflag)
			max_time >>= 1;
	} else {
		tt_signed = TRUE;
		min_time = bit;
		max_time = bit;
		++max_time;
		max_time = -max_time;
		if (sflag)
			min_time = 0;
	}
	min_year = TM_YEAR_BASE + gmtime(&min_time)->tm_year;
	max_year = TM_YEAR_BASE + gmtime(&max_time)->tm_year;
}

static int
itsdir(name)
const char * const	name;
{
	struct stat	s;

	return (stat(name, &s) == 0 && S_ISDIR(s.st_mode));
}

/*
** Associate sets of rules with zones.
*/

/*
** Sort by rule name.
*/

static int
rcomp(cp1, cp2)
const void *	cp1;
const void *	cp2;
{
	return strcmp(((struct rule *) cp1)->r_name,
		((struct rule *) cp2)->r_name);
}

static void
associate()
{
	register struct zone *	zp;
	register struct rule *	rp;
	register int		base, out;
	register int		i;

	if (nrules != 0)
		(void) qsort((void *) rules, (size_t) nrules,
		     (size_t) sizeof *rules, rcomp);
	for (i = 0; i < nzones; ++i) {
		zp = &zones[i];
		zp->z_rules = NULL;
		zp->z_nrules = 0;
	}
	for (base = 0; base < nrules; base = out) {
		rp = &rules[base];
		for (out = base + 1; out < nrules; ++out)
			if (strcmp(rp->r_name, rules[out].r_name) != 0)
				break;
		for (i = 0; i < nzones; ++i) {
			zp = &zones[i];
			if (strcmp(zp->z_rule, rp->r_name) != 0)
				continue;
			zp->z_rules = rp;
			zp->z_nrules = out - base;
		}
	}
	for (i = 0; i < nzones; ++i) {
		zp = &zones[i];
		if (zp->z_nrules == 0) {
			/*
			** Maybe we have a local standard time offset.
			*/
			eat(zp->z_filename, zp->z_linenum);
			zp->z_stdoff =
			    gethms((char *)zp->z_rule, "unruly zone", TRUE);
			/*
			** Note, though, that if there's no rule,
			** a '%s' in the format is a bad thing.
			*/
			if (strchr(zp->z_format, '%') != 0)
				error("%s in ruleless zone");
		}
	}
	if (errors)
		(void) exit(EXIT_FAILURE);
}

static void
infile(name)
const char *	name;
{
	register FILE *			fp;
	register char **		fields;
	register char *			cp;
	register const struct lookup *	lp;
	register int			nfields;
	register int			wantcont;
	register int			num;
	char				buf[BUFSIZ];

	if (strcmp(name, "-") == 0) {
		name = "standard input";
		fp = stdin;
	} else if ((fp = fopen(name, "r")) == NULL) {
		(void) fprintf(stderr, "%s: Can't open ", progname);
		(void) perror(name);
		(void) exit(EXIT_FAILURE);
	}
	wantcont = FALSE;
	for (num = 1; ; ++num) {
		eat(name, num);
		if (fgets(buf, (int) sizeof buf, fp) != buf)
			break;
		cp = strchr(buf, '\n');
		if (cp == NULL) {
			error("line too long");
			(void) exit(EXIT_FAILURE);
		}
		*cp = '\0';
		fields = getfields(buf);
		nfields = 0;
		while (fields[nfields] != NULL) {
			if (ciequal(fields[nfields], "-"))
				fields[nfields] = "";
			++nfields;
		}
		if (nfields == 0) {
			/* nothing to do */
		} else if (wantcont) {
			wantcont = inzcont(fields, nfields);
		} else {
			lp = byword(fields[0], line_codes);
			if (lp == NULL)
				error("input line of unknown type");
			else switch ((int) (lp->l_value)) {
				case LC_RULE:
					inrule(fields, nfields);
					wantcont = FALSE;
					break;
				case LC_ZONE:
					wantcont = inzone(fields, nfields);
					break;
				case LC_LINK:
					inlink(fields, nfields);
					wantcont = FALSE;
					break;
				case LC_LEAP:
					if (name != leapsec)
						(void) fprintf(stderr,
"%s: Leap line in non leap seconds file %s\n",
							progname, name);
					else	inleap(fields, nfields);
					wantcont = FALSE;
					break;
				default:	/* "cannot happen" */
					(void) fprintf(stderr,
"%s: panic: Invalid l_value %d\n",
						progname, lp->l_value);
					(void) exit(EXIT_FAILURE);
			}
		}
		ifree((char *) fields);
	}
	if (ferror(fp)) {
		(void) fprintf(stderr, "%s: Error reading ", progname);
		(void) perror(filename);
		(void) exit(EXIT_FAILURE);
	}
	if (fp != stdin && fclose(fp)) {
		(void) fprintf(stderr, "%s: Error closing ", progname);
		(void) perror(filename);
		(void) exit(EXIT_FAILURE);
	}
	if (wantcont)
		error("expected continuation line not found");
}

/*
** Convert a string of one of the forms
**	h	-h 	hh:mm	-hh:mm	hh:mm:ss	-hh:mm:ss
** into a number of seconds.
** A null string maps to zero.
** Call error with errstring and return zero on errors.
*/

static long
gethms(string, errstring, signable)
char *		string;
const char * const	errstring;
const int		signable;
{
	int	hh, mm, ss, sign;

	if (string == NULL || *string == '\0')
		return 0;
	if (!signable)
		sign = 1;
	else if (*string == '-') {
		sign = -1;
		++string;
	} else	sign = 1;
	if (sscanf(string, scheck(string, "%d"), &hh) == 1)
		mm = ss = 0;
	else if (sscanf(string, scheck(string, "%d:%d"), &hh, &mm) == 2)
		ss = 0;
	else if (sscanf(string, scheck(string, "%d:%d:%d"),
		&hh, &mm, &ss) != 3) {
			error(errstring);
			return 0;
	}
	if (hh < 0 || hh >= HOURSPERDAY ||
		mm < 0 || mm >= MINSPERHOUR ||
		ss < 0 || ss > SECSPERMIN) {
			error(errstring);
			return 0;
	}
	return eitol(sign) *
		(eitol(hh * MINSPERHOUR + mm) *
		eitol(SECSPERMIN) + eitol(ss));
}

static void
inrule(fields, nfields)
register char ** const	fields;
const int		nfields;
{
	static struct rule	r;

	if (nfields != RULE_FIELDS) {
		error("wrong number of fields on Rule line");
		return;
	}
	if (*fields[RF_NAME] == '\0') {
		error("nameless rule");
		return;
	}
	r.r_filename = filename;
	r.r_linenum = linenum;
	r.r_stdoff = gethms(fields[RF_STDOFF], "invalid saved time", TRUE);
	rulesub(&r, fields[RF_LOYEAR], fields[RF_HIYEAR], fields[RF_COMMAND],
		fields[RF_MONTH], fields[RF_DAY], fields[RF_TOD]);
	r.r_name = ecpyalloc(fields[RF_NAME]);
	r.r_abbrvar = ecpyalloc(fields[RF_ABBRVAR]);
	rules = (struct rule *) erealloc((char *) rules,
		(int) ((nrules + 1) * sizeof *rules));
	rules[nrules++] = r;
}

static int
inzone(fields, nfields)
register char ** const	fields;
const int		nfields;
{
	register int	i;
	char		buf[132];

	if (nfields < ZONE_MINFIELDS || nfields > ZONE_MAXFIELDS) {
		error("wrong number of fields on Zone line");
		return FALSE;
	}
	if (strcmp(fields[ZF_NAME], TZDEFAULT) == 0 && lcltime != NULL) {
		(void) sprintf(buf,
			"\"Zone %s\" line and -l option are mutually exclusive",
			TZDEFAULT);
		error(buf);
		return FALSE;
	}
	if (strcmp(fields[ZF_NAME], TZDEFRULES) == 0 && psxrules != NULL) {
		(void) sprintf(buf,
			"\"Zone %s\" line and -p option are mutually exclusive",
			TZDEFRULES);
		error(buf);
		return FALSE;
	}
	for (i = 0; i < nzones; ++i)
		if (zones[i].z_name != NULL &&
			strcmp(zones[i].z_name, fields[ZF_NAME]) == 0) {
				(void) sprintf(buf,
"duplicate zone name %s (file \"%s\", line %d)",
					fields[ZF_NAME],
					zones[i].z_filename,
					zones[i].z_linenum);
				error(buf);
				return FALSE;
		}
	return inzsub(fields, nfields, FALSE);
}

static int
inzcont(fields, nfields)
register char ** const	fields;
const int		nfields;
{
	if (nfields < ZONEC_MINFIELDS || nfields > ZONEC_MAXFIELDS) {
		error("wrong number of fields on Zone continuation line");
		return FALSE;
	}
	return inzsub(fields, nfields, TRUE);
}

static int
inzsub(fields, nfields, iscont)
register char ** const	fields;
const int		nfields;
const int		iscont;
{
	register char *		cp;
	static struct zone	z;
	register int		i_gmtoff, i_rule, i_format;
	register int		i_untilyear, i_untilmonth;
	register int		i_untilday, i_untiltime;
	register int		hasuntil;

	if (iscont) {
		i_gmtoff = ZFC_GMTOFF;
		i_rule = ZFC_RULE;
		i_format = ZFC_FORMAT;
		i_untilyear = ZFC_TILYEAR;
		i_untilmonth = ZFC_TILMONTH;
		i_untilday = ZFC_TILDAY;
		i_untiltime = ZFC_TILTIME;
		z.z_name = NULL;
	} else {
		i_gmtoff = ZF_GMTOFF;
		i_rule = ZF_RULE;
		i_format = ZF_FORMAT;
		i_untilyear = ZF_TILYEAR;
		i_untilmonth = ZF_TILMONTH;
		i_untilday = ZF_TILDAY;
		i_untiltime = ZF_TILTIME;
		z.z_name = ecpyalloc(fields[ZF_NAME]);
	}
	z.z_filename = filename;
	z.z_linenum = linenum;
	z.z_gmtoff = gethms(fields[i_gmtoff], "invalid GMT offset", TRUE);
	if ((cp = strchr(fields[i_format], '%')) != 0) {
		if (*++cp != 's' || strchr(cp, '%') != 0) {
			error("invalid abbreviation format");
			return FALSE;
		}
	}
	z.z_rule = ecpyalloc(fields[i_rule]);
	z.z_format = ecpyalloc(fields[i_format]);
	hasuntil = nfields > i_untilyear;
	if (hasuntil) {
		z.z_untilrule.r_filename = filename;
		z.z_untilrule.r_linenum = linenum;
		rulesub(&z.z_untilrule,
			fields[i_untilyear],
			"only",
			"",
			(nfields > i_untilmonth) ? fields[i_untilmonth] : "Jan",
			(nfields > i_untilday) ? fields[i_untilday] : "1",
			(nfields > i_untiltime) ? fields[i_untiltime] : "0");
		z.z_untiltime = rpytime(&z.z_untilrule, z.z_untilrule.r_loyear);
		if (iscont && nzones > 0 && z.z_untiltime < max_time &&
			z.z_untiltime > min_time &&
			zones[nzones - 1].z_untiltime >= z.z_untiltime) {
error("Zone continuation line end time is not after end time of previous line");
			return FALSE;
		}
	}
	zones = (struct zone *) erealloc((char *) zones,
		(int) ((nzones + 1) * sizeof *zones));
	zones[nzones++] = z;
	/*
	** If there was an UNTIL field on this line,
	** there's more information about the zone on the next line.
	*/
	return hasuntil;
}

static void
inleap(fields, nfields)
register char ** const	fields;
const int		nfields;
{
	register const char *		cp;
	register const struct lookup *	lp;
	register int			i, j;
	int				year, month, day;
	long				dayoff, tod;
	time_t				t;

	if (nfields != LEAP_FIELDS) {
		error("wrong number of fields on Leap line");
		return;
	}
	dayoff = 0;
	cp = fields[LP_YEAR];
	if (sscanf((char *)cp, scheck(cp, "%d"), &year) != 1 ||
		year < min_year || year > max_year) {
			/*
			 * Leapin' Lizards!
			 */
			error("invalid leaping year");
			return;
	}
	j = EPOCH_YEAR;
	while (j != year) {
		if (year > j) {
			i = len_years[isleap(j)];
			++j;
		} else {
			--j;
			i = -len_years[isleap(j)];
		}
		dayoff = oadd(dayoff, eitol(i));
	}
	if ((lp = byword(fields[LP_MONTH], mon_names)) == NULL) {
		error("invalid month name");
		return;
	}
	month = lp->l_value;
	j = TM_JANUARY;
	while (j != month) {
		i = len_months[isleap(year)][j];
		dayoff = oadd(dayoff, eitol(i));
		++j;
	}
	cp = fields[LP_DAY];
	if (sscanf((char *)cp, scheck(cp, "%d"), &day) != 1 ||
		day <= 0 || day > len_months[isleap(year)][month]) {
			error("invalid day of month");
			return;
	}
	dayoff = oadd(dayoff, eitol(day - 1));
	if (dayoff < 0 && !tt_signed) {
		error("time before zero");
		return;
	}
	t = (time_t) dayoff * SECSPERDAY;
	/*
	** Cheap overflow check.
	*/
	if (t / SECSPERDAY != dayoff) {
		error("time overflow");
		return;
	}
	tod = gethms(fields[LP_TIME], "invalid time of day", FALSE);
	cp = fields[LP_CORR];
	if (strcmp(cp, "+") != 0 && strcmp(cp, "") != 0) {
		/* infile() turned "-" into "" */
		error("illegal CORRECTION field on Leap line");
		return;
	}
	if ((lp = byword(fields[LP_ROLL], leap_types)) == NULL) {
		error("illegal Rolling/Stationary field on Leap line");
		return;
	}
	addleap(tadd(t, tod), *cp == '+', lp->l_value);
}

static void
inlink(fields, nfields)
register char ** const	fields;
const int		nfields;
{
	struct link	l;

	if (nfields != LINK_FIELDS) {
		error("wrong number of fields on Link line");
		return;
	}
	if (*fields[LF_FROM] == '\0') {
		error("blank FROM field on Link line");
		return;
	}
	if (*fields[LF_TO] == '\0') {
		error("blank TO field on Link line");
		return;
	}
	l.l_filename = filename;
	l.l_linenum = linenum;
	l.l_from = ecpyalloc(fields[LF_FROM]);
	l.l_to = ecpyalloc(fields[LF_TO]);
	links = (struct link *) erealloc((char *) links,
		(int) ((nlinks + 1) * sizeof *links));
	links[nlinks++] = l;
}

static void
rulesub(rp, loyearp, hiyearp, typep, monthp, dayp, timep)
register struct rule * const	rp;
char * const			loyearp;
char * const			hiyearp;
char * const			typep;
char * const			monthp;
char * const			dayp;
char * const			timep;
{
	register struct lookup const *	lp;
	register char *			cp;

	if ((lp = byword(monthp, mon_names)) == NULL) {
		error("invalid month name");
		return;
	}
	rp->r_month = lp->l_value;
	rp->r_todisstd = FALSE;
	cp = timep;
	if (*cp != '\0') {
		cp += strlen(cp) - 1;
		switch (lowerit(*cp)) {
			case 's':
				rp->r_todisstd = TRUE;
				*cp = '\0';
				break;
			case 'w':
				rp->r_todisstd = FALSE;
				*cp = '\0';
				break;
		}
	}
	rp->r_tod = gethms(timep, "invalid time of day", FALSE);
	/*
	** Year work.
	*/
	cp = loyearp;
	if ((lp = byword(cp, begin_years)) != NULL) switch ((int) lp->l_value) {
		case YR_MINIMUM:
			rp->r_loyear = min_year;
			break;
		case YR_MAXIMUM:
			rp->r_loyear = max_year;
			break;
		default:	/* "cannot happen" */
			(void) fprintf(stderr,
				"%s: panic: Invalid l_value %d\n",
				progname, lp->l_value);
			(void) exit(EXIT_FAILURE);
	} else if (sscanf(cp, scheck(cp, "%d"), &rp->r_loyear) != 1 ||
		rp->r_loyear < min_year || rp->r_loyear > max_year) {
			if (noise)
				error("invalid starting year");
			if (rp->r_loyear > max_year)
				return;
	}
	cp = hiyearp;
	if ((lp = byword(cp, end_years)) != NULL) switch ((int) lp->l_value) {
		case YR_MINIMUM:
			rp->r_hiyear = min_year;
			break;
		case YR_MAXIMUM:
			rp->r_hiyear = max_year;
			break;
		case YR_ONLY:
			rp->r_hiyear = rp->r_loyear;
			break;
		default:	/* "cannot happen" */
			(void) fprintf(stderr,
				"%s: panic: Invalid l_value %d\n",
				progname, lp->l_value);
			(void) exit(EXIT_FAILURE);
	} else if (sscanf(cp, scheck(cp, "%d"), &rp->r_hiyear) != 1 ||
		rp->r_hiyear < min_year || rp->r_hiyear > max_year) {
			if (noise)
				error("invalid ending year");
			if (rp->r_hiyear < min_year)
				return;
	}
	if (rp->r_hiyear < min_year)
 		return;
 	if (rp->r_loyear < min_year)
 		rp->r_loyear = min_year;
 	if (rp->r_hiyear > max_year)
 		rp->r_hiyear = max_year;
	if (rp->r_loyear > rp->r_hiyear) {
		error("starting year greater than ending year");
		return;
	}
	if (*typep == '\0')
		rp->r_yrtype = NULL;
	else {
		if (rp->r_loyear == rp->r_hiyear) {
			error("typed single year");
			return;
		}
		rp->r_yrtype = ecpyalloc(typep);
	}
	/*
	** Day work.
	** Accept things such as:
	**	1
	**	last-Sunday
	**	Sun<=20
	**	Sun>=7
	*/
	if ((lp = byword(dayp, lasts)) != NULL) {
		rp->r_dycode = DC_DOWLEQ;
		rp->r_wday = lp->l_value;
		rp->r_dayofmonth = len_months[1][rp->r_month];
	} else {
		if ((cp = strchr(dayp, '<')) != 0)
			rp->r_dycode = DC_DOWLEQ;
		else if ((cp = strchr(dayp, '>')) != 0)
			rp->r_dycode = DC_DOWGEQ;
		else {
			cp = dayp;
			rp->r_dycode = DC_DOM;
		}
		if (rp->r_dycode != DC_DOM) {
			*cp++ = 0;
			if (*cp++ != '=') {
				error("invalid day of month");
				return;
			}
			if ((lp = byword(dayp, wday_names)) == NULL) {
				error("invalid weekday name");
				return;
			}
			rp->r_wday = lp->l_value;
		}
		if (sscanf(cp, scheck(cp, "%d"), &rp->r_dayofmonth) != 1 ||
			rp->r_dayofmonth <= 0 ||
			(rp->r_dayofmonth > len_months[1][rp->r_month])) {
				error("invalid day of month");
				return;
		}
	}
}

static void
convert(val, buf)
const long	val;
char * const	buf;
{
	register int	i;
	register long	shift;

	for (i = 0, shift = 24; i < 4; ++i, shift -= 8)
		buf[i] = val >> shift;
}

static void
puttzcode(val, fp)
const long	val;
FILE * const	fp;
{
	char	buf[4];

	convert(val, buf);
	(void) fwrite((void *) buf, (size_t) sizeof buf, (size_t) 1, fp);
}

static void
writezone(name)
const char * const	name;
{
	register FILE *		fp;
	register int		i, j;
	char			fullname[BUFSIZ];
	static struct tzhead	tzh;

	if (strlen(directory) + 1 + strlen(name) >= sizeof fullname) {
		(void) fprintf(stderr,
			"%s: File name %s/%s too long\n", progname,
			directory, name);
		(void) exit(EXIT_FAILURE);
	}
	(void) sprintf(fullname, "%s/%s", directory, name);
	if ((fp = fopen(fullname, "wb")) == NULL) {
		if (mkdirs(fullname) != 0)
			(void) exit(EXIT_FAILURE);
		if ((fp = fopen(fullname, "wb")) == NULL) {
			(void) fprintf(stderr, "%s: Can't create ", progname);
			(void) perror(fullname);
			(void) exit(EXIT_FAILURE);
		}
	}
	convert(eitol(typecnt), tzh.tzh_ttisstdcnt);
	convert(eitol(leapcnt), tzh.tzh_leapcnt);
	convert(eitol(timecnt), tzh.tzh_timecnt);
	convert(eitol(typecnt), tzh.tzh_typecnt);
	convert(eitol(charcnt), tzh.tzh_charcnt);
	(void) fwrite((void *) &tzh, (size_t) sizeof tzh, (size_t) 1, fp);
	for (i = 0; i < timecnt; ++i) {
		j = leapcnt;
		while (--j >= 0)
			if (ats[i] >= trans[j]) {
				ats[i] = tadd(ats[i], corr[j]);
				break;
			}
		puttzcode((long) ats[i], fp);
	}
	if (timecnt > 0)
		(void) fwrite((void *) types, (size_t) sizeof types[0],
		    (size_t) timecnt, fp);
	for (i = 0; i < typecnt; ++i) {
		puttzcode((long) gmtoffs[i], fp);
		(void) putc(isdsts[i], fp);
		(void) putc(abbrinds[i], fp);
	}
	if (charcnt != 0)
		(void) fwrite((void *) chars, (size_t) sizeof chars[0],
			(size_t) charcnt, fp);
	for (i = 0; i < leapcnt; ++i) {
		if (roll[i]) {
			if (timecnt == 0 || trans[i] < ats[0]) {
				j = 0;
				while (isdsts[j])
					if (++j >= typecnt) {
						j = 0;
						break;
					}
			} else {
				j = 1;
				while (j < timecnt && trans[i] >= ats[j])
					++j;
				j = types[j - 1];
			}
			puttzcode((long) tadd(trans[i], -gmtoffs[j]), fp);
		} else	puttzcode((long) trans[i], fp);
		puttzcode((long) corr[i], fp);
	}
	for (i = 0; i < typecnt; ++i)
		(void) putc(ttisstds[i], fp);
	if (ferror(fp) || fclose(fp)) {
		(void) fprintf(stderr, "%s: Write error on ", progname);
		(void) perror(fullname);
		(void) exit(EXIT_FAILURE);
	}
}

static void
outzone(zpfirst, zonecount)
const struct zone * const	zpfirst;
const int			zonecount;
{
	register const struct zone *	zp;
	register struct rule *		rp;
	register int			i, j;
	register int			usestart, useuntil;
	register time_t			starttime, untiltime;
	register long			gmtoff;
	register long			stdoff;
	register int			year;
	register long			startoff;
	register int			startisdst;
	register int			startttisstd;
	register int			type;
	char				startbuf[BUFSIZ];

	/*
	** Now. . .finally. . .generate some useful data!
	*/
	timecnt = 0;
	typecnt = 0;
	charcnt = 0;
	/*
	** Two guesses. . .the second may well be corrected later.
	*/
	gmtoff = zpfirst->z_gmtoff;
	stdoff = 0;
#ifdef lint
	starttime = 0;
	startttisstd = FALSE;
#endif /* defined lint */
	for (i = 0; i < zonecount; ++i) {
		usestart = i > 0;
		useuntil = i < (zonecount - 1);
		zp = &zpfirst[i];
		eat(zp->z_filename, zp->z_linenum);
		startisdst = -1;
		if (zp->z_nrules == 0) {
			type = addtype(oadd(zp->z_gmtoff, zp->z_stdoff),
				zp->z_format, zp->z_stdoff != 0,
				startttisstd);
			if (usestart)
				addtt(starttime, type);
			gmtoff = zp->z_gmtoff;
			stdoff = zp->z_stdoff;
		} else for (year = min_year; year <= max_year; ++year) {
			if (useuntil && year > zp->z_untilrule.r_hiyear)
				break;
			/*
			** Mark which rules to do in the current year.
			** For those to do, calculate rpytime(rp, year);
			*/
			for (j = 0; j < zp->z_nrules; ++j) {
				rp = &zp->z_rules[j];
				eats(zp->z_filename, zp->z_linenum,
					rp->r_filename, rp->r_linenum);
				rp->r_todo = year >= rp->r_loyear &&
						year <= rp->r_hiyear &&
						yearistype(year, rp->r_yrtype);
				if (rp->r_todo)
					rp->r_temp = rpytime(rp, year);
			}
			for ( ; ; ) {
				register int	k;
				register time_t	jtime, ktime;
				register long	offset;
				char		buf[BUFSIZ];

				if (useuntil) {
					/*
					** Turn untiltime into GMT
					** assuming the current gmtoff and
					** stdoff values.
					*/
					offset = gmtoff;
					if (!zp->z_untilrule.r_todisstd)
						offset = oadd(offset, stdoff);
					untiltime = tadd(zp->z_untiltime,
						-offset);
				}
				/*
				** Find the rule (of those to do, if any)
				** that takes effect earliest in the year.
				*/
				k = -1;
#ifdef lint
				ktime = 0;
#endif /* defined lint */
				for (j = 0; j < zp->z_nrules; ++j) {
					rp = &zp->z_rules[j];
					if (!rp->r_todo)
						continue;
					eats(zp->z_filename, zp->z_linenum,
						rp->r_filename, rp->r_linenum);
					offset = gmtoff;
					if (!rp->r_todisstd)
						offset = oadd(offset, stdoff);
					jtime = rp->r_temp;
					if (jtime == min_time ||
						jtime == max_time)
							continue;
					jtime = tadd(jtime, -offset);
					if (k < 0 || jtime < ktime) {
						k = j;
						ktime = jtime;
					}
				}
				if (k < 0)
					break;	/* go on to next year */
				rp = &zp->z_rules[k];
				rp->r_todo = FALSE;
				if (useuntil && ktime >= untiltime)
					break;
				if (usestart) {
					if (ktime < starttime) {
						stdoff = rp->r_stdoff;
						startoff = oadd(zp->z_gmtoff,
							rp->r_stdoff);
						(void) sprintf(startbuf,
							zp->z_format,
							rp->r_abbrvar);
						startisdst =
							rp->r_stdoff != 0;
						continue;
					}
					if (ktime != starttime &&
						startisdst >= 0)
addtt(starttime, addtype(startoff, startbuf, startisdst, startttisstd));
					usestart = FALSE;
				}
				eats(zp->z_filename, zp->z_linenum,
					rp->r_filename, rp->r_linenum);
				(void) sprintf(buf, zp->z_format,
					rp->r_abbrvar);
				offset = oadd(zp->z_gmtoff, rp->r_stdoff);
				type = addtype(offset, buf, rp->r_stdoff != 0,
					rp->r_todisstd);
				if (timecnt != 0 || rp->r_stdoff != 0)
					addtt(ktime, type);
				gmtoff = zp->z_gmtoff;
				stdoff = rp->r_stdoff;
			}
		}
		/*
		** Now we may get to set starttime for the next zone line.
		*/
		if (useuntil) {
			starttime = tadd(zp->z_untiltime,
				-gmtoffs[types[timecnt - 1]]);
			startttisstd = zp->z_untilrule.r_todisstd;
		}
	}
	writezone(zpfirst->z_name);
}

static void
addtt(starttime, type)
const time_t	starttime;
const int	type;
{
	if (timecnt != 0 && type == types[timecnt - 1])
		return;	/* easy enough! */
	if (timecnt >= TZ_MAX_TIMES) {
		error("too many transitions?!");
		(void) exit(EXIT_FAILURE);
	}
	ats[timecnt] = starttime;
	types[timecnt] = type;
	++timecnt;
}

static int
addtype(gmtoff, abbr, isdst, ttisstd)
const long		gmtoff;
const char * const	abbr;
const int		isdst;
const int		ttisstd;
{
	register int	i, j;

	/*
	** See if there's already an entry for this zone type.
	** If so, just return its index.
	*/
	for (i = 0; i < typecnt; ++i) {
		if (gmtoff == gmtoffs[i] && isdst == isdsts[i] &&
			strcmp(abbr, &chars[abbrinds[i]]) == 0 &&
			ttisstd == ttisstds[i])
				return i;
	}
	/*
	** There isn't one; add a new one, unless there are already too
	** many.
	*/
	if (typecnt >= TZ_MAX_TYPES) {
		error("too many local time types");
		(void) exit(EXIT_FAILURE);
	}
	gmtoffs[i] = gmtoff;
	isdsts[i] = isdst;
	ttisstds[i] = ttisstd;

	for (j = 0; j < charcnt; ++j)
		if (strcmp(&chars[j], abbr) == 0)
			break;
	if (j == charcnt)
		newabbr(abbr);
	abbrinds[i] = j;
	++typecnt;
	return i;
}

static void
addleap(t, positive, rolling)
const time_t	t;
const int	positive;
const int	rolling;
{
	register int	i, j;

	if (leapcnt >= TZ_MAX_LEAPS) {
		error("too many leap seconds");
		(void) exit(EXIT_FAILURE);
	}
	for (i = 0; i < leapcnt; ++i)
		if (t <= trans[i]) {
			if (t == trans[i]) {
				error("repeated leap second moment");
				(void) exit(EXIT_FAILURE);
			}
			break;
		}
	for (j = leapcnt; j > i; --j) {
		trans[j] = trans[j-1];
		corr[j] = corr[j-1];
		roll[j] = roll[j-1];
	}
	trans[i] = t;
	corr[i] = (positive ? 1L : -1L);
	roll[i] = rolling;
	++leapcnt;
}

static void
adjleap()
{
	register int	i;
	register long	last = 0;

	/*
	** propagate leap seconds forward
	*/
	for (i = 0; i < leapcnt; ++i) {
		trans[i] = tadd(trans[i], last);
		last = corr[i] += last;
	}
}

static int
yearistype(year, type)
const int		year;
const char * const	type;
{
	char	buf[BUFSIZ];
	int	result;

	if (type == NULL || *type == '\0')
		return TRUE;
	if (strcmp(type, "uspres") == 0)
		return (year % 4) == 0;
	if (strcmp(type, "nonpres") == 0)
		return (year % 4) != 0;
	(void) sprintf(buf, "yearistype %d %s", year, type);
	result = system(buf);
	if (result == 0)
		return TRUE;
	if (result == (1 << 8))
		return FALSE;
	error("Wild result from command execution");
	(void) fprintf(stderr, "%s: command was '%s', result was %d\n",
		progname, buf, result);
	for ( ; ; )
		(void) exit(EXIT_FAILURE);
}

static int
lowerit(a)
const int	a;
{
	return (isascii(a) && isupper(a)) ? tolower(a) : a;
}

static int
ciequal(ap, bp)		/* case-insensitive equality */
register const char *	ap;
register const char *	bp;
{
	while (lowerit(*ap) == lowerit(*bp++))
		if (*ap++ == '\0')
			return TRUE;
	return FALSE;
}

static int
itsabbr(abbr, word)
register const char *	abbr;
register const char *	word;
{
	if (lowerit(*abbr) != lowerit(*word))
		return FALSE;
	++word;
	while (*++abbr != '\0')
		do if (*word == '\0')
			return FALSE;
				while (lowerit(*word++) != lowerit(*abbr));
	return TRUE;
}

static const struct lookup *
byword(word, table)
register const char * const		word;
register const struct lookup * const	table;
{
	register const struct lookup *	foundlp;
	register const struct lookup *	lp;

	if (word == NULL || table == NULL)
		return NULL;
	/*
	** Look for exact match.
	*/
	for (lp = table; lp->l_word != NULL; ++lp)
		if (ciequal(word, lp->l_word))
			return lp;
	/*
	** Look for inexact match.
	*/
	foundlp = NULL;
	for (lp = table; lp->l_word != NULL; ++lp)
		if (itsabbr(word, lp->l_word))
			if (foundlp == NULL)
				foundlp = lp;
			else	return NULL;	/* multiple inexact matches */
	return foundlp;
}

static char **
getfields(cp)
register char *	cp;
{
	register char *		dp;
	register char **	array;
	register int		nsubs;

	if (cp == NULL)
		return NULL;
	array = (char **) emalloc((int) ((strlen(cp) + 1) * sizeof *array));
	nsubs = 0;
	for ( ; ; ) {
		while (isascii(*cp) && isspace(*cp))
			++cp;
		if (*cp == '\0' || *cp == '#')
			break;
		array[nsubs++] = dp = cp;
		do {
			if ((*dp = *cp++) != '"')
				++dp;
			else while ((*dp = *cp++) != '"')
				if (*dp != '\0')
					++dp;
				else	error("Odd number of quotation marks");
		} while (*cp != '\0' && *cp != '#' &&
			(!isascii(*cp) || !isspace(*cp)));
		if (isascii(*cp) && isspace(*cp))
			++cp;
		*dp = '\0';
	}
	array[nsubs] = NULL;
	return array;
}

static long
oadd(t1, t2)
const long	t1;
const long	t2;
{
	register long	t;

	t = t1 + t2;
	if (t2 > 0 && t <= t1 || t2 < 0 && t >= t1) {
		error("time overflow");
		(void) exit(EXIT_FAILURE);
	}
	return t;
}

static time_t
tadd(t1, t2)
const time_t	t1;
const long	t2;
{
	register time_t	t;

	if (t1 == max_time && t2 > 0)
		return max_time;
	if (t1 == min_time && t2 < 0)
		return min_time;
	t = t1 + t2;
	if (t2 > 0 && t <= t1 || t2 < 0 && t >= t1) {
		error("time overflow");
		(void) exit(EXIT_FAILURE);
	}
	return t;
}

/*
** Given a rule, and a year, compute the date - in seconds since January 1,
** 1970, 00:00 LOCAL time - in that year that the rule refers to.
*/

static time_t
rpytime(rp, wantedy)
register const struct rule * const	rp;
register const int			wantedy;
{
	register int	y, m, i;
	register long	dayoff;			/* with a nod to Margaret O. */
	register time_t	t;

	dayoff = 0;
	m = TM_JANUARY;
	y = EPOCH_YEAR;
	while (wantedy != y) {
		if (wantedy > y) {
			i = len_years[isleap(y)];
			++y;
		} else {
			--y;
			i = -len_years[isleap(y)];
		}
		dayoff = oadd(dayoff, eitol(i));
	}
	while (m != rp->r_month) {
		i = len_months[isleap(y)][m];
		dayoff = oadd(dayoff, eitol(i));
		++m;
	}
	i = rp->r_dayofmonth;
	if (m == TM_FEBRUARY && i == 29 && !isleap(y)) {
		if (rp->r_dycode == DC_DOWLEQ)
			--i;
		else {
			error("use of 2/29 in non leap-year");
			(void) exit(EXIT_FAILURE);
		}
	}
	--i;
	dayoff = oadd(dayoff, eitol(i));
	if (rp->r_dycode == DC_DOWGEQ || rp->r_dycode == DC_DOWLEQ) {
		register long	wday;

#define LDAYSPERWEEK	((long) DAYSPERWEEK)
		wday = eitol(EPOCH_WDAY);
		/*
		** Don't trust mod of negative numbers.
		*/
		if (dayoff >= 0)
			wday = (wday + dayoff) % LDAYSPERWEEK;
		else {
			wday -= ((-dayoff) % LDAYSPERWEEK);
			if (wday < 0)
				wday += LDAYSPERWEEK;
		}
		while (wday != eitol(rp->r_wday))
			if (rp->r_dycode == DC_DOWGEQ) {
				dayoff = oadd(dayoff, (long) 1);
				if (++wday >= LDAYSPERWEEK)
					wday = 0;
				++i;
			} else {
				dayoff = oadd(dayoff, (long) -1);
				if (--wday < 0)
					wday = LDAYSPERWEEK;
				--i;
			}
		if (i < 0 || i >= len_months[isleap(y)][m]) {
			error("no day in month matches rule");
			(void) exit(EXIT_FAILURE);
		}
	}
	if (dayoff < 0 && !tt_signed) {
		if (wantedy == rp->r_loyear)
			return min_time;
		error("time before zero");
		(void) exit(EXIT_FAILURE);
	}
	t = (time_t) dayoff * SECSPERDAY;
	/*
	** Cheap overflow check.
	*/
	if (t / SECSPERDAY != dayoff) {
		if (wantedy == rp->r_hiyear)
			return max_time;
		if (wantedy == rp->r_loyear)
			return min_time;
		error("time overflow");
		(void) exit(EXIT_FAILURE);
	}
	return tadd(t, rp->r_tod);
}

static void
newabbr(string)
const char * const	string;
{
	register int	i;

	i = strlen(string) + 1;
	if (charcnt + i >= TZ_MAX_CHARS) {
		error("too many, or too long, time zone abbreviations");
		(void) exit(EXIT_FAILURE);
	}
	(void) strcpy(&chars[charcnt], string);
	charcnt += eitol(i);
}

static int
mkdirs(name)
char * const	name;
{
	register char *	cp;

	if ((cp = name) == NULL || *cp == '\0')
		return 0;
	while ((cp = strchr(cp + 1, '/')) != 0) {
		*cp = '\0';
		if (!itsdir(name)) {
			/*
			** It doesn't seem to exist, so we try to create it.
			*/
			if (mkdir(name, 0755) != 0) {
				(void) fprintf(stderr,
					"%s: Can't create directory ",
					progname);
				(void) perror(name);
				return -1;
			}
		}
		*cp = '/';
	}
	return 0;
}

static long
eitol(i)
const int	i;
{
	long	l;

	l = i;
	if (i < 0 && l >= 0 || i == 0 && l != 0 || i > 0 && l <= 0) {
		(void) fprintf(stderr, "%s: %d did not sign extend correctly\n",
			progname, i);
		(void) exit(EXIT_FAILURE);
	}
	return l;
}

/*
** UNIX is a registered trademark of AT&T.
*/
