
dummy() {}
#define	__ISYM__(a,b,c)	b(),
int
#include "./isym"
dummy() ;
#undef __ISYM__
#define	__ISYM__(a,b,c)	"" # b "1" , b,

const struct tbl {
	const char *name;
	int (*func)();
} kernpermsym[] = {

#include "./isym"

0,

dummy

};
