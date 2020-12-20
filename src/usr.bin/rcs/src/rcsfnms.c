/*
 *                     RCS file name handling
 */
#ifndef lint
 static char
 rcsid[]= "$Id: rcsfnms.c,v 1.1 93/12/24 10:26:27 bill Exp Locker: bill $ Purdue CS";
#endif
/****************************************************************************
 *                     creation and deletion of semaphorefile,
 *                     creation of temporary filenames and cleanup()
 *                     pairing of RCS file names and working file names.
 *                     Testprogram: define PAIRTEST
 ****************************************************************************
 */

/* Copyright (C) 1982, 1988, 1989 Walter Tichy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Walter Tichy.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Report all problems and direct all questions to:
 *   rcs-bugs@cs.purdue.edu
 * 







*/




/* $Log:	rcsfnms.c,v $
 * Revision 1.1  93/12/24  10:26:27  bill
 * Initial revision
 * 
 * Revision 3.12  89/08/15  21:38:10  bostic
 * Version 4 from Tom Narten at Purdue
 * 
 * Revision 4.8  89/05/01  15:09:41  narten
 * changed getwd to not stat empty directories.
 * 
 * Revision 4.7  88/11/08  12:01:22  narten
 * changes from  eggert@sm.unisys.com (Paul Eggert)
 * 
 * Revision 4.7  88/08/09  19:12:53  eggert
 * Fix troff macro comment leader bug; add Prolog; allow cc -R; remove lint.
 * 
 * Revision 4.6  87/12/18  11:40:23  narten
 * additional file types added from 4.3 BSD version, and SPARC assembler
 * comment character added. Also, more lint cleanups. (Guy Harris)
 * 
 * Revision 4.5  87/10/18  10:34:16  narten
 * Updating version numbers. Changes relative to 1.1 actually relative
 * to verion 4.3
 * 
 * Revision 1.3  87/03/27  14:22:21  jenkins
 * Port to suns
 * 
 * Revision 1.2  85/06/26  07:34:28  svb
 * Comment leader '% ' for '*.tex' files added.
 * 
 * Revision 1.1  84/01/23  14:50:24  kcs
 * Initial revision
 * 
 * Revision 4.3  83/12/15  12:26:48  wft
 * Added check for KDELIM in file names to pairfilenames().
 * 
 * Revision 4.2  83/12/02  22:47:45  wft
 * Added csh, red, and sl file name suffixes.
 * 
 * Revision 4.1  83/05/11  16:23:39  wft
 * Added initialization of Dbranch to InitAdmin(). Canged pairfilenames():
 * 1. added copying of path from workfile to RCS file, if RCS file is omitted;
 * 2. added getting the file status of RCS and working files;
 * 3. added ignoring of directories.
 * 
 * Revision 3.7  83/05/11  15:01:58  wft
 * Added comtable[] which pairs file name suffixes with comment leaders;
 * updated InitAdmin() accordingly.
 * 
 * Revision 3.6  83/04/05  14:47:36  wft
 * fixed Suffix in InitAdmin().
 * 
 * Revision 3.5  83/01/17  18:01:04  wft
 * Added getwd() and rename(); these can be removed by defining
 * V4_2BSD, since they are not needed in 4.2 bsd.
 * Changed sys/param.h to sys/types.h.
 *
 * Revision 3.4  82/12/08  21:55:20  wft
 * removed unused variable.
 *
 * Revision 3.3  82/11/28  20:31:37  wft
 * Changed mktempfile() to store the generated file names.
 * Changed getfullRCSname() to store the file and pathname, and to
 * delete leading "../" and "./".
 *
 * Revision 3.2  82/11/12  14:29:40  wft
 * changed pairfilenames() to handle file.sfx,v; also deleted checkpathnosfx(),
 * checksuffix(), checkfullpath(). Semaphore name generation updated.
 * mktempfile() now checks for nil path; freefilename initialized properly.
 * Added Suffix .h to InitAdmin. Added testprogram PAIRTEST.
 * Moved rmsema, trysema, trydiraccess, getfullRCSname from rcsutil.c to here.
 *
 * Revision 3.1  82/10/18  14:51:28  wft
 * InitAdmin() now initializes StrictLocks=STRICT_LOCKING (def. in rcsbase.h).
 * renamed checkpath() to checkfullpath().
 */


#include "rcsbase.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>

