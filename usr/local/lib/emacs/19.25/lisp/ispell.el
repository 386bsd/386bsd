;;;;;;;;;;;;;;;;;;;;;;;;;;; -*- Mode: emacs-lisp -*- ;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; GNU EMACS interface for International Ispell Version 3.1 by Geoff Kuenning.
;;;
;;;
;;; Copyright (C) 1994 Free Software Foundation, Inc.
;;;
;;;
;;; Authors         : Ken Stevens et. al.
;;; Last Modified By: Ken Stevens <k.stevens@ieee.org>
;;; Last Modified On: Fri May 20 15:58:52 MDT 1994
;;; Update Revision : 2.30
;;; Syntax          : emacs-lisp
;;; Status	    : Release with 3.1.05 ispell.
;;; Version	    : International Ispell Version 3.1 by Geoff Kuenning.
;;; Bug Reports	    : ispell-el-bugs@itcorp.com
;;;
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
;;;
;;; Commentary:
;;;
;;; INSTRUCTIONS
;;;
;;;  This code contains a section of user-settable variables that you should
;;; inspect prior to installation.  Look past the end of the history list.
;;; Set them up for your locale and the preferences of the majority of the
;;; users.  Otherwise the users may need to set a number of variables
;;; themselves.
;;;  You particularly may want to change the default dictionary for your
;;; country and language.
;;;
;;;
;;; To fully install this, add this file to your Emacs Lisp directory and
;;; compile it with M-X byte-compile-file.  Then add the following to the
;;; appropriate init file:
;;;
;;;  (autoload 'ispell-word "ispell"
;;;    "Check the spelling of word in buffer." t)
;;;  (global-set-key "\e$" 'ispell-word)
;;;  (autoload 'ispell-region "ispell"
;;;    "Check the spelling of region." t)
;;;  (autoload 'ispell-buffer "ispell"
;;;    "Check the spelling of buffer." t)
;;;  (autoload 'ispell-complete-word "ispell"
;;;    "Look up current word in dictionary and try to complete it." t)
;;;  (autoload 'ispell-change-dictionary "ispell"
;;;    "Change ispell dictionary." t)
;;;  (autoload 'ispell-message "ispell"
;;;    "Check spelling of mail message or news post.")
;;;
;;;  Depending on the mail system you use, you may want to include these:
;;;
;;;  (add-hook 'news-inews-hook 'ispell-message)
;;;  (add-hook 'mail-send-hook  'ispell-message)
;;;  (add-hook 'mh-before-send-letter-hook 'ispell-message)
;;;
;;;
;;; Ispell has a TeX parser and a nroff parser (the default).
;;; The parsing is controlled by the variable ispell-parser.  Currently
;;; it is just a "toggle" between TeX and nroff, but if more parsers are
;;; added it will be updated.  See the variable description for more info.
;;;
;;;
;;; TABLE OF CONTENTS
;;;
;;;   ispell-word
;;;   ispell-region
;;;   ispell-buffer
;;;   ispell-message
;;;   ispell-continue
;;;   ispell-complete-word
;;;   ispell-complete-word-interior-frag
;;;   ispell-change-dictionary
;;;   ispell-kill-ispell
;;;   ispell-pdict-save
;;;
;;;
;;; Commands in ispell-region:
;;; Character replacement: Replace word with choice.  May query-replace.
;;; ' ': Accept word this time.
;;; 'i': Accept word and insert into private dictionary.
;;; 'a': Accept word for this session.
;;; 'A': Accept word and place in buffer-local dictionary.
;;; 'r': Replace word with typed-in value.  Rechecked.
;;; 'R': Replace word with typed-in value. Query-replaced in buffer. Rechecked.
;;; '?': Show these commands
;;; 'x': Exit spelling buffer.  Move cursor to original point.
;;; 'X': Exit spelling buffer.  Leave cursor at the current point.
;;; 'q': Quit spelling session (Kills ispell process).
;;; 'l': Look up typed-in replacement in alternate dictionary.  Wildcards okay.
;;; 'u': Like 'i', but the word is lower-cased first.
;;; 'm': Like 'i', but allows one to include dictionary completion info.
;;; 'C-l': redraws screen
;;; 'C-r': recursive edit
;;; 'C-z': suspend emacs or iconify frame
;;;
;;; Buffer-Local features:
;;; There are a number of buffer-local features that can be used to customize
;;;  ispell for the current buffer.  This includes language dictionaries,
;;;  personal dictionaries, parsing, and local word spellings.  Each of these
;;;  local customizations are done either through local variables, or by
;;;  including the keyword and argument(s) at the end of the buffer (usually
;;;  prefixed by the comment characters).  See the end of this file for
;;;  examples.  The local keywords and variables are:
;;;
;;;  ispell-dictionary-keyword   language-dictionary
;;;      uses local variable ispell-local-dictionary
;;;  ispell-pdict-keyword        personal-dictionary
;;;      uses local variable ispell-local-pdict
;;;  ispell-parsing-keyword      mode-arg extended-char-arg
;;;  ispell-words-keyword        any number of local word spellings
;;;
;;;
;;; BUGS:
;;;  Highlighting in version 19 still doesn't work on tty's.
;;;  On some versions of emacs, growing the minibuffer fails.
;;;
;;; HISTORY
;;;
;;; Revision 2.30  1994/5/20 15:58:52  stevens
;;; Continue ispell from ispell-word, C-z functionality fixed.
;;;
;;; Revision 2.29  1994/5/12 09:44:33  stevens
;;; Restored ispell-use-ptys-p, ispell-message aborts sends with interrupt.
;;; defined fn ispell
;;;
;;; Revision 2.28  1994/4/28 16:24:40  stevens
;;; Window checking when ispell-message put on gnus-inews-article-hook jwz.
;;; prefixed ispell- to highlight functions and horiz-scroll fn.
;;; Try and respect case of word in ispell-complete-word.
;;; Ignore non-char events.  Ispell-use-ptys-p commented out. Lucid menu.
;;; Better interrupt handling.  ispell-message improvements from Ethan.
;;;
;;; Revision 2.27
;;; version 18 explicit C-g handling disabled as it didn't work. Added
;;; ispell-extra-args for ispell customization (jwz)
;;;
;;; Revision 2.26  1994/2/15 16:11:14  stevens
;;; name changes for copyright assignment.  Added word-frags in complete-word.
;;; Horizontal scroll (John Conover). Query-replace matches words now.  bugs.
;;;
;;; Revision 2.25
;;; minor mods, upgraded ispell-message
;;;
;;; Revision 2.24
;;; query-replace more robust, messages, defaults, ispell-change-dict.
;;;
;;; Revision 2.23  1993/11/22 23:47:03  stevens
;;; ispell-message, Fixed highlighting, added menu-bar, fixed ispell-help, ...
;;;
;;; Revision 2.22
;;; Added 'u' command.  Fixed default in ispell-local-dictionary.
;;; fixed affix rules display.  Tib skipping more robust.  Contributions by
;;; Per Abraham (parser selection), Denis Howe, and Eberhard Mattes.
;;;
;;; Revision 2.21  1993/06/30 14:09:04  stevens
;;; minor bugs. (nroff word skipping fixed)
;;;
;;; Revision 2.20  1993/06/30 14:09:04  stevens
;;;
;;; Debugging and contributions by: Boris Aronov, Rik Faith, Chris Moore,
;;;  Kevin Rodgers, Malcolm Davis.
;;; Particular thanks to Michael Lipp, Jamie Zawinski, Phil Queinnec
;;;  and John Heidemann for suggestions and code.
;;; Major update including many tweaks.
;;; Many changes were integrations of suggestions.
;;; lookup-words rehacked to use call-process (Jamie).
;;; ispell-complete-word rehacked to be compatible with the rest of the
;;; system for word searching and to include multiple wildcards,
;;; and it's own dictionary.
;;; query-replace capability added.  New options 'X', 'R', and 'A'.
;;; buffer-local modes for dictionary, word-spelling, and formatter-parsing.
;;; Many random bugs, like commented comments being skipped, fix to
;;; keep-choices-win, fix for math mode, added pipe mode choice,
;;; fixed 'q' command, ispell-word checks previous word and leave cursor
;;; in same location.  Fixed tib code which could drop spelling regions.
;;; Cleaned up setq calls for efficiency. Gave more context on window overlays.
;;; Assure context on ispell-command-loop.  Window lossage in look cmd fixed.
;;; Due to pervasive opinion, common-lisp package syntax removed. Display
;;; problem when not highlighting.
;;;
;;; Revision 2.19  1992/01/10  10:54:08  geoff
;;; Make another attempt at fixing the "Bogus, dude" problem.  This one is
;;; less elegant, but has the advantage of working.
;;;
;;; Revision 2.18  1992/01/07  10:04:52  geoff
;;; Fix the "Bogus, Dude" problem in ispell-word.
;;;
;;; Revision 2.17  1991/09/12  00:01:42  geoff
;;; Add some changes to make ispell-complete-word work better, though
;;; still not perfectly.
;;;
;;; Revision 2.16  91/09/04  18:00:52  geoff
;;; More updates from Sebastian, to make the multiple-dictionary support
;;; more flexible.
;;;
;;; Revision 2.15  91/09/04  17:30:02  geoff
;;; Sebastian Kremer's tib support
;;;
;;; Revision 2.14  91/09/04  16:19:37  geoff
;;; Don't do set-window-start if the move-to-window-line moved us
;;; downward, rather than upward.  This prevents getting the buffer all
;;; confused.  Also, don't use the "not-modified" function to clear the
;;; modification flag;  instead use set-buffer-modified-p.  This prevents
;;; extra messages from flashing.
;;;
;;; Revision 2.13  91/09/04  14:35:41  geoff
;;; Fix a spelling error in a comment.  Add code to handshake with the
;;; ispell process before sending anything to it.
;;;
;;; Revision 2.12  91/09/03  20:14:21  geoff
;;; Add Sebastian Kremer's multiple-language support.
;;;
;;;
;;; Walt Buehring
;;; Texas Instruments - Computer Science Center
;;; ARPA:  Buehring%TI-CSL@CSNet-Relay
;;; UUCP:  {smu, texsun, im4u, rice} ! ti-csl ! buehring
;;;
;;; ispell-region and associated routines added by
;;; Perry Smith
;;; pedz@bobkat
;;; Tue Jan 13 20:18:02 CST 1987
;;;
;;; extensively modified by Mark Davies and Andrew Vignaux
;;; {mark,andrew}@vuwcomp
;;; Sun May 10 11:45:04 NZST 1987
;;;
;;; Ken Stevens  ARPA: k.stevens@ieee.org
;;; Tue Jan  3 16:59:07 PST 1989
;;; This file has overgone a major overhaul to be compatible with ispell
;;; version 2.1.  Most of the functions have been totally rewritten, and
;;; many user-accessible variables have been added.  The syntax table has
;;; been removed since it didn't work properly anyway, and a filter is
;;; used rather than a buffer.  Regular expressions are used based on
;;; ispell's internal definition of characters (see ispell(4)).
;;; Some new updates:
;;; - Updated to version 3.0 to include terse processing.
;;; - Added a variable for the look command.
;;; - Fixed a bug in ispell-word when cursor is far away from the word
;;;   that is to be checked.
;;; - Ispell places the incorrect word or guess in the minibuffer now.
;;; - fixed a bug with 'l' option when multiple windows are on the screen.
;;; - lookup-words just didn't work with the process filter.  Fixed.
;;; - Rewrote the process filter to make it cleaner and more robust
;;;   in the event of a continued line not being completed.
;;; - Made ispell-init-process more robust in handling errors.
;;; - Fixed bug in continuation location after a region has been modified by
;;;   correcting a misspelling.
;;; Mon 17 Sept 1990
;;;
;;; Sebastian Kremer <sk@thp.uni-koeln.de>
;;; Wed Aug  7 14:02:17 MET DST 1991
;;; - Ported ispell-complete-word from Ispell 2 to Ispell 3.
;;; - Added ispell-kill-ispell command.
;;; - Added ispell-dictionary and ispell-dictionary-alist variables to
;;;   support other than default language.  See their docstrings and
;;;   command ispell-change-dictionary.
;;; - (ispelled it :-)
;;; - Added ispell-skip-tib variable to support the tib bibliography
;;;   program.
;;;
;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;



;;; **********************************************************************
;;; The following variables should be set according to personal preference
;;; and location of binaries:
;;; **********************************************************************


;;;  ******* THIS FILE IS WRITTEN FOR ISPELL VERSION 3.1
;;; Code:

(defvar ispell-highlight-p t
  "*Highlight spelling errors when non-nil.")

(defvar ispell-highlight-face 'highlight
  "*The face used for Ispell highlighting.  For Emacses with overlays.
Possible values are `highlight', `modeline', `secondary-selection',
`region', and `underline'.
This variable can be set by the user to whatever face they desire.
It's most convenient if the cursor color and highlight color are
slightly different.")

(defvar ispell-check-comments nil
  "*Spelling of comments checked when non-nil.")

(defvar ispell-query-replace-choices nil
  "*Corrections made throughout region when non-nil.
Uses `query-replace' (\\[query-replace]) for corrections.")

(defvar ispell-skip-tib nil
  "*Does not spell check `tib' bibliography references when non-nil.
Skips any text between strings matching regular expressions
`ispell-tib-ref-beginning' and `ispell-tib-ref-end'.

TeX users beware:  Any field starting with [. will skip until a .] -- even
your whole buffer -- unless you set `ispell-skip-tib' to nil.  That includes
a [.5mm] type of number....")

(defvar ispell-tib-ref-beginning "[[<]\\."
  "Regexp matching the beginning of a Tib reference.")

(defvar ispell-tib-ref-end "\\.[]>]"
  "Regexp matching the end of a Tib reference.")

(defvar ispell-keep-choices-win t
  "*When not nil, the `*Choices*' window remains for spelling session.
This minimizes redisplay thrashing.")

(defvar ispell-choices-win-default-height 2
  "*The default size of the `*Choices*' window, including status line.
Must be greater than 1.")

(defvar ispell-program-name "ispell"
  "Program invoked by \\[ispell-word] and \\[ispell-region] commands.")

(defvar ispell-alternate-dictionary
  (cond ((file-exists-p "/usr/dict/web2") "/usr/dict/web2")
	((file-exists-p "/usr/dict/words") "/usr/dict/words")
	((file-exists-p "/usr/lib/dict/words") "/usr/lib/dict/words")
	((file-exists-p "/sys/dict") "/sys/dict")
	(t "/usr/dict/words"))
  "*Alternate dictionary for spelling help.")

(defvar ispell-complete-word-dict ispell-alternate-dictionary
  "*Dictionary used for word completion.")

(defvar ispell-grep-command "/usr/bin/egrep"
  "Name of the grep command for search processes.")

(defvar ispell-grep-options "-i"
  "String of options to use when running the program in `ispell-grep-command'.
Should probably be \"-i\" or \"-e\".
Some machines (like the NeXT) don't support \"-i\"")

(defvar ispell-look-command "/usr/bin/look"
  "Name of the look command for search processes.
This must be an absolute file name.")

(defvar ispell-look-p (file-exists-p ispell-look-command)
  "*Non-nil means use `look; rather than `grep'.
Default is based on whether `look' seems to be available.")

(defvar ispell-have-new-look nil
  "*Non-nil means use the `-r' option (regexp) when running `look'.")

(defvar ispell-look-options (if ispell-have-new-look "-dfr" "-df")
  "String of command options for `ispell-look-command'.")

(defvar ispell-use-ptys-p nil
  "When non-nil, Emacs uses ptys to communicate with Ispell.
When nil, Emacs uses pipes.")

(defvar ispell-following-word nil
  "*Non-nil means `ispell-word' checks the word around or after point.
Otherwise `ispell-word' checks the preceding word.")

(defvar ispell-help-in-bufferp nil
  "*Non-nil means display interactive keymap help in a buffer.
Otherwise use the minibuffer.")

(defvar ispell-quietly nil
  "*Non-nil means suppress messages in `ispell-word'.")

(defvar ispell-format-word (function upcase)
  "*Formatting function for displaying word being spell checked.
The function must take one string argument and return a string.")

(defvar ispell-personal-dictionary nil
  "*File name of your personal spelling dictionary.
If nil, default dictionary `~/.ispell_words' is used.")

(defvar ispell-silently-savep nil
  "*When non-nil, save the personal dictionary without confirmation.")

;;; This variable contains the current dictionary being used if the ispell
;;; process is running.  Otherwise it contains the global default.
(defvar ispell-dictionary nil
  "If non-nil, a dictionary to use instead of the default one.
This is passed to the ispell process using the `-d' switch and is
used as key in `ispell-dictionary-alist' (which see).

You should set this variable before your first use of Emacs spell-checking
commands in the Emacs session, or else use the \\[ispell-change-dictionary] command to
change it.  Otherwise, this variable only takes effect in a newly
started Ispell process.")

(defvar ispell-extra-args nil
  "*If non-nil, a list of extra switches to pass to the Ispell program.
For example, '(\"-W\" \"3\") to cause it to accept all 1-3 character
words as correct.  See also `ispell-dictionary-alist', which may be used
for language-specific arguments.")

(defvar ispell-dictionary-alist		; sk  9-Aug-1991 18:28
  '((nil				; default (english.aff)
     "[A-Za-z]" "[^A-Za-z]" "[---']" nil ("-B") nil)
    ("english"				; make english explicitly selectable
     "[A-Za-z]" "[^A-Za-z]" "[---']" nil ("-B") nil)
    ("deutsch"				; deutsch.aff
     "[a-zA-Z\"]" "[^a-zA-Z\"]" "[---']" t ("-C") nil)
    ("deutsch8"
     "[a-zA-Z\304\326\334\344\366\337\374]"
     "[^a-zA-Z\304\326\334\344\366\337\374]"
     "[---']" t ("-C" "-d" "deutsch") "~latin1")
    ("nederlands8"				; dutch8.aff
     "[A-Za-z\300-\305\307\310-\317\322-\326\331-\334\340-\345\347\350-\357\361\362-\366\371-\374]"
     "[^A-Za-z\300-\305\307\310-\317\322-\326\331-\334\340-\345\347\350-\357\361\362-\366\371-\374]"
     "[---']" t ("-C") nil)
    ("svenska"				;7 bit swedish mode
     "[A-Za-z}{|\\133\\135\\\\]" "[^A-Za-z}{|\\133\\135\\\\]"
     "[---']" nil ("-C") nil)
    ("svenska8"				;8 bit swedish mode
     "[A-Za-z\345\344\366\305\304\366]"  "[^A-Za-z\345\344\366\305\304\366]"
     "[---']" nil ("-C" "-d" "svenska") "~list") ; Add `"-T" "list"' instead?
    ("francais"
     "[A-Za-z]" "[^A-Za-z]" "[`'^---]" t nil nil)
    ("francais8" "[A-Za-z\300\302\306\307\310\311\312\313\316\317\324\331\333\334\340\342\346\347\350\351\352\353\356\357\364\371\373\374]"
     "[^A-Za-z\300\302\304\306\307\310\311\312\313\316\317\324\326\331\333\334\340\342\344\346\347\350\351\352\353\356\357\364\366\371\373\374]" "[---']"
     t nil "~list")
    ("dansk"				; dansk.aff
     "[A-Z\306\330\305a-z\346\370\345]" "[^A-Z\306\330\305a-z\346\370\345]"
     "[---]" nil ("-C") nil)
    )
  "An alist of dictionaries and their associated parameters.

Each element of this list is also a list:

\(DICTIONARY-NAME CASECHARS NOT-CASECHARS OTHERCHARS MANY-OTHERCHARS-P
        ISPELL-ARGS EXTENDED-CHARACTER-MODE\)

DICTIONARY-NAME is a possible value of variable `ispell-dictionary', nil
means the default dictionary.

CASECHARS is a regular expression of valid characters that comprise a
word.

NOT-CASECHARS is the opposite regexp of CASECHARS.

OTHERCHARS is a regular expression of other characters that are valid
in word constructs.  Otherchars cannot be adjacent to each other in a
word, nor can they begin or end a word.  This implies we can't check
\"Stevens'\" as a correct possessive and other correct formations.

Hint: regexp syntax requires the hyphen to be declared first here.

MANY-OTHERCHARS-P is non-nil if many otherchars are to be allowed in a
word instead of only one.

ISPELL-ARGS is a list of additional arguments passed to the ispell
subprocess.

EXTENDED-CHARACTER-MODE should be used when dictionaries are used which
have been configured in Ispell's parse.y.  (For example, umlauts
can be encoded as \\\"a, a\\\", \"a, ...)  Defaults are ~tex and ~nroff
in English.  This has the same effect as the command-line `-T' option.
The buffer Major Mode controls Ispell's parsing in tex or nroff mode,
but the dictionary can control the extended character mode.
Both defaults can be overruled in a buffer-local fashion. See
`ispell-parsing-keyword' for details on this.

Note that the CASECHARS and OTHERCHARS slots of the alist should
contain the same character set as casechars and otherchars in the
language.aff file \(e.g., english.aff\).")



(cond
 ((and (string-lessp "19" emacs-version)
       (string-match "Lucid" emacs-version))
  (let ((dicts (cons (cons "default" nil) ispell-dictionary-alist))
	(current-menubar (or current-menubar default-menubar))
	(menu
	 '(["Help"		(describe-function 'ispell-help) t]
	   ;;["Help"		(popup-menu ispell-help-list)	t]
	   ["Check Message"	ispell-message			t]
	   ["Check Buffer"	ispell-buffer			t]
	   ["Check Word"	ispell-word			t]
	   ["Check Region"	ispell-region  (or (not zmacs-regions) (mark))]
	   ["Continue Check"	ispell-continue			t]
	   ["Complete Word Frag"ispell-complete-word-interior-frag t]
	   ["Complete Word"	ispell-complete-word		t]
	   ["Kill Process"	ispell-kill-ispell		t]
	   "-"
	   ["Save Dictionary"	(ispell-pdict-save t)		t]
	   ["Change Dictionary"	ispell-change-dictionary	t]))
	name)
    (while dicts
      (setq name (car (car dicts))
	    dicts (cdr dicts))
      (if (stringp name)
	  (setq menu (append menu
			     (list
			      (vector (concat "Select " (capitalize name))
				      (list 'ispell-change-dictionary name)
				      t))))))
    (defvar ispell-menu-lucid menu "Lucid's spelling menu.")
    (if current-menubar
	(progn
	  (delete-menu-item '("Edit" "Spell")) ; in case already defined
	  (add-menu '("Edit") "Spell" ispell-menu-lucid)))))

 ;; cond-case:
 ((and (featurep 'menu-bar)
       (string-lessp "19" emacs-version))
  (let ((dicts (reverse (cons (cons "default" nil) ispell-dictionary-alist)))
	name)
    (defvar ispell-menu-map nil)
    ;; Can put in defvar when external defines are removed.
    (setq ispell-menu-map (make-sparse-keymap "Spell"))
    (while dicts
      (setq name (car (car dicts))
	    dicts (cdr dicts))
      (if (stringp name)
	  (define-key ispell-menu-map (vector (intern name))
	    (cons (concat "Select " (capitalize name))
		  (list 'lambda () '(interactive)
			(list 'ispell-change-dictionary name))))))
    ;; Why do we need an alias here?
    (defalias 'ispell-menu-map ispell-menu-map)
    ;; Define commands in opposite order you want them to appear in menu.
    (define-key ispell-menu-map [ispell-change-dictionary]
      '("Change Dictionary" . ispell-change-dictionary))
    (define-key ispell-menu-map [ispell-kill-ispell]
      '("Kill Process" . ispell-kill-ispell))
    (define-key ispell-menu-map [ispell-pdict-save]
      '("Save Dictionary" . (lambda () (interactive) (ispell-pdict-save t))))
    (define-key ispell-menu-map [ispell-complete-word]
      '("Complete Word" . ispell-complete-word))
    (define-key ispell-menu-map [ispell-complete-word-interior-frag]
      '("Complete Word Frag" . ispell-complete-word-interior-frag))
    (define-key ispell-menu-map [ispell-continue]
      '("Continue Check" . ispell-continue))
    (define-key ispell-menu-map [ispell-region]
      '("Check Region" . ispell-region))
    (define-key ispell-menu-map [ispell-word]
      '("Check Word" . ispell-word))
    (define-key ispell-menu-map [ispell-buffer]
      '("Check Buffer" . ispell-buffer))
    (define-key ispell-menu-map [ispell-message]
      '("Check Message" . ispell-message))
    (define-key ispell-menu-map [ispell-help]
      '("Help" . (lambda () (interactive)
		   (describe-function 'ispell-help)
		   ;(x-popup-menu last-nonmenu-event(list "" ispell-help-list))
		   ))))
  (put 'ispell-region 'menu-enable 'mark-active)))


;;; **********************************************************************
;;; The following are used by ispell, and should not be changed.
;;; **********************************************************************


;;; This doesn't match the LAST patch number -- this is for 3.1 or 3.0.09
(defconst ispell-required-versions '("3.1." "3.0.09")
  "Ispell versions with which this version of ispell.el is known to work.")

(defun ispell-get-casechars ()
  (nth 1 (assoc ispell-dictionary ispell-dictionary-alist)))
(defun ispell-get-not-casechars ()
  (nth 2 (assoc ispell-dictionary ispell-dictionary-alist)))
(defun ispell-get-otherchars ()
  (nth 3 (assoc ispell-dictionary ispell-dictionary-alist)))
(defun ispell-get-many-otherchars-p ()
  (nth 4 (assoc ispell-dictionary ispell-dictionary-alist)))
(defun ispell-get-ispell-args ()
  (nth 5 (assoc ispell-dictionary ispell-dictionary-alist)))
(defun ispell-get-extended-character-mode ()
  (nth 6 (assoc ispell-dictionary ispell-dictionary-alist)))

(defvar ispell-process nil
  "The process object for Ispell.")

(defvar ispell-pdict-modified-p nil
  "Non-nil means personal dictionary has modifications to be saved.")

;;; If you want to save the dictionary when quitting, must do so explicitly.
;; When non-nil, the spell session is terminated.
;; When numeric, contains cursor location in buffer, and cursor remains there.
(defvar ispell-quit nil)

(defvar ispell-filter nil
  "Output filter from piped calls to Ispell.")

(defvar ispell-filter-continue nil
  "Control variable for Ispell filter function.")

(defvar ispell-process-directory nil
  "The directory where `ispell-process' was started.")

(defvar ispell-query-replace-marker (make-marker)
  "Marker for `query-replace' processing.")

(defvar ispell-checking-message nil
  "Non-nil when we're checking a mail message")

(defconst ispell-choices-buffer "*Choices*")

(defvar ispell-overlay nil "Overlay variable for Ispell highlighting.")

;;; *** Buffer Local Definitions ***

;;; This is the local dictionary to use.  When nil the default dictionary will
;;; be used.  Do not redefine default value or it will override the global!
(defvar ispell-local-dictionary nil
  "If non-nil, a dictionary to use for Ispell commands in this buffer.
The value should be a string, which is a file name.
This variable becomes buffer-local when set in any fashion.

Setting ispell-local-dictionary to a value has the same effect as
calling \\[ispell-change-dictionary] with that value.  This variable
is automatically set when defined in the file with either
`ispell-dictionary-keyword' or the Local Variable syntax.")

(make-variable-buffer-local 'ispell-local-dictionary)

;; Use default directory, unless locally set.
(set-default 'ispell-local-dictionary nil)

(defconst ispell-words-keyword "LocalWords: "				      
  "The keyword for local oddly-spelled words to accept.
The keyword will be followed by any number of local word spellings.
There can be multiple of these keywords in the file.")

(defconst ispell-dictionary-keyword "Local IspellDict: "
  "The keyword for local dictionary definitions.
There should be only one dictionary keyword definition per file, and it
should be followed by a correct dictionary name in `ispell-dictionary-alist'.")

(defconst ispell-parsing-keyword "Local IspellParsing: "
  "The keyword for overriding default Ispell parsing.
Determined by the buffer's major mode and extended-character mode as well as
the default dictionary.

The above keyword string should be followed by `latex-mode' or
`nroff-mode' to put the current buffer into the desired parsing mode.

Extended character mode can be changed for this buffer by placing
a `~' followed by an extended-character mode -- such as `~.tex'.")

(defvar ispell-local-pdict ispell-personal-dictionary
  "A buffer local variable containing the current personal dictionary.
If non-nil, the value must be a string, which is a file name.

If you specify a personal dictionary for the current buffer which is
different from the current personal dictionary, the effect is similar
to calling \\[ispell-change-dictionary].  This variable is automatically
set when defined in the file with either `ispell-pdict-keyword' or the
local variable syntax.")

(make-variable-buffer-local 'ispell-local-pdict)

(defconst ispell-pdict-keyword "Local IspellPersDict: "
  "The keyword for defining buffer local dictionaries.")

(defvar ispell-buffer-local-name nil
  "Contains the buffer name if local word definitions were used.
Ispell is then restarted because the local words could conflict.")

(defvar ispell-parser 'use-mode-name
   "*Indicates whether ispell should parse the current buffer as TeX Code.
Special value `use-mode-name' tries to guess using the name of major-mode.
Default parser is 'nroff.
Currently the only other valid parser is 'tex.

You can set this variable in hooks in your init file -- eg:

\(add-hook 'tex-mode-hook (function (lambda () (setq ispell-parser 'tex))))")

(defvar ispell-region-end (make-marker)
  "Marker that allows spelling continuations.")


;;; **********************************************************************
;;; **********************************************************************


(defalias 'ispell 'ispell-buffer)

;;;###autoload (define-key global-map "\M-$" 'ispell-word)

;;;###autoload
(defun ispell-word (&optional following quietly continue)
  "Check spelling of word under or before the cursor.
If the word is not found in dictionary, display possible corrections
in a window and so you can choose one.

With a prefix argument (or if CONTINUE is non-nil),
resume interrupted spell-checking of a buffer or region.

If optional argument FOLLOWING is non-nil or if `ispell-following-word'
is non-nil when called interactively, then the following word
\(rather than preceding\) is checked when the cursor is not over a word.
When the optional argument QUIETLY is non-nil or `ispell-quietly' is non-nil
when called interactively, non-corrective messages are suppressed.

Word syntax described by `ispell-dictionary-alist' (which see).

This will check or reload the dictionary.  Use \\[ispell-change-dictionary]
or \\[ispell-region] to update the Ispell process."
  (interactive (list nil nil current-prefix-arg))
  (if continue
      (ispell-continue)
    (if (interactive-p)
	(setq following ispell-following-word
	      quietly ispell-quietly))
    (ispell-buffer-local-dict)		; use the correct dictionary
    (let ((cursor-location (point))	; retain cursor location
	  ispell-keep-choices-win	; override global to force creation
	  (word (ispell-get-word following))
	  start end poss replace)
      ;; destructure return word info list.
      (setq start (car (cdr word))
	    end (car (cdr (cdr word)))
	    word (car word))

      ;; now check spelling of word.
      (or quietly
	  (message "Checking spelling of %s..."
		   (funcall ispell-format-word word)))
      (ispell-init-process)		; erases ispell output buffer
      (process-send-string ispell-process "%\n") ;put in verbose mode
      (process-send-string ispell-process (concat "^" word "\n"))
      ;; wait until ispell has processed word
      (while (progn
	       (accept-process-output ispell-process)
	       (not (string= "" (car ispell-filter)))))
      ;;(process-send-string ispell-process "!\n") ;back to terse mode.
      (setq ispell-filter (cdr ispell-filter))
      (if (listp ispell-filter)
	  (setq poss (ispell-parse-output (car ispell-filter))))
      (cond ((eq poss t)
	     (or quietly
		 (message "%s is correct" (funcall ispell-format-word word))))
	    ((stringp poss)
	     (or quietly
		 (message "%s is correct because of root %s"
			  (funcall ispell-format-word word)
			  (funcall ispell-format-word poss))))
	    ((null poss) (message "Error in ispell process"))
	    (t				; prompt for correct word.
	     (unwind-protect
		 (progn
		   (if ispell-highlight-p ;highlight word
		       (ispell-highlight-spelling-error start end t))
		   (setq replace (ispell-command-loop
				  (car (cdr (cdr poss)))
				  (car (cdr (cdr (cdr poss))))
				  (car poss))))
	       ;; protected
	       (if ispell-highlight-p	; clear highlight
		   (ispell-highlight-spelling-error start end)))
	     (cond ((equal 0 replace)
		    (ispell-add-per-file-word-list (car poss)))
		   (replace
		    (delete-region start end)
		    (setq word (if (atom replace) replace (car replace))
			  cursor-location (+ (- (length word) (- end start))
					     cursor-location))
		    (insert word)
		    (if (not (atom replace)) ; recheck spelling of replacement
			(progn
			  (goto-char cursor-location)
			  (ispell-word following quietly)))))
	     (if (get-buffer ispell-choices-buffer)
		 (kill-buffer ispell-choices-buffer))))
      (goto-char cursor-location)	; return to original location
      (ispell-pdict-save ispell-silently-savep)
      (if ispell-quit (setq ispell-quit nil)))))


(defun ispell-get-word (following &optional extra-otherchars)
  "Return the word for spell-checking according to ispell syntax.
If optional argument FOLLOWING is non-nil or if `ispell-following-word'
is non-nil when called interactively, then the following word
\(rather than preceeding\) is checked when the cursor is not over a word.
Optional second argument contains otherchars that can be included in word
many times.

Word syntax described by `ispell-dictionary-alist' (which see)."
  (let* ((ispell-casechars (ispell-get-casechars))
	 (ispell-not-casechars (ispell-get-not-casechars))
	 (ispell-otherchars (ispell-get-otherchars))
	 (ispell-many-otherchars-p (ispell-get-many-otherchars-p))
	 (word-regexp (concat ispell-casechars
			      "+\\("
			      ispell-otherchars
			      "?"
			      (if extra-otherchars
				  (concat extra-otherchars "?"))
			      ispell-casechars
			      "+\\)"
			      (if (or ispell-many-otherchars-p
				      extra-otherchars)
				  "*" "?")))
	 did-it-once
	 start end word)
    ;; find the word
    (if (not (looking-at ispell-casechars))
	(if following
	    (re-search-forward ispell-casechars (point-max) t)
	  (re-search-backward ispell-casechars (point-min) t)))
    ;; move to front of word
    (re-search-backward ispell-not-casechars (point-min) 'start)
    (while (and (or (looking-at ispell-otherchars)
		    (and extra-otherchars (looking-at extra-otherchars)))
		(not (bobp))
		(or (not did-it-once)
		    ispell-many-otherchars-p))
      (if (and extra-otherchars (looking-at extra-otherchars))
	  (progn
	    (backward-char 1)
	    (if (looking-at ispell-casechars)
		(re-search-backward ispell-not-casechars (point-min) 'move)))
	(setq did-it-once t)
	(backward-char 1)
	(if (looking-at ispell-casechars)
	    (re-search-backward ispell-not-casechars (point-min) 'move)
	  (backward-char -1))))
    ;; Now mark the word and save to string.
    (or (re-search-forward word-regexp (point-max) t)
	(error "No word found to check!"))
    (setq start (match-beginning 0)
	  end (point)
	  word (buffer-substring start end))
    (list word start end)))


;;; Global ispell-pdict-modified-p is set by ispell-command-loop and
;;; tracks changes in the dictionary.  The global may either be
;;; a value or a list, whose value is the state of whether the
;;; dictionary needs to be saved.

(defun ispell-pdict-save (&optional no-query force-save)
  "Check to see if the personal dictionary has been modified.
If so, ask if it needs to be saved."
  (interactive (list ispell-silently-savep t))
  (if (and ispell-pdict-modified-p (listp ispell-pdict-modified-p))
      (setq ispell-pdict-modified-p (car ispell-pdict-modified-p)))
  (if (or ispell-pdict-modified-p force-save)
      (if (or no-query (y-or-n-p "Personal dictionary modified.  Save? "))
	  (process-send-string ispell-process "#\n")))
  ;; unassert variable, even if not saved to avoid questioning.
  (setq ispell-pdict-modified-p nil))


(defun ispell-command-loop (miss guess word)
  "Display possible corrections from list MISS.
GUESS lists possibly valid affix construction of WORD.
Returns nil to keep word.
Returns 0 to insert locally into buffer-local dictionary.
Returns string for new chosen word.
Returns list for new replacement word (will be rechecked).
Global `ispell-pdict-modified-p' becomes a list where the only value
indicates whether the dictionary has been modified when option `a' or `i' is
used."
  (unwind-protect
      (save-window-excursion
	(let ((count ?0)
	      (line 2)
	      (max-lines (- (window-height) 4)) ; assure 4 context lines.
	      (choices miss)
	      (window-min-height (min window-min-height
				      ispell-choices-win-default-height))
	      (command-characters '( ?  ?i ?a ?A ?r ?R ?? ?x ?X ?q ?l ?u ?m ))
	      (skipped 0)
	      char num result)
	  (save-excursion
	    (if ispell-keep-choices-win
		(select-window (previous-window))
	      (set-buffer (get-buffer-create ispell-choices-buffer))
	      (setq mode-line-format "--  %b  --"))
	    (if (equal (get-buffer ispell-choices-buffer) (current-buffer))
		(erase-buffer)
	      (error (concat "Bogus, dude! I should be in the *Choices*"
			     " buffer, but I'm not!")))
	    (if guess
		(progn
		  (insert "Affix rules generate and capitalize "
			  "this word as shown below:\n\t")
		  (while guess
		    (if (> (+ 4 (current-column) (length (car guess)))
			   (window-width))
			(progn
			  (insert "\n\t")
			  (setq line (1+ line))))
		    (insert (car guess) "    ")
		    (setq guess (cdr guess)))
		  (insert "\nUse option `i' if this is a correct composition"
			  " from the derivative root.\n")
		  (setq line (+ line (if choices 3 2)))))
	    (while (and choices
			(< (if (> (+ 7 (current-column) (length (car choices))
				     (if (> count ?~) 3 0))
				  (window-width))
			       (progn
				 (insert "\n")
				 (setq line (1+ line)))
			     line)
			   max-lines))
	      ;; not so good if there are over 20 or 30 options, but then, if
	      ;; there are that many you don't want to scan them all anyway...
	      (while (memq count command-characters) ; skip command characters.
		(setq count (1+ count)
		      skipped (1+ skipped)))
	      (insert "(" count ") " (car choices) "  ")
	      (setq choices (cdr choices)
		    count (1+ count)))
	    (setq count (- count ?0 skipped)))

	  (if ispell-keep-choices-win
	      (if (> line ispell-keep-choices-win)
		  (progn
		    (switch-to-buffer ispell-choices-buffer)
		    (select-window (next-window))
		    (save-excursion
		      (let ((cur-point (point)))
			(move-to-window-line (- line ispell-keep-choices-win))
			(if (<= (point) cur-point)
			    (set-window-start (selected-window) (point)))))
		    (select-window (previous-window))
		    (enlarge-window (- line ispell-keep-choices-win))
		    (goto-char (point-min))))
	    (ispell-overlay-window (max line
					ispell-choices-win-default-height)))
	  (switch-to-buffer ispell-choices-buffer)
	  (goto-char (point-min))
	  (select-window (next-window))
	  (while
	      (eq
	       t
	       (setq
		result
		(progn
		  (undo-boundary)
		  (message (concat "C-h or ? for more options; SPC to leave "
				   "unchanged, Character to replace word"))
		  (let ((inhibit-quit t))
		    (setq char (if (fboundp 'read-char-exclusive)
				   (read-char-exclusive)
				 (read-char))
			  skipped 0)
		    (if (or quit-flag (= char ?\C-g)) ; C-g is like typing q
			(setq char ?q
			      quit-flag nil)))
		  ;; Adjust num to array offset skipping command characters.
		  (let ((com-chars command-characters))
		    (while com-chars
		      (if (and (> (car com-chars) ?0) (< (car com-chars) char))
			  (setq skipped (1+ skipped)))
		      (setq com-chars (cdr com-chars)))
		    (setq num (- char ?0 skipped)))

		  (cond
		   ((= char ? ) nil)	; accept word this time only
		   ((= char ?i)		; accept and insert word into pers dict
		    (process-send-string ispell-process (concat "*" word "\n"))
		    (setq ispell-pdict-modified-p '(t)) ; dictionary modified!
		    nil)
		   ((or (= char ?a) (= char ?A)) ; accept word without insert
		    (process-send-string ispell-process (concat "@" word "\n"))
		    (if (null ispell-pdict-modified-p)
			(setq ispell-pdict-modified-p
			      (list ispell-pdict-modified-p)))
		    (if (= char ?A) 0))	; return 0 for ispell-add buffer-local
		   ((or (= char ?r) (= char ?R)) ; type in replacement
		    (if (or (= char ?R) ispell-query-replace-choices)
			(list (read-string "Query-replacement for: " word) t)
		      (cons (read-string "Replacement for: " word) nil)))
		   ((or (= char ??) (= char help-char) (= char ?\C-h))
		    (ispell-help)
		    t)
		   ;; Quit and move point back.
		   ((= char ?x)
		    (ispell-pdict-save ispell-silently-savep)
		    (message "Exited spell-checking")
		    (setq ispell-quit t)
		    nil)
		   ;; Quit and preserve point.
		   ((= char ?X)
		    (ispell-pdict-save ispell-silently-savep)
		    (message
		     (substitute-command-keys
		      (concat "Spell-checking suspended;"
			      " use C-u \\[ispell-word] to resume")))
		    (setq ispell-quit (max (point-min)
					   (- (point) (length word))))
		    nil)
		   ((= char ?q)
		    (if (y-or-n-p "Really kill Ispell process? ")
			(progn
			  (ispell-kill-ispell t) ; terminate process.
			  (setq ispell-quit (or (not ispell-checking-message)
						(point))
				ispell-pdict-modified-p nil))
		      t))		; continue if they don't quit.
		   ((= char ?l)
		    (let ((new-word (read-string
				     "Lookup string (`*' is wildcard): "
				     word))
			  (new-line 2))
		      (if new-word
			  (progn
			    (save-excursion
			      (set-buffer (get-buffer-create
					   ispell-choices-buffer))
			      (erase-buffer)
			      (setq count ?0
				    skipped 0
				    mode-line-format "--  %b  --"
				    miss (lookup-words new-word)
				    choices miss)
			      (while (and choices ; adjust choices window.
					  (< (if (> (+ 7 (current-column)
						       (length (car choices))
						       (if (> count ?~) 3 0))
						    (window-width))
						 (progn
						   (insert "\n")
						   (setq new-line
							 (1+ new-line)))
					       new-line)
					     max-lines))
				(while (memq count command-characters)
				  (setq count (1+ count)
					skipped (1+ skipped)))
				(insert "(" count ") " (car choices) "  ")
				(setq choices (cdr choices)
				      count (1+ count)))
			      (setq count (- count ?0 skipped)))
			    (select-window (previous-window))
			    (if (/= new-line line)
				(progn
				  (if (> new-line line)
				      (enlarge-window (- new-line line))
				    (shrink-window (- line new-line)))
				  (setq line new-line)))
			    (select-window (next-window)))))
		    t)			; reselect from new choices
		   ((= char ?u)
		    (process-send-string ispell-process
					 (concat "*" (downcase word) "\n"))
		    (setq ispell-pdict-modified-p '(t)) ; dictionary modified!
		    nil)
		   ((= char ?m)		; type in what to insert
		    (process-send-string
		     ispell-process (concat "*" (read-string "Insert: " word)
					    "\n"))
		    (setq ispell-pdict-modified-p '(t))
		    (cons word nil))
		   ((and (>= num 0) (< num count))
		    (if ispell-query-replace-choices ; Query replace flag
			(list (nth num miss) 'query-replace)
		      (nth num miss)))
		   ((= char ?\C-l)
		    (redraw-display) t)
		   ((= char ?\C-r)
		    (save-window-excursion (recursive-edit)) t)
		   ((= char ?\C-z)
		    (funcall (key-binding "\C-z"))
		    t)
		   (t (ding) t))))))
	  result))
    (if (not ispell-keep-choices-win) (bury-buffer ispell-choices-buffer))))


;;;###autoload
(defun ispell-help ()
  "Display a list of the options available when a misspelling is encountered.

Selections are:

DIGIT: Replace the word with a digit offered in the *Choices* buffer.
SPC:   Accept word this time.
`i':   Accept word and insert into private dictionary.
`a':   Accept word for this session.
`A':   Accept word and place in `buffer-local dictionary'.
`r':   Replace word with typed-in value.  Rechecked.
`R':   Replace word with typed-in value. Query-replaced in buffer. Rechecked.
`?':   Show these commands.
`x':   Exit spelling buffer.  Move cursor to original point.
`X':   Exit spelling buffer.  Leaves cursor at the current point, and permits
        the aborted check to be completed later.
`q':   Quit spelling session (Kills ispell process).
`l':   Look up typed-in replacement in alternate dictionary.  Wildcards okay.
`u':   Like `i', but the word is lower-cased first.
`m':   Like `i', but allows one to include dictionary completion information.
`C-l':  redraws screen
`C-r':  recursive edit
`C-z':  suspend emacs or iconify frame"

  (let ((help-1 (concat "[r/R]eplace word; [a/A]ccept for this session; "
			"[i]nsert into private dictionary"))
	(help-2 (concat "[l]ook a word up in alternate dictionary;  "
			"e[x/X]it;  [q]uit session"))
	(help-3 (concat "[u]ncapitalized insert into dictionary.  "
			"Type 'C-h d ispell-help' for more help")))
    (save-window-excursion
      (if ispell-help-in-bufferp
	  (progn
	    (ispell-overlay-window 4)
	    (switch-to-buffer (get-buffer-create "*Ispell Help*"))
	    (insert (concat help-1 "\n" help-2 "\n" help-3))
	    (sit-for 5)
	    (kill-buffer "*Ispell Help*"))
	(select-window (minibuffer-window))
	;;(enlarge-window 2)
	(erase-buffer)
	(cond ((string-match "Lucid" emacs-version)
	       (message help-3)
	       (enlarge-window 1)
	       (message help-2)
	       (enlarge-window 1)
	       (message help-1)
	       (goto-char (point-min)))
	      (t
	       (if (string-lessp "19" emacs-version)
		   (message nil))
	       (enlarge-window 2)
	       (insert (concat help-1 "\n" help-2 "\n" help-3))))
	(sit-for 5)
	(erase-buffer)))))


(defun lookup-words (word &optional lookup-dict)
  "Look up word in word-list dictionary.
A `*' serves as a wild card.  If no wild cards, `look' is used if it exists.
Otherwise the variable `ispell-grep-command' contains the command used to
search for the words (usually egrep).

Optional second argument contains the dictionary to use; the default is
`ispell-alternate-dictionary'."
  ;; We don't use the filter for this function, rather the result is written
  ;; into a buffer.  Hence there is no need to save the filter values.
  (if (null lookup-dict)
      (setq lookup-dict ispell-alternate-dictionary))

  (let* ((process-connection-type ispell-use-ptys-p)
	 (wild-p (string-match "\\*" word))
	 (look-p (and ispell-look-p	; Only use look for an exact match.
		      (or ispell-have-new-look (not wild-p))))
	 (ispell-grep-buffer (get-buffer-create "*Ispell-Temp*")) ; result buf
	 (prog (if look-p ispell-look-command ispell-grep-command))
	 (args (if look-p ispell-look-options ispell-grep-options))
	 status results loc)
    (unwind-protect
	(save-window-excursion
	  (message "Starting \"%s\" process..." (file-name-nondirectory prog))
	  (set-buffer ispell-grep-buffer)
	  (if look-p
	      nil
	    ;; convert * to .*
	    (insert "^" word "$")
	    (while (search-backward "*" nil t) (insert "."))
	    (setq word (buffer-string))
	    (erase-buffer))
	  (setq status (call-process prog nil t nil args word lookup-dict))
	  ;; grep returns status 1 and no output when word not found, which
	  ;; is a perfectly normal thing.
	  (if (stringp status)
	      (setq results (cons (format "error: %s exited with signal %s"
					  (file-name-nondirectory prog) status)
				  results))
	    ;; else collect words into `results' in FIFO order
	    (goto-char (point-max))
	    ;; assure we've ended with \n
	    (or (bobp) (= (preceding-char) ?\n) (insert ?\n))
	    (while (not (bobp))
	      (setq loc (point))
	      (forward-line -1)
	      (setq results (cons (buffer-substring (point) (1- loc))
				  results)))))
      ;; protected
      (kill-buffer ispell-grep-buffer)
      (if (and results (string-match ".+: " (car results)))
	  (error "%s error: %s" ispell-grep-command (car results))))
    results))


;;; "ispell-filter" is a list of output lines from the generating function.
;;;   Each full line (ending with \n) is a separate item on the list.
;;; "output" can contain multiple lines, part of a line, or both.
;;; "start" and "end" are used to keep bounds on lines when "output" contains
;;;   multiple lines.
;;; "ispell-filter-continue" is true when we have received only part of a
;;;   line as output from a generating function ("output" did not end with \n)
;;; NOTE THAT THIS FUNCTION WILL FAIL IF THE PROCESS OUTPUT DOESNT END WITH \n!
;;;   This is the case when a process dies or fails. The default behavior
;;;   in this case treats the next input received as fresh input.

(defun ispell-filter (process output)
  "Output filter function for ispell, grep, and look."
  (let ((start 0)
	(continue t)
	end)
    (while continue
      (setq end (string-match "\n" output start)) ; get text up to the newline.
      ;; If we get out of sync and ispell-filter-continue is asserted when we
      ;; are not continuing, treat the next item as a separate list.  When
      ;; ispell-filter-continue is asserted, ispell-filter *should* always be a
      ;; list!

      ;; Continue with same line (item)?
      (if (and ispell-filter-continue ispell-filter (listp ispell-filter))
	  ;; Yes.  Add it to the prev item
	  (setcar ispell-filter
		  (concat (car ispell-filter) (substring output start end)))
	;; No. This is a new line and item.
	(setq ispell-filter
	      (cons (substring output start end) ispell-filter)))
      (if (null end)
	  ;; We've completed reading the output, but didn't finish the line.
	  (setq ispell-filter-continue t continue nil)
	;; skip over newline, this line complete.
	(setq ispell-filter-continue nil end (1+ end))
	(if (= end (length output))	; No more lines in output
	    (setq continue nil)		;  so we can exit the filter.
	  (setq start end))))))		; else move start to next line of input


;;; This function destroys the mark location if it is in the word being
;;; highlighted.
(defun ispell-highlight-spelling-error-generic (start end &optional highlight)
  "Highlight the word from START to END with a kludge using `inverse-video'.
When the optional third arg HIGHLIGHT is set, the word is highlighted;
otherwise it is displayed normally."
  (let ((modified (buffer-modified-p))	; don't allow this fn to modify buffer
	(buffer-read-only nil)		; Allow highlighting read-only buffers.
	(text (buffer-substring start end)) ; Save highlight region
	(inhibit-quit t)		; inhibit interrupt processing here.
	(buffer-undo-list nil))		; don't clutter the undo list.
    (delete-region start end)
    (insert-char ?  (- end start))	; mimimize amount of redisplay
    (sit-for 0)				; update display
    (if highlight (setq inverse-video (not inverse-video))) ; toggle video
    (delete-region start end)		; delete whitespace
    (insert text)			; insert text in inverse video.
    (sit-for 0)				; update display showing inverse video.
    (if highlight (setq inverse-video (not inverse-video))) ; toggle video
    (set-buffer-modified-p modified)))	; don't modify if flag not set.


(defun ispell-highlight-spelling-error-lucid (start end &optional highlight)
  "Highlight the word from START to END using `isearch-highlight'.
When the optional third arg HIGHLIGHT is set, the word is highlighted
otherwise it is displayed normally."
  (if highlight
      (isearch-highlight start end)
    (isearch-dehighlight t))
  ;;(sit-for 0)
  )


(defun ispell-highlight-spelling-error-overlay (start end &optional highlight)
  "Highlight the word from START to END using overlays.
When the optional third arg HIGHLIGHT is set, the word is highlighted
otherwise it is displayed normally.

The variable `ispell-highlight-face' selects the face to use for highlighting."
  (if highlight
      (progn
	(setq ispell-overlay (make-overlay start end))
	(overlay-put ispell-overlay 'face ispell-highlight-face))
    (delete-overlay ispell-overlay)))


;;; Choose a highlight function at load time.
(fset 'ispell-highlight-spelling-error
      (symbol-function
       (cond
	((string-match "Lucid" emacs-version)
	 'ispell-highlight-spelling-error-lucid)
	((and (string-lessp "19" emacs-version) (featurep 'faces))
	 'ispell-highlight-spelling-error-overlay)
	(t 'ispell-highlight-spelling-error-generic))))


(defun ispell-overlay-window (height)
  "Create a window covering the top HEIGHT lines of the current window.
Ensure that the line above point is still visible but otherwise avoid
scrolling the current window.  Leave the old window selected."
  (save-excursion
    (let ((oldot (save-excursion (forward-line -1) (point)))
	  (top (save-excursion (move-to-window-line height) (point))))
      ;; If line above old point (line starting at olddot) would be
      ;; hidden by new window, scroll it to just below new win
      ;; otherwise set top line of other win so it doesn't scroll.
      (if (< oldot top) (setq top oldot))
      ;; NB: Lemacs 19.9 bug: If a window of size N (N includes the mode
      ;; line) is demanded, the last line is not visible.
      ;; At least this happens on AIX 3.2, lemacs w/ Motif, font 9x15.
      ;; So we increment the height for this case.
      (if (string-match "19\.9.*Lucid" (emacs-version))
	  (setq height (1+ height)))
      (split-window nil height)
      (set-window-start (next-window) top))))


;;; Should we add a compound word match return value?
(defun ispell-parse-output (output)
  "Parse the OUTPUT string from Ispell and return:
1: t for an exact match.
2: A string containing the root word for a match via suffix removal.
3: A list of possible correct spellings of the format:
   '(\"ORIGINAL-WORD\" OFFSET MISS-LIST GUESS-LIST)
   ORIGINAL-WORD is a string of the possibly misspelled word.
   OFFSET is an integer giving the line offset of the word.
   MISS-LIST and GUESS-LIST are possibly null lists of guesses and misses."
  (cond
   ((string= output "") t)		; for startup with pipes...
   ((string= output "*") t)		; exact match
   ((string= output "-") t)             ; compound word match
   ((string= (substring output 0 1) "+") ; found cuz of root word
    (substring output 2))		; return root word
   (t					; need to process &, ?, and #'s
    (let ((type (substring output 0 1))	; &, ?, or #
	  (original-word (substring output 2 (string-match " " output 2)))
	  (cur-count 0)			; contains number of misses + guesses
	  count miss-list guess-list offset)
      (setq output (substring output (match-end 0))) ; skip over misspelling
      (if (string= type "#")
	  (setq count 0)		; no misses for type #
	(setq count (string-to-int output) ; get number of misses.
	      output (substring output (1+ (string-match " " output 1)))))
      (setq offset (string-to-int output))
      (if (string= type "#")		; No miss or guess list.
	  (setq output nil)
	(setq output (substring output (1+ (string-match " " output 1)))))
      (while output
	(let ((end (string-match ", \\|\\($\\)" output))) ; end of miss/guess.
	  (setq cur-count (1+ cur-count))
	  (if (> cur-count count)
	      (setq guess-list (cons (substring output 0 end) guess-list))
	    (setq miss-list (cons (substring output 0 end) miss-list)))
	  (if (match-end 1)		; True only when at end of line.
	      (setq output nil)		; no more misses or guesses
	    (setq output (substring output (+ end 2))))))
      (list original-word offset miss-list guess-list)))))


(defun check-ispell-version ()
  ;; This is a little wasteful as we actually launch ispell twice: once
  ;; to make sure it's the right version, and once for real.  But people
  ;; get confused by version mismatches *all* the time (and I've got the
  ;; email to prove it) so I think this is worthwhile.  And the -v[ersion]
  ;; option is the only way I can think of to do this that works with
  ;; all versions, since versions earlier than 3.0.09 didn't identify
  ;; themselves on startup.
  ;;
  (save-excursion
    (set-buffer (get-buffer-create " *ispell-tmp*"))
    (erase-buffer)
    (let ((status (call-process ispell-program-name nil t nil "-v"))
	  (case-fold-search t))
      (goto-char (point-min))
      (cond ((not (memq status '(0 nil)))
	     (error "%s exited with %s %s" ispell-program-name
		    (if (stringp status) "signal" "code") status))
	    ((not (re-search-forward
		   (concat "\\b\\("
			   (mapconcat 'regexp-quote
				      ispell-required-versions
				      "\\|")
			   "\\)\\b")
		   nil t))
	     (error "version mismatch: ispell.el is for %s, %s is %s"
		    (car ispell-required-versions)
		    ispell-program-name
		    (if (re-search-forward "version \\([0-9][0-9.]+\\)\\b"
					   nil t)
			(buffer-substring (match-beginning 1) (match-end 1))
		      "an unknown version"))))
      (kill-buffer (current-buffer)))))


(defun ispell-init-process ()
  "Check status of Ispell process and start if necessary."
  (if (and ispell-process
	   (eq (process-status ispell-process) 'run)
	   ;; If we're using a personal dictionary, assure
	   ;; we're in the same default directory!
	   (or (not ispell-personal-dictionary)
	       (equal ispell-process-directory default-directory)))
      (setq ispell-filter nil ispell-filter-continue nil)
    ;; may need to restart to select new personal dictionary.
    (ispell-kill-ispell t)
    (message "Starting new Ispell process...")
    (sit-for 0)
    (check-ispell-version)
    (setq ispell-process
	  (let ((process-connection-type ispell-use-ptys-p))
	    (apply 'start-process
		   "ispell" nil ispell-program-name
		   "-a"			; accept single input lines
		   "-m"			; make root/affix combos not in dict
		   (let (args)
		     ;; Local dictionary becomes the global dictionary in use.
		     (if ispell-local-dictionary
			 (setq ispell-dictionary ispell-local-dictionary))
		     (setq args (ispell-get-ispell-args))
		     (if ispell-dictionary ; use specified dictionary
			 (setq args
			       (append (list "-d" ispell-dictionary) args)))
		     (if ispell-personal-dictionary ; use specified pers dict
			 (setq args
			       (append args
				       (list "-p"
					     (expand-file-name
					      ispell-personal-dictionary)))))
		     (setq args (append args ispell-extra-args))
		     args)))
	  ispell-filter nil
	  ispell-filter-continue nil
	  ispell-process-directory default-directory)
    (set-process-filter ispell-process 'ispell-filter)
    (accept-process-output ispell-process) ; Get version ID line
    (cond ((null ispell-filter)
	   (error "%s did not output version line"))
	  ((and (null (cdr ispell-filter))
		(stringp (car ispell-filter))
		(string-match "^@(#) " (car ispell-filter)))
	   ;; got the version line as expected (we already know it's the right
	   ;; version, so don't bother checking again.)
	   nil)
	  (t
	   ;; Otherwise, it must be an error message.  Show the user.
	   ;; But first wait to see if some more output is going to arrive.
	   ;; Otherwise we get cool errors like "Can't open ".
	   (sleep-for 1)
	   (accept-process-output)
	   (error "%s" (mapconcat 'identity ispell-filter "\n"))))
    (setq ispell-filter nil)		; Discard version ID line
    (let ((extended-char-mode (ispell-get-extended-character-mode)))
      (if extended-char-mode
	  (process-send-string ispell-process
			       (concat extended-char-mode "\n"))))
    (process-kill-without-query ispell-process)))

;;;###autoload
(defun ispell-kill-ispell (&optional no-error)
  "Kill current Ispell process (so that you may start a fresh one).
With NO-ERROR, just return non-nil if there was no Ispell running."
  (interactive)
  (if (not (and ispell-process
		(eq (process-status ispell-process) 'run)))
      (or no-error
	  (error "There is no ispell process running!"))
    (kill-process ispell-process)
    (setq ispell-process nil)
    (message "Ispell process killed")
    nil))


;;;###autoload
(defun ispell-change-dictionary (dict &optional arg)
  "Change `ispell-dictionary' (q.v.) and kill old Ispell process.
A new one will be started as soon as necessary.

By just answering RET you can find out what the current dictionary is.

With prefix argument, set the default directory."
  (interactive
   (list (completing-read
	  "Use new dictionary (RET for current, SPC to complete): "
	  (cons (cons "default" nil) ispell-dictionary-alist) nil t)
	 current-prefix-arg))
  (if (equal dict "default") (setq dict nil))
  ;; This relies on completing-read's bug of returning "" for no match
  (cond ((equal dict "")
	 (message "Using %s dictionary"
		  (or ispell-local-dictionary ispell-dictionary "default")))
	((and (equal dict ispell-dictionary)
	      (equal dict ispell-local-dictionary))
	 ;; Specified dictionary is the default already.  No-op
	 (message "No change, using %s dictionary" (or dict "default")))
	(t				; reset dictionary!
	 (if (assoc dict ispell-dictionary-alist)
	     (progn
	       (if (or arg (null dict))	; set default dictionary
		   (setq ispell-dictionary dict))
	       (if (null arg)		; set local dictionary
		   (setq ispell-local-dictionary dict)))
	   (error "Illegal dictionary: %s" dict))
	 (ispell-kill-ispell t)
	 (message "(Next %sIspell command will use %s dictionary)"
		  (cond ((equal ispell-local-dictionary ispell-dictionary)
			 "")
			(arg "global ")
			(t "local "))
		  (or (if (or (equal ispell-local-dictionary ispell-dictionary)
			      (null arg))
			  ispell-local-dictionary
			ispell-dictionary)
		      "default")))))


;;; Spelling of comments are checked when ispell-check-comments is non-nil.

;;;###autoload
(defun ispell-region (reg-start reg-end)
  "Interactively check a region for spelling errors."
  (interactive "r")			; Don't flag errors on read-only bufs.
  (ispell-accept-buffer-local-defs)	; set up dictionary, local words, etc.
  (unwind-protect
      (save-excursion
	(message "Spell checking %s..."
		 (if (and (= reg-start (point-min)) (= reg-end (point-max)))
		     (buffer-name) "region"))
	(sit-for 0)
	;; must be top level, not in ispell-command-loop for keeping window.
	(save-window-excursion
	  (if ispell-keep-choices-win
	      (let ((ocb (current-buffer))
		    (window-min-height ispell-choices-win-default-height))
		(or (eq ocb (window-buffer (selected-window)))
		    (error
		     "current buffer is not visible in selected window: %s"
		     ocb))
		;; This keeps the default window size when choices window saved
		(setq ispell-keep-choices-win
		      ispell-choices-win-default-height)
		(ispell-overlay-window ispell-choices-win-default-height)
		(switch-to-buffer (get-buffer-create ispell-choices-buffer))
		(setq mode-line-format "--  %b  --")
		(erase-buffer)
		(select-window (next-window))
		(or (eq (current-buffer) ocb)
		    (error "ispell is confused about current buffer!"))
		(sit-for 0)))
	  (goto-char reg-start)
	  (let ((transient-mark-mode nil))
	    (while (and (not ispell-quit) (< (point) reg-end))
	      (let ((start (point))
		    (offset-change 0)
		    (end (save-excursion (end-of-line) (min (point) reg-end)))
		    (ispell-casechars (ispell-get-casechars))
		    string)
		(cond			; LOOK AT THIS LINE AND SKIP OR PROCESS
		 ((eolp)		; END OF LINE, just go to next line.
		  (forward-char 1))
		 ((and (null ispell-check-comments) ; SKIPING COMMENTS
		       comment-start	; skip comments that start on the line.
		       (search-forward comment-start end t)) ; or found here.
		  (if (= (- (point) start) (length comment-start))
		      ;; comment starts the line.  Skip entire line or region
		      (if (string= "" comment-end) ; skip to next line
			  (beginning-of-line 2)	; or jump to comment end.
			(search-forward comment-end reg-end 'limit))
		    ;; Comment later in line.  Check spelling before comment.
		    (let ((limit (- (point) (length comment-start))))
		      (goto-char (1- limit))
		      (if (looking-at "\\\\") ; "quoted" comment, don't skip
			  ;; quoted comment.  Skip over comment-start
			  (if (= start (1- limit))
			      (setq limit (+ limit (length comment-start)))
			    (setq limit (1- limit))))
		      (goto-char start)
		      ;; Only check when "casechars" or math before comment
		      (if (or (re-search-forward ispell-casechars limit t)
			      (re-search-forward "[][()$]" limit t))
			  (setq string
				(concat "^" (buffer-substring start limit)
					"\n")))
		      (goto-char limit))))
		 ((and ispell-skip-tib	; SKIP TIB REFERENCES!
		       (re-search-forward ispell-tib-ref-beginning end t))
		  (if (= (- (point) 2) start) ; tib ref is 2 chars.
		      ;; Skip to end of tib ref, not necessarily on this line.
		      ;; Return an error if tib ref not found
		      (if (not(re-search-forward ispell-tib-ref-end reg-end t))
			  (progn
			    (ispell-pdict-save ispell-silently-savep)
			    (ding)
			    (message
			     (concat
			      "Open tib reference--set `ispell-skip-tib'"
			      " to nil to avoid this error"))
			    ;; keep cursor at error location
			    (setq ispell-quit (- (point) 2))))
		    ;; tib ref starts later on line. Check spelling before tib.
		    (let ((limit (- (point) 2)))
		      (goto-char start)
		      (if (or (re-search-forward ispell-casechars limit t)
			      (re-search-forward "[][()$]" limit t))
			  (setq string
				(concat "^" (buffer-substring start limit)
					"\n")))
		      (goto-char limit))))
		 ((looking-at "[---#@*+!%~^]") ; SKIP SPECIAL ISPELL CHARACTERS
		  (forward-char 1))
		 ((or (re-search-forward ispell-casechars end t) ; TEXT EXISTS
		      (re-search-forward "[][()$]" end t)) ; or MATH COMMANDS
		  (setq string (concat "^" (buffer-substring start end) "\n"))
		  (goto-char end))
		 (t (beginning-of-line 2))) ; EMPTY LINE, skip it.

		(setq end (point))	; "end" tracks end of region to check.

		(if string		; there is something to spell!
		    (let (poss)
		      ;; send string to spell process and get input.
		      (process-send-string ispell-process string)
		      (while (progn
			       (accept-process-output ispell-process)
			       ;; Last item of output contains a blank line.
			       (not (string= "" (car ispell-filter)))))
		      ;; parse all inputs from the stream one word at a time.
		      ;; Place in FIFO order and remove the blank item.
		      (setq ispell-filter (nreverse (cdr ispell-filter)))
		      (while (and (not ispell-quit) ispell-filter)
			(setq poss (ispell-parse-output (car ispell-filter)))
			(if (listp poss) ; spelling error occurred.
			    (let* ((word-start (+ start offset-change
						  (car (cdr poss))))
				   (word-end (+ word-start
						(length (car poss))))
				   replace)
			      (goto-char word-start)
			      ;; Adjust the horizontal scroll & point
			      (ispell-horiz-scroll)
			      (goto-char word-end)
			      (ispell-horiz-scroll)
			      (goto-char word-start)
			      (ispell-horiz-scroll)
			      (if (/= word-end
				      (progn
					(search-forward (car poss) word-end t)
					(point)))
				  ;; This occurs due to filter pipe problems
				  (error
				   (concat "Ispell misalignment: word "
					   "`%s' point %d; please retry")
				   (car poss) word-start))
			      (unwind-protect
				  (progn
				    (if ispell-highlight-p
					(ispell-highlight-spelling-error
					 word-start word-end t))
				    (sit-for 0)	; update screen display
				    (setq replace (ispell-command-loop
						   (car (cdr (cdr poss)))
						   (car (cdr (cdr (cdr poss))))
						   (car poss))))
				;; protected
				(if ispell-highlight-p
				    (ispell-highlight-spelling-error
				     word-start word-end)))
			      (cond
			       ((and replace (listp replace))
				;; REPLACEMENT WORD entered.  Recheck line
				;; starting with the replacement word.
				(setq ispell-filter nil
				      string (buffer-substring word-start
							       word-end))
				(let ((change (- (length (car replace))
						 (length (car poss)))))
				  ;; adjust regions
				  (setq reg-end (+ reg-end change)
					offset-change (+ offset-change
							 change)))
				(delete-region word-start word-end)
				(insert (car replace))
				;; I only need to recheck typed-in replacements
				(if (not (eq 'query-replace
					     (car (cdr replace))))
				    (backward-char (length (car replace))))
				(setq end (point)) ; reposition for recheck
				;; when second arg exists, query-replace, saving regions
				(if (car (cdr replace))
				    (unwind-protect
					(save-window-excursion
					  (set-marker
					   ispell-query-replace-marker reg-end)
					  ;; Assume case-replace &
					  ;; case-fold-search correct?
					  (query-replace string (car replace)
							 t))
				      (setq reg-end
					    (marker-position
					     ispell-query-replace-marker))
				      (set-marker ispell-query-replace-marker
						  nil))))
			       ((or (null replace)
				    (equal 0 replace)) ; ACCEPT/INSERT
				(if (equal 0 replace) ; BUFFER-LOCAL DICT ADD
				    (setq reg-end
					  (ispell-add-per-file-word-list
					   (car poss) reg-end)))
				;; This avoids pointing out the word that was
				;; just accepted (via 'i' or 'a') if it follows
				;; on the same line.
				;; Redo check following the accepted word.
				(if (and ispell-pdict-modified-p
					 (listp ispell-pdict-modified-p))
				    ;; Word accepted.  Recheck line.
				    (setq ispell-pdict-modified-p ; update flag
					  (car ispell-pdict-modified-p)
					  ispell-filter nil ; discontinue check
					  end word-start))) ; reposition loc.
			       (replace	; STRING REPLACEMENT for this word.
				(delete-region word-start word-end)
				(insert replace)
				(let ((change (- (length replace)
						 (length (car poss)))))
				  (setq reg-end (+ reg-end change)
					offset-change (+ offset-change change)
					end (+ end change)))))
			      (if (not ispell-quit)
				  (message "Continuing spelling check..."))
			      (sit-for 0)))
			;; finished with line!
			(setq ispell-filter (cdr ispell-filter)))))
		(goto-char end)))))
	(not ispell-quit))
    ;; protected
    (if (get-buffer ispell-choices-buffer)
	(kill-buffer ispell-choices-buffer))
    (if ispell-quit
	(progn
	  ;; preserve or clear the region for ispell-continue.
	  (if (not (numberp ispell-quit))
	      (set-marker ispell-region-end nil)
	    ;; Enable ispell-continue.
	    (set-marker ispell-region-end reg-end)
	    (goto-char ispell-quit))
	  ;; Check for aborting
	  (if (and ispell-checking-message (numberp ispell-quit))
	      (progn
		(setq ispell-quit nil)
		(error "Message send aborted.")))
	  (setq ispell-quit nil))
      (set-marker ispell-region-end nil)
      ;; Only save if successful exit.
      (ispell-pdict-save ispell-silently-savep)
      (message "Spell-checking done"))))



;;;###autoload
(defun ispell-buffer ()
  "Check the current buffer for spelling errors interactively."
  (interactive)
  (ispell-region (point-min) (point-max)))


;;;###autoload
(defun ispell-continue ()
  (interactive)
  "Continue a spelling session after making some changes."
  (if (not (marker-position ispell-region-end))
      (message "No session to continue.  Use 'X' command when checking!")
    (if (not (equal (marker-buffer ispell-region-end) (current-buffer)))
	(message "Must continue ispell from buffer %s"
		 (buffer-name (marker-buffer ispell-region-end)))
      (ispell-region (point) (marker-position ispell-region-end)))))


;;; Horizontal scrolling
(defun ispell-horiz-scroll ()
  "Places point within the horizontal visibility of its window area."
  (if truncate-lines			; display truncating lines?
      ;; See if display needs to be scrolled.
      (let ((column (- (current-column) (max (window-hscroll) 1))))
	(if (and (< column 0) (> (window-hscroll) 0))
	    (scroll-right (max (- column) 10))
	  (if (>= column (- (window-width) 2))
	      (scroll-left (max (- column (window-width) -3) 10)))))))


;;; Interactive word completion.
;;; Forces "previous-word" processing.  Do we want to make this selectable?

;;;###autoload
(defun ispell-complete-word (&optional interior-frag)
  "Look up word before or under point in dictionary (see lookup-words command)
and try to complete it.  If optional INTERIOR-FRAG is non-nil then the word
may be a character sequence inside of a word.

Standard ispell choices are then available."
  (interactive "P")
  (let ((cursor-location (point))
	case-fold-search
	ispell-keep-choices-win
	(word (ispell-get-word nil "\\*")) ; force "previous-word" processing.
	start end possibilities replacement)
    (setq start (car (cdr word))
	  end (car (cdr (cdr word)))
	  word (car word)
	  possibilities
	  (or (string= word "")		; Will give you every word
	      (lookup-words (concat (if interior-frag "*") word "*")
			    ispell-complete-word-dict)))
    (cond ((eq possibilities t)
	   (message "No word to complete"))
	  ((null possibilities)
	   (message "No match for \"%s\"" word))
	  (t				; There is a modification...
	   (cond			; Try and respect case of word.
	    ((string-match "^[^A-Z]+$" word)
	     (setq possibilities (mapcar 'downcase possibilities)))
	    ((string-match "^[^a-z]+$" word)
	     (setq possibilities (mapcar 'upcase possibilities)))
	    ((string-match "^[A-Z]" word)
	     (setq possibilities (mapcar 'capitalize possibilities))))
	   (unwind-protect
	       (progn
		 (if ispell-highlight-p	; highlight word
		     (ispell-highlight-spelling-error start end t))
		 (setq replacement
		       (ispell-command-loop possibilities nil word)))
	     ;; protected
	     (if ispell-highlight-p
		 (ispell-highlight-spelling-error start end))) ; un-highlight
	   (cond
	    ((equal 0 replacement)	; BUFFER-LOCAL ADDITION
	     (ispell-add-per-file-word-list word))
	    (replacement		; REPLACEMENT WORD
	     (delete-region start end)
	     (setq word (if (atom replacement) replacement (car replacement))
		   cursor-location (+ (- (length word) (- end start))
				      cursor-location))
	     (insert word)
	     (if (not (atom replacement)) ; recheck spelling of replacement.
		 (progn
		   (goto-char cursor-location)
		   (ispell-word nil t)))))
	   (if (get-buffer ispell-choices-buffer)
	       (kill-buffer ispell-choices-buffer))))
    (ispell-pdict-save ispell-silently-savep)
    (goto-char cursor-location)))


;;;###autoload
(defun ispell-complete-word-interior-frag ()
  "Completes word matching character sequence inside a word."
  (interactive)
  (ispell-complete-word t))


;;; **********************************************************************
;;; 			Ispell Message
;;; **********************************************************************
;;; Original from D. Quinlan, E. Bradford, A. Albert, and M. Ernst


(defvar ispell-message-text-end
  (mapconcat (function identity)
	     '(
	       ;; Matches postscript files.
	       "^%!PS-Adobe-2.0"
	       ;; Matches uuencoded text
	       "^begin [0-9][0-9][0-9] .*\nM.*\nM.*\nM"
	       ;; Matches shell files (esp. auto-decoding)
	       "^#! /bin/sh"
	       ;; Matches difference listing
	       "diff -c .*\n\\*\\*\\* .*\n--- "
	       ;; Matches "----------------- cut here"
	       "^[-=_]+\\s ?cut here")
	     "\\|")
  "*End of text which will be checked in ispell-message.
If it is a string, limit at first occurence of that regular expression.
Otherwise, it must be a function which is called to get the limit.")


;;;###autoload
(defun ispell-message ()
  "Check the spelling of a mail message or news post.
Don't check spelling of message headers except the Subject field.
Don't check included messages.

To abort spell checking of a message REGION and send the message anyway,
use the `x' or `q' command.  (Any subsequent regions will be checked.)
The `X' command aborts the message send so that you can edit the buffer.

To spell-check whenever a message is sent, include the appropriate lines
in your .emacs file:
   (add-hook 'news-inews-hook 'ispell-message)
   (add-hook 'mail-send-hook  'ispell-message)
   (add-hook 'mh-before-send-letter-hook 'ispell-message)

you can bind this to the key C-c i in GNUS or mail by adding to
`news-reply-mode-hook' or `mail-mode-hook' the following lambda expression:
   (function (lambda () (local-set-key \"\\C-ci\" 'ispell-message)))"
  (interactive)
  (save-excursion
    (goto-char (point-min))
    (let* ((internal-messagep (save-excursion
				(re-search-forward
				 (concat "^"
					 (regexp-quote mail-header-separator)
					 "$")
				 nil t)))
	   (limit (copy-marker
		   (cond
		    ((not ispell-message-text-end) (point-max))
		    ((char-or-string-p ispell-message-text-end)
		     (if (re-search-forward ispell-message-text-end nil t)
			 (match-beginning 0)
		       (point-max)))
		    (t (min (point-max) (funcall ispell-message-text-end))))))
	   (cite-regexp			;Prefix of inserted text
	    (cond
	     ((featurep 'supercite)	; sc 3.0
	      (concat "\\(" (sc-cite-regexp) "\\)" "\\|"
		      (ispell-non-empty-string sc-reference-tag-string)))
	     ((featurep 'sc)		; sc 2.3
	      (concat "\\(" sc-cite-regexp "\\)" "\\|"
		      (ispell-non-empty-string sc-reference-tag-string)))
	     ((equal major-mode 'news-reply-mode) ;GNUS
	      (concat "In article <" "\\|"
		      (if mail-yank-prefix
			  (ispell-non-empty-string mail-yank-prefix)
			"^   \\|^\t")))
	     ((equal major-mode 'mh-letter-mode) ; mh mail message
	      (ispell-non-empty-string mh-ins-buf-prefix))
	     ((not internal-messagep)	; Assume n sent us this message.
	      (concat "In [a-zA-Z.]+ you write:" "\\|"
		      "In <[^,;&+=]+> [^,;&+=]+ writes:" "\\|"
		      " *> *"))
	     ((boundp 'vm-included-text-prefix) ; VM mail message
	      (concat "[^,;&+=]+ writes:" "\\|"
		      (ispell-non-empty-string vm-included-text-prefix)))
	     (mail-yank-prefix		; vanilla mail message.
	      (ispell-non-empty-string mail-yank-prefix))
	     (t "^   \\|^\t")))
	   (cite-regexp-start (concat "^[ \t]*$\\|" cite-regexp))
	   (cite-regexp-end   (concat "^\\(" cite-regexp "\\)"))
	   (old-case-fold-search case-fold-search)
	   (case-fold-search t)
	   (ispell-checking-message t))
      (goto-char (point-min))
      ;; Skip header fields except Subject: without Re:'s
      ;;(search-forward mail-header-separator nil t)
      (while (if internal-messagep
		 (< (point) internal-messagep)
	       (and (looking-at "[a-zA-Z---]+:\\|\t\\| ")
		    (not (eobp))))
	(if (looking-at "Subject: *")	; Spell check new subject fields
	    (progn
	      (goto-char (match-end 0))
	      (if (and (not (looking-at ".*Re\\>"))
		       (not (looking-at "\\[")))
		  (let ((case-fold-search old-case-fold-search))
		    (ispell-region (point)
				   (progn
				     (end-of-line)
				     (while (looking-at "\n[ \t]")
				       (end-of-line 2))
				     (point)))))))
	(forward-line 1))
      (setq case-fold-search nil)
      ;; Skip mail header, particularly for non-english languages.
      (if (looking-at mail-header-separator)
	  (forward-line 1))
      (while (< (point) limit)
	;; Skip across text cited from other messages.
	(while (and (looking-at cite-regexp-start)
		    (< (point) limit))
	  (forward-line 1))
	(if (< (point) limit)
	    ;; Check the next batch of lines that *aren't* cited.
	    (let ((end (save-excursion
			 (if (re-search-forward cite-regexp-end limit 'end)
			     (match-beginning 0)
			   (marker-position limit)))))
	      (ispell-region (point) end)
	      (goto-char end))))
      (set-marker limit nil))))


(defun ispell-non-empty-string (string)
  (if (or (not string) (string-equal string ""))
      "\\'\\`" ; An unmatchable string if string is null.
    (regexp-quote string)))


;;; **********************************************************************
;;; 			Buffer Local Functions
;;; **********************************************************************


(defun ispell-accept-buffer-local-defs ()
  "Load all buffer-local information, restarting ispell when necessary."
  (ispell-buffer-local-dict)		; May kill ispell-process.
  (ispell-buffer-local-words)		; Will initialize ispell-process.
  (ispell-buffer-local-parsing))


(defun ispell-buffer-local-parsing ()
  "Place Ispell into parsing mode for this buffer.
Overrides the default parsing mode.
Includes latex/nroff modes and extended character mode."
  ;; (ispell-init-process) must already be called.
  (process-send-string ispell-process "!\n") ; Put process in terse mode.
  ;; We assume all major modes with "tex-mode" in them should use latex parsing
  (if (or (and (eq ispell-parser 'use-mode-name)
	       (string-match "[Tt][Ee][Xx]-mode" (symbol-name major-mode)))
	  (eq ispell-parser 'tex))
      (process-send-string ispell-process "+\n") ; set ispell mode to tex
    (process-send-string ispell-process "-\n"))	; set mode to normal (nroff)
  ;; Set default extended character mode for given buffer, if any.
  (let ((extended-char-mode (ispell-get-extended-character-mode)))
    (if extended-char-mode
	(process-send-string ispell-process (concat extended-char-mode "\n"))))
  ;; Set buffer-local parsing mode and extended charater mode, if specified.
  (save-excursion
    (goto-char (point-min))
    ;; Uses last valid definition
    (while (search-forward ispell-parsing-keyword nil t)
      (let ((end (save-excursion (end-of-line) (point)))
	    (case-fold-search t)
	    string)
	(while (re-search-forward " *\\([^ \"]+\\)" end t)
	  ;; space separated definitions.
	  (setq string (buffer-substring (match-beginning 1) (match-end 1)))
	  (cond ((string-match "latex-mode" string)
		 (process-send-string ispell-process "+\n"))
		((string-match "nroff-mode" string)
		 (process-send-string ispell-process "-\n"))
		((string-match "~" string) ; Set extended character mode.
		 (process-send-string ispell-process (concat string "\n")))
		(t (message "Illegal Ispell Parsing argument!")
		   (sit-for 2))))))))


;;; Can kill the current ispell process

(defun ispell-buffer-local-dict ()
  "Initializes local dictionary.
When a dictionary is defined in the buffer (see variable
`ispell-dictionary-keyword'), it will override the local setting
from \\[ispell-change-dictionary].
Both should not be used to define a buffer-local dictionary."
  (save-excursion
    (goto-char (point-min))
    (let (end)
      ;; Override the local variable definition.
      ;; Uses last valid definition.
      (while (search-forward ispell-dictionary-keyword nil t)
	(setq end (save-excursion (end-of-line) (point)))
	(if (re-search-forward " *\\([^ \"]+\\)" end t)
	    (setq ispell-local-dictionary
		  (buffer-substring (match-beginning 1) (match-end 1)))))
      (goto-char (point-min))
      (while (search-forward ispell-pdict-keyword nil t)
	(setq end (save-excursion (end-of-line) (point)))
	(if (re-search-forward " *\\([^ \"]+\\)" end t)
	    (setq ispell-local-pdict
		  (buffer-substring (match-beginning 1) (match-end 1)))))))
  ;; Reload if new personal dictionary defined.
  (if (and ispell-local-pdict
	   (not (equal ispell-local-pdict ispell-personal-dictionary)))
      (progn
	(ispell-kill-ispell t)
	(setq ispell-personal-dictionary ispell-local-pdict)))
  ;; Reload if new dictionary defined.
  (if (and ispell-local-dictionary
	   (not (equal ispell-local-dictionary ispell-dictionary)))
      (ispell-change-dictionary ispell-local-dictionary)))


(defun ispell-buffer-local-words ()
  "Loads the buffer-local dictionary in the current buffer."
  (if (and ispell-buffer-local-name
	   (not (equal ispell-buffer-local-name (buffer-name))))
      (progn
	(ispell-kill-ispell t)
	(setq ispell-buffer-local-name nil)))
  (ispell-init-process)
  (save-excursion
    (goto-char (point-min))
    (while (search-forward ispell-words-keyword nil t)
      (or ispell-buffer-local-name
	  (setq ispell-buffer-local-name (buffer-name)))
      (let ((end (save-excursion (end-of-line) (point)))
	    string)
	(while (re-search-forward " *\\([^ \"]+\\)" end t)
	  (setq string (buffer-substring (match-beginning 1) (match-end 1)))
	  (process-send-string ispell-process (concat "@" string "\n")))))))


;;; returns optionally adjusted region-end-point.

(defun ispell-add-per-file-word-list (word &optional reg-end)
  "Adds new word to the per-file word list."
  (or ispell-buffer-local-name
      (setq ispell-buffer-local-name (buffer-name)))
  (if (null reg-end)
      (setq reg-end 0))
  (save-excursion
    (goto-char (point-min))
    (let (case-fold-search line-okay search done string)
      (while (not done)
	(setq search (search-forward ispell-words-keyword nil 'move)
	      line-okay (< (+ (length word) 1 ; 1 for space after word..
			      (progn (end-of-line) (current-column)))
			   80))
	(if (or (and search line-okay)
		(null search))
	    (progn
	      (setq done t)
	      (if (null search)
		  (progn
		    (open-line 1)
		    (setq string (concat comment-start " "
					 ispell-words-keyword))
		    ;; in case the keyword is in the middle of the file....
		    (if (> reg-end (point))
			(setq reg-end (+ reg-end (length string))))
		    (insert string)
		    (if (and comment-end (not (equal "" comment-end)))
			(save-excursion
			  (open-line 1)
			  (forward-line 1)
			  (insert comment-end)))))
	      (if (> reg-end (point))
		  (setq reg-end (+ 1 reg-end (length word))))
	      (insert (concat " " word)))))))
  reg-end)


(defconst ispell-version "2.30 -- Fri May 20 15:58:52 MDT 1994")

(provide 'ispell)


;;; LOCAL VARIABLES AND BUFFER-LOCAL VALUE EXAMPLES.

;;; Local Variable options:
;;; mode: name(-mode)
;;; eval: expression
;;; local-variable: value

;;; The following sets the buffer local dictionary to english!

;;; Local Variables:
;;; mode: emacs-lisp
;;; comment-column: 40
;;; ispell-local-dictionary: "english"
;;; End:


;;; MORE EXAMPLES OF ISPELL BUFFER-LOCAL VALUES

;;; The following places this file in nroff parsing and extended char modes.
;;; Local IspellParsing: nroff-mode ~nroff
;;; Change IspellDict to IspellDict: to enable the following line.
;;; Local IspellDict english
;;; Change IspellPersDict to IspellPersDict: to enable the following line.
;;; Local IspellPersDict ~/.ispell_lisp
;;; The following were automatically generated by ispell using the 'A' command:
; LocalWords:  ispell ispell-highlight-p ispell-check-comments query-replace
; LocalWords:  ispell-query-replace-choices ispell-skip-tib non-nil tib
; LocalWords:  regexps ispell-tib-ref-beginning ispell-tib-ref-end

;; ispell.el ends here
