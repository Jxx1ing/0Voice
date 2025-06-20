#!/bin/bash
echo "hello world"

#变量
zerovoice='www.0voice.com'
echo $zerovoice

#遍历一个目录下的所有文件
for file in $(ls /home/jxx/share/); do
    echo "${file}"
done