extern char * rindex();
extern char * mktemp();
extern FILE * fopen();
extern char * getwd();         /* get working directory; forward decl       */
extern int    stat(), fstat();

extern FILE * finptr;          /* RCS input file descriptor                 */
extern FILE * frewrite;        /* New RCS file descriptor                   */
extern char * RCSfilename, * workfilename; /* filenames                     */
struct stat RCSstat, workstat; /* file status for RCS file and working file */
int    haveRCSstat,  haveworkstat; /* indicators if status availalble       */


char tempfilename [NCPFN+10];  /* used for derived file names               */
char sub1filename [NCPPN];     /* used for files path/file.sfx,v            */
char sub2filename [NCPPN];     /* used for files path/RCS/file.sfx,v        */
char semafilename [NCPPN];     /* name of semaphore file                    */
int  madesema;                 /* indicates whether a semaphore file has been set */
char * tfnames[10];            /* temp. file names to be unlinked when finished   */
int  freefilename;             /* index of next free file name in tfnames[]  */


struct compair {
        char * suffix, * comlead;
};

struct compair comtable[] = {
/* comtable pairs each filename suffix with a comment leader. The comment   */
/* leader is placed before each line generated by the $Log keyword. This    */
/* table is used to guess the proper comment leader from the working file's */
/* suffix during initial ci (see InitAdmin()). Comment leaders are needed   */
/* for languages without multiline comments; for others they are optional.  */
        "c",   " * ",   /* C           */
	"csh", "# ",    /* shell       */
        "e",   "# ",    /* efl         */
        "f",   "c ",    /* fortran     */
        "h",   " * ",   /* C-header    */
        "l",   " * ",   /* lex         NOTE: conflict between lex and franzlisp*/
        "mac", "; ",    /* macro       vms or dec-20 or pdp-11 macro */
	"me",  ".\\\" ",/* me-macros   t/nroff*/
	"mm",  ".\\\" ",/* mm-macros   t/nroff*/
	"ms",  ".\\\" ",/* ms-macros   t/nroff*/
        "p",   " * ",   /* pascal      */
	"pl",  "% ",	/* prolog      */
        "r",   "# ",    /* ratfor      */
        "red", "% ",    /* psl/rlisp   */

#ifdef sparc
        "s",   "! ",    /* assembler   */
#endif
#ifdef mc68000
        "s",   "| ",    /* assembler   */
#endif
#ifdef pdp11
        "s",   "/ ",    /* assembler   */
#endif
#ifdef vax
        "s",   "# ",    /* assembler   */
#endif
#ifdef i386
        "s",   "# ",    /* assembler   */
#endif

        "sh",  "# ",    /* shell       */
        "sl",  "% ",    /* psl         */
        "red", "% ",    /* psl/rlisp   */
        "cl",  ";;; ",  /* common lisp   */
        "ml",  "; ",    /* mocklisp    */
        "el",  "; ",    /* gnulisp     */
	"tex", "% ",	/* tex	       */
        "y",   " * ",   /* yacc        */
        "ye",  " * ",   /* yacc-efl    */
        "yr",  " * ",   /* yacc-ratfor */
        "",    "# ",    /* default for empty suffix */
        nil,   ""       /* default for unknown suffix; must always be last */
};


ffclose(fptr)
FILE * fptr;
/* Function: checks ferror(fptr) and aborts the program if there were
 * errors; otherwise closes fptr.
 */
{       if (ferror(fptr) || fclose(fptr)==EOF)
                faterror("File read or write error; file system full?");
}



int trysema(RCSname,makesema)
char * RCSname; int makesema;
/* Function: Checks whether a semaphore file exists for RCSname. If yes,
 * returns false. If not, creates one if makesema==true and returns true
 * if successful. If a semaphore file was created, madesema is set to true.
 * The name of the semaphore file is put into variable semafilename.
 */
{
        register char * tp, *sp, *lp;
        int fdesc;

        sp=RCSname;
        lp = rindex(sp,'/');
        if (lp==0) {
                semafilename[0]='.'; semafilename[1]='/';
                tp= &semafilename[2];
        } else {
                /* copy path */
                tp=semafilename;
                do *tp++ = *sp++; while (sp<=lp);
        }
        /*now insert `,' and append file name */
        *tp++ = ',';
        lp = rindex(sp, RCSSEP);
        while (sp<lp) *tp++ = *sp++;
        *tp++ = ','; *tp++ = '\0'; /* will be the same length as RCSname*/

        madesema = false;
        if (access(semafilename, 0) == 0) {
                error("RCS file %s is in use",RCSname);
                return false;
        }
        if (makesema) {
                if ((fdesc=creat(semafilename, 000)) == -1) {
                     error("Can't create semaphore file for RCS file %s",RCSname);
                     return false;
                } else
                     VOID close(fdesc);
                     madesema=true;
        }
        return true;
}


