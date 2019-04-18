
__BEGIN_DECLS
void swtch(void);

/* interface symbols */
#define	__ISYM_VERSION__ "1"	/* XXX RCS major revision number of hdr file */
#include "isym.h"		/* this header has interface symbols */

/* functions used in modules */
__ISYM__(int, splx, (int))
__ISYM__(int, splimp, (void))
__ISYM__(int, splnet, (void))
__ISYM__(int, splhigh, (void))

#undef __ISYM__
#undef __ISYM_ALIAS__
#undef __ISYM_VERSION__
u_char inb(u_short);
u_char inb_(const u_char);	/* constant */
u_char __inb(u_short);		/* recovery time */
u_short inw(u_short);
int inl(u_short);
void insb (u_short, caddr_t, int);
void insw (u_short, caddr_t, int);
void insl (u_short, caddr_t, int);
void outb(u_short, u_char);
void outb_(const u_char, u_char);
void __outb(u_short, u_char);
void outw(u_short, u_short);
void outl(u_short, int);
void outsb (u_short, caddr_t, int);
void outsw (u_short, caddr_t, int);
void outsl (u_short, caddr_t, int);

__END_DECLS
