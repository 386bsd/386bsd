/*
 * Copyright (c) 1983 Regents of the University of California.
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

#ifndef lint
static char sccsid[] = "@(#)game.c	5.4 (Berkeley) 6/1/90";
#endif /* not lint */

#include "externs.h"

maxturns(ship, af)
register struct ship *ship;
char *af;
{
	register int turns;

	turns = ship->specs->ta;
	if (*af = (ship->file->drift > 1 && turns)) {
		turns--;
		if (ship->file->FS == 1)
			turns = 0;
	}
	return turns;
}

maxmove(ship, dir, fs)
register struct ship *ship;
int dir, fs;
{
	register int riggone = 0, Move, flank = 0;

	Move = ship->specs->bs;
	if (!ship->specs->rig1)
		riggone++;
	if (!ship->specs->rig2)
		riggone++;
	if (!ship->specs->rig3)
		riggone++;
	if (!ship->specs->rig4)
		riggone++;
	if ((ship->file->FS || fs) && fs != -1) {
		flank = 1;
		Move = ship->specs->fs;
	}
	if (dir == winddir)
		Move -= 1 + WET[windspeed][ship->specs->class-1].B;
	else if (dir == winddir + 2 || dir == winddir - 2 || dir == winddir - 6 || dir == winddir + 6)
		Move -= 1 + WET[windspeed][ship->specs->class-1].C;
	else if (dir == winddir + 3 || dir == winddir - 3 || dir == winddir - 5 || dir == winddir + 5)
		Move = (flank ? 2 : 1) - WET[windspeed][ship->specs->class-1].D;
	else if (dir == winddir + 4 || dir == winddir - 4)
		Move = 0;
	else 
		Move -= WET[windspeed][ship->specs->class-1].A;
	Move -= riggone;
	Move = Move < 0 ? 0 : Move;
	return(Move);
}
