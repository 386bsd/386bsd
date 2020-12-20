/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.vault.c - version 1.0.2 */

#include	"hack.h"
#ifdef QUEST
setgd(/* mtmp */) /* struct monst *mtmp; */ {}
gd_move() { return(2); }
gddead(mtmp) struct monst *mtmp; {}
replgd(mtmp,mtmp2) struct monst *mtmp, *mtmp2; {}
invault(){}

#else


#include "def.mkroom.h"
extern struct monst *makemon();
#define	FCSIZ	(ROWNO+COLNO)
struct fakecorridor {
	xchar fx,fy,ftyp;
};

struct egd {
	int fcbeg, fcend;	/* fcend: first unused pos */
	xchar gdx, gdy;		/* goal of guard's walk */
	unsigned gddone:1;
	struct fakecorridor fakecorr[FCSIZ];
};

static struct permonst pm_guard =
	{ "guard", '@', 12, 12, -1, 4, 10, sizeof(struct egd) };

static struct monst *guard;
static int gdlevel;
#define	EGD	((struct egd *)(&(guard->mextra[0])))

static
restfakecorr()
{
	register fcx,fcy,fcbeg;
	register struct rm *crm;

	while((fcbeg = EGD->fcbeg) < EGD->fcend) {
		fcx = EGD->fakecorr[fcbeg].fx;
		fcy = EGD->fakecorr[fcbeg].fy;
		if((u.ux == fcx && u.uy == fcy) || cansee(fcx,fcy) ||
		   m_at(fcx,fcy))
			return;
		crm = &levl[fcx][fcy];
		crm->typ = EGD->fakecorr[fcbeg].ftyp;
		if(!crm->typ) crm->seen = 0;
		newsym(fcx,fcy);
		EGD->fcbeg++;
	}
	/* it seems he left the corridor - let the guard disappear */
	mondead(guard);
	guard = 0;
}

static
goldincorridor()
{
	register int fci;

	for(fci = EGD->fcbeg; fci < EGD->fcend; fci++)
		if(g_at(EGD->fakecorr[fci].fx, EGD->fakecorr[fci].fy))
			return(1);
	return(0);
}

setgd(){
register struct monst *mtmp;
	for(mtmp = fmon; mtmp; mtmp = mtmp->nmon) if(mtmp->isgd){
		guard = mtmp;
		gdlevel = dlevel;
		return;
	}
	guard = 0;
}

invault(){
register tmp = inroom(u.ux, u.uy);
    if(tmp < 0 || rooms[tmp].rtype != VAULT) {
	u.uinvault = 0;
	return;
    }
    if(++u.uinvault % 50 == 0 && (!guard || gdlevel != dlevel)) {
	char buf[BUFSZ];
	register x,y,dd,gx,gy;

	/* first find the goal for the guard */
	for(dd = 1; (dd < ROWNO || dd < COLNO); dd++) {
	  for(y = u.uy-dd; y <= u.uy+dd; y++) {
	    if(y < 0 || y > ROWNO-1) continue;
	    for(x = u.ux-dd; x <= u.ux+dd; x++) {
	      if(y != u.uy-dd && y != u.uy+dd && x != u.ux-dd)
		x = u.ux+dd;
	      if(x < 0 || x > COLNO-1) continue;
	      if(levl[x][y].typ == CORR) goto fnd;
	    }
	  }
	}
	impossible("Not a single corridor on this level??");
	tele();
	return;
fnd:
	gx = x; gy = y;

	/* next find a good place for a door in the wall */
	x = u.ux; y = u.uy;
	while(levl[x][y].typ == ROOM) {
		register int dx,dy;

		dx = (gx > x) ? 1 : (gx < x) ? -1 : 0;
		dy = (gy > y) ? 1 : (gy < y) ? -1 : 0;
		if(abs(gx-x) >= abs(gy-y))
			x += dx;
		else
			y += dy;
	}

	/* make something interesting happen */
	if(!(guard = makemon(&pm_guard,x,y))) return;
	guard->isgd = guard->mpeaceful = 1;
	EGD->gddone = 0;
	gdlevel = dlevel;
	if(!cansee(guard->mx, guard->my)) {
		mondead(guard);
		guard = 0;
		return;
	}

	pline("Suddenly one of the Vault's guards enters!");
	pmon(guard);
	do {
		pline("\"Hello stranger, who are you?\" - ");
		getlin(buf);
	} while (!letter(buf[0]));

	if(!strcmp(buf, "Croesus") || !strcmp(buf, "Kroisos")) {
		pline("\"Oh, yes - of course. Sorry to have disturbed you.\"");
		mondead(guard);
		guard = 0;
		return;
	}
	clrlin();
	pline("\"I don't know you.\"");
	if(!u.ugold)
	    pline("\"Please follow me.\"");
	else {
	    pline("\"Most likely all that gold was stolen from this vault.\"");
	    pline("\"Please drop your gold (say d$ ) and follow me.\"");
	}
	EGD->gdx = gx;
	EGD->gdy = gy;
	EGD->fcbeg = 0;
	EGD->fakecorr[0].fx = x;
	EGD->fakecorr[0].fy = y;
	EGD->fakecorr[0].ftyp = levl[x][y].typ;
	levl[x][y].typ = DOOR;
	EGD->fcend = 1;
    }
}

