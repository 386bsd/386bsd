/* Analyze file differences for GNU DIFF.
   Copyright (C) 1988, 1989 Free Software Foundation, Inc.

This file is part of GNU DIFF.

GNU DIFF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GNU DIFF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU DIFF; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* The basic algorithm is described in: 
   "An O(ND) Difference Algorithm and its Variations", Eugene Myers,
   Algorithmica Vol. 1 No. 2, 1986, p 251.  */

#include "diff.h"

struct change *find_change ();
void finish_output ();
void print_context_header ();
void print_context_script ();
void print_ed_script ();
void print_ifdef_script ();
void print_normal_script ();
void print_rcs_script ();
void pr_forward_ed_script ();
void setup_output ();

extern int no_discards;

static int *xvec, *yvec;	/* Vectors being compared. */
static int *fdiag;		/* Vector, indexed by diagonal, containing
				   the X coordinate of the point furthest
				   along the given diagonal in the forward
				   search of the edit matrix. */
static int *bdiag;		/* Vector, indexed by diagonal, containing
				   the X coordinate of the point furthest
				   along the given diagonal in the backward
				   search of the edit matrix. */

/* Find the midpoint of the shortest edit script for a specified
   portion of the two files.

   We scan from the beginnings of the files, and simultaneously from the ends,
   doing a breadth-first search through the space of edit-sequence.
   When the two searches meet, we have found the midpoint of the shortest
   edit sequence.

   The value returned is the number of the diagonal on which the midpoint lies.
   The diagonal number equals the number of inserted lines minus the number
   of deleted lines (counting only lines before the midpoint).
   The edit cost is stored into *COST; this is the total number of
   lines inserted or deleted (counting only lines before the midpoint).

   This function assumes that the first lines of the specified portions
   of the two files do not match, and likewise that the last lines do not
   match.  The caller must trim matching lines from the beginning and end
   of the portions it is going to specify.

   Note that if we return the "wrong" diagonal value, or if
   the value of bdiag at that diagonal is "wrong",
   the worst this can do is cause suboptimal diff output.
   It cannot cause incorrect diff output.  */

