/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
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
 *
 *	@(#)string.h	5.10 (Berkeley) 3/9/91
 */

#ifndef _STRING_H_
#define	_STRING_H_
#include <machine/ansi.h>

#ifdef	_SIZE_T_
typedef	_SIZE_T_	size_t;
#undef	_SIZE_T_
#endif

#ifndef	NULL
#define	NULL	0
#endif

#include <sys/cdefs.h>

__BEGIN_DECLS
void	*memchr __P((const void *, int, size_t));
int	 memcmp __P((const void *, const void *, size_t));
void	*memcpy __P((void *, const void *, size_t));
void	*memmove __P((void *, const void *, size_t));
void	*memset __P((void *, int, size_t));
char	*strcat __P((char *, const char *));
char	*strchr __P((const char *, int));
int	 strcmp __P((const char *, const char *));
int	 strcoll __P((const char *, const char *));
char	*strcpy __P((char *, const char *));
size_t	 strcspn __P((const char *, const char *));
char	*strerror __P((int));
size_t	 strlen __P((const char *));
char	*strncat __P((char *, const char *, size_t));
int	 strncmp __P((const char *, const char *, size_t));
char	*strncpy __P((char *, const char *, size_t));
char	*strpbrk __P((const char *, const char *));
char	*strrchr __P((const char *, int));
size_t	 strspn __P((const char *, const char *));
char	*strstr __P((const char *, const char *));
char	*strtok __P((char *, const char *));
size_t	 strxfrm __P((char *, const char *, size_t));

/* Nonstandard routines */
#ifndef _ANSI_SOURCE
int	 bcmp __P((const void *, const void *, size_t));
void	 bcopy __P((const void *, void *, size_t));
void	 bzero __P((void *, size_t));
int	 ffs __P((int));
char	*index __P((const char *, int));
void	*memccpy __P((void *, const void *, int, size_t));
char	*rindex __P((const char *, int));
int	 strcasecmp __P((const char *, const char *));
char	*strdup __P((const char *));
void	 strmode __P((int, char *));
int	 strncasecmp __P((const char *, const char *, size_t));
char	*strsep __P((char **, const char *));
void	 swab __P((const void *, void *, size_t));
#endif 
__END_DECLS

#endif /* _STRING_H_ */
