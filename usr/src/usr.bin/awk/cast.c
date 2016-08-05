
/********************************************
cast.c
copyright 1991, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/


/*   $Log: cast.c,v $
 * Revision 5.3.1.4  1993/01/22  15:05:19  mike
 * pow2->mpow2 for linux
 *
 * Revision 5.3.1.3  1993/01/22  14:18:33  mike
 * const for strtod and ansi picky compilers
 *
 * Revision 5.3.1.2  1993/01/20  12:53:06  mike
 * d_to_l()
 *
 * Revision 5.3.1.1  1993/01/15  03:33:37  mike
 * patch3: safer double to int conversion
 *
 * Revision 5.3  1992/11/28  23:48:42  mike
 * For internal conversion numeric->string, when testing
 * if integer, use longs instead of ints so 16 and 32 bit
 * systems behave the same
 *
 * Revision 5.2  1992/08/17  14:19:45  brennan
 * patch2: After parsing, only bi_sprintf() uses string_buff.
 *
 * Revision 5.1  1991/12/05  07:55:41  brennan
 * 1.1 pre-release
 *
*/


/*  cast.c  */

#include "mawk.h"
#include "field.h"
#include "memory.h"
#include "scan.h"
#include "repl.h"

int mpow2[NUM_CELL_TYPES] = {1,2,4,8,16,32,64,128,256,512} ;

void cast1_to_d( cp )
  register CELL *cp ;
{
  switch( cp->type )
  { case C_NOINIT :  cp->dval = 0.0 ; break ;

    case C_DOUBLE :  return ;

    case C_MBSTRN :
    case C_STRING :  
          { register STRING *s = (STRING *) cp->ptr  ;

#if FPE_TRAPS_ON  /* look for overflow error */
            errno = 0 ;
            cp->dval = strtod(s->str,(char **)0) ;
            if ( errno && cp->dval != 0.0 ) /* ignore underflow */
                rt_error("overflow converting %s to double", s) ;
#else
            cp->dval = strtod(s->str,(char **)0) ;
#endif
            free_STRING(s) ;
          }
            break ;

    case C_STRNUM :  
      /* don't need to convert, but do need to free the STRING part */
            free_STRING( string(cp) ) ;
            break ;


    default :
            bozo("cast on bad type") ;
  }
  cp->type = C_DOUBLE ;
}

void cast2_to_d( cp )
  register CELL *cp ;
{ register STRING *s ;

  switch( cp->type )
  { case C_NOINIT :  cp->dval = 0.0 ; break ;

    case C_DOUBLE :  goto two ;
    case C_STRNUM :  
            free_STRING( string(cp) ) ;
            break ;

    case C_MBSTRN :
    case C_STRING :  
            s = (STRING *) cp->ptr ;

#if FPE_TRAPS_ON  /* look for overflow error */
            errno = 0 ;
            cp->dval = strtod(s->str,(char **)0) ;
            if ( errno && cp->dval != 0.0 ) /* ignore underflow */
                rt_error("overflow converting %s to double", s) ;
#else
            cp->dval = strtod(s->str,(char **)0) ;
#endif
            free_STRING(s) ;
            break ;

    default :
            bozo("cast on bad type") ;
  }
  cp->type = C_DOUBLE ;

two:   cp++ ;
  switch( cp->type )
  { case C_NOINIT :  cp->dval = 0.0 ; break ;

    case C_DOUBLE :  return ;
    case C_STRNUM :  
            free_STRING( string(cp) ) ;
            break ;

    case C_MBSTRN :
    case C_STRING :  
            s = (STRING *) cp->ptr ;

#if FPE_TRAPS_ON  /* look for overflow error */
            errno = 0 ;
            cp->dval = strtod(s->str,(char **)0) ;
            if ( errno && cp->dval != 0.0 ) /* ignore underflow */
                rt_error("overflow converting %s to double", s) ;
#else
            cp->dval = strtod(s->str,(char **)0) ;
#endif
            free_STRING(s) ;
            break ;

    default :
            bozo("cast on bad type") ;
  }
  cp->type = C_DOUBLE ;
}