rmsema()
/* Function: delete the semaphore file if madeseam==true;
 * sets madesema to false.
 */
{
        if (madesema) {
                madesema=false;
                if (unlink(semafilename) == -1) {
                        error("Can't find semaphore file %s",semafilename);
                }
        }
}



InitCleanup()
{       freefilename =  0;  /* initialize pointer */
}


cleanup()
/* Function: closes input file and rewrite file.
 * Unlinks files in tfnames[], deletes semaphore file.
 */
{
        register int i;

        if (finptr!=NULL)   VOID fclose(finptr);
        if (frewrite!=NULL) VOID fclose(frewrite);
        for (i=0; i<freefilename; i++) {
            if (tfnames[i][0]!='\0')  VOID unlink(tfnames[i]);
        }
        InitCleanup();
        rmsema();
}


char * mktempfile(fullpath,filename)
register char * fullpath, * filename;
/* Function: Creates a unique filename using the process id and stores it
 * into a free slot in tfnames. The filename consists of the path contained
 * in fullpath concatenated with filename. filename should end in "XXXXXX".
 * Because of storage in tfnames, cleanup() can unlink the file later.
 * freefilename indicates the lowest unoccupied slot in tfnames.
 * Returns a pointer to the filename created.
 * Example use: mktempfile("/tmp/", somefilename)
 */
{
        register char * lastslash, *tp;
        if ((tp=tfnames[freefilename])==nil)
              tp=tfnames[freefilename] = talloc(NCPPN);
        if (fullpath!=nil && (lastslash=rindex(fullpath,'/'))!=0) {
                /* copy path */
                while (fullpath<=lastslash) *tp++ = *fullpath++;
        }
        while (*tp++ = *filename++);
        return (mktemp(tfnames[freefilename++]));
}




char * bindex(sp,c)
register char * sp, c;
/* Function: Finds the last occurrence of character c in string sp
 * and returns a pointer to the character just beyond it. If the
 * character doesn't occur in the string, sp is returned.
 */
{       register char * r;
        r = sp;
        while (*sp) {
                if (*sp++ == c) r=sp;
        }
        return r;
}





InitAdmin()
/* function: initializes an admin node */
{       register char * Suffix;
        register int i;

        Head=Dbranch=nil; AccessList=nil; Symbols=nil; Locks=nil;
        StrictLocks=STRICT_LOCKING;

        /* guess the comment leader from the suffix*/
        Suffix=bindex(workfilename, '.');
        if (Suffix==workfilename) Suffix= ""; /* empty suffix; will get default*/
        for (i=0;;i++) {
                if (comtable[i].suffix==nil) {
                        Comment=comtable[i].comlead; /*default*/
                        break;
                } elsif (strcmp(Suffix,comtable[i].suffix)==0) {
                        Comment=comtable[i].comlead; /*default*/
                        break;
                }
        }
        Lexinit(); /* Note: if finptr==NULL, reads nothing; only initializes*/
}



char * findpairfile(argc, argv, fname)
int argc; char * argv[], *fname;
/* Function: Given a filename fname, findpairfile scans argv for a pathname
 * ending in fname. If found, returns a pointer to the pathname, and sets
 * the corresponding pointer in argv to nil. Otherwise returns fname.
 * argc indicates the number of entries in argv. Some of them may be nil.
 */
{
        register char * * next, * match;
        register int count;

        for (next = argv, count = argc; count>0; next++,count--) {
                if ((*next != nil) && strcmp(bindex(*next,'/'),fname)==0) {
                        /* bindex finds the beginning of the file name stem */
                        match= *next;
                        *next=nil;
                        return match;
                }
        }
        return fname;
}


