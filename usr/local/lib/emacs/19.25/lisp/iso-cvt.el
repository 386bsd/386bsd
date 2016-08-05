;; iso-cvt.el -- translate to ISO 8859-1 from/to net/TeX conventions
;; Copyright � 1993 Free Software Foundation, Inc.
;; Was formerly called gm-lingo.el.

;; Author: Michael Gschwind <mike@vlsivie.tuwien.ac.at>
;; Keywords: tex, iso, latin, i18n

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
;; This lisp code server two purposes, both of which involve 
;; the translation of various conventions for representing European 
;; character sets to ISO 8859-1.

; Net support: 
; Various conventions exist in Newsgroups on how to represent national 
; characters. The functions provided here translate these net conventions 
; to ISO.
;
; Calling `iso-german' will turn the net convention for umlauts ("a etc.) 
; into ISO latin1 umlaute for easy reading.
; 'iso-spanish' will turn net conventions for representing spanish 
; to ISO latin1. (Note that accents are omitted in news posts most 
; of the time, only enye is escaped.)

; TeX support
; This mode installs hooks which change TeX files to ISO Latin-1 for 
; simplified editing. When the TeX file is saved, ISO latin1 characters are
; translated back to escape sequences.
;
; An alternative is a TeX style that handles 8 bit ISO files 
; (available on ftp.vlsivie.tuwien.ac.at in /pub/8bit)  
; - but these files are difficult to transmit ... so while the net is  
; still @ 7 bit this may be useful

;; TO DO:
; The net support should install hooks (like TeX support does) 
; which recognizes certains news groups and translates all articles from 
; those groups. 
;
; Cover more cases for translation (There is an infinite number of ways to 
; represent accented characters in TeX)

;; SEE ALSO:
; If you are interested in questions related to using the ISO 8859-1 
; characters set (configuring emacs, Unix, etc. to use ISO), then you
; can get the ISO 8859-1 FAQ via anonymous ftp from 
; ftp.vlsivie.tuwien.ac.at in /pub/bit/FAQ-ISO-8859-1

;;; Code:

(provide 'iso-cvt)

(defvar iso-spanish-trans-tab
  '(
    ("~n" "�")
    ("\([a-zA-Z]\)#" "\\1�")
    ("~N" "�")
    ("\\([-a-zA-Z\"`]\\)\"u" "\\1�")
    ("\\([-a-zA-Z\"`]\\)\"U" "\\1�")
    ("\\([-a-zA-Z]\\)'o" "\\1�")
    ("\\([-a-zA-Z]\\)'O" "\\�")
    ("\\([-a-zA-Z]\\)'e" "\\1�")
    ("\\([-a-zA-Z]\\)'E" "\\1�")
    ("\\([-a-zA-Z]\\)'a" "\\1�")
    ("\\([-a-zA-Z]\\)'A" "\\1A")
    ("\\([-a-zA-Z]\\)'i" "\\1�")
    ("\\([-a-zA-Z]\\)'I" "\\1�")
    )
  "Spanish translation table.")

(defun iso-translate-conventions (trans-tab)
  "Use the translation table argument to translate the current buffer."
  (interactive)
  (save-excursion
    (widen)
    (goto-char (point-min))
    (let ((work-tab trans-tab)
	  (buffer-read-only nil))
      (while work-tab
	(save-excursion
	  (let ((trans-this (car work-tab)))
	    (while (re-search-forward (car trans-this) nil t)
	      (replace-match (car (cdr trans-this)) nil nil)))
	  (setq work-tab (cdr work-tab)))))))

(defun iso-spanish ()
  "Translate net conventions for Spanish to ISO 8859-1."
  (interactive)
  (iso-translate-conventions iso-spanish-trans-tab))

(defvar iso-aggressive-german-trans-tab
  '(
    ("\"a" "�")
    ("\"A" "�")
    ("\"o" "�")
    ("\"O" "�")
    ("\"u" "�")
    ("\"U" "�")
    ("\"s" "�")
    ("\\\\3" "�")
    )
  "German translation table. 
This table uses an aggressive translation approach and may translate 
erroneously translate too much.")

