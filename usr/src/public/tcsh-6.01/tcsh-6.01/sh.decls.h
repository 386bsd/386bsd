/* $Header: /home/hyperion/mu/christos/src/sys/tcsh-6.01/RCS/sh.decls.h,v 3.7 1991/12/14 20:45:46 christos Exp $ */
/*
 * sh.decls.h	 External declarations from sh*.c
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
#ifndef _h_sh_decls
#define _h_sh_decls

/*
 * sh.c
 */
extern	int	 	  gethdir	__P((Char *));
extern	void		  dosource	__P((Char **, struct command *));
extern	void		  exitstat	__P((void));
extern	void		  goodbye	__P((Char **, struct command *));
extern	void		  importpath	__P((Char *));
extern	void		  initdesc	__P((void));
extern	sigret_t	  pintr		__P((int));
extern	void		  pintr1	__P((bool));
extern	void		  process	__P((bool));
extern	void		  rechist	__P((void));
extern	void		  untty		__P((void));
#ifdef PROF
extern	void		  done		__P((int));
#else
extern	void		  xexit		__P((int));
#endif

/*
 * sh.dir.c
 */
extern	void		  dinit		__P((Char *));
extern	void		  dodirs	__P((Char **, struct command *));
extern	Char		 *dcanon	__P((Char *, Char *));
extern	void		  dtildepr	__P((Char *, Char *));
extern	void		  dtilde	__P((void));
extern	void		  dochngd	__P((Char **, struct command *));
extern	Char		 *dnormalize	__P((Char *));
extern	void		  dopushd	__P((Char **, struct command *));
extern	void		  dopopd	__P((Char **, struct command *));
extern	void		  dfree		__P((struct directory *));
extern	int		  getstakd	__P((Char *, int));
extern	void		  dextract	__P((struct directory *));
#ifdef	CSHDIRS
extern	void		  recdirs	__P((void));
#endif

/*
 * sh.dol.c
 */
extern	void		  Dfix		__P((struct command *));
extern	Char		 *Dfix1		__P((Char *));
extern	void		  heredoc	__P((Char *));

/*
 * sh.err.c
 */
extern	void		  seterror	__P((unsigned int, ...));
extern	void		  stderror	__P((unsigned int, ...));

/*
 * sh.exec.c
 */
extern	void		  doexec	__P((struct command *));
extern	void		  dohash	__P((Char **, struct command *));
extern	void		  dounhash	__P((Char **, struct command *));
extern	void		  execash	__P((Char **, struct command *));
#ifdef VFORK
extern	void		  hashstat	__P((Char **, struct command *));
#endif
extern	void		  xechoit	__P((Char **));
extern	int		  iscommand	__P((Char *));
extern	int		  executable	__P((Char *, Char *, bool));
extern	void		  tellmewhat	__P((struct wordent *));
extern	void		  dowhere	__P((Char **, struct command *));

/*
 * sh.exp.c
 */
extern	int	 	  expr		__P((Char ***));
extern	int		  exp0		__P((Char ***, bool));

/*
 * sh.file.c
 */
#ifdef FILEC
extern	int		  tenex		__P((Char *, int));
#endif

/*
 * sh.func.c
 */
extern	void		  Setenv	__P((Char *, Char *));
extern	void		  Unsetenv	__P((Char *));
extern	void		  doalias	__P((Char **, struct command *));
extern	void		  dobreak	__P((Char **, struct command *));
extern	void		  docontin	__P((Char **, struct command *));
extern	void		  doecho	__P((Char **, struct command *));
extern	void		  doelse	__P((Char **, struct command *));
extern	void		  doend		__P((Char **, struct command *));
extern	void		  doeval	__P((Char **, struct command *));
extern	void		  doexit	__P((Char **, struct command *));
extern	void		  doforeach	__P((Char **, struct command *));
extern	void		  doglob	__P((Char **, struct command *));
extern	void		  dogoto	__P((Char **, struct command *));
extern	void		  doif		__P((Char **, struct command *));
extern	void		  dolimit	__P((Char **, struct command *));
extern	void		  dologin	__P((Char **, struct command *));
extern	void		  dologout	__P((Char **, struct command *));
#ifdef NEWGRP
extern	void		  donewgrp	__P((Char **, struct command *));
#endif
extern	void		  donohup	__P((Char **, struct command *));
extern	void		  doonintr	__P((Char **, struct command *));
extern	void		  dorepeat	__P((Char **, struct command *));
extern	void		  dosetenv	__P((Char **, struct command *));
extern	void		  dosuspend	__P((Char **, struct command *));
extern	void		  doswbrk	__P((Char **, struct command *));
extern	void		  doswitch	__P((Char **, struct command *));
extern	void		  doumask	__P((Char **, struct command *));
extern	void		  dounlimit	__P((Char **, struct command *));
extern	void		  dounsetenv	__P((Char **, struct command *));
extern	void		  dowhile	__P((Char **, struct command *));
extern	void		  dozip		__P((Char **, struct command *));
extern	void		  func		__P((struct command *, 
					     struct biltins *));
