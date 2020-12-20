/*  Functions from hack's utils library.
    Copyright (C) 1989, 1990, 1991 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* These routines were written as part of a library (by hack), but since most
   people don't have the library, here they are.  */

#ifdef __STDC__
#define VOID void
#else
#define VOID char
#endif

#include <stdio.h>
#if HAVE_STRING_H || defined(STDC_HEADERS)
#include <string.h>
#else
#include <strings.h>
#endif
#if defined(STDC_HEADERS)
#include <stdlib.h>
#else
#ifdef RX_MEMDBUG
#include <sys/types.h>
#include <malloc.h>
#else
VOID *malloc();
VOID *realloc();
#endif /* ndef RX_MEMDBUG  */
#endif

VOID *ck_malloc();

char *myname;


#ifdef __STDC__
#include <stdarg.h>

/* Print an error message and exit */
void
panic(char *str, ...)
{
	va_list iggy;

	fprintf(stderr,"%s: ",myname);
	va_start(iggy,str);
#ifdef HAVE_VPRINTF
	vfprintf(stderr,str,iggy);
#else
#ifdef HAVE_DOPRNT
	_doprnt(str,&iggy,stderr);
#endif
#endif
	va_end(iggy);
	putc('\n',stderr);
	exit(4);
}

#else
#include <varargs.h>

void
panic(str,va_alist)
char *str;
va_dcl
{
	va_list iggy;

	fprintf(stderr,"%s: ",myname);
	va_start(iggy);
#ifdef HAVE_VPRINTF
	vfprintf(stderr,str,iggy);
#else
#ifdef HAVE_DOPRNT
	_doprnt(str,&iggy,stderr);
#endif
#endif
	va_end(iggy);
	putc('\n',stderr);
	exit(4);
}

#endif

/* Store information about files opened with ck_fopen
   so that error messages from ck_fread, etc can print the
   name of the file that had the error */
#define N_FILE 32

struct id {
	FILE *fp;
	char *name;
};

static struct id __id_s[N_FILE];

/* Internal routine to get a filename from __id_s */
char *
__fp_name(fp)
FILE *fp;
{
	int n;

	for(n=0;n<N_FILE;n++) {
		if(__id_s[n].fp==fp)
			return __id_s[n].name;
	}
	return "{Unknown file pointer}";
}

/* Panic on failing fopen */
FILE *
ck_fopen(name,mode)
char *name;
char *mode;
{
	FILE	*ret;
	int	n;

	ret=fopen(name,mode);
	if(ret==(FILE *)0)
		panic("Couldn't open file %s",name);
	for(n=0;n<N_FILE;n++) {
		if(ret==__id_s[n].fp) {
			free((VOID *)__id_s[n].name);
			__id_s[n].name=(char *)ck_malloc(strlen(name)+1);
			strcpy(__id_s[n].name,name);
			break;
		}
	}
	if(n==N_FILE) {
		for(n=0;n<N_FILE;n++)
			if(__id_s[n].fp==(FILE *)0)
				break;
		if(n==N_FILE)
			panic("Internal error: too many files open");
		__id_s[n].fp=ret;
		__id_s[n].name=(char *)ck_malloc(strlen(name)+1);
		strcpy(__id_s[n].name,name);
	}
	return ret;
}

/* Panic on failing fwrite */
void
ck_fwrite(ptr,size,nmemb,stream)
char *ptr;
int size,nmemb;
FILE *stream;
{
	if(fwrite(ptr,size,nmemb,stream)!=nmemb)
		panic("couldn't write %d items to %s",nmemb,__fp_name(stream));
}

/* Panic on failing fclose */
void
ck_fclose(stream)
FILE *stream;
{
	if(fclose(stream)==EOF)
		panic("Couldn't close %s",__fp_name(stream));
}

/* Panic on failing malloc */
VOID *
ck_malloc(size)
int size;
{
	VOID *ret;

	if(!size)
		size++;
	ret=malloc(size);
	if(ret==(VOID *)0)
		panic("Couldn't allocate memory");
	return ret;
}

/* Panic on failing malloc */
VOID *
xmalloc(size)
int size;
{
  return ck_malloc (size);
}

/* Panic on failing realloc */
VOID *
ck_realloc(ptr,size)
VOID *ptr;
int size;
{
	VOID *ret;

	if (!ptr)
	  return ck_malloc (size);
	ret=realloc(ptr,size);
	if(ret==(VOID *)0)
		panic("Couldn't re-allocate memory");
	return ret;
}

/* Return a malloc()'d copy of a string */
char *
ck_strdup(str)
char *str;
{
	char *ret;

	ret=(char *)ck_malloc(strlen(str)+2);
	strcpy(ret,str);
	return ret;
}


/* Implement a variable sized buffer of 'stuff'.  We don't know what it is,
   nor do we care, as long as it doesn't mind being aligned by malloc. */

struct buffer {
	int	allocated;
	int	length;
	char	*b;
};

#define MIN_ALLOCATE 50

VOID *
init_buffer()
{
	struct buffer *b;

	b=(struct buffer *)ck_malloc(sizeof(struct buffer));
	b->allocated=MIN_ALLOCATE;
	b->b=(char *)ck_malloc(MIN_ALLOCATE);
	b->length=0;
	return (VOID *)b;
}

void
flush_buffer(bb)
VOID *bb;
{
	struct buffer *b;

	b=(struct buffer *)bb;
	free(b->b);
	b->b=0;
	b->allocated=0;
	b->length=0;
	free(b);
}

int
size_buffer(b)
VOID *b;
{
	struct buffer *bb;

	bb=(struct buffer *)b;
	return bb->length;
}

void
add_buffer(bb,p,n)
VOID *bb;
char *p;
int n;
{
	struct buffer *b;
	int x;
	char * cp;

	b=(struct buffer *)bb;
	if(b->length+n>b->allocated) {
		b->allocated = (b->length + n) * 2;
		b->b=(char *)ck_realloc(b->b,b->allocated);
	}
	
	x = n;
	cp = b->b + b->length;
	while (x--)
	  *cp++ = *p++;
	b->length+=n;
}

void
add1_buffer(bb,ch)
VOID *bb;
int ch;
{
	struct buffer *b;

	b=(struct buffer *)bb;
	if(b->length+1>b->allocated) {
		b->allocated*=2;
		b->b=(char *)ck_realloc(b->b,b->allocated);
	}
	b->b[b->length]=ch;
	b->length++;
}

char *
get_buffer(bb)
VOID *bb;
{
	struct buffer *b;

	b=(struct buffer *)bb;
	return b->b;
}
