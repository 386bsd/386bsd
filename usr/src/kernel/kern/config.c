/*-
 * Copyright (c) 1994 William F. Jolitz, TeleMuse
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
 *	This software is a component of "386BSD" developed by 
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 5. Non-commercial distribution of this complete file in either source and/or
 *    binary form at no charge to the user (such as from an official Internet 
 *    archive site) is permitted. 
 * 6. Commercial distribution and sale of this complete file in either source
 *    and/or binary form on any media, including that of floppies, tape, or 
 *    CD-ROM, or through a per-charge download such as that of a BBS, is not 
 *    permitted without specific prior written permission.
 * 7. Non-commercial and/or commercial distribution of an incomplete, altered, 
 *    or otherwise modified file in either source and/or binary form is not 
 *    permitted.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: config.c,v 1.1 94/10/20 00:02:46 bill Exp Locker: bill $
 * Software module configuration.
 */

#include "sys/param.h"
#include "sys/stat.h"
#include "sys/ioctl.h"
#include "buf.h"	/* devif_strategy ... */
#include "tty.h"	/* ldiscif...() ... */
#include "modconfig.h"
#include "sys/errno.h"
#include "prototypes.h"


int query(char *s, ...);
extern char config_string[];

static int console_minor;
extern struct devif pc_devif;
struct devif *console_devif = &pc_devif;
struct devif *default_console_devif = &pc_devif;
static struct ldiscif *ldisc;

/*
 * config script language primatives
 */

/* skip over any whitespace in string, including comments */
char *
cfg_skipwhite(char **ptr) {
	char *p = *ptr;

rescan:
	/* white space is any blanks, tabs, newlines or returns */
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
		p++;

	/* comments are treated as white space, terminated by a newline */
	if (*p == '#') {
		while (*p && *p != '\n')
			p++;
		goto rescan;
	}

	return (*ptr = p);
}

/* is character a digit in given base? */
static int
isdigit(char c, int base) {

	if (base == 10)
		return ((c >= '0' && c <= '9') ? 1 : 0);
	if (base == 8)
		return ((c >= '0' && c <= '7') ? 1 : 0);
	if (base == 16) {
		if (c >= '0' && c <= '9')
			return (1);
		if (c >= 'a' && c <= 'f')
			return (1);
		if (c >= 'A' && c <= 'F')
			return (1);
	}
	return (0);
}

/* alphanumeric character? */
static int
isalphanum(char c) {

	if (c >= 'a' && c <= 'z')
		return (1);
	if (c >= 'A' && c <= 'Z')
		return (1);
	if (c >= '0' && c <= '9')
		return (1);

	return (0);
}

/* value of digit */
static int
valdigit(char c) {

	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);
	if (c >= 'A' && c <= 'F')
		return (c - 'A' + 10);

	return (0);
}

int
cfg_number(char **ptr, int *pval) {
	char *p;
	int base = 10, val = 0, sign = 0;

	/* consume any whitespace */
	p = cfg_skipwhite(ptr);

	/* sign? */
	if (*p == '-') {
		p++;
		sign = 1;
	}

	/* must start with at least one decimal numeric character */
	if (isdigit(*p, 10)) {

		/* base of number */
		if (p[0] == '0') {
			if (p[1] == 'x') {
				p += 2; base = 16;
			} else {
				p++ ; base = 8;
			}
		}

		/* accumulate number value. */
		while (isdigit(*p, base))
			val = base * val + valdigit(*p++);

		/* return found number, and advance string pointer */
		*pval = sign ? -val : val;
		*ptr = p;
		return (1);
			
	} else
		return (0);
}

/* accept a string token of finite length? */
int
cfg_string(char **ptr, char *sp, int szval) {
	char *p, *rsp = sp, *lp; 
	int dummyval;

	/* consume any whitespace */
	lp = p = cfg_skipwhite(ptr);

	/* reserve place in buffer for null */
	szval -= 1;

	/* accumulate alphanumeric string value. */
	while (isalphanum(*p)) {
		if (szval-- > 0)
			*rsp++ = *p;
		p++;
	}

	/* anything found? */
	if (rsp != sp) {

		/* return found string, adding terminating null */
		*rsp = 0;
		*ptr = p;
		return (1);
			
	} else
		return (0);
}