int pairfilenames(argc, argv, mustread, tostdout)
int argc; char ** argv; int mustread, tostdout;
/* Function: Pairs the filenames pointed to by argv; argc indicates
 * how many there are.
 * Places a pointer to the RCS filename into RCSfilename,
 * and a pointer to the name of the working file into workfilename.
 * If both the workfilename and the RCS filename are given, and tostdout
 * is true, a warning is printed.
 *
 * If the working file exists, places its status into workstat and
 * sets haveworkstat to 0; otherwise, haveworkstat is set to -1;
 * Similarly for the RCS file and the variables RCSstat and haveRCSstat.
 *
 * If the RCS file exists, it is opened for reading, the file pointer
 * is placed into finptr, and the admin-node is read in; returns 1.
 * If the RCS file does not exist and mustread==true, an error is printed
 * and 0 returned.
 * If the RCS file does not exist and mustread==false, the admin node
 * is initialized to empty (Head, AccessList, Locks, Symbols, StrictLocks, Dbranch)
 * and -1 returned.
 *
 * 0 is returned on all errors. Files that are directories are errors.
 * Also calls InitCleanup();
 */
{
        register char * sp, * tp;
        char * lastsep, * purefname, * pureRCSname;
        int opened, returncode;
        char * RCS1;
	char prefdir[NCPPN];

        if (*argv == nil) return 0; /* already paired filename */
	if (rindex(*argv,KDELIM)!=0) {
		/* KDELIM causes havoc in keyword expansion    */
		error("RCS file name may not contain %c",KDELIM);
		return 0;
	}
        InitCleanup();

        /* first check suffix to see whether it is an RCS file or not */
        purefname=bindex(*argv, '/'); /* skip path */
        lastsep=rindex(purefname, RCSSEP);
        if (lastsep!= 0 && *(lastsep+1)==RCSSUF && *(lastsep+2)=='\0') {
                /* RCS file name given*/
                RCS1=(*argv); pureRCSname=purefname;
                /* derive workfilename*/
                sp = purefname; tp=tempfilename;
                while (sp<lastsep) *tp++ = *sp++; *tp='\0';
                /* try to find workfile name among arguments */
                workfilename=findpairfile(argc-1,argv+1,tempfilename);
                if (strlen(pureRCSname)>NCPFN) {
                        error("RCS file name %s too long",RCS1);
                        return 0;
                }
        } else {
                /* working file given; now try to find RCS file */
                workfilename= *argv;
                /* derive RCS file name*/
                sp=purefname; tp=tempfilename;
                while (*tp++ = *sp++);
                *(tp-1)=RCSSEP; *tp++=RCSSUF; *tp++='\0';
                /* Try to find RCS file name among arguments*/
                RCS1=findpairfile(argc-1,argv+1,tempfilename);
                pureRCSname=bindex(RCS1, '/');
                if (strlen(pureRCSname)>NCPFN) {
                        error("working file name %s too long",workfilename);
                        return 0;
                }
        }
        /* now we have a (tentative) RCS filename in RCS1 and workfilename  */
        /* First, get status of workfilename */
        haveworkstat=stat(workfilename, &workstat);
        if ((haveworkstat==0) && ((workstat.st_mode & S_IFDIR) == S_IFDIR)) {
                diagnose("Directory %s ignored",workfilename);
                return 0;
        }
        /* Second, try to find the right RCS file */
        if (pureRCSname!=RCS1) {
                /* a path for RCSfile is given; single RCS file to look for */
                finptr=fopen(RCSfilename=RCS1, "r");
                if (finptr!=NULL) {
                    returncode=1;
                } else { /* could not open */
                    if (access(RCSfilename,0)==0) {
                        error("Can't open existing %s", RCSfilename);
                        return 0;
                    }
                    if (mustread) {
                        error("Can't find %s", RCSfilename);
                        return 0;
                    } else {
                        /* initialize if not mustread */
                        returncode = -1;
                    }
                }
        } else {
		/* no path for RCS file name. Prefix it with path of work */
		/* file if RCS file omitted. Make a second name including */
		/* RCSDIR and try to open that one first.                 */
		sub1filename[0]=sub2filename[0]= '\0';
		if (RCS1==tempfilename) {
			/* RCS file name not given; prepend work path */
			sp= *argv; tp= sub1filename;
			while (sp<purefname) *tp++ = *sp ++;
			*tp='\0';
			VOID strcpy(sub2filename,sub1filename); /* second one */
		}
		VOID strcat(sub1filename,RCSDIR);
		VOID strcpy(prefdir,sub1filename); /* preferred directory for RCS file*/
		VOID strcat(sub1filename,RCS1); VOID strcat(sub2filename,RCS1);


                opened=(
		((finptr=fopen(RCSfilename=sub1filename, "r"))!=NULL) ||
		((finptr=fopen(RCSfilename=sub2filename,"r"))!=NULL) );

                if (opened) {
                        /* open succeeded */
                        returncode=1;
                } else {
                        /* open failed; may be read protected */
			if ((access(RCSfilename=sub1filename,0)==0) ||
			    (access(RCSfilename=sub2filename,0)==0)) {
                                error("Can't open existing %s",RCSfilename);
                                return 0;
                        }
                        if (mustread) {
				error("Can't find %s nor %s",sub1filename,sub2filename);
                                return 0;
                        } else {
                                /* initialize new file. Put into ./RCS if possible, strip off suffix*/
				RCSfilename= (access(prefdir,0)==0)?sub1filename:sub2filename;
                                returncode= -1;
                        }
                }
        }

        if (returncode == 1) { /* RCS file open */
                haveRCSstat=fstat(fileno(finptr),&RCSstat);
                if ((haveRCSstat== 0) && ((RCSstat.st_mode & S_IFDIR) == S_IFDIR)) {
                        diagnose("Directory %s ignored",RCSfilename);
                        return 0;
                }
                Lexinit(); getadmin();
        } else {  /* returncode == -1; RCS file nonexisting */
                haveRCSstat = -1;
                InitAdmin();
        };

        if (tostdout&&
            !(RCS1==tempfilename||workfilename==tempfilename))
                /*The last term determines whether a pair of        */
                /* file names was given in the argument list        */
                warn("Option -p is set; ignoring output file %s",workfilename);

        return returncode;
}


