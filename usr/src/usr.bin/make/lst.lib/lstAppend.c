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
static char sccsid[] = "@(#)lstAppend.c	5.3 (Berkeley) 6/1/90";
#endif /* not lint */

/*-
 * LstAppend.c --
 *	Add a new node with a new datum after an existing node
 */

#include	"lstInt.h"

/*-
 *-----------------------------------------------------------------------
 * Lst_Append --
 *	Create a new node and add it to the given list after the given node.
 *
 * Results:
 *	SUCCESS if all went well.
 *
 * Side Effects:
 *	A new ListNode is created and linked in to the List. The lastPtr
 *	field of the List will be altered if ln is the last node in the
 *	list. lastPtr and firstPtr will alter if the list was empty and
 *	ln was NILLNODE.
 *
 *-----------------------------------------------------------------------
 */
ReturnStatus
Lst_Append (l, ln, d)
    Lst	  	l;	/* affected list */
    LstNode	ln;	/* node after which to append the datum */
    ClientData	d;	/* said datum */
{
    register List 	list;
    register ListNode	lNode;
    register ListNode	nLNode;
    
    if (LstValid (l) && (ln == NILLNODE && LstIsEmpty (l))) {
	goto ok;
    }
    
    if (!LstValid (l) || LstIsEmpty (l)  || ! LstNodeValid (ln, l)) {
	return (FAILURE);
    }
    ok:
    
    list = (List)l;
    lNode = (ListNode)ln;

    PAlloc (nLNode, ListNode);
    nLNode->datum = d;
    nLNode->useCount = nLNode->flags = 0;
    
    if (lNode == NilListNode) {
	if (list->isCirc) {
	    nLNode->nextPtr = nLNode->prevPtr = nLNode;
	} else {
	    nLNode->nextPtr = nLNode->prevPtr = NilListNode;
	}
	list->firstPtr = list->lastPtr = nLNode;
    } else {
	nLNode->prevPtr = lNode;
	nLNode->nextPtr = lNode->nextPtr;
	
	lNode->nextPtr = nLNode;
	if (nLNode->nextPtr != NilListNode) {
	    nLNode->nextPtr->prevPtr = nLNode;
	}
	
	if (lNode == list->lastPtr) {
	    list->lastPtr = nLNode;
	}
    }
    
    return (SUCCESS);
}