/* accept a character token ? */
int
cfg_char(char **ptr, char t) {
	char *p;

	p = cfg_skipwhite(ptr);

	if (*p++ == t) {
		*ptr = p;
		return (1);
	} else
		return (0);
}

/* parse and return any name/token pairs found into respective values */
int
cfg_namelist(char **cfg, struct namelist *nmp) {
	char *lcfg = *cfg;
	char val[80];
	struct namelist *n;
	int any = 0;

	while (cfg_string(&lcfg, val, sizeof(val))) {
		for (n = nmp; n->name ; n++)
			if (strcmp(n->name, val) == 0)
				goto found;
		return (any);
	found:
		switch (n->type) {
		case NUMBER:
			if (cfg_number(&lcfg, (int *)n->value) == 0)
				return (any);
			break;

		case STRING:
			/* XXX do I really want a length? */
			if (cfg_string(&lcfg, (char *)n->value, 80) == 0)
				return (any);
			break;

		default:
			return (any);
		}
		*cfg = lcfg;	/* reduce */
		any++;
	}
	return (any);
}

static char arg[32];	/* currently configuring module */

/* discover if module should configure itself */
int
config_scan(char *cfg, char **cfg_sp) {
	extern end;
	char *lp = (char *)&end /*config_string*/;
	char strbuf[32];
	int dummy, exclaim;

	/* fetch default configuration script */
	(void)cfg_string(&cfg, arg, sizeof(arg));

	/* if starting string matches, signal configuration state */
next_entry:
	exclaim = cfg_char(&lp, '!');
	if (cfg_string(&lp, strbuf, 32) && strcmp(arg, strbuf) == 0 && 
	    query("config: override %s", arg)) {

		/* return config string? */
		if (cfg_sp)
			*cfg_sp = lp;

		return (exclaim == 0);
	}

	/* does not match, consume entry */
	for (;; lp++) {

		/* consume either kind of token */
		while (cfg_number(&lp, &dummy) || cfg_string(&lp, strbuf, 32))
				;
		/* done with string */
		if (cfg_char(&lp, 0))
			goto done_all;

		/* done with entry */
		if (cfg_char(&lp, '.'))
			break;
	}
	goto next_entry;

done_all:
	/* if not found, and not to be configured if not found, ignore */
	if (cfg_char(&cfg, '.'))
		return(0);

	if (cfg_sp)
		*cfg_sp = cfg;

	return(query("config: default %s", arg));
}

/*
 * Module Communications, by class type.
 */

extern int etext, edata;	/* XXX scan all kernel loaded data segments */

/* tell all modules of a class type to initialize themselves */
void
modscaninit(modtype_t modtype) {
	int *sigp;

	for (sigp = &etext; sigp < &edata; sigp++)

		/* if valid signature ... */
		if (*sigp == MODULE_SIGNATURE) {
			struct modconfig *mcp = (struct modconfig *)sigp;
			
			/* ... and a valid type ... */
			if (MODT_ISVALID(mcp->mod_type)) {

				/* either to the supplied arg, or to all */
				if ((mcp->mod_type == modtype
				    || modtype == __MODT_ALL__)
				    && mcp->mod_init)
					(mcp->mod_init)();
			}
		}
}

void
smodscaninit(modtype_t modtype, int *sstart, int *send) {
	int *sigp;

	for (sigp = sstart; sigp < send; sigp++)

		/* if valid signature ... */
		if (*sigp == MODULE_SIGNATURE) {
			struct modconfig *mcp = (struct modconfig *)sigp;
			
			/* ... and a valid type ... */
			if (MODT_ISVALID(mcp->mod_type)) {

				/* either to the supplied arg, or to all */
				if ((mcp->mod_type == modtype
				    || modtype == __MODT_ALL__)
				    && mcp->mod_init)
					(mcp->mod_init)();
			}
		}
}

