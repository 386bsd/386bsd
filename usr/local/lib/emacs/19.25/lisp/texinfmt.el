;;;; texinfmt.el
;;; Copyright (C) 1985, 1986, 1988,
;;;               1990, 1991, 1992, 1993  Free Software Foundation, Inc.

;; Maintainer: Robert J. Chassell <bug-texinfo@prep.ai.mit.edu>

;;; This file is part of GNU Emacs.

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

;;; Emacs lisp functions to convert Texinfo files to Info files.

(defvar texinfmt-version "2.32 of 19 November 1993")

;;; Variable definitions

(require 'texinfo)          ; So `texinfo-footnote-style' is defined.
(require 'texnfo-upd)       ; So `texinfo-section-types-regexp' is defined.

(defvar texinfo-format-syntax-table nil)

(defvar texinfo-vindex)
(defvar texinfo-findex)
(defvar texinfo-cindex)
(defvar texinfo-pindex)
(defvar texinfo-tindex)
(defvar texinfo-kindex)
(defvar texinfo-last-node)
(defvar texinfo-node-names)
(defvar texinfo-enclosure-list)

(defvar texinfo-command-start)
(defvar texinfo-command-end)
(defvar texinfo-command-name)
(defvar texinfo-defun-type)
(defvar texinfo-last-node-pos)
(defvar texinfo-stack)
(defvar texinfo-short-index-cmds-alist)
(defvar texinfo-short-index-format-cmds-alist)
(defvar texinfo-format-filename)
(defvar texinfo-footnote-number)
(defvar texinfo-start-of-header)
(defvar texinfo-end-of-header)
(defvar texinfo-raisesections-alist)
(defvar texinfo-lowersections-alist)

;;; Syntax table

(if texinfo-format-syntax-table
    nil
  (setq texinfo-format-syntax-table (make-syntax-table))
  (modify-syntax-entry ?\" " " texinfo-format-syntax-table)
  (modify-syntax-entry ?\\ " " texinfo-format-syntax-table)
  (modify-syntax-entry ?@ "\\" texinfo-format-syntax-table)
  (modify-syntax-entry ?\^q "\\" texinfo-format-syntax-table)
  (modify-syntax-entry ?\[ "." texinfo-format-syntax-table)
  (modify-syntax-entry ?\] "." texinfo-format-syntax-table)
  (modify-syntax-entry ?\( "." texinfo-format-syntax-table)
  (modify-syntax-entry ?\) "." texinfo-format-syntax-table)
  (modify-syntax-entry ?{ "(}" texinfo-format-syntax-table)
  (modify-syntax-entry ?} "){" texinfo-format-syntax-table)
  (modify-syntax-entry ?\' "." texinfo-format-syntax-table))


;;; Top level buffer and region formatting functions

;;;###autoload
(defun texinfo-format-buffer (&optional notagify)
  "Process the current buffer as texinfo code, into an Info file.
The Info file output is generated in a buffer visiting the Info file
names specified in the @setfilename command.

Non-nil argument (prefix, if interactive) means don't make tag table
and don't split the file if large.  You can use Info-tagify and
Info-split to do these manually."
  (interactive "P")
  (let ((lastmessage "Formatting Info file..."))
    (message lastmessage)
    (texinfo-format-buffer-1)
    (if notagify
        nil
      (if (> (buffer-size) 30000)
          (progn
            (message (setq lastmessage "Making tags table for Info file..."))
            (Info-tagify)))
      (if (> (buffer-size) 100000)
          (progn
            (message (setq lastmessage "Splitting Info file..."))
            (Info-split))))
    (message (concat lastmessage
                     (if (interactive-p) "done.  Now save it." "done.")))))

(defvar texinfo-region-buffer-name "*Info Region*"
  "*Name of the temporary buffer used by \\[texinfo-format-region].")

;;;###autoload
(defun texinfo-format-region (region-beginning region-end)
  "Convert the current region of the Texinfo file to Info format.
This lets you see what that part of the file will look like in Info.
The command is bound to \\[texinfo-format-region].  The text that is
converted to Info is stored in a temporary buffer."
  (interactive "r")
  (message "Converting region to Info format...")
  (let (texinfo-command-start
        texinfo-command-end
        texinfo-command-name
        texinfo-vindex
        texinfo-findex
        texinfo-cindex
        texinfo-pindex
        texinfo-tindex
        texinfo-kindex
        texinfo-stack
        (texinfo-format-filename "")
        texinfo-example-start
        texinfo-last-node-pos
        texinfo-last-node
        texinfo-node-names
        (texinfo-footnote-number 0)
        last-input-buffer
        (fill-column-for-info fill-column)
        (input-buffer (current-buffer))
        (input-directory default-directory)
        (header-text "")
        (header-beginning 1)
        (header-end 1))
    
;;; Copy lines between beginning and end of header lines, 
;;;    if any, or else copy the `@setfilename' line, if any.
    (save-excursion
        (save-restriction
          (widen)
          (goto-char (point-min))
          (let ((search-end (save-excursion (forward-line 100) (point))))
            (if (or
                 ;; Either copy header text.
                 (and 
                  (prog1 
                      (search-forward tex-start-of-header search-end t)
                    (forward-line 1)
                    ;; Mark beginning of header.
                    (setq header-beginning (point)))
                  (prog1 
                      (search-forward tex-end-of-header nil t)
                    (beginning-of-line)
                    ;; Mark end of header
                    (setq header-end (point))))
                 ;; Or copy @filename line.
                 (prog2
                  (goto-char (point-min))
                  (search-forward "@setfilename" search-end t)
                  (beginning-of-line)
                  (setq header-beginning (point))
                  (forward-line 1)
                  (setq header-end (point))))
                
                ;; Copy header  
                (setq header-text
                      (buffer-substring
                       (min header-beginning region-beginning)
                       header-end))))))

;;; Find a buffer to use.
    (switch-to-buffer (get-buffer-create texinfo-region-buffer-name))
    (erase-buffer)
    ;; Insert the header into the buffer.
    (insert header-text)
    ;; Insert the region into the buffer.
    (insert-buffer-substring
     input-buffer
     (max region-beginning header-end)
     region-end)
    ;; Make sure region ends in a newline.
    (or (= (preceding-char) ?\n)
        (insert "\n"))
    
    (goto-char (point-min))
    (texinfo-mode)
    (message "Converting region to Info format...")
    (setq fill-column fill-column-for-info)
    ;; Install a syntax table useful for scanning command operands.
    (set-syntax-table texinfo-format-syntax-table)

    ;; Insert @include files so `texinfo-raise-lower-sections' can
    ;; work on them without losing track of multiple
    ;; @raise/@lowersections commands. 
    (while (re-search-forward "^@include" nil t)
      (setq texinfo-command-end (point))
      (let ((filename (concat input-directory
                              (texinfo-parse-line-arg))))
        (beginning-of-line)
        (delete-region (point) (save-excursion (forward-line 1) (point)))
        (message "Reading included file: %s" filename)
        (save-excursion
          (save-restriction
            (narrow-to-region
             (point)
             (+ (point) (car (cdr (insert-file-contents filename)))))
            (goto-char (point-min))
            ;; Remove `@setfilename' line from included file, if any,
            ;; so @setfilename command not duplicated.
            (if (re-search-forward 
                 "^@setfilename" (save-excursion (forward-line 100) (point)) t)
                (progn
                  (beginning-of-line)
                  (delete-region
                   (point) (save-excursion (forward-line 1) (point)))))))))

    ;; Raise or lower level of each section, if necessary.
    (goto-char (point-min))
    (texinfo-raise-lower-sections)
    ;; Append @refill to appropriate paragraphs for filling.
    (goto-char (point-min))
    (texinfo-append-refill)
    ;; If the region includes the effective end of the data,
    ;; discard everything after that.
    (goto-char (point-max))
    (if (re-search-backward "^@bye" nil t)
        (delete-region (point) (point-max)))
    ;; Make sure buffer ends in a newline.
    (or (= (preceding-char) ?\n)
        (insert "\n"))
    ;; Don't use a previous value of texinfo-enclosure-list.
    (setq texinfo-enclosure-list nil)

    (goto-char (point-min))
    (if (looking-at "\\\\input[ \t]+texinfo")
        (delete-region (point) (save-excursion (forward-line 1) (point))))

    ;; Insert Info region title text.
    (goto-char (point-min))
    (if (search-forward 
         "@setfilename" (save-excursion (forward-line 100) (point)) t)
        (progn
          (setq texinfo-command-end (point))
          (beginning-of-line)
          (setq texinfo-command-start (point))
          (let ((arg (texinfo-parse-arg-discard)))
            (insert " "
              texinfo-region-buffer-name
              " buffer for:  `") 
            (insert (file-name-nondirectory (expand-file-name arg)))
            (insert "',        -*-Text-*-\n")))
      ;; Else no `@setfilename' line
      (insert " "
              texinfo-region-buffer-name
              " buffer                       -*-Text-*-\n"))
    (insert "produced by `texinfo-format-region'\n"
            "from a region in: "
            (if (buffer-file-name input-buffer)
                  (concat "`"
                          (file-name-sans-versions
                           (file-name-nondirectory
                            (buffer-file-name input-buffer)))
                          "'")
                (concat "buffer `" (buffer-name input-buffer) "'"))
              "\nusing `texinfmt.el' version "
              texinfmt-version
              ".\n\n")

    ;; Now convert for real.
    (goto-char (point-min))
    (texinfo-format-scan)
    (goto-char (point-min))
    
    (message "Done.")))


;;; Primary internal formatting function for the whole buffer.

(defun texinfo-format-buffer-1 ()
  (let (texinfo-format-filename
        texinfo-example-start
        texinfo-command-start
        texinfo-command-end
        texinfo-command-name
        texinfo-last-node
        texinfo-last-node-pos
        texinfo-vindex
        texinfo-findex
        texinfo-cindex
        texinfo-pindex
        texinfo-tindex
        texinfo-kindex
        texinfo-stack
        texinfo-node-names
        (texinfo-footnote-number 0)
        last-input-buffer
        outfile
        (fill-column-for-info fill-column)
        (input-buffer (current-buffer))
        (input-directory default-directory))
    (setq texinfo-enclosure-list nil)
    (save-excursion
      (goto-char (point-min))
      (or (search-forward "@setfilename" nil t)
          (error "Texinfo file needs an `@setfilename FILENAME' line."))
      (setq texinfo-command-end (point))
      (setq outfile (texinfo-parse-line-arg)))
    (find-file outfile)
    (texinfo-mode)
    (setq fill-column fill-column-for-info)
    (set-syntax-table texinfo-format-syntax-table)
    (erase-buffer)
    (insert-buffer-substring input-buffer)
    (message "Converting %s to Info format..." (buffer-name input-buffer))
    
    ;; Insert @include files so `texinfo-raise-lower-sections' can
    ;; work on them without losing track of multiple
    ;; @raise/@lowersections commands. 
    (goto-char (point-min))
    (while (re-search-forward "^@include" nil t)
      (setq texinfo-command-end (point))
      (let ((filename (concat input-directory
                              (texinfo-parse-line-arg))))
        (beginning-of-line)
        (delete-region (point) (save-excursion (forward-line 1) (point)))
        (message "Reading included file: %s" filename)
        (save-excursion
          (save-restriction
            (narrow-to-region
             (point)
             (+ (point) (car (cdr (insert-file-contents filename)))))
            (goto-char (point-min))
            ;; Remove `@setfilename' line from included file, if any,
            ;; so @setfilename command not duplicated.
            (if (re-search-forward 
                 "^@setfilename"
                 (save-excursion (forward-line 100) (point)) t)
                (progn
                  (beginning-of-line)
                  (delete-region
                   (point) (save-excursion (forward-line 1) (point)))))))))
    ;; Raise or lower level of each section, if necessary.
    (goto-char (point-min))
    (texinfo-raise-lower-sections)
    ;; Append @refill to appropriate paragraphs
    (goto-char (point-min))
    (texinfo-append-refill)
    (goto-char (point-min))
    (search-forward "@setfilename")
    (beginning-of-line)
    (delete-region (point-min) (point))
    ;; Remove @bye at end of file, if it is there.
    (goto-char (point-max))
    (if (search-backward "@bye" nil t)
        (delete-region (point) (point-max)))
    ;; Make sure buffer ends in a newline.
    (or (= (preceding-char) ?\n)
        (insert "\n"))
    ;; Scan the whole buffer, converting to Info format.
    (texinfo-format-scan)
    ;; Return data for indices.
    (goto-char (point-min))
    (list outfile
          texinfo-vindex texinfo-findex texinfo-cindex
          texinfo-pindex texinfo-tindex texinfo-kindex)))


;;; Perform non-@-command file conversions: quotes and hyphens

(defun texinfo-format-convert (min max)
  ;; Convert left and right quotes to typewriter font quotes.
  (goto-char min)
  (while (search-forward "``" max t)
    (replace-match "\""))
  (goto-char min)
  (while (search-forward "''" max t)
    (replace-match "\""))
  ;; Convert three hyphens in a row to two.
  (goto-char min)
  (while (re-search-forward "\\( \\|\\w\\)\\(---\\)\\( \\|\\w\\)" max t)
    (delete-region (1+ (match-beginning 2)) (+ 2 (match-beginning
    2)))))


;;; Handle paragraph filling

(defvar texinfo-no-refill-regexp
  "^@\\(example\\|smallexample\\|lisp\\|smalllisp\\|display\\|format\\|flushleft\\|flushright\\|menu\\|titlepage\\|iftex\\|tex\\)"
  "Regexp specifying environments in which paragraphs are not filled.")

(defvar texinfo-part-of-para-regexp
  "^@\\(b{\\|bullet{\\|cite{\\|code{\\|emph{\\|equiv{\\|error{\\|expansion{\\|file{\\|i{\\|inforef{\\|kbd{\\|key{\\|lisp{\\|minus{\\|point{\\|print{\\|pxref{\\|r{\\|ref{\\|result{\\|samp{\\|sc{\\|t{\\|TeX{\\|today{\\|var{\\|w{\\|xref{\\)"
  "Regexp specifying @-commands found within paragraphs.")

(defun texinfo-append-refill ()
  "Append @refill at end of each paragraph that should be filled.
Do not append @refill to paragraphs within @example and similar environments.  
Do not append @refill to paragraphs containing @w{TEXT} or @*."

  ;; It is necessary to append @refill before other processing because
  ;; the other processing removes information that tells Texinfo
  ;; whether the text should or should not be filled.
  
  (while (< (point) (point-max))
    (let ((refill-blank-lines "^[ \t\n]*$")
          (case-fold-search nil))       ; Don't confuse @TeX and @tex....
      (beginning-of-line)
      ;; 1. Skip over blank lines;
      ;;    skip over lines beginning with @-commands, 
      ;;    but do not skip over lines
      ;;      that are no-refill environments such as @example or
      ;;      that begin with within-paragraph @-commands such as @code.
      (while (and (looking-at (concat "^@\\|^\\\\\\|" refill-blank-lines))
                  (not (looking-at 
                        (concat
                         "\\(" 
                         texinfo-no-refill-regexp
                         "\\|" 
                         texinfo-part-of-para-regexp
                         "\\)")))
                  (< (point) (point-max)))
        (forward-line 1))
      ;; 2. Skip over @example and similar no-refill environments.
      (if (looking-at texinfo-no-refill-regexp)
          (let ((environment
                 (buffer-substring (match-beginning 1) (match-end 1))))
            (progn (re-search-forward (concat "^@end " environment) nil t)
                   (forward-line 1)))
        ;; 3. Do not refill a paragraph containing @w or @*
        (if  (or
              (>= (point) (point-max))
              (re-search-forward
               "@w{\\|@\\*" (save-excursion (forward-paragraph) (point)) t))
            ;; Go to end of paragraph and do nothing.
            (forward-paragraph) 
          ;; 4. Else go to end of paragraph and insert @refill
          (forward-paragraph)
          (forward-line -1)
          (end-of-line)
          (delete-region
           (point)
           (save-excursion (skip-chars-backward " \t") (point)))
          ;; `looking-at-backward' not available in v. 18.57
          ;; (if (not (looking-at-backward "@refill\\|@bye")) ;)
          (if (not (re-search-backward
                    "@refill\\|@bye"
                    (save-excursion (beginning-of-line) (point))
                    t))
              (insert "@refill"))
          (forward-line 1))))))


;;; Handle `@raisesections' and `@lowersections' commands

;; These commands change the hierarchical level of chapter structuring
;; commands. 
;;    
;; @raisesections changes @subsection to @section,
;;                        @section    to @chapter,
;;                        etc.
;;
;; @lowersections changes @chapter    to @section
;;                        @subsection to @subsubsection,
;;                        etc.
;;
;; An @raisesections/@lowersections command changes only those
;; structuring commands that follow the @raisesections/@lowersections
;; command.
;;
;; Repeated @raisesections/@lowersections continue to raise or lower
;; the heading level.
;; 
;; An @lowersections command cancels an @raisesections command, and
;; vice versa.
;;
;; You cannot raise or lower "beyond" chapters or subsubsections, but
;; trying to do so does not elicit an error---you just get more
;; headings that mean the same thing as you keep raising or lowering
;; (for example, after a single @raisesections, both @chapter and
;; @section produce chapter headings).

(defun texinfo-raise-lower-sections ()
  "Raise or lower the hierarchical level of chapters, sections, etc. 

This function acts according to `@raisesections' and `@lowersections'
commands in the Texinfo file.

For example, an `@lowersections' command is useful if you wish to
include what is written as an outer or standalone Texinfo file in
another Texinfo file as an inner, included file.  The `@lowersections'
command changes chapters to sections, sections to subsections and so
on.

@raisesections changes @subsection to @section,
                       @section    to @chapter,
                       @heading    to @chapheading,
                       etc.

@lowersections changes @chapter    to @section,
                       @subsection to @subsubsection,
                       @heading    to @subheading,
                       etc.

An `@raisesections' or `@lowersections' command changes only those
structuring commands that follow the `@raisesections' or
`@lowersections' command.

An `@lowersections' command cancels an `@raisesections' command, and
vice versa.

Repeated use of the commands continue to raise or lower the hierarchical
level a step at a time.

An attempt to raise above `chapters' reproduces chapter commands; an
attempt to lower below subsubsections reproduces subsubsection
commands."
  
  ;; `texinfo-section-types-regexp' is defined in `texnfo-upd.el';
  ;; it is a regexp matching chapter, section, other headings
  ;; (but not the top node).

  (let (type (level 0))
    (while 
        (re-search-forward
         (concat
          "\\(\\(^@\\(raise\\|lower\\)sections\\)\\|\\("
          texinfo-section-types-regexp
          "\\)\\)")
         nil t)
      (beginning-of-line)
      (save-excursion (setq type (read (current-buffer))))
      (cond 
       
       ;; 1. Increment level
       ((eq type '@raisesections)
        (setq level (1+ level))
        (delete-region
         (point) (save-excursion (forward-line 1) (point))))
       
       ;; 2. Decrement level
       ((eq type '@lowersections)
        (setq level (1- level))
        (delete-region
         (point) (save-excursion (forward-line 1) (point))))
       
       ;; Now handle structuring commands
       ((cond
         
         ;; 3. Raise level when positive
         ((> level 0)
          (let ((count level)
                (new-level type))
            (while (> count 0)
              (setq new-level
                    (cdr (assq new-level texinfo-raisesections-alist)))
              (setq count (1- count)))
            (kill-word 1)
            (insert (symbol-name new-level))))
         
         ;; 4. Do nothing except move point when level is zero
         ((= level 0) (forward-line 1))
         
         ;; 5. Lower level when positive
         ((< level 0)
          (let ((count level)
                (new-level type))
            (while (< count 0)
              (setq new-level
                    (cdr (assq new-level texinfo-lowersections-alist)))
              (setq count (1+ count)))
            (kill-word 1)
            (insert (symbol-name new-level))))))))))

(defvar texinfo-raisesections-alist
  '((@chapter . @chapter)             ; Cannot go higher
    (@unnumbered . @unnumbered)

    (@majorheading . @majorheading)
    (@chapheading . @chapheading)
    (@appendix . @appendix)
    
    (@section . @chapter)
    (@unnumberedsec . @unnumbered)
    (@heading . @chapheading)
    (@appendixsec . @appendix)
    
    (@subsection . @section)
    (@unnumberedsubsec . @unnumberedsec)
    (@subheading . @heading)
    (@appendixsubsec . @appendixsec)
    
    (@subsubsection . @subsection)
    (@unnumberedsubsubsec . @unnumberedsubsec)
    (@subsubheading . @subheading)
    (@appendixsubsubsec . @appendixsubsec))
  "*An alist of next higher levels for chapters, sections. etc.
For example, section to chapter, subsection to section.
Used by `texinfo-raise-lower-sections'.
The keys specify types of section; the values correspond to the next
higher types.")

(defvar texinfo-lowersections-alist
  '((@chapter . @section)  
    (@unnumbered . @unnumberedsec)
    (@majorheading . @heading)
    (@chapheading . @heading)
    (@appendix . @appendixsec)
    
    (@section . @subsection)
    (@unnumberedsec . @unnumberedsubsec)
    (@heading . @subheading)
    (@appendixsec . @appendixsubsec)
    
    (@subsection . @subsubsection)
    (@unnumberedsubsec . @unnumberedsubsubsec)
    (@subheading . @subsubheading)
    (@appendixsubsec . @appendixsubsubsec)
    
    (@subsubsection . @subsubsection) ; Cannot go lower.
    (@unnumberedsubsubsec . @unnumberedsubsubsec)
    (@subsubheading . @subsubheading)
    (@appendixsubsubsec . @appendixsubsubsec))
  "*An alist of next lower levels for chapters, sections. etc.
For example, chapter to section, section to subsection.
Used by `texinfo-raise-lower-sections'.
The keys specify types of section; the values correspond to the next
lower types.")


;;; Perform those texinfo-to-info conversions that apply to the whole input
;;; uniformly.

(defun texinfo-format-scan ()
  (texinfo-format-convert (point-min) (point-max))
  ;; Scan for @-commands.
  (goto-char (point-min))
  (while (search-forward "@" nil t)
    (if (looking-at "[@{}'` *]")
        ;; Handle a few special @-followed-by-one-char commands.
        (if (= (following-char) ?*)
            (progn
              ;; remove command
              (delete-region (1- (point)) (1+ (point)))
              ;; insert return if not at end of line;
              ;; else line is already broken.
              (if (not (= (following-char) ?\n))
                  (insert ?\n)))      
          ;; The other characters are simply quoted.  Delete the @.
          (delete-char -1)
          (forward-char 1))
      ;; @ is followed by a command-word; find the end of the word.
      (setq texinfo-command-start (1- (point)))
      (if (= (char-syntax (following-char)) ?w)
          (forward-word 1)
        (forward-char 1))
      (setq texinfo-command-end (point))
      ;; Call the handler for this command.
      (setq texinfo-command-name
            (intern (buffer-substring
             (1+ texinfo-command-start) texinfo-command-end)))
      (let ((enclosure-type
             (assoc
              (symbol-name texinfo-command-name)
              texinfo-enclosure-list)))
        (if enclosure-type
            (progn
              (insert
               (car (car (cdr enclosure-type))) 
               (texinfo-parse-arg-discard)
               (car (cdr (car (cdr enclosure-type)))))
              (goto-char texinfo-command-start))
          (let ((cmd (get texinfo-command-name 'texinfo-format)))
            (if cmd (funcall cmd) (texinfo-unsupported)))))))
  
  (cond (texinfo-stack
         (goto-char (nth 2 (car texinfo-stack)))
         (error "Unterminated @%s" (car (car texinfo-stack))))))

(put 'begin 'texinfo-format 'texinfo-format-begin)
(defun texinfo-format-begin ()
  (texinfo-format-begin-end 'texinfo-format))

(put 'end 'texinfo-format 'texinfo-format-end)
(defun texinfo-format-end ()
  (texinfo-format-begin-end 'texinfo-end))

(defun texinfo-format-begin-end (prop)
  (setq texinfo-command-name (intern (texinfo-parse-line-arg)))
  (let ((cmd (get texinfo-command-name prop)))
    (if cmd (funcall cmd)
      (texinfo-unsupported))))

;;; Parsing functions

(defun texinfo-parse-line-arg ()
  (goto-char texinfo-command-end)
  (let ((start (point)))
    (cond ((looking-at " ")
           (skip-chars-forward " ")
           (setq start (point))
           (end-of-line)
           (skip-chars-backward " ")
           (delete-region (point) (progn (end-of-line) (point)))
           (setq texinfo-command-end (1+ (point))))
          ((looking-at "{")
           (setq start (1+ (point)))
           (forward-list 1)
           (setq texinfo-command-end (point))
           (forward-char -1))
          (t
           (error "Invalid texinfo command arg format")))
    (prog1 (buffer-substring start (point))
           (if (eolp) (forward-char 1)))))

(defun texinfo-parse-expanded-arg ()
  (goto-char texinfo-command-end)
  (let ((start (point))
        marker)
    (cond ((looking-at " ")
           (skip-chars-forward " ")
           (setq start (point))
           (end-of-line)
           (setq texinfo-command-end (1+ (point))))
          ((looking-at "{")
           (setq start (1+ (point)))
           (forward-list 1)
           (setq texinfo-command-end (point))
           (forward-char -1))
          (t
           (error "Invalid texinfo command arg format")))
    (setq marker (move-marker (make-marker) texinfo-command-end))
    (texinfo-format-expand-region start (point))
    (setq texinfo-command-end (marker-position marker))
    (move-marker marker nil)
    (prog1 (buffer-substring start (point))
           (if (eolp) (forward-char 1)))))

(defun texinfo-format-expand-region (start end)
  (save-restriction
    (narrow-to-region start end)
    (let (texinfo-command-start
          texinfo-command-end
          texinfo-command-name
          texinfo-stack)
      (texinfo-format-scan))
    (goto-char (point-max))))

(defun texinfo-parse-arg-discard ()
  (prog1 (texinfo-parse-line-arg)
         (texinfo-discard-command)))

(defun texinfo-discard-command ()
  (delete-region texinfo-command-start texinfo-command-end))

(defun texinfo-optional-braces-discard ()
  "Discard braces following command, if any."
  (goto-char texinfo-command-end)
  (let ((start (point)))
    (cond ((looking-at "[ \t]*\n"))     ; do nothing
          ((looking-at "{")             ; remove braces, if any
           (forward-list 1)
           (setq texinfo-command-end (point)))
          (t
           (error
            "Invalid `texinfo-optional-braces-discard' format \(need braces?\)")))
    (delete-region texinfo-command-start texinfo-command-end)))

(defun texinfo-format-parse-line-args ()
  (let ((start (1- (point)))
        next beg end
        args)
    (skip-chars-forward " ")
    (while (not (eolp))
      (setq beg (point))
      (re-search-forward "[\n,]")
      (setq next (point))
      (if (bolp) (setq next (1- next)))
      (forward-char -1)
      (skip-chars-backward " ")
      (setq end (point))
      (setq args (cons (if (> end beg) (buffer-substring beg end))
                       args))
      (goto-char next)
      (skip-chars-forward " "))
    (if (eolp) (forward-char 1))
    (setq texinfo-command-end (point))
    (nreverse args)))

(defun texinfo-format-parse-args ()
  (let ((start (1- (point)))
        next beg end
        args)
    (search-forward "{")
    (save-excursion
      (texinfo-format-expand-region 
       (point)
       (save-excursion (up-list 1) (1- (point)))))
    ;; The following does not handle cross references of the form:
    ;; `@xref{bullet, , @code{@@bullet}@{@}}.' because the
    ;; re-search-forward finds the first right brace after the second
    ;; comma. 
    (while (/= (preceding-char) ?\})
      (skip-chars-forward " \t\n")
      (setq beg (point))
      (re-search-forward "[},]")
      (setq next (point))
      (forward-char -1)
      (skip-chars-backward " \t\n")
      (setq end (point))
      (cond ((< beg end)
             (goto-char beg)
             (while (search-forward "\n" end t)
               (replace-match " "))))
      (setq args (cons (if (> end beg) (buffer-substring beg end))
                       args))
      (goto-char next))
    (if (eolp) (forward-char 1))
    (setq texinfo-command-end (point))
    (nreverse args)))

(defun texinfo-format-parse-defun-args ()
  (goto-char texinfo-command-end)
  (let ((start (point)))
    (end-of-line)
    (setq texinfo-command-end (1+ (point)))
    (let ((marker (move-marker (make-marker) texinfo-command-end)))
      (texinfo-format-expand-region start (point))
      (setq texinfo-command-end (marker-position marker))
      (move-marker marker nil))
    (goto-char start)
    (let ((args '())
          beg end)
      (skip-chars-forward " ")
      (while (not (eolp))
        (cond ((looking-at "{")
               (setq beg (1+ (point)))
               (forward-list 1)
               (setq end (1- (point))))
              (t
               (setq beg (point))
               (re-search-forward "[\n ]")
               (forward-char -1)
               (setq end (point))))
        (setq args (cons (buffer-substring beg end) args))
        (skip-chars-forward " "))
      (forward-char 1)
      (nreverse args))))

(defun texinfo-discard-line ()
  (goto-char texinfo-command-end)
  (skip-chars-forward " \t")
  (or (eolp)
      (error "Extraneous text at end of command line."))
  (goto-char texinfo-command-start)
  (or (bolp)
      (error "Extraneous text at beginning of command line."))
  (delete-region (point) (progn (forward-line 1) (point))))

(defun texinfo-discard-line-with-args ()
  (goto-char texinfo-command-start)
  (delete-region (point) (progn (forward-line 1) (point))))


;;; @setfilename

;; Only `texinfo-format-buffer' handles @setfilename with this
;; definition; `texinfo-format-region' handles @setfilename, if any,
;; specially. 
(put 'setfilename 'texinfo-format 'texinfo-format-setfilename)
(defun texinfo-format-setfilename ()
  (let ((arg (texinfo-parse-arg-discard)))
    (message "Formatting Info file: %s" arg)
    (setq texinfo-format-filename
          (file-name-nondirectory (expand-file-name arg)))
    (insert "Info file: "
            texinfo-format-filename ",    -*-Text-*-\n"
            ;; Date string removed so that regression testing is easier.
            ;; "produced on "
            ;; (substring (current-time-string) 8 10) " "
            ;; (substring (current-time-string) 4 7) " "
            ;; (substring (current-time-string) -4)  " "
            "produced by `texinfo-format-buffer'\n"
            "from file"
            (if (buffer-file-name input-buffer)
                (concat " `"
                        (file-name-sans-versions
                         (file-name-nondirectory
                          (buffer-file-name input-buffer)))
                        "'")
              (concat "buffer `" (buffer-name input-buffer) "'"))
            "\nusing `texinfmt.el' version "
            texinfmt-version
            ".\n\n")))

;;; @node, @menu

(put 'node 'texinfo-format 'texinfo-format-node)
(put 'nwnode 'texinfo-format 'texinfo-format-node)
(defun texinfo-format-node ()
  (let* ((args (texinfo-format-parse-line-args))
         (name (nth 0 args))
         (next (nth 1 args))
         (prev (nth 2 args))
         (up (nth 3 args)))
    (texinfo-discard-command)
    (setq texinfo-last-node name)
    (let ((tem (downcase name)))
      (if (assoc tem texinfo-node-names)
          (error "Duplicate node name: %s" name)
        (setq texinfo-node-names (cons (list tem) texinfo-node-names))))
    (setq texinfo-footnote-number 0)
    (or (bolp)
        (insert ?\n))
    (insert "\^_\nFile: " texinfo-format-filename
            ", Node: " name)
    (if next
        (insert ", Next: " next))
    (if prev
        (insert ", Prev: " prev))
    (if up
        (insert ", Up: " up))
    (insert ?\n)
    (setq texinfo-last-node-pos (point))))

(put 'menu 'texinfo-format 'texinfo-format-menu)
(defun texinfo-format-menu ()
  (texinfo-discard-line)
  (insert "* Menu:\n\n"))

(put 'menu 'texinfo-end 'texinfo-discard-command)


;;; Cross references

; @xref {NODE, FNAME, NAME, FILE, DOCUMENT}
; -> *Note FNAME: (FILE)NODE
;   If FILE is missing,
;    *Note FNAME: NODE
;   If FNAME is empty and NAME is present
;    *Note NAME: Node
;   If both NAME and FNAME are missing
;    *Note NODE::
;   texinfo ignores the DOCUMENT argument.
; -> See section <xref to NODE> [NAME, else NODE], page <xref to NODE>
;   If FILE is specified, (FILE)NODE is used for xrefs.
;   If fifth argument DOCUMENT is specified, produces
;    See section <xref to NODE> [NAME, else NODE], page <xref to NODE>
;    of DOCUMENT

; @ref             a reference that does not put `See' or `see' in
;                  the hardcopy and is the same as @xref in Info
(put 'ref 'texinfo-format 'texinfo-format-xref)

(put 'xref 'texinfo-format 'texinfo-format-xref)
(defun texinfo-format-xref ()
  (let ((args (texinfo-format-parse-args)))
    (texinfo-discard-command)
    (insert "*Note ")
    (let ((fname (or (nth 1 args) (nth 2 args))))
      (if (null (or fname (nth 3 args)))
          (insert (car args) "::")
        (insert (or fname (car args)) ": ")
        (if (nth 3 args)
            (insert "(" (nth 3 args) ")"))
        (insert (car args))))))

(put 'pxref 'texinfo-format 'texinfo-format-pxref)
(defun texinfo-format-pxref ()
  (texinfo-format-xref)
  (or (save-excursion
        (forward-char -2)
        (looking-at "::"))
      (insert ".")))

;@inforef{NODE, FNAME, FILE}
;Like @xref{NODE, FNAME,,FILE} in texinfo.
;In Tex, generates "See Info file FILE, node NODE"
(put 'inforef 'texinfo-format 'texinfo-format-inforef)
(defun texinfo-format-inforef ()
  (let ((args (texinfo-format-parse-args)))
    (texinfo-discard-command)
    (if (nth 1 args)
        (insert "*Note " (nth 1 args) ": (" (nth 2 args) ")" (car args))
      (insert "*Note " "(" (nth 2 args) ")" (car args) "::"))))


;;; Section headings

(put 'majorheading 'texinfo-format 'texinfo-format-chapter)
(put 'chapheading 'texinfo-format 'texinfo-format-chapter)
(put 'ichapter 'texinfo-format 'texinfo-format-chapter)
(put 'chapter 'texinfo-format 'texinfo-format-chapter)
(put 'iappendix 'texinfo-format 'texinfo-format-chapter)
(put 'appendix 'texinfo-format 'texinfo-format-chapter)
(put 'iunnumbered 'texinfo-format 'texinfo-format-chapter)
(put 'top 'texinfo-format 'texinfo-format-chapter)
(put 'unnumbered 'texinfo-format 'texinfo-format-chapter)
(defun texinfo-format-chapter ()
  (texinfo-format-chapter-1 ?*))

(put 'heading 'texinfo-format 'texinfo-format-section)
(put 'isection 'texinfo-format 'texinfo-format-section)
(put 'section 'texinfo-format 'texinfo-format-section)
(put 'iappendixsection 'texinfo-format 'texinfo-format-section)
(put 'appendixsection 'texinfo-format 'texinfo-format-section)
(put 'iappendixsec 'texinfo-format 'texinfo-format-section)
(put 'appendixsec 'texinfo-format 'texinfo-format-section)
(put 'iunnumberedsec 'texinfo-format 'texinfo-format-section)
(put 'unnumberedsec 'texinfo-format 'texinfo-format-section)
(defun texinfo-format-section ()
  (texinfo-format-chapter-1 ?=))

(put 'subheading 'texinfo-format 'texinfo-format-subsection)
(put 'isubsection 'texinfo-format 'texinfo-format-subsection)
(put 'subsection 'texinfo-format 'texinfo-format-subsection)
(put 'iappendixsubsec 'texinfo-format 'texinfo-format-subsection)
(put 'appendixsubsec 'texinfo-format 'texinfo-format-subsection)
(put 'iunnumberedsubsec 'texinfo-format 'texinfo-format-subsection)
(put 'unnumberedsubsec 'texinfo-format 'texinfo-format-subsection)
(defun texinfo-format-subsection ()
  (texinfo-format-chapter-1 ?-))

(put 'subsubheading 'texinfo-format 'texinfo-format-subsubsection)
(put 'isubsubsection 'texinfo-format 'texinfo-format-subsubsection)
(put 'subsubsection 'texinfo-format 'texinfo-format-subsubsection)
(put 'iappendixsubsubsec 'texinfo-format 'texinfo-format-subsubsection)
(put 'appendixsubsubsec 'texinfo-format 'texinfo-format-subsubsection)
(put 'iunnumberedsubsubsec 'texinfo-format 'texinfo-format-subsubsection)
(put 'unnumberedsubsubsec 'texinfo-format 'texinfo-format-subsubsection)
(defun texinfo-format-subsubsection ()
  (texinfo-format-chapter-1 ?.))

(defun texinfo-format-chapter-1 (belowchar)
  (let ((arg (texinfo-parse-arg-discard)))
    (message "Formatting: %s ... " arg)   ; So we can see where we are.
    (insert ?\n arg ?\n "@SectionPAD " belowchar ?\n)
    (forward-line -2)))

(put 'SectionPAD 'texinfo-format 'texinfo-format-sectionpad)
(defun texinfo-format-sectionpad ()
  (let ((str (texinfo-parse-arg-discard)))
    (forward-char -1)
    (let ((column (current-column)))
      (forward-char 1)
      (while (> column 0)
        (insert str)
        (setq column (1- column))))
    (insert ?\n)))


;;; Space controling commands:  @. and @:   
(put '\. 'texinfo-format 'texinfo-format-\.)
(defun texinfo-format-\. ()
  (texinfo-discard-command)
  (insert "."))

(put '\: 'texinfo-format 'texinfo-format-\:)
(defun texinfo-format-\: ()
  (texinfo-discard-command))


;;; @center, @sp, and @br

(put 'center 'texinfo-format 'texinfo-format-center)
(defun texinfo-format-center ()
  (let ((arg (texinfo-parse-expanded-arg)))
    (texinfo-discard-command)
    (insert arg)
    (insert ?\n)
    (save-restriction
      (goto-char (1- (point)))
      (let ((indent-tabs-mode nil))
        (center-line)))))

(put 'sp 'texinfo-format 'texinfo-format-sp)
(defun texinfo-format-sp ()
  (let* ((arg (texinfo-parse-arg-discard))
         (num (read arg)))
    (insert-char ?\n num)))

(put 'br 'texinfo-format 'texinfo-format-paragraph-break)
(defun texinfo-format-paragraph-break ()
  "Force a paragraph break.
If used within a line, follow `@br' with braces."
  (texinfo-optional-braces-discard)
  ;; insert one return if at end of line;
  ;; else insert two returns, to generate a blank line.
  (if (= (following-char) ?\n)
      (insert ?\n)
    (insert-char ?\n 2)))


;;; @footnote  and  @footnotestyle

; In Texinfo, footnotes are created with the `@footnote' command.
; This command is followed immediately by a left brace, then by the text of
; the footnote, and then by a terminating right brace.  The
; template for a footnote is:
; 
;      @footnote{TEXT}
;
; Info has two footnote styles:
; 
;    * In the End of node style, all the footnotes for a single node
;      are placed at the end of that node.  The footnotes are
;      separated from the rest of the node by a line of dashes with
;      the word `Footnotes' within it.
; 
;    * In the Separate node style, all the footnotes for a single node
;      are placed in an automatically constructed node of their own.

; Footnote style is specified by the @footnotestyle command, either
;    @footnotestyle separate
; or
;    @footnotestyle end
; 
; The default is  separate

(defvar texinfo-footnote-style "separate" 
  "Footnote style, either separate or end.")

(put 'footnotestyle 'texinfo-format 'texinfo-footnotestyle)
(defun texinfo-footnotestyle ()
  "Specify whether footnotes are at end of node or in separate nodes.
Argument is either end or separate."
  (setq texinfo-footnote-style (texinfo-parse-arg-discard)))

(defvar texinfo-footnote-number)

(put 'footnote 'texinfo-format 'texinfo-format-footnote)
(defun texinfo-format-footnote ()
  "Format a footnote in either end of node or separate node style.
The   texinfo-footnote-style  variable controls which style is used."
  (setq texinfo-footnote-number (1+ texinfo-footnote-number))
  (cond ((string= texinfo-footnote-style "end")
         (texinfo-format-end-node))
        ((string= texinfo-footnote-style "separate")
         (texinfo-format-separate-node))))

(defun texinfo-format-separate-node ()
  "Format footnote in Separate node style, with notes in own node.
The node is constructed automatically."
  (let* (start
         (arg (texinfo-parse-line-arg))
         (node-name-beginning
          (save-excursion
            (re-search-backward
             "^File: \\w+\\(\\w\\|\\s_\\|\\.\\|,\\)*[ \t]+Node:")
            (match-end 0)))
         (node-name
          (save-excursion
            (buffer-substring
             (progn (goto-char node-name-beginning) ; skip over node command
                    (skip-chars-forward " \t")  ; and over spaces
                    (point))
             (if (search-forward
                  ","
                  (save-excursion (end-of-line) (point)) t) ; bound search
                 (1- (point))
               (end-of-line) (point))))))
    (texinfo-discard-command)  ; remove or insert whitespace, as needed
    (delete-region (save-excursion (skip-chars-backward " \t\n") (point))
                   (point))
    (insert (format " (%d) (*Note %s-Footnotes::)"
                    texinfo-footnote-number node-name))
    (fill-paragraph nil)
    (save-excursion
    (if (re-search-forward "^@node" nil 'move)
        (forward-line -1))

    ;; two cases: for the first footnote, we must insert a node header;
    ;; for the second and subsequent footnotes, we need only insert 
    ;; the text of the  footnote.

    (if (save-excursion
         (re-search-backward
          (concat node-name "-Footnotes, Up: ")
          node-name-beginning
          t))
        (progn   ; already at least one footnote
          (setq start (point))
          (insert (format "\n(%d)  %s\n" texinfo-footnote-number arg))
          (fill-region start (point)))
      ;; else not yet a footnote
      (insert "\n\^_\nFile: "  texinfo-format-filename
              "  Node: " node-name "-Footnotes, Up: " node-name "\n")
      (setq start (point))
      (insert (format "\n(%d)  %s\n" texinfo-footnote-number arg))
      (fill-region start (point))))))

(defun texinfo-format-end-node ()
  "Format footnote in the End of node style, with notes at end of node."
  (let (start
        (arg (texinfo-parse-line-arg)))
    (texinfo-discard-command)  ; remove or insert whitespace, as needed
    (delete-region (save-excursion (skip-chars-backward " \t\n") (point))
                   (point))
    (insert (format " (%d) " texinfo-footnote-number))
    (fill-paragraph nil)
    (save-excursion
      (if (search-forward "\n--------- Footnotes ---------\n" nil t)
          (progn ; already have footnote, put new one before end of node
            (if (re-search-forward "^@node" nil 'move)
                (forward-line -1))
            (setq start (point))
            (insert (format "\n(%d)  %s\n" texinfo-footnote-number arg))
            (fill-region start (point)))
        ;; else no prior footnote
        (if (re-search-forward "^@node" nil 'move)
            (forward-line -1))
        (insert "\n--------- Footnotes ---------\n")
        (setq start (point))
        (insert (format "\n(%d)  %s\n" texinfo-footnote-number arg))))))


;;; @itemize, @enumerate, and similar commands

;; @itemize pushes (itemize "COMMANDS" STARTPOS) on texinfo-stack.
;; @enumerate pushes (enumerate 0 STARTPOS).
;; @item dispatches to the texinfo-item prop of the first elt of the list.
;; For itemize, this puts in and rescans the COMMANDS.
;; For enumerate, this increments the number and puts it in.
;; In either case, it puts a Backspace at the front of the line
;; which marks it not to be indented later.
;; All other lines get indented by 5 when the @end is reached.

(defvar texinfo-stack-depth 0
  "Count of number of unpopped texinfo-push-stack calls.
Used by @refill indenting command to avoid indenting within lists, etc.")

(defun texinfo-push-stack (check arg)
  (setq texinfo-stack-depth (1+ texinfo-stack-depth))
  (setq texinfo-stack
        (cons (list check arg texinfo-command-start)
              texinfo-stack)))

(defun texinfo-pop-stack (check)
  (setq texinfo-stack-depth (1- texinfo-stack-depth))
  (if (null texinfo-stack)
      (error "Unmatched @end %s" check))
  (if (not (eq (car (car texinfo-stack)) check))
      (error "@end %s matches @%s"
             check (car (car texinfo-stack))))
  (prog1 (cdr (car texinfo-stack))
         (setq texinfo-stack (cdr texinfo-stack))))

(put 'itemize 'texinfo-format 'texinfo-itemize)
(defun texinfo-itemize ()
  (texinfo-push-stack
   'itemize
   (progn (skip-chars-forward " \t")
          (if (eolp)
              "@bullet"
            (texinfo-parse-line-arg))))
  (texinfo-discard-line-with-args)
  (setq fill-column (- fill-column 5)))

(put 'itemize 'texinfo-end 'texinfo-end-itemize)
(defun texinfo-end-itemize ()
  (setq fill-column (+ fill-column 5))
  (texinfo-discard-command)
  (let ((stacktop
         (texinfo-pop-stack 'itemize)))
    (texinfo-do-itemize (nth 1 stacktop))))

(put 'enumerate 'texinfo-format 'texinfo-enumerate)
(defun texinfo-enumerate ()
  (texinfo-push-stack
   'enumerate 
   (progn (skip-chars-forward " \t")
          (if (eolp)
              1
            (read (current-buffer)))))
  (if (and (symbolp (car (cdr (car texinfo-stack))))
           (> 1 (length (symbol-name (car (cdr (car texinfo-stack)))))))
      (error
       "@enumerate: Use a number or letter, eg: 1, A, a, 3, B, or d." ))
  (texinfo-discard-line-with-args)
  (setq fill-column (- fill-column 5)))

(put 'enumerate 'texinfo-end 'texinfo-end-enumerate)
(defun texinfo-end-enumerate ()
  (setq fill-column (+ fill-column 5))
  (texinfo-discard-command)
  (let ((stacktop
         (texinfo-pop-stack 'enumerate)))
    (texinfo-do-itemize (nth 1 stacktop))))

;; @alphaenumerate never became a standard part of Texinfo
(put 'alphaenumerate 'texinfo-format 'texinfo-alphaenumerate)
(defun texinfo-alphaenumerate ()
  (texinfo-push-stack 'alphaenumerate (1- ?a))
  (setq fill-column (- fill-column 5))
  (texinfo-discard-line))

(put 'alphaenumerate 'texinfo-end 'texinfo-end-alphaenumerate)
(defun texinfo-end-alphaenumerate ()
  (setq fill-column (+ fill-column 5))
  (texinfo-discard-command)
  (let ((stacktop
         (texinfo-pop-stack 'alphaenumerate)))
    (texinfo-do-itemize (nth 1 stacktop))))

;; @capsenumerate never became a standard part of Texinfo
(put 'capsenumerate 'texinfo-format 'texinfo-capsenumerate)
(defun texinfo-capsenumerate ()
  (texinfo-push-stack 'capsenumerate (1- ?A))
  (setq fill-column (- fill-column 5))
  (texinfo-discard-line))

(put 'capsenumerate 'texinfo-end 'texinfo-end-capsenumerate)
(defun texinfo-end-capsenumerate ()
  (setq fill-column (+ fill-column 5))
  (texinfo-discard-command)
  (let ((stacktop
         (texinfo-pop-stack 'capsenumerate)))
    (texinfo-do-itemize (nth 1 stacktop))))

;; At the @end, indent all the lines within the construct
;; except those marked with backspace.  FROM says where
;; construct started.
(defun texinfo-do-itemize (from)
  (save-excursion
   (while (progn (forward-line -1)
                 (>= (point) from))
     (if (= (following-char) ?\b)
         (save-excursion
           (delete-char 1)
           (end-of-line)
           (delete-char 6))
       (if (not (looking-at "[ \t]*$"))
           (save-excursion (insert "     ")))))))

(put 'item 'texinfo-format 'texinfo-item)
(put 'itemx 'texinfo-format 'texinfo-item)
(defun texinfo-item ()
  (funcall (get (car (car texinfo-stack)) 'texinfo-item)))

(put 'itemize 'texinfo-item 'texinfo-itemize-item)
(defun texinfo-itemize-item ()
  ;; (texinfo-discard-line)   ; Did not handle text on same line as @item.
  (delete-region (1+ (point)) (save-excursion (beginning-of-line) (point)))
  (if (looking-at "[ \t]*[^ \t\n]+")
      ;; Text on same line as @item command.
      (insert "\b   " (nth 1 (car texinfo-stack)) " \n")
    ;; Else text on next line.
    (insert "\b   " (nth 1 (car texinfo-stack)) " "))
  (forward-line -1))

(put 'enumerate 'texinfo-item 'texinfo-enumerate-item)
(defun texinfo-enumerate-item ()
  (texinfo-discard-line)
  (let (enumerating-symbol)
    (cond ((integerp (car (cdr (car texinfo-stack))))
           (setq enumerating-symbol (car (cdr (car texinfo-stack))))
           (insert ?\b (format "%3d. " enumerating-symbol) ?\n)
           (setcar (cdr (car texinfo-stack)) (1+ enumerating-symbol)))
          ((symbolp (car (cdr (car texinfo-stack))))
           (setq enumerating-symbol
                 (symbol-name (car (cdr (car texinfo-stack)))))
           (if (or (equal ?\[ (string-to-char enumerating-symbol))
                   (equal ?\{ (string-to-char enumerating-symbol)))
               (error
                "Too many items in enumerated list; alphabet ends at Z."))
           (insert ?\b (format "%3s. " enumerating-symbol) ?\n)
           (setcar (cdr (car texinfo-stack))
                   (make-symbol
                    (char-to-string
                     (1+ 
                      (string-to-char enumerating-symbol))))))
          (t
          (error
           "@enumerate: Use a number or letter, eg: 1, A, a, 3, B or d." )))
    (forward-line -1)))

(put 'alphaenumerate 'texinfo-item 'texinfo-alphaenumerate-item)
(defun texinfo-alphaenumerate-item ()
  (texinfo-discard-line)
  (let ((next (1+ (car (cdr (car texinfo-stack))))))
    (if (> next ?z)
        (error "More than 26 items in @alphaenumerate; get a bigger alphabet."))
    (setcar (cdr (car texinfo-stack)) next)
    (insert "\b  " next ". \n"))
  (forward-line -1))

(put 'capsenumerate 'texinfo-item 'texinfo-capsenumerate-item)
(defun texinfo-capsenumerate-item ()
  (texinfo-discard-line)
  (let ((next (1+ (car (cdr (car texinfo-stack))))))
    (if (> next ?Z)
        (error "More than 26 items in @capsenumerate; get a bigger alphabet."))
    (setcar (cdr (car texinfo-stack)) next)
    (insert "\b  " next ". \n"))
  (forward-line -1))


;;; @table

; The `@table' command produces two-column tables.

(put 'table 'texinfo-format 'texinfo-table)
(defun texinfo-table ()
  (texinfo-push-stack 
   'table 
   (progn (skip-chars-forward " \t")
          (if (eolp)
              "@asis"
            (texinfo-parse-line-arg))))
  (texinfo-discard-line-with-args)
  (setq fill-column (- fill-column 5)))

(put 'table 'texinfo-item 'texinfo-table-item)
(defun texinfo-table-item ()
  (let ((arg (texinfo-parse-arg-discard))
        (itemfont (car (cdr (car texinfo-stack)))))
    (insert ?\b itemfont ?\{ arg "}\n     \n"))
  (forward-line -2))

(put 'table 'texinfo-end 'texinfo-end-table)
(defun texinfo-end-table ()
  (setq fill-column (+ fill-column 5))
  (texinfo-discard-command)
  (let ((stacktop
         (texinfo-pop-stack 'table)))
    (texinfo-do-itemize (nth 1 stacktop))))

;; @description appears to be an undocumented variant on @table that
;; does not require an arg.  It fails in texinfo.tex 2.58 and is not
;; part of makeinfo.c   The command appears to be a relic of the past.
(put 'description 'texinfo-end 'texinfo-end-table)
(put 'description 'texinfo-format 'texinfo-description)
(defun texinfo-description ()
  (texinfo-push-stack 'table "@asis")
  (setq fill-column (- fill-column 5))
  (texinfo-discard-line))


;;; @ftable, @vtable

; The `@ftable' and `@vtable' commands are like the `@table' command
; but they also insert each entry in the first column of the table
; into the function or variable index.

;; Handle the @ftable and @vtable commands:

(put 'ftable 'texinfo-format 'texinfo-ftable)
(put 'vtable 'texinfo-format 'texinfo-vtable)

(defun texinfo-ftable () (texinfo-indextable 'ftable))
(defun texinfo-vtable () (texinfo-indextable 'vtable))

(defun texinfo-indextable (table-type)
  (texinfo-push-stack table-type (texinfo-parse-arg-discard))
  (setq fill-column (- fill-column 5)))

;; Handle the @item commands within ftable and vtable:

(put 'ftable 'texinfo-item 'texinfo-ftable-item)
(put 'vtable 'texinfo-item 'texinfo-vtable-item)

(defun texinfo-ftable-item () (texinfo-indextable-item 'texinfo-findex))
(defun texinfo-vtable-item () (texinfo-indextable-item 'texinfo-vindex))

(defun texinfo-indextable-item (index-type)
  (let ((item (texinfo-parse-arg-discard))
        (itemfont (car (cdr (car texinfo-stack))))
        (indexvar index-type))
    (insert ?\b itemfont ?\{ item "}\n     \n")
    (set indexvar
         (cons
          (list item texinfo-last-node)
          (symbol-value indexvar)))
    (forward-line -2)))

;; Handle @end ftable, @end vtable

(put 'ftable 'texinfo-end 'texinfo-end-ftable)
(put 'vtable 'texinfo-end 'texinfo-end-vtable)

(defun texinfo-end-ftable () (texinfo-end-indextable 'ftable))
(defun texinfo-end-vtable () (texinfo-end-indextable 'vtable))

(defun texinfo-end-indextable (table-type)
  (setq fill-column (+ fill-column 5))
  (texinfo-discard-command)
  (let ((stacktop
         (texinfo-pop-stack table-type)))
    (texinfo-do-itemize (nth 1 stacktop))))


;;; @ifinfo,  @iftex, @tex

(put 'ifinfo 'texinfo-format 'texinfo-discard-line)
(put 'ifinfo 'texinfo-end 'texinfo-discard-command)

(put 'iftex 'texinfo-format 'texinfo-format-iftex)
(defun texinfo-format-iftex ()
  (delete-region texinfo-command-start
                 (progn (re-search-forward "@end iftex[ \t]*\n")
                        (point))))

(put 'tex 'texinfo-format 'texinfo-format-tex)
(defun texinfo-format-tex ()
  (delete-region texinfo-command-start
                 (progn (re-search-forward "@end tex[ \t]*\n")
                        (point))))


;;; @titlepage

(put 'titlepage 'texinfo-format 'texinfo-format-titlepage)
(defun texinfo-format-titlepage ()
  (delete-region texinfo-command-start
                 (progn (re-search-forward "@end titlepage[ \t]*\n")
                        (point))))

(put 'endtitlepage 'texinfo-format 'texinfo-discard-line)

; @titlespec         an alternative titling command; ignored by Info

(put 'titlespec 'texinfo-format 'texinfo-format-titlespec)
(defun texinfo-format-titlespec ()
  (delete-region texinfo-command-start
                 (progn (re-search-forward "@end titlespec[ \t]*\n")
                        (point))))

(put 'endtitlespec 'texinfo-format 'texinfo-discard-line)


;;; @today

(put 'today 'texinfo-format 'texinfo-format-today)

; Produces Day Month Year style of output.  eg `1 Jan 1900'
; The `@today{}' command requires a pair of braces, like `@dots{}'.
(defun texinfo-format-today ()
  (texinfo-parse-arg-discard)
  (insert (format "%s %s %s"
          (substring (current-time-string) 8 10)
          (substring (current-time-string) 4 7)
          (substring (current-time-string) -4))))


;;; @ignore

(put 'ignore 'texinfo-format 'texinfo-format-ignore)
(defun texinfo-format-ignore ()
  (delete-region texinfo-command-start
                 (progn (re-search-forward "@end ignore[ \t]*\n")
                        (point))))

(put 'endignore 'texinfo-format 'texinfo-discard-line)


;;; Define the Info enclosure command: @definfoenclose

; A `@definfoenclose' command may be used to define a highlighting
; command for Info, but not for TeX.  A command defined using
; `@definfoenclose' marks text by enclosing it in strings that precede
; and follow the text.
; 
; Presumably, if you define a command with `@definfoenclose` for Info,
; you will also define the same command in the TeX definitions file,
; `texinfo.tex' in a manner appropriate for typesetting.
; 
; Write a `@definfoenclose' command on a line and follow it with three
; arguments separated by commas (commas are used as separators in an
; `@node' line in the same way).  The first argument to
; `@definfoenclose' is the @-command name \(without the `@'\); the
; second argument is the Info start delimiter string; and the third
; argument is the Info end delimiter string.  The latter two arguments
; enclose the highlighted text in the Info file.  A delimiter string
; may contain spaces.  Neither the start nor end delimiter is
; required.  However, if you do not provide a start delimiter, you
; must follow the command name with two commas in a row; otherwise,
; the Info formatting commands will misinterpret the end delimiter
; string as a start delimiter string.
; 
; An enclosure command defined this way takes one argument in braces.
;
; For example, you can write:
;
;     @ifinfo
;     @definfoenclose phoo, //, \\
;     @end ifinfo
;
; near the beginning of a Texinfo file at the beginning of the lines
; to define `@phoo' as an Info formatting command that inserts `//'
; before and `\\' after the argument to `@phoo'.  You can then write
; `@phoo{bar}' wherever you want `//bar\\' highlighted in Info.
;
; Also, for TeX formatting, you could write 
;
;     @iftex
;     @global@let@phoo=@i
;     @end iftex
;
; to define `@phoo' as a command that causes TeX to typeset
; the argument to `@phoo' in italics.
;
; Note that each definition applies to its own formatter: one for TeX,
; the other for texinfo-format-buffer or texinfo-format-region.
;
; Here is another example: write
;
;     @definfoenclose headword, , :
;
; near the beginning of the file, to define `@headword' as an Info
; formatting command that inserts nothing before and a colon after the
; argument to `@headword'.

(put 'definfoenclose 'texinfo-format 'texinfo-define-info-enclosure)
(defun texinfo-define-info-enclosure ()
  (let* ((args (texinfo-format-parse-line-args))
         (command-name (nth 0 args))
         (beginning-delimiter (or (nth 1 args) ""))
         (end-delimiter (or (nth 2 args) "")))
    (texinfo-discard-command)
    (setq texinfo-enclosure-list
        (cons
         (list command-name
               (list
                beginning-delimiter
                end-delimiter))
         texinfo-enclosure-list))))


;;; @var, @code and the like

(put 'var 'texinfo-format 'texinfo-format-var)
;  @sc  a small caps font for TeX; formatted as `var' in Info
(put 'sc 'texinfo-format 'texinfo-format-var)
(defun texinfo-format-var ()
  (insert (upcase (texinfo-parse-arg-discard)))
  (goto-char texinfo-command-start))

; various noops

(put 'b 'texinfo-format 'texinfo-format-noop)
(put 'i 'texinfo-format 'texinfo-format-noop)
(put 'r 'texinfo-format 'texinfo-format-noop)
(put 't 'texinfo-format 'texinfo-format-noop)
(put 'w 'texinfo-format 'texinfo-format-noop)
(put 'asis 'texinfo-format 'texinfo-format-noop)
(put 'dmn 'texinfo-format 'texinfo-format-noop)
(put 'key 'texinfo-format 'texinfo-format-noop)
(put 'math 'texinfo-format 'texinfo-format-noop)
(put 'titlefont 'texinfo-format 'texinfo-format-noop)
(defun texinfo-format-noop ()
  (insert (texinfo-parse-arg-discard))
  (goto-char texinfo-command-start))

(put 'cite 'texinfo-format 'texinfo-format-code)
(put 'code 'texinfo-format 'texinfo-format-code)
(put 'file 'texinfo-format 'texinfo-format-code)
(put 'kbd 'texinfo-format 'texinfo-format-code)
(put 'samp 'texinfo-format 'texinfo-format-code)
(defun texinfo-format-code ()
  (insert "`" (texinfo-parse-arg-discard) "'")
  (goto-char texinfo-command-start))

(put 'emph 'texinfo-format 'texinfo-format-emph)
(put 'strong 'texinfo-format 'texinfo-format-emph)
(defun texinfo-format-emph ()
  (insert "*" (texinfo-parse-arg-discard) "*")
  (goto-char texinfo-command-start))

(put 'dfn 'texinfo-format 'texinfo-format-defn)
(put 'defn 'texinfo-format 'texinfo-format-defn)
(defun texinfo-format-defn ()
  (insert "\"" (texinfo-parse-arg-discard) "\"")
  (goto-char texinfo-command-start))

(put 'bullet 'texinfo-format 'texinfo-format-bullet)
(defun texinfo-format-bullet ()
  "Insert an asterisk.
If used within a line, follow `@bullet' with braces."
  (texinfo-optional-braces-discard)
  (insert "*"))


;;; @example, @lisp, @quotation, @display, @smalllisp, @smallexample

(put 'display 'texinfo-format 'texinfo-format-example)
(put 'example 'texinfo-format 'texinfo-format-example)
(put 'lisp 'texinfo-format 'texinfo-format-example)
(put 'quotation 'texinfo-format 'texinfo-format-example)
(put 'smallexample 'texinfo-format 'texinfo-format-example)
(put 'smalllisp 'texinfo-format 'texinfo-format-example)
(defun texinfo-format-example ()
  (texinfo-push-stack 'example nil)
  (setq fill-column (- fill-column 5))
  (texinfo-discard-line))

(put 'example 'texinfo-end 'texinfo-end-example)
(put 'display 'texinfo-end 'texinfo-end-example)
(put 'lisp 'texinfo-end 'texinfo-end-example)
(put 'quotation 'texinfo-end 'texinfo-end-example)
(put 'smallexample 'texinfo-end 'texinfo-end-example)
(put 'smalllisp 'texinfo-end 'texinfo-end-example)
(defun texinfo-end-example ()
  (setq fill-column (+ fill-column 5))
  (texinfo-discard-command)
  (let ((stacktop
         (texinfo-pop-stack 'example)))
    (texinfo-do-itemize (nth 1 stacktop))))

(put 'exdent 'texinfo-format 'texinfo-format-exdent)
(defun texinfo-format-exdent ()
  (texinfo-discard-command)
  (delete-region (point)
                 (progn
                  (skip-chars-forward " ")
                  (point)))
  (insert ?\b)
  ;; Cancel out the deletion that texinfo-do-itemize
  ;; is going to do at the end of this line.
  (save-excursion
    (end-of-line)
    (insert "\n     ")))


;;; @cartouche 

; The @cartouche command is a noop in Info; in a printed manual,
; it makes a box with rounded corners.

(put 'cartouche 'texinfo-format 'texinfo-discard-line)
(put 'cartouche 'texinfo-end 'texinfo-discard-command)


;;; @flushleft and @format

; The @flushleft command left justifies every line but leaves the
; right end ragged.  As far as Info is concerned, @flushleft is a
; `do-nothing' command

; The @format command is similar to @example except that it does not
; indent; this means that in Info, @format is similar to @flushleft.

(put 'format 'texinfo-format 'texinfo-format-flushleft)
(put 'flushleft 'texinfo-format 'texinfo-format-flushleft)
(defun texinfo-format-flushleft ()
  (texinfo-discard-line))

(put 'format 'texinfo-end 'texinfo-end-flushleft)
(put 'flushleft 'texinfo-end 'texinfo-end-flushleft)
(defun texinfo-end-flushleft ()
  (texinfo-discard-command))


;;; @flushright

; The @flushright command right justifies every line but leaves the
; left end ragged.  Spaces and tabs at the right ends of lines are
; removed so that visible text lines up on the right side.

(put 'flushright 'texinfo-format 'texinfo-format-flushright)
(defun texinfo-format-flushright ()
  (texinfo-push-stack 'flushright nil)
  (texinfo-discard-line))

(put 'flushright 'texinfo-end 'texinfo-end-flushright)
(defun texinfo-end-flushright ()
  (texinfo-discard-command)

  (let ((stacktop
         (texinfo-pop-stack 'flushright)))

    (texinfo-do-flushright (nth 1 stacktop))))

(defun texinfo-do-flushright (from)
  (save-excursion
   (while (progn (forward-line -1)
                 (>= (point) from))

     (beginning-of-line)
     (insert
      (make-string
       (- fill-column
          (save-excursion
            (end-of-line) 
            (skip-chars-backward " \t")
            (delete-region (point) (progn (end-of-line) (point)))
            (current-column)))  
       ? )))))


;;; @ctrl, @TeX, @copyright, @minus, @dots

(put 'ctrl 'texinfo-format 'texinfo-format-ctrl)
(defun texinfo-format-ctrl ()
  (let ((str (texinfo-parse-arg-discard)))
    (insert (logand 31 (aref str 0)))))

(put 'TeX 'texinfo-format 'texinfo-format-TeX)
(defun texinfo-format-TeX ()
  (texinfo-parse-arg-discard)
  (insert "TeX"))

(put 'copyright 'texinfo-format 'texinfo-format-copyright)
(defun texinfo-format-copyright ()
  (texinfo-parse-arg-discard)
  (insert "(C)"))

(put 'minus 'texinfo-format 'texinfo-format-minus)
(defun texinfo-format-minus ()
  "Insert a minus sign.
If used within a line, follow `@minus' with braces."
  (texinfo-optional-braces-discard)
  (insert "-"))

(put 'dots 'texinfo-format 'texinfo-format-dots)
(defun texinfo-format-dots ()
  (texinfo-parse-arg-discard)
  (insert "..."))


;;; Refilling and indenting:  @refill, @paragraphindent, @noindent

;;; Indent only those paragraphs that are refilled as a result of an
;;; @refill command.  

;    * If the value is `asis', do not change the existing indentation at
;      the starts of paragraphs.

;    * If the value zero, delete any existing indentation.

;    * If the value is greater than zero, indent each paragraph by that
;      number of spaces.

;;; But do not refill paragraphs with an @refill command that are
;;; preceded by @noindent or are part of a table, list, or deffn.

(defvar texinfo-paragraph-indent "asis"
  "Number of spaces for @refill to indent a paragraph; else to leave as is.")

(put 'paragraphindent 'texinfo-format 'texinfo-paragraphindent)

(defun texinfo-paragraphindent ()
  "Specify the number of spaces for @refill to indent a paragraph.
Default is to leave the number of spaces as is."
  (let ((arg  (texinfo-parse-arg-discard)))
    (if (string= "asis" arg)
        (setq texinfo-paragraph-indent "asis")
      (setq texinfo-paragraph-indent (string-to-int arg)))))

(put 'refill 'texinfo-format 'texinfo-format-refill)
(defun texinfo-format-refill ()
  "Refill paragraph. Also, indent first line as set by @paragraphindent.
Default is to leave paragraph indentation as is."
  (texinfo-discard-command)
  (forward-paragraph -1)     
  (if (looking-at "[ \t\n]*$") (forward-line 1))
  ;; Do not indent if an entry in a list, table, or deffn,
  ;; or if paragraph is preceded by @noindent.
  ;; Otherwise, indent
  (cond 
   ;; delete a @noindent line and do not indent paragraph
   ((save-excursion (forward-line -1)
                    (looking-at "^@noindent")) 
    (forward-line -1)
    (delete-region (point) (progn (forward-line 1) (point))))
   ;; do nothing if "asis"
   ((equal texinfo-paragraph-indent "asis"))
   ;; do no indenting in list, etc.
   ((> texinfo-stack-depth 0))   
   ;; otherwise delete existing whitespace and indent
   (t 
    (delete-region (point) (progn (skip-chars-forward " \t") (point)))
    (insert (make-string texinfo-paragraph-indent ? ))))
  (forward-paragraph 1) 
  (forward-line -1)
  (end-of-line)
  ;; Do not fill a section title line with asterisks, hyphens, etc. that
  ;; are used to underline it.  This could occur if the line following
  ;; the underlining is not an index entry and has text within it.
  (let* ((previous-paragraph-separate paragraph-separate)
         (paragraph-separate (concat paragraph-separate "\\|^[-=*.]+"))
         (previous-paragraph-start paragraph-start)
         (paragraph-start (concat paragraph-start "\\|^[-=*.]+")))
    (unwind-protect
        (fill-paragraph nil)
      (setq paragraph-separate previous-paragraph-separate)
      (setq paragraph-start previous-paragraph-start))))

(put 'noindent 'texinfo-format 'texinfo-noindent)
(defun texinfo-noindent ()  
  (save-excursion 
    (forward-paragraph 1)
    (if (search-backward "@refill"
                            (save-excursion (forward-line -1) (point)) t)
        () ; leave @noindent command so @refill command knows not to indent
      ;; else
      (texinfo-discard-line))))


;;; Index generation

(put 'vindex 'texinfo-format 'texinfo-format-vindex)
(defun texinfo-format-vindex ()
  (texinfo-index 'texinfo-vindex))

(put 'cindex 'texinfo-format 'texinfo-format-cindex)
(defun texinfo-format-cindex ()
  (texinfo-index 'texinfo-cindex))

(put 'findex 'texinfo-format 'texinfo-format-findex)
(defun texinfo-format-findex ()
  (texinfo-index 'texinfo-findex))

(put 'pindex 'texinfo-format 'texinfo-format-pindex)
(defun texinfo-format-pindex ()
  (texinfo-index 'texinfo-pindex))

(put 'tindex 'texinfo-format 'texinfo-format-tindex)
(defun texinfo-format-tindex ()
  (texinfo-index 'texinfo-tindex))

(put 'kindex 'texinfo-format 'texinfo-format-kindex)
(defun texinfo-format-kindex ()
  (texinfo-index 'texinfo-kindex))

(defun texinfo-index (indexvar)
  (let ((arg (texinfo-parse-expanded-arg)))
    (texinfo-discard-command)
    (set indexvar
         (cons (list arg
                     texinfo-last-node
                     ;; Region formatting may not provide last node position.
                     (if texinfo-last-node-pos
                         (1+ (count-lines texinfo-last-node-pos (point)))
                       1))
               (symbol-value indexvar)))))

(defconst texinfo-indexvar-alist
  '(("cp" . texinfo-cindex)
    ("fn" . texinfo-findex)
    ("vr" . texinfo-vindex)
    ("tp" . texinfo-tindex)
    ("pg" . texinfo-pindex)
    ("ky" . texinfo-kindex)))


;;; @defindex   @defcodeindex
(put 'defindex 'texinfo-format 'texinfo-format-defindex)
(put 'defcodeindex 'texinfo-format 'texinfo-format-defindex)

(defun texinfo-format-defindex ()
  (let* ((index-name (texinfo-parse-arg-discard)) ; eg: `aa'
         (indexing-command (intern (concat index-name "index")))
         (index-formatting-command      ; eg: `texinfo-format-aaindex'
          (intern (concat "texinfo-format-" index-name "index")))
         (index-alist-name              ; eg: `texinfo-aaindex'
          (intern (concat "texinfo-" index-name "index"))))

    (set index-alist-name nil)

    (put indexing-command               ; eg, aaindex
         'texinfo-format
         index-formatting-command)      ; eg, texinfo-format-aaindex

    ;; eg: "aa" . texinfo-aaindex
    (or (assoc index-name texinfo-indexvar-alist)
        (setq texinfo-indexvar-alist
              (cons
               (cons index-name
                     index-alist-name)
               texinfo-indexvar-alist)))

    (fset index-formatting-command
          (list 'lambda 'nil
                (list 'texinfo-index 
                      (list 'quote index-alist-name))))))


;;; @synindex   @syncodeindex

(put 'synindex 'texinfo-format 'texinfo-format-synindex)
(put 'syncodeindex 'texinfo-format 'texinfo-format-synindex)

(defun texinfo-format-synindex ()
  (let* ((args (texinfo-parse-arg-discard))
         (second (cdr (read-from-string args)))
         (joiner (symbol-name (car (read-from-string args))))
         (joined (symbol-name (car (read-from-string args second)))))

    (if (assoc joiner texinfo-short-index-cmds-alist)
        (put
          (cdr (assoc joiner texinfo-short-index-cmds-alist))
         'texinfo-format
         (or (cdr (assoc joined texinfo-short-index-format-cmds-alist))
             (intern (concat "texinfo-format-" joined "index"))))
      (put
       (intern (concat joiner "index"))
       'texinfo-format
       (or (cdr(assoc joined texinfo-short-index-format-cmds-alist))
           (intern (concat "texinfo-format-" joined "index")))))))

(defconst texinfo-short-index-cmds-alist
  '(("cp" . cindex)
    ("fn" . findex)
    ("vr" . vindex)
    ("tp" . tindex)
    ("pg" . pindex)
    ("ky" . kindex)))

(defconst texinfo-short-index-format-cmds-alist
  '(("cp" . texinfo-format-cindex)
    ("fn" . texinfo-format-findex)
    ("vr" . texinfo-format-vindex)
    ("tp" . texinfo-format-tindex)
    ("pg" . texinfo-format-pindex)
    ("ky" . texinfo-format-kindex)))


;;; Sort and index (for VMS)

;; Sort an index which is in the current buffer between START and END.
;; Used on VMS, where the `sort' utility is not available.
(defun texinfo-sort-region (start end)
  (require 'sort)
  (save-restriction
    (narrow-to-region start end)
    (sort-subr nil 'forward-line 'end-of-line 'texinfo-sort-startkeyfun)))

;; Subroutine for sorting an index.
;; At start of a line, return a string to sort the line under.
(defun texinfo-sort-startkeyfun ()
  (let ((line
         (buffer-substring (point) (save-excursion (end-of-line) (point)))))
    ;; Canonicalize whitespace and eliminate funny chars.
    (while (string-match "[ \t][ \t]+\\|[^a-z0-9 ]+" line)
      (setq line (concat (substring line 0 (match-beginning 0))
                         " "
                         (substring line (match-end 0) (length line)))))
    line))


;;; @printindex

(put 'printindex 'texinfo-format 'texinfo-format-printindex)

(defun texinfo-format-printindex ()
  (let ((indexelts (symbol-value
                    (cdr (assoc (texinfo-parse-arg-discard)
                                texinfo-indexvar-alist))))
        opoint)
    (insert "\n* Menu:\n\n")
    (setq opoint (point))
    (texinfo-print-index nil indexelts)

    (if (eq system-type 'vax-vms)
        (texinfo-sort-region opoint (point))
      (shell-command-on-region opoint (point) "sort -fd" 1))))

(defun texinfo-print-index (file indexelts)
  (while indexelts
    (if (stringp (car (car indexelts)))
        (progn
          (insert "* " (car (car indexelts)) ": " )
          (indent-to 32)
          (insert
           (if file (concat "(" file ")") "")
           (nth 1 (car indexelts)) ".")
          (indent-to 54)
          (insert
           (if (nth 2 (car indexelts))
               (format "  %d." (nth 2 (car indexelts)))
             "")
           "\n"))
      ;; index entries from @include'd file
      (texinfo-print-index (nth 1 (car indexelts))
                           (nth 2 (car indexelts))))
    (setq indexelts (cdr indexelts))))


;;; Glyphs: @equiv, @error, etc

;; @equiv           to show that two expressions are equivalent
;; @error           to show an error message
;; @expansion       to show what a macro expands to
;; @point           to show the location of point in an example
;; @print           to show what an evaluated expression prints
;; @result          to indicate the value returned by an expression

(put 'equiv 'texinfo-format 'texinfo-format-equiv)
(defun texinfo-format-equiv ()
  (texinfo-parse-arg-discard)
  (insert "=="))

(put 'error 'texinfo-format 'texinfo-format-error)
(defun texinfo-format-error ()
  (texinfo-parse-arg-discard)
  (insert "error-->"))

(put 'expansion 'texinfo-format 'texinfo-format-expansion)
(defun texinfo-format-expansion ()
  (texinfo-parse-arg-discard)
  (insert "==>"))

(put 'point 'texinfo-format 'texinfo-format-point)
(defun texinfo-format-point ()
  (texinfo-parse-arg-discard)
  (insert "-!-"))

(put 'print 'texinfo-format 'texinfo-format-print)
(defun texinfo-format-print ()
  (texinfo-parse-arg-discard)
  (insert "-|"))

(put 'result 'texinfo-format 'texinfo-format-result)
(defun texinfo-format-result ()
  (texinfo-parse-arg-discard)
  (insert "=>"))


;;; Definition formatting: @deffn, @defun, etc

;; What definition formatting produces:
;;
;; @deffn category name args...
;;     In Info, `Category: name ARGS'
;;     In index: name:  node. line#.
;;
;; @defvr category name 
;;     In Info, `Category: name'
;;     In index: name:  node. line#.
;;
;; @deftp category name attributes...
;; `category name attributes...'       Note: @deftp args in lower case.
;;     In index: name:  node. line#.
;;
;; Specialized function-like or variable-like entity:
;;
;; @defun, @defmac, @defspec, @defvar, @defopt
;;
;; @defun name args           In Info, `Function: name ARGS'
;; @defmac name args          In Info, `Macro: name ARGS'
;; @defvar name               In Info, `Variable: name'
;; etc.
;;     In index: name:  node. line#.
;;
;; Generalized typed-function-like or typed-variable-like entity:
;; @deftypefn category data-type name args...
;;     In Info, `Category:  data-type name args...'
;; @deftypevr category data-type name 
;;     In Info, `Category:  data-type name'
;;     In index: name:  node. line#.
;;
;; Specialized typed-function-like or typed-variable-like entity:
;; @deftypefun data-type name args...
;;     In Info, `Function:  data-type name ARGS'
;;     In index: name:  node. line#.   
;;
;; @deftypevar data-type name 
;;     In Info, `Variable:  data-type name'
;;     In index: name:  node. line#.   but include args after name!?
;;
;; Generalized object oriented entity: 
;; @defop category class name args...
;;     In Info, `Category on class: name ARG'
;;     In index: name on class: node. line#.
;;
;; @defcv category class name         
;;     In Info, `Category of class: name'
;;     In index: name of class: node. line#.
;;
;; Specialized object oriented entity:
;; @defmethod class name args... 
;;     In Info, `Method on class: name ARGS'
;;     In index: name on class: node. line#.
;;
;; @defivar class name
;;     In Info, `Instance variable of class: name'
;;     In index: name of class: node. line#.


;;; The definition formatting functions

(defun texinfo-format-defun ()
  (texinfo-push-stack 'defun nil)
  (setq fill-column (- fill-column 5))
  (texinfo-format-defun-1 t))

(defun texinfo-end-defun ()
  (setq fill-column (+ fill-column 5))
  (texinfo-discard-command)
  (let ((start (nth 1 (texinfo-pop-stack 'defun))))
    (texinfo-do-itemize start)
    ;; Delete extra newline inserted after header.
    (save-excursion
      (goto-char start)
      (delete-char -1))))

(defun texinfo-format-defunx ()
  (texinfo-format-defun-1 nil))

(defun texinfo-format-defun-1 (first-p)
  (let ((parse-args (texinfo-format-parse-defun-args))
        (texinfo-defun-type (get texinfo-command-name 'texinfo-defun-type)))
    (texinfo-discard-command)
    ;; Delete extra newline inserted after previous header line.
    (if (not first-p)
        (delete-char -1))
    (funcall
     (get texinfo-command-name 'texinfo-deffn-formatting-property) parse-args)
    ;; Insert extra newline so that paragraph filling does not mess
    ;; with header line.
    (insert "\n\n")
    (rplaca (cdr (cdr (car texinfo-stack))) (point))
    (funcall
     (get texinfo-command-name 'texinfo-defun-indexing-property) parse-args)))

;;; Formatting the first line of a definition

;; @deffn, @defvr, @deftp
(put 'deffn 'texinfo-deffn-formatting-property 'texinfo-format-deffn)
(put 'deffnx 'texinfo-deffn-formatting-property 'texinfo-format-deffn)
(put 'defvr 'texinfo-deffn-formatting-property 'texinfo-format-deffn)
(put 'defvrx 'texinfo-deffn-formatting-property 'texinfo-format-deffn)
(put 'deftp 'texinfo-deffn-formatting-property 'texinfo-format-deffn)
(put 'deftpx 'texinfo-deffn-formatting-property 'texinfo-format-deffn)
(defun texinfo-format-deffn (parsed-args)
  ;; Generalized function-like, variable-like, or generic data-type entity:
  ;; @deffn category name args...
  ;;     In Info, `Category: name ARGS'
  ;; @deftp category name attributes...
  ;; `category name attributes...'       Note: @deftp args in lower case.
  (let ((category (car parsed-args))
        (name (car (cdr parsed-args)))
        (args (cdr (cdr parsed-args))))
    (insert " -- " category ": " name)
    (while args
      (insert " "
              (if (or (= ?& (aref (car args) 0))
                      (eq (eval (car texinfo-defun-type)) 'deftp-type))
                  (car args)
                (upcase (car args))))
      (setq args (cdr args)))))

;; @defun, @defmac, @defspec, @defvar, @defopt: Specialized, simple
(put 'defun 'texinfo-deffn-formatting-property
     'texinfo-format-specialized-defun)
(put 'defunx 'texinfo-deffn-formatting-property
     'texinfo-format-specialized-defun)
(put 'defmac 'texinfo-deffn-formatting-property
     'texinfo-format-specialized-defun)
(put 'defmacx 'texinfo-deffn-formatting-property
     'texinfo-format-specialized-defun)
(put 'defspec 'texinfo-deffn-formatting-property
     'texinfo-format-specialized-defun)
(put 'defspecx 'texinfo-deffn-formatting-property
     'texinfo-format-specialized-defun)
(put 'defvar 'texinfo-deffn-formatting-property
     'texinfo-format-specialized-defun)
(put 'defvarx 'texinfo-deffn-formatting-property
     'texinfo-format-specialized-defun)
(put 'defopt 'texinfo-deffn-formatting-property
     'texinfo-format-specialized-defun)
(put 'defoptx 'texinfo-deffn-formatting-property
     'texinfo-format-specialized-defun)
(defun texinfo-format-specialized-defun (parsed-args)
  ;; Specialized function-like or variable-like entity:
  ;; @defun name args           In Info, `Function: Name ARGS'
  ;; @defmac name args          In Info, `Macro: Name ARGS'
  ;; @defvar name               In Info, `Variable: Name'
  ;; Use cdr of texinfo-defun-type to determine category:
  (let ((category (car (cdr texinfo-defun-type)))
        (name (car parsed-args))
        (args (cdr parsed-args)))
    (insert " -- " category ": " name)
    (while args
      (insert " "
              (if (= ?& (aref (car args) 0))
                  (car args)
                (upcase (car args))))
      (setq args (cdr args)))))

;; @deftypefn, @deftypevr: Generalized typed
(put 'deftypefn 'texinfo-deffn-formatting-property 'texinfo-format-deftypefn)
(put 'deftypefnx 'texinfo-deffn-formatting-property 'texinfo-format-deftypefn)
(put 'deftypevr 'texinfo-deffn-formatting-property 'texinfo-format-deftypefn)
(put 'deftypevrx 'texinfo-deffn-formatting-property 'texinfo-format-deftypefn)
(defun texinfo-format-deftypefn (parsed-args)
  ;; Generalized typed-function-like or typed-variable-like entity:
  ;; @deftypefn category data-type name args...
  ;;     In Info, `Category:  data-type name args...'
  ;; @deftypevr category data-type name 
  ;;     In Info, `Category:  data-type name'
  ;; Note: args in lower case, unless modified in command line.
  (let ((category (car parsed-args))
        (data-type (car (cdr parsed-args)))
        (name (car (cdr (cdr parsed-args))))
        (args (cdr (cdr (cdr parsed-args)))))
    (insert " -- " category ": " data-type " " name)
    (while args
      (insert " " (car args))
      (setq args (cdr args)))))

;; @deftypefun, @deftypevar: Specialized typed
(put 'deftypefun 'texinfo-deffn-formatting-property 'texinfo-format-deftypefun)
(put 'deftypefunx 'texinfo-deffn-formatting-property
     'texinfo-format-deftypefun)
(put 'deftypevar 'texinfo-deffn-formatting-property 'texinfo-format-deftypefun)
(put 'deftypevarx 'texinfo-deffn-formatting-property
     'texinfo-format-deftypefun)
(defun texinfo-format-deftypefun (parsed-args)
  ;; Specialized typed-function-like or typed-variable-like entity:
  ;; @deftypefun data-type name args...
  ;;     In Info, `Function:  data-type name ARGS'
  ;; @deftypevar data-type name 
  ;;     In Info, `Variable:  data-type name'
  ;; Note: args in lower case, unless modified in command line.
  ;; Use cdr of texinfo-defun-type to determine category:
  (let ((category (car (cdr texinfo-defun-type)))
        (data-type (car parsed-args))
        (name (car (cdr  parsed-args)))
        (args (cdr (cdr parsed-args))))
    (insert " -- " category ": " data-type " " name)
    (while args
      (insert " " (car args))
      (setq args (cdr args)))))

;; @defop: Generalized object-oriented
(put 'defop 'texinfo-deffn-formatting-property 'texinfo-format-defop)
(put 'defopx 'texinfo-deffn-formatting-property 'texinfo-format-defop)
(defun texinfo-format-defop (parsed-args)
  ;; Generalized object oriented entity: 
  ;; @defop category class name args...
  ;;     In Info, `Category on class: name ARG'
  ;; Note: args in upper case; use of `on'
  (let ((category (car parsed-args))
        (class (car (cdr parsed-args)))
        (name (car (cdr (cdr parsed-args))))
        (args (cdr (cdr (cdr parsed-args)))))
    (insert " -- " category " on " class ": " name)
    (while args
      (insert " " (upcase (car args)))
      (setq args (cdr args)))))

;; @defcv: Generalized object-oriented
(put 'defcv 'texinfo-deffn-formatting-property 'texinfo-format-defcv)
(put 'defcvx 'texinfo-deffn-formatting-property 'texinfo-format-defcv)
(defun texinfo-format-defcv (parsed-args)
  ;; Generalized object oriented entity: 
  ;; @defcv category class name         
  ;;     In Info, `Category of class: name'
  ;; Note: args in upper case; use of `of'
  (let ((category (car parsed-args))
        (class (car (cdr parsed-args)))
        (name (car (cdr (cdr parsed-args))))
        (args (cdr (cdr (cdr parsed-args)))))
    (insert " -- " category " of " class ": " name)
    (while args
      (insert " " (upcase (car args)))
      (setq args (cdr args)))))

;; @defmethod: Specialized object-oriented
(put 'defmethod 'texinfo-deffn-formatting-property 'texinfo-format-defmethod)
(put 'defmethodx 'texinfo-deffn-formatting-property 'texinfo-format-defmethod)
(defun texinfo-format-defmethod (parsed-args)
  ;; Specialized object oriented entity:
  ;; @defmethod class name args... 
  ;;     In Info, `Method on class: name ARGS'
  ;; Note: args in upper case; use of `on'
  ;; Use cdr of texinfo-defun-type to determine category:
  (let ((category (car (cdr texinfo-defun-type)))
        (class (car parsed-args))
        (name (car (cdr  parsed-args)))
        (args (cdr  (cdr parsed-args))))
    (insert " -- " category " on " class ": " name)
    (while args
      (insert " " (upcase (car args)))
      (setq args (cdr args)))))

;; @defivar: Specialized object-oriented
(put 'defivar 'texinfo-deffn-formatting-property 'texinfo-format-defivar)
(put 'defivarx 'texinfo-deffn-formatting-property 'texinfo-format-defivar)
(defun texinfo-format-defivar (parsed-args)
  ;; Specialized object oriented entity:
  ;; @defivar class name
  ;;     In Info, `Instance variable of class: name'
  ;; Note: args in upper case; use of `of'
  ;; Use cdr of texinfo-defun-type to determine category:
  (let ((category (car (cdr texinfo-defun-type)))
        (class (car parsed-args))
        (name (car (cdr  parsed-args)))
        (args (cdr  (cdr parsed-args))))
    (insert " -- " category " of " class ": " name)
    (while args
      (insert " " (upcase (car args)))
      (setq args (cdr args)))))


;;; Indexing for definitions

;; An index entry has three parts: the `entry proper', the node name, and the
;; line number.  Depending on the which command is used, the entry is
;; formatted differently:
;;
;; @defun, 
;; @defmac, 
;; @defspec, 
;; @defvar, 
;; @defopt          all use their 1st argument as the entry-proper 
;;
;; @deffn, 
;; @defvr, 
;; @deftp 
;; @deftypefun
;; @deftypevar      all use their 2nd argument as the entry-proper
;;
;; @deftypefn, 
;; @deftypevr       both use their 3rd argument as the entry-proper  
;;
;; @defmethod       uses its 2nd and 1st arguments as an entry-proper 
;;                    formatted: NAME on CLASS

;; @defop           uses its 3rd and 2nd arguments as an entry-proper 
;;                    formatted: NAME on CLASS
;;        
;; @defivar         uses its 2nd and 1st arguments as an entry-proper
;;                    formatted: NAME of CLASS
;;
;; @defcv           uses its 3rd and 2nd argument as an entry-proper
;;                    formatted: NAME of CLASS

(put 'defun 'texinfo-defun-indexing-property 'texinfo-index-defun)
(put 'defunx 'texinfo-defun-indexing-property 'texinfo-index-defun)
(put 'defmac 'texinfo-defun-indexing-property 'texinfo-index-defun)
(put 'defmacx 'texinfo-defun-indexing-property 'texinfo-index-defun)
(put 'defspec 'texinfo-defun-indexing-property 'texinfo-index-defun)
(put 'defspecx 'texinfo-defun-indexing-property 'texinfo-index-defun)
(put 'defvar 'texinfo-defun-indexing-property 'texinfo-index-defun)
(put 'defvarx 'texinfo-defun-indexing-property 'texinfo-index-defun)
(put 'defopt  'texinfo-defun-indexing-property 'texinfo-index-defun)
(put 'defoptx  'texinfo-defun-indexing-property 'texinfo-index-defun)
(defun texinfo-index-defun (parsed-args)
  ;; use 1st parsed-arg  as entry-proper
  ;; `index-list' will be texinfo-findex or the like
  (let ((index-list (get texinfo-command-name 'texinfo-defun-index)))
    (set index-list
         (cons 
          ;; Three elements: entry-proper, node-name, line-number
          (list
           (car parsed-args)
           texinfo-last-node
           ;; Region formatting may not provide last node position.
           (if texinfo-last-node-pos
               (1+ (count-lines texinfo-last-node-pos (point)))
             1))
          (symbol-value index-list)))))

(put 'deffn 'texinfo-defun-indexing-property 'texinfo-index-deffn)
(put 'deffnx 'texinfo-defun-indexing-property 'texinfo-index-deffn)
(put 'defvr 'texinfo-defun-indexing-property 'texinfo-index-deffn)
(put 'defvrx 'texinfo-defun-indexing-property 'texinfo-index-deffn)
(put 'deftp 'texinfo-defun-indexing-property 'texinfo-index-deffn)
(put 'deftpx 'texinfo-defun-indexing-property 'texinfo-index-deffn)
(put 'deftypefun 'texinfo-defun-indexing-property 'texinfo-index-deffn)
(put 'deftypefunx 'texinfo-defun-indexing-property 'texinfo-index-deffn)
(put 'deftypevar 'texinfo-defun-indexing-property 'texinfo-index-deffn)
(put 'deftypevarx 'texinfo-defun-indexing-property 'texinfo-index-deffn)
(defun texinfo-index-deffn (parsed-args) 
 ;; use 2nd parsed-arg  as entry-proper
  ;; `index-list' will be texinfo-findex or the like
  (let ((index-list (get texinfo-command-name 'texinfo-defun-index)))
    (set index-list
         (cons 
          ;; Three elements: entry-proper, node-name, line-number
          (list
           (car (cdr parsed-args))
           texinfo-last-node
           ;; Region formatting may not provide last node position.
           (if texinfo-last-node-pos
               (1+ (count-lines texinfo-last-node-pos (point)))
             1))
          (symbol-value index-list)))))

(put 'deftypefn 'texinfo-defun-indexing-property 'texinfo-index-deftypefn)
(put 'deftypefnx 'texinfo-defun-indexing-property 'texinfo-index-deftypefn)
(put 'deftypevr 'texinfo-defun-indexing-property 'texinfo-index-deftypefn)
(put 'deftypevrx 'texinfo-defun-indexing-property 'texinfo-index-deftypefn)
(defun texinfo-index-deftypefn (parsed-args)
  ;; use 3rd parsed-arg  as entry-proper
  ;; `index-list' will be texinfo-findex or the like
  (let ((index-list (get texinfo-command-name 'texinfo-defun-index)))
    (set index-list
         (cons 
          ;; Three elements: entry-proper, node-name, line-number
          (list
           (car (cdr (cdr parsed-args)))
           texinfo-last-node
           ;; Region formatting may not provide last node position.
           (if texinfo-last-node-pos
               (1+ (count-lines texinfo-last-node-pos (point)))
             1))
          (symbol-value index-list)))))

(put 'defmethod 'texinfo-defun-indexing-property 'texinfo-index-defmethod)
(put 'defmethodx 'texinfo-defun-indexing-property 'texinfo-index-defmethod)
(defun texinfo-index-defmethod (parsed-args)
  ;; use 2nd on 1st parsed-arg  as entry-proper
  ;; `index-list' will be texinfo-findex or the like
  (let ((index-list (get texinfo-command-name 'texinfo-defun-index)))
    (set index-list
         (cons 
          ;; Three elements: entry-proper, node-name, line-number
          (list
           (format "%s on %s"            
                   (car (cdr parsed-args))
                   (car parsed-args))
           texinfo-last-node
           ;; Region formatting may not provide last node position.
           (if texinfo-last-node-pos
               (1+ (count-lines texinfo-last-node-pos (point)))
             1))
          (symbol-value index-list)))))

(put 'defop 'texinfo-defun-indexing-property 'texinfo-index-defop)
(put 'defopx 'texinfo-defun-indexing-property 'texinfo-index-defop)
(defun texinfo-index-defop (parsed-args)
  ;; use 3rd on 2nd parsed-arg  as entry-proper
  ;; `index-list' will be texinfo-findex or the like
  (let ((index-list (get texinfo-command-name 'texinfo-defun-index)))
    (set index-list
         (cons 
          ;; Three elements: entry-proper, node-name, line-number
          (list
           (format "%s on %s"            
                   (car (cdr (cdr parsed-args)))
                   (car (cdr parsed-args)))
           texinfo-last-node
           ;; Region formatting may not provide last node position.
           (if texinfo-last-node-pos
               (1+ (count-lines texinfo-last-node-pos (point)))
             1))
          (symbol-value index-list)))))

(put 'defivar 'texinfo-defun-indexing-property 'texinfo-index-defivar)
(put 'defivarx 'texinfo-defun-indexing-property 'texinfo-index-defivar)
(defun texinfo-index-defivar (parsed-args)
  ;; use 2nd of 1st parsed-arg  as entry-proper
  ;; `index-list' will be texinfo-findex or the like
  (let ((index-list (get texinfo-command-name 'texinfo-defun-index)))
    (set index-list
         (cons 
          ;; Three elements: entry-proper, node-name, line-number
          (list
           (format "%s of %s"            
                   (car (cdr parsed-args))
                   (car parsed-args))
           texinfo-last-node
           ;; Region formatting may not provide last node position.
           (if texinfo-last-node-pos
               (1+ (count-lines texinfo-last-node-pos (point)))
             1))
          (symbol-value index-list)))))

(put 'defcv 'texinfo-defun-indexing-property 'texinfo-index-defcv)
(put 'defcvx 'texinfo-defun-indexing-property 'texinfo-index-defcv)
(defun texinfo-index-defcv (parsed-args)
  ;; use 3rd of 2nd parsed-arg  as entry-proper
  ;; `index-list' will be texinfo-findex or the like
  (let ((index-list (get texinfo-command-name 'texinfo-defun-index)))
    (set index-list
         (cons 
          ;; Three elements: entry-proper, node-name, line-number
          (list
           (format "%s of %s"            
                   (car (cdr (cdr parsed-args)))
                   (car (cdr parsed-args)))
           texinfo-last-node
           ;; Region formatting may not provide last node position.
           (if texinfo-last-node-pos
               (1+ (count-lines texinfo-last-node-pos (point)))
             1))
          (symbol-value index-list)))))


;;; Properties for definitions

;; Each definition command has six properties:
;;
;; 1. texinfo-deffn-formatting-property      to format definition line
;; 2. texinfo-defun-indexing-property        to create index entry
;; 3. texinfo-format                         formatting command
;; 4. texinfo-end                            end formatting command
;; 5. texinfo-defun-type                     type of deffn to format
;; 6. texinfo-defun-index                    type of index to use
;;
;; The `x' forms of each definition command are used for the second
;; and subsequent header lines.

;; The texinfo-deffn-formatting-property and texinfo-defun-indexing-property
;; are listed just before the appropriate formatting and indexing commands.

(put 'deffn 'texinfo-format 'texinfo-format-defun)
(put 'deffnx 'texinfo-format 'texinfo-format-defunx)
(put 'deffn 'texinfo-end 'texinfo-end-defun)
(put 'deffn 'texinfo-defun-type '('deffn-type nil))
(put 'deffnx 'texinfo-defun-type '('deffn-type nil))
(put 'deffn 'texinfo-defun-index 'texinfo-findex)
(put 'deffnx 'texinfo-defun-index 'texinfo-findex)

(put 'defun 'texinfo-format 'texinfo-format-defun)
(put 'defunx 'texinfo-format 'texinfo-format-defunx)
(put 'defun 'texinfo-end 'texinfo-end-defun)
(put 'defun 'texinfo-defun-type '('defun-type "Function"))
(put 'defunx 'texinfo-defun-type '('defun-type "Function"))
(put 'defun 'texinfo-defun-index 'texinfo-findex)
(put 'defunx 'texinfo-defun-index 'texinfo-findex)

(put 'defmac 'texinfo-format 'texinfo-format-defun)
(put 'defmacx 'texinfo-format 'texinfo-format-defunx)
(put 'defmac 'texinfo-end 'texinfo-end-defun)
(put 'defmac 'texinfo-defun-type '('defun-type "Macro"))
(put 'defmacx 'texinfo-defun-type '('defun-type "Macro"))
(put 'defmac 'texinfo-defun-index 'texinfo-findex)
(put 'defmacx 'texinfo-defun-index 'texinfo-findex)

(put 'defspec 'texinfo-format 'texinfo-format-defun)
(put 'defspecx 'texinfo-format 'texinfo-format-defunx)
(put 'defspec 'texinfo-end 'texinfo-end-defun)
(put 'defspec 'texinfo-defun-type '('defun-type "Special form"))
(put 'defspecx 'texinfo-defun-type '('defun-type "Special form"))
(put 'defspec 'texinfo-defun-index 'texinfo-findex)
(put 'defspecx 'texinfo-defun-index 'texinfo-findex)

(put 'defvr 'texinfo-format 'texinfo-format-defun)
(put 'defvrx 'texinfo-format 'texinfo-format-defunx)
(put 'defvr 'texinfo-end 'texinfo-end-defun)
(put 'defvr 'texinfo-defun-type '('deffn-type nil))
(put 'defvrx 'texinfo-defun-type '('deffn-type nil))
(put 'defvr 'texinfo-defun-index 'texinfo-vindex)
(put 'defvrx 'texinfo-defun-index 'texinfo-vindex)

(put 'defvar 'texinfo-format 'texinfo-format-defun)
(put 'defvarx 'texinfo-format 'texinfo-format-defunx)
(put 'defvar 'texinfo-end 'texinfo-end-defun)
(put 'defvar 'texinfo-defun-type '('defun-type "Variable"))
(put 'defvarx 'texinfo-defun-type '('defun-type "Variable"))
(put 'defvar 'texinfo-defun-index 'texinfo-vindex)
(put 'defvarx 'texinfo-defun-index 'texinfo-vindex)

(put 'defconst 'texinfo-format 'texinfo-format-defun)
(put 'defconstx 'texinfo-format 'texinfo-format-defunx)
(put 'defconst 'texinfo-end 'texinfo-end-defun)
(put 'defconst 'texinfo-defun-type '('defun-type "Constant"))
(put 'defconstx 'texinfo-defun-type '('defun-type "Constant"))
(put 'defconst 'texinfo-defun-index 'texinfo-vindex)
(put 'defconstx 'texinfo-defun-index 'texinfo-vindex)

(put 'defcmd 'texinfo-format 'texinfo-format-defun)
(put 'defcmdx 'texinfo-format 'texinfo-format-defunx)
(put 'defcmd 'texinfo-end 'texinfo-end-defun)
(put 'defcmd 'texinfo-defun-type '('defun-type "Command"))
(put 'defcmdx 'texinfo-defun-type '('defun-type "Command"))
(put 'defcmd 'texinfo-defun-index 'texinfo-findex)
(put 'defcmdx 'texinfo-defun-index 'texinfo-findex)

(put 'defopt 'texinfo-format 'texinfo-format-defun)
(put 'defoptx 'texinfo-format 'texinfo-format-defunx)
(put 'defopt 'texinfo-end 'texinfo-end-defun)
(put 'defopt 'texinfo-defun-type '('defun-type "User Option"))
(put 'defoptx 'texinfo-defun-type '('defun-type "User Option"))
(put 'defopt 'texinfo-defun-index 'texinfo-vindex)
(put 'defoptx 'texinfo-defun-index 'texinfo-vindex)

(put 'deftp 'texinfo-format 'texinfo-format-defun)
(put 'deftpx 'texinfo-format 'texinfo-format-defunx)
(put 'deftp 'texinfo-end 'texinfo-end-defun)
(put 'deftp 'texinfo-defun-type '('deftp-type nil))
(put 'deftpx 'texinfo-defun-type '('deftp-type nil))
(put 'deftp 'texinfo-defun-index 'texinfo-tindex)
(put 'deftpx 'texinfo-defun-index 'texinfo-tindex)

;;; Object-oriented stuff is a little hairier.

(put 'defop 'texinfo-format 'texinfo-format-defun)
(put 'defopx 'texinfo-format 'texinfo-format-defunx)
(put 'defop 'texinfo-end 'texinfo-end-defun)
(put 'defop 'texinfo-defun-type '('defop-type nil))
(put 'defopx 'texinfo-defun-type '('defop-type nil))
(put 'defop 'texinfo-defun-index 'texinfo-findex)
(put 'defopx 'texinfo-defun-index 'texinfo-findex)

(put 'defmethod 'texinfo-format 'texinfo-format-defun)
(put 'defmethodx 'texinfo-format 'texinfo-format-defunx)
(put 'defmethod 'texinfo-end 'texinfo-end-defun)
(put 'defmethod 'texinfo-defun-type '('defmethod-type "Method"))
(put 'defmethodx 'texinfo-defun-type '('defmethod-type "Method"))
(put 'defmethod 'texinfo-defun-index 'texinfo-findex)
(put 'defmethodx 'texinfo-defun-index 'texinfo-findex)

(put 'defcv 'texinfo-format 'texinfo-format-defun)
(put 'defcvx 'texinfo-format 'texinfo-format-defunx)
(put 'defcv 'texinfo-end 'texinfo-end-defun)
(put 'defcv 'texinfo-defun-type '('defop-type nil))
(put 'defcvx 'texinfo-defun-type '('defop-type nil))
(put 'defcv 'texinfo-defun-index 'texinfo-vindex)
(put 'defcvx 'texinfo-defun-index 'texinfo-vindex)

(put 'defivar 'texinfo-format 'texinfo-format-defun)
(put 'defivarx 'texinfo-format 'texinfo-format-defunx)
(put 'defivar 'texinfo-end 'texinfo-end-defun)
(put 'defivar 'texinfo-defun-type '('defmethod-type "Instance variable"))
(put 'defivarx 'texinfo-defun-type '('defmethod-type "Instance variable"))
(put 'defivar 'texinfo-defun-index 'texinfo-vindex)
(put 'defivarx 'texinfo-defun-index 'texinfo-vindex)

;;; Typed functions and variables

(put 'deftypefn 'texinfo-format 'texinfo-format-defun)
(put 'deftypefnx 'texinfo-format 'texinfo-format-defunx)
(put 'deftypefn 'texinfo-end 'texinfo-end-defun)
(put 'deftypefn 'texinfo-defun-type '('deftypefn-type nil))
(put 'deftypefnx 'texinfo-defun-type '('deftypefn-type nil))
(put 'deftypefn 'texinfo-defun-index 'texinfo-findex)
(put 'deftypefnx 'texinfo-defun-index 'texinfo-findex)

(put 'deftypefun 'texinfo-format 'texinfo-format-defun)
(put 'deftypefunx 'texinfo-format 'texinfo-format-defunx)
(put 'deftypefun 'texinfo-end 'texinfo-end-defun)
(put 'deftypefun 'texinfo-defun-type '('deftypefun-type "Function"))
(put 'deftypefunx 'texinfo-defun-type '('deftypefun-type "Function"))
(put 'deftypefun 'texinfo-defun-index 'texinfo-findex)
(put 'deftypefunx 'texinfo-defun-index 'texinfo-findex)

(put 'deftypevr 'texinfo-format 'texinfo-format-defun)
(put 'deftypevrx 'texinfo-format 'texinfo-format-defunx)
(put 'deftypevr 'texinfo-end 'texinfo-end-defun)
(put 'deftypevr 'texinfo-defun-type '('deftypefn-type nil))
(put 'deftypevrx 'texinfo-defun-type '('deftypefn-type nil))
(put 'deftypevr 'texinfo-defun-index 'texinfo-vindex)
(put 'deftypevrx 'texinfo-defun-index 'texinfo-vindex)

(put 'deftypevar 'texinfo-format 'texinfo-format-defun)
(put 'deftypevarx 'texinfo-format 'texinfo-format-defunx)
(put 'deftypevar 'texinfo-end 'texinfo-end-defun)
(put 'deftypevar 'texinfo-defun-type '('deftypevar-type "Variable"))
(put 'deftypevarx 'texinfo-defun-type '('deftypevar-type "Variable"))
(put 'deftypevar 'texinfo-defun-index 'texinfo-vindex)
(put 'deftypevarx 'texinfo-defun-index 'texinfo-vindex)


;;; @set, @clear, @ifset, @ifclear

;; If a flag is set with @set FLAG, then text between @ifset and @end
;; ifset is formatted normally, but if the flag is is cleared with
;; @clear FLAG, then the text is not formatted; it is ignored.

;; If a flag is cleared with @clear FLAG, then text between @ifclear
;; and @end ifclear is formatted normally, but if the flag is is set with
;; @set FLAG, then the text is not formatted; it is ignored.  @ifclear
;; is the opposite of @ifset.

;; If a flag is set to a string with @set FLAG, 
;; replace  @value{FLAG} with the string.
;; If a flag with a value is cleared, 
;; @value{FLAG} is invalid, 
;; as if there had never been any @set FLAG previously.

(put 'clear 'texinfo-format 'texinfo-clear)
(defun texinfo-clear ()
  "Clear the value of the flag."
  (let* ((arg (texinfo-parse-arg-discard))
         (flag (car (read-from-string arg)))
         (value (substring arg (cdr (read-from-string arg)))))
    (put flag 'texinfo-whether-setp 'flag-cleared)
    (put flag 'texinfo-set-value "")))

(put 'set 'texinfo-format 'texinfo-set)
(defun texinfo-set ()
  "Set the value of the flag, optionally to a string.
The command  `@set foo This is a string.'
sets flag foo to the value: `This is a string.'
The command  `@value{foo}'  expands to the value."
  (let* ((arg (texinfo-parse-arg-discard))
         (flag (car (read-from-string arg)))
         (value (substring arg (cdr (read-from-string arg)))))
    (put flag 'texinfo-whether-setp 'flag-set)
    (put flag 'texinfo-set-value value)))

(put 'value 'texinfo-format 'texinfo-value)
(defun texinfo-value ()
  "Insert the string to which the flag is set.
The command  `@set foo This is a string.'
sets flag foo to the value: `This is a string.'
The command  `@value{foo}'  expands to the value."
  (let ((arg (texinfo-parse-arg-discard)))
    (cond ((and
            (eq (get (car (read-from-string arg)) 'texinfo-whether-setp)
                'flag-set)
            (get (car (read-from-string arg)) 'texinfo-set-value))
           (insert (get (car (read-from-string arg)) 'texinfo-set-value)))
          ((eq (get (car (read-from-string arg)) 'texinfo-whether-setp) 
               'flag-cleared)
           (insert (format "{No value for \"%s\"}"  arg)))
          ((eq (get (car (read-from-string arg)) 'texinfo-whether-setp) nil)
           (insert (format "{No value for \"%s\"}"  arg))))))

(put 'ifset 'texinfo-end 'texinfo-discard-command)
(put 'ifset 'texinfo-format 'texinfo-if-set)
(defun texinfo-if-set ()
  "If set, continue formatting; else do not format region up to @end ifset"
  (let ((arg (texinfo-parse-arg-discard)))
    (cond
     ((eq (get (car (read-from-string arg)) 'texinfo-whether-setp)
          'flag-set)
      ;; Format the text (i.e., do not remove it); do nothing here.
      ())
     ((eq (get (car (read-from-string arg)) 'texinfo-whether-setp)
          'flag-cleared)
      ;; Clear region (i.e., cause the text to be ignored).
      (delete-region texinfo-command-start
                       (progn (re-search-forward "@end ifset[ \t]*\n")
                              (point))))
     ((eq (get (car (read-from-string arg)) 'texinfo-whether-setp)
          nil)
      ;; In this case flag is neither set nor cleared.  
      ;; Act as if set, i.e. do nothing.
      ()))))

(put 'ifclear 'texinfo-end 'texinfo-discard-command)
(put 'ifclear 'texinfo-format 'texinfo-if-clear)
(defun texinfo-if-clear ()
  "If clear, continue formatting; if set, do not format up to @end ifset"
  (let ((arg (texinfo-parse-arg-discard)))
    (cond
     ((eq (get (car (read-from-string arg)) 'texinfo-whether-setp)
          'flag-set)
      ;; Clear region (i.e., cause the text to be ignored).
      (delete-region texinfo-command-start
                       (progn (re-search-forward "@end ifclear[ \t]*\n")
                              (point))))
     ((eq (get (car (read-from-string arg)) 'texinfo-whether-setp)
          'flag-cleared)
      ;; Format the text (i.e., do not remove it); do nothing here.
      ())
     ((eq (get (car (read-from-string arg)) 'texinfo-whether-setp)
          nil)
      ;; In this case flag is neither set nor cleared.  
      ;; Act as if clear, i.e. do nothing.
      ()))))


;;; Process included files:  `@include' command

;; Updated 19 October 1990
;; In the original version, include files were ignored by Info but
;; incorporated in to the printed manual.  To make references to the
;; included file, the Texinfo source file has to refer to the included
;; files using the `(filename)nodename' format for refering to other
;; Info files.  Also, the included files had to be formatted on their
;; own.  It was just like they were another file.

;; Currently, include files are inserted into the buffer that is
;; formatted for Info.  If large, the resulting info file is split and
;; tagified.  For current include files to work, the master menu must
;; refer to all the nodes, and the highest level nodes in the include
;; files must have the correct next, prev, and up pointers.

;; The included file may have an @setfilename and even an @settitle,
;; but not an `\input texinfo' line.

;; Updated 24 March 1993
;; In order for @raisesections and @lowersections to work, included
;; files must be inserted into the buffer holding the outer file
;; before other Info formatting takes place.  So @include is no longer
;; is treated like other @-commands.
(put 'include 'texinfo-format  'texinfo-format-noop)

; Original definition:
; (defun texinfo-format-include ()
;   (let ((filename (texinfo-parse-arg-discard))
;       (default-directory input-directory)
;       subindex)
;     (setq subindex
;         (save-excursion
;           (progn (find-file
;                   (cond ((file-readable-p (concat filename ".texinfo"))
;                          (concat filename ".texinfo"))
;                         ((file-readable-p (concat filename ".texi"))
;                          (concat filename ".texi"))
;                         ((file-readable-p (concat filename ".tex"))
;                          (concat filename ".tex"))
;                         ((file-readable-p filename)
;                          filename)
;                         (t (error "@include'd file %s not found"
;                                   filename))))
;                  (texinfo-format-buffer-1))))
;     (texinfo-subindex 'texinfo-vindex (car subindex) (nth 1 subindex))
;     (texinfo-subindex 'texinfo-findex (car subindex) (nth 2 subindex))
;     (texinfo-subindex 'texinfo-cindex (car subindex) (nth 3 subindex))
;     (texinfo-subindex 'texinfo-pindex (car subindex) (nth 4 subindex))
;     (texinfo-subindex 'texinfo-tindex (car subindex) (nth 5 subindex))
;     (texinfo-subindex 'texinfo-kindex (car subindex) (nth 6 subindex))))
;
;(defun texinfo-subindex (indexvar file content)
;  (set indexvar (cons (list 'recurse file content)
;                      (symbol-value indexvar))))

; Second definition:
; (put 'include 'texinfo-format 'texinfo-format-include)
; (defun texinfo-format-include ()
;   (let ((filename (concat input-directory
;                           (texinfo-parse-arg-discard)))
;         (default-directory input-directory))
;     (message "Reading: %s" filename)
;     (save-excursion
;       (save-restriction
;         (narrow-to-region
;          (point)
;          (+ (point) (car (cdr (insert-file-contents filename)))))
;         (goto-char (point-min))
;         (texinfo-append-refill)
;         (texinfo-format-convert (point-min) (point-max))))
;     (setq last-input-buffer input-buffer)  ; to bypass setfilename
;     ))


;;; Numerous commands do nothing in Texinfo

;; These commands are defined in texinfo.tex for printed output.

(put 'bye 'texinfo-format 'texinfo-discard-line)
(put 'c 'texinfo-format 'texinfo-discard-line-with-args)
(put 'comment 'texinfo-format 'texinfo-discard-line-with-args)
(put 'contents 'texinfo-format 'texinfo-discard-line-with-args)
(put 'finalout 'texinfo-format 'texinfo-discard-line)
(put 'group 'texinfo-end 'texinfo-discard-line-with-args)
(put 'group 'texinfo-format 'texinfo-discard-line-with-args)
(put 'headings 'texinfo-format 'texinfo-discard-line-with-args)
(put 'hsize 'texinfo-format 'texinfo-discard-line-with-args)
(put 'itemindent 'texinfo-format 'texinfo-discard-line-with-args)
(put 'lispnarrowing 'texinfo-format 'texinfo-discard-line-with-args)
(put 'need 'texinfo-format 'texinfo-discard-line-with-args)
(put 'nopara 'texinfo-format 'texinfo-discard-line-with-args)
(put 'page 'texinfo-format 'texinfo-discard-line-with-args)
(put 'parindent 'texinfo-format 'texinfo-discard-line-with-args)
(put 'setchapternewpage 'texinfo-format 'texinfo-discard-line-with-args)
(put 'setq 'texinfo-format 'texinfo-discard-line-with-args)
(put 'settitle 'texinfo-format 'texinfo-discard-line-with-args)
(put 'setx 'texinfo-format 'texinfo-discard-line-with-args)
(put 'shortcontents 'texinfo-format 'texinfo-discard-line-with-args)
(put 'smallbook 'texinfo-format 'texinfo-discard-line)
(put 'summarycontents 'texinfo-format 'texinfo-discard-line-with-args)


;;; Some commands cannot be handled

(defun texinfo-unsupported ()
  (error "%s is not handled by texinfo"
         (buffer-substring texinfo-command-start texinfo-command-end)))

;;; Batch formatting

(defun batch-texinfo-format ()
  "Runs  texinfo-format-buffer  on the files remaining on the command line.
Must be used only with -batch, and kills emacs on completion.
Each file will be processed even if an error occurred previously.
For example, invoke
  \"emacs -batch -funcall batch-texinfo-format $docs/ ~/*.texinfo\"."
  (if (not noninteractive)
      (error "batch-texinfo-format may only be used -batch."))
  (let ((version-control t)
        (auto-save-default nil)
        (find-file-run-dired nil)
        (kept-old-versions 259259)
        (kept-new-versions 259259))
    (let ((error 0)
          file
          (files ()))
      (while command-line-args-left
        (setq file (expand-file-name (car command-line-args-left)))
        (cond ((not (file-exists-p file))
               (message ">> %s does not exist!" file)
               (setq error 1
                     command-line-args-left (cdr command-line-args-left)))
              ((file-directory-p file)
               (setq command-line-args-left
                     (nconc (directory-files file)
                            (cdr command-line-args-left))))
              (t
               (setq files (cons file files)
                     command-line-args-left (cdr command-line-args-left)))))
      (while files
        (setq file (car files)
              files (cdr files))
        (condition-case err
            (progn
              (if buffer-file-name (kill-buffer (current-buffer)))
              (find-file file)
              (buffer-disable-undo (current-buffer))
              (set-buffer-modified-p nil)
              (texinfo-mode)
              (message "texinfo formatting %s..." file)
              (texinfo-format-buffer nil)
              (if (buffer-modified-p)
                  (progn (message "Saving modified %s" (buffer-file-name))
                         (save-buffer))))
          (error
           (message ">> Error: %s" (prin1-to-string err))
           (message ">>  point at")
           (let ((s (buffer-substring (point)
                                      (min (+ (point) 100)
                                           (point-max))))
                 (tem 0))
             (while (setq tem (string-match "\n+" s tem))
               (setq s (concat (substring s 0 (match-beginning 0))
                               "\n>>  "
                               (substring s (match-end 0)))
                     tem (1+ tem)))
             (message ">>  %s" s))
           (setq error 1))))
      (kill-emacs error))))


;;; Place `provide' at end of file.
(provide 'texinfmt)

;;; texinfmt.el ends here.
