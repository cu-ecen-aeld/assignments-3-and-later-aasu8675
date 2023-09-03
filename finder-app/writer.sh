#!/bin/sh
# Script to create a file with a specific text string
# Author: Aamir Suhail Burhan


writefile="$1"  # Variable to store path
writestr="$2"  # Variable to store string which will be added to the file
num_of_args=$# #  Variable to store number of arguments which will be passed in the script

if [ $num_of_args -eq 2 ]
then
	mkdir -p "$(dirname "${writefile}")"  # Create directory
	echo "${writestr}" > "${writefile}"  # Write content to the file
	if [ $? -eq 0 ]
	then
		echo "Success: Test script wrote ${writestr} into ${writefile}"
		exit 0
	else
		echo "Error: Failed to write into file"
		exit 1
	fi
else
	echo "Error: Number of arguments entered should be 2"
	exit 1
fi
