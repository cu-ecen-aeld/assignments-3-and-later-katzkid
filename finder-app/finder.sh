#!/bin/bash  


if [ $# -ne 2 ]
then
	echo Provide arguments
	exit 1
fi

if [ -d "$1" ]
then
	echo Valid directory
	total_lines=$(grep -r "$2" "$1"| wc -l)
	num_files=$(grep -r -l "$2" "$1" | wc -l)

else
	echo Not a valid directory
	exit 1
fi

echo The number of files are $num_files and the number of matching lines are $total_lines

