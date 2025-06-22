#!/bin/bash

funcWithReturn(){
    echo "请输入第一个数字"
    read num1
    echo "请输入第二个数字"
    read num2
    return $(($num1 + $num2))
}

funcWithReturn
echo "输入的两个数字之和是 $? !"