char * getfullRCSname()
/* Function: returns a pointer to the full path name of the RCS file.
 * Calls getwd(), but only once.
 * removes leading "../" and "./".
 */
{       static char pathbuf[NCPPN];
        static char namebuf[NCPPN];
        static int  pathlength;

        register char * realname, * lastpathchar;
        register int  dotdotcounter, realpathlength;

        if (*RCSfilename=='/') {
                return(RCSfilename);
        } else {
                if (pathlength==0) { /*call curdir for the first time*/
                    if (getwd(pathbuf)==NULL)
                        faterror("Can't build current directory path");
                    pathlength=strlen(pathbuf);
                    if (!((pathlength==1) && (pathbuf[0]=='/'))) {
                        pathbuf[pathlength++]='/';
                        /* Check needed because some getwd implementations */
                        /* generate "/" for the root.                      */
                    }
                }
                /*the following must be redone since RCSfilename may change*/
                /* find how many ../ to remvove from RCSfilename */
                dotdotcounter =0;
                realname = RCSfilename;
                while( realname[0]=='.' &&
                      (realname[1]=='/'||(realname[1]=='.'&&realname[2]=='/'))){
                        if (realname[1]=='/') {
                            /* drop leading ./ */
                            realname += 2;
                        } else {
                            /* drop leading ../ and remember */
                            dotdotcounter++;
                            realname += 3;
                        }
                }
                /* now remove dotdotcounter trailing directories from pathbuf*/
                lastpathchar=pathbuf + pathlength-1;
                while (dotdotcounter>0 && lastpathchar>pathbuf) {
                    /* move pointer backwards over trailing directory */
                    lastpathchar--;
                    if (*lastpathchar=='/') {
                        dotdotcounter--;
                    }
                }
                if (dotdotcounter>0) {
                    error("Can't generate full path name for RCS file");
                    return RCSfilename;
                } else {
                    /* build full path name */
                    realpathlength=lastpathchar-pathbuf+1;
                    VOID strncpy(namebuf,pathbuf,realpathlength);
                    VOID strcpy(&namebuf[realpathlength],realname);
                    return(namebuf);
                }
        }
}



int trydiraccess(filename)
char * filename;
/* checks write permission in directory of filename and returns
 * true if writable, false otherwise
 */
{
        char pathname[NCPPN];
        register char * tp, *sp, *lp;
        lp = rindex(filename,'/');
        if (lp==0) {
                /* check current directory */
                if (access(".",2)==0)
                        return true;
                else {
                        error("Current directory not writable");
                        return false;
                }
        }
        /* copy path */
        sp=filename;
        tp=pathname;
        do *tp++ = *sp++; while (sp<=lp);
        *tp='\0';
        if (access(pathname,2)==0)
                return true;
        else {
                error("Directory %s not writable", pathname);
                return false;
        }
}