static int
diag (xoff, xlim, yoff, ylim, cost)
     int xoff, xlim, yoff, ylim;
     int *cost;
{
  int *const fd = fdiag;	/* Give the compiler a chance. */
  int *const bd = bdiag;	/* Additional help for the compiler. */
  int *const xv = xvec;		/* Still more help for the compiler. */
  int *const yv = yvec;		/* And more and more . . . */
  const int dmin = xoff - ylim;	/* Minimum valid diagonal. */
  const int dmax = xlim - yoff;	/* Maximum valid diagonal. */
  const int fmid = xoff - yoff;	/* Center diagonal of top-down search. */
  const int bmid = xlim - ylim;	/* Center diagonal of bottom-up search. */
  int fmin = fmid, fmax = fmid;	/* Limits of top-down search. */
  int bmin = bmid, bmax = bmid;	/* Limits of bottom-up search. */
  int c;			/* Cost. */
  int odd = fmid - bmid & 1;	/* True if southeast corner is on an odd
				   diagonal with respect to the northwest. */

  fd[fmid] = xoff;
  bd[bmid] = xlim;

  for (c = 1;; ++c)
    {
      int d;			/* Active diagonal. */
      int big_snake = 0;

      /* Extend the top-down search by an edit step in each diagonal. */
      fmin > dmin ? fd[--fmin - 1] = -1 : ++fmin;
      fmax < dmax ? fd[++fmax + 1] = -1 : --fmax;
      for (d = fmax; d >= fmin; d -= 2)
	{
	  int x, y, oldx, tlo = fd[d - 1], thi = fd[d + 1];

	  if (tlo >= thi)
	    x = tlo + 1;
	  else
	    x = thi;
	  oldx = x;
	  y = x - d;
	  while (x < xlim && y < ylim && xv[x] == yv[y])
	    ++x, ++y;
	  if (x - oldx > 20)
	    big_snake = 1;
	  fd[d] = x;
	  if (odd && bmin <= d && d <= bmax && bd[d] <= fd[d])
	    {
	      *cost = 2 * c - 1;
	      return d;
	    }
	}

      /* Similar extend the bottom-up search. */
      bmin > dmin ? bd[--bmin - 1] = INT_MAX : ++bmin;
      bmax < dmax ? bd[++bmax + 1] = INT_MAX : --bmax;
      for (d = bmax; d >= bmin; d -= 2)
	{
	  int x, y, oldx, tlo = bd[d - 1], thi = bd[d + 1];

	  if (tlo < thi)
	    x = tlo;
	  else
	    x = thi - 1;
	  oldx = x;
	  y = x - d;
	  while (x > xoff && y > yoff && xv[x - 1] == yv[y - 1])
	    --x, --y;
	  if (oldx - x > 20)
	    big_snake = 1;
	  bd[d] = x;
	  if (!odd && fmin <= d && d <= fmax && bd[d] <= fd[d])
	    {
	      *cost = 2 * c;
	      return d;
	    }
	}

      /* Heuristic: check occasionally for a diagonal that has made
	 lots of progress compared with the edit distance.
	 If we have any such, find the one that has made the most
	 progress and return it as if it had succeeded.

	 With this heuristic, for files with a constant small density
	 of changes, the algorithm is linear in the file size.  */

      if (c > 200 && big_snake && heuristic)
	{
	  int best;
	  int bestpos;

	  best = 0;
	  for (d = fmax; d >= fmin; d -= 2)
	    {
	      int dd = d - fmid;
	      if ((fd[d] - xoff)*2 - dd > 12 * (c + (dd > 0 ? dd : -dd)))
		{
		  if (fd[d] * 2 - dd > best
		      && fd[d] - xoff > 20
		      && fd[d] - d - yoff > 20)
		    {
		      int k;
		      int x = fd[d];

		      /* We have a good enough best diagonal;
			 now insist that it end with a significant snake.  */
		      for (k = 1; k <= 20; k++)
			if (xvec[x - k] != yvec[x - d - k])
			  break;

		      if (k == 21)
			{
			  best = fd[d] * 2 - dd;
			  bestpos = d;
			}
		    }
		}
	    }
	  if (best > 0)
	    {
	      *cost = 2 * c - 1;
	      return bestpos;
	    }

	  best = 0;
	  for (d = bmax; d >= bmin; d -= 2)
	    {
	      int dd = d - bmid;
	      if ((xlim - bd[d])*2 + dd > 12 * (c + (dd > 0 ? dd : -dd)))
		{
		  if ((xlim - bd[d]) * 2 + dd > best
		      && xlim - bd[d] > 20
		      && ylim - (bd[d] - d) > 20)
		    {
		      /* We have a good enough best diagonal;
			 now insist that it end with a significant snake.  */
		      int k;
		      int x = bd[d];

		      for (k = 0; k < 20; k++)
			if (xvec[x + k] != yvec[x - d + k])
			  break;
		      if (k == 20)
			{
			  best = (xlim - bd[d]) * 2 + dd;
			  bestpos = d;
			}
		    }
		}
	    }
	  if (best > 0)
	    {
	      *cost = 2 * c - 1;
	      return bestpos;
	    }
	}
    }
}

/* Compare in detail contiguous subsequences of the two files
   which are known, as a whole, to match each other.

   The results are recorded in the vectors files[N].changed_flag, by
   storing a 1 in the element for each line that is an insertion or deletion.

   The subsequence of file 0 is [XOFF, XLIM) and likewise for file 1.

   Note that XLIM, YLIM are exclusive bounds.
   All line numbers are origin-0 and discarded lines are not counted.  */

