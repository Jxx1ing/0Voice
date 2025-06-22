#!/bin/bash

a=10
b=20

#加法
val=`expr $a + $b`
echo "a + b : $val"

#减法
val=`expr $a - $b`
echo " a - b : $val"

#乘法
val=`expr $a \* $b`
echo "a * b : $val"

#除法
val=`expr $b % $a`
echo "a / b : $val"

#等于
if [ $a == $b ]
then
    echo "a 等于 b"
fi
if [ $a != $b ]
then
    echo "a 不等于 b"
fi