/*
 * Kernel interface to device drivers.
 */

#define NDEVIF	100	/* more than adequate */
struct devif *chrmajtodevif[NDEVIF], *blkmajtodevif[NDEVIF];

/* fill in the name of the supplied device in a supplied string buffer */
int
devif_name(dev_t dev, devif_type_t typ, char *name, int namelen) {
	struct devif *dif;
	unsigned major = major(dev);
	int minor;
	char *fp, tmpbuf[16];

	switch (typ) {
	case CHRDEV:
	    	if (major > sizeof chrmajtodevif / sizeof chrmajtodevif[0] ||
		    (dif = chrmajtodevif[major]) == 0)
			return (ENODEV);
		break;

	case BLKDEV:
	    	if (major > sizeof blkmajtodevif / sizeof blkmajtodevif[0] ||
		    (dif = blkmajtodevif[major]) == 0)
			return (ENODEV);
		break;

	default:
		panic("devif_name");	/* unimplemented device type */
	}

	/* put in device name */
	fp = dif->di_name;
	while (namelen) {
		*name++ = *fp++;
		namelen--;
	}

	/* construct minor device name with unit & subunit */
	minor = minor(dev);
	tmpbuf[0] = 0;
	if (dif->di_minor_unit_mask && dif->di_minor_subunit_mask) 
		sprintf(tmpbuf,"%d%c",
	    	    (minor & dif->di_minor_unit_mask)
			>> dif->di_minor_unit_shift,
	    	    ((minor & dif->di_minor_subunit_mask)
			>> dif->di_minor_subunit_shift) + 'a');

	/* ... or construct minor device name with unit */
	else if (dif->di_minor_unit_mask && dif->di_minor_subunit_mask) 
		sprintf(tmpbuf,"%d",
	    	    (minor & dif->di_minor_unit_mask)
			>> dif->di_minor_unit_shift);

	/* put into name */
	fp = tmpbuf;
	while (namelen && *fp) {
		*name++ = *fp++;
		namelen--;
	}
}

/* check and adjust a bootdev's parameters so as to create a root device */
int
devif_root(unsigned major, unsigned unit, unsigned subunit, dev_t *rd)
{
	struct devif *dif;

	/* should we configure this as root? */
	if (query("devif_root: major %d unit %d sub %d\n",
		major, unit, subunit) == 0)
			return (0);

	/* is there a device driver for the root? */
	if (major >= NDEVIF || (dif = blkmajtodevif[major]) == 0)
		return (0);
	
	/* turn a boot dev set of parameters into the root dev */
	*rd = makedev(major, ((unit) << dif->di_minor_unit_shift)
		+ ((subunit) << dif->di_minor_subunit_shift));

	return (1);
}

/* locate and call a device driver open routine via the device driver interface */
int
devif_open(dev_t dev, devif_type_t typ, int flag, struct proc *p) {
	struct devif *dif;
	unsigned major = major(dev);

	/* if a BSD UN*X character device ... */
	if (typ == CHRDEV &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
		dif = chrmajtodevif[major];
		if (dif == console_devif)
			dev = makedev(major, console_minor);
		if (dif)
			return ((dif->di_open)(dev, flag, S_IFCHR, p));
			
		return (ENXIO);
	}

	/* if a BSD UN*X block device ... */
	if (typ == BLKDEV &&
	    major < sizeof blkmajtodevif / sizeof blkmajtodevif[0]) {
		if (dif = blkmajtodevif[major])
			return ((dif->di_open)(dev, flag, S_IFBLK, p));
			
		return (ENXIO);
	}

	/* add support for new types here */
#ifdef	notyet
	/* if a 386BSD UNICODE character device ... */
	if (typ == WCHRDEV &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
		if (dif = chrmajtodevif[major])
			return ((dif->di_open)(dev, flag, S_IFWCHR, p));
			
		return (ENXIO);
	}

	/* if a 386BSD page i/o device ... */
	if (typ == PAGEDEV &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
		if (dif = chrmajtodevif[major])
			return ((dif->di_open)(dev, flag, S_IFPAGE, p));
			
		return (ENXIO);
	}
#endif
	panic("devif_open");	/* unimplemented type */
}