static void
compareseq (xoff, xlim, yoff, ylim)
     int xoff, xlim, yoff, ylim;
{
  /* Slide down the bottom initial diagonal. */
  while (xoff < xlim && yoff < ylim && xvec[xoff] == yvec[yoff])
    ++xoff, ++yoff;
  /* Slide up the top initial diagonal. */
  while (xlim > xoff && ylim > yoff && xvec[xlim - 1] == yvec[ylim - 1])
    --xlim, --ylim;
  
  /* Handle simple cases. */
  if (xoff == xlim)
    while (yoff < ylim)
      files[1].changed_flag[files[1].realindexes[yoff++]] = 1;
  else if (yoff == ylim)
    while (xoff < xlim)
      files[0].changed_flag[files[0].realindexes[xoff++]] = 1;
  else
    {
      int c, d, f, b;

      /* Find a point of correspondence in the middle of the files.  */

      d = diag (xoff, xlim, yoff, ylim, &c);
      f = fdiag[d];
      b = bdiag[d];

      if (c == 1)
	{
	  /* This should be impossible, because it implies that
	     one of the two subsequences is empty,
	     and that case was handled above without calling `diag'.
	     Let's verify that this is true.  */
	  abort ();
#if 0
	  /* The two subsequences differ by a single insert or delete;
	     record it and we are done.  */
	  if (d < xoff - yoff)
	    files[1].changed_flag[files[1].realindexes[b - d - 1]] = 1;
	  else
	    files[0].changed_flag[files[0].realindexes[b]] = 1;
#endif
	}
      else
	{
	  /* Use that point to split this problem into two subproblems.  */
	  compareseq (xoff, b, yoff, b - d);
	  /* This used to use f instead of b,
	     but that is incorrect!
	     It is not necessarily the case that diagonal d
	     has a snake from b to f.  */
	  compareseq (b, xlim, b - d, ylim);
	}
    }
}

/* Discard lines from one file that have no matches in the other file.

   A line which is discarded will not be considered by the actual
   comparison algorithm; it will be as if that line were not in the file.
   The file's `realindexes' table maps virtual line numbers
   (which don't count the discarded lines) into real line numbers;
   this is how the actual comparison algorithm produces results
   that are comprehensible when the discarded lines are counted.

   When we discard a line, we also mark it as a deletion or insertion
   so that it will be printed in the output.  */

void
discard_confusing_lines (filevec)
     struct file_data filevec[];
{
  unsigned int f, i;
  char *discarded[2];
  int *equiv_count[2];

  /* Allocate our results.  */
  for (f = 0; f < 2; f++)
    {
      filevec[f].undiscarded
	= (int *) xmalloc (filevec[f].buffered_lines * sizeof (int));
      filevec[f].realindexes
	= (int *) xmalloc (filevec[f].buffered_lines * sizeof (int));
    }

  /* Set up equiv_count[F][I] as the number of lines in file F
     that fall in equivalence class I.  */

  equiv_count[0] = (int *) xmalloc (filevec[0].equiv_max * sizeof (int));
  bzero (equiv_count[0], filevec[0].equiv_max * sizeof (int));
  equiv_count[1] = (int *) xmalloc (filevec[1].equiv_max * sizeof (int));
  bzero (equiv_count[1], filevec[1].equiv_max * sizeof (int));

  for (i = 0; i < filevec[0].buffered_lines; ++i)
    ++equiv_count[0][filevec[0].equivs[i]];
  for (i = 0; i < filevec[1].buffered_lines; ++i)
    ++equiv_count[1][filevec[1].equivs[i]];

  /* Set up tables of which lines are going to be discarded.  */

  discarded[0] = (char *) xmalloc (filevec[0].buffered_lines);
  discarded[1] = (char *) xmalloc (filevec[1].buffered_lines);
  bzero (discarded[0], filevec[0].buffered_lines);
  bzero (discarded[1], filevec[1].buffered_lines);

  /* Mark to be discarded each line that matches no line of the other file.
     If a line matches many lines, mark it as provisionally discardable.  */