extern	void		  gotolab	__P((Char *));
extern struct biltins 	 *isbfunc	__P((struct command *));
extern	void		  prvars	__P((void));
extern	int		  srchx		__P((Char *));
extern	void		  unalias	__P((Char **, struct command *));
extern	void		  wfree		__P((void));

/*
 * sh.glob.c
 */
extern	Char		**dobackp	__P((Char *, bool));
extern	void		  Gcat		__P((Char *, Char *));
extern	Char		 *globone	__P((Char *, int));
extern	int		  Gmatch	__P((Char *, Char *));
extern	void		  ginit		__P((void));
extern	Char		**globall	__P((Char **));
extern	void		  rscan		__P((Char **, void (*)()));
extern	void		  tglob		__P((Char **));
extern	void		  trim		__P((Char **));
#ifdef FILEC
extern	int		  sortscmp	__P((Char **, Char **));
#endif

/*
 * sh.hist.c
 */
extern	void	 	  dohist	__P((Char **, struct command *));
extern struct Hist 	 *enthist	__P((int, struct wordent *, bool));
extern	void	 	  savehist	__P((struct wordent *));


/*
 * sh.lex.c
 */
extern	void		  addla		__P((Char *));
extern	void		  bseek		__P((struct Ain *));
extern	void		  btell		__P((struct Ain *));
extern	void		  btoeof	__P((void));
extern	void		  copylex	__P((struct wordent *, 
					     struct wordent *));
extern	Char		 *domod		__P((Char *, int));
extern	void		  freelex	__P((struct wordent *));
extern	int		  lex		__P((struct wordent *));
extern	void		  prlex		__P((struct wordent *));
extern	int		  readc		__P((bool));
extern	void		  settell	__P((void));
extern	void		  unreadc	__P((int));


/*
 * sh.misc.c
 */
extern	int		  any		__P((char *, int));
extern	Char		**blkcat	__P((Char **, Char **));
extern	Char		**blkcpy	__P((Char **, Char **));
extern	Char		**blkend	__P((Char **));
extern	void		  blkfree	__P((Char **));
extern	int		  blklen	__P((Char **));
extern	void		  blkpr		__P((Char **));
extern	Char		**blkspl	__P((Char **, Char **));
#ifndef copy
extern  void		  copy		__P((char *, char *, int));
#endif
extern	void		  closem	__P((void));
#ifndef FIOCLEX
extern  void 		  closech	__P((void));
#endif
extern	Char		**copyblk	__P((Char **));
extern	int		  dcopy		__P((int, int));
extern	int		  dmove		__P((int, int));
extern	void		  donefds	__P((void));
extern	Char		  lastchr	__P((Char *));
extern	void		  lshift	__P((Char **, int));
extern	int		  number	__P((Char *));
extern	int		  prefix	__P((Char *, Char *));
extern	Char		**saveblk	__P((Char **));
extern	void		  setzero	__P((char *, int));
extern	Char		 *strip		__P((Char *));
extern	char		 *strsave	__P((const char *));
extern	char		 *strspl	__P((char *, char *));
#ifndef POSIX
extern  char   	  	 *strstr	__P((const char *, const char *));
#endif
extern	void		  udvar		__P((Char *));
#ifndef SHORT_STRINGS
extern	char		 *strend	__P((char *));
#endif

/*
 * sh.parse.c
 */
extern	void		  alias		__P((struct wordent *));
extern	void		  freesyn	__P((struct command *));
extern struct command 	 *syntax	__P((struct wordent *, 
					     struct wordent *, int));

