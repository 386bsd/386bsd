/* $RCSfile: INTERN.h,v $$Revision: 4.0.1.1 $$Date: 91/06/07 12:11:20 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log:	INTERN.h,v $
 * Revision 4.0.1.1  91/06/07  12:11:20  lwall
 * patch4: new copyright notice
 * 
 * Revision 4.0  91/03/20  01:56:58  lwall
 * 4.0 baseline.
 * 
 */

#undef EXT
#define EXT

#undef INIT
#define INIT(x) = x

#define DOINIT
