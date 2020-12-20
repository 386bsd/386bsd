divert(0)dnl
#
# Copyright (c) 1983 Eric P. Allman
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by the University of
#	California, Berkeley and its contributors.
# 4. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#


######################################################################
######################################################################
#####
#####		SENDMAIL CONFIGURATION FILE
#####
define(`TEMPFILE', maketemp(/tmp/cfXXXXXX))dnl
syscmd(sh ../sh/makeinfo.sh > TEMPFILE)dnl
include(TEMPFILE)dnl
syscmd(rm -f TEMPFILE)dnl
#####
######################################################################
######################################################################

divert(-1)

changecom()
undefine(`format')
undefine(`hpux')
ifdef(`pushdef', `',
	`errprint(`You need a newer version of M4, at least as new as
System V or GNU')
	include(NoSuchFile)')
define(`PUSHDIVERT', `pushdef(`__D__', divnum)divert($1)')
define(`POPDIVERT', `divert(__D__)popdef(`__D__')')
define(`OSTYPE', `PUSHDIVERT(-1)define(`_ARG_', $2)include(../ostype/$1.m4)POPDIVERT`'')
define(`MAILER',
`ifdef(`_MAILER_$1_', `dnl`'',
`define(`_MAILER_$1_', `')PUSHDIVERT(7)include(../mailer/$1.m4)POPDIVERT`'')')
define(`DOMAIN', `PUSHDIVERT(-1)define(`_ARG_', $2)include(../domain/$1.m4)POPDIVERT`'')
define(`FEATURE', `PUSHDIVERT(-1)define(`_ARG_', $2)include(../feature/$1.m4)POPDIVERT`'')
define(`HACK', `PUSHDIVERT(-1)define(`_ARG_', $2)include(../hack/$1.m4)POPDIVERT`'')
define(`OLDSENDMAIL', `define(`_OLD_SENDMAIL_', `')')
define(`VERSIONID', ``#####  $1  #####'')
define(`LOCAL_RULE_0', `divert(3)')
define(`LOCAL_RULE_1',
`divert(9)dnl
#######################################
###  Ruleset 1 -- Sender Rewriting  ###
#######################################

S1
')
define(`LOCAL_RULE_2',
`divert(9)dnl
##########################################
###  Ruleset 2 -- Recipient Rewriting  ###
##########################################

S2
')
define(`LOCAL_RULE_3', `divert(2)')
define(`LOCAL_CONFIG', `divert(6)')
define(`LOCAL_NET_CONFIG', `define(`_LOCAL_RULES_', 1)divert(1)')
define(`UUCPSMTP', `R DOL(*) < @ $1 .UUCP > DOL(*)	DOL(1) < @ $2 > DOL(2)')
define(`CONCAT', `$1$2$3$4$5$6$7')
define(`DOL', ``$'$1')
define(`SITECONFIG',
`CONCAT(D, $3, $2)
define(`_CLASS_$3_', `')dnl
ifelse($3, U, Cw$2 $2.UUCP, `dnl')
define(`SITE', `ifelse(CONCAT($'2`, $3), SU,
		CONCAT(CY, $'1`),
		CONCAT(C, $3, $'1`))')
sinclude(../siteconfig/$1.m4)')
define(`EXPOSED_USER', `PUSHDIVERT(5)CE$1
POPDIVERT`'dnl')
define(`LOCAL_USER', `PUSHDIVERT(5)CL$1
POPDIVERT`'dnl')
define(`MASQUERADE_AS', `define(`MASQUERADE_NAME', $1)')

m4wrap(`include(`../m4/proto.m4')')

# set up default values for options
define(`confMAILER_NAME', ``MAILER-DAEMON'')
define(`confFROM_LINE', `From $g  $d')
define(`confOPERATORS', `.:%@!^/[]')
define(`confSMTP_LOGIN_MSG', `$j Sendmail $v/$Z ready at $b')
define(`confSEVEN_BIT_INPUT', `False')
define(`confALIAS_WAIT', `10')
define(`confMIN_FREE_BLOCKS', `4')
define(`confBLANK_SUB', `.')
define(`confCON_EXPENSIVE', `False')
define(`confCHECKPOINT_INTERVAL', `10')
define(`confDELIVERY_MODE', `background')
define(`confAUTO_REBUILD', `False')
define(`confSAVE_FROM_LINES', `False')
define(`confTEMP_FILE_MODE', `0600')
define(`confMATCH_GECOS', `False')
define(`confDEF_GROUP_ID', `1')
define(`confMAX_HOP', `17')
define(`confIGNORE_DOTS', `False')
define(`confBIND_OPTS', `')
define(`confMCI_CACHE_SIZE', `2')
define(`confMCI_CACHE_TIMEOUT', `5m')
define(`confUSE_ERRORS_TO', `False')
define(`confLOG_LEVEL', `9')
define(`confME_TOO', `False')
define(`confCHECK_ALIASES', `True')
define(`confOLD_STYLE_HEADERS', `True')
define(`confPRIVACY_FLAGS', `authwarnings')
define(`confSAFE_QUEUE', `True')
define(`confMESSAGE_TIMEOUT', `5d/4h')
define(`confTIME_ZONE', `USE_SYSTEM')
define(`confDEF_USER_ID', `1')
define(`confQUEUE_LA', `8')
define(`confREFUSE_LA', `12')
define(`confSEPARATE_PROC', `False')
define(`confCW_FILE', `/etc/sendmail.cw')
define(`confMIME_FORMAT_ERRORS', `True')
define(`confTRY_NULL_MX_LIST', `False')

divert(0)dnl
VERSIONID(`@(#)cf.m4	8.4 (Berkeley) 12/24/93')
