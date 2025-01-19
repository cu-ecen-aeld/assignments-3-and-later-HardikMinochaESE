#!/bin/sh

# Check if there are required number of parameters.
# If not, return with exit code 1
if [ $# -ne 2 ];
	then
		echo "Not Enough Parameters Entered. The command takes two parameters.\r\nfilesdr: Path to directory\r\nsearchstr: String to find in the directory"
		exit 1;
fi

s
writefile="$1"
writestr="$2"


# Extract directory path and try to make directory. If not successful, return with exit code 1.
directory_extracted=$(dirname "$writefile")
mkdir -p "$directory_extracted"
if [ $? -ne 0 ];
	then
		echo "Could not create directory."
		echo "Exit code 1"
		exit 1;
fi

# Try writing the string to the file. If not successful, return with exit code 1.
echo $writestr > $writefile
if [ $? -ne 0 ];
	then
		echo "Could not write to file."
		echo "Exit code 1"
		exit 1;
fi


