.nr _C \n(.C
.cp 0
.ftr CW CR
.ftr C CR
.ftr CO CI
.ftr CX CBI
.ftr H HR
.ftr HO HI
.ftr HX HBI
.ftr NX NBI
.char \(ru \D'l .5m 0'
.char \(ul \v'.25m'\D'l .5m 0'\v'-.25m'
.char \(br \v'.25m'\D'l 0 -1m'\v'.75m'
.char \(rn \v'-.75m'\D'l .5m 0'\v'.75m'
.char ~ \v'-.55m'\\s[\\n(.s/2u]\v'.2m'\(ti\v'-.2m'\s0\v'.55m'
.char ^ \v'-.55m'\\s[\\n(.s/2u]\v'.3m'\(ha\v'-.3m'\s0\v'.55m'
.if !c\(va .char \(va \o'\(ua\(da'
.if !c\(em .char \(em --
.if !c\(en .char \(en \-
.if !c\(fi .char \(fi fi
.if !c\(fl .char \(fl fl
.if !c\(ff .char \(ff ff
.if !c\(Fi .char \(Fi ffi
.if !c\(Fl .char \(Fl ffl
.if !c\(ci .char \(ci \v'-.25m'\h'.05m'\D'c .5m'\h'.05m'\v'.25m'
.if !c\(sq .char \(sq \h'.05m'\D'l .5m 0'\D'l 0 -.5m'\D'l -.5m 0'\D'l 0 .5m'\h'.55m'
.if !c\(ga .char \(ga \Z'\v'-.7m'\D'l .22m .18m''\h'.33m'
.if !c\(dg .char \(dg \Z'\h'.25m'\v'.15m'\D'l 0 -.8m'\v'.2m'\h'-.195m'\
\D'l .39m 0''\h'.5m'
.if !c\(dd .char \(dd \Z'\h'.25m'\v'.15m'\D'l 0 -.8m'\v'.2m'\h'-.195m'\
\D'l .39m 0'\v'.4m'\D'l -.39m 0''\h'.5m'
.if !c\(lq .char \(lq ``
.if !c\(rq .char \(rq ''
.if !c\(Bq .char \(bq ,,
.if !c\(OE .char \(OE O\h'-.25m'E
.if !c\(oe .char \(oe o\h'-.14m'e
.if !c\(ah .char \(ah \v'-.55m'\s[\En[.s]/2u]v\s0\v'.55m'
.if !c\(ao .char \(ao \v'-.55m'\s[\En[.s]*6u/10u]\D'c .25m'\s0\v'.55m'
.if !c\(ho .char \(ho \s[\En[.s]/2u]\v'.4m'c\v'-.4m'\s0
.if !c\(lh .tr \(lh\(lA
.if !c\(rh .tr \(rh\(rA
.if !c\(bq .tr \(bq,
.if !c\(aq .tr \(aq'
.if '\*(.T'X100' .char \[radicalex] \h'-\w'\(sr'u'\[radicalex]\h'\w'\(sr'u'
.if !\n(_C .mso tmac.pspic
.cp \n(_C
