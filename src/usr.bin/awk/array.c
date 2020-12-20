
/********************************************
array.c
copyright 1991, 1992.  Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/* $Log: array.c,v $
 * Revision 5.3.1.1  1993/01/20  12:24:25  mike
 * patch3: safer double to int conversions
 *
 * Revision 5.3  1992/11/28  23:48:42  mike
 * For internal conversion numeric->string, when testing
 * if integer, use longs instead of ints so 16 and 32 bit
 * systems behave the same
 *
 * Revision 5.2  1992/04/07  17:17:31  brennan
 * patch 2
 * n = split(s,A,r)
 * delete A[i] if i not in 1..n
 * This is consistent with [ng]?awk
 *
 * Revision 5.1  91/12/05  07:55:32  brennan
 * 1.1 pre-release
 * 
*/

#include "mawk.h"
#include "symtype.h"
#include "memory.h"
#include "field.h"
#include "bi_vars.h"

/* convert doubles to string for array indexing */
#define   NO_MOVE       2
#define   NO_IVAL       (-1)

static ANODE *PROTO(find_by_sval, (ARRAY, STRING *, int) ) ;
static ANODE *PROTO(find_by_index, (ARRAY, int,long,int) ) ;
static ANODE *PROTO(find_by_dval, (ARRAY, double, int)) ;
static void PROTO(load_array_ov, (ARRAY) ) ;
static void PROTO(ilist_delete, (ARRAY, ANODE*)) ;


extern unsigned hash() ;

/* An array A is a pointer to an array of struct array,
   which is two hash tables in one.  One for strings
   and one for non-negative integers (this is much simplier
   and works just as well as previous versions)

   Each array is of size A_HASH_PRIME.

   When an index is deleted via  delete A[i], the
   ANODE is not removed from the hash chain.  A[i].cp
   and A[i].sval are both freed and sval is set NULL.
   This method of deletion simplifies for( i in A ) loops.

*/


static  ANODE *find_by_sval(A, sval, cflag)
  ARRAY  A ;
  STRING *sval ;
  int  cflag ; /* create if on */
{ 
   char *s = sval->str ;
   unsigned h = hash(s) % A_HASH_PRIME ;
   register ANODE *p = A[h].link ;
   ANODE *q = 0 ; /* holds first deleted ANODE */

   while ( 1 )
     if ( !p )  goto not_there ;
     else 
     if ( p->sval )
	if ( strcmp(s,p->sval->str) == 0 )  return p ;
	else  p = p->link ;
     else { q = p ; p = p->link ; break ; }
	
   while ( p )  /* q is now set */
     if ( p->sval && strcmp(s,p->sval->str) == 0 )  return p ; 
     else  p = p->link ;

not_there :
   if ( cflag )
   {
       if ( q )  p = q ; /* reuse the deleted node q */
       else
       { p = (ANODE *)zmalloc(sizeof(ANODE)) ;
         p->link = A[h].link ; A[h].link = p ;
       }

       p->ival = NO_IVAL ;
       p->sval = sval ;
       sval->ref_cnt++ ;
       p->cp = (CELL *) zmalloc(sizeof(CELL)) ;
       p->cp->type = C_NOINIT ;
   }
   return p ;
}


/* find an array by (long) integer ival.
   Caller has already computed the hash value index.
   (This allows fast insertion for split())
*/

static ANODE  *find_by_index(A, index, ival, flag)
  ARRAY  A ;
  int index; 
  long ival; 
  int flag ;
{
  register ANODE *p = A[index].ilink ;
  ANODE *q = 0 ; /* trails p */

  while ( p )
       if ( p->ival == ival )  /* found */
         if ( !q || flag == NO_MOVE )  return  p ;
         else /* delete to put at front */
         { q->ilink = p->ilink ; goto found ; }
        
       else
       { q = p ; p = p->ilink ; }

   /* not there, still need to look by sval */
   
   { /* convert to string */
     char xbuff[16] ;
     STRING *sval ;
     char *s = xbuff+14 ;
     long x = ival ;

     xbuff[15] = 0 ;

     do { *s-- = x%10 + '0' ; x /= 10 ; } while(x) ;
     sval = new_STRING(s+1) ;
     p = find_by_sval(A, sval, flag) ;
     free_STRING(sval) ;
   }


   if ( ! p )  return (ANODE *) 0 ;

   p->ival = ival ;

found : /* put p at front */
   p->ilink = A[index].ilink ; A[index].ilink = p ;
   return p ;
}


