;; tcl-mode.el - A major-mode for editing tcl/tk scripts
;;
;; Author: Gregor Schmid <schmid@fb3-s7.math.tu-berlin.de>
;; Keywords: languages, processes, tools
;;
;; Copyright (C) 1993, 1994 Free Software Foundation, Inc.
;; Version 1.1

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

;; Please send bug-reports, suggestions etc. to
;;
;; 		schmid@fb3-s7.math.tu-berlin.de
;;

;; This file was written with emacs using Jamie Lokier's folding mode
;; That's what the funny ;;{{{ marks are there for

;;{{{ Usage

;;; Commentary:

;; Tcl-mode supports c-mode style formatting and sending of
;; lines/regions/files to a tcl interpreter. An interpreter (see
;; variable `tcl-default-application') will be started if you try to
;; send some code and none is running. You can use the process-buffer
;; (named after the application you chose) as if it were an
;; interactive shell. See the documentation for `comint.el' for
;; details.

;; Another version of this package which has support for other Emacs
;; versions is in the LCD archive.

;;}}}
;;{{{ Key-bindings

;; To see all the keybindings for folding mode, look at `tcl-setup-keymap'
;; or start `tcl-mode' and type `\C-h m'.
;; The keybindings may seem strange, since I prefer to use them with
;; tcl-prefix-key set to nil, but since those keybindings are already used
;; the default for `tcl-prefix-key' is `\C-c', which is the conventional
;; prefix for major-mode commands.

;; You can customise the keybindings either by setting `tcl-prefix-key'
;; or by putting the following in your .emacs
;; 	(setq tcl-mode-map (make-sparse-keymap))
;; and
;; 	(define-key tcl-mode-map <your-key> <function>)
;; for all the functions you need.

;;}}}
;;{{{ Variables

;; You may want to customize the following variables:
;; 	tcl-indent-level
;; 	tcl-always-show
;;	tcl-mode-map
;;	tcl-prefix-key
;;	tcl-mode-hook
;; 	tcl-default-application
;; 	tcl-default-command-switches

;;}}}

;;; Code:

;; We need that !
(require 'comint)

;;{{{ variables

(defvar tcl-default-application "wish"
  "Default tcl/tk application to run in tcl subprocess.")

(defvar tcl-default-command-switches nil
  "Command switches for `tcl-default-application'.
Should be a list of strings.")

(defvar tcl-process nil
  "The active tcl subprocess corresponding to current buffer.")

(defvar tcl-process-buffer nil
  "Buffer used for communication with tcl subprocess for current buffer.")

(defvar tcl-always-show t
  "*Non-nil means display tcl-process-buffer after sending a command.")

(defvar tcl-mode-map nil
  "Keymap used with tcl mode.")

(defvar tcl-prefix-key "\C-c"
  "Prefix for all tcl-mode commands.")

(defvar tcl-mode-hook nil
  "Hooks called when tcl mode fires up.")

(defvar tcl-region-start (make-marker)
  "Start of special region for tcl communication.")

(defvar tcl-region-end (make-marker)
  "End of special region for tcl communication.")

(defvar tcl-indent-level 4
  "Amount by which tcl subexpressions are indented.")

(defvar tcl-default-eval "eval"
  "Default command used when sending regions.")

(defvar tcl-mode-menu (make-sparse-keymap "Tcl-Mode")
  "Keymap for tcl-mode's menu.")

;;}}}
;;{{{ tcl-mode

;;;###autoload
(defun tcl-mode ()
  "Major mode for editing tcl scripts.
The following keys are bound:
\\{tcl-mode-map}
"
  (interactive)
  (let ((switches nil)
	s)
    (kill-all-local-variables)
    (setq major-mode 'tcl-mode)
    (setq mode-name "TCL")
    (set (make-local-variable 'tcl-process) nil)
    (set (make-local-variable 'tcl-process-buffer) nil)
    (make-local-variable 'tcl-default-command-switches)
    (set (make-local-variable 'indent-line-function) 'tcl-indent-line)
    (set (make-local-variable 'comment-start) "#")
    (set (make-local-variable 'comment-start-skip) "\\(\\(^\\|;\\)[ \t]*\\)#")
    (make-local-variable 'tcl-default-eval)
    (or tcl-mode-map
	(tcl-setup-keymap))
    (use-local-map tcl-mode-map)
    (set-syntax-table (copy-syntax-table))
    (modify-syntax-entry ?# "<")
    (modify-syntax-entry ?\n ">")
    ;; look for a #!.../wish -f line at bob
    (save-excursion
      (goto-char (point-min))
      (if (looking-at "#![ \t]*\\([^ \t]*\\)[ \t]\\(.*[ \t]\\)*-f")
	  (progn
	    (set (make-local-variable 'tcl-default-application)
		 (buffer-substring (match-beginning 1)
				   (match-end 1)))
	    (if (match-beginning 2)
		(progn
		  (goto-char (match-beginning 2))
		  (set (make-local-variable 'tcl-default-command-switches) nil)
		  (while (< (point) (match-end 2))
		    (setq s (read (current-buffer)))
		    (if (<= (point) (match-end 2))
			(setq tcl-default-command-switches
			      (append tcl-default-command-switches
				      (list (prin1-to-string s)))))))))
	;; if this fails, look for the #!/bin/csh ... exec hack
	(while (eq (following-char) ?#)
	  (forward-line 1))
	(or (bobp)
	    (forward-char -1))
	(if (eq (preceding-char) ?\\)
	    (progn
	      (forward-char 1)
	      (if (looking-at "exec[ \t]+\\([^ \t]*\\)[ \t]\\(.*[ \t]\\)*-f")
		  (progn
		    (set (make-local-variable 'tcl-default-application)
			 (buffer-substring (match-beginning 1)
					   (match-end 1)))
		    (if (match-beginning 2)
			(progn
			  (goto-char (match-beginning 2))
			  (set (make-local-variable
				'tcl-default-command-switches)
			       nil)
			  (while (< (point) (match-end 2))
			    (setq s (read (current-buffer)))
			    (if (<= (point) (match-end 2))
				(setq tcl-default-command-switches
				      (append tcl-default-command-switches
					      (list (prin1-to-string s)))))))))
		)))))
    (run-hooks 'tcl-mode-hook)))

;;}}}
;;{{{ tcl-setup-keymap

(defun tcl-setup-keymap ()
  "Set up keymap for tcl mode.
If the variable `tcl-prefix-key' is nil, the bindings go directly
to `tcl-mode-map', otherwise they are prefixed with `tcl-prefix-key'."
  (setq tcl-mode-map (make-sparse-keymap))
  (define-key tcl-mode-map [menu-bar tcl-mode]
    (cons "Tcl-Mode" tcl-mode-menu))
  (let ((map (if tcl-prefix-key
		 (make-sparse-keymap)
	       tcl-mode-map)))
  ;; indentation
  (define-key tcl-mode-map [?}] 'tcl-electric-brace)
  ;; communication
  (define-key map "\M-e" 'tcl-send-current-line)
  (define-key map "\M-r" 'tcl-send-region)
  (define-key map "\M-w" 'tcl-send-proc)
  (define-key map "\M-a" 'tcl-send-buffer)
  (define-key map "\M-q" 'tcl-kill-process)
  (define-key map "\M-u" 'tcl-restart-with-whole-file)
  (define-key map "\M-s" 'tcl-show-process-buffer)
  (define-key map "\M-h" 'tcl-hide-process-buffer)
  (define-key map "\M-i" 'tcl-get-error-info)
  (define-key map "\M-[" 'tcl-beginning-of-proc)
  (define-key map "\M-]" 'tcl-end-of-proc)
  (define-key map "\C-\M-s" 'tcl-set-tcl-region-start)
  (define-key map "\C-\M-e" 'tcl-set-tcl-region-end)
  (define-key map "\C-\M-r" 'tcl-send-tcl-region)
  (if tcl-prefix-key
      (define-key tcl-mode-map tcl-prefix-key map))
  ))

;;}}}
;;{{{ indentation

;;{{{ tcl-indent-line

(defun tcl-indent-line ()
  "Indent current line as tcl code.
Return the amount the indentation changed by."
  (let ((indent (tcl-calculate-indentation nil))
	beg shift-amt
	(case-fold-search nil)
	(pos (- (point-max) (point))))
    (beginning-of-line)
    (setq beg (point))
    (skip-chars-forward " \t")
    (save-excursion
      (while (eq (following-char) ?})
	(setq indent (max (- indent tcl-indent-level) 0))
	(forward-char 1)
	(if (looking-at "\\([ \t]*\\)}")
	    (progn
	      (delete-region (match-beginning 1) (match-end 1))
	      (insert-char ?  (1- tcl-indent-level))))))
    (setq shift-amt (- indent (current-column)))
    (if (zerop shift-amt)
	(if (> (- (point-max) pos) (point))
	    (goto-char (- (point-max) pos)))
      (delete-region beg (point))
      (indent-to indent)
      ;; If initial point was within line's indentation,
      ;; position after the indentation.  Else stay at same point in text.
      (if (> (- (point-max) pos) (point))
	  (goto-char (- (point-max) pos))))
    shift-amt))

;;}}}
;;{{{ tcl-calculate-indentation

(defun tcl-calculate-indentation (&optional parse-start)
  "Return appropriate indentation for current line as tcl code.
In usual case returns an integer: the column to indent to."
  (let ((pos (point)))
    (save-excursion
      (if parse-start
	  (setq pos (goto-char parse-start)))
      (beginning-of-line)
      (if (bobp)
	  (current-indentation)
	(forward-char -1)
	(if (eq (preceding-char) ?\\)
	    (+ (current-indentation)
	       (progn
		 (beginning-of-line)
		 (if (bobp)
		     (* 2 tcl-indent-level)
		   (forward-char -1)
		   (if (not (eq (preceding-char) ?\\))
		       (* 2 tcl-indent-level)
		     0))))
	  (forward-char 1)
	  (if (re-search-backward
	       "\\(^[^ \t\n\r]\\)\\|\\({\\s *\n\\)\\|\\(}\\s *\n\\)"
	       nil  t)
	      (+ (- (current-indentation)
		    (if (save-excursion
			  (beginning-of-line)
			  (and (not (bobp))
			       (progn
				 (forward-char -1)
				 (eq (preceding-char) ?\\))))
			(* 2 tcl-indent-level)
		      0))
		 (if (eq (following-char) ?{)
		     tcl-indent-level
		   0))
	    (goto-char pos)
	    (beginning-of-line)
	    (forward-line -1)
	    (current-indentation)))))))

;;}}}
;;{{{ tcl-electric-brace

(defun tcl-electric-brace (arg)
  "Insert `}' and indent line for tcl."
  (interactive "P")
  (insert-char ?} (prefix-numeric-value arg))
  (tcl-indent-line)
  (blink-matching-open))

;;}}}

;;}}}
;;{{{ searching

;;{{{ tcl-beginning-of-proc

(defun tcl-beginning-of-proc (&optional arg)
  "Move backward to the beginning of a tcl proc (or similar).
With argument, do it that many times.  Negative arg -N
means move forward to Nth following beginning of proc.
Returns t unless search stops due to beginning or end of buffer."
  (interactive "P")
  (or arg
      (setq arg 1))
  (let ((found nil)
	(ret t))
    (if (and (< arg 0)
	     (looking-at "^[^ \t\n#][^\n]*{[ \t]*$"))
	(forward-char 1))
    (while (< arg 0)
      (if (re-search-forward "^[^ \t\n#][^\n]*{[ \t]*$" nil t)
	  (setq arg (1+ arg)
		found t)
	(setq ret nil
	      arg 0)))
    (if found
	(beginning-of-line))
    (while (> arg 0)
      (if (re-search-backward "^[^ \t\n#][^\n]*{[ \t]*$" nil t)
	  (setq arg (1- arg))
	(setq ret nil
	      arg 0)))
    ret))

;;}}}
;;{{{ tcl-end-of-proc

(defun tcl-end-of-proc (&optional arg)
  "Move forward to next end of tcl proc (or similar).
With argument, do it that many times.  Negative argument -N means move
back to Nth preceding end of proc.

This function just searches for a `}' at the beginning of a line."
  (interactive "P")
  (or arg
      (setq arg 1))
  (let ((found nil)
	(ret t))
    (if (and (< arg 0)
	     (not (bolp))
	     (save-excursion
	       (beginning-of-line)
	       (eq (following-char) ?})))
	(forward-char -1))
    (while (> arg 0)
      (if (re-search-forward "^}" nil t)
	  (setq arg (1- arg)
		found t)
	(setq ret nil
	      arg 0)))
    (while (< arg 0)
      (if (re-search-backward "^}" nil t)
	  (setq arg (1+ arg)
		found t)
	(setq ret nil
	      arg 0)))
    (if found
	(end-of-line))
    ret))

;;}}}

;;}}}
;;{{{ communication with a inferior process via comint

;;{{{ tcl-start-process

(defun tcl-start-process (name program &optional startfile &rest switches)
  "Start a tcl process named NAME, running PROGRAM."
  (or switches
      (setq switches tcl-default-command-switches))
  (setq tcl-process-buffer (apply 'make-comint name program startfile switches))
  (setq tcl-process (get-buffer-process tcl-process-buffer))
  (save-excursion
    (set-buffer tcl-process-buffer)
    (setq comint-prompt-regexp "^[^% ]*%\\( %\\)* *"))
  )

;;}}}
;;{{{ tcl-kill-process

(defun tcl-kill-process ()
  "Kill tcl subprocess and its buffer."
  (interactive)
  (if tcl-process-buffer
      (kill-buffer tcl-process-buffer)))

;;}}}
;;{{{ tcl-set-tcl-region-start

(defun tcl-set-tcl-region-start (&optional arg)
  "Set start of region for use with `tcl-send-tcl-region'."
  (interactive)
  (set-marker tcl-region-start (or arg (point))))

;;}}}
;;{{{ tcl-set-tcl-region-end

(defun tcl-set-tcl-region-end (&optional arg)
  "Set end of region for use with `tcl-send-tcl-region'."
  (interactive)
  (set-marker tcl-region-end (or arg (point))))

;;}}}
;;{{{ send line/region/buffer to tcl-process

;;{{{ tcl-send-current-line

(defun tcl-send-current-line ()
  "Send current line to tcl subprocess, found in `tcl-process'.
If `tcl-process' is nil or dead, start a new process first."
  (interactive)
  (let ((start (save-excursion (beginning-of-line) (point)))
	(end (save-excursion (end-of-line) (point))))
    (or (and tcl-process
	     (eq (process-status tcl-process) 'run))
	(tcl-start-process tcl-default-application tcl-default-application))
    (comint-simple-send tcl-process (buffer-substring start end))
    (forward-line 1)
    (if tcl-always-show
	(display-buffer tcl-process-buffer))))

;;}}}
;;{{{ tcl-send-region

(defun tcl-send-region (start end)
  "Send region to tcl subprocess, wrapped in `eval { ... }'."
  (interactive "r")
  (or (and tcl-process
	   (comint-check-proc tcl-process-buffer))
      (tcl-start-process tcl-default-application tcl-default-application))
  (comint-simple-send tcl-process
		      (concat tcl-default-eval
			      " {\n"(buffer-substring start end) "\n}"))
  (if tcl-always-show
      (display-buffer tcl-process-buffer)))

;;}}}
;;{{{ tcl-send-tcl-region

(defun tcl-send-tcl-region ()
  "Send preset tcl region to tcl subprocess, wrapped in `eval { ... }'."
  (interactive)
  (or (and tcl-region-start tcl-region-end)
      (error "tcl-region not set"))
  (or (and tcl-process
	   (comint-check-proc tcl-process-buffer))
      (tcl-start-process tcl-default-application tcl-default-application))
  (comint-simple-send tcl-process
		      (concat tcl-default-eval
			      " {\n"
			      (buffer-substring tcl-region-start tcl-region-end)
			      "\n}"))
  (if tcl-always-show
      (display-buffer tcl-process-buffer)))

;;}}}
;;{{{ tcl-send-proc

(defun tcl-send-proc ()
  "Send proc around point to tcl subprocess, wrapped in `eval { ... }'."
  (interactive)
  (let (beg end)
    (save-excursion
      (tcl-beginning-of-proc)
      (setq beg (point))
      (tcl-end-of-proc)
      (setq end (point)))
    (or (and tcl-process
	     (comint-check-proc tcl-process-buffer))
	(tcl-start-process tcl-default-application tcl-default-application))
    (comint-simple-send tcl-process
			(concat tcl-default-eval
				" {\n"
				(buffer-substring beg end)
				"\n}"))
    (if tcl-always-show
	(display-buffer tcl-process-buffer))))

;;}}}
;;{{{ tcl-send-buffer

(defun tcl-send-buffer ()
  "Send whole buffer to tcl subprocess, wrapped in `eval { ... }'."
  (interactive)
  (or (and tcl-process
	   (comint-check-proc tcl-process-buffer))
      (tcl-start-process tcl-default-application tcl-default-application))
  (if (buffer-modified-p)
      (comint-simple-send tcl-process
			  (concat
			   tcl-default-eval
			   " {\n"
			   (buffer-substring (point-min) (point-max))
			   "\n}"))
    (comint-simple-send tcl-process
			(concat "source "
				(buffer-file-name)
				"\n")))
  (if tcl-always-show
      (display-buffer tcl-process-buffer)))

;;}}}

;;}}}
;;{{{ tcl-get-error-info

(defun tcl-get-error-info ()
  "Send string `set errorInfo' to tcl subprocess and display the tcl buffer."
  (interactive)
  (or (and tcl-process
	   (comint-check-proc tcl-process-buffer))
      (tcl-start-process tcl-default-application tcl-default-application))
  (comint-simple-send tcl-process "set errorInfo\n")
  (display-buffer tcl-process-buffer))

;;}}}
;;{{{ tcl-restart-with-whole-file

(defun tcl-restart-with-whole-file ()
  "Restart tcl subprocess and send whole file as input."
  (interactive)
  (tcl-kill-process)
  (tcl-start-process tcl-default-application tcl-default-application)
  (tcl-send-buffer))
  
;;}}}  
;;{{{ tcl-show-process-buffer

(defun tcl-show-process-buffer ()
  "Make sure `tcl-process-buffer' is being displayed."
  (interactive)
  (display-buffer tcl-process-buffer))

;;}}}
;;{{{ tcl-hide-process-buffer

(defun tcl-hide-process-buffer ()
  "Delete all windows that display `tcl-process-buffer'."
  (interactive)
  (delete-windows-on tcl-process-buffer))

;;}}}

;;}}}

;;{{{ menu bar

(define-key tcl-mode-menu [restart-with-whole-file]
  '("Restart With Whole File" .  tcl-restart-with-whole-file))
(define-key tcl-mode-menu [kill-process]
  '("Kill Process" . tcl-kill-process))

(define-key tcl-mode-menu [hide-process-buffer]
  '("Hide Process Buffer" . tcl-hide-process-buffer))
(define-key tcl-mode-menu [get-error-info]
  '("Get Error Info" . tcl-get-error-info))
(define-key tcl-mode-menu [show-process-buffer]
  '("Show Process Buffer" . tcl-show-process-buffer))

(define-key tcl-mode-menu [end-of-proc]
  '("End Of Proc" . tcl-end-of-proc))
(define-key tcl-mode-menu [beginning-of-proc]
  '("Beginning Of Proc" . tcl-beginning-of-proc))

(define-key tcl-mode-menu [send-tcl-region]
  '("Send Tcl-Region" . tcl-send-tcl-region))
(define-key tcl-mode-menu [set-tcl-regio-end]
  '("Set Tcl-Region End" . tcl-set-tcl-region-end))
(define-key tcl-mode-menu [set-tcl-region-start]
  '("Set Tcl-Region Start" . tcl-set-tcl-region-start))

(define-key tcl-mode-menu [send-current-line]
  '("Send Current Line" . tcl-send-current-line))
(define-key tcl-mode-menu [send-region]
  '("Send Region" . tcl-send-region))
(define-key tcl-mode-menu [send-proc]
  '("Send Proc" . tcl-send-proc))
(define-key tcl-mode-menu [send-buffer]
  '("Send Buffer" . tcl-send-buffer))

;;}}}

;;{{{ Emacs local variables


;; Local Variables:
;; folded-file: t
;; End:

;;}}}

;;; tcl-mode.el ends here
