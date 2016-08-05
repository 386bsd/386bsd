/* fileman.c -- A tiny application which demonstrates how to use the
   GNU Readline library.  This application interactively allows users
   to manipulate files and their modes. */

#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/errno.h>

/* The names of functions that actually do the manipulation. */
int com_list (), com_view (), com_rename (), com_stat (), com_pwd ();
int com_delete (), com_help (), com_cd (), com_quit ();

/* A structure which contains information on the commands this program
   can understand. */

typedef struct {
  char *name;			/* User printable name of the function. */
  Function *func;		/* Function to call to do the job. */
  char *doc;			/* Documentation for this function.  */
} COMMAND;

COMMAND commands[] = {
  { "cd", com_cd, "Change to directory DIR" },
  { "delete", com_delete, "Delete FILE" },
  { "help", com_help, "Display this text" },
  { "?", com_help, "Synonym for `help'" },
  { "list", com_list, "List files in DIR" },
  { "ls", com_list, "Synonym for `list'" },
  { "pwd", com_pwd, "Print the current working directory" },
  { "quit", com_quit, "Quit using Fileman" },
  { "rename", com_rename, "Rename FILE to NEWNAME" },
  { "stat", com_stat, "Print out statistics on FILE" },
  { "view", com_view, "View the contents of FILE" },
  { (char *)NULL, (Function *)NULL, (char *)NULL }
};

/* The name of this program, as taken from argv[0]. */
char *progname;

/* When non-zero, this global means the user is done using this program. */
int done = 0;

main (argc, argv)
     int argc;
     char **argv;
{
  progname = argv[0];

  initialize_readline ();	/* Bind our completer. */

  /* Loop reading and executing lines until the user quits. */
  while (!done)
    {
      char *line;

      line = readline ("FileMan: ");

      if (!line)
	{
	  done = 1;		/* Encountered EOF at top level. */
	}
      else
	{
	  /* Remove leading and trailing whitespace from the line.
	     Then, if there is anything left, add it to the history list
	     and execute it. */
	  stripwhite (line);

	  if (*line)
	    {
	      add_history (line);
	      execute_line (line);
	    }
	}

      if (line)
	free (line);
    }
  exit (0);
}

/* Execute a command line. */
execute_line (line)
     char *line;
{
  register int i;
  COMMAND *find_command (), *command;
  char *word;

  /* Isolate the command word. */
  i = 0;
  while (line[i] && !whitespace (line[i]))
    i++;

  word = line;

  if (line[i])
    line[i++] = '\0';

  command = find_command (word);

  if (!command)
    {
      fprintf (stderr, "%s: No such command for FileMan.\n", word);
      return;
    }

  /* Get argument to command, if any. */
  while (whitespace (line[i]))
    i++;

  word = line + i;

  /* Call the function. */
  (*(command->func)) (word);
}

/* Look up NAME as the name of a command, and return a pointer to that
   command.  Return a NULL pointer if NAME isn't a command name. */
COMMAND *
find_command (name)
     char *name;
{
  register int i;

  for (i = 0; commands[i].name; i++)
    if (strcmp (name, commands[i].name) == 0)
      return (&commands[i]);

  return ((COMMAND *)NULL);
}

/* Strip whitespace from the start and end of STRING. */
stripwhite (string)
     char *string;
{
  register int i = 0;

  while (whitespace (string[i]))
    i++;

  if (i)
    strcpy (string, string + i);

  i = strlen (string) - 1;

  while (i > 0 && whitespace (string[i]))
    i--;

  string[++i] = '\0';
}

/* **************************************************************** */
/*                                                                  */
/*                  Interface to Readline Completion                */
/*                                                                  */
/* **************************************************************** */

/* Tell the GNU Readline library how to complete.  We want to try to complete
   on command names if this is the first word in the line, or on filenames
   if not. */
initialize_readline ()
{
  char **fileman_completion ();

  /* Allow conditional parsing of the ~/.inputrc file. */
  rl_readline_name = "FileMan";

  /* Tell the completer that we want a crack first. */
  rl_attempted_completion_function = (Function *)fileman_completion;
}

/* Attempt to complete on the contents of TEXT.  START and END show the
   region of TEXT that contains the word to complete.  We can use the
   entire line in case we want to do some simple parsing.  Return the
   array of matches, or NULL if there aren't any. */