  for (f = 0; f < 2; f++)
    {
      unsigned int end = filevec[f].buffered_lines;
      char *discards = discarded[f];
      int *counts = equiv_count[1 - f];
      int *equivs = filevec[f].equivs;
      unsigned int many = 5;
      unsigned int tem = end / 64;

      /* Multiply MANY by approximate square root of number of lines.
	 That is the threshold for provisionally discardable lines.  */
      while ((tem = tem >> 2) > 0)
	many *= 2;

      for (i = 0; i < end; i++)
	{
	  int nmatch;
	  if (equivs[i] == 0)
	    continue;
	  nmatch = counts[equivs[i]];
	  if (nmatch == 0)
	    discards[i] = 1;
	  else if (nmatch > many)
	    discards[i] = 2;
	}
    }

  /* Don't really discard the provisional lines except when they occur
     in a run of discardables, with nonprovisionals at the beginning
     and end.  */

  for (f = 0; f < 2; f++)
    {
      unsigned int end = filevec[f].buffered_lines;
      register char *discards = discarded[f];

      for (i = 0; i < end; i++)
	{
	  /* Cancel provisional discards not in middle of run of discards.  */
	  if (discards[i] == 2)
	    discards[i] = 0;
	  else if (discards[i] != 0)
	    {
	      /* We have found a nonprovisional discard.  */
	      register int j;
	      unsigned int length;
	      unsigned int provisional = 0;

	      /* Find end of this run of discardable lines.
		 Count how many are provisionally discardable.  */
	      for (j = i; j < end; j++)
		{
		  if (discards[j] == 0)
		    break;
		  if (discards[j] == 2)
		    ++provisional;
		}

	      /* Cancel provisional discards at end, and shrink the run.  */
	      while (j > i && discards[j - 1] == 2)
		discards[--j] = 0, --provisional;

	      /* Now we have the length of a run of discardable lines
		 whose first and last are not provisional.  */
	      length = j - i;

	      /* If 1/4 of the lines in the run are provisional,
		 cancel discarding of all provisional lines in the run.  */
	      if (provisional * 4 > length)
		{
		  while (j > i)
		    if (discards[--j] == 2)
		      discards[j] = 0;
		}
	      else
		{
		  register unsigned int consec;
		  unsigned int minimum = 1;
		  unsigned int tem = length / 4;

		  /* MINIMUM is approximate square root of LENGTH/4.
		     A subrun of two or more provisionals can stand
		     when LENGTH is at least 16.
		     A subrun of 4 or more can stand when LENGTH >= 64.  */
		  while ((tem = tem >> 2) > 0)
		    minimum *= 2;
		  minimum++;

		  /* Cancel any subrun of MINIMUM or more provisionals
		     within the larger run.  */
		  for (j = 0, consec = 0; j < length; j++)
		    if (discards[i + j] != 2)
		      consec = 0;
		    else if (minimum == ++consec)
		      /* Back up to start of subrun, to cancel it all.  */
		      j -= consec;
		    else if (minimum < consec)
		      discards[i + j] = 0;

		  /* Scan from beginning of run
		     until we find 3 or more nonprovisionals in a row
		     or until the first nonprovisional at least 8 lines in.
		     Until that point, cancel any provisionals.  */
		  for (j = 0, consec = 0; j < length; j++)
		    {
		      if (j >= 8 && discards[i + j] == 1)
			break;
		      if (discards[i + j] == 2)
			consec = 0, discards[i + j] = 0;
		      else if (discards[i + j] == 0)
			consec = 0;
		      else
			consec++;
		      if (consec == 3)
			break;
		    }

		  /* I advances to the last line of the run.  */
		  i += length - 1;

		  /* Same thing, from end.  */
		  for (j = 0, consec = 0; j < length; j++)
		    {
		      if (j >= 8 && discards[i - j] == 1)
			break;
		      if (discards[i - j] == 2)
			consec = 0, discards[i - j] = 0;
		      else if (discards[i - j] == 0)
			consec = 0;
		      else
			consec++;
		      if (consec == 3)
			break;
		    }
		}
	    }
	}
    }

  /* Actually discard the lines. */
  for (f = 0; f < 2; f++)
    {
      char *discards = discarded[f];
      unsigned int end = filevec[f].buffered_lines;
      unsigned int j = 0;
      for (i = 0; i < end; ++i)
	if (no_discards || discards[i] == 0)
	  {
	    filevec[f].undiscarded[j] = filevec[f].equivs[i];
	    filevec[f].realindexes[j++] = i;
	  }
	else
	  filevec[f].changed_flag[i] = 1;
      filevec[f].nondiscarded_lines = j;
    }

