;;; vt100.el --- define VT100 function key sequences in function-key-map

;; Author: FSF
;; Keywords: terminals

;; Copyright (C) 1989, 1993 Free Software Foundation, Inc.
;;; This file is part of GNU Emacs.
;;;
;;; GNU Emacs is free software; you can redistribute it and/or modify
;;; it under the terms of the GNU General Public License as published by
;;; the Free Software Foundation; either version 2, or (at your option)
;;; any later version.
;;;
;;; GNU Emacs is distributed in the hope that it will be useful,
;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;; GNU General Public License for more details.
;;;
;;; You should have received a copy of the GNU General Public License
;;; along with GNU Emacs; see the file COPYING.  If not, write to
;;; the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

;;; Commentary:

;; Uses the Emacs 19 terminal initialization features --- won't work with 18.

;; Handles all VT100 clones, including the Apollo terminal.  Also handles
;; the VT200 --- its PF- and arrow- keys are different, but all those
;; are really set up by the terminal initialization code, which mines them
;; out of termcap.  This package is here to define the keypad comma, dash
;; and period (which aren't in termcap's repertoire) and the function for
;; changing from 80 to 132 columns & vv.

;;; Code:

;; Set up function-key-map entries that termcap and terminfo don't know.
(load "term/lk201" nil t)

;;; Controlling the screen width.
(defconst vt100-wide-mode (= (frame-width) 132)
  "t if vt100 is in 132-column mode.")

(defun vt100-wide-mode (&optional arg)
  "Toggle 132/80 column mode for vt100s.
With positive argument, switch to 132-column mode.
With negative argument, switch to 80-column mode."
 (interactive "P")
 (setq vt100-wide-mode 
	(if (null arg) (not vt100-wide-mode)
	  (> (prefix-numeric-value arg) 0)))
 (send-string-to-terminal (if vt100-wide-mode "\e[?3h" "\e[?3l"))
 (set-frame-width terminal-frame (if vt100-wide-mode 132 80)))

;;; vt100.el ends here