static ANODE *find_by_dval(A, d, flag)
  ARRAY A ;
  double d ;
  int flag ;
{
  long lval ;
  ANODE *p ;
  char xbuff[260] ;
  STRING *sval ;
  

  lval = d_to_l(d) ;
  if ( (double)lval == d ) /* integer valued */
  {
    if ( lval >= 0 )  
    {
            return 
	    find_by_index(A, (int)(lval%A_HASH_PRIME), lval, flag) ;
    }
    else
    (void) sprintf(xbuff, INT_FMT, lval) ;
  } 
  else (void) sprintf(xbuff, string(CONVFMT)->str, d) ;

  sval = new_STRING(xbuff) ;
  p = find_by_sval(A, sval, flag) ;
  free_STRING(sval) ;
  return p ;
}

  

CELL *array_find(A, cp, create_flag)
  ARRAY A ;
  CELL *cp ;
  int create_flag ;
{
  ANODE *ap ;

  switch( cp->type )
  {
    case C_DOUBLE :
        ap = find_by_dval(A, cp->dval, create_flag) ;
        break ;

    case  C_NOINIT :
        ap = find_by_sval(A, &null_str, create_flag) ;
        break ;

    default :
        ap = find_by_sval(A, string(cp), create_flag) ;
        break ;
  }

  return  ap ? ap->cp : (CELL *) 0 ;
}


void  array_delete(A, cp)
  ARRAY A ; CELL *cp ;
{
  ANODE *ap ;

  switch( cp->type )
  {
    case C_DOUBLE :
        ap = find_by_dval(A, cp->dval, NO_CREATE) ;
	/* cut the ilink */
        if ( ap && ap->ival >= 0 ) /* must be at front */
                A[(int)(ap->ival%A_HASH_PRIME)].ilink = ap->ilink ;
        break ;

    case  C_NOINIT :
        ap = find_by_sval(A, &null_str, NO_CREATE) ;
        break ;

    default :
        ap = find_by_sval(A, string(cp), NO_CREATE) ;
        if ( ap && ap->ival >= 0 ) ilist_delete(A, ap) ;
        break ;
  }

  
  /* delete -- leave the empty ANODE so for(i in A)
     works */
  if ( ap )
  { free_STRING(ap->sval) ; ap->sval = (STRING *) 0 ;
    cell_destroy(ap->cp)  ; zfree(ap->cp, sizeof(CELL)) ;
  }
}

/* load into an array from the split_ov_list */

static void  load_array_ov(A)
  ARRAY A ;
{
  register SPLIT_OV *p ;
  register CELL *cp ;
  SPLIT_OV *q ;

  int cnt = MAX_SPLIT + 1 ;
  int index = (MAX_SPLIT+1) % A_HASH_PRIME ;

  p = split_ov_list ; split_ov_list = (SPLIT_OV*) 0 ;

#ifdef  DEBUG
  if ( !p ) bozo("array_ov") ;
#endif

  while( 1 )
  {
    cp = find_by_index(A, index, (long)cnt, NO_MOVE) ->cp ;
    cell_destroy(cp) ;
    cp->type = C_MBSTRN ;
    cp->ptr = (PTR) p->sval ;

    q = p ; p = p->link ; zfree(q, sizeof(SPLIT_OV)) ;
    if ( !p )  break ;

    cnt++ ;
    if ( ++index == A_HASH_PRIME )  index = 0 ;
  }
}

/* delete an ANODE from the ilist that is known
   to be there */

static  void  ilist_delete(A, d) 
  ARRAY A ;
  ANODE *d ;
{
  int index = d->ival % A_HASH_PRIME ;
  register ANODE *p = A[index].ilink ;
  register ANODE *q = (ANODE *) 0 ;

  while ( p != d ) { q = p ; p = p->ilink ; }

  if ( q )  q->ilink = p->ilink ;
  else  A[index].ilink = p->ilink ;
}


/* this is called by bi_split()
   to load strings into an array
*/

