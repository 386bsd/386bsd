;;; compare-w.el --- compare text between windows for Emacs.

;; Copyright (C) 1986, 1989, 1993 Free Software Foundation, Inc.

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

;;; Commentary:

;; This package provides one entry point, compare-windows.  It compares
;; text starting from point in two adjacent windows, advancing point
;; until it finds a difference.  Option variables permit you to ignore
;; whitespace differences, or case differences, or both.

;;; Code:

(defvar compare-windows-whitespace "[ \t\n]+"
  "*Regexp that defines whitespace sequences for \\[compare-windows].
Changes in whitespace are optionally ignored.

The value of `compare-windows-whitespace' may instead be a function; this
function is called in each buffer, with point at the current scanning point.
The function's job is to categorize any whitespace around (including before)
point; it should also advance past any whitespace.

The function is passed one argument, the point where `compare-windows'
was originally called; it should not consider any text before that point.
If the function returns the same value for both buffers, then the
whitespace is considered to match, and is skipped.")

(defvar compare-ignore-case nil
  "*Non-nil means \\[compare-windows] ignores case differences.")

;;;###autoload
(defun compare-windows (ignore-whitespace)
  "Compare text in current window with text in next window.
Compares the text starting at point in each window,
moving over text in each one as far as they match.

A prefix arg means ignore changes in whitespace.
The variable `compare-windows-whitespace' controls how whitespace is skipped.
If `compare-ignore-case' is non-nil, changes in case are also ignored."
  (interactive "P")
  (let* (p1 p2 maxp1 maxp2 b1 b2 w2
	    success size
	    (opoint1 (point))
	    opoint2
	    (skip-whitespace (if ignore-whitespace
				 compare-windows-whitespace)))
    (setq p1 (point) b1 (current-buffer))
    (setq w2 (next-window (selected-window)))
    (if (eq w2 (selected-window))
	(error "No other window"))
    (setq p2 (window-point w2)
	  b2 (window-buffer w2))
    (setq opoint2 p2)
    (setq maxp1 (point-max))
    (save-excursion
      (set-buffer b2)
      (setq maxp2 (point-max)))

    (setq success t)
    (while success
      (setq success nil)
      ;; if interrupted, show how far we've gotten
      (goto-char p1)
      (set-window-point w2 p2)

      ;; If both buffers have whitespace next to point,
      ;; optionally skip over it.

      (and skip-whitespace
	   (save-excursion
	     (let (p1a p2a w1 w2 result1 result2)
	       (setq result1
		     (if (stringp skip-whitespace)
			 (compare-windows-skip-whitespace opoint1)
		       (funcall skip-whitespace opoint1)))
	       (setq p1a (point))
	       (set-buffer b2)
	       (goto-char p2)
	       (setq result2
		     (if (stringp skip-whitespace)
			 (compare-windows-skip-whitespace opoint2)
		       (funcall skip-whitespace opoint2)))
	       (setq p2a (point))
	       (if (or (stringp skip-whitespace)
		       (and result1 result2 (eq result1 result2)))
		   (setq p1 p1a
			 p2 p2a)))))

      ;; Try advancing comparing 1000 chars at a time.
      ;; When that fails, go 500 chars at a time, and so on.
      (let ((size 1000)
	    success-1
	    (case-fold-search compare-ignore-case))
	(while (> size 0)
	  (setq success-1 t)
	  ;; Try comparing SIZE chars at a time, repeatedly, till that fails.
	  (while success-1
	    (setq size (min size (- maxp1 p1) (- maxp2 p2)))
	    (setq success-1
		  (and (> size 0)
		       (= 0 (compare-buffer-substrings b2 p2 (+ size p2)
						       b1 p1 (+ size p1)))))
	    (if success-1
		(setq p1 (+ p1 size) p2 (+ p2 size)
		      success t)))
	  ;; If SIZE chars don't match, try fewer.
	  (setq size (/ size 2)))))

    (goto-char p1)
    (set-window-point w2 p2)
    (if (= (point) opoint1)
	(ding))))

;; Move forward over whatever might be called whitespace.
;; compare-windows-whitespace is a regexp that matches whitespace.
;; Match it at various starting points before the original point
;; and find the latest point at which a match ends.
;; Don't try starting points before START, though.
;; Value is non-nil if whitespace is found.

;; If there is whitespace before point, but none after,
;; then return t, but don't advance point.
(defun compare-windows-skip-whitespace (start)
  (let ((end (point))
	(beg (point))
	(opoint (point)))
    (while (or (and (looking-at compare-windows-whitespace)
		    (<= end (match-end 0))
		    ;; This match goes past END, so advance END.
		    (progn (setq end (match-end 0))
			   (> (point) start)))
	       (and (/= (point) start)
		    ;; Consider at least the char before point,
		    ;; unless it is also before START.
		    (= (point) opoint)))
      ;; keep going back until whitespace
      ;; doesn't extend to or past end
      (forward-char -1))
    (setq beg (point))
    (goto-char end)
    (or (/= beg opoint)
	(/= end opoint))))

(provide 'compare-w)

;;; compare-w.el ends here
