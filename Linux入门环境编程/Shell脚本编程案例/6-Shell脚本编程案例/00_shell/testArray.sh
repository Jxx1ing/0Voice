#!/bin/bash
array_name=("zhangsan" "lisi" "wangwu" 3.14 true 12)    #bash是弱类型，数组的值可以是不同类型
echo ${array_name[0]}
echo ${array_name[4]}
echo ${array_name[@]}   #使用@可以获取数组中的所有元素

#获取数组的长度
length=${#array_name[@]}
length2=${#array_name[*]}
echo $length
echo $length2
#获取单个元素的长度
length3=${#array_name[4]}
echo $length3
#使用循环打出每个元素的长度
for ((index=0; index<length; index++)); do
    echo "Length of element $index (${array_name[index]}): ${#array_name[index]}"
done
