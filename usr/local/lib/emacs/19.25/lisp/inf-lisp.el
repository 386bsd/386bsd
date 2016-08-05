;;; inf-lisp.el --- an inferior-lisp mode
;;; Copyright (C) 1988, 1993, 1994 Free Software Foundation, Inc.

;; Author: Olin Shivers <shivers@cs.cmu.edu>
;; Keywords: processes, lisp

;;; This file is part of GNU Emacs.

;;; GNU Emacs is free software; you can redistribute it and/or modify
;;; it under the terms of the GNU General Public License as published by
;;; the Free Software Foundation; either version 2, or (at your option)
;;; any later version.

;;; GNU Emacs is distributed in the hope that it will be useful,
;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;; GNU General Public License for more details.

;;; You should have received a copy of the GNU General Public License
;;; along with GNU Emacs; see the file COPYING.  If not, write to
;;; the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

;;; Commentary:

;;; Hacked from tea.el by Olin Shivers (shivers@cs.cmu.edu). 8/88

;;; This file defines a a lisp-in-a-buffer package (inferior-lisp
;;; mode) built on top of comint mode.  This version is more
;;; featureful, robust, and uniform than the Emacs 18 version.  The
;;; key bindings are also more compatible with the bindings of Hemlock
;;; and Zwei (the Lisp Machine emacs).

;;; Since this mode is built on top of the general command-interpreter-in-
;;; a-buffer mode (comint mode), it shares a common base functionality, 
;;; and a common set of bindings, with all modes derived from comint mode.
;;; This makes these modes easier to use.

;;; For documentation on the functionality provided by comint mode, and
;;; the hooks available for customising it, see the file comint.el.
;;; For further information on inferior-lisp mode, see the comments below.

;;; Needs fixin:
;;; The load-file/compile-file default mechanism could be smarter -- it
;;; doesn't know about the relationship between filename extensions and
;;; whether the file is source or executable. If you compile foo.lisp
;;; with compile-file, then the next load-file should use foo.bin for
;;; the default, not foo.lisp. This is tricky to do right, particularly
;;; because the extension for executable files varies so much (.o, .bin,
;;; .lbin, .mo, .vo, .ao, ...).
;;;
;;; It would be nice if inferior-lisp (and inferior scheme, T, ...) modes
;;; had a verbose minor mode wherein sending or compiling defuns, etc.
;;; would be reflected in the transcript with suitable comments, e.g.
;;; ";;; redefining fact". Several ways to do this. Which is right?
;;;
;;; When sending text from a source file to a subprocess, the process-mark can 
;;; move off the window, so you can lose sight of the process interactions.
;;; Maybe I should ensure the process mark is in the window when I send
;;; text to the process? Switch selectable?

;;; Code:

