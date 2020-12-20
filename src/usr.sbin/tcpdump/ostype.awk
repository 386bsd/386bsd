# @(#) $Header: ostype.awk,v 1.3 90/07/14 21:58:39 leres Exp $ (LBL)

BEGIN {
	os = "UNKNOWN"
}

$0 ~ /^Sun.* Release 3\./ {
	os = "sunos3"
}

$0 ~ /^SunOS.* Release 4\./ {
	os = "sunos4"
}

$0 ~ /^4.[1-9] BSD / {
	os = "bsd"
}

# XXX need an example Ultrix motd
$0 ~ /[Uu][Ll][Tt][Rr][Ii][Xx]/ {
	os = "ultrix"
}

END {
	print os
}