/*
 * sh.print.c
 */
extern	void		  draino	__P((void));
extern	void		  flush		__P((void));
#ifdef BSDTIMES
extern	void		  pcsecs	__P((long));
#else /* !BSDTIMES */
# ifdef POSIX
extern	void		  pcsecs	__P((clock_t));
# else /* !POSIX */
extern	void		  pcsecs	__P((time_t));
# endif /* !POSIX */
#endif /* BSDTIMES */
#ifdef RLIMIT_CPU
extern	void		  psecs		__P((long));
#endif /* RLIMIT_CPU */
extern	int		  putpure	__P((int));
extern	int		  putraw	__P((int));
extern	void		  xputchar	__P((int));


/*
 * sh.proc.c
 */
extern	void		  dobg		__P((Char **, struct command *));
extern	void		  dobg1		__P((Char **, struct command *));
extern	void		  dofg		__P((Char **, struct command *));
extern	void		  dofg1		__P((Char **, struct command *));
extern	void		  dojobs	__P((Char **, struct command *));
extern	void		  dokill	__P((Char **, struct command *));
extern	void		  donotify	__P((Char **, struct command *));
extern	void		  dostop	__P((Char **, struct command *));
extern	void		  dowait	__P((Char **, struct command *));
extern	void		  palloc	__P((int, struct command *));
extern	void		  panystop	__P((bool));
extern	sigret_t	  pchild	__P((int));
extern	void		  pendjob	__P((void));
extern	struct process 	 *pfind		__P((Char *));
extern	int		  pfork		__P((struct command *, int));
extern	void		  pgetty	__P((int, int));
extern	void		  pjwait	__P((struct process *));
extern	void		  pnote		__P((void));
extern	void		  prestjob	__P((void));
extern	void		  psavejob	__P((void));
extern	void		  pstart	__P((struct process *, int));
extern	void		  pwait		__P((void));

/*
 * sh.sem.c
 */
extern	void		  execute	__P((struct command *, int, int *, 
					     int *));
extern	void		  mypipe	__P((int *));

/*
 * sh.set.c
 */
extern	struct varent 	 *adrof1	__P((Char *, struct varent *));
extern	void		  doset		__P((Char **, struct command *));
extern	void		  dolet		__P((Char **, struct command *));
extern	Char		 *putn		__P((int));
extern	int		  getn		__P((Char *));
extern	Char		 *value1	__P((Char *, struct varent *));
extern	void		  set		__P((Char *, Char *));
extern	void		  set1		__P((Char *, Char **, struct varent *));
extern	void		  setq		__P((Char *, Char **, struct varent *));
extern	void		  unset		__P((Char **, struct command *));
extern	void		  unset1	__P((Char *[], struct varent *));
extern	void		  unsetv	__P((Char *));
extern	void		  setNS		__P((Char *));
extern	void		  shift		__P((Char **, struct command *));
extern	void		  plist		__P((struct varent *));

/*
 * sh.time.c
 */
extern	void		  donice	__P((Char **, struct command *));
extern	void		  dotime	__P((Char **, struct command *));
#ifdef BSDTIMES
extern	void		  prusage	__P((struct rusage *, struct rusage *, 
					     timeval_t *, timeval_t *));
#else /* BSDTIMES */
# ifdef _SEQUENT_
extern	void		  prusage	__P((struct process_stats *,
					     struct process_stats *, 
					     timeval_t *, timeval_t *));
# else /* !_SEQUENT_ */
#  ifdef POSIX
extern	void		  prusage	__P((struct tms *, struct tms *, 
					     clock_t, clock_t));
#  else	/* !POSIX */
extern	void		  prusage	__P((struct tms *, struct tms *, 
					     time_t, time_t));
#  endif /* !POSIX */
# endif	/* !_SEQUENT_ */
#endif /* BSDTIMES */
extern	void		  settimes	__P((void));
#if defined(BSDTIMES) || defined(_SEQUENT_)
extern	void		  ruadd		__P((struct rusage *, struct rusage *));
extern	void		  tvadd		__P((struct timeval *, 
					     struct timeval *));
extern	void		  tvsub		__P((struct timeval *, 
					     struct timeval *, 
					     struct timeval *));
#endif /* BSDTIMES || _SEQUENT_ */

#endif /* _h_sh_decls */
