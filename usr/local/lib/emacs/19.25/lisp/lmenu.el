;;; lmenu.el --- emulate Lucid's menubar support

;; Keywords: emulations

;; Copyright (C) 1992, 1993, 1994 Free Software Foundation, Inc.

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

;;; Code:


;; First, emulate the Lucid menubar support in GNU Emacs 19.

;; Arrange to use current-menubar to set up part of the menu bar.

(defvar current-menubar)

(setq recompute-lucid-menubar 'recompute-lucid-menubar)
(defun recompute-lucid-menubar ()
  (define-key lucid-menubar-map [menu-bar]
    (condition-case nil
	(make-lucid-menu-keymap "menu-bar" current-menubar)
      (error (message "Invalid data in current-menubar moved to lucid-failing-menubar")
	     (sit-for 1)
	     (setq lucid-failing-menubar current-menubar
		   current-menubar nil))))
  (setq lucid-menu-bar-dirty-flag nil))

(defvar lucid-menubar-map (make-sparse-keymap))
(or (assq 'current-menubar minor-mode-map-alist)
    (setq minor-mode-map-alist
	  (cons (cons 'current-menubar lucid-menubar-map)
		minor-mode-map-alist)))

(defun set-menubar-dirty-flag ()
  (force-mode-line-update)
  (setq lucid-menu-bar-dirty-flag t))

(defvar add-menu-item-count 0)

;; Return a menu keymap corresponding to a Lucid-style menu list
;; MENU-ITEMS, and with name MENU-NAME.
(defun make-lucid-menu-keymap (menu-name menu-items)
  (let ((menu (make-sparse-keymap menu-name)))
    ;; Process items in reverse order,
    ;; since the define-key loop reverses them again.
    (setq menu-items (reverse menu-items))
    (while menu-items
      (let* ((item (car menu-items))
	     (callback (if (vectorp item) (aref item 1)))
	     command name)
	(cond ((stringp item)
	       (setq command nil)
	       (setq name (if (string-match "^-+$" item) "" item)))
	      ((consp item)
	       (setq command (make-lucid-menu-keymap (car item) (cdr item)))
	       (setq name (car item)))
	      ((vectorp item)
	       (setq command (make-symbol (format "menu-function-%d"
						  add-menu-item-count)))
	       (setq add-menu-item-count (1+ add-menu-item-count))
	       (put command 'menu-enable (aref item 2))
	       (setq name (aref item 0))	       
	       (if (symbolp callback)
		   (fset command callback)
		 (fset command (list 'lambda () '(interactive) callback)))))
	(if (null command)
	    ;; Handle inactive strings specially--allow any number
	    ;; of identical ones.
	    (setcdr menu (cons (list nil name) (cdr menu)))
	  (if name 
	      (define-key menu (vector (intern name)) (cons name command)))))
      (setq menu-items (cdr menu-items)))
    menu))

(defun popup-menu (menu-desc)
  "Pop up the given menu.
A menu is a list of menu items, strings, and submenus.

The first element of a menu must be a string, which is the name of the
menu.  This is the string that will be displayed in the parent menu, if
any.  For toplevel menus, it is ignored.  This string is not displayed
in the menu itself.

A menu item is a vector of three or four elements:

 - the name of the menu item (a string);
 - the `callback' of that item;
 - whether this item is active (selectable);
 - and an optional string to append to the name.

If the `callback' of a menu item is a symbol, then it must name a command.
It will be invoked with `call-interactively'.  If it is a list, then it is
evaluated with `eval'.

The fourth element of a menu item is a convenient way of adding the name
of a command's ``argument'' to the menu, like ``Kill Buffer NAME''.

If an element of a menu is a string, then that string will be presented in
the menu as unselectable text.

If an element of a menu is a string consisting solely of hyphens, then that
item will be presented as a solid horizontal line.

If an element of a menu is a list, it is treated as a submenu.  The name of
that submenu (the first element in the list) will be used as the name of the
item representing this menu on the parent.

The syntax, more precisely:

   form		:=  <something to pass to `eval'>
   command	:=  <a symbol or string, to pass to `call-interactively'>
   callback 	:=  command | form
   active-p	:=  <t or nil, whether this thing is selectable>
   text		:=  <string, non selectable>
   name		:=  <string>
   argument	:=  <string>
   menu-item	:=  '['  name callback active-p [ argument ]  ']'
   menu		:=  '(' name [ menu-item | menu | text ]+ ')'
"
  (let ((menu (make-lucid-menu-keymap (car menu-desc) (cdr menu-desc)))
	(pos (mouse-pixel-position))
	answer cmd)
    (while menu
      (setq answer (x-popup-menu (list (list (nth 1 pos) (nthcdr 2 pos))
				       (car pos))
				 menu))
      (setq cmd (lookup-key menu (vector answer)))
      (setq menu nil)
      (and cmd
	   (if (keymapp cmd)
	       (setq menu cmd)
	     (call-interactively cmd))))))

(defun popup-dialog-box (data)
  "Pop up a dialog box.
A dialog box description is a list.

 - The first element of the list is a string to display in the dialog box.
 - The rest of the elements are descriptions of the dialog box's buttons.
   Each one is a vector of three elements:
   - The first element is the text of the button.
   - The second element is the `callback'.
   - The third element is t or nil, whether this button is selectable.

If the `callback' of a button is a symbol, then it must name a command.
It will be invoked with `call-interactively'.  If it is a list, then it is
evaluated with `eval'.

One (and only one) of the buttons may be `nil'.  This marker means that all
following buttons should be flushright instead of flushleft.

The syntax, more precisely:

   form		:=  <something to pass to `eval'>
   command	:=  <a symbol or string, to pass to `call-interactively'>
   callback 	:=  command | form
   active-p	:=  <t, nil, or a form to evaluate to decide whether this
		    button should be selectable>
   name		:=  <string>
   partition	:=  'nil'
   button	:=  '['  name callback active-p ']'
   dialog	:=  '(' name [ button ]+ [ partition [ button ]+ ] ')'"
  (let ((name (car data))
	(tail (cdr data))
	converted
	choice meaning)
    (while tail
      (if (null (car tail))
	  (setq converted (cons nil converted))
	(let ((item (aref (car tail) 0))
	      (callback (aref (car tail) 1))
	      (enable (aref (car tail) 2)))
	  (setq converted
		(cons (if enable (cons item callback) item)
		      converted))))
      (setq tail (cdr tail)))
    (setq choice (x-popup-dialog t (cons name (nreverse converted))))
    (setq meaning (assq choice converted))
    (if meaning
	(if (symbolp (cdr meaning))
	    (call-interactively (cdr meaning))
	  (eval (cdr meaning))))))

;; This is empty because the usual elements of the menu bar 
;; are provided by menu-bar.el instead.
;; It would not make sense to duplicate them here.
(defconst default-menubar nil)

(defun set-menubar (menubar)
  "Set the default menubar to be menubar."
  (setq-default current-menubar (copy-sequence menubar))
  (set-menubar-dirty-flag))

(defun set-buffer-menubar (menubar)
  "Set the buffer-local menubar to be menubar."
  (make-local-variable 'current-menubar)
  (setq current-menubar (copy-sequence menubar))
  (set-menubar-dirty-flag))


;;; menu manipulation functions

(defun find-menu-item (menubar item-path-list &optional parent)
  "Searches MENUBAR for item given by ITEM-PATH-LIST.
Returns (ITEM . PARENT), where PARENT is the immediate parent of
 the item found.
Signals an error if the item is not found."
  (or parent (setq item-path-list (mapcar 'downcase item-path-list)))
  (if (not (consp menubar))
      nil
    (let ((rest menubar)
	  result)
      (while rest
	(if (and (car rest)
		 (equal (car item-path-list)
			(downcase (if (vectorp (car rest))
				      (aref (car rest) 0)
				    (if (stringp (car rest))
					(car rest)
				      (car (car rest)))))))
	    (setq result (car rest) rest nil)
	  (setq rest (cdr rest))))
      (if (cdr item-path-list)
	  (if (consp result)
	      (find-menu-item (cdr result) (cdr item-path-list) result)
	    (if result
		(signal 'error (list "not a submenu" result))
	      (signal 'error (list "no such submenu" (car item-path-list)))))
	(cons result parent)))))


(defun disable-menu-item (path)
  "Make the named menu item be unselectable.
PATH is a list of strings which identify the position of the menu item in 
the menu hierarchy.  (\"File\" \"Save\") means the menu item called \"Save\"
under the toplevel \"File\" menu.  (\"Menu\" \"Foo\" \"Item\") means the 
menu item called \"Item\" under the \"Foo\" submenu of \"Menu\"."
  (let* ((menubar current-menubar)
	 (pair (find-menu-item menubar path))
	 (item (car pair))
	 (menu (cdr pair)))
    (or item
	(signal 'error (list (if menu "No such menu item" "No such menu")
			     path)))
    (if (consp item) (error "can't disable menus, only menu items"))
    (aset item 2 nil)
    (set-menubar-dirty-flag)
    item))


(defun enable-menu-item (path)
  "Make the named menu item be selectable.
PATH is a list of strings which identify the position of the menu item in 
the menu hierarchy.  (\"File\" \"Save\") means the menu item called \"Save\"
under the toplevel \"File\" menu.  (\"Menu\" \"Foo\" \"Item\") means the 
menu item called \"Item\" under the \"Foo\" submenu of \"Menu\"."
  (let* ((menubar current-menubar)
	 (pair (find-menu-item menubar path))
	 (item (car pair))
	 (menu (cdr pair)))
    (or item
	(signal 'error (list (if menu "No such menu item" "No such menu")
			     path)))
    (if (consp item) (error "%S is a menu, not a menu item" path))
    (aset item 2 t)
    (set-menubar-dirty-flag)
    item))


(defun add-menu-item-1 (item-p menu-path item-name item-data enabled-p before)
  (if before (setq before (downcase before)))
  (let* ((menubar current-menubar)
	 (menu (condition-case ()
		   (car (find-menu-item menubar menu-path))
		 (error nil)))
	 (item (if (listp menu)
		   (car (find-menu-item (cdr menu) (list item-name)))
		 (signal 'error (list "not a submenu" menu-path)))))
    (or menu
	(let ((rest menu-path)
	      (so-far menubar))
	  (while rest
;;;	    (setq menu (car (find-menu-item (cdr so-far) (list (car rest)))))
	    (setq menu
		  (if (eq so-far menubar)
		      (car (find-menu-item so-far (list (car rest))))
		    (car (find-menu-item (cdr so-far) (list (car rest))))))
	    (or menu
		(let ((rest2 so-far))
		  (while (and (cdr rest2) (car (cdr rest2)))
		    (setq rest2 (cdr rest2)))
		  (setcdr rest2
			  (nconc (list (setq menu (list (car rest))))
				 (cdr rest2)))))
	    (setq so-far menu)
	    (setq rest (cdr rest)))))
    (or menu (setq menu menubar))
    (if item
	nil	; it's already there
      (if item-p
	  (setq item (vector item-name item-data enabled-p))
	(setq item (cons item-name item-data)))
      ;; if BEFORE is specified, try to add it there.
      (if before
	  (setq before (car (find-menu-item menu (list before)))))
      (let ((rest menu)
	    (added-before nil))
	(while rest
	  (if (eq before (car (cdr rest)))
	      (progn
		(setcdr rest (cons item (cdr rest)))
		(setq rest nil added-before t))
	    (setq rest (cdr rest))))
	(if (not added-before)
	    ;; adding before the first item on the menubar itself is harder
	    (if (and (eq menu menubar) (eq before (car menu)))
		(setq menu (cons item menu)
		      current-menubar menu)
	      ;; otherwise, add the item to the end.
	      (nconc menu (list item))))))
    (if item-p
	(progn
	  (aset item 1 item-data)
	  (aset item 2 (not (null enabled-p))))
      (setcar item item-name)
      (setcdr item item-data))
    (set-menubar-dirty-flag)
    item))

(defun add-menu-item (menu-path item-name function enabled-p &optional before)
  "Add a menu item to some menu, creating the menu first if necessary.
If the named item exists already, it is changed.
MENU-PATH identifies the menu under which the new menu item should be inserted.
 It is a list of strings; for example, (\"File\") names the top-level \"File\"
 menu.  (\"File\" \"Foo\") names a hypothetical submenu of \"File\".
ITEM-NAME is the string naming the menu item to be added.
FUNCTION is the command to invoke when this menu item is selected.
 If it is a symbol, then it is invoked with `call-interactively', in the same
 way that functions bound to keys are invoked.  If it is a list, then the 
 list is simply evaluated.
ENABLED-P controls whether the item is selectable or not.
BEFORE, if provided, is the name of a menu item before which this item should
 be added, if this item is not on the menu already.  If the item is already
 present, it will not be moved."
  (or menu-path (error "must specify a menu path"))
  (or item-name (error "must specify an item name"))
  (add-menu-item-1 t menu-path item-name function enabled-p before))


(defun delete-menu-item (path)
  "Remove the named menu item from the menu hierarchy.
PATH is a list of strings which identify the position of the menu item in 
the menu hierarchy.  (\"File\" \"Save\") means the menu item called \"Save\"
under the toplevel \"File\" menu.  (\"Menu\" \"Foo\" \"Item\") means the 
menu item called \"Item\" under the \"Foo\" submenu of \"Menu\"."
  (let* ((menubar current-menubar)
	 (pair (find-menu-item menubar path))
	 (item (car pair))
	 (menu (or (cdr pair) menubar)))
    (if (not item)
	nil
      ;; the menubar is the only special case, because other menus begin
      ;; with their name.
      (if (eq menu current-menubar)
	  (setq current-menubar (delq item menu))
	(delq item menu))
      (set-menubar-dirty-flag)
      item)))


(defun relabel-menu-item (path new-name)
  "Change the string of the specified menu item.
PATH is a list of strings which identify the position of the menu item in 
the menu hierarchy.  (\"File\" \"Save\") means the menu item called \"Save\"
under the toplevel \"File\" menu.  (\"Menu\" \"Foo\" \"Item\") means the 
menu item called \"Item\" under the \"Foo\" submenu of \"Menu\".
NEW-NAME is the string that the menu item will be printed as from now on."
  (or (stringp new-name)
      (setq new-name (signal 'wrong-type-argument (list 'stringp new-name))))
  (let* ((menubar current-menubar)
	 (pair (find-menu-item menubar path))
	 (item (car pair))
	 (menu (cdr pair)))
    (or item
	(signal 'error (list (if menu "No such menu item" "No such menu")
			     path)))
    (if (and (consp item)
	     (stringp (car item)))
	(setcar item new-name)
      (aset item 0 new-name))
    (set-menubar-dirty-flag)
    item))

(defun add-menu (menu-path menu-name menu-items &optional before)
  "Add a menu to the menubar or one of its submenus.
If the named menu exists already, it is changed.
MENU-PATH identifies the menu under which the new menu should be inserted.
 It is a list of strings; for example, (\"File\") names the top-level \"File\"
 menu.  (\"File\" \"Foo\") names a hypothetical submenu of \"File\".
 If MENU-PATH is nil, then the menu will be added to the menubar itself.
MENU-NAME is the string naming the menu to be added.
MENU-ITEMS is a list of menu item descriptions.
 Each menu item should be a vector of three elements:
   - a string, the name of the menu item;
   - a symbol naming a command, or a form to evaluate;
   - and a form whose value determines whether this item is selectable.
BEFORE, if provided, is the name of a menu before which this menu should
 be added, if this menu is not on its parent already.  If the menu is already
 present, it will not be moved."
  (or menu-name (error "must specify a menu name"))
  (or menu-items (error "must specify some menu items"))
  (add-menu-item-1 nil menu-path menu-name menu-items t before))



(defvar put-buffer-names-in-file-menu t)


;; Don't unconditionally enable menu bars; leave that up to the user.
;;(let ((frames (frame-list)))
;;  (while frames
;;    (modify-frame-parameters (car frames) '((menu-bar-lines . 1)))
;;    (setq frames (cdr frames))))
;;(or (assq 'menu-bar-lines default-frame-alist)
;;    (setq default-frame-alist
;;	  (cons '(menu-bar-lines . 1) default-frame-alist)))

(set-menubar default-menubar)

(provide 'lmenu)

;;; lmenu.el ends here
