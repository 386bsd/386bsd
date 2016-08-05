;;; menu-bar.el --- define a default menu bar.

;; Author: RMS
;; Keywords: internal

;; Copyright (C) 1993, 1994 Free Software Foundation, Inc.

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

;; Don't clobber an existing menu-bar keymap, to preserve any menu-bar key
;; definitions made in loaddefs.el.
(or (lookup-key global-map [menu-bar])
    (define-key global-map [menu-bar] (make-sparse-keymap "menu-bar")))
(defvar menu-bar-help-menu (make-sparse-keymap "Help"))
;; Put Help item last.
(setq menu-bar-final-items '(help))
(define-key global-map [menu-bar help] (cons "Help" menu-bar-help-menu))
(defvar menu-bar-edit-menu (make-sparse-keymap "Edit"))
(define-key global-map [menu-bar edit] (cons "Edit" menu-bar-edit-menu))
(defvar menu-bar-file-menu (make-sparse-keymap "File"))
(define-key global-map [menu-bar file] (cons "File" menu-bar-file-menu))

(define-key menu-bar-file-menu [exit-emacs]
  '("Exit Emacs" . save-buffers-kill-emacs))
(define-key menu-bar-file-menu [kill-buffer]
  '("Kill Buffer" . kill-this-buffer))
(define-key menu-bar-file-menu [delete-frame] '("Delete Frame" . delete-frame))
(define-key menu-bar-file-menu [epatch]
  '("Apply Patch" . menu-bar-epatch-menu))
(define-key menu-bar-file-menu [ediff]
  '("Compare Files" . menu-bar-ediff-menu))
(define-key menu-bar-file-menu [emerge] '("Emerge" . menu-bar-emerge-menu))
(define-key menu-bar-file-menu [calendar] '("Calendar" . calendar))
(define-key menu-bar-file-menu [rmail] '("Read Mail" . rmail))
(define-key menu-bar-file-menu [gnus] '("Read Net News" . gnus))
(define-key menu-bar-file-menu [bookmark]
  '("Bookmarks" . menu-bar-bookmark-map))
(define-key menu-bar-file-menu [print-buffer] '("Print Buffer" . print-buffer))
(define-key menu-bar-file-menu [revert-buffer]
  '("Revert Buffer" . revert-buffer))
(define-key menu-bar-file-menu [write-file]
  '("Save Buffer As..." . write-file))
(define-key menu-bar-file-menu [save-buffer] '("Save Buffer" . save-buffer))
(define-key menu-bar-file-menu [dired] '("Open Directory..." . dired))
(define-key menu-bar-file-menu [open-file] '("Open File..." . find-file))
(define-key menu-bar-file-menu [make-frame] '("Make New Frame" . make-frame))

(define-key menu-bar-edit-menu [spell] '("Spell" . ispell-menu-map))
(define-key menu-bar-edit-menu [fill] '("Fill" . fill-region))
(define-key menu-bar-edit-menu [clear] '("Clear" . delete-region))
(define-key menu-bar-edit-menu [query-replace]
  '("Query Replace" . query-replace))
(define-key menu-bar-edit-menu [re-search-back]
  '("Regexp Search Backwards" . re-search-backward))
(define-key menu-bar-edit-menu [search-back]
  '("Search Backwards" . search-backward))
(define-key menu-bar-edit-menu [re-search-fwd]
  '("Regexp Search" . re-search-forward))
(define-key menu-bar-edit-menu [search-fwd]
  '("Search" . search-forward))
(define-key menu-bar-edit-menu [choose-next-paste]
  '("Choose Next Paste >" . mouse-menu-choose-yank))
(define-key menu-bar-edit-menu [paste] '("Paste" . yank))
(define-key menu-bar-edit-menu [copy] '("Copy" . kill-ring-save))
(define-key menu-bar-edit-menu [cut] '("Cut" . kill-region))
(define-key menu-bar-edit-menu [undo] '("Undo" . undo))

(put 'fill-region 'menu-enable 'mark-active)
(put 'kill-region 'menu-enable 'mark-active)
(put 'kill-ring-save 'menu-enable 'mark-active)
(put 'yank 'menu-enable '(x-selection-exists-p))
(put 'delete-region 'menu-enable 'mark-active)
(put 'undo 'menu-enable '(if (eq last-command 'undo)
			     pending-undo-list
			   (consp buffer-undo-list)))
(put 'query-replace 'menu-enable (not buffer-read-only))

(autoload 'ispell-menu-map "ispell" nil t 'keymap)

;; These are alternative definitions for the cut, paste and copy
;; menu items.  Use them if your system expects these to use the clipboard

(put 'clipboard-kill-region 'menu-enable 'mark-active)
(put 'clipboard-kill-ring-save 'menu-enable 'mark-active)
(put 'clipboard-yank 'menu-enable
     '(or (x-selection-exists-p) (x-selection-exists-p 'CLIPBOARD)))

(defun clipboard-yank ()
  "Reinsert the last stretch of killed text, or the clipboard contents."
  (interactive)
  (let ((x-select-enable-clipboard t))
    (yank)))

(defun clipboard-kill-ring-save (beg end)
  "Copy region to kill ring, and save in the X clipboard."
  (interactive "r")
  (let ((x-select-enable-clipboard t))
    (kill-ring-save beg end)))

(defun clipboard-kill-region (beg end)
  "Kill the region, and save it in the X clipboard."
  (interactive "r")
  (let ((x-select-enable-clipboard t))
    (kill-region beg end)))

(defun menu-bar-enable-clipboard ()
  "Make the menu bar CUT, PASTE and COPY items use the clipboard."
  (interactive)
  ;; We can't use constant list structure here because it becomes pure,
  ;; and because it gets modified with cache data.
  (define-key menu-bar-edit-menu [paste]
    (cons "Paste" 'clipboard-yank))
  (define-key menu-bar-edit-menu [copy]
    (cons "Copy" 'clipboard-kill-ring-save))
  (define-key menu-bar-edit-menu [cut]
    (cons "Cut" 'clipboard-kill-region)))

;; Sun expects these commands on these keys, so why not?
(define-key global-map [f20] 'clipboard-kill-region)
(define-key global-map [f16] 'clipboard-kill-ring-save)
(define-key global-map [f18] 'clipboard-yank)

(define-key menu-bar-help-menu [emacs-version]
  '("Show Version" . emacs-version))
(define-key menu-bar-help-menu [report-emacs-bug]
  '("Send Bug Report" . report-emacs-bug))
(define-key menu-bar-help-menu [emacs-tutorial]
  '("Emacs Tutorial" . help-with-tutorial))
(define-key menu-bar-help-menu [man] '("Man..." . manual-entry))
(define-key menu-bar-help-menu [describe-variable]
  '("Describe Variable..." . describe-variable))
(define-key menu-bar-help-menu [describe-function]
  '("Describe Function..." . describe-function))
(define-key menu-bar-help-menu [describe-key]
  '("Describe Key..." . describe-key))
(define-key menu-bar-help-menu [list-keybindings]
  '("List Keybindings" . describe-bindings))
(define-key menu-bar-help-menu [command-apropos]
  '("Command Apropos..." . command-apropos))
(define-key menu-bar-help-menu [describe-mode]
  '("Describe Mode" . describe-mode))
(define-key menu-bar-help-menu [info] '("Info" . info))

(define-key menu-bar-help-menu [emacs-news] '("Emacs News" . view-emacs-news))
(defun kill-this-buffer ()	; for the menubar
  "Kills the current buffer."
  (interactive)
  (kill-buffer (current-buffer)))

(defun kill-this-buffer-enabled-p ()
  (let ((count 0)
	(buffers (buffer-list)))
    (while buffers
      (or (string-match "^ " (buffer-name (car buffers)))
	  (setq count (1+ count)))
      (setq buffers (cdr buffers)))
    (> count 1)))

(put 'save-buffer 'menu-enable '(buffer-modified-p))
(put 'revert-buffer 'menu-enable
     '(or revert-buffer-function revert-buffer-insert-file-contents-function
	  (and (buffer-file-name)
	       (or (buffer-modified-p)
		   (not (verify-visited-file-modtime (current-buffer)))))))
;; Permit deleting frame if it would leave a visible or iconified frame.
(put 'delete-frame 'menu-enable
     '(let ((frames (frame-list))
	    (count 0))
	(while frames
	  (if (cdr (assq 'visibility (frame-parameters (car frames))))
	      (setq count (1+ count)))
	  (setq frames (cdr frames)))
	(> count 1)))
(put 'kill-this-buffer 'menu-enable '(kill-this-buffer-enabled-p))

(put 'advertised-undo 'menu-enable
     '(and (not (eq t buffer-undo-list))
	   (if (eq last-command 'undo)
	       (and (boundp 'pending-undo-list)
		    pending-undo-list)
	     buffer-undo-list)))

(defvar yank-menu-length 100
  "*Maximum length of an item in the menu for \
\\[mouse-menu-choose-yank].")

(defun mouse-menu-choose-yank (event)
  "Pop up a menu of the kill-ring for selection with the mouse.
The kill-ring-yank-pointer is moved to the selected element.
A subsequent \\[yank] yanks the choice just selected."
  (interactive "e")
  (let* ((count 0)
	 (menu (mapcar (lambda (string)
			 (if (> (length string) yank-menu-length)
			     (setq string (substring string
						     0 yank-menu-length)))
			 (prog1 (cons string count)
			   (setq count (1+ count))))
		       kill-ring))
	 (arg (x-popup-menu event 
			    (list "Yank Menu"
				  (cons "Choose Next Yank" menu)))))
    ;; A mouse click outside the menu returns nil.
    ;; Avoid a confusing error from passing nil to rotate-yank-pointer.
    ;; XXX should this perhaps do something other than simply return? -rm
    (if arg
	(progn
	  ;; We don't use `rotate-yank-pointer' because we want to move
	  ;; relative to the beginning of kill-ring, not the current
	  ;; position.  Also, that would ask for any new X selection and
	  ;; thus change the list of items the user just chose from, which
	  ;; would be highly confusing.
	  (setq kill-ring-yank-pointer (nthcdr arg kill-ring))
	  (if (interactive-p)
	      (message "The next yank will insert the selected text.")
	    (current-kill 0))))))
(put 'mouse-menu-choose-yank 'menu-enable 'kill-ring)

(define-key global-map [menu-bar buffer] '("Buffers" . menu-bar-buffers))

(defalias 'menu-bar-buffers (make-sparse-keymap "Buffers"))

(defvar complex-buffers-menu-p nil
  "*Non-nil says, offer a choice of actions after you pick a buffer.
This applies to the Buffers menu from the menu bar.")

(defvar buffers-menu-max-size 10
  "*Maximum number of entries which may appear on the Buffers menu.
If this is 10, then only the ten most-recently-selected buffers are shown.
If this is nil, then all buffers are shown.
A large number or nil slows down menu responsiveness.")

(defvar list-buffers-directory nil)

(defun menu-bar-select-buffer ()
  (interactive)
  (switch-to-buffer last-command-event))

(defun menu-bar-select-frame ()
  (interactive)
  (make-frame-visible last-command-event)
  (raise-frame last-command-event)
  (select-frame last-command-event))

(defvar menu-bar-update-buffers-last-buffers nil)
(defvar menu-bar-update-buffers-last-frames nil)

(defun menu-bar-update-buffers ()
  (let ((buffers (buffer-list))
	(frames (frame-list))
	buffers-info
	buffers-menu frames-menu)
    (setq buffers-info
	  (mapcar (function (lambda (buffer)
			      (list buffer (buffer-modified-p buffer)
				    (save-excursion
				      (set-buffer buffer)
				      buffer-read-only))))
		  buffers))
    (if (and (equal buffers-info menu-bar-update-buffers-last-buffers)
	     (equal frames menu-bar-update-buffers-last-frames))
	nil
      (setq menu-bar-update-buffers-last-buffers buffers-info)
      (setq menu-bar-update-buffers-last-frames frames)
      ;; If requested, list only the N most recently selected buffers.
      (if (and (integerp buffers-menu-max-size)
	       (> buffers-menu-max-size 1))
	  (if (> (length buffers) buffers-menu-max-size)
	      (setcdr (nthcdr buffers-menu-max-size buffers) nil)))

      ;; Make the menu of buffers proper.
      (setq buffers-menu
	    (cons "Select Buffer"
		  (let ((tail buffers)
			(maxbuf 0)
			(maxlen 0)
			alist
			head)
		    (while tail
		      (or (eq ?\ (aref (buffer-name (car tail)) 0))
			  (setq maxbuf
				(max maxbuf
				     (length (buffer-name (car tail))))))
		      (setq tail (cdr tail)))
		    (setq tail buffers)
		    (while tail
		      (let ((elt (car tail)))
			(or (eq ?\ (aref (buffer-name elt) 0))
			    (setq alist (cons
					 (cons
					  (format
					   (format "%%%ds  %%s%%s  %%s"
						   maxbuf)
					   (buffer-name elt)
					   (if (buffer-modified-p elt)
					       "*" " ")
					   (save-excursion
					     (set-buffer elt)
					     (if buffer-read-only "%" " "))
					   (or (buffer-file-name elt)
					       (save-excursion
						 (set-buffer elt)
						 list-buffers-directory)
					       ""))
					  elt)
					 alist)))
			(and alist (> (length (car (car alist))) maxlen)
			     (setq maxlen (length (car (car alist))))))
		      (setq tail (cdr tail)))
		    (setq alist (nreverse alist))
		    (nconc (mapcar '(lambda (pair)
				      ;; This is somewhat risque, to use
				      ;; the buffer name itself as the event type
				      ;; to define, but it works.
				      ;; It would not work to use the buffer
				      ;; since a buffer as an event has its
				      ;; own meaning.
				      (nconc (list (buffer-name (cdr pair))
						   (car pair)
						   (cons nil nil))
					     'menu-bar-select-buffer))
				   alist)
			   (list (cons 'list-buffers
				       (cons
					(concat (make-string (max (- (/ maxlen
									2)
								     8)
								  0) ?\ )
						"List All Buffers")
					'list-buffers)))))))


      ;; Make a Frames menu if we have more than one frame.
      (if (cdr frames)
	  (setq frames-menu
		(cons "Select Frame"
		      (mapcar '(lambda (frame)
				 (nconc (list frame
					      (cdr (assq 'name
							 (frame-parameters frame)))
					      (cons nil nil))
					'menu-bar-select-frame))
			      frames))))
      (if buffers-menu
	  (setq buffers-menu (cons 'keymap buffers-menu)))
      (if frames-menu
	  (setq frames-menu (cons 'keymap frames-menu)))
      (define-key global-map [menu-bar buffer]
	(cons "Buffers"
	      (if (and buffers-menu frames-menu)
		  (list 'keymap "Buffers and Frames"
			(cons 'buffers (cons "Buffers" buffers-menu))
			(cons 'frames (cons "Frames" frames-menu)))
		(or buffers-menu frames-menu 'undefined)))))))

(add-hook 'menu-bar-update-hook 'menu-bar-update-buffers)

;; this version is too slow
;;;(defun format-buffers-menu-line (buffer)
;;;  "Returns a string to represent the given buffer in the Buffer menu.
;;;nil means the buffer shouldn't be listed.  You can redefine this."
;;;  (if (string-match "\\` " (buffer-name buffer))
;;;      nil
;;;    (save-excursion
;;;     (set-buffer buffer)
;;;     (let ((size (buffer-size)))
;;;       (format "%s%s %-19s %6s %-15s %s"
;;;	       (if (buffer-modified-p) "*" " ")
;;;	       (if buffer-read-only "%" " ")
;;;	       (buffer-name)
;;;	       size
;;;	       mode-name
;;;	       (or (buffer-file-name) ""))))))

(defun menu-bar-mode (flag)
  "Toggle display of a menu bar on each frame.
This command applies to all frames that exist and frames to be
created in the future.
With a numeric argument, if the argument is negative,
turn off menu bars; otherwise, turn on menu bars."
 (interactive "P")

 ;; Obtain the current setting by looking at default-frame-alist.
 (let ((menu-bar-mode
	(not (zerop (let ((assq (assq 'menu-bar-lines default-frame-alist)))
		      (if assq (cdr assq) 0))))))

   ;; Tweedle it according to the argument.
   (setq menu-bar-mode (if (null flag) (not menu-bar-mode)
			 (> (prefix-numeric-value flag) 0)))

   ;; Apply it to default-frame-alist.
   (let ((parameter (assq 'menu-bar-lines default-frame-alist)))
     (if (consp parameter)
	 (setcdr parameter (if menu-bar-mode 1 0))
       (setq default-frame-alist
	     (cons (cons 'menu-bar-lines (if menu-bar-mode 1 0))
		   default-frame-alist))))

   ;; Apply it to existing frames.
   (let ((frames (frame-list)))
     (while frames
       (let ((height (cdr (assq 'height (frame-parameters (car frames))))))
	 (modify-frame-parameters (car frames)
				  (list (cons 'menu-bar-lines
					    (if menu-bar-mode 1 0))))
	 (modify-frame-parameters (car frames)
				  (list (cons 'height height))))
       (setq frames (cdr frames))))))

(provide 'menu-bar)

;;; menu-bar.el ends here