  free (discarded[1]);
  free (discarded[0]);
  free (equiv_count[1]);
  free (equiv_count[0]);
}

/* Adjust inserts/deletes of blank lines to join changes
   as much as possible.

   We do something when a run of changed lines include a blank
   line at one end and have an excluded blank line at the other.
   We are free to choose which blank line is included.
   `compareseq' always chooses the one at the beginning,
   but usually it is cleaner to consider the following blank line
   to be the "change".  The only exception is if the preceding blank line
   would join this change to other changes.  */

int inhibit;

static void
shift_boundaries (filevec)
     struct file_data filevec[];
{
  int f;

  if (inhibit)
    return;

  for (f = 0; f < 2; f++)
    {
      char *changed = filevec[f].changed_flag;
      char *other_changed = filevec[1-f].changed_flag;
      int i = 0;
      int j = 0;
      int i_end = filevec[f].buffered_lines;
      int preceding = -1;
      int other_preceding = -1;

      while (1)
	{
	  int start, end, other_start;

	  /* Scan forwards to find beginning of another run of changes.
	     Also keep track of the corresponding point in the other file.  */

	  while (i < i_end && changed[i] == 0)
	    {
	      while (other_changed[j++])
		/* Non-corresponding lines in the other file
		   will count as the preceding batch of changes.  */
		other_preceding = j;
	      i++;
	    }

	  if (i == i_end)
	    break;

	  start = i;
	  other_start = j;

	  while (1)
	    {
	      /* Now find the end of this run of changes.  */

	      while (i < i_end && changed[i] != 0) i++;
	      end = i;

	      /* If the first changed line matches the following unchanged one,
		 and this run does not follow right after a previous run,
		 and there are no lines deleted from the other file here,
		 then classify the first changed line as unchanged
		 and the following line as changed in its place.  */

	      /* You might ask, how could this run follow right after another?
		 Only because the previous run was shifted here.  */

	      if (end != i_end
		  && files[f].equivs[start] == files[f].equivs[end]
		  && !other_changed[j]
		  && end != i_end
		  && !((preceding >= 0 && start == preceding)
		       || (other_preceding >= 0
			   && other_start == other_preceding)))
		{
		  changed[end++] = 1;
		  changed[start++] = 0;
		  ++i;
		  /* Since one line-that-matches is now before this run
		     instead of after, we must advance in the other file
		     to keep in synch.  */
		  ++j;
		}
	      else
		break;
	    }

	  preceding = i;
	  other_preceding = j;
	}
    }
}

/* Cons an additional entry onto the front of an edit script OLD.
   LINE0 and LINE1 are the first affected lines in the two files (origin 0).
   DELETED is the number of lines deleted here from file 0.
   INSERTED is the number of lines inserted here in file 1.

   If DELETED is 0 then LINE0 is the number of the line before
   which the insertion was done; vice versa for INSERTED and LINE1.  */

static struct change *
add_change (line0, line1, deleted, inserted, old)
     int line0, line1, deleted, inserted;
     struct change *old;
{
  struct change *new = (struct change *) xmalloc (sizeof (struct change));

  new->line0 = line0;
  new->line1 = line1;
  new->inserted = inserted;
  new->deleted = deleted;
  new->link = old;
  return new;
}

/* Scan the tables of which lines are inserted and deleted,
   producing an edit script in reverse order.  */

static struct change *
build_reverse_script (filevec)
     struct file_data filevec[];
{
  struct change *script = 0;
  char *changed0 = filevec[0].changed_flag;
  char *changed1 = filevec[1].changed_flag;
  int len0 = filevec[0].buffered_lines;
  int len1 = filevec[1].buffered_lines;

  /* Note that changedN[len0] does exist, and contains 0.  */

  int i0 = 0, i1 = 0;

