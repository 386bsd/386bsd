;;; time.el --- display time and load in mode line of Emacs.

;; Copyright (C) 1985, 1986, 1987, 1993, 1994 Free Software Foundation, Inc.

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

;;; Facilities to display current time/date and a new-mail indicator
;;; in the Emacs mode line.  The single entry point is `display-time'.

;;; Code:

(defvar display-time-mail-file nil
  "*File name of mail inbox file, for indicating existence of new mail.
Default is system-dependent, and is the same as used by Rmail.")

;;;###autoload
(defvar display-time-day-and-date nil "\
*Non-nil means \\[display-time] should display day and date as well as time.")

(defvar display-time-process nil)

(defvar display-time-interval 60
  "*Seconds between updates of time in the mode line.")

(defvar display-time-24hr-format nil
  "*Non-nill indicates time should be displayed as hh:mm, 0 <= hh <= 23.
Nil means 1 <= hh <= 12, and an AM/PM suffix is used.")

(defvar display-time-string nil)

(defvar display-time-hook nil
  "* List of functions to be called when the time is updated on the mode line.")

(defvar display-time-server-down-time nil
   "Time when mail file's file system was recorded to be down.
If that file system seems to be up, the value is nil.")

;;;###autoload
(defun display-time ()
  "Display current time, load level, and mail flag in mode line of each buffer.
Updates automatically every minute.
If `display-time-day-and-date' is non-nil, the current day and date
are displayed as well.
After each update, `display-time-hook' is run with `run-hooks'."
  (interactive)
  (let ((live (and display-time-process
		   (eq (process-status display-time-process) 'run))))
    (if (not live)
	(progn
	  (if display-time-process
	      (delete-process display-time-process))
	  (or global-mode-string (setq global-mode-string '("")))
	  (or (memq 'display-time-string global-mode-string)
	      (setq global-mode-string
		    (append global-mode-string '(display-time-string))))
	  (setq display-time-string "")
	  ;; Using a pty is wasteful, and the separate session causes
	  ;; annoyance sometimes (some systems kill idle sessions).
	  (let ((process-connection-type nil))
	    (setq display-time-process
		  (start-process "display-time" nil
				 (expand-file-name "wakeup" exec-directory)
				 (int-to-string display-time-interval))))
	  (process-kill-without-query display-time-process)
	  (set-process-sentinel display-time-process 'display-time-sentinel)
	  (set-process-filter display-time-process 'display-time-filter)))))

(defun display-time-sentinel (proc reason)
  (or (eq (process-status proc) 'run)
      (setq display-time-string ""))
  ;; Force mode-line updates
  (save-excursion (set-buffer (other-buffer)))
  (set-buffer-modified-p (buffer-modified-p))
  (sit-for 0))

(defun display-time-filter (proc string)
  (let ((time (current-time-string))
	(load (condition-case ()
		  (if (zerop (car (load-average))) ""
		    (let ((str (format " %03d" (car (load-average)))))
		      (concat (substring str 0 -2) "." (substring str -2))))
		(error "")))
	(mail-spool-file (or display-time-mail-file
			     (getenv "MAIL")
			     (concat rmail-spool-directory
				     (user-login-name))))
	hour am-pm-flag mail-flag)
    (setq hour (read (substring time 11 13)))
    (if (not display-time-24hr-format)
	(progn
	  (setq am-pm-flag (if (>= hour 12) "pm" "am"))
	  (if (> hour 12)
	      (setq hour (- hour 12))
	    (if (= hour 0)
		(setq hour 12))))
      (setq am-pm-flag ""))
    (setq mail-flag
	  (if (and (or (null display-time-server-down-time)
		       ;; If have been down for 20 min, try again.
		       (> (- (nth 1 (current-time))
			     display-time-server-down-time)
			  1200))
		   (let ((start-time (current-time)))
		     (prog1
			 (display-time-file-nonempty-p mail-spool-file)
		       (if (> (- (nth 1 (current-time)) (nth 1 start-time))
			      20)
			   ;; Record that mail file is not accessible.
			   (setq display-time-server-down-time 
				 (nth 1 (current-time)))
			 ;; Record that mail file is accessible.
			 (setq display-time-server-down-time nil))
		       )))
	      " Mail"
	    ""))
    (setq display-time-string
	  (concat (format "%d" hour) (substring time 13 16)
		  am-pm-flag
		  load
		  mail-flag))
    ;; Append the date if desired.
    (if display-time-day-and-date
	(setq display-time-string
	      (concat (substring time 0 11) display-time-string))))
  (run-hooks 'display-time-hook)
  ;; Force redisplay of all buffers' mode lines to be considered.
  (save-excursion (set-buffer (other-buffer)))
  (set-buffer-modified-p (buffer-modified-p))
  ;; Do redisplay right now, if no input pending.
  (sit-for 0))

(defun display-time-file-nonempty-p (file)
  (and (file-exists-p file)
       (< 0 (nth 7 (file-attributes (file-chase-links file))))))

;;; time.el ends here
