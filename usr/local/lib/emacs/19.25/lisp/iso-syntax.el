;;; iso-syntax.el --- set up case-conversion and syntax tables for ISO 8859/1

;; Copyright (C) 1988 Free Software Foundation, Inc.

;; Author: Howard Gayle
;; Maintainer: FSF
;; Keywords: i18n

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

;; Written by Howard Gayle.  See case-table.el for details.

;;; Code:

(require 'case-table)

(let ((downcase (concat (car (standard-case-table)))))
  (set-case-syntax 160 " " downcase)	; NBSP (no-break space)
  (set-case-syntax 161 "." downcase)	; inverted exclamation mark
  (set-case-syntax 162 "w" downcase)	; cent sign
  (set-case-syntax 163 "w" downcase)	; pound sign
  (set-case-syntax 164 "w" downcase)	; general currency sign
  (set-case-syntax 165 "w" downcase)	; yen sign
  (set-case-syntax 166 "_" downcase)	; broken vertical line
  (set-case-syntax 167 "w" downcase)	; section sign
  (set-case-syntax 168 "w" downcase)	; diaeresis
  (set-case-syntax 169 "_" downcase)	; copyright sign
  (set-case-syntax 170 "w" downcase)	; ordinal indicator, feminine
  (set-case-syntax-delims 171 187 downcase) ; angle quotation marks
  (set-case-syntax 172 "_" downcase)	; not sign
  (set-case-syntax 173 "_" downcase)	; soft hyphen
  (set-case-syntax 174 "_" downcase)	; registered sign
  (set-case-syntax 175 "w" downcase)	; macron
  (set-case-syntax 176 "_" downcase)	; degree sign
  (set-case-syntax 177 "_" downcase)	; plus or minus sign
  (set-case-syntax 178 "w" downcase)	; superscript two
  (set-case-syntax 179 "w" downcase)	; superscript three
  (set-case-syntax 180 "w" downcase)	; acute accent
  (set-case-syntax 181 "_" downcase)	; micro sign
  (set-case-syntax 182 "w" downcase)	; pilcrow
  (set-case-syntax 183 "_" downcase)	; middle dot
  (set-case-syntax 184 "w" downcase)	; cedilla
  (set-case-syntax 185 "w" downcase)	; superscript one
  (set-case-syntax 186 "w" downcase)	; ordinal indicator, masculine
  ;;    	       	      187          ; See 171 above.
  (set-case-syntax 188 "_" downcase)	; fraction one-quarter
  (set-case-syntax 189 "_" downcase)	; fraction one-half
  (set-case-syntax 190 "_" downcase)	; fraction three-quarters
  (set-case-syntax 191 "." downcase)	; inverted question mark
  (set-case-syntax-pair 192 224 downcase) ; A with grave accent
  (set-case-syntax-pair 193 225 downcase) ; A with acute accent
  (set-case-syntax-pair 194 226 downcase) ; A with circumflex accent
  (set-case-syntax-pair 195 227 downcase) ; A with tilde
  (set-case-syntax-pair 196 228 downcase) ; A with diaeresis or umlaut mark
  (set-case-syntax-pair 197 229 downcase) ; A with ring
  (set-case-syntax-pair 198 230 downcase) ; AE diphthong
  (set-case-syntax-pair 199 231 downcase) ; C with cedilla
  (set-case-syntax-pair 200 232 downcase) ; E with grave accent
  (set-case-syntax-pair 201 233 downcase) ; E with acute accent
  (set-case-syntax-pair 202 234 downcase) ; E with circumflex accent
  (set-case-syntax-pair 203 235 downcase) ; E with diaeresis or umlaut mark
  (set-case-syntax-pair 204 236 downcase) ; I with grave accent
  (set-case-syntax-pair 205 237 downcase) ; I with acute accent
  (set-case-syntax-pair 206 238 downcase) ; I with circumflex accent
  (set-case-syntax-pair 207 239 downcase) ; I with diaeresis or umlaut mark
  (set-case-syntax-pair 208 240 downcase) ; D with stroke, Icelandic eth
  (set-case-syntax-pair 209 241 downcase) ; N with tilde
  (set-case-syntax-pair 210 242 downcase) ; O with grave accent
  (set-case-syntax-pair 211 243 downcase) ; O with acute accent
  (set-case-syntax-pair 212 244 downcase) ; O with circumflex accent
  (set-case-syntax-pair 213 245 downcase) ; O with tilde
  (set-case-syntax-pair 214 246 downcase) ; O with diaeresis or umlaut mark
  (set-case-syntax 215 "_" downcase)	; multiplication sign
  (set-case-syntax-pair 216 248 downcase) ; O with slash
  (set-case-syntax-pair 217 249 downcase) ; U with grave accent
  (set-case-syntax-pair 218 250 downcase) ; U with acute accent
  (set-case-syntax-pair 219 251 downcase) ; U with circumflex accent
  (set-case-syntax-pair 220 252 downcase) ; U with diaeresis or umlaut mark
  (set-case-syntax-pair 221 253 downcase) ; Y with acute accent
  (set-case-syntax-pair 222 254 downcase) ; thorn, Icelandic
  (set-case-syntax 223 "w" downcase)	; small sharp s, German
  (set-case-syntax 247 "_" downcase)	; division sign
  (set-case-syntax 255 "w" downcase)	; small y with diaeresis or umlaut mark
  (set-standard-case-table (list downcase nil nil nil)))

(provide 'iso-syntax)

;;; iso-syntax.el ends here
