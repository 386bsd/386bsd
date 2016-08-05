/* $Header: /home/hyperion/mu/christos/src/sys/tcsh-6.01/RCS/sh.file.c,v 3.3 1991/12/19 22:34:14 christos Exp $ */
/*
 * sh.file.c: File completion for csh. This file is not used in tcsh.
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
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
#ifdef FILEC
#include "sh.h"

RCSID("$Id: sh.file.c,v 3.3 1991/12/19 22:34:14 christos Exp $")

/*
 * Tenex style file name recognition, .. and more.
 * History:
 *	Author: Ken Greer, Sept. 1975, CMU.
 *	Finally got around to adding to the Cshell., Ken Greer, Dec. 1981.
 */

#define ON	1
#define OFF	0
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define ESC	'\033'

typedef enum {
    LIST, RECOGNIZE
}       COMMAND;

static	void	 setup_tty		__P((int));
static	void	 back_to_col_1		__P((void));
static	void	 pushback		__P((Char *));
static	void	 catn			__P((Char *, Char *, int));
static	void	 copyn			__P((Char *, Char *, int));
static	Char	 filetype		__P((Char *, Char *));
static	void	 print_by_column	__P((Char *, Char *[], int));
static	Char 	*tilde			__P((Char *, Char *));
static	void	 retype			__P((void));
static	void	 beep			__P((void));
static	void 	 print_recognized_stuff	__P((Char *));
static	void	 extract_dir_and_name	__P((Char *, Char *, Char *));
static	Char	*getentry		__P((DIR *, int));
static	void	 free_items		__P((Char **));
static	int	 tsearch		__P((Char *, COMMAND, int));
static	int	 recognize		__P((Char *, Char *, int, int));
static	int	 is_prefix		__P((Char *, Char *));
static	int	 is_suffix		__P((Char *, Char *));
static	int	 ignored		__P((Char *));


/*
 * Put this here so the binary can be patched with adb to enable file
 * completion by default.  Filec controls completion, nobeep controls
 * ringing the terminal bell on incomplete expansions.
 */
bool    filec = 0;

static void
setup_tty(on)
    int     on;
{
#ifdef TERMIO
# ifdef POSIX
    static struct termios tchars;
# else
    static struct termio tchars;
# endif /* POSIX */

    if (on) {
# ifdef POSIX
	(void) tcgetattr(SHIN, &tchars);
# else
        (void) ioctl(SHIN, TCGETA, (ioctl_t) &tchars);
# endif /* POSIX */
	tchars.c_cc[VEOL] = ESC;
	if (tchars.c_lflag & ICANON)
# ifdef POSIX
	    on = TCSANOW;
# else
	    on = TCSETAW;
# endif /* POSIX */
	else {
# ifdef POSIX
	    on = TCSAFLUSH;
# else
	    on = TCSETAF;
# endif /* POSIX */
	    tchars.c_lflag |= ICANON;
    
	}
#ifdef POSIX
        (void) tcsetattr(SHIN, on, &tchars);
#else
        (void) ioctl(SHIN, on, (ioctl_t) &tchars);
#endif /* POSIX */
    }
    else {
	tchars.c_cc[VEOL] = _POSIX_VDISABLE;
# ifdef POSIX
	(void) tcsetattr(SHIN, TCSANOW, &tchars);
# else
        (void) ioctl(SHIN, TCSETAW, (ioctl_t) &tchars);
# endif /* POSIX */
    }
#else
    struct sgttyb sgtty;
    static struct tchars tchars;/* INT, QUIT, XON, XOFF, EOF, BRK */

    if (on) {
	(void) ioctl(SHIN, TIOCGETC, (ioctl_t) & tchars);
	tchars.t_brkc = ESC;
	(void) ioctl(SHIN, TIOCSETC, (ioctl_t) & tchars);
	/*
	 * This must be done after every command: if the tty gets into raw or
	 * cbreak mode the user can't even type 'reset'.
	 */
	(void) ioctl(SHIN, TIOCGETP, (ioctl_t) & sgtty);
	if (sgtty.sg_flags & (RAW | CBREAK)) {
	    sgtty.sg_flags &= ~(RAW | CBREAK);
	    (void) ioctl(SHIN, TIOCSETP, (ioctl_t) & sgtty);
	}
    }
    else {
	tchars.t_brkc = -1;
	(void) ioctl(SHIN, TIOCSETC, (ioctl_t) & tchars);
    }
#endif /* TERMIO */
}

/*
 * Move back to beginning of current line
 */
