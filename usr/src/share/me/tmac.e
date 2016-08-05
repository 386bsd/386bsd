.nr _0 \n(.c
.\" Copyright (c) 1988 The Regents of the University of California.
.\" All rights reserved.
.\"
.\" Redistribution and use in source and binary forms, with or without
.\" modification, are permitted provided that the following conditions
.\" are met:
.\" 1. Redistributions of source code must retain the above copyright
.\"    notice, this list of conditions and the following disclaimer.
.\" 2. Redistributions in binary form must reproduce the above copyright
.\"    notice, this list of conditions and the following disclaimer in the
.\"    documentation and/or other materials provided with the distribution.
.\" 3. All advertising materials mentioning features or use of this software
.\"    must display the following acknowledgement:
.\"	This product includes software developed by the University of
.\"	California, Berkeley and its contributors.
.\" 4. Neither the name of the University nor the names of its contributors
.\"    may be used to endorse or promote products derived from this software
.\"    without specific prior written permission.
.\"
.\" THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
.\" ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
.\" IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
.\" ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
.\" FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
.\" DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
.\" OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
.\" HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
.\" LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
.\" OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
.\" SUCH DAMAGE.
.\"
.\"	@(#)tmac.e	2.35 (Berkeley) 4/17/91
.\"
.\"	%beginstrip%
.\"
.\"**********************************************************************
.\"*									*
.\"*	******  - M E   N R O F F / T R O F F   M A C R O S  ******	*
.\"*									*
.\"*	Produced for your edification and enjoyment by:			*
.\"*		Eric Allman						*
.\"*		Electronics Research Laboratory				*
.\"*		U.C. Berkeley.						*
.\"*	current address:						*
.\"*		Britton-Lee, Inc.					*
.\"*		1919 Addison Street Suite 105				*
.\"*		Berkeley, California  94704				*
.\"*									*
.\"*	VERSION 2.35	First Release: 11 Sept 1978			*
.\"*	See file \*(||/revisions for revision history			*
.\"*									*
.\"*	Documentation is available.					*
.\"*									*
.\"**********************************************************************
.\"
.\"	Code on .de commands:
.\"		***	a user interface macro.
.\"		&&&	a user interface macro which is redefined
.\"			when used to be the real thing.
.\"		$$$	a macro which may be redefined by the user
.\"			to provide variant functions.
.\"		---	an internal macro.
.\"
.\" library directory for sourced files:
.ds || /usr/share/me
.if \n@>0 .ds || .
.\"
.if !\n(.V .tm You are using the wrong version of NROFF/TROFF!!
.if !\n(.V .tm This macro package works only on the version seven
.if !\n(.V .tm release of NROFF and TROFF.
.if !\n(.V .ex
.if \n(pf \
.	nx \*(||/null.me
.\"		*** INTERNAL GP MACROS ***
.de @C			\" --- change ev's, taking info with us
.nr _S \\n(.s
.nr _V \\n(.v
.nr _F \\n(.f
.nr _I \\n(.i
.ev \\$1
.ps \\n(_S
.vs \\n(_Vu
.ft \\n(_F
'in \\n(_Iu
.xl \\n($lu
.lt \\n($lu
.rr _S
.rr _V
.rr _F
.rr _I
.ls 1
'ce 0
..
.de @D			\" --- determine display type (Indent, Left, Center)
.ds |p "\\$3
.nr _d \\$1
.ie "\\$2"C" \
.	nr _d 1
.el .ie "\\$2"L" \
.	nr _d 2
.el .ie "\\$2"I" \
.	nr _d 3
.el .ie "\\$2"M" \
.	nr _d 4
.el \
.	ds |p "\\$2
..
.de @z			\" --- end macro
.if \n@>1 .tm >> @z, .z=\\n(.z ?a=\\n(?a
.if !"\\n(.z"" \
\{\
.	tm Line \\n(c. -- Unclosed block, footnote, or other diversion (\\n(.z)
.	di
.	ex
.\}
.if \\n(?a \
.	bp			\" force out final table
.rm bp
.rm @b\"			\" don't start another page
.if t \
.	wh -1p @m
.br
.if \n@>1 .tm << @z
..
.de @I			\" --- initialize processor
.rm th
.rm ac
.rm lo
.rm sc
.rm @I
..
.\"		*** STANDARD HEADERS AND FOOTERS ***
.de he			\" *** define header
.ie !\\n(.$ \
\{\
.	rm |4
.	rm |5
.\}
.el \
\{\
.	ds |4 "\\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
.	ds |5 "\\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
.\}
..
.de eh			\" *** define even header
.ie !\\n(.$ \
.	rm |4
.el \
.	ds |4 "\\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
..
.de oh			\" *** define odd header
.ie !\\n(.$ \
.	rm |5
.el \
.	ds |5 "\\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
..
.de fo			\" *** define footer
.ie !\\n(.$ \
\{\
.	rm |6
.	rm |7
.\}
.el \
\{\
.	ds |6 "\\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
.	ds |7 "\\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
.\}
..
.de ef			\" *** define even foot
.ie !\\n(.$ \
.	rm |6
.el \
.	ds |6 "\\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
..
.de of			\" *** define odd footer
.ie !\\n(.$ \
.	rm |7
.el \
.	ds |7 "\\$1 \\$2 \\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
..
.de ep			\" *** end page (must always be followed by a .bp)
.if \\n(nl>0 \
\{\
.	wh 0
.	rs
.	@b
.\}
..
.\"		*** INTERNAL HEADER AND FOOTER MACROS ***
.de @h			\" --- header
.if \n@>1 .tm >> @h %=\\n% ?a=\\n(?a ?b=\\n(?b ?w=\\n(?w
.if (\\n(.i+\\n(.o)>=\\n(.l \
.	tm Line \\n(c. -- Offset + indent exceeds line length
.if t .if (\\n(.l+\\n(.o)>7.75i \
.	tm Line \\n(c. -- Offset + line length exceeds paper width
.\" initialize a pile of junk
.nr ?h \\n(?H			\" transfer "next page" to "this page"
.rr ?H
.nr ?c \\n(?C
.rr ?C
.rn |4 |0
.rn |5 |1
.rn |6 |2
.rn |7 |3
.nr _w 0			\" reset max footnote width
.nr ?W 0			\" no wide floats this page (yet)
.nr ?I 1
.\" begin actual header stuff
.ev 2
.rs
.if t .@m			\" output cut mark
.if \\n(hm>0 \
.	sp |\\n(hmu		\" move to header position
.@t $h				\" output header title
.if \\n(tm<=0 \
.	nr tm \n(.Vu
.sp |\\n(tmu			\" move to top of text
.ev
.mk _k				\" for columned output
.if \\n(?n .nm 1		\" restore line numbering if n1 mode
.nr $c 1			\" set first column
.if \n@>4 .tm -- @h >> .ns nl=\\n(nl %=\\n% _k=\\n(_k tm=\\n(tm
.ie \\n(?s \
\{\
.	rr ?s
.	rs
'	@b
.\}
.el \
.	@n			\" begin the column
.if \n@>2 .tm << @h
..
.if \nv=2 \
\{\
.	de @m		\" --- output cut mark (only on C/A/T-style)
.	@O 0
.	lt 7.5i
.	tl '\(rn''\(rn'
.	@O
.	lt
..
.\}
.de @n			\" --- new column or page
.if \n@>3 .tm >> @n nl=\\n(nl %=\\n% ?f=\\n(?f ?o=\\n(?o
.if \\n(bm<=0 \
.	nr bm \\n(.Vu
.if (\\n(_w<=\\n($l)&(\\n(?W=0) \
\{\
.	nr _b (\\n(ppu*\\n($ru)/2u	\" compute fudge factor (must be < 1P)
.	if \\n(_bu>((\\n(bmu-\\n(fmu-(\\n(tpu*\\n($ru))/2u) \
.		nr _b (\\n(ppu*\\n($ru)-\n(.Vu
.	nr _b +\\n(bmu
.\}
.nr _B \\n(_bu
.ch @f
.wh -\\n(_bu @f
.nr _b +(\\n(ppu*\\n($ru)	\" add 1 paragraph v in case of sweep past
.if \n@>2 .tm @n .p=\\n(.p bm=\\n(bm _b=\\n(_b _B=\\n(_B
.nr ?f 0			\" reset footnote flag
.if \\n(?o \
\{\
.	(f _			\" reprocess footnotes which run off page
.	nf
.	|o
.	fi
.	)f
.	rm |o
.\}
.nr ?o 0
.if \\n(?T \
\{\
.	nr _i \\n(.i
.	in \\n($iu
.	|h			\" output the table header
.	in \\n(_iu
.	rr _i
.	mk #T			\" for tbl commands
.	ns
.\}
.if (\\n(?a)&((\\n($c<2):(\\n(?w=0)) \
\{\
.	nr ?a 0			\" output floating keep
.	@k |t
.	if \\n(?w \
.		mk _k		\" don't overstrike wide keeps
.	nr ?w 0
.\}
.os
.$H				\" special column header macro
.ns
..
.de @f			\" --- footer
.if \n@>1 .tm >> @f %=\\n% nl=\\n(nl ?a=\\n(?a ?b=\\n(?b ?f=\\n(?f
.if \n@>2 .nr VL \\n(.pu-\\n(nlu
.if \n@>2 .tm @f bm=\\n(bm _B=\\n(_B _b=\\n(_b .p-nl=\\n(VL
.ec
.if \\n(?T \
\{\
.	nr T. 1			\" for tbl commands (to output bottom line)
.	T# 1			\" output the sides and bottom lines
.	br
.\}
.ev 2
.ce 0
.if \\n(?b \
\{\
.	nr ?b 0
.	@k |b\"			\" output bottom of page tables
.\}
.if \\n(?f \
.	@o			\" output footnote if present
.ie \\n($c<\\n($m \
.	@c			\" handle new column
.el \
.	@e			\" new page
.ev
.if \n@>2 .tm << @f
..
.de @o			\" --- output footnote
.nf
.ls 1
.in 0
.if \n@>2 .tm @o last printed text = \\n(nl placing @r trap at -\\n(_B
.wh -\\n(_Bu @r
.|f
.fi
.if \n@>2 .tm @o triggered @r (?o) = \\n(?o
.if \\n(?o \
\{\
.	di			\" just in case triggered @r
.	if \\n(dn=0 \
\{\
.		rm |o
.		nr ?o 0
.	\}
.	nr dn \\n(_D
.	rr _D
.\}
.rm |f
.ch @r
..
.de @c			\" --- new column
.if \n@>2 .tm	>> @c %=\\n%
.rs
.sp |\\n(_ku
.@O +\\n($lu+\\n($su
.nr $c +1
.@n
..
.de @e			\" --- end page
.if \n@>2 .tm	>> @e
.@O \\n(_ou
.rs
.sp |\\n(.pu-\\n(fmu-(\\n(tpu*\\n($ru)	\" move to footer position
.@t $f				\" output footer title
.nr ?h 0
.bp
..
.de @t			\" --- output header or footer title
.if !\\n(?h \
\{\
.	sz \\n(tp		\" set header/footer type fonts, etc.
.	@F \\n(tf
.	lt \\n(_Lu		\" make title span entire page
.	nf
.	\\$1
.	br
.\}
..
.de $h			\" $$$ print header
.rm |z
.if !\\n(?c \
\{\
.	if e .ds |z "\\*(|0
.	if o .ds |z "\\*(|1
.\}
.if !\(ts\\*(|z\(ts\(ts \
'	tl \\*(|z
.rm |z
..
.de $f			\" $$$ print footer
.rm |z
.if \\n(?c \
\{\
.	if e .ds |z "\\*(|0
.	if o .ds |z "\\*(|1
.\}
.if \(ts\\*(|z\(ts\(ts \
\{\
.	if e .ds |z "\\*(|2
.	if o .ds |z "\\*(|3
.\}
.if !\(ts\\*(|z\(ts\(ts \
'	tl \\*(|z
.rm |z
..
.de @r			\" --- reprocess overflow footnotes
.if \n@>3 .tm		>> @r .z=\\n(.z ?f=\\n(?f ?a=\\n(?a ?b=\\n(?b _b=\\n(_b
.di |o				\" save overflow footnote
.nr ?o 1
.nr _D \\n(dn
.ns
..
.\"		*** COMMANDS WITH VARIANT DEFINITIONS ***
.rn bp @b		\" --- begin page
.de bp			\" *** begin new page (overrides columns)
.nr $c \\n($m			\" force new page, not new column
.ie \\n(nl>0 \
.	@b \\$1
.el \
\{\
.	if \\n(.$>0 \
.		pn \\$1
.	if \\n(?I \
.		@h		\" 'spring' the header trap
.\}
.br
.wh 0 @h			\" reset header
..
.rn ll xl		\" *** special line length (local)
.de ll			\" *** line length (global to environments)
.xl \\$1
.lt \\$1
.nr $l \\n(.l
.if (\\n($m<=1):(\\n($l>\\n(_L) \
.	nr _L \\n(.l
..
.rn po @O		\" --- local page offset
.de po			\" *** page offset
.@O \\$1
.nr _o \\n(.o
..
.\"		*** MISCELLANEOUS ROFF COMMANDS ***
.de hx			\" *** suppress headers and footers next page
.nr ?H 1
..
.de ix			\" *** indent, no break
'in \\$1
..
.de bl			\" *** contiguous blank lines
.br
.ne \\$1
.rs
.sp \\$1
..
.de n1			\" *** line numbering 1
.nm 1
.xl -\w'0000'u
.nr ?n 1
..
.de n2			\" *** line numbering 2
.nm \\$1
.ie \\n(.$ \
.	xl -\w'0000'u
.el \
.	xl \\n($lu
..
.de pa			\" *** new page
.bp \\$1
..
.de ro			\" *** roman page numbers
.af % i
..
.de ar			\" *** arabic page numbers
.af % 1
..
.de m1			\" *** position one space
.nr _0 \\n(hmu
.nr hm \\$1v
.nr tm +\\n(hmu-\\n(_0u
.rr _0
..
.de m2			\" *** position two space
.nr tm \\n(hmu+\\n(tpp+\\$1v
..
.de m3			\" *** position three space
.nr bm \\n(fmu+\\n(tpp+\\$1v
..
.de m4			\" *** position four space
.nr _0 \\n(fmu
.nr fm \\$1v
.nr bm +\\n(fmu-\\n(_0u
..
.de sk			\" *** leave a blank page (next page)
.if \\n(.$>0 \
.	tm Line \\n(c. -- I cannot skip multiple pages
.nr ?s 1
..
.\"		*** MISCELLANEOUS USER SUPPORT COMMANDS ***
.de re			\" *** reset tabs (TROFF defines 15 stops default)
.ta 0.5i +0.5i +0.5i +0.5i +0.5i +0.5i +0.5i +0.5i +0.5i +0.5i +0.5i +0.5i +0.5i +0.5i +0.5i
..
.if t .ig
.de re			\" *** reset tabs (NROFF version)
.ta 0.8i +0.8i +0.8i +0.8i +0.8i +0.8i +0.8i +0.8i +0.8i +0.8i +0.8i +0.8i +0.8i +0.8i +0.8i
..
.de ba			\" *** set base indent
.ie \\n(.$ \
.	nr $i \\$1n
.el \
.	nr $i \\n(siu*\\n($0u
..
.de hl			\" *** draw horizontal line
.br
\l'\\n(.lu-\\n(.iu'
.sp
..
.\"		*** PARAGRAPHING ***
.de pp			\" *** paragraph
.lp \\n(piu
..
.de lp			\" *** left aligned paragraph
.@p
.if \\n(.$ \
.	ti +\\$1
.nr $p 0 1
..
.de ip			\" *** indented paragraph w/ optional tag
.if (\\n(ii>0)&(\\n(ii<1n) \
.	nr ii \\n(iin
.nr _0 \\n(ii
.if \\n(.$>1 \
.	nr _0 \\$2n
.@p \\n(_0u
.if \\w"\\$1" \
\{\
.	ti -\\n(_0u
.	ie \\w"\\$1">=\\n(_0 \
\{\
\&\\$1
.		br
.	\}
.	el \&\\$1\h'|\\n(_0u'\c
.\}
.rr _0
..
.de np			\" *** numbered paragraph
.if \\n($p<0 \
.	nr $p 0			\" reset number after .bu
.nr $p +1			\" increment paragraph number
.@p \w'\0(000)\0'u
.ti -\w'\0(000)\0'u
\0(\\n($p)\h'|\w'\0(000)\0'u'\c
..
.de bu			\" *** bulleted paragraph
.br
.if \\n($p<0 \
.	ns			\" don't space between .bu paragraphs
.nr $p 0-1			\" mark "bulleted paragraph" mode
.@p \w'\0\(bu\0'u
.ti -\w'\0\(bu\0'u
\0\(bu\0\c
..
.de @p			\" --- initialize for paragraph
.@I				\" initialize macro processor
.if "\\n(.z"|e" .tm Line \\n(c. -- Unmatched continued equation
.in \\n($iu+\\n(pou
.if \\n(.$ \
.	in +\\$1n
.ce 0
.fi
.@F \\n(pf
.sz \\n(pp
.sp \\n(psu
.ne \\n(.Lv+\\n(.Vu
.ns
..
.\"		*** SECTION HEADINGS ***
.de sh			\" &&& section heading
.rn sh @T
.so \\*(||/sh.me
.sh "\\$1" "\\$2" \\$3 \\$4 \\$5 \\$6 \\$7 \\$8
.rm @T
..
.de $p			\" $$$ print section heading
.if (\\n(si>0)&(\\n(.$>2) \
.	nr $i \\$3*\\n(si
.in \\n($iu
.ie !"\\$1\\$2"" \
\{\
.	sp \\n(ssu 		\" one of them is non-null
.	ne \\n(.Lv+\\n(.Vu+\\n(psu+(\\n(spu*\\n($ru*\\n(.Lu)
.	ie \\n(.$>2 \
.		ti -(\\n(siu-\\n(sou)
.	el \
.		ti +\\n(sou
.	@F \\n(sf
.	sz \\n(sp
.	if \\$3>0 \
.		$\\$3
.	if \w"\\$2">0 \\$2.
.	if \w"\\$1">0 \\$1\f1\ \ \&
.\}
.el \
.	sp \\n(psu
.@F \\n(pf
.sz \\n(pp
..
.de uh			\" *** unnumbered section heading
.rn uh @T
.so \\*(||/sh.me
.uh "\\$1"
.rm @T
..
.\"		*** COLUMNNED OUTPUT ***
.de 2c			\" *** double columned output
.br
.if \\n($m>1 \
.	1c			\" revert to 1c if already 2c
.nr $c 1
.nr $m 2
.if \\n(.$>1 \
.	nr $m \\$2
.if \\n(.$>0 \
.	nr $s \\$1n		\" param 1: column seperation
.nr $l (\\n(.l-((\\n($m-1)*\\n($s))/\\n($m
.xl \\n($lu
.mk _k
.ns
..
.de 1c			\" *** single columned output
.br
.nr $c 1
.nr $m 1
.ll \\n(_Lu			\" return to normal output
.sp |\\n(.hu
.@O \\n(_ou
..
.de bc			\" *** begin column
.sp 24i
..
.\"		*** FLOATING TABLES AND NONFLOATING BLOCKS ***
.de (z			\" &&& begin floating keep
.rn (z @V
.so \\*(||/float.me
.(z \\$1 \\$2
.rm @V
..
.de )z			\" &&& end floating keep
.tm Line \\n(c. -- unmatched .)z
..
.de (t			\" XXX temp ref to (z
.(z \\$1 \\$2
..
.de )t			\" XXX temp ref to )t
.)z \\$1 \\$2
..
.de (b			\" *** begin block
.br
.@D 3 \\$1 \\$2
.sp \\n(bsu
.@(
..
.de )b			\" *** end block
.br
.@)
.if (\\n(bt=0):(\\n(.t<\\n(bt) \
.	ne \\n(dnu		\" make it all on one page
.ls 1
.nf
.|k
.ec
.fi
.in 0
.xl \\n($lu
.ev
.rm |k
.sp \\n(bsu+\\n(.Lv-1v
..
.de @(			\" --- begin keep
.if !"\\n(.z"" .tm Line \\n(c. -- Illegal nested keep \\n(.z
.@M
.di |k
\!'rs
..
.de @M			\" --- set modes for display
.nr ?k 1
.@C 1
.@F \\n(df
.vs \\n(.su*\\n($Ru
.nf
.if "\\*(|p"F" \
.	fi			\" set fill mode if "F" parameter
.if \\n(_d=4 \
.	in 0
.if \\n(_d=3 \
\{\
.	in +\\n(biu
.	xl -\\n(biu
.\}
.if \\n(_d=1 \
.	ce 10000
..
.de @)			\" --- end keep
.br
.if !"\\n(.z"|k" .tm Line \\n(c. -- Close of a keep which has never been opened
.nr ?k 0
.di
.in 0
.ce 0
..
.de (c			\" *** begin block centered text
.if "\\n(.z"|c" .tm Line \\n(c. -- Nested .(c requests
.di |c
..
.de )c			\" *** end block centered text
.if !"\\n(.z"|c" .tm Line \\n(c. -- Unmatched .)c
.br				\" force out final line
.di
.if \n@>4 .tm >> .)c .l=\\n(.l .i=\\n(.i $i=\\n($i dl=\\n(dl
.ev 1
.ls 1
.in (\\n(.lu-\\n(.iu-\\n(dlu)/2u
.if \n@>4 .tm -- .)c << .in .l=\\n(.l .i=\\n(.i dl=\\n(dl
.nf
.|c
.ec
.in
.ls
.ev
.rm |c
..
.\"		*** BLOCK QUOTES (OR WHATEVER) AND LISTS ***
.de (q			\" *** begin block quote
.br
.@C 1
.fi
.sp \\n(qsu
.in +\\n(qiu
.xl -\\n(qiu
.sz \\n(qp
..
.de )q			\" *** end block quote
.br
.ev
.sp \\n(qsu+\\n(.Lv-1v
.nr ?k 0
..
.de (l			\" *** begin list
.br
.sp \\n(bsu
.@D 3 \\$1 \\$2
.@M
..
.de )l			\" *** end list
.br
.ev
.sp \\n(bsu+\\n(.Lv-1v
.nr ?k 0
..
.\"		*** PREPROCESSOR SUPPORT ***
.\"
.\"	EQN
.\"
.de EQ			\" &&& begin equation
.rn EQ @T
.so \\*(||/eqn.me
.EQ \\$1 \\$2
.rm @T
..
.\"
.\"	TBL
.\"
.de TS			\" &&& begin table
.rn TS @W
.so \\*(||/tbl.me
.TS \\$1 \\$2
.rm @W
..
.\"
.\"	REFER
.\"
.de ]-			\" &&& initialize reference
.rn ]- @]
.so \\*(||/refer.me
.]-
.rm @]
..
.de ]<			\" &&& initialize reference
.rn ]< @]
.so \\*(||/refer.me
.]<
.rm @]
..
.if n .ds [. " [
.if t .ds [. \s-2\v'-.4m'\f1
.if n .ds .] ]
.if t .ds .] \v'.4m'\s+2\fP
.if n .ds <. "
.if t .ds <. .
.if n .ds >. .
.if t .ds >. "
.\"
.\"	IDEAL
.\"
.de IS			\" *** start ideal picture
.nr g7 \\n(.u
.ls 1
..
.de IF
.if \\n(g7 .fi
.ls
..
.de IE			\" *** end ideal picture
.if \\n(g7 .fi
.ls
..
.\"
.\"	PIC
.\"
.de PS		\" *** start picture: $1=height, $2=width in units or inches
.if t \
.	sp 0.3
.nr g7 \\$2
.in (\\n(.lu-\\n(g7u)/2u
.ne \\$1u
.nr g7 \\n(.u
.ls 1
..
.de PE			\" *** end picture
.ls
.in
.if \\n(g7 .fi
.if t .sp .6
..
.\"
.\"	GREMLIN
.\"
.de GS			\" *** start gremlin picture
.nr g7 (\\n(.lu-\\n(g1u)/2u
.if "\\$1"L" .nr g7 \\n(.iu
.if "\\$1"R" .nr g7 \\n(.lu-\\n(g1u
.in \\n(g7u
.nr g7 \\n(.u
.ls 1
.nf
.ne \\n(g2u
..
.de GE			\" *** end gremlin picture
.GF
.if t .sp .6
..
.de GF			\" *** finish gremlin picture; stay at top
.ls
.in
.if \\n(g7 .fi
..
.\"		*** FONT AIDS ***
.de sz			\" *** set point size and vertical spacing
.ps \\$1
.vs \\n(.su*\\n($ru		\" default vs at pointsize + 20%
.bd S B \\n(.su/3u
..
.de r			\" *** enter roman font
.nr _F \\n(.f
.ul 0
.ft 1
.if \\n(.$ \&\\$1\f\\n(_F\\$2
.rr _F
..
.de i			\" *** enter italic
.nr _F \\n(.f
.ul 0
.ft 2
.if \\n(.$ \&\\$1\f\\n(_F\\$2
.rr _F
..
.de b			\" *** enter boldface (underline in NROFF)
.nr _F \\n(.f
.ul 0
.ft \\n($b
.if \\n(.$ \&\\$1\f\\n(_F\\$2
.rr _F
..
.de rb			\" *** enter real boldface (not underlined in NROFF)
.nr _F \\n(.f
.ul 0
.ft 3
.if \\n(.$ \&\\$1\f\\n(_F\\$2
.rr _F
..
.de u			\" *** enter underlined word
\&\\$1\l'|0\(ul'\\$2
..
.de q			\" *** enter quoted word
\&\\*(lq\\$1\\*(rq\\$2
..
.de bi			\" *** enter word in bold italics
.ft 2
.ie t \&\k~\\$1\h'|\\n~u+(\\w' 'u/4u)'\\$1\fP\\$2
.el \&\\$1\fP\\$2
..
.de bx			\" *** enter boxed word
.ie \\n($T \&\f2\\$1\fP\\$2
.el \k~\(br\|\\$1\|\(br\l'|\\n~u\(rn'\l'|\\n~u\(ul'\^\\$2
..
.de sm			\" *** print in smaller font
\s-1\\$1\\s0\\$2
..
.de @F			\" --- change font (8 -> underlined, 0 -> no change)
.nr ~ \\$1
.if \\n~>0 \
\{\
.	ul 0
.	if \\n~=8 \
.		nr ~ \\n($b
.	ft \\n~
.\}
.rr ~
..
.\"		*** FOOTNOTING ***
.de (f			\" &&& begin footnote
.rn (f @U
.so \\*(||/footnote.me
.(f \\$1 \\$2
.rm @U
..
.de )f			\" &&& end footnote
.tm Line \\n(c. -- unmatched .)f
..
.de $s			\" $$$ footnote separator
\l'2i'
.if n \
.	sp 0.3
..
.\"		*** DELAYED TEXT ***
.de (d			\" &&& begin delayed text
.rn (d @U
.so \\*(||/deltext.me
.(d \\$1 \\$2
.rm @U
..
.de )d			\" &&& end delayed text
.tm Line \\n(c. -- unmatched .)d
..
.\"		*** INDEXES (TABLE OF CONTENTS) ***
.de (x			\" &&& begin index
.rn (x @U
.so \\*(||/index.me
.(x \\$1 \\$2
.rm @U
..
.de )x			\" &&& end index entry
.tm Line \\n(c. -- unmatched .)x
..
.\"		*** STUFF FOR "STANDARD" PAPERS ***
.de th			\" *** set "thesis" mode
.so \\*(||/thesis.me
.rm th
..
.de +c			\" *** begin chapter
.ep				\" force out footnotes
.if \\n(?o:\\n(?a \
\{\
.	bp			\" force out a table or more footnote
.	rs
.	ep
.\}
.nr ?C 1
.nr $f 1 1
.ds * \\*[1\\*]\k*
.if \\n(?R \
.	pn 1
.bp
.in \\n($iu			\" reset the indent
.rs
.ie \\n(.$ \
.	$c "\\$1"
.el \
.	sp 3
..
.de ++			\" *** declare chapter type
.nr _0 0
.if "\\$1"C" \
.	nr _0 1			\" chapter
.if "\\$1"RC" \
.	nr _0 11		\" renumbered chapter
.if "\\$1"A" \
.	nr _0 2			\" appendix
.if "\\$1"RA" \
.	nr _0 12		\" renumbered appendix
.if "\\$1"P" \
.	nr _0 3			\" preliminary material
.if "\\$1"B" \
.	nr _0 4			\" bibliographic material
.if "\\$1"AB" \
.	nr _0 5			\" abstract
.if \\n(_0=0 \
.	tm Line \\n(c. -- Bad mode to .++
.nr ?R 0
.if \\n(_0>10 \
.\{
.	nr ?R 1
.	nr _0 -10
.\}
.nr ch 0 1
.if (\\n(_0=3):(\\n(_0=5) \
.	pn 1			\" must do before .ep
.ep				\" end page for correct page number types
.if \\n(_0=1 \
\{\
.	af ch 1
.	af % 1
.\}
.if \\n(_0=2 \
\{\
.	af ch A
.	af % 1
.\}
.if \\n(_0=3 \
.	af % i
.if \\n(_0=4 \
.	af % 1
.if \\n(_0=5 \
.	af % 1
.if \\n(.$>1 \
.	he \\$2
.if !\\n(_0=\\n(_M .if \\n(_M=3 \
.	pn 1
.nr _M \\n(_0
.rr _0
..
.de $c			\" $$$ print chapter title
.sz 12
.ft B
.ce 1000
.if \\n(_M<3 \
.	nr ch +1
.ie \\n(_M=1 CHAPTER\ \ \\n(ch
.el .if \\n(_M=2 APPENDIX\ \ \\n(ch
.if \w"\\$1" .sp 3-\\n(.L
.if \w"\\$1" \\$1
.if (\\n(_M<3):(\w"\\$1") \
.	sp 4-\\n(.L
.ce 0
.ft
.sz
.ie \\n(_M=1 \
.	$C Chapter \\n(ch "\\$1"
.el .if \\n(_M=2 \
.	$C Appendix \\n(ch "\\$1"
..
.de tp			\" *** title page
.hx
.bp
.br
.rs
.pn \\n%
..
.de ac			\" *** setup for ACM photo-ready paper
.rn ac @T
.so \\*(||/acm.me
.ac "\\$1" "\\$2"
.rm @T
..
.de lo			\" *** pull in the set of local macros
.\" all these macros should be named "*X", where X is any letter
.so \\*(||/local.me
.rm lo
..
.de lh			\" *** letterhead
.so \\*(||/letterhead.me
..
.\"		*** DATES ***
.if \n(mo=1 .ds mo January
.if \n(mo=2 .ds mo February
.if \n(mo=3 .ds mo March
.if \n(mo=4 .ds mo April
.if \n(mo=5 .ds mo May
.if \n(mo=6 .ds mo June
.if \n(mo=7 .ds mo July
.if \n(mo=8 .ds mo August
.if \n(mo=9 .ds mo September
.if \n(mo=10 .ds mo October
.if \n(mo=11 .ds mo November
.if \n(mo=12 .ds mo December
.if \n(dw=1 .ds dw Sunday
.if \n(dw=2 .ds dw Monday
.if \n(dw=3 .ds dw Tuesday
.if \n(dw=4 .ds dw Wednesday
.if \n(dw=5 .ds dw Thursday
.if \n(dw=6 .ds dw Friday
.if \n(dw=7 .ds dw Saturday
.ds td \*(mo \n(dy, 19\n(yr
.\"		*** PARAMETRIC INITIALIZATIONS ***
.if (1m<0.1i)&(\nx!=0) \
.	vs 9p			\" for 12-pitch DTC terminals
.rr x
.nr $r \n(.v/\n(.s		\" ratio of vs to ps for .sz request
.nr $R \n($r			\" ratio for displays & footnotes
.nr hm 4v			\" header margin
.nr tm 7v			\" top margin
.nr bm 6v			\" bottom margin
.nr fm 3v			\" footer margin
.nr tf 3			\" title font: (real) Times Bold
.nr tp 10			\" title point size
.hy 14
.nr bi 4m			\" indent for blocks
.nr pi 5n			\" indent for paragraphs
.nr pf 1			\" normal text font
.nr pp 10			\" normal text point size
.nr qi 4n			\" indent for quotes
.nr qp -1			\" down one point
.nr ii 5n			\" indent for .ip's and .np's
.nr $m 1			\" max number of columns
.nr $s 4n			\" column separation
.bd S B 3
.\"		*** OTHER INITIALIZATION ***
.ds [ \u\x'-0.25v'
.ds ] \d
.ds < \d\x'0.25v'
.ds > \u
.ds - --
.if t \
\{\
.	ds [ \v'-0.4m'\x'-0.2m'\s-3
.	ds ] \s0\v'0.4m'
.	ds < \v'0.4m'\x'0.2m'\s-3
.	ds > \s0\v'-0.4m'
.	ds - \(em
.	nr fi 0.3i
.\}
.if n \
\{\
.	nr fi 3n
.\}
.nr _o \n(.o
.if n .po 1i
.if \n(.V=1v \
.	nr $T 2
.if n .if \n(.T=0 \
.	nr $T 1
.if \nv=2 \
\{\
.	nr $T 0
.	po -0.5i		\" make ugly line on LHS on C/A/T typesetters
.\}
.if \n($T \
\{\
.	if \n($T=1 \
.		po 0
.	ds [ [
.	ds ] ]
.	ds < <
.	ds > >
.\}
.nr $b \nb			\" figure the real font 8 font
.rr b
.if \n($b=0 \
\{\
.	if n .nr $b 2		\" italic
.	if t .nr $b 3		\" bold
.\}
.nr ps 0.5v			\" paragraph pre/post spacing
.if \n($T \
.	nr ps 1v
.if t .nr ps 0.35v
.nr bs \n(ps			\" block pre/post spacing
.nr qs \n(ps			\" quote pre/post spacing
.nr zs 1v			\" float-block pre/postspacing
.nr xs 0.2v			\" index prespacing
.nr fs 0.2v			\" footnote prespacing
.if \n($T \
.	nr fs 0
.if n .nr es 1v			\" equation pre/postspacing
.if t .nr es 0.5v
.wh 0 @h			\" set header
.nr $l \n(.lu			\" line length
.nr _L \n(.lu			\" line length of page
.nr $c 1			\" current column number
.nr $f 1 1			\" footnote number
.ds * \*[1\*]\k*\"		\" footnote "name"
.nr $d 1 1			\" delayed text number
.ds # [1]\k#\"			\" delayed text "name"
.nr _M 1			\" chapter mode is chapter
.ds lq \&"\"			\" left quote
.ds rq \&"\"			\" right quote
.if t \
.	ds lq ``
.if t \
.	ds rq ''
.em @z
.\"		*** FOREIGN LETTERS AND SPECIAL CHARACTERS ***
.de sc			\" *** define special characters
.so \\*(||/chars.me
.rm sc
..
.ll 6.0i
.lt 6.0i
