#include <stdlib.h>
#include <stddef.h>

double
strtod(ascii, ep)
	const char *ascii;
	char **ep;
{
	return(atof(ascii));
}
