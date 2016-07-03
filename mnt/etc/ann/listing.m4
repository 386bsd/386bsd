define(__comment,`
#
# Macros to insert listings into nroff/troff documents
#
') dnl
define(listing,`.(b L
------------------------------------
 ... [ File: $1, line: $2 ]
syscmd(`sed -n $2,$3p $1') ...
.)b') dnl
define(listing1,`.(b L
------------------------------------
 ... [ File: $1, line: $2 ]
syscmd(`sed -n $2p $1') ...
.)b') dnl
