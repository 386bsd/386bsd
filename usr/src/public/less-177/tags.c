#include <stdio.h>
#include "less.h"

#define	WHITESP(c)	((c)==' ' || (c)=='\t')

#if TAGS

public char *tagfile;
public char *tagpattern;

public char *tags = "tags";

extern int linenums;
extern int sigs;
extern int jump_sline;

/*
 * Find a tag in the "tags" file.
 * Sets "tagfile" to the name of the file containing the tag,
 * and "tagpattern" to the search pattern which should be used
 * to find the tag.
 */
	public void
findtag(tag)
	register char *tag;
{
	register char *p;
	register FILE *f;
	register int taglen;
	int search_char;
	static char tline[200];

	if ((f = fopen(tags, "r")) == NULL)
	{
		error("No tags file", NULL_PARG);
		tagfile = NULL;
		return;
	}

	taglen = strlen(tag);

	/*
	 * Search the tags file for the desired tag.
	 */
	while (fgets(tline, sizeof(tline), f) != NULL)
	{
		if (strncmp(tag, tline, taglen) != 0 || !WHITESP(tline[taglen]))
			continue;

		/*
		 * Found it.
		 * The line contains the tag, the filename and the
		 * pattern, separated by white space.
		 * The pattern is surrounded by a pair of identical
		 * search characters.
		 * Parse the line and extract these parts.
		 */
		tagfile = tagpattern = NULL;

		/*
		 * Skip over the whitespace after the tag name.
		 */
		for (p = tline;  !WHITESP(*p) && *p != '\0';  p++)
			continue;
		while (WHITESP(*p))
			p++;
		if (*p == '\0')
			/* File name is missing! */
			continue;

		/*
		 * Save the file name.
		 * Skip over the whitespace after the file name.
		 */
		tagfile = p;
		while (!WHITESP(*p) && *p != '\0')
			p++;
		*p++ = '\0';
		while (WHITESP(*p))
			p++;
		if (*p == '\0')
			/* Pattern is missing! */
			continue;

		/*
		 * Save the pattern.
		 * Skip to the end of the pattern.
		 * Delete the initial "^" and the final "$" from the pattern.
		 */
		search_char = *p++;
		if (*p == '^')
			p++;
		tagpattern = p;
		while (*p != search_char && *p != '\0')
			p++;
		if (p[-1] == '$')
			p--;
		*p = '\0';

		fclose(f);
		return;
	}
	fclose(f);
	error("No such tag in tags file", NULL_PARG);
	tagfile = NULL;
}

/*
 * Search for a tag.
 * This is a stripped-down version of search().
 * We don't use search() for several reasons:
 *   -	We don't want to blow away any search string we may have saved.
 *   -	The various regular-expression functions (from different systems:
 *	regcmp vs. re_comp) behave differently in the presence of 
 *	parentheses (which are almost always found in a tag).
 */
	public int
tagsearch()
{
	POSITION pos, linepos;
	int linenum;
	char *line;

	pos = ch_zero();
	linenum = find_linenum(pos);

	for (;;)
	{
		/*
		 * Get lines until we find a matching one or 
		 * until we hit end-of-file.
		 */
		if (sigs)
			return (1);

		/*
		 * Read the next line, and save the 
		 * starting position of that line in linepos.
		 */
		linepos = pos;
		pos = forw_raw_line(pos, &line);
		if (linenum != 0)
			linenum++;

		if (pos == NULL_POSITION)
		{
			/*
			 * We hit EOF without a match.
			 */
			error("Tag not found", NULL_PARG);
			return (1);
		}

		/*
		 * If we're using line numbers, we might as well
		 * remember the information we have now (the position
		 * and line number of the current line).
		 */
		if (linenums)
			add_lnum(linenum, pos);

		/*
		 * Test the line to see if we have a match.
		 * Use strncmp because the pattern may be
		 * truncated (in the tags file) if it is too long.
		 */
		if (strncmp(tagpattern, line, strlen(tagpattern)) == 0)
			break;
	}

	jump_loc(linepos, jump_sline);
	return (0);
}

#endif
