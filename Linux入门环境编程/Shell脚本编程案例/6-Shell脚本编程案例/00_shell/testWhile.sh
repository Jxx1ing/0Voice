#!/bin/bash

:<< EOF
int=0
while (( $int <= 5 ))
do
    echo $int
    let int++
done
EOF

while read FILM #FILM是键盘输入
do
    echo "是的$FILM读取成功"
done