void cast1_to_s( cp )
  register CELL *cp ;
{ 
  register long lval ;
  char xbuff[260] ;

  switch( cp->type )
  { case C_NOINIT :  
        null_str.ref_cnt++ ;
        cp->ptr = (PTR) &null_str ;
        break ;

    case C_DOUBLE  :
	
	lval = d_to_l(cp->dval) ;
	if ( lval == cp->dval )
	    (void) sprintf(xbuff, INT_FMT, lval) ;
	else
            (void) sprintf(xbuff , string(CONVFMT)->str, cp->dval) ;

        cp->ptr = (PTR) new_STRING(xbuff) ;
        break ;

    case C_STRING :  return ;

    case C_MBSTRN :
    case C_STRNUM :  break ;

    default :  bozo("bad type on cast") ;
  }
  cp->type = C_STRING ;
}

void cast2_to_s( cp )
  register CELL *cp ;
{ 
  register long lval ;
  char xbuff[260] ;

  switch( cp->type )
  { case C_NOINIT : 
        null_str.ref_cnt++ ;
        cp->ptr = (PTR) &null_str ;
        break ;

    case C_DOUBLE  :

	lval = d_to_l(cp->dval) ;
	if ( lval == cp->dval )
	    (void) sprintf(xbuff, INT_FMT, lval) ;
	else
            (void) sprintf(xbuff , string(CONVFMT)->str, cp->dval) ;

        cp->ptr = (PTR) new_STRING(xbuff) ;
        break ;

    case C_STRING :  goto two ;

    case C_MBSTRN :
    case C_STRNUM :  break ;

    default :  bozo("bad type on cast") ;
  }
  cp->type = C_STRING ;

two:
  cp++ ;

  switch( cp->type )
  { case C_NOINIT :  
        null_str.ref_cnt++ ; 
        cp->ptr = (PTR) &null_str ;
        break ;

    case C_DOUBLE  :

	lval = d_to_l(cp->dval) ;
	if ( lval == cp->dval )
	    (void) sprintf(xbuff, INT_FMT, lval) ;
	else
            (void) sprintf(xbuff , string(CONVFMT)->str, cp->dval) ;

        cp->ptr = (PTR) new_STRING(xbuff) ;
        break ;

    case C_STRING :  return ;

    case C_MBSTRN :
    case C_STRNUM :  break ;

    default :  bozo("bad type on cast") ;
  }
  cp->type = C_STRING ;
}

void  cast_to_RE( cp )
  register CELL *cp ;
{ register PTR p ;

  if ( cp->type < C_STRING )  cast1_to_s(cp) ;

  p = re_compile( string(cp) ) ;
  free_STRING( string(cp) ) ;
  cp->type = C_RE ;
  cp->ptr = p ;

}

void  cast_for_split(cp)
  register CELL *cp ;
{
  static char meta[] = "^$.*+?|[]()" ;
  static char xbuff[] = "\\X" ;
  int c ;
  unsigned len ;
    
  if ( cp->type < C_STRING )  cast1_to_s(cp) ;

  if ( (len = string(cp)->len) == 1 )
  {
        if ( (c = string(cp)->str[0]) == ' ' )
        { free_STRING(string(cp)) ;
          cp->type = C_SPACE ; 
          return ; 
        }
        else
        if ( strchr(meta, c) )
        { xbuff[1] = c ;
          free_STRING(string(cp)) ;
          cp->ptr = (PTR) new_STRING(xbuff) ;
        }
  }
  else
  if ( len == 0 ) 
  { free_STRING(string(cp)) ;
    cp->type = C_SNULL ; 
    return ; 
  }

  cast_to_RE(cp) ;
}

/* input: cp-> a CELL of type C_MBSTRN (maybe strnum)
   test it -- casting it to the appropriate type
   which is C_STRING or C_STRNUM
*/

