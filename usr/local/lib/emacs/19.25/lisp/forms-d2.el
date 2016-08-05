;; demo2 -- demo forms-mode	-*- emacs-lisp -*-

;; SCCS Status     : @(#)@ demo2	1.1.2
;; Author          : Johan Vromans
;; Created On      : 1989
;; Last Modified By: Johan Vromans
;; Last Modified On: Mon Jul  1 13:56:31 1991
;; Update Count    : 2
;; Status          : OK
;; 
;; This sample forms exploit most of the features of forms mode.

;; Set the name of the data file.
(setq forms-file "forms-d2.dat")

;; Use 'forms-enumerate' to set field names and number thereof.
(setq forms-number-of-fields
      (forms-enumerate
       '(arch-newsgroup			; 1
	 arch-volume			; 2
	 arch-issue			; and ...
	 arch-article			; ... so
	 arch-shortname			; ... ... on
	 arch-parts
	 arch-from
	 arch-longname
	 arch-keywords
	 arch-date
	 arch-remarks)))

;; The following functions are used by this form for layout purposes.
;;
(defun arch-tocol (target &optional fill)
  "Produces a string to skip to column TARGET. Prepends newline if needed.
The optional FILL should be a character, used to fill to the column."
  (if (null fill)
      (setq fill ? ))
  (if (< target (current-column))
      (concat "\n" (make-string target fill))
    (make-string (- target (current-column)) fill)))
;;
(defun arch-rj (target field &optional fill) 
  "Produces a string to skip to column TARGET minus the width of field FIELD.
Prepends newline if needed. The optional FILL should be a character,
used to fill to the column."
  (arch-tocol (- target (length (nth field forms-fields))) fill))

;; Record filters.
;; This example uses the (defun ...) method of defining.
;;
(defun forms-new-record-filter (the-record)
  "Form a new record with some defaults."
  (aset the-record arch-from (user-full-name))
  (aset the-record arch-date (current-time-string))
  the-record				; return it
)

;; The format list.
(setq forms-format-list
     (list
       "====== Public Domain Software Archive ======\n\n"
       arch-shortname
       " - "			arch-longname
       "\n\n"
       "Article: "		arch-newsgroup
       "/"			arch-article
       " "
       '(arch-tocol 40)
       "Issue: "		arch-issue
       " "
       '(arch-rj 73 10)
       "Date: "			arch-date
       "\n\n"
       "Submitted by: "		arch-from
       "\n"
       '(arch-tocol 79 ?-)
       "\n"
       "Keywords: "		arch-keywords
       "\n\n"
       "Parts: "		arch-parts
       "\n\n====== Remarks ======\n\n"
       arch-remarks
     ))

;; That's all, folks!
