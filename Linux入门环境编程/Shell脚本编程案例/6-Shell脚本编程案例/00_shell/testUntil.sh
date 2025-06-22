#!/bin/bash

# 输出0到9
int=0
until [ ! $int -lt 10 ] #注意[]前后需要空格
do
    echo $int
    int=`expr $int + 1` #不要写成 $int++，因为不支持类似C语言的写法
                        #还有注意运算符 和 数字 空格
done
