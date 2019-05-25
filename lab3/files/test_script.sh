#!/bin/bash

gen_test_file() {
	COUNTER=1
	# 1k 16k 256k 4M 64M 1G 
	while [ $COUNTER -le 1048576 ]; do
		dd if=/dev/urandom of=test${COUNTER}k.txt
		COUNTER=$(($COUNTER*16))
	done
}

run_test_once() {
	siege -c 200 -n 20 http://192.168.10.2:8000/$1 >/dev/null 2>$1.report
}

run_test() {
	COUNTER=1
	while [ $COUNTER -le 1048576 ]; do
		run_test_once test${COUNTER}k.txt
		COUNTER=$(($COUNTER*16))
	done
}
