#!/bin/bash

a=10
b=20

# -eq 相等
if [ $a -eq $b ]
then
    echo "a等于b"
else
    echo "a不等于b"
fi

# -ne 不相等
if [ $a -ne $b ]
then
    echo "a不等于b"
else
    echo "a等于b"
fi

# -gt 检测左边的数是否大于右边的数
if [ $a -gt $b ]
then
    echo "a大于b"
else
    echo "a小于b"
fi

# -lt 检测左边的数是否小于右边的数
if [ $a -lt $b ]
then
    echo "a小于b"
else
    echo "a大于b"
fi

# -ge 检测左边的数是否大于等于右边的数
if [ $a -ge $b ]
then
    echo "a大于等于b"
else
    echo "a小于b"
fi

# -le 检测左边的数是否小于等于右边的数
if [ $a -le $b ]
then
    echo "a小于等于b"
else
    echo "a大于b"
fi




