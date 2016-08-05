;;; rsz-mini.el --- dynamically resize minibuffer to display entire contents

;;; Copyright (C) 1990, 1993, 1994 Free Software Foundation, Inc.

;;; Author: Noah Friedman <friedman@prep.ai.mit.edu>
;;;         Roland McGrath <roland@prep.ai.mit.edu>
;;; Maintainer: friedman@prep.ai.mit.edu
;;; Keywords: minibuffer, window, frame, display
;;; Status: Known to work in FSF GNU Emacs 19.23.
;;; $Id: rsz-mini.el,v 1.3 1994/05/20 17:43:40 friedman Exp $

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

;;; This package allows the entire contents (or as much as possible) of the
;;; minibuffer to be visible at once when typing.  As the end of a line is
;;; reached, the minibuffer will resize itself.  When the user is done
;;; typing, the minibuffer will return to its original size.

;;; In window systems where it is possible to have a frame in which the
;;; minibuffer is the only window, the frame itself can be resized.  In FSF
;;; GNU Emacs 19.22 and earlier, the frame may not be properly returned to
;;; its original size after it ceases to be active because
;;; `minibuffer-exit-hook' didn't exist until version 19.23.

;;; Note that the minibuffer and echo area are not the same!  They simply
;;; happen to occupy roughly the same place on the frame.  Messages put in
;;; the echo area will not cause any resizing by this package.

;;; This package is considered a minor mode but it doesn't put anything in
;;; minor-mode-alist because this mode is specific to the minibuffer, which
;;; has no mode line.

;;; To use this package, put the following in your .emacs:
;;;
;;;     (autoload 'resize-minibuffer-mode "rsz-mini" nil t)
;;;
;;; Invoking the command `resize-minibuffer-mode' will then enable this mode.
;;; Simply loading this file will enable resize-minibuffer-mode.

;;; Code:


;;;###autoload
(defvar resize-minibuffer-mode nil
  "*If non-`nil', resize the minibuffer so its entire contents are visible.")

;;;###autoload
(defvar resize-minibuffer-window-max-height nil
  "*Maximum size the minibuffer window is allowed to become.
If less than 1 or not a number, the limit is the height of the frame in
which the active minibuffer window resides.")

;;;###autoload
(defvar resize-minibuffer-window-exactly t
  "*If non-`nil', make minibuffer exactly the size needed to display all its contents.
Otherwise, the minibuffer window can temporarily increase in size but
never get smaller while it is active.")


;;;###autoload
(defvar resize-minibuffer-frame nil
  "*If non-`nil' and the active minibuffer is the sole window in its frame, allow changing the frame height.")

;;;###autoload
(defvar resize-minibuffer-frame-max-height nil
  "*Maximum size the minibuffer frame is allowed to become.
If less than 1 or not a number, there is no limit.")

;;;###autoload
(defvar resize-minibuffer-frame-exactly nil
  "*If non-`nil', make minibuffer frame exactly the size needed to display all its contents.
Otherwise, the minibuffer frame can temporarily increase in size but
never get smaller while it is active.")


;;;###autoload
(defun resize-minibuffer-mode (&optional prefix)
  "Enable or disable resize-minibuffer mode.
A negative prefix argument disables this mode.  A positive argument or
argument of 0 enables it.

When this minor mode is enabled, the minibuffer is dynamically resized to
contain the entire region of text put in it as you type.

The variable `resize-minibuffer-mode' is set to t or nil depending on
whether this mode is active or not.

The maximum height to which the minibuffer can grow is controlled by the
variable `resize-minibuffer-window-max-height'.

The variable `resize-minibuffer-window-exactly' determines whether the
minibuffer window should ever be shrunk to make it no larger than needed to
display its contents.

When using a window system, it is possible for a minibuffer to be the sole
window in a frame.  Since that window is already its maximum size, the only
way to make more text visible at once is to increase the size of the frame.
The variable `resize-minibuffer-frame' controls whether this should be
done.  The variables `resize-minibuffer-frame-max-height' and
`resize-minibuffer-frame-exactly' are analogous to their window
counterparts."
  (interactive "p")
  (or prefix (setq prefix 0))
  (cond
   ((>= prefix 0)
    (setq resize-minibuffer-mode t))
   (t
    (setq resize-minibuffer-mode nil))))

(defun resize-minibuffer-setup ()
  (cond
   (resize-minibuffer-mode
    (cond
     ((and window-system
           (eq 'only (cdr (assq 'minibuffer (frame-parameters)))))
      (and resize-minibuffer-frame
           (progn
             (make-local-variable 'minibuffer-exit-hook)
             (add-hook 'minibuffer-exit-hook 'resize-minibuffer-frame-restore)
             (make-local-variable 'post-command-hook)
             (add-hook 'post-command-hook 'resize-minibuffer-frame))))
     (t
      (make-local-variable 'post-command-hook)
      (add-hook 'post-command-hook 'resize-minibuffer-window))))))

(defun resize-minibuffer-count-window-lines (&optional start end)
  "Return number of window lines occupied by text in region.
The number of window lines may be greater than the number of actual lines
in the buffer if any wrap on the display due to their length.

Optional arguments START and END default to point-min and point-max,
respectively."
  (or start (setq start (point-min)))
  (or end   (setq end   (point-max)))
  (if (= start end)
      0
    (save-excursion
      (save-restriction
        (widen)
	(narrow-to-region start end)
	(goto-char start)
        (vertical-motion (buffer-size))))))


;; Resize the minibuffer window to contain the minibuffer's contents.
;; The minibuffer must be the current window.
(defun resize-minibuffer-window ()
  (let ((height (window-height))
        (lines (1+ (resize-minibuffer-count-window-lines))))
    (and (numberp resize-minibuffer-window-max-height)
         (> resize-minibuffer-window-max-height 0)
         (setq lines (min lines resize-minibuffer-window-max-height)))
    (or (if resize-minibuffer-window-exactly
            (= lines height)
          (<= lines height))
        (enlarge-window (- lines height)))))


;; Resize the minibuffer frame to contain the minibuffer's contents.
;; The minibuffer frame must be the current frame.
(defun resize-minibuffer-frame ()
  (let ((height (frame-height))
        (lines (1+ (resize-minibuffer-count-window-lines))))
    (and (numberp resize-minibuffer-frame-max-height)
         (> resize-minibuffer-frame-max-height 0)
         (setq lines (min lines resize-minibuffer-frame-max-height)))
    (cond
     ((> lines height)
      (set-frame-size (selected-frame) (frame-width) lines))
     ((and resize-minibuffer-frame-exactly
           (> height (cdr (assq 'height minibuffer-frame-alist)))
           (< lines height))
      (set-frame-size (selected-frame) (frame-width) lines)))))

;; Restore the original height of the frame.
(defun resize-minibuffer-frame-restore ()
  (set-frame-size (selected-frame)
                  (frame-width)
                  (cdr (assq 'height minibuffer-frame-alist))))


(provide 'rsz-mini)

(add-hook 'minibuffer-setup-hook 'resize-minibuffer-setup)
(resize-minibuffer-mode)

;; rsz-mini.el ends here