/* locate and call a device driver close routine via the device driver interface */
int
devif_close(dev_t dev, devif_type_t typ, int flag, struct proc *p) {
	struct devif *dif;
	unsigned major = major(dev);

	/* if a BSD UN*X character device ... */
	if (typ == CHRDEV &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
		dif = chrmajtodevif[major];
		if (dif == console_devif)
			dev = makedev(major, console_minor);
		if (dif)
			return ((dif->di_close)(dev, flag, S_IFCHR, p));
			
		return (ENODEV);
	}

	/* if a BSD UN*X block device ... */
	if (typ == BLKDEV &&
	    major < sizeof blkmajtodevif / sizeof blkmajtodevif[0]) {
		if (dif = blkmajtodevif[major])
			return ((dif->di_close)(dev, flag, S_IFBLK, p));
			
		return (ENODEV);
	}

	/* add support for new types here */
#ifdef	notyet
	/* if a 386BSD UNICODE character device ... */
	if (typ == WCHRDEV &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
		if (dif = chrmajtodevif[major])
			return ((dif->di_close)(dev, flag, S_IFWCHR, p));
		return (ENXIO);
	}

	/* if a 386BSD page i/o device ... */
	if (typ == PAGEDEV &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
		if (dif = chrmajtodevif[major])
			return ((dif->di_close)(dev, flag, S_IFPAGE, p));
		return (ENXIO);
	}
#endif
	panic("devif_close");	/* unimplemented type */
}

/* locate and call a device driver ioctl routine via the device driver interface */
int
devif_ioctl(dev_t dev, devif_type_t typ, int cmd, caddr_t data, int flag,
    struct proc *p) {
	struct devif *dif;
	unsigned major = major(dev);

	/* if a BSD UN*X character device ... */
	if (typ == CHRDEV &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
	chrcase:
		dif = chrmajtodevif[major];
		if (dif == console_devif)
			dev = makedev(major, console_minor);
		if (dif)
			return ((dif->di_ioctl)(dev, cmd, data, flag, p));
			
		return (ENODEV);
	}

	/* if a BSD UN*X block device ... */
	if (typ == BLKDEV &&
	    major < sizeof blkmajtodevif / sizeof blkmajtodevif[0]) {
		if (dif = blkmajtodevif[major])
			return ((dif->di_ioctl)(dev, cmd, data, flag, p));
			
		return (ENODEV);
	}

	/* add support for new types here */
#ifdef notyet
	/* if either a 386BSD UNICODE or page character device ... */
	if ((typ == WCHRDEV || typ == PAGEDEV)) &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
		goto chrcase;
#endif
	panic("devif_ioctl");	/* unimplemented type */
}

/* locate and call a device driver read routine via the device driver interface */
int
devif_read(dev_t dev, devif_type_t typ, struct uio *uio, int flag) {
	struct devif *dif;
	unsigned major = major(dev);

	/* if a BSD UN*X character device ... */
	if (typ == CHRDEV &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
		dif = chrmajtodevif[major];
		if (dif == console_devif)
			dev = makedev(major, console_minor);
		if (dif)
			return ((dif->di_read)(dev, uio, flag));
			
		return (ENODEV);
	}

	/* add support for new types here */
#ifdef notyet
	/* if a 386BSD UNICODE character device ... */
	if (typ == WCHRDEV &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
		if (dif = chrmajtodevif[major])
			return ((dif->di_wcread)(dev, uio, flag));
		return (ENODEV);
	}

	/* if a 386BSD page i/o device ... */
	if (typ == PAGEDEV &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
		if (dif = chrmajtodevif[major])
			return ((dif->di_pageread)(dev, uio, flag));
		return (ENODEV);
	}
#endif
	panic("devif_read");	/* unimplemented type */
}

