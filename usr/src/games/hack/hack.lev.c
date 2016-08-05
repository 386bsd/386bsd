/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.lev.c - version 1.0.3 */

#include "hack.h"
#include "def.mkroom.h"
#include <stdio.h>
extern struct monst *restmonchn();
extern struct obj *restobjchn();
extern struct obj *billobjs;
extern char *itoa();
extern char SAVEF[];
extern int hackpid;
extern xchar dlevel;
extern char nul[];

#ifndef NOWORM
#include	"def.wseg.h"
extern struct wseg *wsegs[32], *wheads[32];
extern long wgrowtime[32];
#endif NOWORM

boolean level_exists[MAXLEVEL+1];

savelev(fd,lev)
int fd;
xchar lev;
{
#ifndef NOWORM
	register struct wseg *wtmp, *wtmp2;
	register tmp;
#endif NOWORM

	if(fd < 0) panic("Save on bad file!");	/* impossible */
	if(lev >= 0 && lev <= MAXLEVEL)
		level_exists[lev] = TRUE;

	bwrite(fd,(char *) &hackpid,sizeof(hackpid));
	bwrite(fd,(char *) &lev,sizeof(lev));
	bwrite(fd,(char *) levl,sizeof(levl));
	bwrite(fd,(char *) &moves,sizeof(long));
	bwrite(fd,(char *) &xupstair,sizeof(xupstair));
	bwrite(fd,(char *) &yupstair,sizeof(yupstair));
	bwrite(fd,(char *) &xdnstair,sizeof(xdnstair));
	bwrite(fd,(char *) &ydnstair,sizeof(ydnstair));
	savemonchn(fd, fmon);
	savegoldchn(fd, fgold);
	savetrapchn(fd, ftrap);
	saveobjchn(fd, fobj);
	saveobjchn(fd, billobjs);
	billobjs = 0;
	save_engravings(fd);
#ifndef QUEST
	bwrite(fd,(char *) rooms,sizeof(rooms));
	bwrite(fd,(char *) doors,sizeof(doors));
#endif QUEST
	fgold = 0;
	ftrap = 0;
	fmon = 0;
	fobj = 0;
#ifndef NOWORM
	bwrite(fd,(char *) wsegs,sizeof(wsegs));
	for(tmp=1; tmp<32; tmp++){
		for(wtmp = wsegs[tmp]; wtmp; wtmp = wtmp2){
			wtmp2 = wtmp->nseg;
			bwrite(fd,(char *) wtmp,sizeof(struct wseg));
		}
		wsegs[tmp] = 0;
	}
	bwrite(fd,(char *) wgrowtime,sizeof(wgrowtime));
#endif NOWORM
}

bwrite(fd,loc,num)
register fd;
register char *loc;
register unsigned num;
{
/* lint wants the 3rd arg of write to be an int; lint -p an unsigned */
	if(write(fd, loc, (int) num) != num)
		panic("cannot write %u bytes to file #%d", num, fd);
}

saveobjchn(fd,otmp)
register fd;
register struct obj *otmp;
{
	register struct obj *otmp2;
	unsigned xl;
	int minusone = -1;

	while(otmp) {
		otmp2 = otmp->nobj;
		xl = otmp->onamelth;
		bwrite(fd, (char *) &xl, sizeof(int));
		bwrite(fd, (char *) otmp, xl + sizeof(struct obj));
		free((char *) otmp);
		otmp = otmp2;
	}
	bwrite(fd, (char *) &minusone, sizeof(int));
}

savemonchn(fd,mtmp)
register fd;
register struct monst *mtmp;
{
	register struct monst *mtmp2;
	unsigned xl;
	int minusone = -1;
	struct permonst *monbegin = &mons[0];

	bwrite(fd, (char *) &monbegin, sizeof(monbegin));

	while(mtmp) {
		mtmp2 = mtmp->nmon;
		xl = mtmp->mxlth + mtmp->mnamelth;
		bwrite(fd, (char *) &xl, sizeof(int));
		bwrite(fd, (char *) mtmp, xl + sizeof(struct monst));
		if(mtmp->minvent) saveobjchn(fd,mtmp->minvent);
		free((char *) mtmp);
		mtmp = mtmp2;
	}
	bwrite(fd, (char *) &minusone, sizeof(int));
}

savegoldchn(fd,gold)
register fd;
register struct gold *gold;
{
	register struct gold *gold2;
	while(gold) {
		gold2 = gold->ngold;
		bwrite(fd, (char *) gold, sizeof(struct gold));
		free((char *) gold);
		gold = gold2;
	}
	bwrite(fd, nul, sizeof(struct gold));
}

