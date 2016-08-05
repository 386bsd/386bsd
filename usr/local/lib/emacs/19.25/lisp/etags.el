;;; etags.el --- etags facility for Emacs

;; Copyright (C) 1985, 1986, 1988, 1989, 1992, 1993, 1994
;;	Free Software Foundation, Inc.

;; Author: Roland McGrath <roland@gnu.ai.mit.edu>
;; Keywords: tools

;; This file is part of GNU Emacs.

;; GNU Emacs is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; GNU Emacs is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs; see the file COPYING.  If not, write to
;; the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

;;; Code:

;;;###autoload
(defvar tags-file-name nil
  "*File name of tags table.
To switch to a new tags table, setting this variable is sufficient.
If you set this variable, do not also set `tags-table-list'.
Use the `etags' program to make a tags table file.")
;; Make M-x set-variable tags-file-name like M-x visit-tags-table.
;;;###autoload (put 'tags-file-name 'variable-interactive "fVisit tags table: ")

;;;###autoload
;; Use `visit-tags-table-buffer' to cycle through tags tables in this list.
(defvar tags-table-list nil
  "*List of file names of tags tables to search.
An element that is a directory means the file \"TAGS\" in that directory.
To switch to a new list of tags tables, setting this variable is sufficient.
If you set this variable, do not also set `tags-file-name'.
Use the `etags' program to make a tags table file.")

;;;###autoload
(defvar tags-add-tables 'ask-user
  "*Control whether to add a new tags table to the current list.
t means do; nil means don't (always start a new list).
Any other value means ask the user whether to add a new tags table
to the current list (as opposed to starting a new list).")

(defvar tags-table-list-pointer nil
  "Pointer into `tags-table-list' where the current state of searching is.
Might instead point into a list of included tags tables.
Use `visit-tags-table-buffer' to cycle through tags tables in this list.")

(defvar tags-table-list-started-at nil
  "Pointer into `tags-table-list', where the current search started.")

(defvar tags-table-parent-pointer-list nil
  "Saved state of the tags table that included this one.
Each element is (LIST POINTER STARTED-AT), giving the values of
 `tags-table-list', `tags-table-list-pointer' and
 `tags-table-list-started-at' from before we moved into the current table.")

(defvar tags-table-set-list nil
  "List of sets of tags table which have been used together in the past.
Each element is a list of strings which are file names.")

;;;###autoload
(defvar find-tag-hook nil
  "*Hook to be run by \\[find-tag] after finding a tag.  See `run-hooks'.
The value in the buffer in which \\[find-tag] is done is used,
not the value in the buffer \\[find-tag] goes to.")

;;;###autoload
(defvar find-tag-default-function nil
  "*A function of no arguments used by \\[find-tag] to pick a default tag.
If nil, and the symbol that is the value of `major-mode'
has a `find-tag-default-function' property (see `put'), that is used.
Otherwise, `find-tag-default' is used.")

(defvar default-tags-table-function nil
  "If non-nil, a function to choose a default tags file for a buffer.
This function receives no arguments and should return the default
tags table file to use for the current buffer.")

(defvar tags-location-stack nil
  "List of markers which are locations visited by \\[find-tag].
Pop back to the last location with \\[negative-argument] \\[find-tag].")

;; Tags table state.
;; These variables are local in tags table buffers.

(defvar tag-lines-already-matched nil
  "List of positions of beginnings of lines within the tags table
that are already matched.")

(defvar tags-table-files nil
  "List of file names covered by current tags table.
nil means it has not yet been computed; use `tags-table-files' to do so.")

(defvar tags-completion-table nil
  "Alist of tag names defined in current tags table.")

(defvar tags-included-tables nil
  "List of tags tables included by the current tags table.")

(defvar next-file-list nil
  "List of files for \\[next-file] to process.")

;; Hooks for file formats.

(defvar tags-table-format-hooks '(etags-recognize-tags-table
				  recognize-empty-tags-table)
  "List of functions to be called in a tags table buffer to identify
the type of tags table.  The functions are called in order, with no arguments,
until one returns non-nil.  The function should make buffer-local bindings
of the format-parsing tags function variables if successful.")

(defvar file-of-tag-function nil
  "Function to do the work of `file-of-tag' (which see).")
(defvar tags-table-files-function nil
  "Function to do the work of `tags-table-files' (which see).")
(defvar tags-completion-table-function nil
  "Function to build the tags-completion-table.")
(defvar snarf-tag-function nil
  "Function to get info about a matched tag for `goto-tag-location-function'.")
(defvar goto-tag-location-function nil
  "Function of to go to the location in the buffer specified by a tag.
One argument, the tag info returned by `snarf-tag-function'.")
(defvar find-tag-regexp-search-function nil
  "Search function passed to `find-tag-in-order' for finding a regexp tag.")
(defvar find-tag-regexp-tag-order nil
  "Tag order passed to `find-tag-in-order' for finding a regexp tag.")
(defvar find-tag-regexp-next-line-after-failure-p nil
  "Flag passed to `find-tag-in-order' for finding a regexp tag.")
(defvar find-tag-search-function nil
  "Search function passed to `find-tag-in-order' for finding a tag.")
(defvar find-tag-tag-order nil
  "Tag order passed to `find-tag-in-order' for finding a tag.")
(defvar find-tag-next-line-after-failure-p nil
  "Flag passed to `find-tag-in-order' for finding a tag.")
(defvar list-tags-function nil
  "Function to do the work of `list-tags' (which see).")
(defvar tags-apropos-function nil
  "Function to do the work of `tags-apropos' (which see).")
(defvar tags-included-tables-function nil
  "Function to do the work of `tags-included-tables' (which see).")
(defvar verify-tags-table-function nil
  "Function to return t iff the current buffer contains a valid
\(already initialized\) tags file.")

;; Initialize the tags table in the current buffer.
;; Returns non-nil iff it is a valid tags table.  On
;; non-nil return, the tags table state variable are
;; made buffer-local and initialized to nil.
(defun initialize-new-tags-table ()
  (set (make-local-variable 'tag-lines-already-matched) nil)
  (set (make-local-variable 'tags-table-files) nil)
  (set (make-local-variable 'tags-completion-table) nil)
  (set (make-local-variable 'tags-included-tables) nil)
  ;; Value is t if we have found a valid tags table buffer.
  (let ((hooks tags-table-format-hooks))
    (while (and hooks
		(not (funcall (car hooks))))
      (setq hooks (cdr hooks)))
    hooks))

;;;###autoload
(defun visit-tags-table (file &optional local)
  "Tell tags commands to use tags table file FILE.
FILE should be the name of a file created with the `etags' program.
A directory name is ok too; it means file TAGS in that directory.

Normally \\[visit-tags-table] sets the global value of `tags-file-name'.
With a prefix arg, set the buffer-local value instead.
When you find a tag with \\[find-tag], the buffer it finds the tag
in is given a local value of this variable which is the name of the tags
file the tag was in."
  (interactive (list (read-file-name "Visit tags table: (default TAGS) "
				     default-directory
				     (expand-file-name "TAGS"
						       default-directory)
				     t)
		     current-prefix-arg))
  (or (stringp file) (signal 'wrong-type-argument (list 'stringp file)))
  ;; Bind tags-file-name so we can control below whether the local or
  ;; global value gets set.  Calling visit-tags-table-buffer will
  ;; initialize a buffer for the file and set tags-file-name to the
  ;; fully-expanded name.
  (let ((tags-file-name file))
    (save-excursion
      (or (visit-tags-table-buffer file)
	  (signal 'file-error (list "Visiting tags table"
				    "file does not exist"
				    file)))
      ;; Set FILE to the expanded name.
      (setq file tags-file-name)))
  (if local
      ;; Set the local value of tags-file-name.
      (set (make-local-variable 'tags-file-name) file)
    ;; Set the global value of tags-file-name.
    (setq-default tags-file-name file)))

;; Move tags-table-list-pointer along and set tags-file-name.
;; If NO-INCLUDES is non-nil, ignore included tags tables.
;; Returns nil when out of tables.
(defun tags-next-table (&optional no-includes)
  ;; Do we have any included tables?
  (if (and (not no-includes)
	   (visit-tags-table-buffer 'same)
	   (tags-included-tables))

      ;; Move into the included tags tables.
      (setq tags-table-parent-pointer-list
	    ;; Save the current state of what table we are in.
	    (cons (list tags-table-list
			tags-table-list-pointer
			tags-table-list-started-at)
		  tags-table-parent-pointer-list)
	    ;; Start the pointer in the list of included tables.
	    tags-table-list tags-included-tables
	    tags-table-list-pointer tags-included-tables
	    tags-table-list-started-at tags-included-tables)

    ;; No included tables.  Go to the next table in the list.
    (setq tags-table-list-pointer
	  (cdr tags-table-list-pointer))
    (or tags-table-list-pointer
	;; Wrap around.
	(setq tags-table-list-pointer tags-table-list))

    (if (eq tags-table-list-pointer tags-table-list-started-at)
	;; We have come full circle.  No more tables.
	(if tags-table-parent-pointer-list
	    ;; Pop back to the tags table which includes this one.
	    (progn
	      ;; Restore the state variables.
	      (setq tags-table-list
		    (nth 0 (car tags-table-parent-pointer-list))
		    tags-file-name (car tags-table-list)
		    tags-table-list-pointer
		    (nth 1 (car tags-table-parent-pointer-list))
		    tags-table-list-started-at
		    (nth 2 (car tags-table-parent-pointer-list))
		    tags-table-parent-pointer-list
		    (cdr tags-table-parent-pointer-list))
	      ;; Recurse to skip to the next table after the parent.
	      (tags-next-table t))
	  ;; All out of tags tables.
	  (setq tags-table-list-pointer nil))))

  (and tags-table-list-pointer
       ;; Set tags-file-name to the fully-expanded name.
       (setq tags-file-name
	     (tags-expand-table-name (car tags-table-list-pointer)))))

;; Expand tags table name FILE into a complete file name.
(defun tags-expand-table-name (file)
  (setq file (expand-file-name file))
  (if (file-directory-p file)
      (expand-file-name "TAGS" file)
    file))

;; Search for FILE in LIST (default: tags-table-list); also search
;; tables that are already in core for FILE being included by them.  Return t
;; if we find it, nil if not.  Comparison is done after tags-expand-table-name
;; on both sides.  If MOVE-TO is non-nil, update tags-table-list and the list
;; pointers to point to the table found.  In recursive calls, MOVE-TO is a list
;; value for tags-table-parent-pointer-list describing the position of the
;; caller's search.
(defun tags-find-table-in-list (file move-to &optional list)
  (or list
      (setq list tags-table-list))
  (setq file (tags-expand-table-name file))
  (let (;; Set up the MOVE-TO argument used for the recursive calls we will do
	;; for included tables.  This is a list value for
	;; tags-table-parent-pointer-list describing the included tables we are
	;; descending; we cons our position onto the list from our recursive
	;; caller (which is searching a list that contains the table whose
	;; included tables we are searching).  The atom `in-progress' is a
	;; placeholder; when a recursive call locates FILE, we replace
	;; 'in-progress with the tail of LIST whose car contained FILE.
	(recursing-move-to (if move-to
			       (cons (list list 'in-progress 'in-progress)
				     (if (eq move-to t) nil move-to))))
	this-file)
    (while (and (consp list)		; We set LIST to t when we locate FILE.
		(not (string= file
			      (setq this-file
				    (tags-expand-table-name (car list))))))
      (if (get-file-buffer this-file)
	  ;; This table is already in core.  Visit it and recurse to check
	  ;; its included tables.
	  (save-excursion
	    (let ((tags-file-name this-file)
		  found)
	      (visit-tags-table-buffer 'same)
	      (and (tags-included-tables)
		   ;; We have some included tables; check them.
		   (tags-find-table-in-list file recursing-move-to
					    tags-included-tables)
		   (progn
		     ;; We found FILE in the included table.
		     (if move-to
			 (progn
			   ;; The recursive call has already frobbed the list
			   ;; pointers.  It set tags-table-parent-pointer-list
			   ;; to a list including RECURSING-MOVE-TO.  Now we
			   ;; must mutate that cons so its list pointers show
			   ;; the position where we found this included table.
			   (setcar (cdr (car recursing-move-to)) list)
			   (setcar (cdr (cdr (car recursing-move-to))) list)
			   ;; Don't do further list frobnication below.
			   (setq move-to nil)))
		     (setq list t))))))
      (if (consp list)
	  (setq list (cdr list))))
    (and list move-to
	 (progn
	   ;; We have located FILE in the list.
	   ;; Now frobnicate the list pointers to point to it.
	   (setq tags-table-list-started-at list
		 tags-table-list-pointer list)
	   (if (consp move-to)
	       ;; We are in a recursive call.  MOVE-TO is the value for
	       ;; tags-table-parent-pointer-list that describes the tables
	       ;; descended by the caller (and its callers, recursively).
	       (setq tags-table-parent-pointer-list move-to)))))
  list)

;; Local var in visit-tags-table-buffer
;; which is set by tags-table-including.
(defvar visit-tags-table-buffer-cont)

;; Subroutine of visit-tags-table-buffer.  Frobs its local vars.
;; Search TABLES for one that has tags for THIS-FILE.  Recurses on
;; included tables.  Returns the tail of TABLES (or of an inner
;; included list) whose car is a table listing THIS-FILE.  If
;; CORE-ONLY is non-nil, check only tags tables that are already in
;; buffers--don't visit any new files.
(defun tags-table-including (this-file tables core-only &optional recursing)
  (let ((starting-tables tables)
	(found nil))
    ;; Loop over TABLES, looking for one containing tags for THIS-FILE.
    (while (and (not found)
		tables)
      (let ((tags-file-name (tags-expand-table-name (car tables))))
	(if (or (get-file-buffer tags-file-name)
		(and (not core-only)
		     (file-exists-p tags-file-name)))
	    (progn
	      ;; Select the tags table buffer and get the file list up to date.
	      (visit-tags-table-buffer 'same)
	      (or tags-table-files
		  (setq tags-table-files
			(funcall tags-table-files-function)))

	      (cond ((member this-file tags-table-files)
		     ;; Found it.
		     (setq found tables))

		    ((tags-included-tables)
		     ;; This table has included tables.  Check them.
		     (let ((old tags-table-parent-pointer-list))
		       (unwind-protect
			   (progn
			     (or recursing
				 ;; At top level (not in an included tags
				 ;; table), set the list to nil so we can
				 ;; collect just the elts from this run.
				 (setq tags-table-parent-pointer-list nil))
			     (setq found
				   ;; Recurse on the list of included tables.
				   (tags-table-including this-file
							 tags-included-tables
							 core-only
							 t))
			     (if found
				 ;; One of them lists THIS-FILE.
				 ;; Set the table list state variables to move
				 ;; us inside the list of included tables.
				 (setq tags-table-parent-pointer-list
				       (cons
					(list tags-table-list
					      tags-table-list-pointer
					      tags-table-list-started-at)
					tags-table-parent-pointer-list)
				       tags-table-list starting-tables
				       tags-table-list-pointer found
				       tags-table-list-started-at found
				       ;; Set a local variable of
				       ;; our caller, visit-tags-table-buffer.
				       ;; Set it so we won't frob lists later.
				       visit-tags-table-buffer-cont
				       'included)))
			 (or recursing
			     ;; tags-table-parent-pointer-list now describes
			     ;; the path of included tables taken by recursive
			     ;; invocations of this function.  The recursive
			     ;; calls have consed onto the front of the list,
			     ;; so it is now outermost first.  We want it
			     ;; innermost first, so reverse it.  Then append
			     ;; the old list (from before we were called the
			     ;; outermost time), to get the complete current
			     ;; state of included tables.
			     (setq tags-table-parent-pointer-list
				   (nconc (nreverse
					   tags-table-parent-pointer-list)
					  old))))))))))
      (setq tables (cdr tables)))
    found))

(defun visit-tags-table-buffer (&optional cont)
  "Select the buffer containing the current tags table.
If optional arg is a string, visit that file as a tags table.
If optional arg is t, visit the next table in `tags-table-list'.
If optional arg is the atom `same', don't look for a new table;
 just select the buffer visiting `tags-file-name'.
If arg is nil or absent, choose a first buffer from information in
 `tags-file-name', `tags-table-list', `tags-table-list-pointer'.
Returns t if it visits a tags table, or nil if there are no more in the list."

  ;; Set tags-file-name to the tags table file we want to visit.
  (let ((visit-tags-table-buffer-cont cont))
    (cond ((eq visit-tags-table-buffer-cont 'same)
	   ;; Use the ambient value of tags-file-name.
	   (or tags-file-name
	       (error (substitute-command-keys
		       (concat "No tags table in use!  "
			       "Use \\[visit-tags-table] to select one.")))))

	  ((eq t visit-tags-table-buffer-cont)
	   ;; Find the next table.
	   (if (tags-next-table)
	       ;; Skip over nonexistent files.
	       (let (file)
		 (while (and (setq file
				   (tags-expand-table-name tags-file-name))
			     (not (or (get-file-buffer file)
				      (file-exists-p file))))
		   (tags-next-table)))))

	  (t
	   ;; We are visiting a table anew, so throw away the previous
	   ;; context of what included tables we were inside of.
	   (while tags-table-parent-pointer-list
	     ;; Set the pointer as if we had iterated through all the
	     ;; tables in the list.
	     (setq tags-table-list-pointer tags-table-list-started-at)
	     ;; Fetching the next table will pop the included-table state.
	     (tags-next-table))

	   ;; Pick a table out of our hat.
	   (setq tags-file-name
		 (or
		  ;; If passed a string, use that.
		  (if (stringp visit-tags-table-buffer-cont)
		      (prog1 visit-tags-table-buffer-cont
			(setq visit-tags-table-buffer-cont nil)))
		  ;; First, try a local variable.
		  (cdr (assq 'tags-file-name (buffer-local-variables)))
		  ;; Second, try a user-specified function to guess.
		  (and default-tags-table-function
		       (funcall default-tags-table-function))
		  ;; Third, look for a tags table that contains
		  ;; tags for the current buffer's file.
		  ;; If one is found, the lists will be frobnicated,
		  ;; and VISIT-TAGS-TABLE-BUFFER-CONT
		  ;; will be set non-nil so we don't do it below.
		  (and buffer-file-name
		       (car (or 
			     ;; First check only tables already in buffers.
			     (save-excursion
			       (tags-table-including buffer-file-name
						     tags-table-list
						     t))
			     ;; Since that didn't find any, now do the
			     ;; expensive version: reading new files.
			     (save-excursion
			       (tags-table-including buffer-file-name
						     tags-table-list
						     nil)))))
		  ;; Fourth, use the user variable tags-file-name, if it is
		  ;; not already in tags-table-list.
		  (and tags-file-name
		       (not (tags-find-table-in-list tags-file-name nil))
		       tags-file-name)
		  ;; Fifth, use the user variable giving the table list.
		  ;; Find the first element of the list that actually exists.
		  (let ((list tags-table-list)
			file)
		    (while (and list
				(setq file (tags-expand-table-name (car list)))
				(not (get-file-buffer file))
				(not (file-exists-p file)))
		      (setq list (cdr list)))
		    (car list))
		  ;; Finally, prompt the user for a file name.
		  (expand-file-name
		   (read-file-name "Visit tags table: (default TAGS) "
				   default-directory
				   "TAGS"
				   t))))))

    ;; Expand the table name into a full file name.
    (setq tags-file-name (tags-expand-table-name tags-file-name))

    (if (and (eq visit-tags-table-buffer-cont t)
	     (null tags-table-list-pointer))
	;; All out of tables.
	nil

      ;; Verify that tags-file-name is a valid tags table.
      (if (if (get-file-buffer tags-file-name)
	      ;; The file is already in a buffer.  Check for the visited file
	      ;; having changed since we last used it.
	      (let (win)
		(set-buffer (get-file-buffer tags-file-name))
		(setq win (or verify-tags-table-function
			      (initialize-new-tags-table)))
		(if (or (verify-visited-file-modtime (current-buffer))
			(not (yes-or-no-p
			      "Tags file has changed, read new contents? ")))
		    (and win (funcall verify-tags-table-function))
		  (revert-buffer t t)
		  (initialize-new-tags-table)))
	    (set-buffer (find-file-noselect tags-file-name))
	    (or (string= tags-file-name buffer-file-name)
		;; find-file-noselect has changed the file name.
		;; Propagate the change to tags-file-name and tags-table-list.
		(let ((tail (member tags-file-name tags-table-list)))
		  (if tail
		      (setcar tail buffer-file-name))
		  (setq tags-file-name buffer-file-name)))
	    (initialize-new-tags-table))

	  ;; We have a valid tags table.
	  (progn
	    ;; Bury the tags table buffer so it
	    ;; doesn't get in the user's way.
	    (bury-buffer (current-buffer))

	    ;; If this was a new table selection (CONT is nil), make sure
	    ;; tags-table-list includes the chosen table, and update the
	    ;; list pointer variables.
	    (or visit-tags-table-buffer-cont
		;; Look in the list for the table we chose.
		;; This updates the list pointers if it finds the table.
		(or (tags-find-table-in-list tags-file-name t)
		    ;; The table is not in the current set.
		    ;; Try to find it in another previously used set.
		    (let ((sets tags-table-set-list))
		      (while (and sets
				  (not (tags-find-table-in-list tags-file-name
								t (car sets))))
			(setq sets (cdr sets)))
		      (if sets
			  ;; Found in some other set.  Switch to that set.
			  (progn
			    (or (memq tags-table-list tags-table-set-list)
				;; Save the current list.
				(setq tags-table-set-list
				      (cons tags-table-list
					    tags-table-set-list)))
			    ;; The list pointers are already up to date;
			    ;; we need only set tags-table-list.
			    (setq tags-table-list (car sets)))

			;; Not found in any existing set.
			(if (and tags-table-list
				 (or (eq t tags-add-tables)
				     (and tags-add-tables
					  (y-or-n-p
					   (concat "Keep current list of "
						   "tags tables also? ")))))
			    ;; Add it to the current list.
			    (setq tags-table-list (cons tags-file-name
							tags-table-list))
			  ;; Make a fresh list, and store the old one.
			  (message "Starting a new list of tags tables")
			  (or (null tags-table-list)
			      (memq tags-table-list tags-table-set-list)
			      (setq tags-table-set-list
				    (cons tags-table-list
					  tags-table-set-list)))
			  (setq tags-table-list (list tags-file-name)))

			;; Set the tags table list state variables to point
			;; at the table we want to use first.
			(setq tags-table-list-started-at tags-table-list
			      tags-table-list-pointer tags-table-list)))))

	    ;; Return of t says the tags table is valid.
	    t)

	;; The buffer was not valid.  Don't use it again.
	(let ((file tags-file-name))
	  (kill-local-variable 'tags-file-name)
	  (if (eq file tags-file-name)
	      (setq tags-file-name nil)))
	(error "File %s is not a valid tags table" buffer-file-name)))))

(defun file-of-tag ()
  "Return the file name of the file whose tags point is within.
Assumes the tags table is the current buffer.
File name returned is relative to tags table file's directory."
  (funcall file-of-tag-function))

;;;###autoload
(defun tags-table-files ()
  "Return a list of files in the current tags table.
Assumes the tags table is the current buffer.
File names returned are absolute."
  (or tags-table-files
      (setq tags-table-files
	    (funcall tags-table-files-function))))

(defun tags-included-tables ()
  "Return a list of tags tables included by the current table.
Assumes the tags table is the current buffer."
  (or tags-included-tables
      (setq tags-included-tables (funcall tags-included-tables-function))))

;; Build tags-completion-table on demand.  The single current tags table
;; and its included tags tables (and their included tables, etc.) have
;; their tags included in the completion table.
(defun tags-completion-table ()
  (or tags-completion-table
      (condition-case ()
	  (prog2
	   (message "Making tags completion table for %s..." buffer-file-name)
	   (let ((included (tags-included-tables))
		 (table (funcall tags-completion-table-function)))
	     (save-excursion
	       ;; Iterate over the list of included tables, and combine each
	       ;; included table's completion obarray to the parent obarray.
	       (while included
		 ;; Visit the buffer.
		 (let ((tags-file-name (car included)))
		   (visit-tags-table-buffer 'same))
		 ;; Recurse in that buffer to compute its completion table.
		 (if (tags-completion-table)
		     ;; Combine the tables.
		     (mapatoms (function
				(lambda (sym)
				  (intern (symbol-name sym) table)))
			       tags-completion-table))
		 (setq included (cdr included))))
	     (setq tags-completion-table table))
	   (message "Making tags completion table for %s...done"
		    buffer-file-name))
	(quit (message "Tags completion table construction aborted.")
	      (setq tags-completion-table nil)))))

;; Completion function for tags.  Does normal try-completion,
;; but builds tags-completion-table on demand.
(defun tags-complete-tag (string predicate what)
  (save-excursion
    ;; If we need to ask for the tag table, allow that.
    (let ((enable-recursive-minibuffers t))
      (visit-tags-table-buffer))
    (if (eq what t)
	(all-completions string (tags-completion-table) predicate)
      (try-completion string (tags-completion-table) predicate))))

;; Return a default tag to search for, based on the text at point.
(defun find-tag-default ()
  (save-excursion
    (while (looking-at "\\sw\\|\\s_")
      (forward-char 1))
    (if (or (re-search-backward "\\sw\\|\\s_"
				(save-excursion (beginning-of-line) (point))
				t)
	    (re-search-forward "\\(\\sw\\|\\s_\\)+"
			       (save-excursion (end-of-line) (point))
			       t))
	(progn (goto-char (match-end 0))
	       (buffer-substring (point)
				 (progn (forward-sexp -1)
					(while (looking-at "\\s'")
					  (forward-char 1))
					(point))))
      nil)))

;; Read a tag name from the minibuffer with defaulting and completion.
(defun find-tag-tag (string)
  (let* ((default (funcall (or find-tag-default-function
			       (get major-mode 'find-tag-default-function)
			       'find-tag-default)))
	 (spec (completing-read (if default
				    (format "%s(default %s) " string default)
				  string)
				'tags-complete-tag)))
    (if (equal spec "")
	(or default (error "There is no default tag"))
      spec)))

(defvar last-tag nil
  "Last tag found by \\[find-tag].")

;; Get interactive args for find-tag{-noselect,-other-window,-regexp}.
(defun find-tag-interactive (prompt &optional no-default)
  (if current-prefix-arg
      (list nil (if (< (prefix-numeric-value current-prefix-arg) 0)
		    '-
		  t))
    (list (if no-default
	      (read-string prompt)
	    (find-tag-tag prompt)))))

;;;###autoload
(defun find-tag-noselect (tagname &optional next-p regexp-p)
  "Find tag (in current tags table) whose name contains TAGNAME.
Returns the buffer containing the tag's definition and moves its point there,
but does not select the buffer.
The default for TAGNAME is the expression in the buffer near point.

If second arg NEXT-P is t (interactively, with prefix arg), search for
another tag that matches the last tagname or regexp used.  When there are
multiple matches for a tag, more exact matches are found first.  If NEXT-P
is the atom `-' (interactively, with prefix arg that is a negative number
or just \\[negative-argument]), pop back to the previous tag gone to.

If third arg REGEXP-P is non-nil, treat TAGNAME as a regexp.

See documentation of variable `tags-file-name'."
  (interactive (find-tag-interactive "Find tag: "))

  ;; Save the current buffer's value of `find-tag-hook' before selecting the
  ;; tags table buffer.
  (let ((local-find-tag-hook find-tag-hook))
    (if (eq '- next-p)
	;; Pop back to a previous location.
	(if (null tags-location-stack)
	    (error "No previous tag locations")
	  (let ((marker (car tags-location-stack)))
	    ;; Pop the stack.
	    (setq tags-location-stack (cdr tags-location-stack))
	    (prog1
		;; Move to the saved location.
		(set-buffer (marker-buffer marker))
	      (goto-char (marker-position marker))
	      ;; Kill that marker so it doesn't slow down editing.
	      (set-marker marker nil nil)
	      ;; Run the user's hook.  Do we really want to do this for pop?
	      (run-hooks 'local-find-tag-hook))))
      (if next-p
	  ;; Find the same table we last used.
	  (visit-tags-table-buffer 'same)
	;; Pick a table to use.
	(visit-tags-table-buffer)
	;; Record TAGNAME for a future call with NEXT-P non-nil.
	(setq last-tag tagname))
      (prog1
	  ;; Record the location so we can pop back to it later.
	  (marker-buffer
	   (car
	    (setq tags-location-stack
		  (cons (let ((marker (make-marker)))
			  (save-excursion
			    (set-buffer
			     ;; find-tag-in-order does the real work.
			     (find-tag-in-order
			      (if next-p last-tag tagname)
			      (if regexp-p
				  find-tag-regexp-search-function
				find-tag-search-function)
			      (if regexp-p
				  find-tag-regexp-tag-order
				find-tag-tag-order)
			      (if regexp-p
				  find-tag-regexp-next-line-after-failure-p
				find-tag-next-line-after-failure-p)
			      (if regexp-p "matching" "containing")
			      (not next-p)))
			    (set-marker marker (point))))
			tags-location-stack))))
	(run-hooks 'local-find-tag-hook)))))

;;;###autoload
(defun find-tag (tagname &optional next-p regexp-p)
  "Find tag (in current tags table) whose name contains TAGNAME.
Select the buffer containing the tag's definition, and move point there.
The default for TAGNAME is the expression in the buffer around or before point.

If second arg NEXT-P is t (interactively, with prefix arg), search for
another tag that matches the last tagname or regexp used.  When there are
multiple matches for a tag, more exact matches are found first.  If NEXT-P
is the atom `-' (interactively, with prefix arg that is a negative number
or just \\[negative-argument]), pop back to the previous tag gone to.

See documentation of variable `tags-file-name'."
  (interactive (find-tag-interactive "Find tag: "))
  (switch-to-buffer (find-tag-noselect tagname next-p regexp-p)))
;;;###autoload (define-key esc-map "." 'find-tag)

;;;###autoload
(defun find-tag-other-window (tagname &optional next-p regexp-p)
  "Find tag (in current tags table) whose name contains TAGNAME.
Select the buffer containing the tag's definition in another window, and
move point there.  The default for TAGNAME is the expression in the buffer
around or before point.

If second arg NEXT-P is t (interactively, with prefix arg), search for
another tag that matches the last tagname or regexp used.  When there are
multiple matches for a tag, more exact matches are found first.  If NEXT-P
is negative (interactively, with prefix arg that is a negative number or
just \\[negative-argument]), pop back to the previous tag gone to.

See documentation of variable `tags-file-name'."
  (interactive (find-tag-interactive "Find tag other window: "))

  ;; This hair is to deal with the case where the tag is found in the
  ;; selected window's buffer; without the hair, point is moved in both
  ;; windows.  To prevent this, we save the selected window's point before
  ;; doing find-tag-noselect, and restore it after.
  (let* ((window-point (window-point (selected-window)))
	 (tagbuf (find-tag-noselect tagname next-p regexp-p))
	 (tagpoint (progn (set-buffer tagbuf) (point))))
    (set-window-point (prog1
			  (selected-window)
			(switch-to-buffer-other-window tagbuf)
			;; We have to set this new window's point; it
			;; might already have been displaying a
			;; different portion of tagbuf, in which case
			;; switch-to-buffer-other-window doesn't set
			;; the window's point from the buffer.
			(set-window-point (selected-window) tagpoint))
		      window-point)))
;;;###autoload (define-key ctl-x-4-map "." 'find-tag-other-window)

;;;###autoload
(defun find-tag-other-frame (tagname &optional next-p)
  "Find tag (in current tags table) whose name contains TAGNAME.
Select the buffer containing the tag's definition in another frame, and
move point there.  The default for TAGNAME is the expression in the buffer
around or before point.

If second arg NEXT-P is t (interactively, with prefix arg), search for
another tag that matches the last tagname or regexp used.  When there are
multiple matches for a tag, more exact matches are found first.  If NEXT-P
is negative (interactively, with prefix arg that is a negative number or
just \\[negative-argument]), pop back to the previous tag gone to.

See documentation of variable `tags-file-name'."
  (interactive (find-tag-interactive "Find tag other frame: "))
  (let ((pop-up-frames t))
    (find-tag-other-window tagname next-p)))
;;;###autoload (define-key ctl-x-5-map "." 'find-tag-other-frame)

;;;###autoload
(defun find-tag-regexp (regexp &optional next-p other-window)
  "Find tag (in current tags table) whose name matches REGEXP.
Select the buffer containing the tag's definition and move point there.

If second arg NEXT-P is t (interactively, with prefix arg), search for
another tag that matches the last tagname or regexp used.  When there are
multiple matches for a tag, more exact matches are found first.  If NEXT-P
is negative (interactively, with prefix arg that is a negative number or
just \\[negative-argument]), pop back to the previous tag gone to.

If third arg OTHER-WINDOW is non-nil, select the buffer in another window.

See documentation of variable `tags-file-name'."
  (interactive (find-tag-interactive "Find tag regexp: " t))
  ;; We go through find-tag-other-window to do all the display hair there.
  (funcall (if other-window 'find-tag-other-window 'find-tag)
	   regexp next-p t))

;; Internal tag finding function.

;; PATTERN is a string to pass to second arg SEARCH-FORWARD-FUNC, and to
;; any member of the function list ORDER (third arg).  If ORDER is nil,
;; use saved state to continue a previous search.

;; Fourth arg MATCHING is a string, an English '-ing' word, to be used in
;; an error message.

;; Fifth arg NEXT-LINE-AFTER-FAILURE-P is non-nil if after a failed match,
;; point should be moved to the next line.

;; Algorithm is as follows.  For each qualifier-func in ORDER, go to
;; beginning of tags file, and perform inner loop: for each naive match for
;; PATTERN found using SEARCH-FORWARD-FUNC, qualify the naive match using
;; qualifier-func.  If it qualifies, go to the specified line in the
;; specified source file and return.  Qualified matches are remembered to
;; avoid repetition.  State is saved so that the loop can be continued.

(defun find-tag-in-order (pattern
			  search-forward-func
			  order
			  next-line-after-failure-p
			  matching
			  first-search)
  (let (file				;name of file containing tag
	tag-info			;where to find the tag in FILE
	tags-table-file			;name of tags file
	(first-table t)
	(tag-order order)
	goto-func
	)
    (save-excursion
      (or first-search			;find-tag-noselect has already done it.
	  (visit-tags-table-buffer 'same))

      ;; Get a qualified match.
      (catch 'qualified-match-found

	;; Iterate over the list of tags tables.
	(while (or first-table
		   (visit-tags-table-buffer t))

	  (if first-search
	      (setq tag-lines-already-matched nil))

	  (and first-search first-table
	       ;; Start at beginning of tags file.
	       (goto-char (point-min)))
	  (setq first-table nil)

	  (setq tags-table-file buffer-file-name)
	  ;; Iterate over the list of ordering predicates.
	  (while order
	    (while (funcall search-forward-func pattern nil t)
	      ;; Naive match found.  Qualify the match.
	      (and (funcall (car order) pattern)
		   ;; Make sure it is not a previous qualified match.
		   ;; Use of `memq' depends on numbers being eq.
		   (not (memq (save-excursion (beginning-of-line) (point))
			      tag-lines-already-matched))
		   (throw 'qualified-match-found nil))
	      (if next-line-after-failure-p
		  (forward-line 1)))
	    ;; Try the next flavor of match.
	    (setq order (cdr order))
	    (goto-char (point-min)))
	  (setq order tag-order))
	;; We throw out on match, so only get here if there were no matches.
	(error "No %stags %s %s" (if first-search "" "more ")
	       matching pattern))
      
      ;; Found a tag; extract location info.
      (beginning-of-line)
      (setq tag-lines-already-matched (cons (point)
					    tag-lines-already-matched))
      ;; Expand the filename, using the tags table buffer's default-directory.
      (setq file (expand-file-name (file-of-tag))
	    tag-info (funcall snarf-tag-function))

      ;; Get the local value in the tags table buffer before switching buffers.
      (setq goto-func goto-tag-location-function)

      ;; Find the right line in the specified file.
      (set-buffer (find-file-noselect file))
      (widen)
      (push-mark)
      (funcall goto-func tag-info)
      
      ;; Give this buffer a local value of tags-file-name.
      ;; The next time visit-tags-table-buffer is called,
      ;; it will use the same tags table that found a match in this buffer.
      (make-local-variable 'tags-file-name)
      (setq tags-file-name tags-table-file)
      
      ;; Return the buffer where the tag was found.
      (current-buffer))))

;; `etags' TAGS file format support.

;; If the current buffer is a valid etags TAGS file, give it local values of
;; the tags table format variables, and return non-nil.
(defun etags-recognize-tags-table ()
  (and (etags-verify-tags-table)
       ;; It is annoying to flash messages on the screen briefly,
       ;; and this message is not useful.  -- rms
       ;; (message "%s is an `etags' TAGS file" buffer-file-name)
       (mapcar (function (lambda (elt)
			   (set (make-local-variable (car elt)) (cdr elt))))
	       '((file-of-tag-function . etags-file-of-tag)
		 (tags-table-files-function . etags-tags-table-files)
		 (tags-completion-table-function . etags-tags-completion-table)
		 (snarf-tag-function . etags-snarf-tag)
		 (goto-tag-location-function . etags-goto-tag-location)
		 (find-tag-regexp-search-function . re-search-forward)
		 (find-tag-regexp-tag-order . (tag-re-match-p))
		 (find-tag-regexp-next-line-after-failure-p . t)
		 (find-tag-search-function . search-forward)
		 (find-tag-tag-order . (tag-exact-match-p tag-word-match-p
							  tag-any-match-p))
		 (find-tag-next-line-after-failure-p . nil)
		 (list-tags-function . etags-list-tags)
		 (tags-apropos-function . etags-tags-apropos)
		 (tags-included-tables-function . etags-tags-included-tables)
		 (verify-tags-table-function . etags-verify-tags-table)
		 ))))

;; Return non-nil iff the current buffer is a valid etags TAGS file.
(defun etags-verify-tags-table ()
  ;; Use eq instead of = in case char-after returns nil.
  (eq (char-after 1) ?\f))

(defun etags-file-of-tag ()
  (save-excursion
    (search-backward "\f\n")
    (forward-char 2)
    (buffer-substring (point)
		      (progn (skip-chars-forward "^,") (point)))))

(defun etags-tags-completion-table ()
  (let ((table (make-vector 511 0)))
    (save-excursion
      (goto-char (point-min))
      ;; This monster regexp matches an etags tag line.
      ;;   \1 is the string to match;
      ;;   \2 is not interesting;
      ;;   \3 is the guessed tag name; XXX guess should be better eg DEFUN
      ;;   \4 is not interesting;
      ;;   \5 is the explicitly-specified tag name.
      ;;   \6 is the line to start searching at;
      ;;   \7 is the char to start searching at.
      (while (re-search-forward
	      "^\\(\\(.+[^-a-zA-Z0-9_$]+\\)?\\([-a-zA-Z0-9_$]+\\)\
\[^-a-zA-Z0-9_$]*\\)\177\\(\\([^\n\001]+\\)\001\\)?\
\\([0-9]+\\)?,\\([0-9]+\\)?\n"
	      nil t)
	(intern	(if (match-beginning 5)
		    ;; There is an explicit tag name.
		    (buffer-substring (match-beginning 5) (match-end 5))
		  ;; No explicit tag name.  Best guess.
		  (buffer-substring (match-beginning 3) (match-end 3)))
		table)))
    table))

(defun etags-snarf-tag ()
  (let (tag-text line startpos)
    (search-forward "\177")
    (setq tag-text (buffer-substring (1- (point))
				     (save-excursion (beginning-of-line)
						     (point))))
    ;; Skip explicit tag name if present.
    (search-forward "\001" (save-excursion (forward-line 1) (point)) t)
    (if (looking-at "[0-9]")
	(setq line (string-to-int (buffer-substring
				   (point)
				   (progn (skip-chars-forward "0-9")
					  (point))))))
    (search-forward ",")
    (if (looking-at "[0-9]")
	(setq startpos (string-to-int (buffer-substring
				       (point)
				       (progn (skip-chars-forward "0-9")
					      (point))))))
    ;; Leave point on the next line of the tags file.
    (forward-line 1)
    (cons tag-text (cons line startpos))))

;; TAG-INFO is a cons (TEXT LINE . POSITION) where TEXT is the initial part
;; of a line containing the tag and POSITION is the character position of
;; TEXT within the file (starting from 1); LINE is the line number.  Either
;; LINE or POSITION may be nil; POSITION is used if present.  If the tag
;; isn't exactly at the given position then look around that position using
;; a search window which expands until it hits the start of file.
(defun etags-goto-tag-location (tag-info)
  (let ((startpos (cdr (cdr tag-info)))
	;; This constant is 1/2 the initial search window.
	;; There is no sense in making it too small,
	;; since just going around the loop once probably
	;; costs about as much as searching 2000 chars.
	(offset 1000)
	(found nil)
	(pat (concat (if (eq selective-display t)
			 "\\(^\\|\^m\\)" "^")
		     (regexp-quote (car tag-info)))))
    ;; If no char pos was given, try the given line number.
    (or startpos
	(if (car (cdr tag-info))
	    (setq startpos (progn (goto-line (car (cdr tag-info)))
				  (point)))))
    (or startpos
	(setq startpos (point-min)))
    ;; First see if the tag is right at the specified location.
    (goto-char startpos)
    (setq found (looking-at pat))
    (while (and (not found)
		(progn
		  (goto-char (- startpos offset))
		  (not (bobp))))
      (setq found
	    (re-search-forward pat (+ startpos offset) t)
	    offset (* 3 offset)))	; expand search window
    (or found
	(re-search-forward pat nil t)
	(error "Rerun etags: `%s' not found in %s"
	       pat buffer-file-name)))
  ;; Position point at the right place
  ;; if the search string matched an extra Ctrl-m at the beginning.
  (and (eq selective-display t)
       (looking-at "\^m")
       (forward-char 1))
  (beginning-of-line))

(defun etags-list-tags (file)
  (goto-char 1)
  (if (not (search-forward (concat "\f\n" file ",") nil t))
      nil
    (forward-line 1)
    (while (not (or (eobp) (looking-at "\f")))
      (let ((tag (buffer-substring (point)
				   (progn (skip-chars-forward "^\177")
					  (point)))))
	(princ (if (looking-at "[^\n]+\001")
		   ;; There is an explicit tag name; use that.
		   (buffer-substring (point)
				     (progn (skip-chars-forward "^\001")
					    (point)))
		 tag)))
      (terpri)
      (forward-line 1))
    t))

(defun etags-tags-apropos (string)
  (goto-char 1)
  (while (re-search-forward string nil t)
    (beginning-of-line)
    (princ (buffer-substring (point)
			     (progn (skip-chars-forward "^\177")
				    (point))))
    (terpri)
    (forward-line 1)))

(defun etags-tags-table-files ()
  (let ((files nil)
	beg)
    (goto-char (point-min))
    (while (search-forward "\f\n" nil t)
      (setq beg (point))
      (skip-chars-forward "^,\n")
      (or (looking-at ",include$")
	  ;; Expand in the default-directory of the tags table buffer.
	  (setq files (cons (expand-file-name (buffer-substring beg (point)))
			    files))))
    (nreverse files)))

(defun etags-tags-included-tables ()
  (let ((files nil)
	beg)
    (goto-char (point-min))
    (while (search-forward "\f\n" nil t)
      (setq beg (point))
      (skip-chars-forward "^,\n")
      (if (looking-at ",include$")
	  ;; Expand in the default-directory of the tags table buffer.
	  (setq files (cons (expand-file-name (buffer-substring beg (point)))
			    files))))
    (nreverse files)))

;; Empty tags file support.

;; Recognize an empty file and give it local values of the tags table format
;; variables which do nothing.
(defun recognize-empty-tags-table ()
  (and (zerop (buffer-size))
       (mapcar (function (lambda (sym)
			   (set (make-local-variable sym) 'ignore)))
	       '(tags-table-files-function
		 tags-completion-table-function
		 find-tag-regexp-search-function
		 find-tag-search-function
		 tags-apropos-function
		 tags-included-tables-function))
       (set (make-local-variable 'verify-tags-table-function)
	    (function (lambda ()
			(zerop (buffer-size)))))))

;;; Match qualifier functions for tagnames.
;;; XXX these functions assume etags file format.

;; This might be a neat idea, but it's too hairy at the moment.
;;(defmacro tags-with-syntax (&rest body)
;;  (` (let ((current (current-buffer))
;;	   (otable (syntax-table))
;;	   (buffer (find-file-noselect (file-of-tag)))
;;	   table)
;;       (unwind-protect
;;	   (progn
;;	     (set-buffer buffer)
;;	     (setq table (syntax-table))
;;	     (set-buffer current)
;;	     (set-syntax-table table)
;;	     (,@ body))
;;	 (set-syntax-table otable)))))
;;(put 'tags-with-syntax 'edebug-form-spec '(&rest form))

;; t if point is at a tag line that matches TAG "exactly".
;; point should be just after a string that matches TAG.
(defun tag-exact-match-p (tag)
  ;; The match is really exact if there is an explicit tag name.
  (or (looking-at (concat "[^\177]*\177" (regexp-quote tag) "\001"))
      ;; We also call it "exact" if it is surrounded by symbol boundaries.
      ;; This is needed because etags does not always generate explicit names.
      (and (looking-at "\\Sw.*\177") (looking-at "\\S_.*\177")
	   (save-excursion
	     (backward-char (1+ (length tag)))
	     (and (looking-at "\\Sw") (looking-at "\\S_"))))))

;; t if point is at a tag line that matches TAG as a word.
;; point should be just after a string that matches TAG.
(defun tag-word-match-p (tag)
  (and (looking-at "\\b.*\177")
       (save-excursion (backward-char (1+ (length tag)))
		       (looking-at "\\b"))))

;; t if point is in a tag line with a tag containing TAG as a substring.
(defun tag-any-match-p (tag)
  (looking-at ".*\177"))

;; t if point is at a tag line that matches RE as a regexp.
(defun tag-re-match-p (re)
  (save-excursion
    (beginning-of-line)
    (let ((bol (point)))
      (and (search-forward "\177" (save-excursion (end-of-line) (point)) t)
	   (re-search-backward re bol t)))))

;;;###autoload
(defun next-file (&optional initialize novisit)
  "Select next file among files in current tags table.

A first argument of t (prefix arg, if interactive) initializes to the
beginning of the list of files in the tags table.  If the argument is
neither nil nor t, it is evalled to initialize the list of files.

Non-nil second argument NOVISIT means use a temporary buffer
 to save time and avoid uninteresting warnings.

Value is nil if the file was already visited;
if the file was newly read in, the value is the filename."
  (interactive "P")
  (cond ((not initialize)
	 ;; Not the first run.
	 )
	((eq initialize t)
	 ;; Initialize the list from the tags table.
	 (save-excursion
	   ;; Visit the tags table buffer to get its list of files.
	   (visit-tags-table-buffer)
	   (setq next-file-list (tags-table-files))))
	(t
	 ;; Initialize the list by evalling the argument.
	 (setq next-file-list (eval initialize))))
  (or next-file-list
      (save-excursion
	;; Get the files from the next tags table.
	;; When doing (visit-tags-table-buffer t),
	;; the tags table buffer must be current.
	(if (and (visit-tags-table-buffer 'same)
		 (visit-tags-table-buffer t))
	    (setq next-file-list (tags-table-files))
	  (and novisit
	       (get-buffer " *next-file*")
	       (kill-buffer " *next-file*"))
	  (error "All files processed."))))
  (let ((new (not (get-file-buffer (car next-file-list)))))
    (if (not (and new novisit))
	(set-buffer (find-file-noselect (car next-file-list) novisit))
      ;; Like find-file, but avoids random warning messages.
      (set-buffer (get-buffer-create " *next-file*"))
      (kill-all-local-variables)
      (erase-buffer)
      (setq new (car next-file-list))
      (insert-file-contents new nil))
    (setq next-file-list (cdr next-file-list))
    new))

(defvar tags-loop-operate nil
  "Form for `tags-loop-continue' to eval to change one file.")

(defvar tags-loop-scan
  '(error (substitute-command-keys
	   "No \\[tags-search] or \\[tags-query-replace] in progress."))
  "Form for `tags-loop-continue' to eval to scan one file.
If it returns non-nil, this file needs processing by evalling
\`tags-loop-operate'.  Otherwise, move on to the next file.")

;;;###autoload
(defun tags-loop-continue (&optional first-time)
  "Continue last \\[tags-search] or \\[tags-query-replace] command.
Used noninteractively with non-nil argument to begin such a command (the
argument is passed to `next-file', which see).
Two variables control the processing we do on each file:
the value of `tags-loop-scan' is a form to be executed on each file
to see if it is interesting (it returns non-nil if so)
and `tags-loop-operate' is a form to execute to operate on an interesting file
If the latter returns non-nil, we exit; otherwise we scan the next file."
  (interactive)
  (let (new
	(messaged nil))
    (while
	(progn
	  ;; Scan files quickly for the first or next interesting one.
	  (while (or first-time
		     (save-restriction
		       (widen)
		       (not (eval tags-loop-scan))))
	    (setq new (next-file first-time t))
	    ;; If NEW is non-nil, we got a temp buffer,
	    ;; and NEW is the file name.
	    (if (or messaged
		    (and (not first-time)
			 (> baud-rate search-slow-speed)
			 (setq messaged t)))
		(message "Scanning file %s..." (or new buffer-file-name)))
	    (setq first-time nil)
	    (goto-char (point-min)))

	  ;; If we visited it in a temp buffer, visit it now for real.
	  (if new
	      (let ((pos (point)))
		(erase-buffer)
		(set-buffer (find-file-noselect new))
		(widen)
		(goto-char pos)))

	  (switch-to-buffer (current-buffer))

	  ;; Now operate on the file.
	  ;; If value is non-nil, continue to scan the next file.
	  (eval tags-loop-operate)))
    (and messaged
	 (null tags-loop-operate)
	 (message "Scanning file %s...found" buffer-file-name))))
;;;###autoload (define-key esc-map "," 'tags-loop-continue)

;;;###autoload
(defun tags-search (regexp &optional file-list-form)
  "Search through all files listed in tags table for match for REGEXP.
Stops when a match is found.
To continue searching for next match, use command \\[tags-loop-continue].

See documentation of variable `tags-file-name'."
  (interactive "sTags search (regexp): ")
  (if (and (equal regexp "")
	   (eq (car tags-loop-scan) 're-search-forward)
	   (null tags-loop-operate))
      ;; Continue last tags-search as if by M-,.
      (tags-loop-continue nil)
    (setq tags-loop-scan
	  (list 're-search-forward regexp nil t)
	  tags-loop-operate nil)
    (tags-loop-continue (or file-list-form t))))

;;;###autoload
(defun tags-query-replace (from to &optional delimited file-list-form)
  "Query-replace-regexp FROM with TO through all files listed in tags table.
Third arg DELIMITED (prefix arg) means replace only word-delimited matches.
If you exit (\\[keyboard-quit] or ESC), you can resume the query-replace
with the command \\[tags-loop-continue].

See documentation of variable `tags-file-name'."
  (interactive
   "sTags query replace (regexp): \nsTags query replace %s by: \nP")
  (setq tags-loop-scan (list 'prog1
			     (list 'if (list 're-search-forward from nil t)
				   ;; When we find a match, move back
				   ;; to the beginning of it so perform-replace
				   ;; will see it.
				   '(goto-char (match-beginning 0))))
	tags-loop-operate (list 'perform-replace from to t t delimited))
  (tags-loop-continue (or file-list-form t)))

;;;###autoload
(defun list-tags (file)
  "Display list of tags in file FILE.
FILE should not contain a directory specification."
  (interactive (list (completing-read "List tags in file: "
				      (save-excursion
					(visit-tags-table-buffer)
					(mapcar 'list
						(mapcar 'file-name-nondirectory
							(tags-table-files))))
				      nil t nil)))
  (with-output-to-temp-buffer "*Tags List*"
    (princ "Tags in file ")
    (princ file)
    (terpri)
    (save-excursion
      (let ((first-time t)
	    (gotany nil))
	(while (visit-tags-table-buffer (not first-time))
	  (setq first-time nil)
	  (if (funcall list-tags-function file)
	      (setq gotany t)))
	(or gotany
	    (error "File %s not in current tags tables" file))))))

;;;###autoload
(defun tags-apropos (regexp)
  "Display list of all tags in tags table REGEXP matches."
  (interactive "sTags apropos (regexp): ")
  (with-output-to-temp-buffer "*Tags List*"
    (princ "Tags matching regexp ")
    (prin1 regexp)
    (terpri)
    (save-excursion
      (let ((first-time t))
	(while (visit-tags-table-buffer (not first-time))
	  (setq first-time nil)
	  (funcall tags-apropos-function regexp))))))

;;; XXX Kludge interface.

;; XXX If a file is in multiple tables, selection may get the wrong one.
;;;###autoload
(defun select-tags-table ()
  "Select a tags table file from a menu of those you have already used.
The list of tags tables to select from is stored in `tags-table-file-list';
see the doc of that variable if you want to add names to the list."
  (interactive)
  (pop-to-buffer "*Tags Table List*")
  (setq buffer-read-only nil)
  (erase-buffer)
  (let ((set-list tags-table-set-list)
	(desired-point nil))
    (if tags-table-list
	(progn
	  (setq desired-point (point-marker))
	  (princ tags-table-list (current-buffer))
	  (insert "\C-m")
	  (prin1 (car tags-table-list) (current-buffer)) ;invisible
	  (insert "\n")))
    (while set-list
      (if (eq (car set-list) tags-table-list)
	  ;; Already printed it.
	  ()
	(princ (car set-list) (current-buffer))
	(insert "\C-m")
	(prin1 (car (car set-list)) (current-buffer)) ;invisible
	(insert "\n"))
      (setq set-list (cdr set-list)))
    (if tags-file-name
	(progn
	  (or desired-point
	      (setq desired-point (point-marker)))
	  (insert tags-file-name "\C-m")
	  (prin1 tags-file-name (current-buffer)) ;invisible
	  (insert "\n")))
    (setq set-list (delete tags-file-name
			   (apply 'nconc (cons tags-table-list
					       (mapcar 'copy-sequence
						       tags-table-set-list)))))
    (while set-list
      (insert (car set-list) "\C-m")
      (prin1 (car set-list) (current-buffer)) ;invisible
      (insert "\n")
      (setq set-list (delete (car set-list) set-list)))
    (goto-char 1)
    (insert-before-markers
     "Type `t' to select a tags table or set of tags tables:\n\n")
    (if desired-point
	(goto-char desired-point))
    (set-window-start (selected-window) 1 t))
  (set-buffer-modified-p nil)
  (select-tags-table-mode))

(defvar select-tags-table-mode-map)
(let ((map (make-sparse-keymap)))
  (define-key map "t" 'select-tags-table-select)
  (define-key map " " 'next-line)
  (define-key map "\^?" 'previous-line)
  (define-key map "n" 'next-line)
  (define-key map "p" 'previous-line)
  (define-key map "q" 'select-tags-table-quit)
  (setq select-tags-table-mode-map map))

(defun select-tags-table-mode ()
  "Major mode for choosing a current tags table among those already loaded.

\\{select-tags-table-mode-map}"
  (interactive)
  (kill-all-local-variables)
  (setq buffer-read-only t
	major-mode 'select-tags-table-mode
	mode-name "Select Tags Table")
  (use-local-map select-tags-table-mode-map)
  (setq selective-display t
	selective-display-ellipses nil))
  
(defun select-tags-table-select ()
  "Select the tags table named on this line."
  (interactive)
  (search-forward "\C-m")
  (let ((name (read (current-buffer))))
    (visit-tags-table name)
    (select-tags-table-quit)
    (message "Tags table now %s" name)))

(defun select-tags-table-quit ()
  "Kill the buffer and delete the selected window."
  (interactive)
  (kill-buffer (current-buffer))
  (or (one-window-p)
      (delete-window)))  

;;;###autoload
(defun complete-tag ()
  "Perform tags completion on the text around point.
Completes to the set of names listed in the current tags table.  
The string to complete is chosen in the same way as the default
for \\[find-tag] (which see)."
  (interactive)
  (or tags-table-list
      tags-file-name
      (error (substitute-command-keys
	      "No tags table loaded.  Try \\[visit-tags-table].")))
  (let ((pattern (funcall (or find-tag-default-function
			      (get major-mode 'find-tag-default-function)
			      'find-tag-default)))
	beg
	completion)
    (or pattern
	(error "Nothing to complete"))
    (search-backward pattern)
    (setq beg (point))
    (forward-char (length pattern))
    (setq completion (try-completion pattern 'tags-complete-tag nil))
    (cond ((eq completion t))
	  ((null completion)
	   (message "Can't find completion for \"%s\"" pattern)
	   (ding))
	  ((not (string= pattern completion))
	   (delete-region beg (point))
	   (insert completion))
	  (t
	   (message "Making completion list...")
	   (with-output-to-temp-buffer " *Completions*"
	     (display-completion-list
	      (all-completions pattern 'tags-complete-tag nil)))
	   (message "Making completion list...%s" "done")))))

;;;###autoload (define-key esc-map "\t" 'complete-tag)

(provide 'etags)

;;; etags.el ends here
