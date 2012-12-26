#!/bin/sh

name="signal handling"

test="\033[01;38m$name\033[00m test has"
make clean all > /dev/null 2>&1
result=$(grep "FAILED" result.log | awk '{print $4}')
if [ "" = "$result" ] && [ -f result.log ]; then
        echo "$test \033[01;32mpassed\033[00m"
        make clean>/dev/null
else
        echo "$test \033[01;31mfailed on signal $result\033[00m"
fi