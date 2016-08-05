;;; tpu-edt.el --- Emacs emulating TPU emulating EDT

;; Copyright (C) 1993 Free Software Foundation, Inc.

;; Author: Rob Riepel <riepel@networking.stanford.edu>
;; Maintainer: Rob Riepel <riepel@networking.stanford.edu>
;; Version: 3.2
;; Keywords: emulations

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


;;;
;;;  Revision and Version Information
;;;
(defconst tpu-version "3.2" "TPU-edt version number.")


;;;
;;;  User Configurable Variables
;;;
(defconst tpu-have-ispell t
  "*If non-nil (default), TPU-edt uses ispell for spell checking.")

(defconst tpu-kill-buffers-silently nil
  "*If non-nil, TPU-edt kills modified buffers without asking.")

(defvar tpu-percent-scroll 75
  "*Percentage of the screen to scroll for next/previous screen commands.")

(defvar tpu-pan-columns 16
  "*Number of columns the tpu-pan functions scroll left or right.")


;;;
;;;  Emacs version identifiers - currently referenced by
;;;
;;;     o tpu-mark              o tpu-set-mark
;;;     o tpu-string-prompt     o tpu-regexp-prompt
;;;     o tpu-edt-on            o tpu-load-xkeys
;;;     o tpu-update-mode-line  o mode line section
;;;
(defconst tpu-emacs19-p (not (string-lessp emacs-version "19"))
  "Non-NIL if we are running Lucid or GNU Emacs version 19.")

(defconst tpu-gnu-emacs18-p (not tpu-emacs19-p)
  "Non-NIL if we are running GNU Emacs version 18.")

(defconst tpu-lucid-emacs19-p
  (and tpu-emacs19-p (string-match "Lucid" emacs-version))
  "Non-NIL if we are running Lucid Emacs version 19.")

(defconst tpu-gnu-emacs19-p (and tpu-emacs19-p (not tpu-lucid-emacs19-p))
  "Non-NIL if we are running GNU Emacs version 19.")