#ifndef V4_2BSD
/* rename() and getwd() will be provided in bsd 4.2 */


int rename(from, to)
char * from, *to;
/* Function: renames a file with the name given by from to the name given by to.
 * unlinks the to-file if it already exists. returns -1 on error, 0 otherwise.
 */
{       VOID unlink(to);      /* no need to check return code; will be caught by link*/
                         /* no harm done if file "to" does not exist            */
        if (link(from,to)<0) return -1;
        return(unlink(from));
}



#define dot     "."
#define dotdot  ".."



char * getwd(name)
char * name;
/* Function: places full pathname of current working directory into name and
 * returns name on success, NULL on failure.
 * getwd is an adaptation of pwd. May not return to the current directory on
 * failure.
 */
{
        FILE    *file;
        struct  stat    d, dd;
        char buf[2];    /* to NUL-terminate dir.d_name */
        struct  direct  dir;

        int rdev, rino;
        int off;
        register i,j;

        name[off= 0] = '/';
        name[1] = '\0';
        buf[0] = '\0';
        if (stat("/", &d)<0) return NULL;
        rdev = d.st_dev;
        rino = d.st_ino;
        for (;;) {
                if (stat(dot, &d)<0) return NULL;
                if (d.st_ino==rino && d.st_dev==rdev) {
                        if (name[off] == '/') name[off] = '\0';
                        chdir(name); /*change back to current directory*/
                        return name;
                }
                if ((file = fopen(dotdot,"r")) == NULL) return NULL;
                if (fstat(fileno(file), &dd)<0) goto fail;
                chdir(dotdot);
                if(d.st_dev == dd.st_dev) {
                        if(d.st_ino == dd.st_ino) {
                            if (name[off] == '/') name[off] = '\0';
                            chdir(name); /*change back to current directory*/
                            VOID fclose(file);
                            return name;
                        }
                        do {
                            if (fread((char *)&dir, sizeof(dir), 1, file) !=1)
                                goto fail;
                        } while (dir.d_ino != d.st_ino);
                }
                else do {
                        if(fread((char *)&dir, sizeof(dir), 1, file) != 1) {
                            goto fail;
                        }
                        if (dir.d_ino == 0)
			    dd.st_ino = d.st_ino + 1;
                        else if (stat(dir.d_name, &dd) < 0)
			    goto fail;
                } while(dd.st_ino != d.st_ino || dd.st_dev != d.st_dev);
                VOID fclose(file);

                /* concatenate file name */
                i = -1;
                while (dir.d_name[++i] != 0);
                for(j=off+1; j>0; --j)
                        name[j+i+1] = name[j];
                off=i+off+1;
                name[i+1] = '/';
                for(--i; i>=0; --i)
                        name[i+1] = dir.d_name[i];
        } /* end for */

fail:   VOID fclose(file);
        return NULL;
}


#endif


#ifdef PAIRTEST
/* test program for pairfilenames() and getfullRCSname() */
char * workfilename, *RCSfilename;
extern int quietflag;

main(argc, argv)
int argc; char *argv[];
{
        int result;
        int initflag,tostdout;
        quietflag=tostdout=initflag=false;
        cmdid="pair";

        while(--argc, ++argv, argc>=1 && ((*argv)[0] == '-')) {
                switch ((*argv)[1]) {

                case 'p':       tostdout=true;
                                break;
                case 'i':       initflag=true;
                                break;
                case 'q':       quietflag=true;
                                break;
                default:        error("unknown option: %s", *argv);
                                break;
                }
        }

        do {
                RCSfilename=workfilename=nil;
                result=pairfilenames(argc,argv,!initflag,tostdout);
                if (result!=0) {
                     diagnose("RCSfile: %s; working file: %s",RCSfilename,workfilename);
                     diagnose("Full RCS file name: %s", getfullRCSname());
                }
                switch (result) {
                        case 0: continue; /* already paired file */

                        case 1: if (initflag) {
                                    error("RCS file %s exists already",RCSfilename);
                                } else {
                                    diagnose("RCS file %s exists",RCSfilename);
                                }
                                VOID fclose(finptr);
                                break;

                        case -1:diagnose("RCS file does not exist");
                                break;
                }

        } while (++argv, --argc>=1);

}
#endif
