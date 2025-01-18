#!/bin/sh

if [ $# -ne 2 ];
	then
		echo "Not Enough Parameters Entered. The command takes two parameters.\r\nfilesdr: Path to directory\r\nsearchstr: String to find in the directory"
		exit 1;
fi

writefile="$1"
writestr="$2"

# Try to create the file. If not successful, return with exit code 1.
if [ $(mkdir -p $writefile) -neq 0 ];
	then
		echo "Could not create file."
		exit(1);
fi

# Try to create the file. If not successful, return with exit code 1.
if [ $($writestr > $writefile) -neq 0 ];
	then
		echo "Could not write to file."
		exit(1);
fi

