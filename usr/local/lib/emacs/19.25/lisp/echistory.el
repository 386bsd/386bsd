;;; echistory.el --- Electric Command History Mode

;; Copyright (C) 1985 Free Software Foundation, Inc.

;; Author: K. Shane Hartman
;; Maintainer: FSF

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

(require 'electric)			; command loop
(require 'chistory)			; history lister

;;;###autoload
(defun Electric-command-history-redo-expression (&optional noconfirm)
  "Edit current history line in minibuffer and execute result.
With prefix arg NOCONFIRM, execute current line as-is without editing."
  (interactive "P")
  (let (todo)
    (save-excursion
      (set-buffer "*Command History*")
      (beginning-of-line)
      (setq todo (read (current-buffer)))
      (if (boundp 'electric-history-in-progress)
	  (if todo (throw 'electric-history-quit (list noconfirm todo)))))))

(defvar electric-history-map ())
(if electric-history-map
    ()
  (setq electric-history-map (make-sparse-keymap))
  (define-key electric-history-map [t] 'Electric-history-undefined)
  (define-key electric-history-map "\e" (make-sparse-keymap))
  (define-key electric-history-map [?\e t] 'Electric-history-undefined)
  (define-key electric-history-map "\C-u" 'universal-argument)
  (define-key electric-history-map " " 'Electric-command-history-redo-expression)
  (define-key electric-history-map "!" 'Electric-command-history-redo-expression)
  (define-key electric-history-map "\e\C-x" 'eval-sexp)
  (define-key electric-history-map "\e\C-d" 'down-list)
  (define-key electric-history-map "\e\C-u" 'backward-up-list)
  (define-key electric-history-map "\e\C-b" 'backward-sexp)
  (define-key electric-history-map "\e\C-f" 'forward-sexp)
  (define-key electric-history-map "\e\C-a" 'beginning-of-defun)
  (define-key electric-history-map "\e\C-e" 'end-of-defun)
  (define-key electric-history-map "\e\C-n" 'forward-list)
  (define-key electric-history-map "\e\C-p" 'backward-list)
  (define-key electric-history-map "q" 'Electric-history-quit)
  (define-key electric-history-map "\C-c" nil)
  (define-key electric-history-map "\C-c\C-c" 'Electric-history-quit)
  (define-key electric-history-map "\C-]" 'Electric-history-quit)
  (define-key electric-history-map "\C-z" 'suspend-emacs)
  (define-key electric-history-map (char-to-string help-char) 'Helper-help)
  (define-key electric-history-map "?" 'Helper-describe-bindings)
  (define-key electric-history-map "\e>" 'end-of-buffer)
  (define-key electric-history-map "\e<" 'beginning-of-buffer)
  (define-key electric-history-map "\n" 'next-line)
  (define-key electric-history-map "\r" 'next-line)
  (define-key electric-history-map "\177" 'previous-line)  
  (define-key electric-history-map "\C-n" 'next-line)
  (define-key electric-history-map "\C-p" 'previous-line)
  (define-key electric-history-map "\ev" 'scroll-down)
  (define-key electric-history-map "\C-v" 'scroll-up)
  (define-key electric-history-map [home] 'beginning-of-buffer)
  (define-key electric-history-map [down] 'next-line)
  (define-key electric-history-map [up] 'previous-line)
  (define-key electric-history-map [prior] 'scroll-down)
  (define-key electric-history-map [next] 'scroll-up)
  (define-key electric-history-map "\C-l" 'recenter)
  (define-key electric-history-map "\e\C-v" 'scroll-other-window))

(defvar electric-command-history-hook nil
  "If non-nil, its value is called by `electric-command-history'.")

(defun electric-command-history ()
  "\\<electric-history-map>Major mode for examining and redoing commands from `command-history'.
This pops up a window with the Command History listing.
The number of command listed is controlled by `list-command-history-max'.
The command history is filtered by `list-command-history-filter' if non-nil.
Combines typeout Command History list window with menu like selection
of an expression from the history for re-evaluation in the *original* buffer.

The history displayed is filtered by `list-command-history-filter' if non-nil.

Like Emacs-Lisp mode except that characters do not insert themselves and
Tab and Linefeed do not indent.  Instead these commands are provided:
\\{electric-history-map}

Calls the value of `electric-command-history-hook' if that is non-nil.
The Command History listing is recomputed each time this mode is invoked."
  (interactive)
  (let ((electric-history-in-progress t)
	(old-buffer (current-buffer))
	(todo))
    (unwind-protect
	(setq todo
	      (catch 'electric-history-quit
		(save-window-excursion
		  (save-window-excursion
		    (list-command-history)
		    (set-buffer "*Command History*")
		    (Command-history-setup 'electric-command-history
					   "Electric History"
					   electric-history-map))
		  (Electric-pop-up-window "*Command History*")
		  (run-hooks 'electric-command-history-hook)
		  (if (eobp)
		      (progn (ding)
			     (message "No command history.")
			     (throw 'electric-history-quit nil))
		    (let ((Helper-return-blurb "return to History"))
		      (Electric-command-loop 'electric-history-quit
					     "->" t))))))
      (set-buffer "*Command History*")
      (Command-history-setup)
      (bury-buffer (current-buffer)))
    (if (consp todo)
	(progn (set-buffer old-buffer)
	       (if (car todo)
		   (apply (car (car (cdr todo))) (cdr (car (cdr todo))))
		 (edit-and-eval-command "Redo: " (car (cdr todo))))))))

(defun Electric-history-undefined ()
  (interactive)
  (ding)
  (message (substitute-command-keys "Type \\[Helper-help] for help, ? for commands, C-c C-c to quit, Space to execute"))
  (sit-for 4))

(defun Electric-history-quit ()
  "Quit Electric Command History, restoring previous window configuration."
  (interactive)
  (if (boundp 'electric-history-in-progress)
      (progn (message "")
	     (throw 'electric-history-quit nil))))

;;; echistory.el ends here
