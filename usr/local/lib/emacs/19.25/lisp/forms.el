;;; forms.el -- Forms mode: edit a file as a form to fill in.
;;; Copyright (C) 1991, 1993 Free Software Foundation, Inc.

;; Author: Johan Vromans <jv@nl.net>
;; Version: $Revision: 2.6 $

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

;;; Visit a file using a form.
;;;
;;; === Naming conventions
;;;
;;; The names of all variables and functions start with 'forms-'.
;;; Names which start with 'forms--' are intended for internal use, and
;;; should *NOT* be used from the outside.
;;;
;;; All variables are buffer-local, to enable multiple forms visits 
;;; simultaneously.
;;; Variable `forms--mode-setup' is local to *ALL* buffers, for it 
;;; controls if forms-mode has been enabled in a buffer.
;;;
;;; === How it works ===
;;;
;;; Forms mode means visiting a data file which is supposed to consist
;;; of records each containing a number of fields.  The records are
;;; separated by a newline, the fields are separated by a user-defined
;;; field separater (default: TAB).
;;; When shown, a record is transferred to an Emacs buffer and
;;; presented using a user-defined form.  One record is shown at a
;;; time.
;;;
;;; Forms mode is a composite mode.  It involves two files, and two
;;; buffers.
;;; The first file, called the control file, defines the name of the
;;; data file and the forms format.  This file buffer will be used to
;;; present the forms.
;;; The second file holds the actual data.  The buffer of this file
;;; will be buried, for it is never accessed directly.
;;;
;;; Forms mode is invoked using M-x forms-find-file control-file .
;;; Alternativily `forms-find-file-other-window' can be used.
;;;
;;; You may also visit the control file, and switch to forms mode by hand
;;; with M-x forms-mode .
;;;
;;; Automatic mode switching is supported if you specify 
;;; "-*- forms -*-" in the first line of the control file.
;;; 
;;; The control file is visited, evaluated using `eval-current-buffer',
;;; and should set at least the following variables:
;;;
;;;	forms-file				[string]
;;;			The name of the data file.
;;;
;;;	forms-number-of-fields			[integer]
;;;			The number of fields in each record.
;;;
;;;	forms-format-list			[list]
;;;			Formatting instructions.
;;;
;;; `forms-format-list' should be a list, each element containing
;;;
;;;   - a string, e.g. "hello".  The string is inserted in the forms
;;;	"as is".
;;;   
;;;   - an integer, denoting a field number.
;;;	The contents of this field are inserted at this point.
;;;     Fields are numbered starting with number one.
;;;   
;;;   - a function call, e.g. (insert "text").
;;;	This function call is dynamically evaluated and should return a
;;;     string.  It should *NOT* have side-effects on the forms being
;;;     constructed.  The current fields are available to the function
;;;     in the variable `forms-fields', they should *NOT* be modified.
;;;   
;;;   - a lisp symbol, that must evaluate to one of the above.
;;;
;;; Optional variables which may be set in the control file:
;;;
;;;	forms-field-sep				[string, default TAB]
;;;			The field separator used to separate the
;;;			fields in the data file.  It may be a string.
;;;
;;;	forms-read-only				[bool, default nil]
;;;			Non-nil means that the data file is visited
;;;			read-only (view mode) as opposed to edit mode.
;;;			If no write access to the data file is
;;;			possible, view mode is enforced. 
;;;
;;;	forms-multi-line			[string, default "^K"]
;;;			If non-null the records of the data file may
;;;			contain fields that can span multiple lines in
;;;			the form.
;;;			This variable denotes the separator character
;;;			to be used for this purpose.  Upon display, all
;;;			occurrencies of this character are translated
;;;			to newlines.  Upon storage they are translated
;;;			back to the separator character.
;;;
;;;	forms-forms-scroll			[bool, default nil]
;;;			Non-nil means: rebind locally the commands that
;;;			perform `scroll-up' or `scroll-down' to use
;;;			`forms-next-field' resp. `forms-prev-field'.
;;;
;;;	forms-forms-jump			[bool, default nil]
;;;			Non-nil means: rebind locally the commands that
;;;			perform `beginning-of-buffer' or `end-of-buffer'
;;;			to perform `forms-first-field' resp. `forms-last-field'.
;;;
;;;	forms-new-record-filter			[symbol, default nil]
;;;			If not nil: this should be the name of a 
;;;			function that is called when a new
;;;			record is created.  It can be used to fill in
;;;			the new record with default fields, for example.
;;;
;;;	forms-modified-record-filter		[symbol, default nil]
;;;			If not nil: this should be the name of a 
;;;			function that is called when a record has
;;;			been modified.  It is called after the fields
;;;			are parsed.  It can be used to register
;;;			modification dates, for example.
;;;
;;;	forms-use-text-properties		[bool, see text for default]
;;;			This variable controls if forms mode should use
;;;			text properties to protect the form text from being
;;;			modified (using text-property `read-only').
;;;			Also, the read-write fields are shown using a
;;;			distinct face, if possible.
;;;			This variable defaults to t if running Emacs 19
;;;			with text properties.
;;;			The default face to show read-write fields is
;;;			copied from face `region'.
;;;
;;;	forms-ro-face 				[symbol, default 'default]
;;;			This is the face that is used to show
;;;			read-only text on the screen.If used, this
;;;			variable should be set to a symbol that is a
;;;			valid face.
;;;			E.g.
;;;			  (make-face 'my-face)
;;;			  (setq forms-ro-face 'my-face)
;;;
;;;	forms-rw-face				[symbol, default 'region]
;;;			This is the face that is used to show
;;;			read-write text on the screen.
;;;
;;; After evaluating the control file, its buffer is cleared and used
;;; for further processing.
;;; The data file (as designated by `forms-file') is visited in a buffer
;;; `forms--file-buffer' which will not normally be shown.
;;; Great malfunctioning may be expected if this file/buffer is modified
;;; outside of this package while it is being visited!
;;;
;;; Normal operation is to transfer one line (record) from the data file,
;;; split it into fields (into `forms--the-record-list'), and display it
;;; using the specs in `forms-format-list'.
;;; A format routine `forms--format' is built upon startup to format 
;;; the records according to `forms-format-list'.
;;;
;;; When a form is changed the record is updated as soon as this form
;;; is left.  The contents of the form are parsed using information
;;; obtained from `forms-format-list', and the fields which are
;;; deduced from the form are modified.  Fields not shown on the forms
;;; retain their origional values.  The newly formed record then
;;; replaces the contents of the old record in `forms--file-buffer'.
;;; A parse routine `forms--parser' is built upon startup to parse
;;; the records.
;;;
;;; Two exit functions exist: `forms-exit' and `forms-exit-no-save'.
;;; `forms-exit' saves the data to the file, if modified.
;;; `forms-exit-no-save` does not.  However, if `forms-exit-no-save'
;;; is executed and the file buffer has been modified, Emacs will ask
;;; questions anyway.
;;;
;;; Other functions provided by forms mode are:
;;;
;;;	paging (forward, backward) by record
;;;	jumping (first, last, random number)
;;;	searching
;;;	creating and deleting records
;;;	reverting the form (NOT the file buffer)
;;;	switching edit <-> view mode v.v.
;;;	jumping from field to field
;;;
;;; As an documented side-effect: jumping to the last record in the
;;; file (using forms-last-record) will adjust forms--total-records if
;;; needed.
;;;
;;; The forms buffer can be in on eof two modes: edit mode or view
;;; mode.  View mode is a read-only mode, you cannot modify the
;;; contents of the buffer.
;;;
;;; Edit mode commands:
;;; 
;;; TAB		 forms-next-field
;;; \C-c TAB	 forms-next-field
;;; \C-c <	 forms-first-record
;;; \C-c >	 forms-last-record
;;; \C-c ?	 describe-mode
;;; \C-c \C-k	 forms-delete-record
;;; \C-c \C-q	 forms-toggle-read-only
;;; \C-c \C-o	 forms-insert-record
;;; \C-c \C-l	 forms-jump-record
;;; \C-c \C-n	 forms-next-record
;;; \C-c \C-p	 forms-prev-record
;;; \C-c \C-s	 forms-search
;;; \C-c \C-x	 forms-exit
;;; 
;;; Read-only mode commands:
;;; 
;;; SPC 	 forms-next-record
;;; DEL	 forms-prev-record
;;; ?	 describe-mode
;;; \C-q forms-toggle-read-only
;;; l	 forms-jump-record
;;; n	 forms-next-record
;;; p	 forms-prev-record
;;; s	 forms-search
;;; x	 forms-exit
;;; 
;;; Of course, it is also possible to use the \C-c prefix to obtain the
;;; same command keys as in edit mode.
;;; 
;;; The following bindings are available, independent of the mode: 
;;; 
;;; [next]	  forms-next-record
;;; [prior]	  forms-prev-record
;;; [begin]	  forms-first-record
;;; [end]	  forms-last-record
;;; [S-TAB]	  forms-prev-field
;;; [backtab] forms-prev-field
;;;
;;; For convenience, TAB is always bound to `forms-next-field', so you
;;; don't need the C-c prefix for this command.
;;;
;;; As mentioned above (see `forms-forms-scroll' and `forms-forms-jump')
;;; the bindings of standard functions `scroll-up', `scroll-down',
;;; `beginning-of-buffer' and `end-of-buffer' can be locally replaced with
;;; forms mode functions next/prev record and first/last
;;; record.
;;;
;;; `local-write-file hook' is defined to save the actual data file
;;; instead of the buffer data, `revert-file-hook' is defined to
;;; revert a forms to original.

;;; Code:

;;; Global variables and constants:

(provide 'forms)			;;; official
(provide 'forms-mode)			;;; for compatibility

(defconst forms-version (substring "$Revision: 2.6 $" 11 -2)
  "The version number of forms-mode (as string).  The complete RCS id is:

  $Id: forms.el,v 2.6 1994/05/22 22:07:37 rms Exp $")

(defvar forms-mode-hooks nil
  "Hook functions to be run upon entering Forms mode.")

;;; Mandatory variables - must be set by evaluating the control file.

(defvar forms-file nil
  "Name of the file holding the data.")

(defvar forms-format-list nil
  "List of formatting specifications.")

(defvar forms-number-of-fields nil
  "Number of fields per record.")

;;; Optional variables with default values.

(defvar forms-field-sep "\t"
  "Field separator character (default TAB).")

(defvar forms-read-only nil
  "Non-nil means: visit the file in view (read-only) mode.
\(Defaults to the write access on the data file).")

(defvar forms-multi-line "\C-k"
  "If not nil: use this character to separate multi-line fields (default C-k).")

(defvar forms-forms-scroll nil
  "*Non-nil means replace scroll-up/down commands in Forms mode.
The replacement commands performs forms-next/prev-record.")

(defvar forms-forms-jump nil
  "*Non-nil means redefine beginning/end-of-buffer in Forms mode.
The replacement commands performs forms-first/last-record.")

(defvar forms-new-record-filter nil
  "The name of a function that is called when a new record is created.")

(defvar forms-modified-record-filter nil
  "The name of a function that is called when a record has been modified.")

(defvar forms-fields nil
  "List with fields of the current forms.  First field has number 1.
This variable is for use by the filter routines only. 
The contents may NOT be modified.")

(defvar forms-use-text-properties (fboundp 'set-text-properties)
  "*Non-nil means: use emacs-19 text properties.
Defaults to t if this emacs is capable of handling text properties.")

(defvar forms-ro-face 'default
  "The face (a symbol) that is used to display read-only text on the screen.")

(defvar forms-rw-face 'region
  "The face (a symbol) that is used to display read-write text on the screen.")

;;; Internal variables.

(defvar forms--file-buffer nil
  "Buffer which holds the file data")

(defvar forms--total-records 0
  "Total number of records in the data file.")

(defvar forms--current-record 0
  "Number of the record currently on the screen.")

(defvar forms-mode-map nil
   "Keymap for form buffer.")
(defvar forms-mode-ro-map nil
   "Keymap for form buffer in view mode.")
(defvar forms-mode-edit-map nil
   "Keymap for form buffer in edit mode.")

(defvar forms--markers nil
  "Field markers in the screen.")

(defvar forms--dyntexts nil
  "Dynamic texts (resulting from function calls) on the screen.")

(defvar forms--the-record-list nil 
   "List of strings of the current record, as parsed from the file.")

(defvar forms--search-regexp nil
  "Last regexp used by forms-search.")

(defvar forms--format nil
  "Formatting routine.")

(defvar forms--parser nil
  "Forms parser routine.")

(defvar forms--mode-setup nil
  "To keep track of forms-mode being set-up.")
(make-variable-buffer-local 'forms--mode-setup)

(defvar forms--dynamic-text nil
  "Array that holds dynamic texts to insert between fields.")

(defvar forms--elements nil
  "Array with the order in which the fields are displayed.")

(defvar forms--ro-face nil
  "Face used to represent read-only data on the screen.")

(defvar forms--rw-face nil
  "Face used to represent read-write data on the screen.")

;;;###autoload 
(defun forms-mode (&optional primary)
  "Major mode to visit files in a field-structured manner using a form.

Commands:                        Equivalent keys in read-only mode:
 TAB            forms-next-field          TAB
 \\C-c TAB       forms-next-field          
 \\C-c <         forms-first-record         <
 \\C-c >         forms-last-record          >
 \\C-c ?         describe-mode              ?
 \\C-c \\C-k      forms-delete-record
 \\C-c \\C-q      forms-toggle-read-only     q
 \\C-c \\C-o      forms-insert-record
 \\C-c \\C-l      forms-jump-record          l
 \\C-c \\C-n      forms-next-record          n
 \\C-c \\C-p      forms-prev-record          p
 \\C-c \\C-s      forms-search               s
 \\C-c \\C-x      forms-exit                 x
"
  (interactive)

  ;; This is not a simple major mode, as usual.  Therefore, forms-mode
  ;; takes an optional argument `primary' which is used for the
  ;; initial set-up.  Normal use would leave `primary' to nil.
  ;; A global buffer-local variable `forms--mode-setup' has the same
  ;; effect but makes it possible to auto-invoke forms-mode using
  ;; `find-file'.
  ;; Note: although it seems logical to have `make-local-variable'
  ;; executed where the variable is first needed, I have deliberately
  ;; placed all calls in this function.

  ;; Primary set-up: evaluate buffer and check if the mandatory
  ;; variables have been set.
  (if (or primary (not forms--mode-setup))
      (progn
	;;(message "forms: setting up...")
	(kill-all-local-variables)

	;; Make mandatory variables.
	(make-local-variable 'forms-file)
	(make-local-variable 'forms-number-of-fields)
	(make-local-variable 'forms-format-list)

	;; Make optional variables.
	(make-local-variable 'forms-field-sep)
        (make-local-variable 'forms-read-only)
        (make-local-variable 'forms-multi-line)
	(make-local-variable 'forms-forms-scroll)
	(make-local-variable 'forms-forms-jump)
	(make-local-variable 'forms-use-text-properties)
	(make-local-variable 'forms-new-record-filter)
	(make-local-variable 'forms-modified-record-filter)

	;; Make sure no filters exist.
	(setq forms-new-record-filter nil)
	(setq forms-modified-record-filter nil)

	;; If running Emacs 19 under X, setup faces to show read-only and 
	;; read-write fields.
	(if (fboundp 'make-face)
	    (progn
	      (make-local-variable 'forms-ro-face)
	      (make-local-variable 'forms-rw-face)))

	;; eval the buffer, should set variables
	;;(message "forms: processing control file...")
	(eval-current-buffer)

	;; check if the mandatory variables make sense.
	(or forms-file
	    (error (concat "Forms control file error: " 
			   "'forms-file' has not been set")))
	(or forms-number-of-fields
	    (error (concat "Forms control file error: "
			   "'forms-number-of-fields' has not been set")))
	(or (and (numberp forms-number-of-fields)
		 (> forms-number-of-fields 0))
	    (error (concat "Forms control file error: "
			   "'forms-number-of-fields' must be a number > 0")))
	(or (stringp forms-field-sep)
	    (error (concat "Forms control file error: "
			   "'forms-field-sep' is not a string")))
	(if forms-multi-line
	    (if (and (stringp forms-multi-line)
		     (eq (length forms-multi-line) 1))
		(if (string= forms-multi-line forms-field-sep)
		    (error (concat "Forms control file error: " 
				   "'forms-multi-line' is equal to 'forms-field-sep'")))
	      (error (concat "Forms control file error: "
			     "'forms-multi-line' must be nil or a one-character string"))))
	(or (fboundp 'set-text-properties)
	    (setq forms-use-text-properties nil))
	    
	;; Validate and process forms-format-list.
	;;(message "forms: pre-processing format list...")
	(forms--process-format-list)

	;; Build the formatter and parser.
	;;(message "forms: building formatter...")
	(make-local-variable 'forms--format)
	(make-local-variable 'forms--markers)
	(make-local-variable 'forms--dyntexts)
	(make-local-variable 'forms--elements)
	;;(message "forms: building parser...")
	(forms--make-format)
	(make-local-variable 'forms--parser)
	(forms--make-parser)
	;;(message "forms: building parser... done.")

	;; Check if record filters are defined.
	(if (and forms-new-record-filter
		 (not (fboundp forms-new-record-filter)))
	    (error (concat "Forms control file error: "
			   "'forms-new-record-filter' is not a function")))

	(if (and forms-modified-record-filter
		 (not (fboundp forms-modified-record-filter)))
	    (error (concat "Forms control file error: "
			   "'forms-modified-record-filter' is not a function")))

	;; The filters acces the contents of the forms using `forms-fields'.
	(make-local-variable 'forms-fields)

	;; Dynamic text support.
	(make-local-variable 'forms--dynamic-text)

	;; Prevent accidental overwrite of the control file and autosave.
	(set-visited-file-name nil)

	;; Prepare this buffer for further processing.
	(setq buffer-read-only nil)
	(erase-buffer)

	;;(message "forms: setting up... done.")
	))

  ;; Copy desired faces to the actual variables used by the forms formatter.
  (if (fboundp 'make-face)
      (progn
	(make-local-variable 'forms--ro-face)
	(make-local-variable 'forms--rw-face)
	(if forms-read-only
	    (progn
	      (setq forms--ro-face forms-ro-face)
	      (setq forms--rw-face forms-ro-face))
	  (setq forms--ro-face forms-ro-face)
	  (setq forms--rw-face forms-rw-face))))

  ;; Make more local variables.
  (make-local-variable 'forms--file-buffer)
  (make-local-variable 'forms--total-records)
  (make-local-variable 'forms--current-record)
  (make-local-variable 'forms--the-record-list)
  (make-local-variable 'forms--search-regexp)

  ; The keymaps are global, so multiple forms mode buffers can share them.
  ;(make-local-variable 'forms-mode-map)
  ;(make-local-variable 'forms-mode-ro-map)
  ;(make-local-variable 'forms-mode-edit-map)
  (if forms-mode-map			; already defined
      nil
    ;;(message "forms: building keymap...")
    (forms--mode-commands)
    ;;(message "forms: building keymap... done.")
    )

  ;; set the major mode indicator
  (setq major-mode 'forms-mode)
  (setq mode-name "Forms")

  ;; find the data file
  (setq forms--file-buffer (find-file-noselect forms-file))

  ;; count the number of records, and set see if it may be modified
  (let (ro)
    (setq forms--total-records
	  (save-excursion
	    (prog1
		(progn
		  ;;(message "forms: counting records...")
		  (set-buffer forms--file-buffer)
		  (bury-buffer (current-buffer))
		  (setq ro buffer-read-only)
		  (count-lines (point-min) (point-max)))
	      ;;(message "forms: counting records... done.")
	      )))
    (if ro
	(setq forms-read-only t)))

  ;;(message "forms: proceeding setup...")

  ;; Since we aren't really implementing a minor mode, we hack the modeline
  ;; directly to get the text " View " into forms-read-only form buffers.  For
  ;; that reason, this variable must be buffer only.
  (make-local-variable 'minor-mode-alist)
  (setq minor-mode-alist (list (list 'forms-read-only " View")))

  ;;(message "forms: proceeding setup (keymaps)...")
  (forms--set-keymaps)
  ;;(message "forms: proceeding setup (commands)...")
  (forms--change-commands)

  ;;(message "forms: proceeding setup (buffer)...")
  (set-buffer-modified-p nil)

  ;; setup the first (or current) record to show
  (if (< forms--current-record 1)
      (setq forms--current-record 1))
  (forms-jump-record forms--current-record)

  ;; user customising
  ;;(message "forms: proceeding setup (user hooks)...")
  (run-hooks 'forms-mode-hooks)
  ;;(message "forms: setting up... done.")

  ;; be helpful
  (forms--help)

  ;; initialization done
  (setq forms--mode-setup t))

(defun forms--process-format-list ()
  ;; Validate `forms-format-list' and set some global variables.
  ;; Symbols in the list are evaluated, and consecutive strings are
  ;; concatenated.
  ;; Array `forms--elements' is constructed that contains the order
  ;; of the fields on the display. This array is used by 
  ;; `forms--parser-using-text-properties' to extract the fields data
  ;; from the form on the screen.
  ;; Upon completion, `forms-format-list' is garanteed correct, so
  ;; `forms--make-format' and `forms--make-parser' do not need to perform
  ;; any checks.

  ;; Verify that `forms-format-list' is not nil.
  (or forms-format-list
      (error (concat "Forms control file error: "
		     "'forms-format-list' has not been set")))
  ;; It must be a list.
  (or (listp forms-format-list)
      (error (concat "Forms control file error: "
		     "'forms-format-list' is not a list")))

  ;; Assume every field is painted once.
  ;; `forms--elements' will grow if needed.
  (setq forms--elements (make-vector forms-number-of-fields nil))

  (let ((the-list forms-format-list)	; the list of format elements
	(this-item 0)			; element in list
	(prev-item nil)
	(field-num 0))			; highest field number 

    (setq forms-format-list nil)	; gonna rebuild

    (while the-list

      (let ((el (car-safe the-list))
	    (rem (cdr-safe the-list)))

	;; If it is a symbol, eval it first.
	(if (and (symbolp el)
		 (boundp el))
	    (setq el (eval el)))

	(cond

	 ;; Try string ...
	 ((stringp el)
	  (if (stringp prev-item)	; try to concatenate strings
	      (setq prev-item (concat prev-item el))
	    (if prev-item
		(setq forms-format-list
		      (append forms-format-list (list prev-item) nil)))
	    (setq prev-item el)))

	 ;; Try numeric ...
	 ((numberp el) 

	  ;; Validate range.
	  (if (or (<= el 0)
		  (> el forms-number-of-fields))
	      (error (concat "Forms format error: "
			     "field number %d out of range 1..%d")
		     el forms-number-of-fields))

	  ;; Store forms order.
	  (if (> field-num (length forms--elements))
	      (setq forms--elements (vconcat forms--elements (1- el)))
	    (aset forms--elements field-num (1- el)))
	  (setq field-num (1+ field-num))

	  (if prev-item
	      (setq forms-format-list
		    (append forms-format-list (list prev-item) nil)))
	  (setq prev-item el))

	 ;; Try function ...
	 ((listp el)

	  ;; Validate.
	  (or (fboundp (car-safe el))
	      (error (concat "Forms format error: "
			     "not a function "
			     (prin1-to-string (car-safe el)))))

	  ;; Shift.
	  (if prev-item
	      (setq forms-format-list
		    (append forms-format-list (list prev-item) nil)))
	  (setq prev-item el))

	 ;; else
	 (t
	  (error (concat "Forms format error: "
			 "invalid element "
			 (prin1-to-string el)))))

	;; Advance to next element of the list.
	(setq the-list rem)))

    ;; Append last item.
    (if prev-item
	(progn
	  (setq forms-format-list
		(append forms-format-list (list prev-item) nil))
	  ;; Append a newline if the last item is a field.
	  ;; This prevents parsing problems.
	  ;; Also it makes it possible to insert an empty last field.
	  (if (numberp prev-item)
	      (setq forms-format-list
		    (append forms-format-list (list "\n") nil))))))

  (forms--debug 'forms-format-list
		'forms--elements))

;; Special treatment for read-only segments.
;;
;; If text is inserted between two read-only segments, it inherits the
;; read-only properties.  This is not what we want.
;; To solve this, read-only segments get the `insert-in-front-hooks'
;; property set with a function that temporarily switches the properties
;; of the first character of the segment to read-write, so the new
;; text gets the right properties.
;; The `post-command-hook' is used to restore the original properties.

(defvar forms--iif-start nil
  "Record start of modification command.")
(defvar forms--iif-properties nil
  "Original properties of the character being overridden.")

(defun forms--iif-hook (begin end)
  "`insert-in-front-hooks' function for read-only segments."

  ;; Note start location.  By making it a marker that points one 
  ;; character beyond the actual location, it is guaranteed to move 
  ;; correctly if text is inserted.
  (or forms--iif-start
      (setq forms--iif-start (copy-marker (1+ (point)))))

  ;; Check if there is special treatment required.
  (if (or (<= forms--iif-start 2)
	  (get-text-property (- forms--iif-start 2)
			     'read-only))
      (progn
	;; Fetch current properties.
	(setq forms--iif-properties 
	      (text-properties-at (1- forms--iif-start)))

	;; Replace them.
	(let ((inhibit-read-only t))
	  (set-text-properties 
	   (1- forms--iif-start) forms--iif-start
	   (list 'face forms--rw-face 'front-sticky '(face))))

	;; Enable `post-command-hook' to restore the properties.
	(setq post-command-hook
	      (append (list 'forms--iif-post-command-hook) post-command-hook)))

    ;; No action needed.  Clear marker.
    (setq forms--iif-start nil)))

(defun forms--iif-post-command-hook ()
  "`post-command-hook' function for read-only segments."

  ;; Disable `post-command-hook'.
  (setq post-command-hook
	(delq 'forms--iif-hook-post-command-hook post-command-hook))

  ;; Restore properties.
  (if forms--iif-start
      (let ((inhibit-read-only t))
	(set-text-properties 
	 (1- forms--iif-start) forms--iif-start
	 forms--iif-properties)))

  ;; Cleanup.
  (setq forms--iif-start nil))

(defvar forms--marker)
(defvar forms--dyntext)

(defun forms--make-format ()
  "Generate `forms--format' using the information in `forms-format-list'."

  ;; The real work is done using a mapcar of `forms--make-format-elt' on
  ;; `forms-format-list'.
  ;; This function sets up the necessary environment, and decides
  ;; which function to mapcar.

  (let ((forms--marker 0)
	(forms--dyntext 0))
    (setq 
     forms--format
     (if forms-use-text-properties 
	 (` (lambda (arg)
	      (let ((inhibit-read-only t))
		(,@ (apply 'append
			   (mapcar 'forms--make-format-elt-using-text-properties
				   forms-format-list)))
		;; Prevent insertion before the first text.
		(,@ (if (numberp (car forms-format-list))
			nil
		      '((add-text-properties (point-min) (1+ (point-min))
					     '(front-sticky (read-only))))))
		;; Prevent insertion after the last text.
		(remove-text-properties (1- (point)) (point)
					'(rear-nonsticky)))
	      (setq forms--iif-start nil)))
       (` (lambda (arg)
	    (,@ (apply 'append
		       (mapcar 'forms--make-format-elt forms-format-list)))))))

    ;; We have tallied the number of markers and dynamic texts,
    ;; so we can allocate the arrays now.
    (setq forms--markers (make-vector forms--marker nil))
    (setq forms--dyntexts (make-vector forms--dyntext nil)))
  (forms--debug 'forms--format))

(defun forms--make-format-elt-using-text-properties (el)
  "Helper routine to generate format function."

  ;; The format routine `forms--format' will look like
  ;;
  ;; ;; preamble
  ;; (lambda (arg)
  ;;   (let ((inhibit-read-only t))
  ;;
  ;;     ;; A string, e.g. "text: ".
  ;;     (set-text-properties 
  ;;      (point)
  ;;      (progn (insert "text: ") (point)) 
  ;;      (list 'face forms--ro-face
  ;;		'read-only 1
  ;;		'insert-in-front-hooks 'forms--iif-hook
  ;;		'rear-nonsticky '(read-only face insert-in-front-hooks)))
  ;;
  ;;     ;; A field, e.g. 6.
  ;;     (let ((here (point)))
  ;;       (aset forms--markers 0 (point-marker))
  ;;       (insert (elt arg 5))
  ;;       (or (= (point) here)
  ;; 	  (set-text-properties 
  ;; 	   here (point)
  ;; 	   (list 'face forms--rw-face
  ;;		 'front-sticky '(face))))
  ;;
  ;;     ;; Another string, e.g. "\nmore text: ".
  ;;     (set-text-properties
  ;;      (point)
  ;;      (progn (insert "\nmore text: ") (point))
  ;;      (list 'face forms--ro-face
  ;;		'read-only 2
  ;;		'insert-in-front-hooks 'forms--iif-hook
  ;;		'rear-nonsticky '(read-only face insert-in-front-hooks)))
  ;;
  ;;     ;; A function, e.g. (tocol 40).
  ;;     (set-text-properties
  ;;      (point)
  ;;      (progn
  ;;        (insert (aset forms--dyntexts 0 (tocol 40)))
  ;;        (point))
  ;;      (list 'face forms--ro-face
  ;;		'read-only 2
  ;;		'insert-in-front-hooks 'forms--iif-hook
  ;;		'rear-nonsticky '(read-only face insert-in-front-hooks)))
  ;;
  ;;	 ;; Prevent insertion before the first text.
  ;;	 (add-text-properties (point-min) (1+ (point-min))
  ;;			      '(front-sticky (read-only))))))
  ;;	 ;; Prevent insertion after the last text.
  ;;	 (remove-text-properties (1- (point)) (point)
  ;;	 			 '(rear-nonsticky)))
  ;;
  ;;     ;; wrap up
  ;;     (setq forms--iif-start nil)
  ;;     ))

  (cond
   ((stringp el)
    
    (` ((set-text-properties 
	 (point)			; start at point
	 (progn				; until after insertion
	   (insert (, el))
	   (point))
	 (list 'face forms--ro-face	; read-only appearance
	       'read-only (,@ (list (1+ forms--marker)))
	       'insert-in-front-hooks '(forms--iif-hook)
	       'rear-nonsticky '(face read-only insert-in-front-hooks))))))
    
   ((numberp el)
    (` ((let ((here (point)))
	  (aset forms--markers 
		(, (prog1 forms--marker
		     (setq forms--marker (1+ forms--marker))))
		(point-marker))
	  (insert (elt arg (, (1- el))))
	  (or (= (point) here)
	      (set-text-properties 
	       here (point)
	       (list 'face forms--rw-face
		     'front-sticky '(face))))))))

   ((listp el)
    (` ((set-text-properties
	 (point)
	 (progn
	   (insert (aset forms--dyntexts 
			 (, (prog1 forms--dyntext
			      (setq forms--dyntext (1+ forms--dyntext))))
			 (, el)))
	   (point))
	 (list 'face forms--ro-face
	       'read-only (,@ (list (1+ forms--marker)))
	       'insert-in-front-hooks '(forms--iif-hook)
	       'rear-nonsticky '(read-only face insert-in-front-hooks))))))

   ;; end of cond
   ))

(defun forms--make-format-elt (el)
  "Helper routine to generate format function."

  ;; If we're not using text properties, the format routine
  ;; `forms--format' will look like
  ;;
  ;; (lambda (arg)
  ;;   ;; a string, e.g. "text: "
  ;;   (insert "text: ")
  ;;   ;; a field, e.g. 6
  ;;   (aset forms--markers 0 (point-marker))
  ;;   (insert (elt arg 5))
  ;;   ;; another string, e.g. "\nmore text: "
  ;;   (insert "\nmore text: ")
  ;;   ;; a function, e.g. (tocol 40)
  ;;   (insert (aset forms--dyntexts 0 (tocol 40)))
  ;;   ... )

  (cond 
   ((stringp el)
    (` ((insert (, el)))))
   ((numberp el)
    (prog1
	(` ((aset forms--markers (, forms--marker) (point-marker))
	    (insert (elt arg (, (1- el))))))
      (setq forms--marker (1+ forms--marker))))
   ((listp el)
    (prog1
	(` ((insert (aset forms--dyntexts (, forms--dyntext) (, el)))))
      (setq forms--dyntext (1+ forms--dyntext))))))

(defvar forms--field)
(defvar forms--recordv)
(defvar forms--seen-text)

(defun forms--make-parser ()
  "Generate `forms--parser' from the information in `forms-format-list'."

  ;; If we can use text properties, we simply set it to
  ;; `forms--parser-using-text-properties'.
  ;; Otherwise, the function is constructed using a mapcar of
  ;; `forms--make-parser-elt on `forms-format-list'.

  (setq
   forms--parser
   (if forms-use-text-properties
       (function forms--parser-using-text-properties)
     (let ((forms--field nil)
	   (forms--seen-text nil)
	   (forms--dyntext 0))

       ;; Note: we add a nil element to the list passed to `mapcar',
       ;; see `forms--make-parser-elt' for details.
       (` (lambda nil
	    (let (here)
	      (goto-char (point-min))
	      (,@ (apply 'append
			 (mapcar 
			  'forms--make-parser-elt 
			  (append forms-format-list (list nil)))))))))))

  (forms--debug 'forms--parser))

(defun forms--parser-using-text-properties ()
  "Extract field info from forms when using text properties."

  ;; Using text properties, we can simply jump to the markers, and
  ;; extract the information up to the following read-only segment.

  (let ((i 0)
	here there)
    (while (< i (length forms--markers))
      (goto-char (setq here (aref forms--markers i)))
      (if (get-text-property here 'read-only)
	  (aset forms--recordv (aref forms--elements i) nil)
	(if (setq there 
		  (next-single-property-change here 'read-only))
	    (aset forms--recordv (aref forms--elements i)
		  (buffer-substring here there))
	  (aset forms--recordv (aref forms--elements i)
		(buffer-substring here (point-max)))))
      (setq i (1+ i)))))

(defun forms--make-parser-elt (el)
  "Helper routine to generate forms parser function."

  ;; The parse routine will look like:
  ;;
  ;; (lambda nil
  ;;   (let (here)
  ;;     (goto-char (point-min))
  ;; 
  ;;	 ;;  "text: "
  ;;     (if (not (looking-at "text: "))
  ;; 	    (error "Parse error: cannot find \"text: \""))
  ;;     (forward-char 6)	; past "text: "
  ;; 
  ;;     ;;  6
  ;;	 ;;  "\nmore text: "
  ;;     (setq here (point))
  ;;     (if (not (search-forward "\nmore text: " nil t nil))
  ;; 	    (error "Parse error: cannot find \"\\nmore text: \""))
  ;;     (aset forms--recordv 5 (buffer-substring here (- (point) 12)))
  ;;
  ;;	 ;;  (tocol 40)
  ;;	(let ((forms--dyntext (car-safe forms--dynamic-text)))
  ;;	  (if (not (looking-at (regexp-quote forms--dyntext)))
  ;;	      (error "Parse error: not looking at \"%s\"" forms--dyntext))
  ;;	  (forward-char (length forms--dyntext))
  ;;	  (setq forms--dynamic-text (cdr-safe forms--dynamic-text)))
  ;;     ... 
  ;;     ;; final flush (due to terminator sentinel, see below)
  ;;	(aset forms--recordv 7 (buffer-substring (point) (point-max)))

  (cond
   ((stringp el)
    (prog1
	(if forms--field
	    (` ((setq here (point))
		(if (not (search-forward (, el) nil t nil))
		    (error "Parse error: cannot find \"%s\"" (, el)))
		(aset forms--recordv (, (1- forms--field))
		      (buffer-substring here
					(- (point) (, (length el)))))))
	  (` ((if (not (looking-at (, (regexp-quote el))))
		  (error "Parse error: not looking at \"%s\"" (, el)))
	      (forward-char (, (length el))))))
      (setq forms--seen-text t)
      (setq forms--field nil)))
   ((numberp el)
    (if forms--field
	(error "Cannot parse adjacent fields %d and %d"
	       forms--field el)
      (setq forms--field el)
      nil))
   ((null el)
    (if forms--field
	(` ((aset forms--recordv (, (1- forms--field))
		  (buffer-substring (point) (point-max)))))))
   ((listp el)
    (prog1
	(if forms--field
	    (` ((let ((here (point))
		      (forms--dyntext (aref forms--dyntexts (, forms--dyntext))))
		  (if (not (search-forward forms--dyntext nil t nil))
		      (error "Parse error: cannot find \"%s\"" forms--dyntext))
		  (aset forms--recordv (, (1- forms--field))
			(buffer-substring here
					  (- (point) (length forms--dyntext)))))))
	  (` ((let ((forms--dyntext (aref forms--dyntexts (, forms--dyntext))))
		(if (not (looking-at (regexp-quote forms--dyntext)))
		    (error "Parse error: not looking at \"%s\"" forms--dyntext))
		(forward-char (length forms--dyntext))))))
      (setq forms--dyntext (1+ forms--dyntext))
      (setq forms--seen-text t)
      (setq forms--field nil)))
   ))

(defun forms--set-keymaps ()
  "Set the keymaps used in this mode."

  (use-local-map (if forms-read-only 
		     forms-mode-ro-map 
		   forms-mode-edit-map)))

(defun forms--mode-commands ()
  "Fill the Forms mode keymaps."

  ;; `forms-mode-map' is always accessible via \C-c prefix.
  (setq forms-mode-map (make-keymap))
  (define-key forms-mode-map "\t" 'forms-next-field)
  (define-key forms-mode-map "\C-k" 'forms-delete-record)
  (define-key forms-mode-map "\C-q" 'forms-toggle-read-only)
  (define-key forms-mode-map "\C-o" 'forms-insert-record)
  (define-key forms-mode-map "\C-l" 'forms-jump-record)
  (define-key forms-mode-map "\C-n" 'forms-next-record)
  (define-key forms-mode-map "\C-p" 'forms-prev-record)
  (define-key forms-mode-map "\C-s" 'forms-search)
  (define-key forms-mode-map "\C-x" 'forms-exit)
  (define-key forms-mode-map "<" 'forms-first-record)
  (define-key forms-mode-map ">" 'forms-last-record)
  (define-key forms-mode-map "?" 'describe-mode)
  (define-key forms-mode-map "\C-?" 'forms-prev-record)

  ;; `forms-mode-ro-map' replaces the local map when in read-only mode.
  (setq forms-mode-ro-map (make-keymap))
  (suppress-keymap forms-mode-ro-map)
  (define-key forms-mode-ro-map "\C-c" forms-mode-map)
  (define-key forms-mode-ro-map "\t" 'forms-next-field)
  (define-key forms-mode-ro-map "q" 'forms-toggle-read-only)
  (define-key forms-mode-ro-map "l" 'forms-jump-record)
  (define-key forms-mode-ro-map "n" 'forms-next-record)
  (define-key forms-mode-ro-map "p" 'forms-prev-record)
  (define-key forms-mode-ro-map "s" 'forms-search)
  (define-key forms-mode-ro-map "x" 'forms-exit)
  (define-key forms-mode-ro-map "<" 'forms-first-record)
  (define-key forms-mode-ro-map ">" 'forms-last-record)
  (define-key forms-mode-ro-map "?" 'describe-mode)
  (define-key forms-mode-ro-map " " 'forms-next-record)
  (forms--mode-commands1 forms-mode-ro-map)

  ;; This is the normal, local map.
  (setq forms-mode-edit-map (make-keymap))
  (define-key forms-mode-edit-map "\t"   'forms-next-field)
  (define-key forms-mode-edit-map "\C-c" forms-mode-map)
  (forms--mode-commands1 forms-mode-edit-map)
  )

(defun forms--mode-commands1 (map)
  "Helper routine to define keys."
  (define-key map [TAB] 'forms-next-field)
  (define-key map [S-tab] 'forms-prev-field)
  (define-key map [next] 'forms-next-record)
  (define-key map [prior] 'forms-prev-record)
  (define-key map [begin] 'forms-first-record)
  (define-key map [last] 'forms-last-record)
  (define-key map [backtab] 'forms-prev-field)
  )

;;; Changed functions

(defun forms--change-commands ()
  "Localize some commands for Forms mode."

  ;; scroll-down -> forms-prev-record
  ;; scroll-up -> forms-next-record
  (if forms-forms-scroll
      (progn
	(substitute-key-definition 'scroll-up 'forms-next-record
				   (current-local-map)
				   (current-global-map))
	(substitute-key-definition 'scroll-down 'forms-prev-record
				   (current-local-map)
				   (current-global-map))))
  ;;
  ;; beginning-of-buffer -> forms-first-record
  ;; end-of-buffer -> forms-end-record
  (if forms-forms-jump
      (progn
	(substitute-key-definition 'beginning-of-buffer 'forms-first-record
				   (current-local-map)
				   (current-global-map))
	(substitute-key-definition 'end-of-buffer 'forms-last-record
				   (current-local-map)
				   (current-global-map))))
  ;;
  ;; save-buffer -> forms--save-buffer
  (make-local-variable 'local-write-file-hooks)
  (add-hook 'local-write-file-hooks
	    (function
	     (lambda (nil)
	       (forms--checkmod)
	       (save-excursion
		 (set-buffer forms--file-buffer)
		 (save-buffer))
	       t)))
  ;; We have our own revert function - use it
  (make-local-variable 'revert-buffer-function)
  (setq revert-buffer-function 'forms-revert-buffer)

  t)

(defun forms--help ()
  "Initial help for Forms mode."
  ;; We should use
  (message (substitute-command-keys (concat
  "\\[forms-next-record]:next"
  "   \\[forms-prev-record]:prev"
  "   \\[forms-first-record]:first"
  "   \\[forms-last-record]:last"
  "   \\[describe-mode]:help"))))
  ; but it's too slow ....
;  (if forms-read-only
;      (message "SPC:next   DEL:prev   <:first   >:last   ?:help   q:exit")
;    (message "C-c n:next   C-c p:prev   C-c <:first   C-c >:last   C-c ?:help   C-c q:exit"))
;  )

(defun forms--trans (subj arg rep)
  "Translate in SUBJ all chars ARG into char REP.  ARG and REP should
 be single-char strings."
  (let ((i 0)
	(x (length subj))
	(re (regexp-quote arg))
	(k (string-to-char rep)))
    (while (setq i (string-match re subj i))
      (aset subj i k)
      (setq i (1+ i)))))

(defun forms--exit (query &optional save)
  "Internal exit from forms mode function."

  (let ((buf (buffer-name forms--file-buffer)))
    (forms--checkmod)
    (if (and save
	     (buffer-modified-p forms--file-buffer))
	(save-excursion
	  (set-buffer forms--file-buffer)
	  (save-buffer)))
    (save-excursion
      (set-buffer forms--file-buffer)
      (delete-auto-save-file-if-necessary)
      (kill-buffer (current-buffer)))
    (if (get-buffer buf)	; not killed???
      (if save
	  (progn
	    (beep)
	    (message "Problem saving buffers?")))
      (delete-auto-save-file-if-necessary)
      (kill-buffer (current-buffer)))))

(defun forms--get-record ()
  "Fetch the current record from the file buffer."

  ;; This function is executed in the context of the `forms--file-buffer'.

  (or (bolp)
      (beginning-of-line nil))
  (let ((here (point)))
    (prog2
     (end-of-line)
     (buffer-substring here (point))
     (goto-char here))))

(defun forms--show-record (the-record)
  "Format THE-RECORD and display it in the current buffer."

  ;; Split the-record.
  (let (the-result
	(start-pos 0)
	found-pos
	(field-sep-length (length forms-field-sep)))
    (if forms-multi-line
	(forms--trans the-record forms-multi-line "\n"))
    ;; Add an extra separator (makes splitting easy).
    (setq the-record (concat the-record forms-field-sep))
    (while (setq found-pos (string-match forms-field-sep the-record start-pos))
      (let ((ent (substring the-record start-pos found-pos)))
	(setq the-result
	      (append the-result (list ent)))
	(setq start-pos (+ field-sep-length found-pos))))
    (setq forms--the-record-list the-result))

  (setq buffer-read-only nil)
  (if forms-use-text-properties
      (let ((inhibit-read-only t))
	(set-text-properties (point-min) (point-max) nil)))
  (erase-buffer)

  ;; Verify the number of fields, extend forms--the-record-list if needed.
  (if (= (length forms--the-record-list) forms-number-of-fields)
      nil
    (beep)
    (message "Warning: this record has %d fields instead of %d"
	     (length forms--the-record-list) forms-number-of-fields)
    (if (< (length forms--the-record-list) forms-number-of-fields)
	(setq forms--the-record-list 
	      (append forms--the-record-list
		      (make-list 
		       (- forms-number-of-fields 
			  (length forms--the-record-list))
		       "")))))

  ;; Call the formatter function.
  (setq forms-fields (append (list nil) forms--the-record-list nil))
  (funcall forms--format forms--the-record-list)

  ;; Prepare.
  (goto-char (point-min))
  (set-buffer-modified-p nil)
  (setq buffer-read-only forms-read-only)
  (setq mode-line-process
	(concat " " forms--current-record "/" forms--total-records)))

(defun forms--parse-form ()
  "Parse contents of form into list of strings."
  ;; The contents of the form are parsed, and a new list of strings
  ;; is constructed.
  ;; A vector with the strings from the original record is 
  ;; constructed, which is updated with the new contents.  Therefore
  ;; fields which were not in the form are not modified.
  ;; Finally, the vector is transformed into a list for further processing.

  (let (forms--recordv)

    ;; Build the vector.
    (setq forms--recordv (vconcat forms--the-record-list))

    ;; Parse the form and update the vector.
    (let ((forms--dynamic-text forms--dynamic-text))
      (funcall forms--parser))

    (if forms-modified-record-filter
	;; As a service to the user, we add a zeroth element so she
	;; can use the same indices as in the forms definition.
	(let ((the-fields (vconcat [nil] forms--recordv)))
	  (setq the-fields (funcall forms-modified-record-filter the-fields))
	  (cdr (append the-fields nil)))

      ;; Transform to a list and return.
      (append forms--recordv nil))))

(defun forms--update ()
  "Update current record with contents of form.
As a side effect: sets `forms--the-record-list'."

  (if forms-read-only
      (progn
	(message "Read-only buffer!")
	(beep))

    (let (the-record)
      ;; Build new record.
      (setq forms--the-record-list (forms--parse-form))
      (setq the-record
	    (mapconcat 'identity forms--the-record-list forms-field-sep))

      ;; Handle multi-line fields, if allowed.
      (if forms-multi-line
	  (forms--trans the-record "\n" forms-multi-line))

      ;; A final sanity check before updating.
      (if (string-match "\n" the-record)
	  (progn
	    (message "Multi-line fields in this record - update refused!")
	    (beep))

	(save-excursion
	  (set-buffer forms--file-buffer)
	  ;; Use delete-region instead of kill-region, to avoid
	  ;; adding junk to the kill-ring.
	  (delete-region (save-excursion (beginning-of-line) (point))
			 (save-excursion (end-of-line) (point)))
	  (insert the-record)
	  (beginning-of-line))))))

(defun forms--checkmod ()
  "Check if this form has been modified, and call forms--update if so."
  (if (buffer-modified-p nil)
      (let ((here (point)))
	(forms--update)
	(set-buffer-modified-p nil)
	(goto-char here))))

;;; Start and exit

;;;###autoload
(defun forms-find-file (fn)
  "Visit a file in Forms mode."
  (interactive "fForms file: ")
  (find-file-read-only fn)
  (or forms--mode-setup (forms-mode t)))

;;;###autoload
(defun forms-find-file-other-window (fn)
  "Visit a file in Forms mode in other window."
  (interactive "fFbrowse file in other window: ")
  (find-file-other-window fn)
  (eval-current-buffer)
  (or forms--mode-setup (forms-mode t)))

(defun forms-exit (query)
  "Normal exit from Forms mode.  Modified buffers are saved."
  (interactive "P")
  (forms--exit query t))

(defun forms-exit-no-save (query)
  "Exit from Forms mode without saving buffers."
  (interactive "P")
  (forms--exit query nil))

;;; Navigating commands

(defun forms-next-record (arg)
  "Advance to the ARGth following record."
  (interactive "P")
  (forms-jump-record (+ forms--current-record (prefix-numeric-value arg)) t))

(defun forms-prev-record (arg)
  "Advance to the ARGth previous record."
  (interactive "P")
  (forms-jump-record (- forms--current-record (prefix-numeric-value arg)) t))

(defun forms-jump-record (arg &optional relative)
  "Jump to a random record."
  (interactive "NRecord number: ")

  ;; Verify that the record number is within range.
  (if (or (> arg forms--total-records)
	  (<= arg 0))
    (progn
      (beep)
      ;; Don't give the message if just paging.
      (if (not relative)
	  (message "Record number %d out of range 1..%d"
		   arg forms--total-records))
      )

    ;; Flush.
    (forms--checkmod)

    ;; Calculate displacement.
    (let ((disp (- arg forms--current-record))
	  (cur forms--current-record))

      ;; `forms--show-record' needs it now.
      (setq forms--current-record arg)

      ;; Get the record and show it.
      (forms--show-record
       (save-excursion
	 (set-buffer forms--file-buffer)
	 (beginning-of-line)

	 ;; Move, and adjust the amount if needed (shouldn't happen).
	 (if relative
	     (if (zerop disp)
		 nil
	       (setq cur (+ cur disp (- (forward-line disp)))))
	   (setq cur (+ cur disp (- (goto-line arg)))))

	 (forms--get-record)))

      ;; This shouldn't happen.
      (if (/= forms--current-record cur)
	  (progn
	    (setq forms--current-record cur)
	    (beep)
	    (message "Stuck at record %d" cur))))))

(defun forms-first-record ()
  "Jump to first record."
  (interactive)
  (forms-jump-record 1))

(defun forms-last-record ()
  "Jump to last record.
As a side effect: re-calculates the number of records in the data file."
  (interactive)
  (let
      ((numrec 
	(save-excursion
	  (set-buffer forms--file-buffer)
	  (count-lines (point-min) (point-max)))))
    (if (= numrec forms--total-records)
	nil
      (beep)
      (setq forms--total-records numrec)
      (message "Warning: number of records changed to %d" forms--total-records)))
  (forms-jump-record forms--total-records))

;;; Other commands

(defun forms-toggle-read-only (arg)
  "Toggles read-only mode of a forms mode buffer.
With an argument, enables read-only mode if the argument is positive.
Otherwise enables edit mode if the visited file is writeable."

  (interactive "P")

  (if (if arg
	  ;; Negative arg means switch it off.
	  (<= (prefix-numeric-value arg) 0)
	;; No arg means toggle.
	forms-read-only)

      ;; Enable edit mode, if possible.
      (let ((ro forms-read-only))
	(if (save-excursion
	      (set-buffer forms--file-buffer)
	      buffer-read-only)
	    (progn
	      (setq forms-read-only t)
	      (message "No write access to \"%s\"" forms-file)
	      (beep))
	  (setq forms-read-only nil))
	(if (equal ro forms-read-only)
	    nil
	  (forms-mode)))

    ;; Enable view mode.
    (if forms-read-only
	nil
      (forms--checkmod)			; sync
      (setq forms-read-only t)
      (forms-mode))))

;; Sample:
;; (defun my-new-record-filter (the-fields)
;;   ;; numbers are relative to 1
;;   (aset the-fields 4 (current-time-string))
;;   (aset the-fields 6 (user-login-name))
;;   the-list)
;; (setq forms-new-record-filter 'my-new-record-filter)

(defun forms-insert-record (arg)
  "Create a new record before the current one.
With ARG: store the record after the current one.
If `forms-new-record-filter' contains the name of a function, 
it is called to fill (some of) the fields with default values."

  (interactive "P")

  (if forms-read-only
      (error ""))

  (let ((ln (if arg (1+ forms--current-record) forms--current-record))
        the-list the-record)

    (forms--checkmod)
    (if forms-new-record-filter
	;; As a service to the user, we add a zeroth element so she
	;; can use the same indices as in the forms definition.
	(let ((the-fields (make-vector (1+ forms-number-of-fields) "")))
	  (setq the-fields (funcall forms-new-record-filter the-fields))
	  (setq the-list (cdr (append the-fields nil))))
      (setq the-list (make-list forms-number-of-fields "")))

    (setq the-record
	  (mapconcat
	  'identity
	  the-list
	  forms-field-sep))

    (save-excursion
      (set-buffer forms--file-buffer)
      (goto-line ln)
      (open-line 1)
      (insert the-record)
      (beginning-of-line))
    
    (setq forms--current-record ln))

  (setq forms--total-records (1+ forms--total-records))
  (forms-jump-record forms--current-record))

(defun forms-delete-record (arg)
  "Deletes a record.  With a prefix argument: don't ask."
  (interactive "P")

  (if forms-read-only
      (error ""))

  (forms--checkmod)
  (if (or arg
	  (y-or-n-p "Really delete this record? "))
      (let ((ln forms--current-record))
	(save-excursion
	  (set-buffer forms--file-buffer)
	  (goto-line ln)
	  ;; Use delete-region instead of kill-region, to avoid
	  ;; adding junk to the kill-ring.
	  (delete-region (save-excursion (beginning-of-line) (point))
			 (save-excursion (end-of-line) (1+ (point)))))
	(setq forms--total-records (1- forms--total-records))
	(if (> forms--current-record forms--total-records)
	    (setq forms--current-record forms--total-records))
	(forms-jump-record forms--current-record)))
  (message ""))

(defun forms-search (regexp)
  "Search REGEXP in file buffer."
  (interactive 
   (list (read-string (concat "Search for" 
				  (if forms--search-regexp
				   (concat " ("
					   forms--search-regexp
					   ")"))
				  ": "))))
  (if (equal "" regexp)
      (setq regexp forms--search-regexp))
  (forms--checkmod)

  (let (the-line the-record here
		 (fld-sep forms-field-sep))
    (if (save-excursion
	  (set-buffer forms--file-buffer)
	  (setq here (point))
	  (end-of-line)
	  (if (null (re-search-forward regexp nil t))
	      (progn
		(goto-char here)
		(message (concat "\"" regexp "\" not found."))
		nil)
	    (setq the-record (forms--get-record))
	    (setq the-line (1+ (count-lines (point-min) (point))))))
	(progn
	  (setq forms--current-record the-line)
	  (forms--show-record the-record)
	  (re-search-forward regexp nil t))))
  (setq forms--search-regexp regexp))

(defun forms-revert-buffer (&optional arg noconfirm)
  "Reverts current form to un-modified."
  (interactive "P")
  (if (or noconfirm
	  (yes-or-no-p "Revert form to unmodified? "))
      (progn
	(set-buffer-modified-p nil)
	(forms-jump-record forms--current-record))))

(defun forms-next-field (arg)
  "Jump to ARG-th next field."
  (interactive "p")

  (let ((i 0)
	(here (point))
	there
	(cnt 0))

    (if (zerop arg)
	(setq cnt 1)
      (setq cnt (+ cnt arg)))

    (if (catch 'done
	  (while (< i (length forms--markers))
	    (if (or (null (setq there (aref forms--markers i)))
		    (<= there here))
		nil
	      (if (<= (setq cnt (1- cnt)) 0)
		  (progn
		    (goto-char there)
		    (throw 'done t))))
	    (setq i (1+ i))))
	nil
      (goto-char (aref forms--markers 0)))))

(defun forms-prev-field (arg)
  "Jump to ARG-th previous field."
  (interactive "p")

  (let ((i (length forms--markers))
	(here (point))
	there
	(cnt 0))

    (if (zerop arg)
	(setq cnt 1)
      (setq cnt (+ cnt arg)))

    (if (catch 'done
	  (while (> i 0)
	    (setq i ( 1- i))
	    (if (or (null (setq there (aref forms--markers i)))
		    (>= there here))
		nil
	      (if (<= (setq cnt (1- cnt)) 0)
		  (progn
		    (goto-char there)
		    (throw 'done t))))))
	nil
      (goto-char (aref forms--markers (1- (length forms--markers)))))))
;;;
;;; Special service
;;;
(defun forms-enumerate (the-fields)
  "Take a quoted list of symbols, and set their values to sequential numbers.
The first symbol gets number 1, the second 2 and so on.
It returns the higest number.

Usage: (setq forms-number-of-fields
             (forms-enumerate
              '(field1 field2 field2 ...)))"

  (let ((the-index 0))
    (while the-fields
      (setq the-index (1+ the-index))
      (let ((el (car-safe the-fields)))
	(setq the-fields (cdr-safe the-fields))
	(set el the-index)))
    the-index))

;;; Debugging

(defvar forms--debug nil
  "*Enables forms-mode debugging if not nil.")

(defun forms--debug (&rest args)
  "Internal debugging routine."
  (if forms--debug
      (let ((ret nil))
	(while args
	  (let ((el (car-safe args)))
	    (setq args (cdr-safe args))
	    (if (stringp el)
		(setq ret (concat ret el))
	      (setq ret (concat ret (prin1-to-string el) " = "))
	      (if (boundp el)
		  (let ((vel (eval el)))
		    (setq ret (concat ret (prin1-to-string vel) "\n")))
		(setq ret (concat ret "<unbound>" "\n")))
	      (if (fboundp el)
		  (setq ret (concat ret (prin1-to-string (symbol-function el)) 
				    "\n"))))))
	(save-excursion
	  (set-buffer (get-buffer-create "*forms-mode debug*"))
	  (if (zerop (buffer-size))
	      (emacs-lisp-mode))
	  (goto-char (point-max))
	  (insert ret)))))

;;; forms.el ends here.
