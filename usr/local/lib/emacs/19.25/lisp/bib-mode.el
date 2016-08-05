;;; bib-mode.el --- bib-mode, major mode for editing bib files.

;; Copyright (C) 1989 Free Software Foundation, Inc.

;; Maintainer: FSF
;; Keywords: bib

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

;;; Commentary:

;;   GNU Emacs code to help maintain databases compatible with (troff)
;;   refer and lookbib.  The file bib-file should be set to your 
;;   bibliography file.  Keys are automagically inserted as you type,
;;   and appropriate keys are presented for various kinds of entries.

;;; Code:

(defvar bib-file "~/my-bibliography.bib" 
   "Default name of file used by `addbib'.")

(defvar unread-bib-file "~/to-be-read.bib"
   "Default name of file used by `unread-bib' in Bib mode.")

(defvar bib-mode-map (copy-keymap text-mode-map))
(define-key bib-mode-map "\C-M" 'return-key-bib)
(define-key bib-mode-map "\C-c\C-u" 'unread-bib)
(define-key bib-mode-map "\C-c\C-@" 'mark-bib)
(define-key bib-mode-map "\e`" 'abbrev-mode)
(defvar bib-mode-abbrev-table nil
   "Abbrev table used in Bib mode")

(defun addbib ()
   "Set up editor to add to troff bibliography file specified 
by global variable `bib-file'.  See description of `bib-mode'."
   (interactive)
   (find-file bib-file)
   (goto-char (point-max))
   (bib-mode)
   )
   
(defun bib-mode ()
   "Mode for editing `lookbib' style bibliographies.  
Hit RETURN to get next % field key.
If you want to ignore this field, just hit RETURN again.
Use `text-mode' to turn this feature off.

 journal papers:                    A* T D J V N P K W X
 articles in books & proceedings:   A* T D B E* I C P K W X 
 tech reports:                      A* T D R I C K W X
 books:                             A* T D I C K W X

Fields:

A uthor		T itle		D ate  		J ournal
V olume		N umber		P age		K eywords
B in book or proceedings	E ditor		C ity & state
I nstitution, school, or publisher
R eport number or 'phd thesis' or 'masters thesis' or 'draft' or 
     'unnumbered' or 'unpublished'
W here can be found locally (login name, or ailib, etc.)
X comments (not used in indexing)

\\[unread-bib] appends current entry to a different file (for example,
a file of papers to be read in the future), given by the value of the
variable `unread-bib-file'.
\\[mark-bib] marks current or previous entry.
Abbreviations are saved in `bib-mode-abbrev-table'.
Hook can be stored in `bib-mode-hook'.
Field keys given by variable `bib-assoc'.

Commands:
\\{bib-mode-map}
"
   (interactive)
   (text-mode)
   (use-local-map bib-mode-map)
   (setq mode-name "Bib")
   (setq major-mode 'bib-mode)
   (define-abbrev-table 'bib-mode-abbrev-table ())
   (setq local-abbrev-table bib-mode-abbrev-table)
   (abbrev-mode 1)
   (run-hooks 'bib-mode-hook)
   )

(defconst bib-assoc '(
		   (" *$" . "%A ")
		   ("%A ." . "%A ")
		   ("%A $" . "%T ")
		   ("%T " . "%D ")
		   ("%D " . "%J ")
		   ("%J ." . "%V ")
		   ("%V " . "%N ")
		   ("%N " . "%P ")
		   ("%P " . "%K ")
		   ("%K " . "%W ")
		   ("%W " . "%X ")
		   ("%X " . "")
		   ("%J $" . "%B ")
		   ("%B ." . "%E ")
		   ("%E ." . "%E ")
		   ("%E $" . "%I ")
		   ("%I " . "%C ")
		   ("%C " . "%P ")
		   ("%B $" . "%R ")
		   ("%R " . "%I ")
		   )
		   
"Describes bibliographic database format.  A line beginning with
the car of an entry is followed by one beginning with the cdr.
")

(defun bib-find-key (slots)
   (cond
      ((null slots)
	 (if (bobp)
	    ""
	    (progn (previous-line 1) (bib-find-key bib-assoc))))
      ((looking-at (car (car slots)))
	 (cdr (car slots)))
      (t (bib-find-key (cdr slots)))
      ))


(defvar bib-auto-capitalize t 
"*True to automatically capitalize appropriate fields in Bib mode.")

(defconst bib-capitalized-fields "%[AETCBIJR]")

(defun return-key-bib ()
  "Magic when user hits return, used by `bib-mode'."
  (interactive)
  (if (eolp)
    (let (empty new-key beg-current end-current)
      (beginning-of-line)
      (setq empty (looking-at "%. $"))
      (if (not empty)
	(progn
	  (end-of-line)
	  (newline)
	  (forward-line -1)
	  ))
      (end-of-line)
      (setq end-current (point))
      (beginning-of-line)
      (setq beg-current (point))
      (setq new-key (bib-find-key bib-assoc))
      (if (and (not empty) bib-auto-capitalize
	    (looking-at bib-capitalized-fields))
	(save-excursion
	  (capitalize-title-region (+ (point) 3) end-current)))
      (goto-char beg-current)
      (if empty
	(kill-line nil)
	(forward-line 1)
	)
      (insert-string new-key))
    (newline)))

(defun mark-bib ()
   "Set mark at beginning of current or previous bib entry, point at end."
   (interactive)
   (beginning-of-line nil)
   (if (looking-at "^ *$") (re-search-backward "[^ \n]" nil 2))
   (re-search-backward "^ *$" nil 2)
   (re-search-forward "^%")
   (beginning-of-line nil)
   (push-mark (point))
   (re-search-forward "^ *$" nil 2)
   (next-line 1)
   (beginning-of-line nil))

(defun unread-bib ()
   "Append current or previous entry to file of unread papers
named by variable `unread-bib-file'."
   (interactive)
   (mark-bib)
   (if (get-file-buffer unread-bib-file)
      (append-to-buffer (get-file-buffer unread-bib-file) (mark) (point))
      (append-to-file (mark) (point) unread-bib-file)))


(defvar capitalize-title-stop-words
   (concat
      "the\\|and\\|of\\|is\\|a\\|an\\|of\\|for\\|in\\|to\\|in\\|on\\|at\\|"
      "by\\|with\\|that\\|its")
   "Words not to be capitialized in a title (unless they're the first word
in the title).")

(defvar capitalize-title-stop-regexp
   (concat "\\(" capitalize-title-stop-words "\\)\\(\\b\\|'\\)"))

(defun capitalize-title-region (begin end)
   "Like `capitalize-region', but don't capitalize stop words, except the first."
   (interactive "r")
   (let ((case-fold-search nil) (orig-syntax-table (syntax-table)))
      (unwind-protect
	 (save-restriction
	    (set-syntax-table text-mode-syntax-table)
	    (narrow-to-region begin end)
	    (goto-char (point-min))
	    (if (looking-at "[A-Z][a-z]*[A-Z]")
	       (forward-word 1)
	       (capitalize-word 1))
	    (while (re-search-forward "\\<" nil t)
	       (if (looking-at "[A-Z][a-z]*[A-Z]")
		  (forward-word 1)
		  (if (let ((case-fold-search t))
			 (looking-at capitalize-title-stop-regexp))
		     (downcase-word 1)
		     (capitalize-word 1)))
	       ))
	 (set-syntax-table orig-syntax-table))))


(defun capitalize-title (s)
   "Like `capitalize', but don't capitalize stop words, except the first."
   (save-excursion
      (set-buffer (get-buffer-create "$$$Scratch$$$"))
      (erase-buffer)
      (insert s)
      (capitalize-title-region (point-min) (point-max))
      (buffer-string)))

(provide 'bib-mode)

;;; bib-mode.el ends here
