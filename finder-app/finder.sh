#!/bin/sh

if [ $# -ne 2 ];
	then
		echo "Not Enough Parameters Entered. The command takes two parameters.\r\nfilesdr: Path to directory\r\nsearchstr: String to find in the directory"
		exit 1;
fi

filesdir="$1"
searchstr="$2"

if [ ! -d $filesdir ];
	then
		echo "$filesdir is not a directory"
		exit 1;
fi

filecount=$(find "$filesdir" -type f | wc -l)

string_found=$(grep -r "$searchstr" | wc -l)

echo "The number of files are $filecount and the number of matching lines are $string_found"

exit 0;

