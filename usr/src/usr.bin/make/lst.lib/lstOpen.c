/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
static char sccsid[] = "@(#)lstOpen.c	5.3 (Berkeley) 6/1/90";
#endif /* not lint */

/*-
 * LstOpen.c --
 *	Open a list for sequential access. The sequential functions access the
 *	list in a slightly different way. CurPtr points to their idea of the
 *	current node in the list and they access the list based on it.
 *	If the list is circular, Lst_Next and Lst_Prev will go around
 *	the list forever. Lst_IsAtEnd must be used to determine when to stop.
 */

#include	"lstInt.h"

/*-
 *-----------------------------------------------------------------------
 * Lst_Open --
 *	Open a list for sequential access. A list can still be searched,
 *	etc., without confusing these functions.
 *
 * Results:
 *	SUCCESS or FAILURE.
 *
 * Side Effects:
 *	isOpen is set TRUE and curPtr is set to NilListNode so the
 *	other sequential functions no it was just opened and can choose
 *	the first element accessed based on this.
 *
 *-----------------------------------------------------------------------
 */
ReturnStatus
Lst_Open (l)
	register Lst	l;
{
	if (LstValid (l) == FALSE) {
		return (FAILURE);
	}
	((List) l)->isOpen = TRUE;
	((List) l)->atEnd = LstIsEmpty (l) ? Head : Unknown;
	((List) l)->curPtr = NilListNode;

	return (SUCCESS);
}