(defvar iso-conservative-german-trans-tab
  '(
    ("\\([-a-zA-Z\"`]\\)\"a" "\\1�")
    ("\\([-a-zA-Z\"`]\\)\"A" "\\1�")
    ("\\([-a-zA-Z\"`]\\)\"o" "\\1�")
    ("\\([-a-zA-Z\"`]\\)\"O" "\\1�")
    ("\\([-a-zA-Z\"`]\\)\"u" "\\1�")
    ("\\([-a-zA-Z\"`]\\)\"U" "\\1�")
    ("\\([-a-zA-Z\"`]\\)\"s" "\\1�")
    ("\\([-a-zA-Z\"`]\\)\\\\3" "\\1�")
    )
  "German translation table.
This table uses a conservative translation approach and may translate too 
little.")


(defvar iso-german-trans-tab iso-aggressive-german-trans-tab 
  "Currently active translation table for German.")

(defun iso-german ()
 "Translate net conventions for German to ISO 8859-1."
 (interactive)
 (iso-translate-conventions iso-german-trans-tab))
 
(defvar iso-iso2tex-trans-tab
  '(
    ("�" "{\\\\\"a}")
    ("�" "{\\\\`a}")
    ("�" "{\\\\'a}")
    ("�" "{\\\\~a}")
    ("�" "{\\\\^a}")
    ("�" "{\\\\\"e}")
    ("�" "{\\\\`e}")
    ("�" "{\\\\'e}")
    ("�" "{\\\\^e}")
    ("�" "{\\\\\"\\\\i}")
    ("�" "{\\\\`\\\\i}")
    ("�" "{\\\\'\\\\i}")
    ("�" "{\\\\^\\\\i}")
    ("�" "{\\\\\"o}")
    ("�" "{\\\\`o}")
    ("�" "{\\\\'o}")
    ("�" "{\\\\~o}")
    ("�" "{\\\\^o}")
    ("�" "{\\\\\"u}")
    ("�" "{\\\\`u}")
    ("�" "{\\\\'u}")
    ("�" "{\\\\^u}")
    ("�" "{\\\\\"A}")
    ("�" "{\\\\`A}")
    ("�" "{\\\\'A}")
    ("�" "{\\\\~A}")
    ("�" "{\\\\^A}")
    ("�" "{\\\\\"E}")
    ("�" "{\\\\`E}")
    ("�" "{\\\\'E}")
    ("�" "{\\\\^E}")
    ("�" "{\\\\\"I}")
    ("�" "{\\\\`I}")
    ("�" "{\\\\'I}")
    ("�" "{\\\\^I}")
    ("�" "{\\\\\"O}")
    ("�" "{\\\\`O}")
    ("�" "{\\\\'O}")
    ("�" "{\\\\~O}")
    ("�" "{\\\\^O}")
    ("�" "{\\\\\"U}")
    ("�" "{\\\\`U}")
    ("�" "{\\\\'U}")
    ("�" "{\\\\^U}")
    ("�" "{\\\\~n}")
    ("�" "{\\\\~N}")
    ("�" "{\\\\c c}")
    ("�" "{\\\\c C}")
    ("�" "{\\\\ss}")
    ("�" "{?`}")
    ("�" "{!`}")
    )
  "Translation table for translating ISO 8859-1 characters to TeX sequences.")




(defun iso-iso2tex ()
 "Translate ISO 8859-1 characters to TeX sequences."
 (interactive)
 (iso-translate-conventions iso-iso2tex-trans-tab))


