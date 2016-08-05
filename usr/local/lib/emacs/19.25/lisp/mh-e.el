;;; mh-e.el --- GNU Emacs interface to the MH mail system

;;; Copyright 1985,86,87,88,90,92,93,94 Free Software Foundation, Inc.

(defconst mh-e-time-stamp "Time-stamp: <94/04/13 11:30:48 gildea>")
(defconst mh-e-version "4.1"
  "Version numbers of this version of mh-e.")

;; Maintainer: Stephen Gildea <gildea@lcs.mit.edu>
;; Version: 4.1
;; Keywords: mail

;; mh-e is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; mh-e is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with mh-e; see the file COPYING.  If not, write to
;; the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

;;; Commentary:

;;; HOW TO USE:
;;; M-x mh-rmail to read mail.  Type C-h m there for a list of commands.
;;; C-u M-x mh-rmail to visit any folder.
;;; M-x mh-smail to send mail.  From within the mail reader, "m" works, too.
;;; Your .emacs might benefit from these bindings:
;;; (global-set-key "\C-cr" 'mh-rmail)
;;; (global-set-key "\C-xm" 'mh-smail)
;;; (global-set-key "\C-x4m" 'mh-smail-other-window)

;;; MH (Message Handler) is a powerful mail reader.  The MH newsgroup
;;; is comp.mail.mh; the mailing list is mh-users@ics.uci.edu (send to
;;; mh-users-request to be added).  See the monthly Frequently Asked
;;; Questions posting there for information on getting MH.

;;; mh-e works with Emacs 18 or 19, and MH 5 or 6.

;;; NB.  MH must have been compiled with the MHE compiler flag or several
;;; features necessary mh-e will be missing from MH commands, specifically
;;; the -build switch to repl and forw.

;;; Change Log:

;;; Original version for Gosling emacs by Brian Reid, Stanford, 1982.
;;; Modified by James Larus, BBN, July 1984 and UCB, 1984 & 1985.
;;; Rewritten for GNU Emacs, James Larus 1985.  larus@ginger.berkeley.edu
;;; Modified by Stephen Gildea 1988.  gildea@lcs.mit.edu
(defconst mh-e-RCS-id "$Header: /home/fsf/rms/e19/lisp/RCS/mh-e.el,v 1.2 1994/04/13 20:42:10 rms Exp $")

;;; Code:

(provide 'mh-e)
(require 'mh-utils)


;;; Site customization (see also mh-utils.el):

(defvar mh-redist-full-contents nil
  "Non-nil if the `dist' command needs whole letter for redistribution.
This is the case only when `send' is compiled with the BERK option.
If MH will not allow you to redist a previously redist'd msg, set to nil.")

;;; Hooks:

(defvar mh-folder-mode-hook nil
  "Invoked in MH-Folder mode on a new folder.")

(defvar mh-inc-folder-hook nil
  "Invoked by \\<mh-folder-mode-map>`\\[mh-inc-folder]' after incorporating mail into a folder.")

(defvar mh-show-hook nil
  "Invoked after \\<mh-folder-mode-map>`\\[mh-show]' shows a message.")

(defvar mh-show-mode-hook nil
  "Invoked in MH-Show mode on each message.")

(defvar mh-delete-msg-hook nil
  "Invoked after marking each message for deletion.")

(defvar mh-refile-msg-hook nil
  "Invoked after marking each message for refiling.")

(defvar mh-before-quit-hook nil
  "Invoked by \\<mh-folder-mode-map>`\\[mh-quit]' before quitting mh-e.  See also  mh-quit-hook.")

(defvar mh-quit-hook nil
  "Invoked after \\<mh-folder-mode-map>`\\[mh-quit]' quits mh-e.  See also  mh-before-quit-hook.")



;;; Personal preferences:

(defvar mh-lpr-command-format "lpr -p -J '%s'"
  "*Format for Unix command that prints a message.
The string should be a Unix command line, with the string '%s' where
the job's name (folder and message number) should appear.  The formatted
message text is piped to this command when you type \\<mh-folder-mode-map>`\\[mh-print-msg]'.")

(defvar mh-scan-prog "scan"
  "*Program to run to generate one-line-per-message listing of a folder.
Normally \"scan\" or a file name linked to scan.  This file is searched
for relative to the mh-progs directory unless it is an absolute pathname.
Automatically becomes buffer-local when set in any fashion.")
(make-variable-buffer-local 'mh-scan-prog)

(defvar mh-inc-prog "inc"
  "*Program to run to incorporate new mail into a folder.
Normally \"inc\".  This file is searched for relative to
the mh-progs directory unless it is an absolute pathname.")

(defvar mh-print-background nil
  "*Print messages in the background if non-nil.
WARNING: do not delete the messages until printing is finished;
otherwise, your output may be truncated.")

(defvar mh-recenter-summary-p nil
  "*Recenter summary window when the show window is toggled off if non-nil.")

(defvar mh-ins-buf-prefix "> "
  "*String to put before each non-blank line of a yanked or inserted message.
\\<mh-letter-mode-map>Used when the message is inserted into an outgoing letter
by \\[mh-insert-letter] or \\[mh-yank-cur-msg].")

(defvar mh-do-not-confirm nil
  "*Non-nil means do not prompt for confirmation before some mh-e commands.
Affects non-recoverable commands such as mh-kill-folder and mh-undo-folder.")

(defvar mh-store-default-directory nil
  "*Last directory used by \\[mh-store-msg]; default for next store.
A directory name string, or nil to use current directory.")

;;; Parameterize mh-e to work with different scan formats.  The defaults work
;;; with the standard MH scan listings, in which the first 4 characters on
;;; the line are the message number, followed by two places for notations.

(defvar mh-good-msg-regexp  "^....[^D^]"
  "Regexp specifiying the scan lines that are 'good' messages.")

(defvar mh-deleted-msg-regexp "^....D"
  "Regexp matching scan lines of deleted messages.")

(defvar mh-refiled-msg-regexp  "^....\\^"
  "Regexp matching scan lines of refiled messages.")

(defvar mh-valid-scan-line "^ *[0-9]"
  "Regexp matching scan lines for messages (not error messages).")

(defvar mh-flagged-scan-msg-regexp "^....\\D\\|^....\\^\\|^....\\+\\|^.....%"
  "Regexp matching flagged scan lines.
Matches lines marked as deleted, refiled, in a sequence, or the cur message.")

(defvar mh-cur-scan-msg-regexp "^....\\+"
  "Regexp matching scan line for the cur message.")

(defvar mh-show-buffer-mode-line-buffer-id "{show-%s} %d"
  "Format string to produce `mode-line-buffer-identification' for show buffers.
First argument is folder name.  Second is message number.")

(defvar mh-partial-folder-mode-line-annotation "select"
  "Annotation when displaying part of a folder.
The string is displayed after the folder's name.  NIL for no annotation.")


;;; Internal variables:

(defvar mh-last-destination nil
  "Destination of last refile or write command.")

(defvar mh-folder-mode-map (make-keymap)
  "Keymap for MH folders.")

(defvar mh-next-seq-num nil
  "Index of free sequence id.")

(defvar mh-delete-list nil
  "List of msg numbers to delete.")

(defvar mh-refile-list nil
  "List of folder names in mh-seq-list.")

(defvar mh-next-direction 'forward
  "Direction to move to next message.")

(defvar mh-narrowed-to-seq nil
  "Sequence display is narrowed to.")

(defvar mh-first-msg-num nil
  "Number of first msg in buffer.")

(defvar mh-last-msg-num nil
  "Number of last msg in buffer.")


;;; Macros and generic functions:

(defun mh-mapc (func list)
  (while list
    (funcall func (car list))
    (setq list (cdr list))))



;;; Entry points:

;;;###autoload
(defun mh-rmail (&optional arg)
  "Inc(orporate) new mail with MH, or, with arg, scan an MH mail folder.
This function is an entry point to mh-e, the Emacs front end
to the MH mail system."
  (interactive "P")
  (mh-find-path)
  (if arg
      (call-interactively 'mh-visit-folder)
      (mh-inc-folder)))


;;; mh-smail and mh-smail-other-window have been moved to the new file
;;; mh-comp.el, but Emacs 18 still looks for them here, so provide a
;;; definition here, too, for a while.

(defun mh-smail ()
  "Compose and send mail with the MH mail system.
This function is an entry point to mh-e, the Emacs front end
to the MH mail system."
  (interactive)
  (mh-find-path)
  (require 'mh-comp)
  (call-interactively 'mh-send))


(defun mh-smail-other-window ()
  "Compose and send mail in other window with the MH mail system.
This function is an entry point to mh-e, the Emacs front end
to the MH mail system."
  (interactive)
  (mh-find-path)
  (require 'mh-comp)
  (call-interactively 'mh-send-other-window))



;;; User executable mh-e commands:


(defun mh-delete-msg (msg-or-seq)
  "Mark the specified MESSAGE(s) for subsequent deletion and move to the next.
Default is the displayed message.  If optional prefix argument is
given then prompt for the message sequence."
  (interactive (list (if current-prefix-arg
			 (mh-read-seq-default "Delete" t)
			 (mh-get-msg-num t))))
  (mh-delete-msg-no-motion msg-or-seq)
  (mh-next-msg))


(defun mh-delete-msg-no-motion (msg-or-seq)
  "Mark the specified MESSAGE(s) for subsequent deletion.
Default is the displayed message.  If optional prefix argument is
provided, then prompt for the message sequence."
  (interactive (list (if current-prefix-arg
			 (mh-read-seq-default "Delete" t)
			 (mh-get-msg-num t))))
  (if (numberp msg-or-seq)
      (mh-delete-a-msg msg-or-seq)
      (mh-map-to-seq-msgs 'mh-delete-a-msg msg-or-seq)))


(defun mh-execute-commands ()
  "Process outstanding delete and refile requests."
  (interactive)
  (if mh-narrowed-to-seq (mh-widen))
  (mh-process-commands mh-current-folder)
  (mh-set-scan-mode)
  (mh-goto-cur-msg)			; after mh-set-scan-mode for efficiency
  (mh-make-folder-mode-line)
  t)					; return t for write-file-hooks


(defun mh-first-msg ()
  "Move to the first message."
  (interactive)
  (goto-char (point-min)))


(defun mh-header-display ()
  "Show the current message with all its headers.
Displays headers that might have been suppressed by setting the
variables `mh-clean-message-header' or `mhl-formfile', or by the fallback
behavior of scrolling uninteresting headers off the top of the window.
Type \"\\[mh-show]\" to show the message normally again."
  (interactive)
  (and (not mh-showing-with-headers)
       (or mhl-formfile mh-clean-message-header)
       (mh-invalidate-show-buffer))
  (let ((mhl-formfile nil)
	(mh-clean-message-header nil))
    (mh-show-msg nil)
    (mh-in-show-buffer (mh-show-buffer)
      (goto-char (point-min))
      (mh-recenter 0))
    (setq mh-showing-with-headers t)))


(defun mh-inc-folder (&optional maildrop-name)
  "Inc(orporate)s new mail into +inbox.
Optional prefix argument specifies an alternate maildrop from the default.
If this is given, incorporates mail into the current folder, rather
than +inbox.  Runs `mh-inc-folder-hook' after incorporating new mail.
Do not call this function from outside mh-e; use \\[mh-rmail] instead."
  (interactive (list (if current-prefix-arg
			 (expand-file-name
			  (read-file-name "inc mail from file: "
					  mh-user-path)))))
  (let ((config (current-window-configuration)))
    (if (not maildrop-name)
	(cond ((not (get-buffer "+inbox"))
	       (mh-make-folder "+inbox")
	       (setq mh-previous-window-config config))
	      ((not (eq (current-buffer) (get-buffer "+inbox")))
	       (switch-to-buffer "+inbox")
	       (setq mh-previous-window-config config)))))
  (mh-get-new-mail maildrop-name)
  (run-hooks 'mh-inc-folder-hook))


(defun mh-last-msg ()
  "Move to the last message."
  (interactive)
  (goto-char (point-max))
  (while (and (not (bobp)) (looking-at "^$"))
    (forward-line -1)))


(defun mh-next-undeleted-msg (&optional arg)
  "Move to next undeleted message in window."
  (interactive "P")
  (forward-line (prefix-numeric-value arg))
  (setq mh-next-direction 'forward)
  (cond ((re-search-forward mh-good-msg-regexp nil 0 arg)
	 (beginning-of-line)
	 (mh-maybe-show))
	(t
	 (forward-line -1)
	 (if (get-buffer mh-show-buffer)
	     (delete-windows-on mh-show-buffer)))))


(defun mh-refile-msg (msg-or-seq dest)
  "Refile MESSAGE(s) (default: displayed message) in FOLDER.
If optional prefix argument provided, then prompt for message sequence."
  (interactive
   (list (if current-prefix-arg
	     (mh-read-seq-default "Refile" t)
	   (mh-get-msg-num t))
	 (intern
	  (mh-prompt-for-folder
	   "Destination"
	   (or (and mh-msg-folder-hook
		    (let ((file-name (mh-msg-filename (mh-get-msg-num t))))
		      (save-excursion
			(set-buffer (get-buffer-create " *mh-temp*"))
			(erase-buffer)
			(insert-file-contents file-name)
			(let ((buffer-file-name file-name))
			  (funcall mh-msg-folder-hook)))))
	       (and (eq 'refile (car mh-last-destination))
		    (symbol-name (cdr mh-last-destination)))
	       "")
	   t))))
  (setq mh-last-destination (cons 'refile dest))
  (if (numberp msg-or-seq)
      (mh-refile-a-msg msg-or-seq dest)
      (mh-map-to-seq-msgs 'mh-refile-a-msg msg-or-seq dest))
  (mh-next-msg))


(defun mh-refile-or-write-again (msg)
  "Re-execute the last refile or write command on the given MESSAGE.
Default is the displayed message.  Use the same folder or file as the
previous refile or write command."
  (interactive (list (mh-get-msg-num t)))
  (if (null mh-last-destination)
      (error "No previous refile or write"))
  (cond ((eq (car mh-last-destination) 'refile)
	 (mh-refile-a-msg msg (cdr mh-last-destination))
	 (message "Destination folder: %s" (cdr mh-last-destination)))
	(t
	 (apply 'mh-write-msg-to-file msg (cdr mh-last-destination))
	 (message "Destination: %s" (cdr mh-last-destination))))
  (mh-next-msg))


(defun mh-quit ()
  "Quit mh-e.
Start by running mh-before-quit-hook.  Restore the previous window
configuration, if one exists.  Finish by running mh-quit-hook."
  (interactive)
  (run-hooks 'mh-before-quit-hook) 
  (mh-update-unseen)
  (mh-invalidate-show-buffer)
  (bury-buffer (current-buffer))
  (if (get-buffer mh-show-buffer)
      (bury-buffer mh-show-buffer))
  (if mh-previous-window-config
      (set-window-configuration mh-previous-window-config))
  (run-hooks 'mh-quit-hook))

(defun mh-page-msg (&optional arg)
  "Page the displayed message forwards.
Scrolls ARG lines or a full screen if no argument is supplied."
  (interactive "P")
  (scroll-other-window arg))


(defun mh-previous-page (&optional arg)
  "Page the displayed message backwards.
Scrolls ARG lines or a full screen if no argument is supplied."
  (interactive "P")
  (mh-in-show-buffer (mh-show-buffer)
    (scroll-down arg)))


(defun mh-previous-undeleted-msg (&optional arg)
  "Move to previous undeleted message in window."
  (interactive "p")
  (setq mh-next-direction 'backward)
  (beginning-of-line)
  (cond ((re-search-backward mh-good-msg-regexp nil 0 arg)
	 (mh-maybe-show))
	(t
	 (if (get-buffer mh-show-buffer)
	     (delete-windows-on mh-show-buffer)))))


(defun mh-rescan-folder (&optional range)
  "Rescan a folder after optionally processing the outstanding commands.
If optional prefix argument is provided, prompt for the range of
messages to display.  Otherwise show the entire folder."
  (interactive (list (if current-prefix-arg
			 (mh-read-msg-range "Range to scan [all]? ")
		       nil)))
  (setq mh-next-direction 'forward)
  (mh-scan-folder mh-current-folder (or range "all")))


(defun mh-write-msg-to-file (msg file no-headers)
  "Append MESSAGE to the end of a FILE.
If NO-HEADERS (prefix argument) is provided, write only the message body.
Otherwise send the entire message including the headers."
  (interactive
   (list (mh-get-msg-num t)
	 (let ((default-dir (if (eq 'write (car mh-last-destination))
				(file-name-directory (car (cdr mh-last-destination)))
			      default-directory)))
	   (read-file-name "Save message in file: " default-dir
			   (expand-file-name "mail.out" default-dir)))
	 current-prefix-arg))
  (let ((file-name (mh-msg-filename msg))
	(output-file (mh-expand-file-name file)))
    (setq mh-last-destination (list 'write file no-headers))
    (save-excursion
      (set-buffer (get-buffer-create " *mh-temp*"))
      (erase-buffer)
      (insert-file-contents file-name)
      (goto-char (point-min))
      (if no-headers (search-forward "\n\n"))
      (append-to-file (point) (point-max) output-file))))


(defun mh-toggle-showing ()
  "Toggle the scanning mode/showing mode of displaying messages."
  (interactive)
  (if mh-showing
      (mh-set-scan-mode)
      (mh-show)))


(defun mh-undo (msg-or-seq)
  "Undo the deletion or refile of the specified MESSAGE(s).
Default is the displayed message.  If optional prefix argument is
provided, then prompt for the message sequence."
  (interactive (list (if current-prefix-arg
			 (mh-read-seq-default "Undo" t)
			 (mh-get-msg-num t))))
  (cond ((numberp msg-or-seq)
	 (let ((original-position (point)))
	   (beginning-of-line)
	   (while (not (or (looking-at mh-deleted-msg-regexp)
			   (looking-at mh-refiled-msg-regexp)
			   (and (eq mh-next-direction 'forward) (bobp))
			   (and (eq mh-next-direction 'backward)
				(save-excursion (forward-line) (eobp)))))
	     (forward-line (if (eq mh-next-direction 'forward) -1 1)))
	   (if (or (looking-at mh-deleted-msg-regexp)
		   (looking-at mh-refiled-msg-regexp))
	       (progn
		 (mh-undo-msg (mh-get-msg-num t))
		 (mh-maybe-show))
	       (goto-char original-position)
	       (error "Nothing to undo"))))
	(t
	 (mh-mapc (function mh-undo-msg) (mh-seq-to-msgs msg-or-seq))))
  ;; update the mh-refile-list so mh-outstanding-commands-p will work
  (mh-mapc (function
	    (lambda (elt)
	      (if (not (mh-seq-to-msgs elt))
		  (setq mh-refile-list (delq elt mh-refile-list)))))
	   mh-refile-list)
  (if (not (mh-outstanding-commands-p))
      (mh-set-folder-modified-p nil)))


(defun mh-version ()
  "Display version information about mh-e and MH."
  (interactive)
  (mh-find-progs)
  (set-buffer (get-buffer-create " *mh-temp*"))
  (erase-buffer)
  (insert "  mh-e info:\n\nversion: " mh-e-version "\n" mh-e-time-stamp
	  "\nEmacs: " emacs-version " on " (symbol-name system-type) " ")
  (condition-case ()
      (call-process "uname" nil t nil "-a")
    (file-error))
  (insert "\n\n  MH info:\n\n" (expand-file-name "inc" mh-progs) ":\n")
  (let ((help-start (point)))
    (condition-case err-data
	(mh-exec-cmd-output "inc" nil "-help")
      (file-error (insert (mapconcat 'concat (cdr err-data) ": "))))
    (goto-char help-start)
    (search-forward "version: " nil t)
    (beginning-of-line)
    (delete-region help-start (point))
    (goto-char (point-min)))
  (display-buffer " *mh-temp*"))


(defun mh-visit-folder (folder &optional range)
  "Visits FOLDER and displays RANGE of messages.
Assumes mh-e has already been initialized.
Do not call this function from outside mh-e; see \\[mh-rmail] instead."
  (interactive (list (mh-prompt-for-folder "Visit" "+inbox" t)
		     (mh-read-msg-range "Range [all]? ")))
  (let ((config (current-window-configuration)))
    (mh-scan-folder folder (or range "all"))
    (setq mh-previous-window-config config))
  nil)


(defun mh-compat-quit ()
  "\"b\" reserved for future use as mh-burst-digest; will assume you want \"\\[mh-quit]\" ..."
  ;; This is a temporary compatibility function
  (interactive)
  (message "%s" (documentation this-command))
  (sit-for 1)
  (call-interactively 'mh-quit))



;;; Support routines.

(defun mh-delete-a-msg (msg)
  ;; Delete the MESSAGE.
  (save-excursion
    (mh-goto-msg msg nil t)
    (if (looking-at mh-refiled-msg-regexp)
	(error "Message %d is refiled.  Undo refile before deleting." msg))
    (if (looking-at mh-deleted-msg-regexp)
	nil
	(mh-set-folder-modified-p t)
	(setq mh-delete-list (cons msg mh-delete-list))
	(mh-add-msgs-to-seq msg 'deleted t)
	(mh-notate msg ?D mh-cmd-note)
	(run-hooks 'mh-delete-msg-hook))))

(defun mh-refile-a-msg (msg destination)
  ;; Refile MESSAGE in FOLDER.  FOLDER is a symbol, not a string.
  (save-excursion
    (mh-goto-msg msg nil t)
    (cond ((looking-at mh-deleted-msg-regexp)
	   (error "Message %d is deleted.  Undo delete before moving." msg))
	  ((looking-at mh-refiled-msg-regexp)
	   (if (y-or-n-p
		(format "Message %d already refiled.  Copy to %s as well? "
			msg destination))
	       (mh-exec-cmd "refile" (mh-get-msg-num t) "-link"
			    "-src" mh-current-folder
			    (symbol-name destination))
	       (message "Message not copied.")))
	  (t
	   (mh-set-folder-modified-p t)
	   (if (not (memq destination mh-refile-list))
	       (setq mh-refile-list (cons destination mh-refile-list)))
	   (if (not (memq msg (mh-seq-to-msgs destination)))
	       (mh-add-msgs-to-seq msg destination t))
	   (mh-notate msg ?^ mh-cmd-note)
	   (run-hooks 'mh-refile-msg-hook)))))


(defun mh-next-msg ()
  ;; Move backward or forward to the next undeleted message in the buffer.
  (if (eq mh-next-direction 'forward)
      (mh-next-undeleted-msg 1)
      (mh-previous-undeleted-msg 1)))


(defun mh-set-scan-mode ()
  ;; Display the scan listing buffer, but do not show a message.
  (if (get-buffer mh-show-buffer)
      (delete-windows-on mh-show-buffer))
  (setq mh-showing nil)
  (set-buffer-modified-p (buffer-modified-p)) ;force mode line update
  (if mh-recenter-summary-p
      (mh-recenter nil)))


(defun mh-undo-msg (msg)
  ;; Undo the deletion or refile of one MESSAGE.
  (cond ((memq msg mh-delete-list)
	 (setq mh-delete-list (delq msg mh-delete-list))
	 (mh-delete-msg-from-seq msg 'deleted t))
	(t
	 (mh-mapc (function (lambda (dest)
			      (mh-delete-msg-from-seq msg dest t)))
		  mh-refile-list)))
  (mh-notate msg ?  mh-cmd-note))




;;; The folder data abstraction.

(defun mh-make-folder (name)
  ;; Create and initialize a new mail folder called NAME and make it the
  ;; current folder.
  (switch-to-buffer name)
  (setq buffer-read-only nil)
  (erase-buffer)
  (setq buffer-read-only t)
  (mh-folder-mode)
  (mh-set-folder-modified-p nil)
  (setq buffer-file-name mh-folder-filename))


;;; Ensure new buffers won't get this mode if default-major-mode is nil.
(put 'mh-folder-mode 'mode-class 'special)

(defun mh-folder-mode ()
  "Major mh-e mode for \"editing\" an MH folder scan listing.\\<mh-folder-mode-map>
You can show the message the cursor is pointing to, and step through the
messages.  Messages can be marked for deletion or refiling into another
folder; these commands are executed all at once with a separate command.

A prefix argument (\\[universal-argument]) to delete, refile, list, or undo
applies the action to a message sequence.

Here is a list of the standard keys for mh-e commands, grouped by function.
This list is purposefully not customized; mh-e has a long history, and many
alternate key bindings as a result.  This list is to encourage users to use
standard keys so the other keys can perhaps someday be put to new uses.

t	toggle show or scan-only mode
.	show message, or back to top if already showing

SPC	page forward
DEL	page back

n	next message
p	previous message
j	jump to message by number

d	mark for deletion
o, ^	mark for output (refile) to another folder
?	show folder of pending refile
u	undo delete or refile marking

x	execute marked deletes and refiles
i	incorporate new mail

m	mail a new message
r	reply to a message
f	forward a message

q	quit mh-e

M-f	visit new folder
M-r	rescan this folder

Here are all the commands with their current binding, listed in key order:
\\{mh-folder-mode-map}

Variables controlling mh-e operation are (defaults in parentheses):

 mh-recursive-folders (nil)
    Non-nil means commands which operate on folders do so recursively.

 mh-bury-show-buffer (t)
    Non-nil means that the buffer used to display message is buried.
    It will never be offered as the default other buffer.

 mh-clean-message-header (nil)
    Non-nil means remove header lines matching the regular expression
    specified in mh-invisible-headers from messages.

 mh-visible-headers (nil)
    If non-nil, it contains a regexp specifying the headers that are shown in
    a message if mh-clean-message-header is non-nil.  Setting this variable
    overrides mh-invisible-headers.

 mh-do-not-confirm (nil)
    Non-nil means do not prompt for confirmation before executing some
    non-recoverable commands such as mh-kill-folder and mh-undo-folder.

 mhl-formfile (nil)
    Name of format file to be used by mhl to show messages.
    A value of T means use the default format file.
    Nil means don't use mhl to format messages.

 mh-lpr-command-format (\"lpr -p -J '%s'\")
    Format for command used to print a message on a system printer.

 mh-scan-prog (\"scan\")
    Program to run to generate one-line-per-message listing of a folder.
    Normally \"scan\" or a file name linked to scan.  This file is searched
    for relative to the mh-progs directory unless it is an absolute pathname.
    Automatically becomes buffer-local when set in any fashion.

 mh-print-background (nil)
    Print messages in the background if non-nil.
    WARNING: do not delete the messages until printing is finished;
    otherwise, your output may be truncated.

 mh-recenter-summary-p (nil)
    If non-nil, then the scan listing is recentered when the window displaying
    a messages is toggled off.

 mh-summary-height (4)
    Number of lines in the summary window including the mode line.

 mh-ins-buf-prefix (\"> \")
    String to insert before each non-blank line of a message as it is
    inserted in a draft letter.

The value of mh-folder-mode-hook is called when a new folder is set up."

  (kill-all-local-variables)
  (use-local-map mh-folder-mode-map)
  (setq major-mode 'mh-folder-mode)
  (mh-set-mode-name "MH-Folder")
  (make-local-vars
   'mh-current-folder (buffer-name)	; Name of folder, a string
   'mh-show-buffer (format "show-%s" (buffer-name)) ; Buffer that displays msgs
   'mh-folder-filename			; e.g. "/usr/foobar/Mail/inbox/"
   (file-name-as-directory (mh-expand-file-name (buffer-name)))
   'mh-showing nil			; Show message also?
   'mh-next-seq-num 0			; Index of free sequence id
   'mh-delete-list nil			; List of msgs nums to delete
   'mh-refile-list nil			; List of folder names in mh-seq-list
   'mh-seq-list nil			; Alist of (seq . msgs) nums
   'mh-seen-list nil			; List of displayed messages
   'mh-next-direction 'forward		; Direction to move to next message
   'mh-narrowed-to-seq nil		; Sequence display is narrowed to
   'mh-first-msg-num nil		; Number of first msg in buffer
   'mh-last-msg-num nil			; Number of last msg in buffer
   'mh-previous-window-config nil)	; Previous window configuration
  (setq truncate-lines t)
  (auto-save-mode -1)
  (setq buffer-offer-save t)
  (make-local-variable 'write-file-hooks)
  (setq write-file-hooks '(mh-execute-commands))
  (make-local-variable 'revert-buffer-function)
  (setq revert-buffer-function 'mh-undo-folder)
  (or (assq 'mh-showing minor-mode-alist)
      (setq minor-mode-alist
	    (cons '(mh-showing " Show") minor-mode-alist)))
  (run-hooks 'mh-folder-mode-hook))


(defun make-local-vars (&rest pairs)
  ;; Take VARIABLE-VALUE pairs and makes local variables initialized to the
  ;; value.
  (while pairs
    (make-variable-buffer-local (car pairs))
    (set (car pairs) (car (cdr pairs)))
    (setq pairs (cdr (cdr pairs)))))


(defun mh-scan-folder (folder range)
  ;; Scan the FOLDER over the RANGE.  Return in the folder's buffer.
  (cond ((null (get-buffer folder))
	 (mh-make-folder folder))
	(t
	 (mh-process-or-undo-commands folder)
	 (switch-to-buffer folder)))
  (mh-regenerate-headers range)
  (cond ((zerop (buffer-size))
	 (if (equal range "all")
	     (message "Folder %s is empty" folder)
	   (message "No messages in %s, range %s" folder range))
	 (sit-for 5)))
  (mh-goto-cur-msg))


(defun mh-regenerate-headers (range)
  ;; Replace buffer with scan of its contents over range RANGE.
  (let ((folder mh-current-folder))
    (message "Scanning %s..." folder)
    (with-mh-folder-updating (nil)
      (erase-buffer)
      (mh-exec-cmd-output mh-scan-prog nil
			  "-noclear" "-noheader"
			  "-width" (window-width)
			  folder range)
      (goto-char (point-min))
      (cond ((looking-at "scan: no messages in")
	     (keep-lines mh-valid-scan-line)) ; Flush random scan lines
	    ((looking-at "scan: "))	; Keep error messages
	    (t
	     (keep-lines mh-valid-scan-line))) ; Flush random scan lines
      (mh-delete-seq-locally 'cur)	; To pick up new one
      (setq mh-seq-list (mh-read-folder-sequences folder nil))
      (mh-notate-user-sequences)
      (mh-make-folder-mode-line (if (equal range "all")
				    nil
				    mh-partial-folder-mode-line-annotation)))
    (message "Scanning %s...done" folder)))


(defun mh-get-new-mail (maildrop-name)
  ;; Read new mail from a maildrop into the current buffer.
  ;; Return in the current buffer.
  (let ((point-before-inc (point))
	(folder mh-current-folder)
	(new-mail-p nil))
    (with-mh-folder-updating (t)
      (message (if maildrop-name
		   (format "inc %s -file %s..." folder maildrop-name)
		   (format "inc %s..." folder)))
      (setq mh-next-direction 'forward)
      (goto-char (point-max))
      (let ((start-of-inc (point)))
	(if maildrop-name
	    (mh-exec-cmd-output mh-inc-prog nil folder
				"-file" (expand-file-name maildrop-name)
				"-width" (window-width)
				"-truncate")
	    (mh-exec-cmd-output "inc" nil
				"-width" (window-width)))
	(message
	 (if maildrop-name
	     (format "inc %s -file %s...done" folder maildrop-name)
	     (format "inc %s...done" folder)))
	(goto-char start-of-inc)
	(cond ((looking-at "inc: no mail")
	       (message "No new mail%s%s" (if maildrop-name " in " "")
			(if maildrop-name maildrop-name "")))
	      ((re-search-forward "^inc:" nil t) ; Error messages
	       (error "inc error"))
	      (t
	       ;; remove old cur notation (cf mh-goto-cur-msg code)
	       (let ((cur-msg (car (mh-seq-to-msgs 'cur))))
		 (save-excursion
		   (and cur-msg
			(mh-goto-msg cur-msg t nil)
			(looking-at mh-cur-scan-msg-regexp)
			(mh-notate nil ?  mh-cmd-note))))
	       (setq new-mail-p t)))
	(keep-lines mh-valid-scan-line) ; Flush random scan lines
	(mh-delete-seq-locally 'cur)	; To pick up new one
	(setq mh-seq-list (mh-read-folder-sequences folder t))
	(mh-notate-user-sequences)
	(if new-mail-p
	    (progn
	      (mh-goto-cur-msg)
	      (mh-make-folder-mode-line))
	    (goto-char point-before-inc))))))


(defun mh-make-folder-mode-line (&optional annotation)
  ;; Set the fields of the mode line for a folder buffer.
  ;; The optional ANNOTATION string is displayed after the folder's name.
  (save-excursion
    (mh-first-msg)
    (setq mh-first-msg-num (mh-get-msg-num nil))
    (mh-last-msg)
    (setq mh-last-msg-num (mh-get-msg-num nil))
    (let ((lines (count-lines (point-min) (point-max))))
      (setq mode-line-buffer-identification
	    (list (format "{%%b%s} %d msg%s"
			  (if annotation (format "/%s" annotation) "")
			  lines
			  (if (zerop lines)
			      "s"
			      (if (> lines 1)
				  (format "s (%d-%d)" mh-first-msg-num
					  mh-last-msg-num)
				  (format " (%d)" mh-first-msg-num)))))))))


(defun mh-unmark-all-headers (remove-all-flags)
  ;; Remove all '+' flags from the headers, and if called with a non-nil
  ;; argument, remove all 'D', '^' and '%' flags too.
  ;; Optimized for speed (i.e., no regular expressions).
  (save-excursion
    (let ((case-fold-search nil)
	  (last-line (- (point-max) mh-cmd-note))
	  char)
      (mh-first-msg)
      (while (<= (point) last-line)
	(forward-char mh-cmd-note)
	(setq char (following-char))
	(if (or (and remove-all-flags
		     (or (eql char ?D)
			 (eql char ?^)
			 (eql char ?%)))
		(eql char ?+))
	    (progn
	      (delete-char 1)
	      (insert " ")))
	(forward-line)))))


(defun mh-goto-cur-msg ()
  ;; Position the cursor at the current message.
  (let ((cur-msg (car (mh-seq-to-msgs 'cur))))
    (cond ((and cur-msg
		(mh-goto-msg cur-msg t nil))
	   (mh-notate nil ?+ mh-cmd-note)
	   (mh-recenter 0)
	   (mh-maybe-show cur-msg))
	  (t
	   (mh-last-msg)
	   (message "No current message")))))


(defun mh-process-or-undo-commands (folder)
  ;; If FOLDER has outstanding commands, then either process or discard them.
  ;; Called by functions like mh-sort-folder, so also invalidate show buffer.
  (set-buffer folder)
  (if (mh-outstanding-commands-p)
      (if (or mh-do-not-confirm
	      (y-or-n-p
		"Process outstanding deletes and refiles (or lose them)? "))
	  (mh-process-commands folder)
	  (mh-undo-folder)))
  (mh-update-unseen)
  (mh-invalidate-show-buffer))


(defun mh-process-commands (folder)
  ;; Process outstanding commands for the folder FOLDER.
  (message "Processing deletes and refiles for %s..." folder)
  (set-buffer folder)
  (with-mh-folder-updating (nil)
    ;; Update the unseen sequence if it exists
    (mh-update-unseen)

    ;; Then refile messages
    (mh-mapc
     (function
      (lambda (dest)
	(let ((msgs (mh-seq-to-msgs dest)))
	  (cond (msgs
		 (apply 'mh-exec-cmd "refile"
			"-src" folder (symbol-name dest) msgs)
		 (mh-delete-scan-msgs msgs))))))
     mh-refile-list)
    (setq mh-refile-list nil)

    ;; Now delete messages
    (cond (mh-delete-list
	   (apply 'mh-exec-cmd "rmm" folder mh-delete-list)
	   (mh-delete-scan-msgs mh-delete-list)
	   (setq mh-delete-list nil)))

    ;; Don't need to remove sequences since delete and refile do so.

    ;; Mark cur message
    (if (> (buffer-size) 0)
	(mh-define-sequence 'cur (or (mh-get-msg-num nil) "last")))

    (and (buffer-file-name (get-buffer mh-show-buffer))
	 (not (file-exists-p (buffer-file-name (get-buffer mh-show-buffer))))
	 ;; If "inc" were to put a new msg in this file,
	 ;; we would not notice, so mark it invalid now.
	 (mh-invalidate-show-buffer))

    (setq mh-seq-list (mh-read-folder-sequences mh-current-folder nil))
    (mh-unmark-all-headers t)
    (mh-notate-user-sequences)
    (message "Processing deletes and refiles for %s...done" folder)))


(defun mh-update-unseen ()
  ;; Push updates to the Unseen sequence out to MH.
  (if mh-seen-list
      (progn
	(if (mh-seq-to-msgs mh-unseen-seq)
	    (mh-undefine-sequence mh-unseen-seq mh-seen-list))
	(setq mh-seen-list nil))))


(defun mh-delete-scan-msgs (msgs)
  ;; Delete the scan listing lines for each of the msgs in the LIST.
  ;; Optimized for speed (i.e., no regular expressions).
  (setq msgs (sort msgs (function <)))	;okay to clobber msgs
  (save-excursion
    (mh-first-msg)
    (while (and msgs (< (point) (point-max)))
      (cond ((equal (mh-get-msg-num nil) (car msgs))
	     (delete-region (point) (save-excursion (forward-line) (point)))
	     (setq msgs (cdr msgs)))
	    (t
	     (forward-line))))))


(defun mh-outstanding-commands-p ()
  ;; Returns non-nil if there are outstanding deletes or refiles.
  (or mh-delete-list mh-refile-list))



;;; Basic sequence handling

(defun mh-delete-seq-locally (seq)
  ;; Remove mh-e's record of SEQUENCE.
  (let ((entry (mh-find-seq seq)))
    (setq mh-seq-list (delq entry mh-seq-list))))

(defun mh-read-folder-sequences (folder save-refiles)
  ;; Read and return the predefined sequences for a FOLDER.
  ;; If SAVE-REFILES is non-nil, then keep the sequences
  ;; that note messages to be refiled.
  (let ((seqs ()))
    (cond (save-refiles
	    (mh-mapc (function (lambda (seq) ; Save the refiling sequences
				 (if (mh-folder-name-p (mh-seq-name seq))
				     (setq seqs (cons seq seqs)))))
		     mh-seq-list)))
    (save-excursion
      (if (eq 0 (mh-exec-cmd-quiet nil "mark" folder "-list"))
	  (progn
	    ;; look for name in line of form "cur: 4" or "myseq (private): 23"
	    (while (re-search-forward "^[^: ]+" nil t)
	      (setq seqs (cons (mh-make-seq (intern (buffer-substring
						     (match-beginning 0)
						     (match-end 0)))
					    (mh-read-msg-list))
			       seqs)))
	    (delete-region (point-min) (point))))) ; avoid race with mh-process-daemon
    seqs))

(defun mh-read-msg-list ()
  ;; Return a list of message numbers from the current point to the end of
  ;; the line.
  (let ((msgs ())
	(end-of-line (save-excursion (end-of-line) (point)))
	num)
    (while (re-search-forward "[0-9]+" end-of-line t)
      (setq num (string-to-int (buffer-substring (match-beginning 0)
						 (match-end 0))))
      (cond ((looking-at "-")		; Message range
	     (forward-char 1)
	     (re-search-forward "[0-9]+" end-of-line t)
	     (let ((num2 (string-to-int (buffer-substring (match-beginning 0)
							  (match-end 0)))))
	       (if (< num2 num)
		   (error "Bad message range: %d-%d" num num2))
	       (while (<= num num2)
		 (setq msgs (cons num msgs))
		 (setq num (1+ num)))))
	    ((not (zerop num)) (setq msgs (cons num msgs)))))
    msgs))

(defun mh-notate-user-sequences ()
  ;; Mark the scan listing of all messages in user-defined sequences.
  (let ((seqs mh-seq-list)
	name)
    (while seqs
      (setq name (mh-seq-name (car seqs)))
      (if (not (mh-internal-seq name))
	  (mh-notate-seq name ?% (1+ mh-cmd-note)))
      (setq seqs (cdr seqs)))))


(defun mh-internal-seq (name)
  ;; Return non-NIL if NAME is the name of an internal mh-e sequence.
  (or (memq name '(answered cur deleted forwarded printed))
      (eq name mh-unseen-seq)
      (eq name mh-previous-seq)
      (mh-folder-name-p name)))


(defun mh-delete-msg-from-seq (msg seq &optional internal-flag)
  "Delete MESSAGE from SEQUENCE.  MESSAGE defaults to displayed message.
  From Lisp, optional third arg INTERNAL non-nil means do not
  inform MH of the change."
  (interactive (list (mh-get-msg-num t)
		     (mh-read-seq-default "Delete from" t)
		     nil))
  (let ((entry (mh-find-seq seq)))
    (cond (entry
	   (mh-notate-if-in-one-seq msg ?  (1+ mh-cmd-note) (mh-seq-name entry))
	   (if (not internal-flag)
	       (mh-undefine-sequence seq msg))
	   (setcdr entry (delq msg (mh-seq-msgs entry)))))))


(defun mh-undefine-sequence (seq msgs)
  ;; Remove from the SEQUENCE the MSGS, which may be a list or single msg.
  (mh-exec-cmd "mark" mh-current-folder "-delete"
	       "-sequence" (symbol-name seq)
	       msgs))


(defun mh-define-sequence (seq msgs)
  ;; Define the SEQUENCE to contain the list of MSGS.
  ;; Do not mark pseudo-sequences or empty sequences.
  ;; Signals an error if SEQUENCE is an illegal name.
  (if (and msgs
	   (not (mh-folder-name-p seq)))
      (save-excursion
	(mh-exec-cmd-error nil "mark" mh-current-folder "-add" "-zero"
			   "-sequence" (symbol-name seq)
			   msgs))))


(defun mh-define-sequences (seq-list)
  ;; Define the sequences in SEQ-LIST.
  (mh-map-over-seqs 'mh-define-sequence seq-list))


(defun mh-map-over-seqs (func seq-list)
  ;; Apply the FUNCTION to each element in the list of SEQUENCES,
  ;; passing the sequence name and the list of messages as arguments.
  (while seq-list
    (funcall func (mh-seq-name (car seq-list)) (mh-seq-msgs (car seq-list)))
    (setq seq-list (cdr seq-list))))


(defun mh-notate-if-in-one-seq (msg notation offset seq)
  ;; If the MESSAGE is in only the SEQUENCE, then mark the scan listing of the
  ;; message with the CHARACTER at the given OFFSET from the beginning of the
  ;; listing line.
  (let ((in-seqs (mh-seq-containing-msg msg)))
    (if (and (eq seq (car in-seqs)) (null (cdr in-seqs)))
	(mh-notate msg notation offset))))


(defun mh-seq-containing-msg (msg)
  ;; Return a list of the sequences containing MESSAGE.
  (let ((l mh-seq-list)
	(seqs ()))
    (while l
      (if (memq msg (mh-seq-msgs (car l)))
	  (setq seqs (cons (mh-seq-name (car l)) seqs)))
      (setq l (cdr l)))
    seqs))




;;; User prompting commands.


(defun mh-read-msg-range (prompt)
  ;; Read a list of blank-separated items.
  (let* ((buf (read-string prompt))
	 (buf-size (length buf))
	 (start 0)
	 (input ()))
    (while (< start buf-size)
      (let ((next (read-from-string buf start buf-size)))
	(setq input (cons (car next) input))
	(setq start (cdr next))))
    (nreverse input)))



;;; Build the folder-mode keymap:

(suppress-keymap mh-folder-mode-map)
(define-key mh-folder-mode-map "q" 'mh-quit)
(define-key mh-folder-mode-map "b" 'mh-compat-quit)
(define-key mh-folder-mode-map "?" 'mh-msg-is-in-seq)
(define-key mh-folder-mode-map "%" 'mh-put-msg-in-seq)
(define-key mh-folder-mode-map "|" 'mh-pipe-msg)
(define-key mh-folder-mode-map "\ea" 'mh-edit-again)
(define-key mh-folder-mode-map "\e%" 'mh-delete-msg-from-seq)
(define-key mh-folder-mode-map "\e#" 'mh-delete-seq)
(define-key mh-folder-mode-map "\C-xn" 'mh-narrow-to-seq)
(define-key mh-folder-mode-map "\C-xw" 'mh-widen)
(define-key mh-folder-mode-map "\eb" 'mh-burst-digest)
(define-key mh-folder-mode-map "\eu" 'mh-undo-folder)
(define-key mh-folder-mode-map "\e " 'mh-page-digest)
(define-key mh-folder-mode-map "\e\177" 'mh-page-digest-backwards)
(define-key mh-folder-mode-map "\ed" 'mh-redistribute)
(define-key mh-folder-mode-map "\ee" 'mh-extract-rejected-mail)
(define-key mh-folder-mode-map "\ef" 'mh-visit-folder)
(define-key mh-folder-mode-map "\ek" 'mh-kill-folder)
(define-key mh-folder-mode-map "\el" 'mh-list-folders)
(define-key mh-folder-mode-map "\en" 'mh-store-msg)
(define-key mh-folder-mode-map "\ep" 'mh-pack-folder)
(define-key mh-folder-mode-map "\eq" 'mh-list-sequences)
(define-key mh-folder-mode-map "\es" 'mh-search-folder)
(define-key mh-folder-mode-map "\er" 'mh-rescan-folder)
(define-key mh-folder-mode-map "l" 'mh-print-msg)
(define-key mh-folder-mode-map "t" 'mh-toggle-showing)
(define-key mh-folder-mode-map "c" 'mh-copy-msg)
(define-key mh-folder-mode-map "i" 'mh-inc-folder)
(define-key mh-folder-mode-map "x" 'mh-execute-commands)
(define-key mh-folder-mode-map "e" 'mh-execute-commands)
(define-key mh-folder-mode-map "f" 'mh-forward)
(define-key mh-folder-mode-map "m" 'mh-send)
(define-key mh-folder-mode-map "s" 'mh-send)
(define-key mh-folder-mode-map "r" 'mh-reply)
(define-key mh-folder-mode-map "a" 'mh-reply)
(define-key mh-folder-mode-map "j" 'mh-goto-msg)
(define-key mh-folder-mode-map "g" 'mh-goto-msg)
(define-key mh-folder-mode-map "\e>" 'mh-last-msg)
(define-key mh-folder-mode-map "\177" 'mh-previous-page)
(define-key mh-folder-mode-map " " 'mh-page-msg)
(define-key mh-folder-mode-map "." 'mh-show)
(define-key mh-folder-mode-map "," 'mh-header-display)
(define-key mh-folder-mode-map "u" 'mh-undo)
(define-key mh-folder-mode-map "d" 'mh-delete-msg)
(define-key mh-folder-mode-map "\C-d" 'mh-delete-msg-no-motion)
(define-key mh-folder-mode-map "p" 'mh-previous-undeleted-msg)
(define-key mh-folder-mode-map "n" 'mh-next-undeleted-msg)
(define-key mh-folder-mode-map "o" 'mh-refile-msg)
(define-key mh-folder-mode-map "^" 'mh-refile-msg)
(define-key mh-folder-mode-map "\C-o" 'mh-write-msg-to-file)
(define-key mh-folder-mode-map ">" 'mh-write-msg-to-file)
(define-key mh-folder-mode-map "!" 'mh-refile-or-write-again)



;;;autoload the other mh-e parts

;;; mh-comp

(autoload 'mh-smail "mh-comp"
  "Compose and send mail with the MH mail system." t)
(autoload 'mh-smail-other-window "mh-comp"
  "Compose and send mail in other window with the MH mail system." t)
(autoload 'mh-edit-again "mh-comp"
  "Clean-up a draft or a message previously sent and make it resendable." t)
(autoload 'mh-extract-rejected-mail "mh-comp"
  "Extract a letter returned by the mail system and make it resendable." t)
(autoload 'mh-forward "mh-comp"
  "Forward MESSAGE(s) (default: displayed message)." t)
(autoload 'mh-redistribute "mh-comp"
  "Redistribute a letter." t)
(autoload 'mh-reply "mh-comp"
  "Reply to a MESSAGE (default: displayed message)." t)
(autoload 'mh-send "mh-comp"
  "Compose and send a letter." t)
(autoload 'mh-send-other-window "mh-comp"
  "Compose and send a letter in another window." t)
(autoload 'mh-letter-mode "mh-comp"
  "Mode for composing letters in mh-e." t)


;;; mh-funcs

(autoload 'mh-burst-digest "mh-funcs"
  "Burst apart the current message, which should be a digest." t)
(autoload 'mh-copy-msg "mh-funcs"
  "Copy specified MESSAGE(s) to another FOLDER without deleting them." t)
(autoload 'mh-kill-folder "mh-funcs"
  "Remove the current folder." t)
(autoload 'mh-list-folders "mh-funcs"
  "List mail folders." t)
(autoload 'mh-pack-folder "mh-funcs"
  "Renumber the messages of a folder to be 1..n." t)
(autoload 'mh-pipe-msg "mh-funcs"
  "Pipe the current message through the given shell COMMAND." t)
(autoload 'mh-page-digest "mh-funcs"
  "Advance displayed message to next digested message." t)
(autoload 'mh-page-digest-backwards "mh-funcs"
  "Back up displayed message to previous digested message." t)
(autoload 'mh-print-msg "mh-funcs"
  "Print MESSAGE(s) (default: displayed message) on a line printer." t)
(autoload 'mh-sort-folder "mh-funcs"
  "Sort the messages in the current folder by date." t)
(autoload 'mh-undo-folder "mh-funcs"
  "Undo all commands in current folder." t)
(autoload 'mh-store-msg "mh-funcs"
  "Store the file(s) contained in the current message into DIRECTORY.
The message can contain a shar file or uuencoded file." t)
(autoload 'mh-store-buffer "mh-funcs"
  "Store the file(s) contained in the current buffer into DIRECTORY.
The buffer can contain a shar file or uuencoded file." t)


;;; mh-pick

(autoload 'mh-search-folder "mh-pick"
  "Search FOLDER for messages matching a pattern." t)

;;; mh-seq

(autoload 'mh-put-msg-in-seq "mh-seq"
  "Add MESSAGE(s) (default: displayed message) to SEQUENCE." t)
(autoload 'mh-delete-seq "mh-seq"
  "Delete the SEQUENCE." t)
(autoload 'mh-list-sequences "mh-seq"
  "List the sequences defined in FOLDER." t)
(autoload 'mh-msg-is-in-seq "mh-seq"
  "Display the sequences that contain MESSAGE (default: displayed message)." t)
(autoload 'mh-narrow-to-seq "mh-seq"
  "Restrict display of this folder to just messages in a sequence." t)
(autoload 'mh-widen "mh-seq"
  "Remove restrictions from current folder, thereby showing all messages." t)
(autoload 'mh-rename-seq "mh-seq"
  "Rename a SEQUENCE to have a new NAME." t)

;;; mh-e.el ends here
