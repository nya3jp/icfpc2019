#!/bin/sh

for i in $(seq 1 150);
do
	id=`printf %03d $i`
	echo ${i}
	echo ${id}

	bazel run main < ../official/part-1-initial/prob-${id}.desc > prob-${id}.sol
done