gd_move(){
register int x,y,dx,dy,gx,gy,nx,ny,typ;
register struct fakecorridor *fcp;
register struct rm *crm;
	if(!guard || gdlevel != dlevel){
		impossible("Where is the guard?");
		return(2);	/* died */
	}
	if(u.ugold || goldincorridor())
		return(0);	/* didnt move */
	if(dist(guard->mx,guard->my) > 1 || EGD->gddone) {
		restfakecorr();
		return(0);	/* didnt move */
	}
	x = guard->mx;
	y = guard->my;
	/* look around (hor & vert only) for accessible places */
	for(nx = x-1; nx <= x+1; nx++) for(ny = y-1; ny <= y+1; ny++) {
	    if(nx == x || ny == y) if(nx != x || ny != y)
	    if(isok(nx,ny))
	    if(!IS_WALL(typ = (crm = &levl[nx][ny])->typ) && typ != POOL) {
		register int i;
		for(i = EGD->fcbeg; i < EGD->fcend; i++)
			if(EGD->fakecorr[i].fx == nx &&
			   EGD->fakecorr[i].fy == ny)
				goto nextnxy;
		if((i = inroom(nx,ny)) >= 0 && rooms[i].rtype == VAULT)
			goto nextnxy;
		/* seems we found a good place to leave him alone */
		EGD->gddone = 1;
		if(ACCESSIBLE(typ)) goto newpos;
		crm->typ = (typ == SCORR) ? CORR : DOOR;
		goto proceed;
	    }
    nextnxy:	;
	}
	nx = x;
	ny = y;
	gx = EGD->gdx;
	gy = EGD->gdy;
	dx = (gx > x) ? 1 : (gx < x) ? -1 : 0;
	dy = (gy > y) ? 1 : (gy < y) ? -1 : 0;
	if(abs(gx-x) >= abs(gy-y)) nx += dx; else ny += dy;

	while((typ = (crm = &levl[nx][ny])->typ) != 0) {
	/* in view of the above we must have IS_WALL(typ) or typ == POOL */
	/* must be a wall here */
		if(isok(nx+nx-x,ny+ny-y) && typ != POOL &&
		    ZAP_POS(levl[nx+nx-x][ny+ny-y].typ)){
			crm->typ = DOOR;
			goto proceed;
		}
		if(dy && nx != x) {
			nx = x; ny = y+dy;
			continue;
		}
		if(dx && ny != y) {
			ny = y; nx = x+dx; dy = 0;
			continue;
		}
		/* I don't like this, but ... */
		crm->typ = DOOR;
		goto proceed;
	}
	crm->typ = CORR;
proceed:
	if(cansee(nx,ny)) {
		mnewsym(nx,ny);
		prl(nx,ny);
	}
	fcp = &(EGD->fakecorr[EGD->fcend]);
	if(EGD->fcend++ == FCSIZ) panic("fakecorr overflow");
	fcp->fx = nx;
	fcp->fy = ny;
	fcp->ftyp = typ;
newpos:
	if(EGD->gddone) nx = ny = 0;
	guard->mx = nx;
	guard->my = ny;
	pmon(guard);
	restfakecorr();
	return(1);
}

gddead(){
	guard = 0;
}

replgd(mtmp,mtmp2)
register struct monst *mtmp, *mtmp2;
{
	if(mtmp == guard)
		guard = mtmp2;
}

#endif QUEST
