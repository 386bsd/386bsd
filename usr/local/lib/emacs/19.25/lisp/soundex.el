;;; soundex.el -- implement Soundex algorithm

;; Copyright (C) 1993 Free Software Foundation, Inc.

;; Author: Christian Plaunt <chris@bliss.berkeley.edu>
;; Maintainer: FSF 
;; Keywords: matching
;; Created: Sat May 15 14:48:18 1993

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

;; The Soundex algorithm maps English words into representations of
;; how they sound.  Words with vaguely similar sound map to the same string.

;;; Code: 

(defvar soundex-alist
  '((?A . nil) (?E . nil) (?H . nil) (?I . nil) (?O . nil) (?U . nil)
    (?W . nil) (?Y . nil) (?B . "1") (?F . "1") (?P . "1") (?V . "1")
    (?C . "2") (?G . "2") (?J . "2") (?K . "2") (?Q . "2") (?S . "2")
    (?X . "2") (?Z . "2") (?D . "3") (?T . "3") (?L . "4") (?M . "5")
    (?N . "5") (?R . "6"))
  "Alist of chars-to-key-code for building Soundex keys.")

(defun soundex (word)
  "Return a Soundex key for WORD.
Implemented as described in:
Knuth, Donald E. \"The Art of Computer Programming, Vol. 3: Sorting
and Searching\", Addison-Wesley (1973), pp. 391-392."
  (let* ((word (upcase word)) (length (length word))
	 (code (cdr (assq (aref word 0) soundex-alist)))
	 (key (substring word 0 1)) (index 1) (prev-code code))
    ;; once we have a four char key, were done
    (while (and (> 4 (length key)) (< index length))
      ;; look up the code for each letter in word at index
      (setq code (cdr (assq (aref word index) soundex-alist))
	    index (1+ index)
	    ;; append code to key unless the same codes belong to
	    ;; adjacent letters in the original string
	    key (concat key (if (or (null code) (string= code prev-code))
				()
			      code))
	    prev-code code))
    ;; return a key that is 4 chars long and padded by "0"s if needed
    (if (> 4 (length key))
	(substring (concat key "000") 0 4)
      key)))

;(defvar soundex-test
;  '("Euler" "Gauss" "Hilbert" "Knuth" "Lloyd" "Lukasiewicz"
;    "Ellery" "Ghosh" "Heilbronn" "Kant" "Ladd" "Lissajous")
;  "\n  Knuth's names to demonstrate the Soundex algorithm.")
;
;(mapcar 'soundex soundex-test)
;("E460" "G200" "H416" "K530" "L300" "L222"
; "E460" "G200" "H416" "K530" "L300" "L222")

;; soundex.el ends here
