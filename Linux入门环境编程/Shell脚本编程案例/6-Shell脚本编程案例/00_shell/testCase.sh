#!/bin/bash

while : 
do
    echo '输入1到4之间的数字'
    echo '你输入的数字为：'
    read aNum
    case $aNum in
        1) echo '选择1'
        ;;
        2) echo '选择2'
        ;;
        3) echo '选择3'
        ;;
        4) echo ‘选择4’
        ;;

        *)echo '未输入1到4的数字'
            echo ‘游戏结束’
            continue; #break;也可以
        ;;
    esac
done
