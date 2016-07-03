/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: io.h,v 1.1 94/06/09 18:11:49 bill Exp Locker: bill $
 * This file contains potential inline procedures that implement i/o
 * instructions that are specific to the i386 family of processors.
 *
 * These procedures can be "non" inlined to allow for debugging,
 * profiling, and tracing.
 */

/*__BEGIN_DECLS
u_char inb(u_short);
u_short inw(u_short);
int inl(u_short);
void insb (u_short, caddr_t, int);
void insw (u_short, caddr_t, int);
void insl (u_short, caddr_t, int);
void outb(u_short, u_char);
void outw(u_short, u_short);
void outl(u_short, int);
void outsb (u_short, caddr_t, int);
void outsw (u_short, caddr_t, int);
void outsl (u_short, caddr_t, int);
__END_DECLS*/

#ifndef __NO_INLINES

#undef	__INLINE
#ifndef __NO_INLINES_BUT_EMIT_CODE
#define	__INLINE	extern inline
#else
#define	__INLINE
#endif

__INLINE u_char
inb(u_short port)
{	u_char rv;

	asm volatile (" in%z0 %1, %0 " : "=a" (rv) : "d" (port));
	return (rv);
}

#define inb_(port) ({						\
	u_char rv;					\
								\
	asm volatile (" in%z0 $" # port ", %b0 " : "=a" (rv)); \
	/* asm volatile (" in%z0 %1, %b0 " : "=a" (rv) : "K" (port)); */ \
	rv;							\
})

#ifdef nope
__INLINE u_char
inb_(const u_char port)
{	u_char rv;

	/*asm volatile (" in%z0 %1, %0 " : "=a" (rv) : "d" (port));*/
	return (rv);
}
#endif

__INLINE u_short
inw(u_short port)
{	u_short rv;

	asm volatile (" in%z0 %1, %0 " : "=a" (rv) : "d" (port));
	return (rv);
}

__INLINE int
inl(u_short port)
{	int rv;

	asm volatile (" in%z0 %1, %0 " : "=a" (rv) : "d" (port));
	return (rv);
}

__INLINE void
insb (u_short port, caddr_t addr, int len)
{
	asm volatile (" cld ; rep ; insb " : "=D" (addr) , "=c" (len) :
		"d" (port), "0" (addr), "1" (len) );
}

__INLINE void
insl (u_short port, caddr_t addr, int len)
{
	asm volatile (" cld ; rep ; insl " : "=D" (addr) , "=c" (len) :
		"d" (port), "0" (addr), "1" (len) );
}

__INLINE void
insw (u_short port, caddr_t addr, int len)
{
	asm volatile (" cld ; rep ; insw " : "=D" (addr) , "=c" (len) :
		"d" (port), "0" (addr), "1" (len) );
}

__INLINE void
outb(u_short port, u_char value)
{
	asm volatile ("out%z0 %0, %1" : : "a" (value), "d" (port) );
}

#ifdef nope
__INLINE void
outb_(const u_char port, u_char value)
{
	/*asm volatile ("out%z0 %0, %1" : : "a" (value), "d" (port) );*/
	asm volatile ("out%z0 %b0, %1" : : "a" (value), "i" (port) );
}
#else
#define outb_(port, value) ({					\
								\
	asm volatile ("out%z0 %b0, $" # port : : "a" (value)); \
	/* asm volatile ("out%z0 %b0, %1" : : "a" (value), "i" (port) ); */ \
})
#endif

__INLINE void
outl(u_short port, int value)
{
	asm volatile ("out%z0 %0, %1" : : "a" (value), "d" (port) );
}

__INLINE void
outsb (u_short port, caddr_t addr, int len)
{

	asm volatile (" cld ; rep ; outsb " : :
		"d" (port), "S" (addr), "c" (len));
}

__INLINE void
outsl (u_short port, caddr_t addr, int len)
{

	asm volatile (" cld ; rep ; outsl " : :
		"d" (port), "S" (addr), "c" (len));
}

__INLINE void
outsw (u_short port, caddr_t addr, int len)
{
	asm volatile (" cld ; rep ; outsw " : :
		"d" (port), "S" (addr), "c" (len));
}

__INLINE void
outw(u_short port, u_short value)
{
	asm volatile ("out%z0 %0, %1" : : "a" (value), "d" (port) );
}

#undef	__INLINE
#endif