static void
back_to_col_1()
{
#ifdef TERMIO
# ifdef POSIX
    struct termios tty, tty_normal;
# else
    struct termio tty, tty_normal;
# endif /* POSIX */
#else
    struct sgttyb tty, tty_normal;
#endif /* TERMIO */

# ifdef BSDSIGS
    sigmask_t omask = sigblock(sigmask(SIGINT));
# else
    sighold(SIGINT);
# endif /* BSDSIGS */

#ifdef TERMIO
# ifdef POSIX
    (void) tcgetattr(SHOUT, &tty);
# else
    (void) ioctl(SHOUT, TCGETA, (ioctl_t) &tty_normal);
# endif /* POSIX */
    tty_normal = tty;
    tty.c_iflag &= ~INLCR;
    tty.c_oflag &= ~ONLCR;
# ifdef POSIX
    (void) tcsetattr(SHOUT, TCSANOW, &tty);
# else
    (void) ioctl(SHOUT, TCSETAW, (ioctl_t) &tty);
# endif /* POSIX */
    (void) write(SHOUT, "\r", 1);
# ifdef POSIX
    (void) tcsetattr(SHOUT, TCSANOW, &tty_normal);
# else
    (void) ioctl(SHOUT, TCSETAW, (ioctl_t) &tty_normal);
# endif /* POSIX */
#else
    (void) ioctl(SHIN, TIOCGETP, (ioctl_t) & tty);
    tty_normal = tty;
    tty.sg_flags &= ~CRMOD;
    (void) ioctl(SHIN, TIOCSETN, (ioctl_t) & tty);
    (void) write(SHOUT, "\r", 1);
    (void) ioctl(SHIN, TIOCSETN, (ioctl_t) & tty_normal);
#endif /* TERMIO */

# ifdef BSDSIGS
    (void) sigsetmask(omask);
# else
    (void) sigrelse(SIGINT);
# endif /* BSDISGS */
}

/*
 * Push string contents back into tty queue
 */
static void
pushback(string)
    Char   *string;
{
    register Char *p;
    char    c;
#ifdef TERMIO
# ifdef POSIX
    struct termios tty, tty_normal;
# else
    struct termio tty, tty_normal;
# endif /* POSIX */
#else
    struct sgttyb tty, tty_normal;
#endif /* TERMIO */

#ifdef BSDSIGS
    sigmask_t omask = sigblock(sigmask(SIGINT));
#else
    sighold(SIGINT);
#endif /* BSDSIGS */

#ifdef TERMIO
# ifdef POSIX
    (void) tcgetattr(SHOUT, &tty);
# else
    (void) ioctl(SHOUT, TCSETAW, (ioctl_t) &tty);
# endif /* POSIX */
    tty_normal = tty;
    tty.c_lflag &= ~(ECHOKE | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOCTL);
# ifdef POSIX
    (void) tcsetattr(SHOUT, TCSANOW, &tty);
# else
    (void) ioctl(SHOUT, TCSETAW, (ioctl_t) &tty);
# endif /* POSIX */

    for (p = string; c = *p; p++)
	(void) ioctl(SHOUT, TIOCSTI, (ioctl_t) & c);
# ifdef POSIX
    (void) tcsetattr(SHOUT, TCSANOW, &tty_normal);
# else
    (void) ioctl(SHOUT, TCSETAW, (ioctl_t) &tty_normal);
# endif /* POSIX */
    (void) sigsetmask(omask);
#else
    (void) ioctl(SHOUT, TIOCGETP, (ioctl_t) & tty);
    tty_normal = tty;
    tty.sg_flags &= ~ECHO;
    (void) ioctl(SHOUT, TIOCSETN, (ioctl_t) & tty);

    for (p = string; c = *p; p++)
	(void) ioctl(SHOUT, TIOCSTI, (ioctl_t) & c);
    (void) ioctl(SHOUT, TIOCSETN, (ioctl_t) & tty_normal);
#endif /* TERMIO */

# ifdef BSDSIGS
    (void) sigsetmask(omask);
# else
    (void) sigrelse(SIGINT);
# endif /* BSDISGS */
}

/*
 * Concatenate src onto tail of des.
 * Des is a string whose maximum length is count.
 * Always null terminate.
 */
static void
catn(des, src, count)
    register Char *des, *src;
    register count;
{
    while (--count >= 0 && *des)
	des++;
    while (--count >= 0)
	if ((*des++ = *src++) == 0)
	    return;
    *des = '\0';
}

/*
 * Like strncpy but always leave room for trailing \0
 * and always null terminate.
 */
static void
copyn(des, src, count)
    register Char *des, *src;
    register count;
{
    while (--count >= 0)
	if ((*des++ = *src++) == 0)
	    return;
    *des = '\0';
}