/* locate and call a device driver write routine via the device driver interface */
int
devif_write(dev_t dev, devif_type_t typ, struct uio *uio, int flag) {
	struct devif *dif;
	unsigned major = major(dev);

	/* if a BSD UN*X character device ... */
	if (typ == CHRDEV &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
		dif = chrmajtodevif[major];
		if (dif == console_devif)
			dev = makedev(major, console_minor);
		if (dif)
			return ((dif->di_write)(dev, uio, flag));
			
		return (ENODEV);
	}

	/* add support for new types here */
#ifdef notyet
	/* if a 386BSD UNICODE character device ... */
	if (typ == WCHRDEV &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
		if (dif = chrmajtodevif[major])
			return ((dif->di_wcwrite)(dev, uio, flag));
		return (ENXIO);
	}

	/* if a 386BSD page i/o device ... */
	if (typ == PAGEDEV &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
		if (dif = chrmajtodevif[major])
			return ((dif->di_pagewrite)(dev, uio, flag));
		return (ENXIO);
	}
#endif
	panic("devif_write");	/* unimplemented type */
}

/* locate and call a device driver select routine via the device driver interface */
int
devif_select(dev_t dev, devif_type_t typ, int rw, struct proc *p) {
	struct devif *dif;
	unsigned major = major(dev);

	/* if a BSD UN*X character device ... */
	if (typ == CHRDEV &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
	chrcase:
		dif = chrmajtodevif[major];
		if (dif == console_devif)
			dev = makedev(major, console_minor);
		if (dif)
			return ((dif->di_select)(dev, rw, p));
			
		return (ENODEV);
	}

	/* add support for new types here */
#ifdef notyet
	/* if either a 386BSD UNICODE or page character device ... */
	if ((typ == WCHRDEV || typ == PAGEDEV)) &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
		goto chrcase;
#endif
	panic("devif_select");	/* unimplemented type */
}

/* locate and call a device driver mmap routine via the device driver interface */
int
devif_mmap(dev_t dev, devif_type_t typ, int offset, int nprot) {
	struct devif *dif;
	unsigned major = major(dev);

	/* if a BSD UN*X character device ... */
	if (typ == CHRDEV &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
		dif = chrmajtodevif[major];
		if (dif == console_devif)
			dev = makedev(major, console_minor);
		if (dif)
			return ((dif->di_mmap)(dev, offset, nprot));
			
		return (-1);
	}

	/* add support for new types here */
	panic("devif_mmap");	/* unimplemented type */
}

/* locate and call a device driver strategy routine via the device driver interface */
int
devif_strategy(devif_type_t typ, struct buf *bp) {
	struct devif *dif;
	unsigned major = major(bp->b_dev);

	/* if a BSD UN*X character device ... */
	if (typ == CHRDEV &&
	    major < sizeof chrmajtodevif / sizeof chrmajtodevif[0]) {
		if (dif = chrmajtodevif[major])
			return ((dif->di_strategy)(bp));
			
		return (ENODEV);
	}

	/* if a BSD UN*X block device ... */
	if (typ == BLKDEV &&
	    major < sizeof blkmajtodevif / sizeof blkmajtodevif[0]) {
		if (dif = blkmajtodevif[major])
			return ((dif->di_strategy)(bp));
			
		return (ENODEV);
	}

	panic("devif_strategy");	/* illegal type */
}

/* locate and call a device driver size routine via the device driver interface */
int
devif_psize(dev_t dev, devif_type_t typ) {
	struct devif *dif;
	unsigned major = major(dev);

	/* if a BSD UN*X block device ... */
	if (typ == BLKDEV &&
	    major < sizeof blkmajtodevif / sizeof blkmajtodevif[0]) {
		if (dif = blkmajtodevif[major])
			return ((dif->di_psize)(dev));
			
		return (0);
	}

	panic("devif_psize");	/* illegal type */
}

