#!/bin/bash

writefile="$1"
writestr="$2"

if [ $# -ne 2 ]
then
	echo Insufficient arguments
	exit 1
fi

dir_path=$(dirname "$writefile")

mkdir -p "$dir_path"

if echo "$writestr" > "$writefile" 2>/dev/null
then
	echo Write successful
else
	echo File could not be created
	exit 1
fi