static  Char
filetype(dir, file)
    Char   *dir, *file;
{
    Char    path[MAXPATHLEN];
    struct stat statb;

    catn(Strcpy(path, dir), file, sizeof(path) / sizeof(Char));
    if (lstat(short2str(path), &statb) == 0) {
	switch (statb.st_mode & S_IFMT) {
	case S_IFDIR:
	    return ('/');

	case S_IFLNK:
	    if (stat(short2str(path), &statb) == 0 &&	/* follow it out */
		S_ISDIR(statb.st_mode))
		return ('>');
	    else
		return ('@');

	case S_IFSOCK:
	    return ('=');

	default:
	    if (statb.st_mode & 0111)
		return ('*');
	}
    }
    return (' ');
}

static struct winsize win;

/*
 * Print sorted down columns
 */
static void
print_by_column(dir, items, count)
    Char   *dir, *items[];
    int     count;
{
    register int i, rows, r, c, maxwidth = 0, columns;

    if (ioctl(SHOUT, TIOCGWINSZ, (ioctl_t) & win) < 0 || win.ws_col == 0)
	win.ws_col = 80;
    for (i = 0; i < count; i++)
	maxwidth = maxwidth > (r = Strlen(items[i])) ? maxwidth : r;
    maxwidth += 2;		/* for the file tag and space */
    columns = win.ws_col / maxwidth;
    if (columns == 0)
	columns = 1;
    rows = (count + (columns - 1)) / columns;
    for (r = 0; r < rows; r++) {
	for (c = 0; c < columns; c++) {
	    i = c * rows + r;
	    if (i < count) {
		register int w;

		xprintf("%s", short2str(items[i]));
		xputchar(dir ? filetype(dir, items[i]) : ' ');
		if (c < columns - 1) {	/* last column? */
		    w = Strlen(items[i]) + 1;
		    for (; w < maxwidth; w++)
			xputchar(' ');
		}
	    }
	}
	xputchar('\r');
	xputchar('\n');
    }
}

/*
 * Expand file name with possible tilde usage
 *	~person/mumble
 * expands to
 *	home_directory_of_person/mumble
 */
static Char *
tilde(new, old)
    Char   *new, *old;
{
    register Char *o, *p;
    register struct passwd *pw;
    static Char person[40];

    if (old[0] != '~')
	return (Strcpy(new, old));

    for (p = person, o = &old[1]; *o && *o != '/'; *p++ = *o++);
    *p = '\0';
    if (person[0] == '\0')
	(void) Strcpy(new, value(STRhome));
    else {
	pw = getpwnam(short2str(person));
	if (pw == NULL)
	    return (NULL);
	(void) Strcpy(new, str2short(pw->pw_dir));
    }
    (void) Strcat(new, o);
    return (new);
}

/*
 * Cause pending line to be printed
 */
static void
retype()
{
#ifdef TERMIO
# ifdef POSIX
    struct termios tty;

    (void) tcgetattr(SHOUT, &tty);
# else
    struct termio tty;

    (void) ioctl(SHOUT, TCGETA, (ioctl_t) &tty);
# endif /* POSIX */

    tty.c_lflag |= PENDIN;

# ifdef POSIX
    (void) tcsetattr(SHOUT, TCSANOW, &tty);
# else
    (void) ioctl(SHOUT, TCSETAW, (ioctl_t) &tty);
# endif /* POSIX */
#else
    int     pending_input = LPENDIN;

    (void) ioctl(SHOUT, TIOCLBIS, (ioctl_t) & pending_input);
#endif /* TERMIO */
}

static void
beep()
{
    if (adrof(STRnobeep) == 0)
	(void) write(SHOUT, "\007", 1);
}

/*
 * Erase that silly ^[ and
 * print the recognized part of the string
 */
static void
print_recognized_stuff(recognized_part)
    Char   *recognized_part;
{
    /* An optimized erasing of that silly ^[ */
    putraw('\b');
    putraw('\b');
    switch (Strlen(recognized_part)) {

    case 0:			/* erase two Characters: ^[ */
	putraw(' ');
	putraw(' ');
	putraw('\b');
	putraw('\b');
	break;

    case 1:			/* overstrike the ^, erase the [ */
	xprintf("%s", short2str(recognized_part));
	putraw(' ');
	putraw('\b');
	break;

    default:			/* overstrike both Characters ^[ */
	xprintf("%s", short2str(recognized_part));
	break;
    }
    flush();
}

/*
 * Parse full path in file into 2 parts: directory and file names
 * Should leave final slash (/) at end of dir.
 */