savetrapchn(fd,trap)
register fd;
register struct trap *trap;
{
	register struct trap *trap2;
	while(trap) {
		trap2 = trap->ntrap;
		bwrite(fd, (char *) trap, sizeof(struct trap));
		free((char *) trap);
		trap = trap2;
	}
	bwrite(fd, nul, sizeof(struct trap));
}

getlev(fd,pid,lev)
int fd,pid;
xchar lev;
{
	register struct gold *gold;
	register struct trap *trap;
#ifndef NOWORM
	register struct wseg *wtmp;
#endif NOWORM
	register tmp;
	long omoves;
	int hpid;
	xchar dlvl;

	/* First some sanity checks */
	mread(fd, (char *) &hpid, sizeof(hpid));
	mread(fd, (char *) &dlvl, sizeof(dlvl));
	if((pid && pid != hpid) || (lev && dlvl != lev)) {
		pline("Strange, this map is not as I remember it.");
		pline("Somebody is trying some trickery here ...");
		pline("This game is void ...");
		done("tricked");
	}

	fgold = 0;
	ftrap = 0;
	mread(fd, (char *) levl, sizeof(levl));
	mread(fd, (char *)&omoves, sizeof(omoves));
	mread(fd, (char *)&xupstair, sizeof(xupstair));
	mread(fd, (char *)&yupstair, sizeof(yupstair));
	mread(fd, (char *)&xdnstair, sizeof(xdnstair));
	mread(fd, (char *)&ydnstair, sizeof(ydnstair));

	fmon = restmonchn(fd);

	/* regenerate animals while on another level */
	{ long tmoves = (moves > omoves) ? moves-omoves : 0;
	  register struct monst *mtmp, *mtmp2;
	  extern char genocided[];

	  for(mtmp = fmon; mtmp; mtmp = mtmp2) {
		long newhp;		/* tmoves may be very large */

		mtmp2 = mtmp->nmon;
		if(index(genocided, mtmp->data->mlet)) {
			mondead(mtmp);
			continue;
		}

		if(mtmp->mtame && tmoves > 250) {
			mtmp->mtame = 0;
			mtmp->mpeaceful = 0;
		}

		newhp = mtmp->mhp +
			(index(MREGEN, mtmp->data->mlet) ? tmoves : tmoves/20);
		if(newhp > mtmp->mhpmax)
			mtmp->mhp = mtmp->mhpmax;
		else
			mtmp->mhp = newhp;
	  }
	}

	setgd();
	gold = newgold();
	mread(fd, (char *)gold, sizeof(struct gold));
	while(gold->gx) {
		gold->ngold = fgold;
		fgold = gold;
		gold = newgold();
		mread(fd, (char *)gold, sizeof(struct gold));
	}
	free((char *) gold);
	trap = newtrap();
	mread(fd, (char *)trap, sizeof(struct trap));
	while(trap->tx) {
		trap->ntrap = ftrap;
		ftrap = trap;
		trap = newtrap();
		mread(fd, (char *)trap, sizeof(struct trap));
	}
	free((char *) trap);
	fobj = restobjchn(fd);
	billobjs = restobjchn(fd);
	rest_engravings(fd);
#ifndef QUEST
	mread(fd, (char *)rooms, sizeof(rooms));
	mread(fd, (char *)doors, sizeof(doors));
#endif QUEST
#ifndef NOWORM
	mread(fd, (char *)wsegs, sizeof(wsegs));
	for(tmp = 1; tmp < 32; tmp++) if(wsegs[tmp]){
		wheads[tmp] = wsegs[tmp] = wtmp = newseg();
		while(1) {
			mread(fd, (char *)wtmp, sizeof(struct wseg));
			if(!wtmp->nseg) break;
			wheads[tmp]->nseg = wtmp = newseg();
			wheads[tmp] = wtmp;
		}
	}
	mread(fd, (char *)wgrowtime, sizeof(wgrowtime));
#endif NOWORM
}

mread(fd, buf, len)
register fd;
register char *buf;
register unsigned len;
{
	register int rlen;
	extern boolean restoring;

	rlen = read(fd, buf, (int) len);
	if(rlen != len){
		pline("Read %d instead of %u bytes.\n", rlen, len);
		if(restoring) {
			(void) unlink(SAVEF);
			error("Error restoring old game.");
		}
		panic("Error reading level file.");
	}
}

mklev()
{
	extern boolean in_mklev;

	if(getbones()) return;

	in_mklev = TRUE;
	makelevel();
	in_mklev = FALSE;
}
