#!/bin/bash

while read name extra
do
	echo $name | grep -q '^#'
	if [ $? -eq 0 ]
	then
		continue
	fi
	path=`cat /proc/$$/cgroup | cut -d ':' -f 3`
	./pathtest $name $path
done < /proc/cgroups