static void
extract_dir_and_name(path, dir, name)
    Char   *path, *dir, *name;
{
    register Char *p;

    p = Strrchr(path, '/');
    if (p == NULL) {
	copyn(name, path, MAXNAMLEN);
	dir[0] = '\0';
    }
    else {
	copyn(name, ++p, MAXNAMLEN);
	copyn(dir, path, p - path);
    }
}

static Char *
getentry(dir_fd, looking_for_lognames)
    DIR    *dir_fd;
    int     looking_for_lognames;
{
    register struct passwd *pw;
    register struct dirent *dirp;

    if (looking_for_lognames) {
	if ((pw = getpwent()) == NULL)
	    return (NULL);
	return (str2short(pw->pw_name));
    }
    if (dirp = readdir(dir_fd))
	return (str2short(dirp->d_name));
    return (NULL);
}

static void
free_items(items)
    register Char **items;
{
    register int i;

    for (i = 0; items[i]; i++)
	xfree((ptr_t) items[i]);
    xfree((ptr_t) items);
}

#ifdef BSDSIGS
# define FREE_ITEMS(items) { \
	sigmask_t omask;\
\
	omask = sigblock(sigmask(SIGINT));\
	free_items(items);\
	items = NULL;\
	(void) sigsetmask(omask);\
}
#else
# define FREE_ITEMS(items) { \
	(void) sighold(SIGINT);\
	free_items(items);\
	items = NULL;\
	(void) sigrelse(SIGINT);\
}
#endif /* BSDSIGS */

/*
 * Perform a RECOGNIZE or LIST command on string "word".
 */
static int
tsearch(word, command, max_word_length)
    Char   *word;
    int     max_word_length;
    COMMAND command;
{
    static Char **items = NULL;
    register DIR *dir_fd;
    register numitems = 0, ignoring = TRUE, nignored = 0;
    register name_length, looking_for_lognames;
    Char    tilded_dir[MAXPATHLEN + 1], dir[MAXPATHLEN + 1];
    Char    name[MAXNAMLEN + 1], extended_name[MAXNAMLEN + 1];
    Char   *entry;

#define MAXITEMS 1024

    if (items != NULL)
	FREE_ITEMS(items);

    looking_for_lognames = (*word == '~') && (Strchr(word, '/') == NULL);
    if (looking_for_lognames) {
	(void) setpwent();
	copyn(name, &word[1], MAXNAMLEN);	/* name sans ~ */
	dir_fd = NULL;
    }
    else {
	extract_dir_and_name(word, dir, name);
	if (tilde(tilded_dir, dir) == 0)
	    return (0);
	dir_fd = opendir(*tilded_dir ? short2str(tilded_dir) : ".");
	if (dir_fd == NULL)
	    return (0);
    }

again:				/* search for matches */
    name_length = Strlen(name);
    for (numitems = 0; entry = getentry(dir_fd, looking_for_lognames);) {
	if (!is_prefix(name, entry))
	    continue;
	/* Don't match . files on null prefix match */
	if (name_length == 0 && entry[0] == '.' &&
	    !looking_for_lognames)
	    continue;
	if (command == LIST) {
	    if (numitems >= MAXITEMS) {
		xprintf("\nYikes!! Too many %s!!\n",
			looking_for_lognames ?
			"names in password file" : "files");
		break;
	    }
	    if (items == NULL)
		items = (Char **) xcalloc(sizeof(items[0]), MAXITEMS);
	    items[numitems] = (Char *) xmalloc((size_t) (Strlen(entry) + 1) *
					       sizeof(Char));
	    copyn(items[numitems], entry, MAXNAMLEN);
	    numitems++;
	}
	else {			/* RECOGNIZE command */
	    if (ignoring && ignored(entry))
		nignored++;
	    else if (recognize(extended_name,
			       entry, name_length, ++numitems))
		break;
	}
    }
    if (ignoring && numitems == 0 && nignored > 0) {
	ignoring = FALSE;
	nignored = 0;
	if (looking_for_lognames)
	    (void) setpwent();
	else
	    rewinddir(dir_fd);
	goto again;
    }

    if (looking_for_lognames)
	(void) endpwent();
    else
	(void) closedir(dir_fd);
    if (numitems == 0)
	return (0);
    if (command == RECOGNIZE) {
	if (looking_for_lognames)
	    copyn(word, STRtilde, 1);
	else
	    /* put back dir part */
	    copyn(word, dir, max_word_length);
	/* add extended name */
	catn(word, extended_name, max_word_length);
	return (numitems);
    }
    else {			/* LIST */
	qsort((ptr_t) items, numitems, sizeof(items[0]), sortscmp);
	print_by_column(looking_for_lognames ? NULL : tilded_dir,
			items, numitems);
	if (items != NULL)
	    FREE_ITEMS(items);
    }
    return (0);
}