(defvar iso-tex2iso-trans-tab
  '(
    ("{\\\\\"a}" "�")
    ("{\\\\`a}" "�")
    ("{\\\\'a}" "�")
    ("{\\\\~a}" "�")
    ("{\\\\^a}" "�")
    ("{\\\\\"e}" "�")
    ("{\\\\`e}" "�")
    ("{\\\\'e}" "�")
    ("{\\\\^e}" "�")
    ("{\\\\\"\\\\i}" "�")
    ("{\\\\`\\\\i}" "�")
    ("{\\\\'\\\\i}" "�")
    ("{\\\\^\\\\i}" "�")
    ("{\\\\\"i}" "�")
    ("{\\\\`i}" "�")
    ("{\\\\'i}" "�")
    ("{\\\\^i}" "�")
    ("{\\\\\"o}" "�")
    ("{\\\\`o}" "�")
    ("{\\\\'o}" "�")
    ("{\\\\~o}" "�")
    ("{\\\\^o}" "�")
    ("{\\\\\"u}" "�")
    ("{\\\\`u}" "�")
    ("{\\\\'u}" "�")
    ("{\\\\^u}" "�")
    ("{\\\\\"A}" "�")
    ("{\\\\`A}" "�")
    ("{\\\\'A}" "�")
    ("{\\\\~A}" "�")
    ("{\\\\^A}" "�")
    ("{\\\\\"E}" "�")
    ("{\\\\`E}" "�")
    ("{\\\\'E}" "�")
    ("{\\\\^E}" "�")
    ("{\\\\\"I}" "�")
    ("{\\\\`I}" "�")
    ("{\\\\'I}" "�")
    ("{\\\\^I}" "�")
    ("{\\\\\"O}" "�")
    ("{\\\\`O}" "�")
    ("{\\\\'O}" "�")
    ("{\\\\~O}" "�")
    ("{\\\\^O}" "�")
    ("{\\\\\"U}" "�")
    ("{\\\\`U}" "�")
    ("{\\\\'U}" "�")
    ("{\\\\^U}" "�")
    ("{\\\\~n}" "�")
    ("{\\\\~N}" "�")
    ("{\\\\c c}" "�")
    ("{\\\\c C}" "�")
    ("\\\\\"{a}" "�")
    ("\\\\`{a}" "�")
    ("\\\\'{a}" "�")
    ("\\\\~{a}" "�")
    ("\\\\^{a}" "�")
    ("\\\\\"{e}" "�")
    ("\\\\`{e}" "�")
    ("\\\\'{e}" "�")
    ("\\\\^{e}" "�")
    ("\\\\\"{\\\\i}" "�")
    ("\\\\`{\\\\i}" "�")
    ("\\\\'{\\\\i}" "�")
    ("\\\\^{\\\\i}" "�")
    ("\\\\\"{i}" "�")
    ("\\\\`{i}" "�")
    ("\\\\'{i}" "�")
    ("\\\\^{i}" "�")
    ("\\\\\"{o}" "�")
    ("\\\\`{o}" "�")
    ("\\\\'{o}" "�")
    ("\\\\~{o}" "�")
    ("\\\\^{o}" "�")
    ("\\\\\"{u}" "�")
    ("\\\\`{u}" "�")
    ("\\\\'{u}" "�")
    ("\\\\^{u}" "�")
    ("\\\\\"{A}" "�")
    ("\\\\`{A}" "�")
    ("\\\\'{A}" "�")
    ("\\\\~{A}" "�")
    ("\\\\^{A}" "�")
    ("\\\\\"{E}" "�")
    ("\\\\`{E}" "�")
    ("\\\\'{E}" "�")
    ("\\\\^{E}" "�")
    ("\\\\\"{I}" "�")
    ("\\\\`{I}" "�")
    ("\\\\'{I}" "�")
    ("\\\\^{I}" "�")
    ("\\\\\"{O}" "�")
    ("\\\\`{O}" "�")
    ("\\\\'{O}" "�")
    ("\\\\~{O}" "�")
    ("\\\\^{O}" "�")
    ("\\\\\"{U}" "�")
    ("\\\\`{U}" "�")
    ("\\\\'{U}" "�")
    ("\\\\^{U}" "�")
    ("\\\\~{n}" "�")
    ("\\\\~{N}" "�")
    ("\\\\c{c}" "�")
    ("\\\\c{C}" "�")
    ("{\\\\ss}" "�")
    ("?`" "�")
    ("!`" "�")
    ("{?`}" "�")
    ("{!`}" "�")
    )
  "Translation table for translating TeX sequences to ISO 8859-1 characters. 
