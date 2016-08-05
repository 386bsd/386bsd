;;; prolog.el --- major mode for editing and running Prolog under Emacs

;; Copyright (C) 1986, 1987 Free Software Foundation, Inc.

;; Author: Masanobu UMEDA <umerin@flab.flab.fujitsu.junet>
;; Keywords: languages

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

;; This package provides a major mode for editing Prolog.  It knows
;; about Prolog syntax and comments, and can send regions to an inferior
;; Prolog interpreter process.

;;; Code:

(defvar prolog-mode-syntax-table nil)
(defvar prolog-mode-abbrev-table nil)
(defvar prolog-mode-map nil)
  
(defvar prolog-program-name "prolog"
  "*Program name for invoking an inferior Prolog with `run-prolog'.")

(defvar prolog-consult-string "reconsult(user).\n"
  "*(Re)Consult mode (for C-Prolog and Quintus Prolog). ")

(defvar prolog-compile-string "compile(user).\n"
  "*Compile mode (for Quintus Prolog).")

(defvar prolog-eof-string "end_of_file.\n"
  "*String that represents end of file for prolog.
nil means send actual operating system end of file.")

(defvar prolog-indent-width 4)

(if prolog-mode-syntax-table
    ()
  (let ((table (make-syntax-table)))
    (modify-syntax-entry ?_ "w" table)
    (modify-syntax-entry ?\\ "\\" table)
    (modify-syntax-entry ?/ "." table)
    (modify-syntax-entry ?* "." table)
    (modify-syntax-entry ?+ "." table)
    (modify-syntax-entry ?- "." table)
    (modify-syntax-entry ?= "." table)
    (modify-syntax-entry ?% "<" table)
    (modify-syntax-entry ?< "." table)
    (modify-syntax-entry ?> "." table)
    (modify-syntax-entry ?\' "\"" table)
    (setq prolog-mode-syntax-table table)))

(define-abbrev-table 'prolog-mode-abbrev-table ())

(defun prolog-mode-variables ()
  (set-syntax-table prolog-mode-syntax-table)
  (setq local-abbrev-table prolog-mode-abbrev-table)
  (make-local-variable 'paragraph-start)
  (setq paragraph-start (concat "^%%\\|^$\\|" page-delimiter)) ;'%%..'
  (make-local-variable 'paragraph-separate)
  (setq paragraph-separate paragraph-start)
  (make-local-variable 'paragraph-ignore-fill-prefix)
  (setq paragraph-ignore-fill-prefix t)
  (make-local-variable 'indent-line-function)
  (setq indent-line-function 'prolog-indent-line)
  (make-local-variable 'comment-start)
  (setq comment-start "%")
  (make-local-variable 'comment-start-skip)
  (setq comment-start-skip "%+ *")
  (make-local-variable 'comment-column)
  (setq comment-column 48)
  (make-local-variable 'comment-indent-function)
  (setq comment-indent-function 'prolog-comment-indent))

(defun prolog-mode-commands (map)
  (define-key map "\t" 'prolog-indent-line)
  (define-key map "\e\C-x" 'prolog-consult-region))

(if prolog-mode-map
    nil
  (setq prolog-mode-map (make-sparse-keymap))
  (prolog-mode-commands prolog-mode-map))

;;;###autoload
(defun prolog-mode ()
  "Major mode for editing Prolog code for Prologs.
Blank lines and `%%...' separate paragraphs.  `%'s start comments.
Commands:
\\{prolog-mode-map}
Entry to this mode calls the value of `prolog-mode-hook'
if that value is non-nil."
  (interactive)
  (kill-all-local-variables)
  (use-local-map prolog-mode-map)
  (setq major-mode 'prolog-mode)
  (setq mode-name "Prolog")
  (prolog-mode-variables)
  (run-hooks 'prolog-mode-hook))

(defun prolog-indent-line (&optional whole-exp)
  "Indent current line as Prolog code.
With argument, indent any additional lines of the same clause
rigidly along with this one (not yet)."
  (interactive "p")
  (let ((indent (prolog-indent-level))
	(pos (- (point-max) (point))) beg)
    (beginning-of-line)
    (setq beg (point))
    (skip-chars-forward " \t")
    (if (zerop (- indent (current-column)))
	nil
      (delete-region beg (point))
      (indent-to indent))
    (if (> (- (point-max) pos) (point))
	(goto-char (- (point-max) pos)))
    ))

(defun prolog-indent-level ()
  "Compute prolog indentation level."
  (save-excursion
    (beginning-of-line)
    (skip-chars-forward " \t")
    (cond
     ((looking-at "%%%") 0)		;Large comment starts
     ((looking-at "%[^%]") comment-column) ;Small comment starts
     ((bobp) 0)				;Beginning of buffer
     (t
      (let ((empty t) ind more less)
	(if (looking-at ")")
	    (setq less t)		;Find close
	  (setq less nil))
	;; See previous indentation
	(while empty
	  (forward-line -1)
	  (beginning-of-line)
 	  (if (bobp)
 	      (setq empty nil)
 	    (skip-chars-forward " \t")
 	    (if (not (or (looking-at "%[^%]") (looking-at "\n")))
 		(setq empty nil))))
 	(if (bobp)
 	    (setq ind 0)		;Beginning of buffer
	  (setq ind (current-column)))	;Beginning of clause
	;; See its beginning
	(if (looking-at "%%[^%]")
	    ind
	  ;; Real prolog code
	  (if (looking-at "(")
	      (setq more t)		;Find open
	    (setq more nil))
	  ;; See its tail
	  (end-of-prolog-clause)
	  (or (bobp) (forward-char -1))
	  (cond ((looking-at "[,(;>]")
		 (if (and more (looking-at "[^,]"))
		     (+ ind prolog-indent-width) ;More indentation
		   (max tab-width ind))) ;Same indentation
		((looking-at "-") tab-width) ;TAB
		((or less (looking-at "[^.]"))
		 (max (- ind prolog-indent-width) 0)) ;Less indentation
		(t 0))			;No indentation
	  )))
     )))

(defun end-of-prolog-clause ()
  "Go to end of clause in this line."
  (beginning-of-line 1)
  (let* ((eolpos (save-excursion (end-of-line) (point))))
    (if (re-search-forward comment-start-skip eolpos 'move)
	(goto-char (match-beginning 0)))
    (skip-chars-backward " \t")))

(defun prolog-comment-indent ()
  "Compute prolog comment indentation."
  (cond ((looking-at "%%%") 0)
	((looking-at "%%") (prolog-indent-level))
	(t
	 (save-excursion
	       (skip-chars-backward " \t")
	       ;; Insert one space at least, except at left margin.
	       (max (+ (current-column) (if (bolp) 0 1))
		    comment-column)))
	))


;;;
;;; Inferior prolog mode
;;;
(defvar inferior-prolog-mode-map nil)

(defun inferior-prolog-mode ()
  "Major mode for interacting with an inferior Prolog process.

The following commands are available:
\\{inferior-prolog-mode-map}

Entry to this mode calls the value of `prolog-mode-hook' with no arguments,
if that value is non-nil.  Likewise with the value of `comint-mode-hook'.
`prolog-mode-hook' is called after `comint-mode-hook'.

You can send text to the inferior Prolog from other buffers
using the commands `send-region', `send-string' and \\[prolog-consult-region].

Commands:
Tab indents for Prolog; with argument, shifts rest
 of expression rigidly with the current line.
Paragraphs are separated only by blank lines and '%%'.
'%'s start comments.

Return at end of buffer sends line as input.
Return not at end copies rest of line to end and sends it.
\\[comint-kill-input] and \\[backward-kill-word] are kill commands, imitating normal Unix input editing.
\\[comint-interrupt-subjob] interrupts the shell or its current subjob if any.
\\[comint-stop-subjob] stops. \\[comint-quit-subjob] sends quit signal."
  (interactive)
  (require 'comint)
  (comint-mode)
  (setq major-mode 'inferior-prolog-mode
	mode-name "Inferior Prolog"
	comint-prompt-regexp "^| [ ?][- ] *")
  (prolog-mode-variables)
  (if inferior-prolog-mode-map nil
    (setq inferior-prolog-mode-map (copy-keymap comint-mode-map))
    (prolog-mode-commands inferior-prolog-mode-map))
  (use-local-map inferior-prolog-mode-map)
  (run-hooks 'prolog-mode-hook))

;;;###autoload
(defun run-prolog ()
  "Run an inferior Prolog process, input and output via buffer *prolog*."
  (interactive)
  (require 'comint)
  (switch-to-buffer (make-comint "prolog" prolog-program-name))
  (inferior-prolog-mode))

(defun prolog-consult-region (compile beg end)
  "Send the region to the Prolog process made by \"M-x run-prolog\".
If COMPILE (prefix arg) is not nil, use compile mode rather than consult mode."
  (interactive "P\nr")
  (save-excursion
    (if compile
	(send-string "prolog" prolog-compile-string)
      (send-string "prolog" prolog-consult-string))
    (send-region "prolog" beg end)
    (send-string "prolog" "\n")		;May be unnecessary
    (if prolog-eof-string
	(send-string "prolog" prolog-eof-string)
      (process-send-eof "prolog")))) ;Send eof to prolog process.

(defun prolog-consult-region-and-go (compile beg end)
  "Send the region to the inferior Prolog, and switch to *prolog* buffer.
If COMPILE (prefix arg) is not nil, use compile mode rather than consult mode."
  (interactive "P\nr")
  (prolog-consult-region compile beg end)
  (switch-to-buffer "*prolog*"))

;;; prolog.el ends here