;;;
;;;  Global Keymaps
;;;
(defvar CSI-map (make-sparse-keymap)
  "Maps the CSI function keys on the VT100 keyboard.
CSI is DEC's name for the sequence <ESC>[.")

(defvar SS3-map (make-sparse-keymap)
  "Maps the SS3 function keys on the VT100 keyboard.
SS3 is DEC's name for the sequence <ESC>O.")

(defvar GOLD-map (make-keymap)
  "Maps the function keys on the VT100 keyboard preceeded by PF1.
GOLD is the ASCII 7-bit escape sequence <ESC>OP.")

(defvar GOLD-CSI-map (make-sparse-keymap)
  "Maps the function keys on the VT100 keyboard preceeded by GOLD-CSI.")

(defvar GOLD-SS3-map (make-sparse-keymap)
  "Maps the function keys on the VT100 keyboard preceeded by GOLD-SS3.")

(defvar tpu-global-map nil "TPU-edt global keymap.")
(defvar tpu-original-global-map (copy-keymap global-map)
  "Original global keymap.")

(and tpu-lucid-emacs19-p
     (defvar minibuffer-local-ns-map (make-sparse-keymap)
       "Hack to give Lucid emacs the same maps as GNU emacs."))


;;;
;;;  Global Variables
;;;
(defvar tpu-edt-mode nil
  "If non-nil, TPU-edt mode is active.")

(defvar tpu-last-replaced-text ""
  "Last text deleted by a TPU-edt replace command.")
(defvar tpu-last-deleted-region ""
  "Last text deleted by a TPU-edt remove command.")
(defvar tpu-last-deleted-lines ""
  "Last text deleted by a TPU-edt line-delete command.")
(defvar tpu-last-deleted-words ""
  "Last text deleted by a TPU-edt word-delete command.")
(defvar tpu-last-deleted-char ""
  "Last character deleted by a TPU-edt character-delete command.")

(defvar tpu-searching-forward t
  "If non-nil, TPU-edt is searching in the forward direction.")
(defvar tpu-search-last-string ""
  "Last text searched for by the TPU-edt search commands.")

(defvar tpu-regexp-p nil
  "If non-nil, TPU-edt uses regexp search and replace routines.")
(defvar tpu-rectangular-p nil
  "If non-nil, TPU-edt removes and inserts rectangles.")
(defvar tpu-advance t
  "True when TPU-edt is operating in the forward direction.")
(defvar tpu-reverse nil
  "True when TPU-edt is operating in the backward direction.")
(defvar tpu-control-keys t
  "If non-nil, control keys are set to perform TPU functions.")
(defvar tpu-xkeys-file nil
  "File containing TPU-edt X key map.")

(defvar tpu-rectangle-string nil
  "Mode line string to identify rectangular mode.")
(defvar tpu-direction-string nil
  "Mode line string to identify current direction.")

(defvar tpu-add-at-bol-hist nil
  "History variable for tpu-edt-add-at-bol function.")
(defvar tpu-add-at-eol-hist nil
  "History variable for tpu-edt-add-at-eol function.")
(defvar tpu-regexp-prompt-hist  nil
  "History variable for search and replace functions.")


;;;
;;;  Buffer Local Variables
;;;
(defvar tpu-newline-and-indent-p nil
  "If non-nil, Return produces a newline and indents.")
(make-variable-buffer-local 'tpu-newline-and-indent-p)

(defvar tpu-newline-and-indent-string nil
  "Mode line string to identify AutoIndent mode.")
(make-variable-buffer-local 'tpu-newline-and-indent-string)

(defvar tpu-saved-delete-func nil
  "Saved value of the delete key.")
(make-variable-buffer-local 'tpu-saved-delete-func)

(defvar tpu-buffer-local-map nil
  "TPU-edt buffer local key map.")
(make-variable-buffer-local 'tpu-buffer-local-map)


;;;
;;;  Mode Line - Modify the mode line to show the following
;;;
;;;     o  If the mark is set.
;;;     o  Direction of motion.
;;;     o  Active rectangle mode.
;;;
(defvar tpu-original-mode-line mode-line-format)
(defvar tpu-original-mm-alist minor-mode-alist)

(defvar tpu-mark-flag " ")
(make-variable-buffer-local 'tpu-mark-flag)

(defun tpu-set-mode-line (for-tpu)
  "Set the mode for TPU-edt, or reset it to default Emacs."
  (cond ((not for-tpu)
	 (setq mode-line-format tpu-original-mode-line)
	 (setq minor-mode-alist tpu-original-mm-alist))
	(t
	 (setq-default mode-line-format
		       (list (purecopy "")
			     'mode-line-modified
			     'mode-line-buffer-identification
			     (purecopy "  ")
			     'global-mode-string
			     (purecopy "  ")
			     'tpu-mark-flag
			     (purecopy " %[(")
			     'mode-name 'mode-line-process 'minor-mode-alist "%n"
			     (purecopy ")%]----")
			     (purecopy '(-3 . "%p"))
			     (purecopy "-%-")))
	 (or (assq 'tpu-newline-and-indent-p minor-mode-alist)
	     (setq minor-mode-alist
		   (cons '(tpu-newline-and-indent-p
			   tpu-newline-and-indent-string)
			 minor-mode-alist)))
	 (or (assq 'tpu-rectangular-p minor-mode-alist)
	     (setq minor-mode-alist
		   (cons '(tpu-rectangular-p tpu-rectangle-string)
			 minor-mode-alist)))
	 (or (assq 'tpu-direction-string minor-mode-alist)
	     (setq minor-mode-alist
		   (cons '(tpu-direction-string tpu-direction-string)
			 minor-mode-alist))))))

(defun tpu-update-mode-line nil
  "Make sure mode-line in the current buffer reflects all changes."
  (setq tpu-mark-flag (if (tpu-mark) "M" " "))
  (cond (tpu-emacs19-p (force-mode-line-update))
	(t (set-buffer-modified-p (buffer-modified-p)) (sit-for 0))))

(cond (tpu-gnu-emacs19-p
       (add-hook 'activate-mark-hook 'tpu-update-mode-line)
       (add-hook 'deactivate-mark-hook 'tpu-update-mode-line))
      (tpu-lucid-emacs19-p
       (add-hook 'zmacs-deactivate-region-hook 'tpu-update-mode-line)
       (add-hook 'zmacs-activate-region-hook 'tpu-update-mode-line)))


;;;
;;;  Match Markers -
;;;
;;;     Set in:  Search
;;;
;;;     Used in: Replace, Substitute, Store-Text, Cut/Remove,
;;;              Append, and Change-Case
;;;
(defvar tpu-match-beginning-mark (make-marker))
(defvar tpu-match-end-mark (make-marker))

(defun tpu-set-match nil
  "Set markers at match beginning and end."
  ;; Add one to beginning mark so it stays with the first character of
  ;;   the string even if characters are added just before the string.
  (setq tpu-match-beginning-mark (copy-marker (1+ (match-beginning 0))))
  (setq tpu-match-end-mark (copy-marker (match-end 0))))

(defun tpu-unset-match nil
  "Unset match beginning and end markers."
  (set-marker tpu-match-beginning-mark nil)
  (set-marker tpu-match-end-mark nil))

(defun tpu-match-beginning nil
  "Returns the location of the last match beginning."
  (1- (marker-position tpu-match-beginning-mark)))

(defun tpu-match-end nil
  "Returns the location of the last match end."
  (marker-position tpu-match-end-mark))

(defun tpu-check-match nil
  "Returns t if point is between tpu-match markers.
Otherwise sets the tpu-match markers to nil and returns nil."
  ;; make sure 1- marker is in this buffer
  ;;           2- point is at or after beginning marker
  ;;           3- point is before ending marker, or in the case of
  ;;              zero length regions (like bol, or eol) that the
  ;;              beginning, end, and point are equal.
  (cond ((and
	  (equal (marker-buffer tpu-match-beginning-mark) (current-buffer))
	  (>= (point) (1- (marker-position tpu-match-beginning-mark)))
	  (or
	   (< (point) (marker-position tpu-match-end-mark))
	   (and (= (1- (marker-position tpu-match-beginning-mark))
		   (marker-position tpu-match-end-mark))
		(= (marker-position tpu-match-end-mark) (point))))) t)
	(t
	 (tpu-unset-match) nil)))

(defun tpu-show-match-markers nil
  "Show the values of the match markers."
  (interactive)
  (if (markerp tpu-match-beginning-mark)
      (let ((beg (marker-position tpu-match-beginning-mark)))
	(message "(%s, %s) in %s -- current %s in %s"
		 (if beg (1- beg) nil)
		 (marker-position tpu-match-end-mark)
		 (marker-buffer tpu-match-end-mark)
		 (point) (current-buffer)))))


;;;
;;;  Utilities
;;;
(defun tpu-caar (thingy) (car (car thingy)))
(defun tpu-cadr (thingy) (car (cdr thingy)))

(defun tpu-mark nil
  "TPU-edt version of the mark function.
Return the appropriate value of the mark for the current
version of emacs."
  (cond (tpu-lucid-emacs19-p (mark (not zmacs-regions)))
	(tpu-gnu-emacs19-p (and mark-active (mark (not transient-mark-mode))))
	(t (mark))))

(defun tpu-set-mark (pos)
  "TPU-edt verion of the set-mark function.
Sets the mark at POS and activates the region acording to the
current version of emacs."
  (set-mark pos)
  (and tpu-lucid-emacs19-p pos (zmacs-activate-region)))

(defun tpu-string-prompt (prompt history-symbol)
  "Read a string with PROMPT."
  (if tpu-emacs19-p
      (read-from-minibuffer prompt nil nil nil history-symbol)
    (read-string prompt)))

(defvar tpu-last-answer nil "Most recent response to tpu-y-or-n-p.")

(defun tpu-y-or-n-p (prompt &optional not-yes)
  "Prompt for a y or n answer with positive default.
Optional second argument NOT-YES changes default to negative.
Like emacs y-or-n-p, also accepts space as y and DEL as n."
  (message (format "%s[%s]" prompt (if not-yes "n" "y")))
  (let ((doit t))
    (while doit
      (setq doit nil)
      (let ((ans (read-char)))
	(cond ((or (= ans ?y) (= ans ?Y) (= ans ?\ ))
	       (setq tpu-last-answer t))
	      ((or (= ans ?n) (= ans ?N) (= ans ?\C-?))
	       (setq tpu-last-answer nil))
	      ((= ans ?\r) (setq tpu-last-answer (not not-yes)))
	      (t
	       (setq doit t) (beep)
	       (message (format "Please answer y or n.  %s[%s]"
				prompt (if not-yes "n" "y"))))))))
  tpu-last-answer)

(defun tpu-local-set-key (key func)
  "Replace a key in the TPU-edt local key map.
Create the key map if necessary."
  (cond ((not (keymapp tpu-buffer-local-map))
	 (setq tpu-buffer-local-map (if (current-local-map)
					(copy-keymap (current-local-map))
				      (make-sparse-keymap)))
	 (use-local-map tpu-buffer-local-map)))
  (local-set-key key func))

(defun tpu-current-line nil
  "Return the vertical position of point in the selected window.
Top line is 0.  Counts each text line only once, even if it wraps."
  (+ (count-lines (window-start) (point)) (if (= (current-column) 0) 1 0) -1))


;;;
;;;  Breadcrumbs
;;;
(defvar tpu-breadcrumb-plist nil
  "The set of user-defined markers (breadcrumbs), as a plist.")

(defun tpu-drop-breadcrumb (num)
  "Drops a breadcrumb that can be returned to later with goto-breadcrumb."
  (interactive "p")
  (put tpu-breadcrumb-plist num (list (current-buffer) (point)))
  (message "Mark %d set." num))

(defun tpu-goto-breadcrumb (num)
  "Returns to a breadcrumb set with drop-breadcrumb."
  (interactive "p")
  (cond ((get tpu-breadcrumb-plist num)
	 (switch-to-buffer (car (get tpu-breadcrumb-plist num)))
	 (goto-char (tpu-cadr (get tpu-breadcrumb-plist num)))
	 (message "mark %d found." num))
	(t
	 (message "mark %d not found." num))))


;;;
;;;  Miscellaneous
;;;
(defun tpu-change-case (num)
  "Change the case of the character under the cursor or region.
Accepts a prefix argument of the number of characters to invert."
  (interactive "p")
  (cond ((tpu-mark)
	 (let ((beg (region-beginning)) (end (region-end)))
	   (while (> end beg)
	     (funcall (if (= (downcase (char-after beg)) (char-after beg))
			  'upcase-region 'downcase-region)
		      beg (1+ beg))
	     (setq beg (1+ beg)))
	   (tpu-unselect t)))
	((tpu-check-match)
	 (let ((beg (tpu-match-beginning)) (end (tpu-match-end)))
	   (while (> end beg)
	     (funcall (if (= (downcase (char-after beg)) (char-after beg))
			  'upcase-region 'downcase-region)
		      beg (1+ beg))
	     (setq beg (1+ beg)))
	   (tpu-unset-match)))
	(t
	 (while (> num 0)
	   (funcall (if (= (downcase (following-char)) (following-char))
			'upcase-region 'downcase-region)
		    (point) (1+ (point)))
	   (forward-char (if tpu-reverse -1 1))
	   (setq num (1- num))))))

(defun tpu-fill (num)
  "Fill paragraph or marked region.
With argument, fill and justify."
  (interactive "P")
  (cond ((tpu-mark)
	 (fill-region (point) (tpu-mark) num)
	 (tpu-unselect t))
	(t
	 (fill-paragraph num))))

(defun tpu-version nil
  "Print the TPU-edt version number."
  (interactive)
  (message
   "TPU-edt version %s by Rob Riepel (riepel@networking.stanford.edu)"
   tpu-version))

(defun tpu-reset-screen-size (height width)
  "Sets the screen size."
  (interactive "nnew screen height: \nnnew screen width: ")
  (set-screen-height height)
  (set-screen-width width))

(defun tpu-toggle-newline-and-indent nil
  "Toggle between 'newline and indent' and 'simple newline'."
  (interactive)
  (cond (tpu-newline-and-indent-p
         (setq tpu-newline-and-indent-string "")
         (setq tpu-newline-and-indent-p nil)
         (tpu-local-set-key "\C-m" 'newline))
        (t
         (setq tpu-newline-and-indent-string " AutoIndent")
         (setq tpu-newline-and-indent-p t)
         (tpu-local-set-key "\C-m" 'newline-and-indent)))
  (tpu-update-mode-line)
  (and (interactive-p)
       (message "Carriage return inserts a newline%s"
		(if tpu-newline-and-indent-p " and indents." "."))))

(defun tpu-spell-check nil
  "Checks the spelling of the region, or of the entire buffer if no
 region is selected."
  (interactive)
  (cond (tpu-have-ispell
	 (if (tpu-mark) (ispell-region (tpu-mark) (point)) (ispell-buffer)))
	(t
	 (if (tpu-mark) (spell-region (tpu-mark) (point)) (spell-buffer))))
  (if (tpu-mark) (tpu-unselect t)))

(defun tpu-toggle-overwrite-mode nil
  "Switches in and out of overwrite mode"
  (interactive)
  (cond (overwrite-mode
	 (tpu-local-set-key "\177" tpu-saved-delete-func)
	 (overwrite-mode 0))
	(t
	 (setq tpu-saved-delete-func (local-key-binding "\177"))
	 (tpu-local-set-key "\177" 'picture-backward-clear-column)
	 (overwrite-mode 1))))

(defun tpu-special-insert (num)
  "Insert a character or control code according to
its ASCII decimal value."
  (interactive "P")
  (if overwrite-mode (delete-char 1))
  (insert (if num num 0)))

(defun tpu-quoted-insert (num)
  "Read next input character and insert it.
This is useful for inserting control characters."
  (interactive "*p")
  (let ((char (read-char)) )
    (if overwrite-mode (delete-char num))
    (insert-char char num)))


;;;
;;;  TPU line-mode commands
;;;
(defun tpu-include (file)
  "TPU-like include file"
  (interactive "fInclude file: ")
  (save-excursion
    (insert-file file)
    (message "")))

(defun tpu-get (file)
  "TPU-like get file"
  (interactive "FFile to get: ")
  (find-file file))

(defun tpu-what-line nil
  "Tells what line the point is on,
 and the total number of lines in the buffer."
  (interactive)
  (if (eobp)
      (message "You are at the End of Buffer.  The last line is %d."
	       (count-lines 1 (point-max)))
    (message "Line %d of %d"
	     (count-lines 1 (1+ (point)))
	     (count-lines 1 (point-max)))))

(defun tpu-exit nil
  "Exit the way TPU does, save current buffer and ask about others."
  (interactive)
  (if (not (eq (recursion-depth) 0))
      (exit-recursive-edit)
    (progn (save-buffer) (save-buffers-kill-emacs))))

(defun tpu-quit nil
  "Quit the way TPU does, ask to make sure changes should be abandoned."
  (interactive)
  (let ((list (buffer-list))
	(working t))
    (while (and list working)
      (let ((buffer (car list)))
	(if (and (buffer-file-name buffer) (buffer-modified-p buffer))
            (if (tpu-y-or-n-p
		 "Modifications will not be saved, continue quitting? ")
		(kill-emacs t) (setq working nil)))
	(setq list (cdr list))))
    (if working (kill-emacs t))))


;;;
;;;  Command and Function Aliases
;;;
;;;###autoload
(fset 'tpu-edt-mode 'tpu-edt-on)
(fset 'TPU-EDT-MODE 'tpu-edt-on)

;;;###autoload
(fset 'tpu-edt 'tpu-edt-on)
(fset 'TPU-EDT 'tpu-edt-on)

(fset 'exit 'tpu-exit)
(fset 'EXIT 'tpu-exit)

(fset 'Get 'tpu-get)
(fset 'GET 'tpu-get)

(fset 'include 'tpu-include)
(fset 'INCLUDE 'tpu-include)

(fset 'quit 'tpu-quit)
(fset 'QUIT 'tpu-quit)

(fset 'spell 'tpu-spell-check)
(fset 'SPELL 'tpu-spell-check)

(fset 'what\ line 'tpu-what-line)
(fset 'WHAT\ LINE 'tpu-what-line)

(fset 'replace 'tpu-lm-replace)
(fset 'REPLACE 'tpu-lm-replace)

(fset 'help 'tpu-help)
(fset 'HELP 'tpu-help)

(fset 'set\ cursor\ free 'tpu-set-cursor-free)
(fset 'SET\ CURSOR\ FREE 'tpu-set-cursor-free)

(fset 'set\ cursor\ bound 'tpu-set-cursor-bound)
(fset 'SET\ CURSOR\ BOUND 'tpu-set-cursor-bound)

(fset 'set\ scroll\ margins 'tpu-set-scroll-margins)
(fset 'SET\ SCROLL\ MARGINS 'tpu-set-scroll-margins)


;; Around emacs version 18.57, function line-move was renamed to
;; next-line-internal.  If we're running under an older emacs,
;; make next-line-internal equivalent to line-move.

(if (not (fboundp 'next-line-internal)) (fset 'next-line-internal 'line-move))


;;;
;;;  Help
;;;
(defconst tpu-help-keypad-map "\f
          _______________________    _______________________________
         | HELP  |      Do       |  |       |       |       |       |
         |KeyDefs|               |  |       |       |       |       |
         |_______|_______________|  |_______|_______|_______|_______|
          _______________________    _______________________________
         | Find  |Insert |Remove |  | Gold  | HELP  |FndNxt | Del L |
         |       |       |Sto Tex|  |  key  |E-Help | Find  |Undel L|
         |_______|_______|_______|  |_______|_______|_______|_______|
         |Select |Pre Scr|Nex Scr|  | Page  | Sect  |Append | Del W |
         | Reset |Pre Win|Nex Win|  |  Do   | Fill  |Replace|Undel W|
         |_______|_______|_______|  |_______|_______|_______|_______|
                 |Move up|          |Forward|Reverse|Remove | Del C |
                 |  Top  |          |Bottom |  Top  |Insert |Undel C|
          _______|_______|_______   |_______|_______|_______|_______|
         |Mov Lef|Mov Dow|Mov Rig|  | Word  |  EOL  | Char  |       |
         |StaOfLi|Bottom |EndOfLi|  |ChngCas|Del EOL|SpecIns| Enter |
         |_______|_______|_______|  |_______|_______|_______|       |
                                    |     Line      |Select | Subs  |
                                    |   Open Line   | Reset |       |
                                    |_______________|_______|_______|
")

(defconst tpu-help-text "
\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\f

      Control Characters

      ^A  toggle insert and overwrite
      ^B  recall
      ^E  end of line

      ^G  Cancel current operation
      ^H  beginning of line
      ^J  delete previous word

      ^K  learn
      ^L  insert page break
      ^R  remember (during learn), re-center

      ^U  delete to beginning of line
      ^V  quote
      ^W  refresh

      ^Z  exit
    ^X^X  exchange point and mark - useful for checking region boundaries

\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\f
       Gold-<key> Functions

       B     Next Buffer - display the next buffer (all buffers)
       C     Recall - edit and possibly repeat previous commands
       E     Exit - save current buffer and ask about others

       G     Get - load a file into a new edit buffer
       I     Include - include a file in this buffer
       K     Kill Buffer - abandon edits and delete buffer

       M     Buffer Menu - display a list of all buffers
       N     Next File Buffer - display next buffer containing a file
       O     Occur - show following lines containing REGEXP

       Q     Quit - exit without saving anything
       R     Toggle rectangular mode for remove and insert
       S     Search and substitute - line mode REPLACE command

       U     Undo - undo the last edit
       W     Write - save current buffer
       X     Exit - save all modified buffers and exit

\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\f

   *** No more help, use P to view previous screen")

(defvar tpu-help-enter (format "%s" "\eOM"))    ; tpu-help enter key symbol
(defvar tpu-help-return (format "%s" "\r"))     ; tpu-help enter key symbol
(defvar tpu-help-N "N")                         ; tpu-help "N" symbol
(defvar tpu-help-n "n")                         ; tpu-help "n" symbol
(defvar tpu-help-P "P")                         ; tpu-help "P" symbol
(defvar tpu-help-p "p")                         ; tpu-help "p" symbol

(defun tpu-help nil
  "Display TPU-edt help."
  (interactive)
  ;; Save current window configuration
  (save-window-excursion
    ;; Create and fill help buffer if necessary
    (if (not (get-buffer "*TPU-edt Help*"))
	(progn (generate-new-buffer "*TPU-edt Help*")
	       (switch-to-buffer "*TPU-edt Help*")
	       (insert tpu-help-keypad-map)
	       (insert tpu-help-text)
	       (setq buffer-read-only t)))

    ;; Display the help buffer
    (switch-to-buffer "*TPU-edt Help*")
    (delete-other-windows)
    (tpu-move-to-beginning)
    (forward-line 1)
    (tpu-line-to-top-of-window)

    ;; Prompt for keys to describe, based on screen state (split/not split)
    (let ((key nil) (fkey nil) (split nil))
      (while (not (equal tpu-help-return fkey))
	(if split
	    (setq key
		  (read-key-sequence
		   "Press the key you want help on (RET=exit, ENTER=redisplay, N=next, P=prev): "))
	  (setq key
		(read-key-sequence
		 "Press the key you want help on (RET to exit, N next screen, P prev screen): ")))

	;; Process the read key
	;;
	;;    ENTER   -  Display just the help window
	;;    N or n  -  Next help or describe-key screen
	;;    P or p  -  Previous help or describe-key screen
	;;    RETURN  -  Exit from TPU-help
	;;    default -  describe the key
	;;
	(setq fkey (format "%s" key))
	(cond ((equal tpu-help-enter fkey)
	       (setq split nil)
	       (delete-other-windows))
	      ((or (equal tpu-help-N fkey) (equal tpu-help-n fkey))
	       (cond (split
	              (condition-case nil
	        	  (scroll-other-window 8)
	        	(error nil)))
	             (t
	              (forward-page)
	              (forward-line 1)
	              (tpu-line-to-top-of-window))))
	      ((or (equal tpu-help-P fkey) (equal tpu-help-p fkey))
	       (cond (split
	              (condition-case nil
	        	  (scroll-other-window -8)
	        	(error nil)))
	             (t
	              (backward-page 2)
	              (forward-line 1)
	              (tpu-line-to-top-of-window))))
	      ((not (equal tpu-help-return fkey))
	       (setq split t)
	       (describe-key key)
	       ;; If the key is undefined, leave the
	       ;;   message in the mini-buffer for 3 seconds
	       (if (not (key-binding key)) (sit-for 3))))))))


;;;
;;;  Auto-insert
;;;
(defun tpu-insert-escape nil
  "Inserts an escape character, and so becomes the escape-key alias."
  (interactive)
  (insert "\e"))

(defun tpu-insert-formfeed nil
  "Inserts a formfeed character."
  (interactive)
  (insert "\C-L"))


;;;
;;;  Define key
;;;
(defvar tpu-saved-control-r nil "Saved value of Control-r.")

(defun tpu-end-define-macro-key (key)
  "Ends the current macro definition"
  (interactive "kPress the key you want to use to do what was just learned: ")
  (end-kbd-macro nil)
  (global-set-key key last-kbd-macro)
  (global-set-key "\C-r" tpu-saved-control-r))

(defun tpu-define-macro-key nil
  "Bind a set of keystrokes to a single key, or key combination."
  (interactive)
  (setq tpu-saved-control-r (global-key-binding "\C-r"))
  (global-set-key "\C-r" 'tpu-end-define-macro-key)
  (start-kbd-macro nil))


;;;
;;;  Buffers and Windows
;;;
(defun tpu-kill-buffer nil
  "Kills the current buffer.  If tpu-kill-buffers-silently is non-nil,
kills modified buffers without asking."
  (interactive)
  (if tpu-kill-buffers-silently (set-buffer-modified-p nil))
  (kill-buffer (current-buffer)))

(defun tpu-save-all-buffers-kill-emacs nil
  "Save all buffers and exit emacs."
  (interactive)
  (setq trim-versions-without-asking t)
  (save-buffers-kill-emacs t))

(defun tpu-write-current-buffers nil
  "Save all modified buffers without exiting."
  (interactive)
  (save-some-buffers t))

(defun tpu-next-buffer nil
  "Go to next buffer in ring."
  (interactive)
  (switch-to-buffer (car (reverse (buffer-list)))))

(defun tpu-next-file-buffer nil
  "Go to next buffer in ring that is visiting a file."
  (interactive)
  (let ((starting-buffer (buffer-name)))
    (switch-to-buffer (car (reverse (buffer-list))))
    (while (and (not (equal (buffer-name) starting-buffer))
		(not (buffer-file-name)))
      (switch-to-buffer (car (reverse (buffer-list)))))
    (if (equal (buffer-name) starting-buffer) (error "No other buffers."))))

(defun tpu-next-window nil
  "Move to the next window."
  (interactive)
  (if (one-window-p) (message "There is only one window on screen.")
    (other-window 1)))

(defun tpu-previous-window nil
  "Move to the previous window."
  (interactive)
  (if (one-window-p) (message "There is only one window on screen.")
    (select-window (previous-window))))


;;;
;;;  Search
;;;
(defun tpu-toggle-regexp nil
  "Switches in and out of regular expression search and replace mode."
  (interactive)
  (setq tpu-regexp-p (not tpu-regexp-p))
  (tpu-set-search)
  (and (interactive-p)
       (message "Regular expression search and substitute %sabled."
		(if tpu-regexp-p "en" "dis"))))

(defun tpu-regexp-prompt (prompt)
  "Read a string, adding 'RE' to the prompt if tpu-regexp-p is set."
  (let ((re-prompt (concat (if tpu-regexp-p "RE ") prompt)))
    (if tpu-emacs19-p
	(read-from-minibuffer re-prompt nil nil nil 'tpu-regexp-prompt-hist)
      (read-string re-prompt))))

(defun tpu-search nil
  "Search for a string or regular expression.
The search is performed in the current direction."
  (interactive)
  (tpu-set-search)
  (tpu-search-internal ""))

(defun tpu-search-forward nil
  "Search for a string or regular expression.
The search is begins in the forward direction."
  (interactive)
  (setq tpu-searching-forward t)
  (tpu-set-search t)
  (tpu-search-internal ""))

(defun tpu-search-reverse nil
  "Search for a string or regular expression.
The search is begins in the reverse direction."
  (interactive)
  (setq tpu-searching-forward nil)
  (tpu-set-search t)
  (tpu-search-internal ""))

(defun tpu-search-again nil
  "Search for the same string or regular expression as last time.
The search is performed in the current direction."
  (interactive)
  (tpu-search-internal tpu-search-last-string))

;;  tpu-set-search defines the search functions used by the TPU-edt internal
;;  search function.  It should be called whenever the direction changes, or
;;  the regular expression mode is turned on or off.  It can also be called
;;  to ensure that the next search will be in the current direction.  It is
;;  called from:

;;       tpu-advance              tpu-backup
;;       tpu-toggle-regexp        tpu-toggle-search-direction (t)
;;       tpu-search               tpu-lm-replace
;;       tpu-search-forward (t)   tpu-search-reverse (t)

(defun tpu-set-search (&optional arg)
  "Set the search functions and set the search direction to the current
direction.  If an argument is specified, don't set the search direction."
  (if (not arg) (setq tpu-searching-forward (if tpu-advance t nil)))
  (cond (tpu-searching-forward
	 (cond (tpu-regexp-p
		(fset 'tpu-emacs-search 're-search-forward)
		(fset 'tpu-emacs-rev-search 're-search-backward))
	       (t
		(fset 'tpu-emacs-search 'search-forward)
		(fset 'tpu-emacs-rev-search 'search-backward))))
	(t
	 (cond (tpu-regexp-p
		(fset 'tpu-emacs-search 're-search-backward)
		(fset 'tpu-emacs-rev-search 're-search-forward))
	       (t
		(fset 'tpu-emacs-search 'search-backward)
		(fset 'tpu-emacs-rev-search 'search-forward))))))

(defun tpu-search-internal (pat &optional quiet)
  "Search for a string or regular expression."
  (setq tpu-search-last-string
	(if (not (string= "" pat)) pat (tpu-regexp-prompt "Search: ")))

  (tpu-unset-match)
  (tpu-adjust-search)

  (cond ((tpu-emacs-search tpu-search-last-string nil t)
	 (tpu-set-match) (goto-char (tpu-match-beginning)))

	(t
	 (tpu-adjust-search t)
	 (let ((found nil) (pos nil))
	   (save-excursion
	     (let ((tpu-searching-forward (not tpu-searching-forward)))
	       (tpu-adjust-search)
	       (setq found (tpu-emacs-rev-search tpu-search-last-string nil t))
	       (setq pos (match-beginning 0))))

	   (cond (found
		  (cond ((tpu-y-or-n-p
			  (format "Found in %s direction.  Go there? "
				  (if tpu-searching-forward "reverse" "forward")))
			 (goto-char pos) (tpu-set-match)
			 (tpu-toggle-search-direction))))

		 (t
		  (if (not quiet)
		      (message
		       "%sSearch failed: \"%s\""
		       (if tpu-regexp-p "RE " "") tpu-search-last-string))))))))

(fset 'tpu-search-internal-core (symbol-function 'tpu-search-internal))

(defun tpu-adjust-search (&optional arg)
  "For forward searches, move forward a character before searching,
and backward a character after a failed search.  Arg means end of search."
  (if tpu-searching-forward
      (cond (arg (if (not (bobp)) (forward-char -1)))
	    (t (if (not (eobp)) (forward-char 1))))))

(defun tpu-toggle-search-direction nil
  "Toggle the TPU-edt search direction.
Used for reversing a search in progress."
  (interactive)
  (setq tpu-searching-forward (not tpu-searching-forward))
  (tpu-set-search t)
  (and (interactive-p)
       (message "Searching %sward."
		(if tpu-searching-forward "for" "back"))))


;;;
;;;  Select / Unselect
;;;
(defun tpu-select (&optional quiet)
  "Sets the mark to define one end of a region."
  (interactive "P")
  (cond ((tpu-mark)
	 (tpu-unselect quiet))
	(t
	 (tpu-set-mark (point))
	 (tpu-update-mode-line)
	 (if (not quiet) (message "Move the text cursor to select text.")))))

(defun tpu-unselect (&optional quiet)
  "Removes the mark to unselect the current region."
  (interactive "P")
  (setq mark-ring nil)
  (tpu-set-mark nil)
  (tpu-update-mode-line)
  (if (not quiet) (message "Selection canceled.")))


;;;
;;;  Delete / Cut
;;;
(defun tpu-toggle-rectangle nil
  "Toggle rectangular mode for remove and insert."
  (interactive)
  (setq tpu-rectangular-p (not tpu-rectangular-p))
  (setq tpu-rectangle-string (if tpu-rectangular-p " Rect" ""))
  (tpu-update-mode-line)
  (and (interactive-p)
       (message "Rectangular cut and paste %sabled."
		(if tpu-rectangular-p "en" "dis"))))

(defun tpu-arrange-rectangle nil
  "Adjust point and mark to mark upper left and lower right
corners of a rectangle."
  (let ((mc (current-column))
	(pc (progn (exchange-point-and-mark) (current-column))))

    (cond ((> (point) (tpu-mark))                      ; point on lower line
	   (cond ((> pc mc)                        ; point @  lower-right
		  (exchange-point-and-mark))       ; point -> upper-left

		 (t	                           ; point @  lower-left
		  (move-to-column-force mc)        ; point -> lower-right
		  (exchange-point-and-mark)        ; point -> upper-right
		  (move-to-column-force pc))))     ; point -> upper-left

	  (t                                       ; point on upper line
	   (cond ((> pc mc)                        ; point @  upper-right
		  (move-to-column-force mc)        ; point -> upper-left
		  (exchange-point-and-mark)        ; point -> lower-left
		  (move-to-column-force pc)        ; point -> lower-right
		  (exchange-point-and-mark)))))))  ; point -> upper-left

(defun tpu-cut-text nil
  "Delete the selected region.
The text is saved for the tpu-paste command."
  (interactive)
  (cond ((tpu-mark)
	 (cond (tpu-rectangular-p
		(tpu-arrange-rectangle)
		(picture-clear-rectangle (point) (tpu-mark) (not overwrite-mode))
		(tpu-unselect t))
	       (t
		(setq tpu-last-deleted-region
		      (buffer-substring (tpu-mark) (point)))
		(delete-region (tpu-mark) (point))
		(tpu-unselect t))))
	((tpu-check-match)
	 (let ((beg (tpu-match-beginning)) (end (tpu-match-end)))
	   (setq tpu-last-deleted-region (buffer-substring beg end))
	   (delete-region beg end)
	   (tpu-unset-match)))
	(t
	 (error "No selection active."))))

(defun tpu-store-text nil
  "Copy the selected region to the cut buffer without deleting it.
The text is saved for the tpu-paste command."
  (interactive)
  (cond ((tpu-mark)
	 (cond (tpu-rectangular-p
		(save-excursion
		  (tpu-arrange-rectangle)
		  (setq picture-killed-rectangle
			(extract-rectangle (point) (tpu-mark))))
		(tpu-unselect t))
	       (t
		(setq tpu-last-deleted-region
		      (buffer-substring (tpu-mark) (point)))
		(tpu-unselect t))))
	((tpu-check-match)
	 (setq tpu-last-deleted-region
	       (buffer-substring (tpu-match-beginning) (tpu-match-end)))
	 (tpu-unset-match))
	(t
	 (error "No selection active."))))

(defun tpu-cut (arg)
  "Copy selected region to the cut buffer.  In the absence of an
argument, delete the selected region too."
  (interactive "P")
  (if arg (tpu-store-text) (tpu-cut-text)))

(defun tpu-append-region (arg)
  "Append selected region to the tpu-cut buffer.  In the absence of an
argument, delete the selected region too."
  (interactive "P")
  (cond ((tpu-mark)
	 (let ((beg (region-beginning)) (end (region-end)))
	   (setq tpu-last-deleted-region
		 (concat tpu-last-deleted-region
			 (buffer-substring beg end)))
	   (if (not arg) (delete-region beg end))
	   (tpu-unselect t)))
	((tpu-check-match)
	 (let ((beg (tpu-match-beginning)) (end (tpu-match-end)))
	   (setq tpu-last-deleted-region
		 (concat tpu-last-deleted-region
			 (buffer-substring beg end)))
	   (if (not arg) (delete-region beg end))
	   (tpu-unset-match)))
	(t
	 (error "No selection active."))))

(defun tpu-delete-current-line (num)
  "Delete one or specified number of lines after point.
This includes the newline character at the end of each line.
They are saved for the TPU-edt undelete-lines command."
  (interactive "p")
  (let ((beg (point)))
    (forward-line num)
    (if (not (eq (preceding-char) ?\n))
        (insert "\n"))
    (setq tpu-last-deleted-lines
          (buffer-substring beg (point)))
    (delete-region beg (point))))

(defun tpu-delete-to-eol (num)
  "Delete text up to end of line.
With argument, delete up to to Nth line-end past point.
They are saved for the TPU-edt undelete-lines command."
  (interactive "p")
  (let ((beg (point)))
    (forward-char 1)
    (end-of-line num)
    (setq tpu-last-deleted-lines
          (buffer-substring beg (point)))
    (delete-region beg (point))))

(defun tpu-delete-to-bol (num)
  "Delete text back to beginning of line.
With argument, delete up to to Nth line-end past point.
They are saved for the TPU-edt undelete-lines command."
  (interactive "p")
  (let ((beg (point)))
    (tpu-next-beginning-of-line num)
    (setq tpu-last-deleted-lines
          (buffer-substring (point) beg))
    (delete-region (point) beg)))

(defun tpu-delete-current-word (num)
  "Delete one or specified number of words after point.
They are saved for the TPU-edt undelete-words command."
  (interactive "p")
  (let ((beg (point)))
    (tpu-forward-to-word num)
    (setq tpu-last-deleted-words
          (buffer-substring beg (point)))
    (delete-region beg (point))))

(defun tpu-delete-previous-word (num)
  "Delete one or specified number of words before point.
They are saved for the TPU-edt undelete-words command."
  (interactive "p")
  (let ((beg (point)))
    (tpu-backward-to-word num)
    (setq tpu-last-deleted-words
          (buffer-substring (point) beg))
    (delete-region beg (point))))

(defun tpu-delete-current-char (num)
  "Delete one or specified number of characters after point.  The last
character deleted is saved for the TPU-edt undelete-char command."
  (interactive "p")
  (while (and (> num 0) (not (eobp)))
    (setq tpu-last-deleted-char (char-after (point)))
    (cond (overwrite-mode
	   (picture-clear-column 1)
	   (forward-char 1))
	  (t
	   (delete-char 1)))
    (setq num (1- num))))


;;;
;;;  Undelete / Paste
;;;
(defun tpu-paste (num)
  "Insert the last region or rectangle of killed text.
With argument reinserts the text that many times."
  (interactive "p")
  (while (> num 0)
    (cond (tpu-rectangular-p
	   (let ((beg (point)))
	     (save-excursion
	       (picture-yank-rectangle (not overwrite-mode))
	       (message ""))
	     (goto-char beg)))
	  (t
	   (insert tpu-last-deleted-region)))
    (setq num (1- num))))

(defun tpu-undelete-lines (num)
  "Insert lines deleted by last TPU-edt line-deletion command.
With argument reinserts lines that many times."
  (interactive "p")
  (let ((beg (point)))
    (while (> num 0)
      (insert tpu-last-deleted-lines)
      (setq num (1- num)))
    (goto-char beg)))

(defun tpu-undelete-words (num)
  "Insert words deleted by last TPU-edt word-deletion command.
With argument reinserts words that many times."
  (interactive "p")
  (let ((beg (point)))
    (while (> num 0)
      (insert tpu-last-deleted-words)
      (setq num (1- num)))
    (goto-char beg)))

(defun tpu-undelete-char (num)
  "Insert character deleted by last TPU-edt character-deletion command.
With argument reinserts the character that many times."
  (interactive "p")
  (while (> num 0)
    (if overwrite-mode (prog1 (forward-char -1) (delete-char 1)))
    (insert tpu-last-deleted-char)
    (forward-char -1)
    (setq num (1- num))))


;;;
;;;  Replace and Substitute
;;;
(defun tpu-replace nil
  "Replace the selected region with the contents of the cut buffer."
  (interactive)
  (cond ((tpu-mark)
	 (let ((beg (region-beginning)) (end (region-end)))
	   (setq tpu-last-replaced-text (buffer-substring beg end))
	   (delete-region beg end)
	   (insert tpu-last-deleted-region)
	   (tpu-unselect t)))
	((tpu-check-match)
	 (let ((beg (tpu-match-beginning)) (end (tpu-match-end)))
	   (setq tpu-last-replaced-text (buffer-substring beg end))
	   (replace-match tpu-last-deleted-region
			  (not case-replace) (not tpu-regexp-p))
	   (tpu-unset-match)))
	(t
	 (error "No selection active."))))

(defun tpu-substitute (num)
  "Replace the selected region with the contents of the cut buffer, and
repeat most recent search.  A numeric argument serves as a repeat count.
A negative argument means replace all occurrences of the search string."
  (interactive "p")
  (cond ((or (tpu-mark) (tpu-check-match))
	 (while (and (not (= num 0)) (or (tpu-mark) (tpu-check-match)))
	   (let ((beg (point)))
	     (tpu-replace)
	     (if tpu-searching-forward (forward-char -1) (goto-char beg))
	     (if (= num 1) (tpu-search-internal tpu-search-last-string)
	       (tpu-search-internal-core tpu-search-last-string)))
	   (setq num (1- num))))
	(t
	 (error "No selection active."))))

(defun tpu-lm-replace (from to)
  "Interactively search for OLD-string and substitute NEW-string."
  (interactive (list (tpu-regexp-prompt "Old String: ")
		     (tpu-regexp-prompt "New String: ")))

  (let ((doit t) (strings 0))

    ;; Can't replace null strings
    (if (string= "" from) (error "No string to replace."))

    ;; Find the first occurrence
    (tpu-set-search)
    (tpu-search-internal from t)

    ;; Loop on replace question - yes, no, all, last, or quit.
    (while doit
      (if (not (tpu-check-match)) (setq doit nil)
	(progn (message "Replace? Type Yes, No, All, Last, or Quit: ")
	       (let ((ans (read-char)))

		 (cond ((or (= ans ?y) (= ans ?Y) (= ans ?\r) (= ans ?\ ))
			(let ((beg (point)))
			  (replace-match to (not case-replace) (not tpu-regexp-p))
			  (setq strings (1+ strings))
			  (if tpu-searching-forward (forward-char -1) (goto-char beg)))
			(tpu-search-internal from t))

		       ((or (= ans ?n) (= ans ?N) (= ans ?\C-?))
			(tpu-search-internal from t))

		       ((or (= ans ?a) (= ans ?A))
			(save-excursion
			  (let ((beg (point)))
			    (replace-match to (not case-replace) (not tpu-regexp-p))
			    (setq strings (1+ strings))
			    (if tpu-searching-forward (forward-char -1) (goto-char beg)))
			  (tpu-search-internal-core from t)
			  (while (tpu-check-match)
			    (let ((beg (point)))
			      (replace-match to (not case-replace) (not tpu-regexp-p))
			      (setq strings (1+ strings))
			      (if tpu-searching-forward (forward-char -1) (goto-char beg)))
			    (tpu-search-internal-core from t)))
			(setq doit nil))

		       ((or (= ans ?l) (= ans ?L))
			(let ((beg (point)))
			  (replace-match to (not case-replace) (not tpu-regexp-p))
			  (setq strings (1+ strings))
			  (if tpu-searching-forward (forward-char -1) (goto-char beg)))
			(setq doit nil))

		       ((or (= ans ?q) (= ans ?Q))
			(setq doit nil)))))))

    (message "Replaced %s occurrence%s." strings
	     (if (not (= 1 strings)) "s" ""))))

(defun tpu-emacs-replace (&optional dont-ask)
  "A TPU-edt interface to the emacs replace functions.  If TPU-edt is
currently in regular expression mode, the emacs regular expression
replace functions are used.  If an argument is supplied, replacements
are performed without asking.  Only works in forward direction."
  (interactive "P")
  (cond (dont-ask
	 (setq current-prefix-arg nil)
	 (call-interactively
	  (if tpu-regexp-p 'replace-regexp 'replace-string)))
	(t
	 (call-interactively
	  (if tpu-regexp-p 'query-replace-regexp 'query-replace)))))

(defun tpu-add-at-bol (text)
  "Add text to the beginning of each line in a region,
or each line in the entire buffer if no region is selected."
  (interactive
   (list (tpu-string-prompt "String to add: " 'tpu-add-at-bol-hist)))
  (if (string= "" text) (error "No string specified."))
  (cond ((tpu-mark)
	 (save-excursion
	   (if (> (point) (tpu-mark)) (exchange-point-and-mark))
	   (while (and (< (point) (tpu-mark)) (re-search-forward "^" (tpu-mark) t))
	     (if (< (point) (tpu-mark)) (replace-match text))))
	 (tpu-unselect t))
	(t
	 (save-excursion
	   (goto-char (point-min))
	   (while (and (re-search-forward "^" nil t) (not (eobp)))
	     (replace-match text))))))

(defun tpu-add-at-eol (text)
  "Add text to the end of each line in a region,
or each line of the entire buffer if no region is selected."
  (interactive
   (list (tpu-string-prompt "String to add: " 'tpu-add-at-eol-hist)))
  (if (string= "" text) (error "No string specified."))
  (cond ((tpu-mark)
	 (save-excursion
	   (if (> (point) (tpu-mark)) (exchange-point-and-mark))
	   (while (< (point) (tpu-mark))
	     (end-of-line)
	     (if (<= (point) (tpu-mark)) (insert text))
	     (forward-line)))
	 (tpu-unselect t))
	(t
	 (save-excursion
	   (goto-char (point-min))
	   (while (not (eobp))
	     (end-of-line) (insert text) (forward-line))))))

(defun tpu-trim-line-ends nil
  "Removes trailing whitespace from every line in the buffer."
  (interactive)
  (picture-clean))


;;;
;;;  Movement by character
;;;
(defun tpu-char (num)
  "Move to the next character in the current direction.
A repeat count means move that many characters."
  (interactive "p")
  (if tpu-advance (tpu-forward-char num) (tpu-backward-char num)))

(defun tpu-forward-char (num)
  "Move right ARG characters (left if ARG is negative)."
  (interactive "p")
  (forward-char num))

(defun tpu-backward-char (num)
  "Move left ARG characters (right if ARG is negative)."
  (interactive "p")
  (backward-char num))


;;;
;;;  Movement by word
;;;
(defconst tpu-word-separator-list '()
  "List of additional word separators.")
(defconst tpu-skip-chars "^ \t"
  "Characters to skip when moving by word.
Additional word separators are added to this string.")

(defun tpu-word (num)
  "Move to the beginning of the next word in the current direction.
A repeat count means move that many words."
  (interactive "p")
  (if tpu-advance (tpu-forward-to-word num) (tpu-backward-to-word num)))

(defun tpu-forward-to-word (num)
  "Move forward until encountering the beginning of a word.
With argument, do this that many times."
  (interactive "p")
  (while (and (> num 0) (not (eobp)))
    (let* ((beg (point))
	   (end (prog2 (end-of-line) (point) (goto-char beg))))
      (cond ((eolp)
	     (forward-char 1))
	    ((memq (char-after (point)) tpu-word-separator-list)
	     (forward-char 1)
	     (skip-chars-forward " \t" end))
	    (t
	     (skip-chars-forward tpu-skip-chars end)
	     (skip-chars-forward " \t" end))))
    (setq num (1- num))))

(defun tpu-backward-to-word (num)
  "Move backward until encountering the beginning of a word.
With argument, do this that many times."
  (interactive "p")
  (while (and (> num 0) (not (bobp)))
    (let* ((beg (point))
	   (end (prog2 (beginning-of-line) (point) (goto-char beg))))
      (cond ((bolp)
	     ( forward-char -1))
	    ((memq (char-after (1- (point)))  tpu-word-separator-list)
	     (forward-char -1))
	    (t
	     (skip-chars-backward " \t" end)
	     (skip-chars-backward tpu-skip-chars end)
	     (if (and (not (bolp)) (= ?  (char-syntax (char-after (point)))))
		 (forward-char -1)))))
    (setq num (1- num))))

(defun tpu-add-word-separators (separators)
  "Add new word separators for TPU-edt word commands."
  (interactive "sSeparators: ")
  (let* ((n 0) (length (length separators)))
    (while (< n length)
      (let ((char (aref separators n))
	    (ss (substring separators n (1+ n))))
	(cond ((not (memq char tpu-word-separator-list))
	       (setq tpu-word-separator-list
		     (append ss tpu-word-separator-list))
	       (cond ((= char ?-)
		      (setq tpu-skip-chars (concat tpu-skip-chars "\\-")))
		     ((= char ?\\)
		      (setq tpu-skip-chars (concat tpu-skip-chars "\\\\")))
		     ((= char ?^)
		      (setq tpu-skip-chars (concat tpu-skip-chars "\\^")))
		     (t
		      (setq tpu-skip-chars (concat tpu-skip-chars ss))))))
	(setq n (1+ n))))))

(defun tpu-reset-word-separators nil
  "Reset word separators to default value."
  (interactive)
  (setq tpu-word-separator-list nil)
  (setq tpu-skip-chars "^ \t"))

(defun tpu-set-word-separators (separators)
  "Set new word separators for TPU-edt word commands."
  (interactive "sSeparators: ")
  (tpu-reset-word-separators)
  (tpu-add-word-separators separators))


;;;
;;;  Movement by line
;;;
(defun tpu-next-line (num)
  "Move to next line.
Prefix argument serves as a repeat count."
  (interactive "p")
  (next-line-internal num)
  (setq this-command 'next-line))

(defun tpu-previous-line (num)
  "Move to previous line.
Prefix argument serves as a repeat count."
  (interactive "p")
  (next-line-internal (- num))
  (setq this-command 'previous-line))

(defun tpu-next-beginning-of-line (num)
  "Move to beginning of line; if at beginning, move to beginning of next line.
Accepts a prefix argument for the number of lines to move."
  (interactive "p")
  (backward-char 1)
  (forward-line (- 1 num)))

(defun tpu-end-of-line (num)
  "Move to the next end of line in the current direction.
A repeat count means move that many lines."
  (interactive "p")
  (if tpu-advance (tpu-next-end-of-line num) (tpu-previous-end-of-line num)))

(defun tpu-next-end-of-line (num)
  "Move to end of line; if at end, move to end of next line.
Accepts a prefix argument for the number of lines to move."
  (interactive "p")
  (forward-char 1)
  (end-of-line num))

(defun tpu-previous-end-of-line (num)
  "Move EOL upward.
Accepts a prefix argument for the number of lines to move."
  (interactive "p")
  (end-of-line (- 1 num)))

(defun tpu-current-end-of-line nil
  "Move point to end of current line."
  (interactive)
  (let ((beg (point)))
    (end-of-line)
    (if (= beg (point)) (message "You are already at the end of a line."))))

(defun tpu-line (num)
  "Move to the beginning of the next line in the current direction.
A repeat count means move that many lines."
  (interactive "p")
  (if tpu-advance (tpu-forward-line num) (tpu-backward-line num)))

(defun tpu-forward-line (num)
  "Move to beginning of next line.
Prefix argument serves as a repeat count."
  (interactive "p")
  (forward-line num))

(defun tpu-backward-line (num)
  "Move to beginning of previous line.
Prefix argument serves as repeat count."
  (interactive "p")
  (forward-line (- num)))


;;;
;;;  Movement by paragraph
;;;
(defun tpu-paragraph (num)
  "Move to the next paragraph in the current direction.
A repeat count means move that many paragraphs."
  (interactive "p")
  (if tpu-advance
      (tpu-next-paragraph num) (tpu-previous-paragraph num)))

(defun tpu-next-paragraph (num)
  "Move to beginning of the next paragraph.
Accepts a prefix argument for the number of paragraphs."
  (interactive "p")
  (beginning-of-line)
  (while (and (not (eobp)) (> num 0))
    (if (re-search-forward "^[ \t]*$" nil t)
	(if (re-search-forward "[^ \t\n]" nil t)
	    (goto-char (match-beginning 0))
	  (goto-char (point-max))))
    (setq num (1- num)))
  (beginning-of-line))


(defun tpu-previous-paragraph (num)
  "Move to beginning of previous paragraph.
Accepts a prefix argument for the number of paragraphs."
  (interactive "p")
  (end-of-line)
  (while (and (not (bobp)) (> num 0))
    (if (not (and (re-search-backward "^[ \t]*$" nil t)
		  (re-search-backward "[^ \t\n]" nil t)
		  (re-search-backward "^[ \t]*$" nil t)
		  (progn (re-search-forward "[^ \t\n]" nil t)
			 (goto-char (match-beginning 0)))))
	(goto-char (point-min)))
    (setq num (1- num)))
  (beginning-of-line))


;;;
;;;  Movement by page
;;;
(defun tpu-page (num)
  "Move to the next page in the current direction.
A repeat count means move that many pages."
  (interactive "p")
  (if tpu-advance (forward-page num) (backward-page num))
  (if (eobp) (recenter -1)))


;;;
;;;  Scrolling and movement within the buffer
;;;
(defun tpu-scroll-window (num)
  "Scroll the display to the next section in the current direction.
A repeat count means scroll that many sections."
  (interactive "p")
  (if tpu-advance (tpu-scroll-window-up num) (tpu-scroll-window-down num)))

(defun tpu-scroll-window-down (num)
  "Scroll the display down to the next section.
A repeat count means scroll that many sections."
  (interactive "p")
  (let* ((beg (tpu-current-line))
	 (height (1- (window-height)))
	 (lines (* num (/ (* height tpu-percent-scroll) 100))))
    (next-line-internal (- lines))
    (if (> lines beg) (recenter 0))))

(defun tpu-scroll-window-up (num)
  "Scroll the display up to the next section.
A repeat count means scroll that many sections."
  (interactive "p")
  (let* ((beg (tpu-current-line))
	 (height (1- (window-height)))
	 (lines (* num (/ (* height tpu-percent-scroll) 100))))
    (next-line-internal lines)
    (if (>= (+ lines beg) height) (recenter -1))))

(defun tpu-pan-right (num)
  "Pan right tpu-pan-columns (16 by default).
Accepts a prefix argument for the number of tpu-pan-columns to scroll."
  (interactive "p")
  (scroll-left (* tpu-pan-columns num)))

(defun tpu-pan-left (num)
  "Pan left tpu-pan-columns (16 by default).
Accepts a prefix argument for the number of tpu-pan-columns to scroll."
  (interactive "p")
  (scroll-right (* tpu-pan-columns num)))

(defun tpu-move-to-beginning nil
  "Move cursor to the beginning of buffer, but don't set the mark."
  (interactive)
  (goto-char (point-min)))

(defun tpu-move-to-end nil
  "Move cursor to the end of buffer, but don't set the mark."
  (interactive)
  (goto-char (point-max))
  (recenter -1))

(defun tpu-goto-percent (perc)
  "Move point to ARG percentage of the buffer."
  (interactive "NGoto-percentage: ")
  (if (or (> perc 100) (< perc 0))
      (error "Percentage %d out of range 0 < percent < 100" perc)
    (goto-char (/ (* (point-max) perc) 100))))

(defun tpu-beginning-of-window nil
  "Move cursor to top of window."
  (interactive)
  (move-to-window-line 0))

(defun tpu-end-of-window nil
  "Move cursor to bottom of window."
  (interactive)
  (move-to-window-line -1))

(defun tpu-line-to-bottom-of-window nil
  "Move the current line to the bottom of the window."
  (interactive)
  (recenter -1))

(defun tpu-line-to-top-of-window nil
  "Move the current line to the top of the window."
  (interactive)
  (recenter 0))


;;;
;;;  Direction
;;;
(defun tpu-advance-direction nil
  "Set TPU Advance mode so keypad commands move forward."
  (interactive)
  (setq tpu-direction-string " Advance")
  (setq tpu-advance t)
  (setq tpu-reverse nil)
  (tpu-set-search)
  (tpu-update-mode-line))

(defun tpu-backup-direction nil
  "Set TPU Backup mode so keypad commands move backward."
  (interactive)
  (setq tpu-direction-string " Reverse")
  (setq tpu-advance nil)
  (setq tpu-reverse t)
  (tpu-set-search)
  (tpu-update-mode-line))


;;;
;;;  Define keymaps
;;;
(define-key global-map "\e[" CSI-map)                         ; CSI map
(define-key global-map "\eO" SS3-map)                         ; SS3 map
(define-key SS3-map "P" GOLD-map)                             ; GOLD map
(define-key GOLD-map "\e[" GOLD-CSI-map)                      ; GOLD-CSI map
(define-key GOLD-map "\eO" GOLD-SS3-map)                      ; GOLD-SS3 map


;;;
;;;  CSI-map key definitions
;;;
(define-key CSI-map "A" 'tpu-previous-line)                   ; up
(define-key CSI-map "B" 'tpu-next-line)                       ; down
(define-key CSI-map "D" 'tpu-backward-char)                   ; left
(define-key CSI-map "C" 'tpu-forward-char)                    ; right

(define-key CSI-map "1~" 'tpu-search)                         ; Find
(define-key CSI-map "2~" 'tpu-paste)                          ; Insert Here
(define-key CSI-map "3~" 'tpu-cut)                            ; Remove
(define-key CSI-map "4~" 'tpu-select)                         ; Select
(define-key CSI-map "5~" 'tpu-scroll-window-down)             ; Prev Screen
(define-key CSI-map "6~" 'tpu-scroll-window-up)               ; Next Screen

(define-key CSI-map "11~" 'nil)                               ; F1
(define-key CSI-map "12~" 'nil)                               ; F2
(define-key CSI-map "13~" 'nil)                               ; F3
(define-key CSI-map "14~" 'nil)                               ; F4
(define-key CSI-map "15~" 'nil)                               ; F5
(define-key CSI-map "17~" 'nil)                               ; F6
(define-key CSI-map "18~" 'nil)                               ; F7
(define-key CSI-map "19~" 'nil)                               ; F8
(define-key CSI-map "20~" 'nil)                               ; F9
(define-key CSI-map "21~" 'tpu-exit)                          ; F10
(define-key CSI-map "23~" 'tpu-insert-escape)                 ; F11 (ESC)
(define-key CSI-map "24~" 'tpu-next-beginning-of-line)        ; F12 (BS)
(define-key CSI-map "25~" 'tpu-delete-previous-word)          ; F13 (LF)
(define-key CSI-map "26~" 'tpu-toggle-overwrite-mode)         ; F14
(define-key CSI-map "28~" 'tpu-help)                          ; HELP
(define-key CSI-map "29~" 'execute-extended-command)          ; DO
(define-key CSI-map "31~" 'tpu-goto-breadcrumb)               ; F17
(define-key CSI-map "32~" 'nil)                               ; F18
(define-key CSI-map "33~" 'nil)                               ; F19
(define-key CSI-map "34~" 'nil)                               ; F20


;;;
;;;  SS3-map key definitions
;;;
(define-key SS3-map "A" 'tpu-previous-line)                   ; up
(define-key SS3-map "B" 'tpu-next-line)                       ; down
(define-key SS3-map "C" 'tpu-forward-char)                    ; right
(define-key SS3-map "D" 'tpu-backward-char)                   ; left

(define-key SS3-map "Q" 'tpu-help)                            ; PF2
(define-key SS3-map "R" 'tpu-search-again)                    ; PF3
(define-key SS3-map "S" 'tpu-delete-current-line)             ; PF4
(define-key SS3-map "p" 'tpu-line)                            ; KP0
(define-key SS3-map "q" 'tpu-word)                            ; KP1
(define-key SS3-map "r" 'tpu-end-of-line)                     ; KP2
(define-key SS3-map "s" 'tpu-char)                            ; KP3
(define-key SS3-map "t" 'tpu-advance-direction)               ; KP4
(define-key SS3-map "u" 'tpu-backup-direction)                ; KP5
(define-key SS3-map "v" 'tpu-cut)                             ; KP6
(define-key SS3-map "w" 'tpu-page)                            ; KP7
(define-key SS3-map "x" 'tpu-scroll-window)                   ; KP8
(define-key SS3-map "y" 'tpu-append-region)                   ; KP9
(define-key SS3-map "m" 'tpu-delete-current-word)             ; KP-
(define-key SS3-map "l" 'tpu-delete-current-char)             ; KP,
(define-key SS3-map "n" 'tpu-select)                          ; KP.
(define-key SS3-map "M" 'newline)                             ; KPenter


;;;
;;;  GOLD-map key definitions
;;;
(define-key GOLD-map "\C-A" 'tpu-toggle-overwrite-mode)       ; ^A
(define-key GOLD-map "\C-B" 'nil)                             ; ^B
(define-key GOLD-map "\C-C" 'nil)                             ; ^C
(define-key GOLD-map "\C-D" 'nil)                             ; ^D
(define-key GOLD-map "\C-E" 'nil)                             ; ^E
(define-key GOLD-map "\C-F" 'set-visited-file-name)           ; ^F
(define-key GOLD-map "\C-g" 'keyboard-quit)                   ; safety first
(define-key GOLD-map "\C-h" 'delete-other-windows)            ; BS
(define-key GOLD-map "\C-i" 'other-window)                    ; TAB
(define-key GOLD-map "\C-J" 'nil)                             ; ^J
(define-key GOLD-map "\C-K" 'tpu-define-macro-key)            ; ^K
(define-key GOLD-map "\C-l" 'downcase-region)                 ; ^L
(define-key GOLD-map "\C-M" 'nil)                             ; ^M
(define-key GOLD-map "\C-N" 'nil)                             ; ^N
(define-key GOLD-map "\C-O" 'nil)                             ; ^O
(define-key GOLD-map "\C-P" 'nil)                             ; ^P
(define-key GOLD-map "\C-Q" 'nil)                             ; ^Q
(define-key GOLD-map "\C-R" 'nil)                             ; ^R
(define-key GOLD-map "\C-S" 'nil)                             ; ^S
(define-key GOLD-map "\C-T" 'tpu-toggle-control-keys)         ; ^T
(define-key GOLD-map "\C-u" 'upcase-region)                   ; ^U
(define-key GOLD-map "\C-V" 'nil)                             ; ^V
(define-key GOLD-map "\C-w" 'tpu-write-current-buffers)       ; ^W
(define-key GOLD-map "\C-X" 'nil)                             ; ^X
(define-key GOLD-map "\C-Y" 'nil)                             ; ^Y
(define-key GOLD-map "\C-Z" 'nil)                             ; ^Z
(define-key GOLD-map " " 'undo)                               ; SPC
(define-key GOLD-map "!" 'nil)                                ; !
(define-key GOLD-map "#" 'nil)                                ; #
(define-key GOLD-map "$" 'tpu-add-at-eol)                     ; $
(define-key GOLD-map "%" 'tpu-goto-percent)                   ; %
(define-key GOLD-map "&" 'nil)                                ; &
(define-key GOLD-map "(" 'nil)                                ; (
(define-key GOLD-map ")" 'nil)                                ; )
(define-key GOLD-map "*" 'tpu-toggle-regexp)                  ; *
(define-key GOLD-map "+" 'nil)                                ; +
(define-key GOLD-map "," 'tpu-goto-breadcrumb)                ; ,
(define-key GOLD-map "-" 'negative-argument)                  ; -
(define-key GOLD-map "." 'tpu-drop-breadcrumb)                ; .
(define-key GOLD-map "/" 'tpu-emacs-replace)                  ; /
(define-key GOLD-map "0" 'digit-argument)                     ; 0
(define-key GOLD-map "1" 'digit-argument)                     ; 1
(define-key GOLD-map "2" 'digit-argument)                     ; 2
(define-key GOLD-map "3" 'digit-argument)                     ; 3
(define-key GOLD-map "4" 'digit-argument)                     ; 4
(define-key GOLD-map "5" 'digit-argument)                     ; 5
(define-key GOLD-map "6" 'digit-argument)                     ; 6
(define-key GOLD-map "7" 'digit-argument)                     ; 7
(define-key GOLD-map "8" 'digit-argument)                     ; 8
(define-key GOLD-map "9" 'digit-argument)                     ; 9
(define-key GOLD-map ":" 'nil)                                ; :
(define-key GOLD-map ";" 'tpu-trim-line-ends)                 ; ;
(define-key GOLD-map "<" 'nil)                                ; <
(define-key GOLD-map "=" 'nil)                                ; =
(define-key GOLD-map ">" 'nil)                                ; >
(define-key GOLD-map "?" 'tpu-spell-check)                    ; ?
(define-key GOLD-map "A" 'tpu-toggle-newline-and-indent)      ; A
(define-key GOLD-map "B" 'tpu-next-buffer)                    ; B
(define-key GOLD-map "C" 'repeat-complex-command)             ; C
(define-key GOLD-map "D" 'shell-command)                      ; D
(define-key GOLD-map "E" 'tpu-exit)                           ; E
(define-key GOLD-map "F" 'tpu-set-cursor-free)                ; F
(define-key GOLD-map "G" 'tpu-get)                            ; G
(define-key GOLD-map "H" 'nil)                                ; H
(define-key GOLD-map "I" 'tpu-include)                        ; I
(define-key GOLD-map "K" 'tpu-kill-buffer)                    ; K
(define-key GOLD-map "L" 'tpu-what-line)                      ; L
(define-key GOLD-map "M" 'buffer-menu)                        ; M
(define-key GOLD-map "N" 'tpu-next-file-buffer)               ; N
(define-key GOLD-map "O" 'occur)                              ; O
(define-key GOLD-map "P" 'lpr-buffer)                         ; P
(define-key GOLD-map "Q" 'tpu-quit)                           ; Q
(define-key GOLD-map "R" 'tpu-toggle-rectangle)               ; R
(define-key GOLD-map "S" 'replace)                            ; S
(define-key GOLD-map "T" 'tpu-line-to-top-of-window)          ; T
(define-key GOLD-map "U" 'undo)                               ; U
(define-key GOLD-map "V" 'tpu-version)                        ; V
(define-key GOLD-map "W" 'save-buffer)                        ; W
(define-key GOLD-map "X" 'tpu-save-all-buffers-kill-emacs)    ; X
(define-key GOLD-map "Y" 'copy-region-as-kill)                ; Y
(define-key GOLD-map "Z" 'suspend-emacs)                      ; Z
(define-key GOLD-map "[" 'blink-matching-open)                ; [
(define-key GOLD-map "\\" 'nil)                               ; \
(define-key GOLD-map "]" 'blink-matching-open)                ; ]
(define-key GOLD-map "^" 'tpu-add-at-bol)                     ; ^
(define-key GOLD-map "_" 'split-window-vertically)            ; -
(define-key GOLD-map "`" 'what-line)                          ; `
(define-key GOLD-map "a" 'tpu-toggle-newline-and-indent)      ; a
(define-key GOLD-map "b" 'tpu-next-buffer)                    ; b
(define-key GOLD-map "c" 'repeat-complex-command)             ; c
(define-key GOLD-map "d" 'shell-command)                      ; d
(define-key GOLD-map "e" 'tpu-exit)                           ; e
(define-key GOLD-map "f" 'tpu-set-cursor-free)                ; f
(define-key GOLD-map "g" 'tpu-get)                            ; g
(define-key GOLD-map "h" 'nil)                                ; h
(define-key GOLD-map "i" 'tpu-include)                        ; i
(define-key GOLD-map "k" 'tpu-kill-buffer)                    ; k
(define-key GOLD-map "l" 'goto-line)                          ; l
(define-key GOLD-map "m" 'buffer-menu)                        ; m
(define-key GOLD-map "n" 'tpu-next-file-buffer)               ; n
(define-key GOLD-map "o" 'occur)                              ; o
(define-key GOLD-map "p" 'lpr-region)                         ; p
(define-key GOLD-map "q" 'tpu-quit)                           ; q
(define-key GOLD-map "r" 'tpu-toggle-rectangle)               ; r
(define-key GOLD-map "s" 'replace)                            ; s
(define-key GOLD-map "t" 'tpu-line-to-top-of-window)          ; t
(define-key GOLD-map "u" 'undo)                               ; u
(define-key GOLD-map "v" 'tpu-version)                        ; v
(define-key GOLD-map "w" 'save-buffer)                        ; w
(define-key GOLD-map "x" 'tpu-save-all-buffers-kill-emacs)    ; x
(define-key GOLD-map "y" 'copy-region-as-kill)                ; y
(define-key GOLD-map "z" 'suspend-emacs)                      ; z
(define-key GOLD-map "{" 'nil)                                ; {
(define-key GOLD-map "|" 'split-window-horizontally)          ; |
(define-key GOLD-map "}" 'nil)                                ; }
(define-key GOLD-map "~" 'exchange-point-and-mark)            ; ~
(define-key GOLD-map "\177" 'delete-window)                   ; <X]


;;;
;;;  GOLD-CSI-map key definitions
;;;
(define-key GOLD-CSI-map "A" 'tpu-move-to-beginning)          ; up-arrow
(define-key GOLD-CSI-map "B" 'tpu-move-to-end)                ; down-arrow
(define-key GOLD-CSI-map "C" 'end-of-line)                    ; right-arrow
(define-key GOLD-CSI-map "D" 'beginning-of-line)              ; left-arrow

(define-key GOLD-CSI-map "1~" 'nil)                           ; Find
(define-key GOLD-CSI-map "2~" 'nil)                           ; Insert Here
(define-key GOLD-CSI-map "3~" 'tpu-store-text)                ; Remove
(define-key GOLD-CSI-map "4~" 'tpu-unselect)                  ; Select
(define-key GOLD-CSI-map "5~" 'tpu-previous-window)           ; Prev Screen
(define-key GOLD-CSI-map "6~" 'tpu-next-window)               ; Next Screen

(define-key GOLD-CSI-map "11~" 'nil)                          ; F1
(define-key GOLD-CSI-map "12~" 'nil)                          ; F2
(define-key GOLD-CSI-map "13~" 'nil)                          ; F3
(define-key GOLD-CSI-map "14~" 'nil)                          ; F4
(define-key GOLD-CSI-map "16~" 'nil)                          ; F5
(define-key GOLD-CSI-map "17~" 'nil)                          ; F6
(define-key GOLD-CSI-map "18~" 'nil)                          ; F7
(define-key GOLD-CSI-map "19~" 'nil)                          ; F8
(define-key GOLD-CSI-map "20~" 'nil)                          ; F9
(define-key GOLD-CSI-map "21~" 'nil)                          ; F10
(define-key GOLD-CSI-map "23~" 'nil)                          ; F11
(define-key GOLD-CSI-map "24~" 'nil)                          ; F12
(define-key GOLD-CSI-map "25~" 'nil)                          ; F13
(define-key GOLD-CSI-map "26~" 'nil)                          ; F14
(define-key GOLD-CSI-map "28~" 'describe-bindings)            ; HELP
(define-key GOLD-CSI-map "29~" 'nil)                          ; DO
(define-key GOLD-CSI-map "31~" 'tpu-drop-breadcrumb)          ; F17
(define-key GOLD-CSI-map "32~" 'nil)                          ; F18
(define-key GOLD-CSI-map "33~" 'nil)                          ; F19
(define-key GOLD-CSI-map "34~" 'nil)                          ; F20


;;;
;;;  GOLD-SS3-map key definitions
;;;
(define-key GOLD-SS3-map "A" 'tpu-move-to-beginning)          ; up-arrow
(define-key GOLD-SS3-map "B" 'tpu-move-to-end)                ; down-arrow
(define-key GOLD-SS3-map "C" 'end-of-line)                    ; right-arrow
(define-key GOLD-SS3-map "D" 'beginning-of-line)              ; left-arrow

(define-key GOLD-SS3-map "P" 'keyboard-quit)                  ; PF1
(define-key GOLD-SS3-map "Q" 'help-for-help)                  ; PF2
(define-key GOLD-SS3-map "R" 'tpu-search)                     ; PF3
(define-key GOLD-SS3-map "S" 'tpu-undelete-lines)             ; PF4
(define-key GOLD-SS3-map "p" 'open-line)                      ; KP0
(define-key GOLD-SS3-map "q" 'tpu-change-case)                ; KP1
(define-key GOLD-SS3-map "r" 'tpu-delete-to-eol)              ; KP2
(define-key GOLD-SS3-map "s" 'tpu-special-insert)             ; KP3
(define-key GOLD-SS3-map "t" 'tpu-move-to-end)                ; KP4
(define-key GOLD-SS3-map "u" 'tpu-move-to-beginning)          ; KP5
(define-key GOLD-SS3-map "v" 'tpu-paste)                      ; KP6
(define-key GOLD-SS3-map "w" 'execute-extended-command)       ; KP7
(define-key GOLD-SS3-map "x" 'tpu-fill)                       ; KP8
(define-key GOLD-SS3-map "y" 'tpu-replace)                    ; KP9
(define-key GOLD-SS3-map "m" 'tpu-undelete-words)             ; KP-
(define-key GOLD-SS3-map "l" 'tpu-undelete-char)              ; KP,
(define-key GOLD-SS3-map "n" 'tpu-unselect)                   ; KP.
(define-key GOLD-SS3-map "M" 'tpu-substitute)                 ; KPenter


;;;
;;;  Repeat complex command map additions to make arrows work
;;;
(cond ((boundp 'repeat-complex-command-map)
       (define-key repeat-complex-command-map "\e[A" 'previous-complex-command)
       (define-key repeat-complex-command-map "\e[B" 'next-complex-command)
       (define-key repeat-complex-command-map "\eOA" 'previous-complex-command)
       (define-key repeat-complex-command-map "\eOB" 'next-complex-command)))


;;;
;;;  Minibuffer map additions to make KP_enter = RET
;;;
(define-key minibuffer-local-map "\eOM" 'exit-minibuffer)
(define-key minibuffer-local-ns-map "\eOM" 'exit-minibuffer)
(define-key minibuffer-local-completion-map "\eOM" 'exit-minibuffer)
(define-key minibuffer-local-must-match-map "\eOM" 'minibuffer-complete-and-exit)
(and (boundp 'repeat-complex-command-map)
     (define-key repeat-complex-command-map "\eOM" 'exit-minibuffer))


;;;
;;;  Map control keys
;;;
(define-key global-map "\C-\\" 'quoted-insert)                ; ^\
(define-key global-map "\C-a" 'tpu-toggle-overwrite-mode)     ; ^A
(define-key global-map "\C-b" 'repeat-complex-command)        ; ^B
(define-key global-map "\C-e" 'tpu-current-end-of-line)       ; ^E
(define-key global-map "\C-h" 'tpu-next-beginning-of-line)    ; ^H (BS)
(define-key global-map "\C-j" 'tpu-delete-previous-word)      ; ^J (LF)
(define-key global-map "\C-k" 'tpu-define-macro-key)          ; ^K
(define-key global-map "\C-l" 'tpu-insert-formfeed)           ; ^L (FF)
(define-key global-map "\C-r" 'recenter)                      ; ^R
(define-key global-map "\C-u" 'tpu-delete-to-bol)             ; ^U
(define-key global-map "\C-v" 'tpu-quoted-insert)             ; ^V
(define-key global-map "\C-w" 'redraw-display)                ; ^W
(define-key global-map "\C-z" 'tpu-exit)                      ; ^Z


;;;
;;;  Functions to reset and toggle the control key bindings
;;;
(defun tpu-reset-control-keys (tpu-style)
  "Set control keys to TPU or emacs style functions."
  (let* ((tpu   (and tpu-style (not tpu-control-keys)))
	 (emacs (and (not tpu-style) tpu-control-keys))
	 (doit  (or tpu emacs)))
    (cond (doit
	   (if emacs (setq tpu-global-map (copy-keymap global-map)))
	   (let ((map (if tpu
			  (copy-keymap tpu-global-map)
			(copy-keymap tpu-original-global-map))))

 	     (define-key global-map "\C-\\" (lookup-key map "\C-\\"))   ; ^\
 	     (define-key global-map "\C-a" (lookup-key map "\C-a"))     ; ^A
 	     (define-key global-map "\C-b" (lookup-key map "\C-b"))     ; ^B
 	     (define-key global-map "\C-e" (lookup-key map "\C-e"))     ; ^E
	     (define-key global-map "\C-h" (lookup-key map "\C-h"))     ; ^H (BS)
	     (define-key global-map "\C-j" (lookup-key map "\C-j"))     ; ^J (LF)
	     (define-key global-map "\C-k" (lookup-key map "\C-k"))     ; ^K
	     (define-key global-map "\C-l" (lookup-key map "\C-l"))     ; ^L (FF)
	     (define-key global-map "\C-r" (lookup-key map "\C-r"))     ; ^R
	     (define-key global-map "\C-u" (lookup-key map "\C-u"))     ; ^U
	     (define-key global-map "\C-v" (lookup-key map "\C-v"))     ; ^V
	     (define-key global-map "\C-w" (lookup-key map "\C-w"))     ; ^W
	     (define-key global-map "\C-z" (lookup-key map "\C-z"))     ; ^Z
	     (setq tpu-control-keys tpu-style))))))

(defun tpu-toggle-control-keys nil
  "Toggles control key bindings between TPU-edt and Emacs."
  (interactive)
  (tpu-reset-control-keys (not tpu-control-keys))
  (and (interactive-p)
       (message "Control keys function with %s bindings."
		(if tpu-control-keys "TPU-edt" "Emacs"))))


;;;
;;;  Emacs version 19 minibuffer history support
;;;
(defun tpu-next-history-element (n)
  "Insert the next element of the minibuffer history into the minibuffer."
  (interactive "p")
  (next-history-element n)
  (goto-char (point-max)))

(defun tpu-previous-history-element (n)
  "Insert the previous element of the minibuffer history into the minibuffer."
  (interactive "p")
  (previous-history-element n)
  (goto-char (point-max)))

(defun tpu-arrow-history nil
  "Modify minibuffer maps to use arrows for history recall."
  (interactive)
  (let ((loc (where-is-internal 'tpu-previous-line)) (cur nil))
    (while (setq cur (car loc))
      (define-key read-expression-map cur 'tpu-previous-history-element)
      (define-key minibuffer-local-map cur 'tpu-previous-history-element)
      (define-key minibuffer-local-ns-map cur 'tpu-previous-history-element)
      (define-key minibuffer-local-completion-map cur 'tpu-previous-history-element)
      (define-key minibuffer-local-must-match-map cur 'tpu-previous-history-element)
      (setq loc (cdr loc)))

    (setq loc (where-is-internal 'tpu-next-line))
    (while (setq cur (car loc))
      (define-key read-expression-map cur 'tpu-next-history-element)
      (define-key minibuffer-local-map cur 'tpu-next-history-element)
      (define-key minibuffer-local-ns-map cur 'tpu-next-history-element)
      (define-key minibuffer-local-completion-map cur 'tpu-next-history-element)
      (define-key minibuffer-local-must-match-map cur 'tpu-next-history-element)
      (setq loc (cdr loc)))))


;;;
;;;  Emacs version 19 X-windows key definition support
;;;
(defun tpu-load-xkeys (file)
  "Load the TPU-edt X-windows key definitions FILE.
If FILE is nil, try to load a default file.  The default file names are
~/.tpu-lucid-keys for Lucid emacs, and ~/.tpu-gnu-keys for GNU emacs."
  (interactive "fX key definition file: ")
  (cond (file
	 (setq file (expand-file-name file)))
	(tpu-xkeys-file
	 (setq file (expand-file-name tpu-xkeys-file)))
	(tpu-gnu-emacs19-p
	 (setq file (expand-file-name "~/.tpu-gnu-keys")))
	(tpu-lucid-emacs19-p
	 (setq file (expand-file-name "~/.tpu-lucid-keys"))))
  (cond ((file-readable-p file)
	 (load-file file))
	(t
	 (switch-to-buffer "*scratch*")
	 (erase-buffer)
	 (insert "

     Ack!!  You're running TPU-edt under X-windows without loading an
     X  key definition file.   To create a  TPU-edt X  key definition
     file, run the tpu-mapper.el program.  It  came with TPU-edt.  It
     even includes directions on how to  use it!  Perhaps it's laying
     around here someplace.  ")
	 (let ((file "tpu-mapper.el")
	       (found nil)
	       (path nil)
	       (search-list (append (list (expand-file-name ".")) load-path)))
	   (while (and (not found) search-list)
	     (setq path (concat (car search-list)
				(if (string-match "/$" (car search-list)) "" "/")
				file))
	     (if (and (file-exists-p path) (not (file-directory-p path)))
		 (setq found t))
	     (setq search-list (cdr search-list)))
	   (cond (found
		  (insert (format
			   "Ah yes, there it is, in \n\n       %s \n\n" path))
		  (if (tpu-y-or-n-p "Do you want to run it now? ")
		      (load-file path)))
		 (t
		  (insert "Nope, I can't seem to find it.  :-(\n\n")
		  (sit-for 120)))))))


;;;
;;;  Start and Stop TPU-edt
;;;
;;;###autoload
(defun tpu-edt-on nil
  "Turn on TPU/edt emulation."
  (interactive)
  (cond
   ((not tpu-edt-mode)
    ;; we use picture-mode functions
    (require 'picture)
    (tpu-reset-control-keys t)
    (cond (tpu-emacs19-p
	   (and window-system (tpu-load-xkeys nil))
	   (tpu-arrow-history))
	  (t
	   ;; define ispell functions
	   (autoload 'ispell-word "ispell" "Check spelling of word at or before point" t)
	   (autoload 'ispell-complete-word "ispell" "Complete word at or before point" t)
	   (autoload 'ispell-buffer "ispell" "Check spelling of entire buffer" t)
	   (autoload 'ispell-region "ispell" "Check spelling of region" t)))
    (tpu-set-mode-line t)
    (tpu-advance-direction)
    ;; set page delimiter, display line truncation, and scrolling like TPU
    (setq-default page-delimiter "\f")
    (setq-default truncate-lines t)
    (setq scroll-step 1)
    (setq tpu-edt-mode t))))

(defun tpu-edt-off nil
  "Turn off TPU/edt emulation.  Note that the keypad is left on."
  (interactive)
  (cond
   (tpu-edt-mode
    (tpu-reset-control-keys nil)
    (tpu-set-mode-line nil)
    (setq-default page-delimiter "^\f")
    (setq-default truncate-lines nil)
    (setq scroll-step 0)
    (use-global-map global-map)
    (setq tpu-edt-mode nil))))


;;;
;;;  Turn on TPU-edt and announce it as a feature
;;;
(tpu-edt-mode)

(provide 'tpu-edt)

;;; tpu-edt.el ends here