char **
fileman_completion (text, start, end)
     char *text;
     int start, end;
{
  char **matches;
  char *command_generator ();

  matches = (char **)NULL;

  /* If this word is at the start of the line, then it is a command
     to complete.  Otherwise it is the name of a file in the current
     directory. */
  if (start == 0)
    matches = completion_matches (text, command_generator);

  return (matches);
}

/* Generator function for command completion.  STATE lets us know whether
   to start from scratch; without any state (i.e. STATE == 0), then we
   start at the top of the list. */
char *
command_generator (text, state)
     char *text;
     int state;
{
  static int list_index, len;
  char *name;

  /* If this is a new word to complete, initialize now.  This includes
     saving the length of TEXT for efficiency, and initializing the index
     variable to 0. */
  if (!state)
    {
      list_index = 0;
      len = strlen (text);
    }

  /* Return the next name which partially matches from the command list. */
  while (name = commands[list_index].name)
    {
      list_index++;

      if (strncmp (name, text, len) == 0)
	return (name);
    }

  /* If no names matched, then return NULL. */
  return ((char *)NULL);
}

/* **************************************************************** */
/*                                                                  */
/*                       FileMan Commands                           */
/*                                                                  */
/* **************************************************************** */

/* String to pass to system ().  This is for the LIST, VIEW and RENAME
   commands. */
static char syscom[1024];

/* List the file(s) named in arg. */
com_list (arg)
     char *arg;
{
  if (!arg)
    arg = "*";

  sprintf (syscom, "ls -FClg %s", arg);
  system (syscom);
}

com_view (arg)
     char *arg;
{
  if (!valid_argument ("view", arg))
    return;

  sprintf (syscom, "cat %s | more", arg);
  system (syscom);
}

com_rename (arg)
     char *arg;
{
  too_dangerous ("rename");
}

com_stat (arg)
     char *arg;
{
  struct stat finfo;

  if (!valid_argument ("stat", arg))
    return;

  if (stat (arg, &finfo) == -1)
    {
      perror (arg);
      return;
    }

  printf ("Statistics for `%s':\n", arg);

  printf ("%s has %d link%s, and is %d bytes in length.\n", arg,
	  finfo.st_nlink, (finfo.st_nlink == 1) ? "" : "s",  finfo.st_size);
  printf ("      Created on: %s", ctime (&finfo.st_ctime));
  printf ("  Last access at: %s", ctime (&finfo.st_atime));
  printf ("Last modified at: %s", ctime (&finfo.st_mtime));
}

com_delete (arg)
     char *arg;
{
  too_dangerous ("delete");
}

/* Print out help for ARG, or for all of the commands if ARG is
   not present. */
com_help (arg)
     char *arg;
{
  register int i;
  int printed = 0;

  for (i = 0; commands[i].name; i++)
    {
      if (!*arg || (strcmp (arg, commands[i].name) == 0))
	{
	  printf ("%s\t\t%s.\n", commands[i].name, commands[i].doc);
	  printed++;
	}
    }

  if (!printed)
    {
      printf ("No commands match `%s'.  Possibilties are:\n", arg);

      for (i = 0; commands[i].name; i++)
	{
	  /* Print in six columns. */
	  if (printed == 6)
	    {
	      printed = 0;
	      printf ("\n");
	    }

	  printf ("%s\t", commands[i].name);
	  printed++;
	}

      if (printed)
	printf ("\n");
    }
}

/* Change to the directory ARG. */
com_cd (arg)
     char *arg;
{
  if (chdir (arg) == -1)
    perror (arg);

  com_pwd ("");
}

/* Print out the current working directory. */
com_pwd (ignore)
     char *ignore;
{
  char dir[1024];

  (void) getwd (dir);

  printf ("Current directory is %s\n", dir);
}

/* The user wishes to quit using this program.  Just set DONE non-zero. */
com_quit (arg)
     char *arg;
{
  done = 1;
}

/* Function which tells you that you can't do this. */
too_dangerous (caller)
     char *caller;
{
  fprintf (stderr,
	   "%s: Too dangerous for me to distribute.  Write it yourself.\n",
	   caller);
}

/* Return non-zero if ARG is a valid argument for CALLER, else print
   an error message and return zero. */
int
valid_argument (caller, arg)
     char *caller, *arg;
{
  if (!arg || !*arg)
    {
      fprintf (stderr, "%s: Argument required.\n", caller);
      return (0);
    }

  return (1);
}


/*
 * Local variables:
 * compile-command: "cc -g -I../.. -L.. -o fileman fileman.c -lreadline -ltermcap"
 * end:
 */