/* locate and call a device driver dump routine via the device driver interface */
int
devif_dump(dev_t dev, devif_type_t typ) {
	struct devif *dif;
	unsigned major = major(dev);

	/* if a BSD UN*X block device ... */
	if (typ == BLKDEV &&
	    major < sizeof blkmajtodevif / sizeof blkmajtodevif[0]) {
		if (dif = blkmajtodevif[major])
			return ((dif->di_dump)(dev));
	}

	return (ENODEV);
}

/* configure a BSD UN*X device by registering its "devif" interface */
int
devif_config(char **cfg, struct devif *dif)
{
	int bmaj, cmaj;
	struct devif *odif;
	extern int _ENODEV_();

	/* configure name? */
	if (dif->di_name[0] == 0) {
		if (!config_scan(*cfg, cfg))
			return (0);
		memcpy(dif->di_name, arg, sizeof(arg));
	}

	/* configure block device? */
	if (dif->di_bmajor == -1 && !cfg_number(cfg, &dif->di_bmajor))
		return (0);

	/* configure character device? */
	if (dif->di_cmajor == -1 && !cfg_number(cfg, &dif->di_cmajor))
		return (0);

	/* have a block device interface ? */
	if ((bmaj = dif->di_bmajor) >= 0) {
		struct devif *odif;

		/* device number out of range */
		if (bmaj >= NDEVIF) {
			printf("%s: blkdev %d too big, not configured.\n",
				arg, bmaj);
			return (0);
		}

		/* device already in use? */
		if (odif = blkmajtodevif[bmaj]) {
			printf("%s: blkdev %d already used by %s, not configured.\n",
				arg, bmaj, odif->di_name);
			return (0);
		} 

		if (query("devif: config %s blkdev", arg) == 0)
			return(0);

		blkmajtodevif[bmaj] = dif;
	}
			
	/* have a character device interface ? */
	if ((cmaj = dif->di_cmajor) >= 0) {

		/* device number out of range */
		if (cmaj >= NDEVIF) {
			printf("%s: chrdev %d too big, not configured.\n",
				arg, cmaj);
			return (0);
		}

		/* device already in use? */
		if (odif = chrmajtodevif[cmaj]) {
			printf("%s: chrdev %d already used by %s, not configured.\n",
				arg, cmaj, odif->di_name);
			return (0);
		}

		if (query("devif: config %s chrdev", arg) == 0)
			return(0);

		chrmajtodevif[cmaj] = dif;
	}

	/* check sanity - all devices */
	if (cmaj >= 0 || bmaj >= 0) {
		if ((int)dif->di_open == 0)
			(int *) dif->di_open = (int *)nullop;
		if ((int)dif->di_close == 0)
			(int *) dif->di_close = (int *)nullop;
		if ((int)dif->di_ioctl == 0)
			(int *) dif->di_ioctl = (int *)_ENODEV_;
	}

	/* check sanity - character devices */
	if (cmaj >= 0) {
		if (dif->di_strategy) {
			if (dif->di_read == 0)
				dif->di_read = rawread;
			if (dif->di_write == 0)
				dif->di_write = rawwrite;
		}
		if ((int)dif->di_select == 0)
			dif->di_select = seltrue;
		if (dif->di_write == 0)
			(int *)dif->di_write = _ENODEV_;
		if (dif->di_read == 0)
			(int *)dif->di_read = _ENODEV_;
	}

	/* check sanity - block devices */
	if (bmaj >= 0) {
		if ((int)dif->di_strategy == 0) {
			blkmajtodevif[bmaj] = 0;
			printf("%s: blkdev %d no strategy, not configured.\n",
				arg, bmaj);
			return (0);
		}
		if ((int)dif->di_dump == 0)
			(int *) dif->di_dump = (int *)_ENODEV_;
		if ((int)dif->di_psize == 0)
			(int *) dif->di_psize = (int *)_ENODEV_;
	}

	return (1);
}

/*
 * Kernel interface to the line discipline drivers via the ldiscif interface.
 */