/*
 * Object: extend what user typed up to an ambiguity.
 * Algorithm:
 * On first match, copy full entry (assume it'll be the only match)
 * On subsequent matches, shorten extended_name to the first
 * Character mismatch between extended_name and entry.
 * If we shorten it back to the prefix length, stop searching.
 */
static int
recognize(extended_name, entry, name_length, numitems)
    Char   *extended_name, *entry;
    int     name_length, numitems;
{
    if (numitems == 1)		/* 1st match */
	copyn(extended_name, entry, MAXNAMLEN);
    else {			/* 2nd & subsequent matches */
	register Char *x, *ent;
	register int len = 0;

	x = extended_name;
	for (ent = entry; *x && *x == *ent++; x++, len++);
	*x = '\0';		/* Shorten at 1st Char diff */
	if (len == name_length)	/* Ambiguous to prefix? */
	    return (-1);	/* So stop now and save time */
    }
    return (0);
}

/*
 * Return true if check matches initial Chars in template.
 * This differs from PWB imatch in that if check is null
 * it matches anything.
 */
static int
is_prefix(check, template)
    register Char *check, *template;
{
    do
	if (*check == 0)
	    return (TRUE);
    while (*check++ == *template++);
    return (FALSE);
}

/*
 *  Return true if the Chars in template appear at the
 *  end of check, I.e., are it's suffix.
 */
static int
is_suffix(check, template)
    Char   *check, *template;
{
    register Char *c, *t;

    for (c = check; *c++;);
    for (t = template; *t++;);
    for (;;) {
	if (t == template)
	    return 1;
	if (c == check || *--t != *--c)
	    return 0;
    }
}

int
tenex(inputline, inputline_size)
    Char   *inputline;
    int     inputline_size;
{
    register int numitems, num_read;
    char    tinputline[BUFSIZE];


    setup_tty(ON);

    while ((num_read = read(SHIN, tinputline, BUFSIZE)) > 0) {
	int     i;
	static Char delims[] = {' ', '\'', '"', '\t', ';', '&', '<',
	'>', '(', ')', '|', '^', '%', '\0'};
	register Char *str_end, *word_start, last_Char, should_retype;
	register int space_left;
	COMMAND command;

	for (i = 0; i < num_read; i++)
	    inputline[i] = (unsigned char) tinputline[i];
	last_Char = inputline[num_read - 1] & ASCII;

	if (last_Char == '\n' || num_read == inputline_size)
	    break;
	command = (last_Char == ESC) ? RECOGNIZE : LIST;
	if (command == LIST)
	    xputchar('\n');
	str_end = &inputline[num_read];
	if (last_Char == ESC)
	    --str_end;		/* wipeout trailing cmd Char */
	*str_end = '\0';
	/*
	 * Find LAST occurence of a delimiter in the inputline. The word start
	 * is one Character past it.
	 */
	for (word_start = str_end; word_start > inputline; --word_start)
	    if (Strchr(delims, word_start[-1]))
		break;
	space_left = inputline_size - (word_start - inputline) - 1;
	numitems = tsearch(word_start, command, space_left);

	if (command == RECOGNIZE) {
	    /* print from str_end on */
	    print_recognized_stuff(str_end);
	    if (numitems != 1)	/* Beep = No match/ambiguous */
		beep();
	}

	/*
	 * Tabs in the input line cause trouble after a pushback. tty driver
	 * won't backspace over them because column positions are now
	 * incorrect. This is solved by retyping over current line.
	 */
	should_retype = FALSE;
	if (Strchr(inputline, '\t')) {	/* tab Char in input line? */
	    back_to_col_1();
	    should_retype = TRUE;
	}
	if (command == LIST)	/* Always retype after a LIST */
	    should_retype = TRUE;
	if (should_retype)
	    printprompt();
	pushback(inputline);
	if (should_retype)
	    retype();
    }
    setup_tty(OFF);
    return (num_read);
}

static int
ignored(entry)
    register Char *entry;
{
    struct varent *vp;
    register Char **cp;

    if ((vp = adrof(STRfignore)) == NULL || (cp = vp->vec) == NULL)
	return (FALSE);
    for (; *cp != NULL; cp++)
	if (is_suffix(entry, *cp))
	    return (TRUE);
    return (FALSE);
}
#endif	/* FILEC */