(require 'comint)
(require 'lisp-mode)


;;;###autoload
(defvar inferior-lisp-filter-regexp "\\`\\s *\\(:\\(\\w\\|\\s_\\)\\)?\\s *\\'"
  "*What not to save on inferior Lisp's input history.
Input matching this regexp is not saved on the input history in Inferior Lisp
mode.  Default is whitespace followed by 0 or 1 single-letter colon-keyword 
\(as in :a, :c, etc.)")

(defvar inferior-lisp-mode-map nil)
(cond ((not inferior-lisp-mode-map)
       (setq inferior-lisp-mode-map
	     (copy-keymap comint-mode-map))
       (setq inferior-lisp-mode-map
	     (nconc inferior-lisp-mode-map shared-lisp-mode-map))
       (define-key inferior-lisp-mode-map "\C-x\C-e" 'lisp-eval-last-sexp)
       (define-key inferior-lisp-mode-map "\C-c\C-l" 'lisp-load-file)
       (define-key inferior-lisp-mode-map "\C-c\C-k" 'lisp-compile-file)
       (define-key inferior-lisp-mode-map "\C-c\C-a" 'lisp-show-arglist)
       (define-key inferior-lisp-mode-map "\C-c\C-d" 'lisp-describe-sym)
       (define-key inferior-lisp-mode-map "\C-c\C-f"
	 'lisp-show-function-documentation)
       (define-key inferior-lisp-mode-map "\C-c\C-v"
	 'lisp-show-variable-documentation)))

;;; These commands augment Lisp mode, so you can process Lisp code in
;;; the source files.
(define-key lisp-mode-map "\M-\C-x"  'lisp-eval-defun)     ; Gnu convention
(define-key lisp-mode-map "\C-x\C-e" 'lisp-eval-last-sexp) ; Gnu convention
(define-key lisp-mode-map "\C-c\C-e" 'lisp-eval-defun)
(define-key lisp-mode-map "\C-c\C-r" 'lisp-eval-region)
(define-key lisp-mode-map "\C-c\C-c" 'lisp-compile-defun)
(define-key lisp-mode-map "\C-c\C-z" 'switch-to-lisp)
(define-key lisp-mode-map "\C-c\C-l" 'lisp-load-file)
(define-key lisp-mode-map "\C-c\C-k" 'lisp-compile-file)  ; "kompile" file
(define-key lisp-mode-map "\C-c\C-a" 'lisp-show-arglist)
(define-key lisp-mode-map "\C-c\C-d" 'lisp-describe-sym)
(define-key lisp-mode-map "\C-c\C-f" 'lisp-show-function-documentation)
(define-key lisp-mode-map "\C-c\C-v" 'lisp-show-variable-documentation)


;;; This function exists for backwards compatibility.
;;; Previous versions of this package bound commands to C-c <letter>
;;; bindings, which is not allowed by the gnumacs standard.

;;;  "This function binds many inferior-lisp commands to C-c <letter> bindings,
;;;where they are more accessible. C-c <letter> bindings are reserved for the
;;;user, so these bindings are non-standard. If you want them, you should
;;;have this function called by the inferior-lisp-load-hook:
;;;    (setq inferior-lisp-load-hook '(inferior-lisp-install-letter-bindings))
;;;You can modify this function to install just the bindings you want."
(defun inferior-lisp-install-letter-bindings ()
  (define-key lisp-mode-map "\C-ce" 'lisp-eval-defun-and-go)
  (define-key lisp-mode-map "\C-cr" 'lisp-eval-region-and-go)
  (define-key lisp-mode-map "\C-cc" 'lisp-compile-defun-and-go)
  (define-key lisp-mode-map "\C-cz" 'switch-to-lisp)
  (define-key lisp-mode-map "\C-cl" 'lisp-load-file)
  (define-key lisp-mode-map "\C-ck" 'lisp-compile-file)
  (define-key lisp-mode-map "\C-ca" 'lisp-show-arglist)
  (define-key lisp-mode-map "\C-cd" 'lisp-describe-sym)
  (define-key lisp-mode-map "\C-cf" 'lisp-show-function-documentation)
  (define-key lisp-mode-map "\C-cv" 'lisp-show-variable-documentation)
  
  (define-key inferior-lisp-mode-map "\C-cl" 'lisp-load-file)
  (define-key inferior-lisp-mode-map "\C-ck" 'lisp-compile-file)
  (define-key inferior-lisp-mode-map "\C-ca" 'lisp-show-arglist)
  (define-key inferior-lisp-mode-map "\C-cd" 'lisp-describe-sym)
  (define-key inferior-lisp-mode-map "\C-cf" 'lisp-show-function-documentation)
  (define-key inferior-lisp-mode-map "\C-cv"
    'lisp-show-variable-documentation))


;;;###autoload
(defvar inferior-lisp-program "lisp"
  "*Program name for invoking an inferior Lisp with for Inferior Lisp mode.")

;;;###autoload
(defvar inferior-lisp-load-command "(load \"%s\")\n"
  "*Format-string for building a Lisp expression to load a file.
This format string should use `%s' to substitute a file name
and should result in a Lisp expression that will command the inferior Lisp
to load that file.  The default works acceptably on most Lisps.
The string \"(progn (load \\\"%s\\\" :verbose nil :print t) (values))\\\n\"
produces cosmetically superior output for this application,
but it works only in Common Lisp.")

;;;###autoload
(defvar inferior-lisp-prompt "^[^> \n]*>+:? *"
  "Regexp to recognise prompts in the Inferior Lisp mode.
Defaults to \"^[^> \\n]*>+:? *\", which works pretty good for Lucid, kcl,
and franz.  This variable is used to initialize `comint-prompt-regexp' in the 
Inferior Lisp buffer.

More precise choices:
Lucid Common Lisp: \"^\\(>\\|\\(->\\)+\\) *\"
franz: \"^\\(->\\|<[0-9]*>:\\) *\"
kcl: \"^>+ *\"

This is a fine thing to set in your .emacs file.")

(defvar inferior-lisp-buffer nil "*The current inferior-lisp process buffer.

MULTIPLE PROCESS SUPPORT
===========================================================================
To run multiple Lisp processes, you start the first up
with \\[inferior-lisp].  It will be in a buffer named `*inferior-lisp*'.
Rename this buffer with \\[rename-buffer].  You may now start up a new
process with another \\[inferior-lisp].  It will be in a new buffer,
named `*inferior-lisp*'.  You can switch between the different process
buffers with \\[switch-to-buffer].

Commands that send text from source buffers to Lisp processes --
like `lisp-eval-defun' or `lisp-show-arglist' -- have to choose a process
to send to, when you have more than one Lisp process around.  This
is determined by the global variable `inferior-lisp-buffer'.  Suppose you
have three inferior Lisps running:
    Buffer              Process
    foo                 inferior-lisp
    bar                 inferior-lisp<2>
    *inferior-lisp*     inferior-lisp<3>
If you do a \\[lisp-eval-defun] command on some Lisp source code, 
what process do you send it to?

- If you're in a process buffer (foo, bar, or *inferior-lisp*), 
  you send it to that process.
- If you're in some other buffer (e.g., a source file), you
  send it to the process attached to buffer `inferior-lisp-buffer'.
This process selection is performed by function `inferior-lisp-proc'.

Whenever \\[inferior-lisp] fires up a new process, it resets
`inferior-lisp-buffer' to be the new process's buffer.  If you only run
one process, this does the right thing.  If you run multiple
processes, you can change `inferior-lisp-buffer' to another process
buffer with \\[set-variable].")

;;;###autoload
(defvar inferior-lisp-mode-hook '()
  "*Hook for customising Inferior Lisp mode.")

(defun inferior-lisp-mode () 
  "Major mode for interacting with an inferior Lisp process.  
Runs a Lisp interpreter as a subprocess of Emacs, with Lisp I/O through an
Emacs buffer.  Variable `inferior-lisp-program' controls which Lisp interpreter
is run.  Variables `inferior-lisp-prompt', `inferior-lisp-filter-regexp' and
`inferior-lisp-load-command' can customize this mode for different Lisp
interpreters.

For information on running multiple processes in multiple buffers, see
documentation for variable `inferior-lisp-buffer'.

\\{inferior-lisp-mode-map}

Customisation: Entry to this mode runs the hooks on `comint-mode-hook' and
`inferior-lisp-mode-hook' (in that order).

You can send text to the inferior Lisp process from other buffers containing
Lisp source.  
    switch-to-lisp switches the current buffer to the Lisp process buffer.
    lisp-eval-defun sends the current defun to the Lisp process.
    lisp-compile-defun compiles the current defun.
    lisp-eval-region sends the current region to the Lisp process.
    lisp-compile-region compiles the current region.

    Prefixing the lisp-eval/compile-defun/region commands with
    a \\[universal-argument] causes a switch to the Lisp process buffer after sending
    the text.

Commands:
Return after the end of the process' output sends the text from the 
    end of process to point.
Return before the end of the process' output copies the sexp ending at point
    to the end of the process' output, and sends it.
Delete converts tabs to spaces as it moves back.
Tab indents for Lisp; with argument, shifts rest
    of expression rigidly with the current line.
C-M-q does Tab on each line starting within following expression.
Paragraphs are separated only by blank lines.  Semicolons start comments.
If you accidentally suspend your process, use \\[comint-continue-subjob]
to continue it."
  (interactive)
  (comint-mode)
  (setq comint-prompt-regexp inferior-lisp-prompt)
  (setq major-mode 'inferior-lisp-mode)
  (setq mode-name "Inferior Lisp")
  (setq mode-line-process '(":%s"))
  (lisp-mode-variables t)
  (use-local-map inferior-lisp-mode-map)    ;c-c c-k for "kompile" file
  (setq comint-get-old-input (function lisp-get-old-input))
  (setq comint-input-filter (function lisp-input-filter))
  (setq comint-input-sentinel 'ignore)
  (run-hooks 'inferior-lisp-mode-hook))

(defun lisp-get-old-input ()
  "Return a string containing the sexp ending at point."
  (save-excursion
    (let ((end (point)))
      (backward-sexp)
      (buffer-substring (point) end))))

(defun lisp-input-filter (str)
  "t if STR does not match `inferior-lisp-filter-regexp'."
  (not (string-match inferior-lisp-filter-regexp str)))

;;;###autoload
(defun inferior-lisp (cmd)
  "Run an inferior Lisp process, input and output via buffer `*inferior-lisp*'.
If there is a process already running in `*inferior-lisp*', just switch
to that buffer.
With argument, allows you to edit the command line (default is value
of `inferior-lisp-program').  Runs the hooks from
`inferior-lisp-mode-hook' (after the `comint-mode-hook' is run).
\(Type \\[describe-mode] in the process buffer for a list of commands.)"
  (interactive (list (if current-prefix-arg
			 (read-string "Run lisp: " inferior-lisp-program)
		       inferior-lisp-program)))
  (if (not (comint-check-proc "*inferior-lisp*"))
      (let ((cmdlist (inferior-lisp-args-to-list cmd)))
	(set-buffer (apply (function make-comint)
			   "inferior-lisp" (car cmdlist) nil (cdr cmdlist)))
	(inferior-lisp-mode)))
  (setq inferior-lisp-buffer "*inferior-lisp*")
  (switch-to-buffer "*inferior-lisp*"))

;;;###autoload
(defalias 'run-lisp 'inferior-lisp)

;;; Break a string up into a list of arguments.
;;; This will break if you have an argument with whitespace, as in
;;; string = "-ab +c -x 'you lose'".
(defun inferior-lisp-args-to-list (string)
  (let ((where (string-match "[ \t]" string)))
    (cond ((null where) (list string))
	  ((not (= where 0))
	   (cons (substring string 0 where)
		 (inferior-lisp-args-to-list (substring string (+ 1 where)
							(length string)))))
	  (t (let ((pos (string-match "[^ \t]" string)))
	       (if (null pos)
		   nil
		 (inferior-lisp-args-to-list (substring string pos
							(length string)))))))))

(defun lisp-eval-region (start end &optional and-go)
  "Send the current region to the inferior Lisp process.
Prefix argument means switch to the Lisp buffer afterwards."
  (interactive "r\nP")
  (comint-send-region (inferior-lisp-proc) start end)
  (comint-send-string (inferior-lisp-proc) "\n")
  (if and-go (switch-to-lisp t)))

(defun lisp-eval-defun (&optional and-go)
  "Send the current defun to the inferior Lisp process.
Prefix argument means switch to the Lisp buffer afterwards."
  (interactive "P")
  (save-excursion
    (end-of-defun)
    (skip-chars-backward " \t\n\r\f") ;  Makes allegro happy
    (let ((end (point)))
      (beginning-of-defun)
      (lisp-eval-region (point) end)))
  (if and-go (switch-to-lisp t)))

(defun lisp-eval-last-sexp (&optional and-go)
  "Send the previous sexp to the inferior Lisp process.
Prefix argument means switch to the Lisp buffer afterwards."
  (interactive "P")
  (lisp-eval-region (save-excursion (backward-sexp) (point)) (point) and-go))

;;; Common Lisp COMPILE sux. 
(defun lisp-compile-region (start end &optional and-go)
  "Compile the current region in the inferior Lisp process.
Prefix argument means switch to the Lisp buffer afterwards."
  (interactive "r\nP")
  (comint-send-string
   (inferior-lisp-proc)
   (format "(funcall (compile nil `(lambda () (progn 'compile %s))))\n"
	   (buffer-substring start end)))
  (if and-go (switch-to-lisp t)))

(defun lisp-compile-defun (&optional and-go)
  "Compile the current defun in the inferior Lisp process.
Prefix argument means switch to the Lisp buffer afterwards."
  (interactive "P")
  (save-excursion
    (end-of-defun)
    (skip-chars-backward " \t\n\r\f") ;  Makes allegro happy
    (let ((e (point)))
      (beginning-of-defun)
      (lisp-compile-region (point) e)))
  (if and-go (switch-to-lisp t)))

(defun switch-to-lisp (eob-p)
  "Switch to the inferior Lisp process buffer.
With argument, positions cursor at end of buffer."
  (interactive "P")
  (if (get-buffer inferior-lisp-buffer)
      (pop-to-buffer inferior-lisp-buffer)
    (error "No current inferior Lisp buffer"))
  (cond (eob-p
	 (push-mark)
	 (goto-char (point-max)))))


;;; Now that lisp-compile/eval-defun/region takes an optional prefix arg,
;;; these commands are redundant. But they are kept around for the user
;;; to bind if he wishes, for backwards functionality, and because it's
;;; easier to type C-c e than C-u C-c C-e.

(defun lisp-eval-region-and-go (start end)
  "Send the current region to the inferior Lisp, and switch to its buffer."
  (interactive "r")
  (lisp-eval-region start end t))

(defun lisp-eval-defun-and-go ()
  "Send the current defun to the inferior Lisp, and switch to its buffer."
  (interactive)
  (lisp-eval-defun t))

(defun lisp-compile-region-and-go (start end)
  "Compile the current region in the inferior Lisp, and switch to its buffer."
  (interactive "r")
  (lisp-compile-region start end t))

(defun lisp-compile-defun-and-go ()
  "Compile the current defun in the inferior Lisp, and switch to its buffer."
  (interactive)
  (lisp-compile-defun t))

;;; A version of the form in H. Shevis' soar-mode.el package. Less robust.
;;; (defun lisp-compile-sexp (start end)
;;;   "Compile the s-expression bounded by START and END in the inferior lisp.
;;; If the sexp isn't a DEFUN form, it is evaluated instead."
;;;   (cond ((looking-at "(defun\\s +")
;;; 	 (goto-char (match-end 0))
;;; 	 (let ((name-start (point)))
;;; 	   (forward-sexp 1)
;;; 	   (process-send-string "inferior-lisp"
;;; 				(format "(compile '%s #'(lambda "
;;; 					(buffer-substring name-start
;;; 							  (point)))))
;;; 	 (let ((body-start (point)))
;;; 	   (goto-char start) (forward-sexp 1) ; Can't use end-of-defun.
;;; 	   (process-send-region "inferior-lisp"
;;; 				(buffer-substring body-start (point))))
;;; 	 (process-send-string "inferior-lisp" ")\n"))
;;; 	(t (lisp-eval-region start end)))))
;;; 
;;; (defun lisp-compile-region (start end)
;;;   "Each s-expression in the current region is compiled (if a DEFUN)
;;; or evaluated (if not) in the inferior lisp."
;;;   (interactive "r")
;;;   (save-excursion
;;;     (goto-char start) (end-of-defun) (beginning-of-defun) ; error check
;;;     (if (< (point) start) (error "region begins in middle of defun"))
;;;     (goto-char start)
;;;     (let ((s start))
;;;       (end-of-defun)
;;;       (while (<= (point) end) ; Zip through
;;; 	(lisp-compile-sexp s (point)) ; compiling up defun-sized chunks.
;;; 	(setq s (point))
;;; 	(end-of-defun))
;;;       (if (< s end) (lisp-compile-sexp s end)))))
;;; 
;;; End of HS-style code


(defvar lisp-prev-l/c-dir/file nil
  "Record last directory and file used in loading or compiling.
This holds a cons cell of the form `(DIRECTORY . FILE)'
describing the last `lisp-load-file' or `lisp-compile-file' command.")

(defvar lisp-source-modes '(lisp-mode)
  "*Used to determine if a buffer contains Lisp source code.
If it's loaded into a buffer that is in one of these major modes, it's
considered a Lisp source file by `lisp-load-file' and `lisp-compile-file'.
Used by these commands to determine defaults.")

(defun lisp-load-file (file-name)
  "Load a Lisp file into the inferior Lisp process."
  (interactive (comint-get-source "Load Lisp file: " lisp-prev-l/c-dir/file
				  lisp-source-modes nil)) ; NIL because LOAD
					; doesn't need an exact name
  (comint-check-source file-name) ; Check to see if buffer needs saved.
  (setq lisp-prev-l/c-dir/file (cons (file-name-directory    file-name)
				     (file-name-nondirectory file-name)))
  (comint-send-string (inferior-lisp-proc)
		      (format inferior-lisp-load-command file-name))
  (switch-to-lisp t))


(defun lisp-compile-file (file-name)
  "Compile a Lisp file in the inferior Lisp process."
  (interactive (comint-get-source "Compile Lisp file: " lisp-prev-l/c-dir/file
				  lisp-source-modes nil)) ; NIL = don't need
					; suffix .lisp
  (comint-check-source file-name) ; Check to see if buffer needs saved.
  (setq lisp-prev-l/c-dir/file (cons (file-name-directory    file-name)
				     (file-name-nondirectory file-name)))
  (comint-send-string (inferior-lisp-proc) (concat "(compile-file \""
						   file-name
						   "\"\)\n"))
  (switch-to-lisp t))



;;; Documentation functions: function doc, var doc, arglist, and
;;; describe symbol.
;;; ===========================================================================

;;; Command strings
;;; ===============

(defvar lisp-function-doc-command
  "(let ((fn '%s))
     (format t \"Documentation for ~a:~&~a\"
	     fn (documentation fn 'function))
     (values))\n"
  "Command to query inferior Lisp for a function's documentation.")

(defvar lisp-var-doc-command
  "(let ((v '%s))
     (format t \"Documentation for ~a:~&~a\"
	     v (documentation v 'variable))
     (values))\n"
  "Command to query inferior Lisp for a variable's documentation.")

(defvar lisp-arglist-command
  "(let ((fn '%s))
     (format t \"Arglist for ~a: ~a\" fn (arglist fn))
     (values))\n"
  "Command to query inferior Lisp for a function's arglist.")

(defvar lisp-describe-sym-command
  "(describe '%s)\n"
  "Command to query inferior Lisp for a variable's documentation.")


;;; Ancillary functions
;;; ===================

;;; Reads a string from the user.
(defun lisp-symprompt (prompt default)
  (list (let* ((prompt (if default
			   (format "%s (default %s): " prompt default)
			 (concat prompt ": ")))
	       (ans (read-string prompt)))
	  (if (zerop (length ans)) default ans))))


;;; Adapted from function-called-at-point in help.el.
(defun lisp-fn-called-at-pt ()
  "Returns the name of the function called in the current call.
The value is nil if it can't find one."
  (condition-case nil
      (save-excursion
	(save-restriction
	  (narrow-to-region (max (point-min) (- (point) 1000)) (point-max))
	  (backward-up-list 1)
	  (forward-char 1)
	  (let ((obj (read (current-buffer))))
	    (and (symbolp obj) obj))))
    (error nil)))


;;; Adapted from variable-at-point in help.el.
(defun lisp-var-at-pt ()
  (condition-case ()
      (save-excursion
	(forward-sexp -1)
	(skip-chars-forward "'")
	(let ((obj (read (current-buffer))))
	  (and (symbolp obj) obj)))
    (error nil)))


;;; Documentation functions: fn and var doc, arglist, and symbol describe.
;;; ======================================================================

(defun lisp-show-function-documentation (fn)
  "Send a command to the inferior Lisp to give documentation for function FN.
See variable `lisp-function-doc-command'."
  (interactive (lisp-symprompt "Function doc" (lisp-fn-called-at-pt)))
  (comint-proc-query (inferior-lisp-proc)
		     (format lisp-function-doc-command fn)))

(defun lisp-show-variable-documentation (var)
  "Send a command to the inferior Lisp to give documentation for function FN.
See variable `lisp-var-doc-command'."
  (interactive (lisp-symprompt "Variable doc" (lisp-var-at-pt)))
  (comint-proc-query (inferior-lisp-proc) (format lisp-var-doc-command var)))

(defun lisp-show-arglist (fn)
  "Send a query to the inferior Lisp for the arglist for function FN.
See variable `lisp-arglist-command'."
  (interactive (lisp-symprompt "Arglist" (lisp-fn-called-at-pt)))
  (comint-proc-query (inferior-lisp-proc) (format lisp-arglist-command fn)))

(defun lisp-describe-sym (sym)
  "Send a command to the inferior Lisp to describe symbol SYM.
See variable `lisp-describe-sym-command'."
  (interactive (lisp-symprompt "Describe" (lisp-var-at-pt)))
  (comint-proc-query (inferior-lisp-proc)
		     (format lisp-describe-sym-command sym)))


;;  "Returns the current inferior Lisp process.
;; See variable `inferior-lisp-buffer'."
(defun inferior-lisp-proc ()
  (let ((proc (get-buffer-process (if (eq major-mode 'inferior-lisp-mode)
				      (current-buffer)
				    inferior-lisp-buffer))))
    (or proc
	(error "No Lisp subprocess; see variable `inferior-lisp-buffer'"))))


;;; Do the user's customisation...
;;;===============================
(defvar inferior-lisp-load-hook nil
  "This hook is run when the library `inf-lisp' is loaded.
This is a good place to put keybindings.")

(run-hooks 'inferior-lisp-load-hook)

;;; CHANGE LOG
;;; ===========================================================================
;;; 7/21/92 Jim Blandy
;;; - Changed all uses of the cmulisp name or prefix to inferior-lisp;
;;;   this is now the official inferior lisp package.  Use the global
;;;   ChangeLog from now on.
;;; 5/24/90 Olin
;;; - Split cmulisp and cmushell modes into separate files. 
;;;   Not only is this a good idea, it's apparently the way it'll be rel 19.
;;; - Upgraded process sends to use comint-send-string instead of
;;;   process-send-string.
;;; - Explicit references to process "cmulisp" have been replaced with
;;;   (cmulisp-proc). This allows better handling of multiple process bufs.
;;; - Added process query and var/function/symbol documentation
;;;   commands. Based on code written by Douglas Roberts.
;;; - Added lisp-eval-last-sexp, bound to C-x C-e.
;;;
;;; 9/20/90 Olin
;;; Added a save-restriction to lisp-fn-called-at-pt. This bug and fix
;;; reported by Lennart Staflin.
;;;
;;; 3/12/90 Olin
;;; - lisp-load-file and lisp-compile-file no longer switch-to-lisp.
;;;   Tale suggested this.
;;; - Reversed this decision 7/15/91. You need the visual feedback.
;;;
;;; 7/25/91 Olin
;;; Changed all keybindings of the form C-c <letter>. These are
;;; supposed to be reserved for the user to bind. This affected
;;; mainly the compile/eval-defun/region[-and-go] commands.
;;; This was painful, but necessary to adhere to the gnumacs standard.
;;; For some backwards compatibility, see the 
;;;     cmulisp-install-letter-bindings
;;; function.
;;;
;;; 8/2/91 Olin
;;; - The lisp-compile/eval-defun/region commands now take a prefix arg,
;;;   which means switch-to-lisp after sending the text to the Lisp process.
;;;   This obsoletes all the -and-go commands. The -and-go commands are
;;;   kept around for historical reasons, and because the user can bind
;;;   them to key sequences shorter than C-u C-c C-<letter>.
;;; - If M-x cmulisp is invoked with a prefix arg, it allows you to
;;;   edit the command line.

(provide 'inf-lisp)

;;; inf-lisp.el ends here
