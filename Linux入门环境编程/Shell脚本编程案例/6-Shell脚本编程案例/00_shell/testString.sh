#!/bin/bash

: '
your_name="0_voice"
str="I know you, \"$your_name\"! \n"
echo -e $str
'

: <<'COMMENT'
#使用双引号
your_name="0voice"
greeting_2="hello,"$your_name"!"
greeting_3="hello, ${your_name} !" #双引号会替换变量 "hello," 和 变量之间不可以空格
echo $greeting_2 $greeting_3
#使用单引号
greeting_2='hello,'$your_name'!' #单引号不会替换变量'hello,' 和 变量之间不可以空格  
greeting_3='hello, ${your_name} !'
echo $greeting_2 $greeting_3
COMMENT

: <<'COMMENT'
#获取字符串长度
string='abcd'
echo ${#string}
COMMENT

: <<'COMMENT'
#提取子字符串
string="0voice is a great college"
echo ${string:1:4}  #下标 截取的字符长度（即从字符串第2个字符开始截取4个字符）
COMMENT

string="0voice is a great college"
echo `expr index "$string" io`  #i或者o先出现的位置，非下标（从1开始）








