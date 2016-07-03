#!/bin/sh
# $XConsortium: mkdirhier.sh,v 1.6 91/08/13 18:13:04 rws Exp $
# Courtesy of Paul Eggert

newline='
'
IFS=$newline

case ${1--} in
-*) echo >&2 "mkdirhier: usage: mkdirhier directory ..."; exit 1
esac

status=

for directory
do
	case $directory in
	'')
		echo >&2 "mkdirhier: empty directory name"
		status=1
		continue;;
	*"$newline"*)
		echo >&2 "mkdirhier: directory name contains a newline: \`\`$directory''"
		status=1
		continue;;
	///*) prefix=/;; # See Posix 2.3 "path".
	//*) prefix=//;;
	/*) prefix=/;;
	-*) prefix=./;;
	*) prefix=
	esac

	IFS=/
	set x $directory
	IFS=$newline
	shift

	for filename
	do
		path=$prefix$filename
		prefix=$path/
		shift

		test -d "$path" || {
			paths=$path
			for filename
			do
				if [ "$filename" != "." ]; then
					path=$path/$filename
					paths=$paths$newline$path
				fi
			done

			mkdir $paths || status=$?

			break
		}
	done
  done

exit $status