void  load_array( A, cnt)
  ARRAY  A ;
  register int cnt ;
{
  register CELL *cp ;
  register int index ;

  { /* clear A , leaving only A[1]..A[cnt] (if exist) */
    int i ;
    ANODE *p ;

    for( i = 0 ; i < A_HASH_PRIME ; i++ )
    {
      p = A[i].link ;
      while ( p )
      {
	if ( p->sval && (p->ival <= 0 || p->ival > cnt) )
	{
	  if (p->ival >= 0) ilist_delete(A, p) ;

	  free_STRING(p->sval) ;
	  p->sval = (STRING *) 0 ;
	  cell_destroy(p->cp) ;
	  ZFREE(p->cp) ;
	}
	p = p->link ;
      }
    }
  }
  if ( cnt > MAX_SPLIT )
  {
    load_array_ov(A) ;
    cnt = MAX_SPLIT ;
  }

  index = cnt % A_HASH_PRIME ;

  while ( cnt )
  {
    cp = find_by_index(A, index, (long) cnt, NO_MOVE) ->cp  ;
    cell_destroy(cp) ;
    cp->type = C_MBSTRN ;
    cp->ptr = (PTR) split_buff[--cnt] ;

    if ( --index < 0 )  index = A_HASH_PRIME - 1 ;
  }
}


/* cat together cnt elements on the eval stack to form
   an array index using SUBSEP */

CELL *array_cat( sp, cnt)
  register CELL *sp ;
  int cnt ;
{ register CELL *p ;  /* walks the stack */
  CELL subsep ;  /* a copy of SUBSEP */
  unsigned subsep_len ;
  char *subsep_str ;
  unsigned total_len ; /* length of cat'ed expression */
  CELL *top ;  /* sp at entry */
  char *t ; /* target ptr when catting */
  STRING *sval ;  /* build new STRING here */

  /* get a copy of subsep, we may need to cast */
  (void) cellcpy(&subsep, SUBSEP) ;
  if ( subsep.type < C_STRING ) cast1_to_s(&subsep) ;
  subsep_len = string(&subsep)->len ;
  subsep_str = string(&subsep)->str ;
  total_len = --cnt * subsep_len ;

  top = sp ;
  sp -= cnt ;
  for( p = sp ; p <= top ; p++ )
  {
    if ( p->type < C_STRING ) cast1_to_s(p) ;
    total_len += string(p)->len ;
  }

  sval = new_STRING((char *)0, total_len) ;
  t = sval->str ;

  /* put the pieces together */
  for( p = sp ; p < top ; p++ )
  { (void) memcpy(t, string(p)->str, SIZE_T(string(p)->len)) ;
    (void) memcpy( t += string(p)->len, subsep_str, SIZE_T(subsep_len)) ;
    t += subsep_len ;
  }
  /* p == top */
  (void) memcpy(t, string(p)->str, SIZE_T(string(p)->len)) ;

  /* done, now cleanup */
  free_STRING(string(&subsep)) ;
  while ( p >= sp )  { free_STRING(string(p)) ; p-- ; }
  sp->type = C_STRING ;
  sp->ptr = (PTR) sval ;
  return sp ;
}


/* free all memory used by an array,
   only used for arrays local to a function call
*/

void  array_free(A)
  ARRAY  A ;
{ register ANODE *p ;
  register int i ;
  ANODE *q ;

  for( i = 0 ; i < A_HASH_PRIME ; i++ )
  { p = A[i].link ;
    while ( p )
    { /* check its not a deleted node */
      if ( p->sval )
      { free_STRING(p->sval) ;
        cell_destroy(p->cp) ;
        free_CELL(p->cp) ;
      }

      q = p ; p = p->link ;
      zfree( q, sizeof(ANODE)) ;
    }
  }

  zfree(A, sizeof(A[0]) * A_HASH_PRIME ) ;
}


int  inc_aloop_state( ap )
  ALOOP_STATE *ap ;
{
  register ANODE *p ;
  register int i ;
  ARRAY  A = ap->A ;
  CELL *cp ;

  if ( (i = ap->index) == -1 )  p = A[++i].link ;
  else  p = ap->ptr->link ;

  while ( 1 )
  {
    if ( !p )
	if ( ++i == A_HASH_PRIME )  return 0 ;
	else
	{ p = A[i].link ; continue ; }

    if ( p->sval )  /* found one */
    {
      cp = ap->var ;
      cell_destroy(cp) ;
      cp->type = C_STRING ;
      cp->ptr = (PTR) p->sval ;
      p->sval->ref_cnt++ ;

      /* save the state */
      ap->index = i ;
      ap->ptr = p ;
      return 1 ;
    }
    else /* its deleted */
    p = p->link ;
  }
}
