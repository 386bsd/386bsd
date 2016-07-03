
__BEGIN_DECLS
void swtch(void);

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