  while (i0 < len0 || i1 < len1)
    {
      if (changed0[i0] || changed1[i1])
	{
	  int line0 = i0, line1 = i1;

	  /* Find # lines changed here in each file.  */
	  while (changed0[i0]) ++i0;
	  while (changed1[i1]) ++i1;

	  /* Record this change.  */
	  script = add_change (line0, line1, i0 - line0, i1 - line1, script);
	}

      /* We have reached lines in the two files that match each other.  */
      i0++, i1++;
    }

  return script;
}

/* Scan the tables of which lines are inserted and deleted,
   producing an edit script in forward order.  */

static struct change *
build_script (filevec)
     struct file_data filevec[];
{
  struct change *script = 0;
  char *changed0 = filevec[0].changed_flag;
  char *changed1 = filevec[1].changed_flag;
  int len0 = filevec[0].buffered_lines;
  int len1 = filevec[1].buffered_lines;
  int i0 = len0, i1 = len1;

  /* Note that changedN[-1] does exist, and contains 0.  */

#if 0 /* Unnecessary since a line includes its trailing newline.  */
  /* In RCS comparisons, making the existence or nonexistence of trailing
     newlines really matter. */
  if (output_style == OUTPUT_RCS
      && filevec[0].missing_newline != filevec[1].missing_newline)
    changed0[len0 - 1] = changed1[len1 - 1] = 1;
#endif

  while (i0 >= 0 || i1 >= 0)
    {
      if (changed0[i0 - 1] || changed1[i1 - 1])
	{
	  int line0 = i0, line1 = i1;

	  /* Find # lines changed here in each file.  */
	  while (changed0[i0 - 1]) --i0;
	  while (changed1[i1 - 1]) --i1;

	  /* Record this change.  */
	  script = add_change (i0, i1, line0 - i0, line1 - i1, script);
	}

      /* We have reached lines in the two files that match each other.  */
      i0--, i1--;
    }

  return script;
}

/* Report the differences of two files.  DEPTH is the current directory
   depth. */
