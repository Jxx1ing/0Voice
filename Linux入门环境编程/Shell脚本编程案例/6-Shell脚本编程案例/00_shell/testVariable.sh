#!/bin/bash

: '
domain="www.0Voice.com"
#echo ${domain}
'

: '
for skill in C CPP Linux shell; do
    echo "I am good at ${skill}Code"
    echo "I am good at $skillCode"
done
'

: '
url="http://www.google.com"
readonly url
url="http://www.0Voice.com"
'

url="http://www.0Voice.com"
unset url
echo $url #变量被删除后不能再次被使用
