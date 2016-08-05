/* Getopt for GNU.
   Copyright (C) 1987, 1989, 1991 Free Software Foundation, Inc.

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

#include "getopt.h"

#ifdef __STDC__
#define CONST const
#else
#define CONST
#endif

#if !defined (NULL)
#define NULL 0
#endif

int
getopt_long (argc, argv, options, long_options, opt_index)
     int argc;
     char **argv;
     CONST char *options;
     CONST struct option *long_options;
     int *opt_index;
{
  int val;

  _getopt_long_options = long_options;
  val = getopt (argc, argv, options);
  if (val == 0 && opt_index != NULL)
    *opt_index = option_index;
  return val;
}

/* Like getopt_long, but '-' as well as '+' can indicate a long option.
   If an option that starts with '-' doesn't match a long option,
   but does match a short option, it is parsed as a short option
   instead. */

int 
getopt_long_only (argc, argv, options, long_options, opt_index)
     int argc;
     char **argv;
     CONST char *options;
     CONST struct option *long_options;
     int *opt_index;
{
  int val;

  _getopt_long_options = long_options;
  _getopt_long_only = 1;
  val = getopt (argc, argv, options);
  if (val == 0 && opt_index != NULL)
    *opt_index = option_index;
  return val;
}


#ifdef TEST

#include <stdio.h>

int
main (argc, argv)
     int argc;
     char **argv;
{
  int c;
  int digit_optind = 0;

  while (1)
    {
      int this_option_optind = optind ? optind : 1;
      char *name = '\0';
      int option_index = 0;
      static struct option long_options[] =
      {
	{"add", 1, 0, 0},
	{"append", 0, 0, 0},
	{"delete", 1, 0, 0},
	{"verbose", 0, 0, 0},
	{"create", 0, 0, 0},
	{"file", 1, 0, 0},
	{0, 0, 0, 0}
      };

      c = getopt_long (argc, argv, "abc:d:0123456789",
		       long_options, &option_index);
      if (c == EOF)
	break;

      switch (c)
	{
	case 0:
	  printf ("option %s", (long_options[option_index]).name);
	  if (optarg)
	    printf (" with arg %s", optarg);
	  printf ("\n");
	  break;

	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	  if (digit_optind != 0 && digit_optind != this_option_optind)
	    printf ("digits occur in two different argv-elements.\n");
	  digit_optind = this_option_optind;
	  printf ("option %c\n", c);
	  break;

	case 'a':
	  printf ("option a\n");
	  break;

	case 'b':
	  printf ("option b\n");
	  break;

	case 'c':
	  printf ("option c with value `%s'\n", optarg);
	  break;

	case '?':
	  break;

	default:
	  printf ("?? getopt returned character code 0%o ??\n", c);
	}
    }

  if (optind < argc)
    {
      printf ("non-option ARGV-elements: ");
      while (optind < argc)
	printf ("%s ", argv[optind++]);
      printf ("\n");
    }

  exit (0);
}

#endif /* TEST */