int
diff_2_files (filevec, depth)
     struct file_data filevec[];
     int depth;
{
  int diags;
  int i;
  struct change *e, *p;
  struct change *script;
  int binary;
  int changes;

  /* See if the two named files are actually the same physical file.
     If so, we know they are identical without actually reading them.  */

  if (output_style != OUTPUT_IFDEF
      && filevec[0].stat.st_ino == filevec[1].stat.st_ino
      && filevec[0].stat.st_dev == filevec[1].stat.st_dev)
    return 0;

  binary = read_files (filevec);

  /* If we have detected that file 0 is a binary file,
     compare the two files as binary.  This can happen
     only when the first chunk is read.
     Also, -q means treat all files as binary.  */

  if (binary || no_details_flag)
    {
      int differs = (filevec[0].buffered_chars != filevec[1].buffered_chars
		     || bcmp (filevec[0].buffer, filevec[1].buffer,
			      filevec[1].buffered_chars));
      if (differs) 
	message (binary ? "Binary files %s and %s differ\n"
		 : "Files %s and %s differ\n",
		 filevec[0].name, filevec[1].name);

      for (i = 0; i < 2; ++i)
	if (filevec[i].buffer)
	  free (filevec[i].buffer);
      return differs;
    }

  /* Allocate vectors for the results of comparison:
     a flag for each line of each file, saying whether that line
     is an insertion or deletion.
     Allocate an extra element, always zero, at each end of each vector.  */

  filevec[0].changed_flag = (char *) xmalloc (filevec[0].buffered_lines + 2);
  filevec[1].changed_flag = (char *) xmalloc (filevec[1].buffered_lines + 2);
  bzero (filevec[0].changed_flag, filevec[0].buffered_lines + 2);
  bzero (filevec[1].changed_flag, filevec[1].buffered_lines + 2);
  filevec[0].changed_flag++;
  filevec[1].changed_flag++;

  /* Some lines are obviously insertions or deletions
     because they don't match anything.  Detect them now,
     and avoid even thinking about them in the main comparison algorithm.  */

  discard_confusing_lines (filevec);

  /* Now do the main comparison algorithm, considering just the
     undiscarded lines.  */

  xvec = filevec[0].undiscarded;
  yvec = filevec[1].undiscarded;
  diags = filevec[0].nondiscarded_lines + filevec[1].nondiscarded_lines + 3;
  fdiag = (int *) xmalloc (diags * sizeof (int));
  fdiag += filevec[1].nondiscarded_lines + 1;
  bdiag = (int *) xmalloc (diags * sizeof (int));
  bdiag += filevec[1].nondiscarded_lines + 1;

  files[0] = filevec[0];
  files[1] = filevec[1];

  compareseq (0, filevec[0].nondiscarded_lines,
	      0, filevec[1].nondiscarded_lines);

  bdiag -= filevec[1].nondiscarded_lines + 1;
  free (bdiag);
  fdiag -= filevec[1].nondiscarded_lines + 1;
  free (fdiag);

  /* Modify the results slightly to make them prettier
     in cases where that can validly be done.  */

  shift_boundaries (filevec);

  /* Get the results of comparison in the form of a chain
     of `struct change's -- an edit script.  */

  if (output_style == OUTPUT_ED)
    script = build_reverse_script (filevec);
  else
    script = build_script (filevec);

  if (script || output_style == OUTPUT_IFDEF)
    {
      setup_output (files[0].name, files[1].name, depth);

      switch (output_style)
	{
	case OUTPUT_CONTEXT:
	  print_context_header (files, 0);
	  print_context_script (script, 0);
	  break;

	case OUTPUT_UNIFIED:
	  print_context_header (files, 1);
	  print_context_script (script, 1);
	  break;

	case OUTPUT_ED:
	  print_ed_script (script);
	  break;

	case OUTPUT_FORWARD_ED:
	  pr_forward_ed_script (script);
	  break;

	case OUTPUT_RCS:
	  print_rcs_script (script);
	  break;

	case OUTPUT_NORMAL:
	  print_normal_script (script);
	  break;

	case OUTPUT_IFDEF:
	  print_ifdef_script (script);
	  break;
	}

      finish_output ();
    }

  /* Set CHANGES if we had any diffs that were printed.
     If some changes are being ignored, we must scan the script to decide.  */
  if (ignore_blank_lines_flag || ignore_regexp)
    {
      struct change *next = script;
      changes = 0;

      while (next && changes == 0)
	{
	  struct change *this, *end;
	  int first0, last0, first1, last1, deletes, inserts;

	  /* Find a set of changes that belong together.  */
	  this = next;
	  end = find_change (next);

	  /* Disconnect them from the rest of the changes,
	     making them a hunk, and remember the rest for next iteration.  */
	  next = end->link;
	  end->link = NULL;

	  /* Determine whether this hunk was printed.  */
	  analyze_hunk (this, &first0, &last0, &first1, &last1,
			&deletes, &inserts);

	  /* Reconnect the script so it will all be freed properly.  */
	  end->link = next;

	  if (deletes || inserts)
	    changes = 1;
	}
    }
  else
    changes = (script != 0);

  for (i = 1; i >= 0; --i)
    {
      free (filevec[i].realindexes);
      free (filevec[i].undiscarded);
    }

  for (i = 1; i >= 0; --i)
    free (--filevec[i].changed_flag);

  for (i = 1; i >= 0; --i)
    free (filevec[i].equivs);

  for (i = 0; i < 2; ++i)
    {
      if (filevec[i].buffer != 0)
	free (filevec[i].buffer);
      free (filevec[i].linbuf);
    }

  for (e = script; e; e = p)
    {
      p = e->link;
      free (e);
    }

  if (! ROBUST_OUTPUT_STYLE (output_style)
      /* For -D, invent newlines silently.  That's ok in C code.  */
      && output_style != OUTPUT_IFDEF)
    for (i = 0; i < 2; ++i)
      if (filevec[i].missing_newline)
	{
	  error ("No newline at end of file %s", filevec[i].name, "");
	  changes = 2;
	}

  return changes;
}