void check_strnum( cp )
  CELL *cp ;
{ char *test ;
  register unsigned char *s , *q ;

  cp->type = C_STRING ; /* assume not C_STRNUM */
  s = (unsigned char *) string(cp)->str ;
  q = s + string(cp)->len ;
  while ( scan_code[*s] == SC_SPACE )  s++ ;
  if ( s == q )  return ;

  while ( scan_code[ q[-1] ] == SC_SPACE )  q-- ;
  if ( scan_code[ q[-1] ] != SC_DIGIT &&
       q[-1] != '.' )   return ;

  switch ( scan_code[*s] )
  {
    case SC_DIGIT :
    case SC_PLUS  :
    case SC_MINUS :
    case SC_DOT   :

#if FPE_TRAPS_ON
             errno = 0 ;
             cp->dval  = strtod((char *)s, &test) ;
             if ( errno && cp->dval != 0.0 )
                rt_error(
                "overflow converting %s to double" , s) ;
#else
             cp->dval = strtod((char *)s, &test) ;
#endif

             if ((char *) q <= test )  cp->type = C_STRNUM ;
	     /*  <= instead of == , for some buggy strtod
		 e.g. Apple Unix */
  }
}

/* cast a CELL to a replacement cell */

void cast_to_REPL( cp )
  register CELL *cp ;
{ register STRING *sval ;

  if ( cp->type < C_STRING )  cast1_to_s(cp) ;
  sval = (STRING *) cp->ptr ;

  (void) cellcpy(cp, repl_compile(sval)) ;
  free_STRING(sval) ;
}


/* convert a double to long (this is not as simple as a
   cast because the results are undefined if it won't fit).
   Truncate large values to +MAX__LONG or -MAX__LONG
   Send nans to -MAX__LONG
*/

long
d_to_l(d)
  double d ;
{
  if ( d >= MAX__LONG )  return MAX__LONG ;
  if ( d >  -MAX__LONG ) return (int) d ;
  return -MAX__LONG ;
}

#if   HAVE_STRTOD==0

/* don't use this unless you really don't have strtod() because
   (1) its probably slower than your real strtod()
   (2) atof() may call the real strtod() 
*/

#if __STDC__ == 0
#define const
#endif

double strtod(s, endptr)
  const char *s ;
  char  **endptr ;
{
  register unsigned char *p ; 
  int flag ;
  double atof() ;

  if ( endptr )
  {
    p = (unsigned char*) s ;

    flag = 0 ;
    while ( *p == ' ' || *p == '\t' ) p++ ;
    if ( *p == '-' || *p == '+' ) p++ ;
    while ( scan_code[*p] == SC_DIGIT ) { flag++ ; p++ ; }
    if ( *p == '.' )
    {
      p++ ;
      while ( scan_code[*p] == SC_DIGIT ) { flag++ ; p++ ; }
    }
    /* done with number part */
    if ( flag == 0 )
    { /* no number part */
      *endptr = s ; return 0.0 ; 
    }
    else *endptr = (char *) p ;

    /* now look for exponent */
    if ( *p == 'e' || *p == 'E' )
    {
      flag = 0 ;
      p++ ;
      if ( *p == '-' || *p == '+' ) p++ ;
      while ( scan_code[*p] == SC_DIGIT ) { flag++ ; p++ ; }
      if ( flag ) *endptr = (char *) p ;
    }
  }
  return atof(s) ;
}
#endif  /* HAVE_STRTOD==0 */

#if   HAVE_FMOD==0

#if SW_FP_CHECK   /* this is V7 and XNX23A specific */

double  fmod(x, y)
  double x, y ;
{ double modf() ;
  double dtmp, ipart ;

  clrerr();
  dtmp = x / y;
  fpcheck();
  (void) modf(dtmp, &ipart) ;
  return x - ipart*y ;
}

#else

double  fmod(x, y)
  double x, y ;
{ double modf() ;
  double ipart ;

  (void) modf(x/y, &ipart) ;
  return x - ipart*y ;
}

#endif
#endif  



