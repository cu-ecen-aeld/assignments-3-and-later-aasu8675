#!/bin/sh
# Script to search for text string in files present in a directory
# Author: Aamir Suhail Burhan


filesdir="$1"    # Variable which accepts file directory
searchstr="$2"    # Variable which stores string text to be searched
num_of_args=$#    # Variable to store number of arguments passed

if [ "$num_of_args" -eq 2 ] 
then
	if [ -d "$filesdir" ]
	then
		matching_lines=$(grep -rl "$searchstr" "$filesdir" | wc -l)
		total_files=$(find "$filesdir" -type f | wc -l)
		echo "The number of files are $total_files and the number of matching lines are $matching_lines"
		exit 0
	else
		echo "Failed: Entered directory $filesdir is invalid"
		exit 1
	fi
else 
	echo "Failed: Number of arguments are incorrect"
	exit 1
fi