/* open and initialize the tty associated with the device */
int
ldiscif_open(dev_t dev, struct tty *tp, int flag) {
	struct ldiscif *lif;
	unsigned line = tp->t_line;
	int val;

	/* find line discipline */
	for (lif = ldisc; lif && lif->li_disc != line ; lif = lif->li_next)
		;

	/* not found */
	if (lif == 0)
		return (ENXIO);

	/* if a BSD UN*X line discipline ... */
	val = (lif->li_open)(dev, tp, flag);
	tp->t_ldiscif = (void *) lif;
	return (val);
}

/* device force close the tty */
void
ldiscif_close(struct tty *tp, int flag) {
	struct ldiscif *lif = (struct ldiscif *)tp->t_ldiscif;

	/* panic if closed line discipline */
	if (lif == 0)
		panic("ldiscif_close");

	/* if a BSD UN*X line discipline ... */
	(lif->li_close)(tp, flag);
	tp->t_ldiscif = (void *) 0;
}

/* perform a device read on a tty */
int
ldiscif_read(struct tty *tp, struct uio *devuio, int devflag) {
	struct ldiscif *lif = (struct ldiscif *)tp->t_ldiscif;

	/* panic if closed line discipline */
	if (lif == 0)
		panic("ldiscif_read");

	/* if a BSD UN*X line discipline ... */
	return ((lif->li_read)(tp, devuio, devflag));
}

/* perform a device write on a tty */
int
ldiscif_write(struct tty *tp, struct uio *devuio, int devflag) {
	struct ldiscif *lif = (struct ldiscif *)tp->t_ldiscif;

	/* panic if closed line discipline */
	if (lif == 0)
		panic("ldiscif_write");

	/* if a BSD UN*X line discipline ... */
	return ((lif->li_write)(tp, devuio, devflag));
}

/* perform a device ioctl on a tty */
int
ldiscif_ioctl(struct tty *tp, int cmd, caddr_t data, int flag, struct proc *p) {
	struct ldiscif *lif = (struct ldiscif *)tp->t_ldiscif;

	/* panic if closed line discipline */
	if (lif == 0)
		panic("ldiscif_ioctl");

	/* if a BSD UN*X line discipline ... */
	return ((lif->li_ioctl)(tp, cmd, data, flag, p));
}

/* hand a tty a "character" */
void
ldiscif_rint(unsigned ch, struct tty *tp) {
	struct ldiscif *lif = (struct ldiscif *)tp->t_ldiscif;

	/* if a BSD UN*X line discipline ... */
	if (lif)
		(lif->li_rint)(ch, tp);
}

/* output a "character" via a tty */
unsigned
ldiscif_put(unsigned ch, struct tty *tp) {
	struct ldiscif *lif = (struct ldiscif *)tp->t_ldiscif;

	/* if a BSD UN*X line discipline ... */
	if (lif)
		return((lif->li_put)(ch, tp));
	else
		return (ch);
}

/* driver (re)initiates output on tty */
void
ldiscif_start(struct tty *tp) {
	struct ldiscif *lif = (struct ldiscif *)tp->t_ldiscif;

	/* if a BSD UN*X line discipline ... */
	if (lif)
		(lif->li_start)(tp);
}

/* inform a tty of a change with modem control */
int
ldiscif_modem(struct tty *tp, int flag) {
	struct ldiscif *lif = (struct ldiscif *)tp->t_ldiscif;

	/* if a BSD UN*X line discipline ... */
	if (lif)
		return ((lif->li_modem)(tp, flag));

	return (1);
}

/* return the size of a line discipline's queue */
int
ldiscif_qsize(struct tty *tp, void *q) {
	struct ldiscif *lif = (struct ldiscif *)tp->t_ldiscif;

	/* if a BSD UN*X line discipline ... */
	if (lif)
		return ((lif->li_qsize)(tp, q));

	return (0);
}

