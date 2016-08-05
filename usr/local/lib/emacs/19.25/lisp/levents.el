;;; levents.el --- emulate the Lucid event data type and associated functions.

;; Copyright (C) 1993 Free Software Foundation, Inc.

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

;; Things we cannot emulate in Lisp:
;; It is not possible to emulate current-mouse-event as a variable,
;; though it is not hard to obtain the data from (this-command-keys).

;; We do not have a variable unread-command-event;
;; instead, we have the more general unread-command-events.

;; Our read-key-sequence and read-char are not precisely
;; compatible with those in Lucid Emacs, but they should work ok.

;;; Code:

(defun next-command-event (event)
  (error "You must rewrite to use `read-command-event' instead of `next-command-event'"))

(defun next-event (event)
  (error "You must rewrite to use `read-event' instead of `next-event'"))

(defun dispatch-event (event)
  (error "`dispatch-event' not supported"))

;; Make events of type eval, menu and timeout
;; execute properly.

(define-key global-map [menu] 'execute-eval-event)
(define-key global-map [timeout] 'execute-eval-event)
(define-key global-map [eval] 'execute-eval-event)

(defun execute-eval-event (event)
  (interactive "e")
  (funcall (nth 1 event) (nth 2 event)))

(put 'eval 'event-symbol-elements '(eval))
(put 'menu 'event-symbol-elements '(eval))
(put 'timeout 'event-symbol-elements '(eval))

(defsubst eventp (obj)
  "True if the argument is an event object."
  (or (integerp obj)
      (and (symbolp obj)
	   (get obj 'event-symbol-elements))
      (and (consp obj)
	   (symbolp (car obj))
	   (get (car obj) 'event-symbol-elements))))

(defun allocate-event ()
  "Returns an empty event structure.
In this emulation, it returns nil."
  nil)

(defun button-press-event-p (obj)
  "True if the argument is a mouse-button-press event object."
  (and (consp obj) (symbolp (car obj))
       (memq 'down (get (car obj) 'event-symbol-elements))))

(defun button-release-event-p (obj)
  "True if the argument is a mouse-button-release event object."
  (and (consp obj) (symbolp (car obj))
       (or (memq 'click (get (car obj) 'event-symbol-elements))
	   (memq 'drag (get (car obj) 'event-symbol-elements)))))

(defun character-to-event (ch &optional event)
  "Converts a numeric ASCII value to an event structure, replete with
bucky bits.  The character is the first argument, and the event to fill
in is the second.  This function contains knowledge about what the codes
mean -- for example, the number 9 is converted to the character Tab,
not the distinct character Control-I.

Beware that character-to-event and event-to-character are not strictly 
inverse functions, since events contain much more information than the 
ASCII character set can encode."
  ch)

(defun copy-event (event1 &optional event2)
  "Make a copy of the given event object.
In this emulation, `copy-event' just returns its argument."
  event1)

(defun deallocate-event (event)
  "Allow the given event structure to be reused.
In actual Lucid Emacs, you MUST NOT use this event object after
calling this function with it.  You will lose.  It is not necessary to
call this function, as event objects are garbage- collected like all
other objects; however, it may be more efficient to explicitly
deallocate events when you are sure that that is safe.

This emulation does not actually deallocate or reuse events
except via garbage collection and `cons'."
  nil)

(defun enqueue-eval-event: (function object)
  "Add an eval event to the back of the queue.
It will be the next event read after all pending events."
  (setq unread-command-events
	(nconc unread-command-events
	       (list (list 'eval function object)))))

(defun eval-event-p (obj)
  "True if the argument is an eval or menu event object."
  (eq (car-safe obj) 'eval))

(defun event-button (event)
  "Return the button-number of the given mouse-button-press event."
  (let ((sym (car (get (car event) 'event-symbol-elements))))
    (cdr (assq sym '((mouse-1 . 1) (mouse-2 . 2) (mouse-3 . 3)
		     (mouse-4 . 4) (mouse-5 . 5))))))

(defun event-function (event)
  "Return the callback function of the given timeout, menu, or eval event."
  (nth 1 event))

(defun event-key (event)
  "Returns the KeySym of the given key-press event.
The value is an ASCII printing character (not upper case) or a symbol."
  (if (symbolp event)
      (car (get event 'event-symbol-elements))
    (let ((base (logand event (1- (lsh 1 18)))))
      (downcase (if (< base 32) (logior base 64) base)))))

(defun event-object (event)
  "Returns the function argument of the given timeout, menu, or eval event."
  (nth 2 event))

(defun event-point (event)
  "Returns the character position of the given mouse-related event.
If the event did not occur over a window, or did
not occur over text, then this returns nil.  Otherwise, it returns an index
into the buffer visible in the event's window."
  (posn-point (event-end event)))

(defun event-process (event)
  "Returns the process of the given process-output event."
  (nth 1 event))

(defun event-timestamp (event)
  "Returns the timestamp of the given event object.
In Lucid Emacs, this works for any kind of event.
In this emulation, it returns nil for non-mouse-related events."
  (and (listp event)
       (posn-timestamp (event-end event))))

(defun event-to-character (event &optional lenient)
  "Returns the closest ASCII approximation to the given event object.
If the event isn't a keypress, this returns nil.
If the second argument is non-nil, then this is lenient in its 
translation; it will ignore modifier keys other than control and meta,
and will ignore the shift modifier on those characters which have no 
shifted ASCII equivalent (Control-Shift-A for example, will be mapped to 
the same ASCII code as Control-A.)  If the second arg is nil, then nil 
will be returned for events which have no direct ASCII equivalent."
  (if (symbolp event)
      (and lenient
	   (cdr (assq event '((backspace . 8) (delete . 127) (tab . 9)
			      (return . 10) (enter . 10)))))
    ;; Our interpretation is, ASCII means anything a number can represent.
    (if (integerp event)
	event nil)))

(defun event-window (event)
  "Returns the window of the given mouse-related event object."
  (posn-window (event-end event)))

(defun event-x (event)
  "Returns the X position in characters of the given mouse-related event."
  (/ (car (posn-col-row (event-end event)))
     (frame-char-width (window-frame (event-window event)))))

(defun event-x-pixel (event)
  "Returns the X position in pixels of the given mouse-related event."
  (car (posn-col-row (event-end event))))

(defun event-y (event)
  "Returns the Y position in characters of the given mouse-related event."
  (/ (cdr (posn-col-row (event-end event)))
     (frame-char-height (window-frame (event-window event)))))

(defun event-y-pixel (event)
  "Returns the Y position in pixels of the given mouse-related event."
  (cdr (posn-col-row (event-end event))))

(defun key-press-event-p (obj)
  "True if the argument is a keyboard event object."
  (or (integerp obj)
      (and (symbolp obj)
	   (get obj 'event-symbol-elements))))

(defun menu-event-p (obj)
  "True if the argument is a menu event object."
  (eq (car-safe obj) 'menu))

(defun motion-event-p (obj)
  "True if the argument is a mouse-motion event object."
  (eq (car-safe obj) 'mouse-movement))

(defun read-command-event ()
  "Return the next keyboard or mouse event; execute other events.
This is similar to the function `next-command-event' of Lucid Emacs,
but different in that it returns the event rather than filling in
an existing event object."
  (let (event)
    (while (progn
	     (setq event (read-event))
	     (not (or (key-press-event-p event)
		      (button-press-event-p event)
		      (button-release-event-p event)
		      (menu-event-p event))))
      (let ((type (car-safe event)))
	(cond ((eq type 'eval)
	       (funcall (nth 1 event) (nth 2 event)))
	      ((eq type 'switch-frame)
	       (select-frame (nth 1 event))))))
    event))

(defun process-event-p (obj)
  "True if the argument is a process-output event object.
GNU Emacs 19 does not currently generate process-output events."
  (eq (car-safe obj) 'process))

(defun timeout-event-p (obj)
  "True if the argument is a timeout event object.
GNU Emacs 19 does not currently generate timeout events."
  (eq (car-safe obj) 'timeout))

;;; levents.el ends here