This table is not exhaustive (and due to TeX's power can never be). It only
contains commonly used sequences.")

(defun iso-tex2iso ()
 "Translate TeX sequences to ISO 8859-1 characters."
 (interactive)
 (iso-translate-conventions iso-tex2iso-trans-tab))

(defvar iso-gtex2iso-trans-tab
  '(
    ("{\\\\\"a}" "�")
    ("{\\\\`a}" "�")
    ("{\\\\'a}" "�")
    ("{\\\\~a}" "�")
    ("{\\\\^a}" "�")
    ("{\\\\\"e}" "�")
    ("{\\\\`e}" "�")
    ("{\\\\'e}" "�")
    ("{\\\\^e}" "�")
    ("{\\\\\"\\\\i}" "�")
    ("{\\\\`\\\\i}" "�")
    ("{\\\\'\\\\i}" "�")
    ("{\\\\^\\\\i}" "�")
    ("{\\\\\"i}" "�")
    ("{\\\\`i}" "�")
    ("{\\\\'i}" "�")
    ("{\\\\^i}" "�")
    ("{\\\\\"o}" "�")
    ("{\\\\`o}" "�")
    ("{\\\\'o}" "�")
    ("{\\\\~o}" "�")
    ("{\\\\^o}" "�")
    ("{\\\\\"u}" "�")
    ("{\\\\`u}" "�")
    ("{\\\\'u}" "�")
    ("{\\\\^u}" "�")
    ("{\\\\\"A}" "�")
    ("{\\\\`A}" "�")
    ("{\\\\'A}" "�")
    ("{\\\\~A}" "�")
    ("{\\\\^A}" "�")
    ("{\\\\\"E}" "�")
    ("{\\\\`E}" "�")
    ("{\\\\'E}" "�")
    ("{\\\\^E}" "�")
    ("{\\\\\"I}" "�")
    ("{\\\\`I}" "�")
    ("{\\\\'I}" "�")
    ("{\\\\^I}" "�")
    ("{\\\\\"O}" "�")
    ("{\\\\`O}" "�")
    ("{\\\\'O}" "�")
    ("{\\\\~O}" "�")
    ("{\\\\^O}" "�")
    ("{\\\\\"U}" "�")
    ("{\\\\`U}" "�")
    ("{\\\\'U}" "�")
    ("{\\\\^U}" "�")
    ("{\\\\~n}" "�")
    ("{\\\\~N}" "�")
    ("{\\\\c c}" "�")
    ("{\\\\c C}" "�")
    ("\\\\\"{a}" "�")
    ("\\\\`{a}" "�")
    ("\\\\'{a}" "�")
    ("\\\\~{a}" "�")
    ("\\\\^{a}" "�")
    ("\\\\\"{e}" "�")
    ("\\\\`{e}" "�")
    ("\\\\'{e}" "�")
    ("\\\\^{e}" "�")
    ("\\\\\"{\\\\i}" "�")
    ("\\\\`{\\\\i}" "�")
    ("\\\\'{\\\\i}" "�")
    ("\\\\^{\\\\i}" "�")
    ("\\\\\"{i}" "�")
    ("\\\\`{i}" "�")
    ("\\\\'{i}" "�")
    ("\\\\^{i}" "�")
    ("\\\\\"{o}" "�")
    ("\\\\`{o}" "�")
    ("\\\\'{o}" "�")
    ("\\\\~{o}" "�")
    ("\\\\^{o}" "�")
    ("\\\\\"{u}" "�")
    ("\\\\`{u}" "�")
    ("\\\\'{u}" "�")
    ("\\\\^{u}" "�")
    ("\\\\\"{A}" "�")
    ("\\\\`{A}" "�")
    ("\\\\'{A}" "�")
    ("\\\\~{A}" "�")
    ("\\\\^{A}" "�")
    ("\\\\\"{E}" "�")
    ("\\\\`{E}" "�")
    ("\\\\'{E}" "�")
    ("\\\\^{E}" "�")
    ("\\\\\"{I}" "�")
    ("\\\\`{I}" "�")
    ("\\\\'{I}" "�")
    ("\\\\^{I}" "�")
    ("\\\\\"{O}" "�")
    ("\\\\`{O}" "�")
    ("\\\\'{O}" "�")
    ("\\\\~{O}" "�")
    ("\\\\^{O}" "�")
    ("\\\\\"{U}" "�")
    ("\\\\`{U}" "�")
    ("\\\\'{U}" "�")
    ("\\\\^{U}" "�")
    ("\\\\~{n}" "�")
    ("\\\\~{N}" "�")
    ("\\\\c{c}" "�")
    ("\\\\c{C}" "�")
    ("{\\\\ss}" "�")
    ("?`" "�")
    ("!`" "�")
    ("{?`}" "�")
    ("{!`}" "�")
    ("\"a" "�")
    ("\"A" "�")
    ("\"o" "�")
    ("\"O" "�")
    ("\"u" "�")
    ("\"U" "�")
    ("\"s" "�")
    ("\\\\3" "�")
    )
  "Translation table for translating German TeX sequences to ISO 8859-1.
This table is not exhaustive (and due to TeX's power can never be).  It only
contains commonly used sequences.")

(defvar iso-iso2gtex-trans-tab
  '(
    ("�" "\"a")
    ("�" "{\\\\`a}")
    ("�" "{\\\\'a}")
    ("�" "{\\\\~a}")
    ("�" "{\\\\^a}")
    ("�" "{\\\\\"e}")
    ("�" "{\\\\`e}")
    ("�" "{\\\\'e}")
    ("�" "{\\\\^e}")
    ("�" "{\\\\\"\\\\i}")
    ("�" "{\\\\`\\\\i}")
    ("�" "{\\\\'\\\\i}")
    ("�" "{\\\\^\\\\i}")
    ("�" "\"o")
    ("�" "{\\\\`o}")
    ("�" "{\\\\'o}")
    ("�" "{\\\\~o}")
    ("�" "{\\\\^o}")
    ("�" "\"u")
    ("�" "{\\\\`u}")
    ("�" "{\\\\'u}")
    ("�" "{\\\\^u}")
    ("�" "\"A")
    ("�" "{\\\\`A}")
    ("�" "{\\\\'A}")
    ("�" "{\\\\~A}")
    ("�" "{\\\\^A}")
    ("�" "{\\\\\"E}")
    ("�" "{\\\\`E}")
    ("�" "{\\\\'E}")
    ("�" "{\\\\^E}")
    ("�" "{\\\\\"I}")
    ("�" "{\\\\`I}")
    ("�" "{\\\\'I}")
    ("�" "{\\\\^I}")
    ("�" "\"O")
    ("�" "{\\\\`O}")
    ("�" "{\\\\'O}")
    ("�" "{\\\\~O}")
    ("�" "{\\\\^O}")
    ("�" "\"U")
    ("�" "{\\\\`U}")
    ("�" "{\\\\'U}")
    ("�" "{\\\\^U}")
    ("�" "{\\\\~n}")
    ("�" "{\\\\~N}")
    ("�" "{\\\\c c}")
    ("�" "{\\\\c C}")
    ("�" "\"s")
    ("�" "{?`}")
    ("�" "{!`}")
    )
  "Translation table for translating ISO 8859-1 characters to German TeX.")

(defun iso-gtex2iso ()
 "Translate German TeX sequences to ISO 8859-1 characters."
 (interactive)
 (iso-translate-conventions iso-gtex2iso-trans-tab))


(defun iso-iso2gtex ()
 "Translate ISO 8859-1 characters to German TeX sequences."
 (interactive)
 (iso-translate-conventions iso-iso2gtex-trans-tab))


(defun iso-german-tex-p ()
 "Check if tex buffer is German LaTeX."
 (save-excursion
   (widen)
   (goto-char (point-min))
   (re-search-forward "\\\\documentstyle\\[.*german.*\\]" nil t)))

(defun iso-fix-iso2tex ()
  "Turn ISO 8859-1 (aka. ISO Latin-1) buffer into TeX sequences.
If German TeX is used, German TeX sequences are generated."
  (if (or (equal major-mode 'latex-mode)
	  (equal major-mode 'LaTeX-mode)) ; AucTeX wants this
      (if (iso-german-tex-p)
	  (iso-iso2gtex)
	(iso-iso2tex)))
  (if (or (equal major-mode 'tex-mode)
	  (equal major-mode 'TeX-mode)) ; AucTeX wants this
      (iso-iso2tex)))

(defun iso-fix-tex2iso ()
  "Turn TeX sequences into ISO 8859-1 (aka. ISO Latin-1) characters.
This function recognices German TeX buffers."
  (if (or (equal major-mode 'latex-mode)
	  (equal major-mode 'Latex-mode)) ; AucTeX wants this
      (if (iso-german-tex-p)
	  (iso-gtex2iso)
	(iso-tex2iso)))
  (if (or (equal major-mode 'tex-mode)
	  (equal major-mode 'TeX-mode))  ;; AucTeX wants this
      (iso-tex2iso)))

(defun iso-cvt-ffh ()
  "find-file-hook for iso-cvt-cvt.el."
  (iso-fix-tex2iso)
  (set-buffer-modified-p nil))

(defun iso-cvt-wfh ()
  "write file hook for iso-cvt-cvt.el."
  (iso-fix-iso2tex))

(defun iso-cvt-ash ()
  "after save hook for iso-cvt-cvt.el."
  (iso-fix-tex2iso)
  (set-buffer-modified-p nil))

(add-hook 'find-file-hooks 'iso-cvt-ffh)
(add-hook 'write-file-hooks 'iso-cvt-wfh)
(add-hook 'after-save-hook 'iso-cvt-ash)

;;; iso-cvt.el ends here