/* line discipline module to kernel configuration function */
int
ldiscif_config(char **cfg, struct ldiscif *lif)
{
	int line;
	struct ldiscif *olif;
	extern int _ENODEV_();

	/* configure name? */
	if (lif->li_name[0] == 0) {
		if (!config_scan(*cfg, cfg))
			return (0);
		memcpy(lif->li_name, arg, sizeof(arg));
	}

	/* configure line index? */
	if (lif->li_disc == -1 && !cfg_number(cfg, &lif->li_disc))
		return (0);

	/* have a line discipline interface ? */
	if ((line = lif->li_disc) >= 0) {

		/* attempt to find line discipline */
		for (olif = ldisc; olif && olif->li_disc != line ;
		    olif = lif->li_next)
			;

		/* device already in use? */
		if (olif) {
			printf("%s: ldisc %d already used by %s, not configured.\n",
				arg, line, olif->li_name);
			return (0);
		}

		if (query("ldiscif: config %s", arg) == 0)
			return(0);

		lif->li_next = ldisc;
		ldisc = lif;
	} else
		return (0);
			
	/* check sanity */
	if ((int)lif->li_open == 0)
		(int *) lif->li_open = (int *)_ENODEV_;
	if ((int)lif->li_close == 0)
		(int *) lif->li_close = (int *)_ENODEV_;
	if ((int)lif->li_read == 0)
		(int *) lif->li_read = (int *)_ENODEV_;
	if ((int)lif->li_write == 0)
		(int *) lif->li_write = (int *)_ENODEV_;
	if ((int)lif->li_ioctl == 0)
		(int *) lif->li_ioctl = (int *)_ENODEV_;
	if ((int)lif->li_rint == 0)
		(int *) lif->li_rint = (int *)_ENODEV_;
	if ((int)lif->li_start == 0)
		(int *) lif->li_start = (int *)_ENODEV_;
	if ((int)lif->li_modem == 0)
		(int *) lif->li_modem = (int *)_ENODEV_;
	/* if ((int)lif->li_qsize == 0)
		(int *) lif->li_qsize = def_qsize; */

	return (1);
}

/* send a character to a console */
void
console_putchar(unsigned c) {

	if (console_devif)
	    (console_devif->di_putchar)(
		makedev(console_devif->di_cmajor, console_minor), c);
}

/* receive a character from the console */
unsigned
console_getchar(void) {
	int ch, s;

	if (console_devif) {
		/* s = splhigh(); */ 
	    	ch = (console_devif->di_getchar)
			(makedev(console_devif->di_cmajor, console_minor));
		/* splx(s); */
		return (ch);
	} else
		return (0);
}

/* console module configuration of a device as a console.  */
int
console_config(char **cons_cfg, char **mod_cfg, struct devif *dif) {
	char *cfg;
	char modname1[80]; /* XXX - should allow arbitrary size... */
	int dummy;

	/* is this a default console? */
	cfg = *cons_cfg;

	/* is there a console defined? */
	if (config_scan(cfg, cons_cfg) == 0)
		return (0);

	/* is there an effective console module? */
	if (cfg_string(cons_cfg, modname1, sizeof(modname1)) == 0)
		return(0);

	/* is the requested console module configurable? */
	cfg = *mod_cfg;
	if (config_scan(cfg, mod_cfg) == 0)
		return (0);

	/* default console */
	if (strcmp(modname1, "default") == 0)
		default_console_devif = dif;

	/* is it the same module? */
	else if (strcmp(modname1, arg) != 0)
		return(0);

	/* set minor device */
	console_minor = 0;
	(void)cfg_number(cons_cfg, &console_minor);

	/* consume major unit */
	(void)cfg_number(mod_cfg, &dummy);

	/* set console device interface */
	/*console_devif = dif;*/
	chrmajtodevif[0] = dif;

	/* if not default console, record use of console */
	if (default_console_devif == 0) {
		DELAY(10000);
		printf("using console %s\n", dif->di_name);
	}

	return (1);
}

int
query(char *s, ...) {

	return (1);
}

#ifndef DDB
void
Debugger (char *s) {
	pg(s);
}
#endif
