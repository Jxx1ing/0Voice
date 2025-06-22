#!/bin/bash

:<< EOF
# $#传递到脚本的参数个数    ./testCommand.sh 1 2 3(结果是3) 
echo "参数个数是 $#"
EOF

:<< EOF
# $*以一个字符串显示所有传递的参数(不加引号会拆分)  ./testCommand.sh "hello wrold" abc(结果是hello world abc)
echo “所有参数是 $*”
EOF

:<< EOF
# $$脚本当前运行的进程ID    ./testCommand.sh(结果是PID)
echo "当前脚本的进程ID $$"
EOF

:<< EOF
# $!后台运行的最后一个进程ID    ./testCommand.sh(参考上一条 这里没有模拟出)
echo "后台运行的最后一个进程ID $!"
EOF

:<< EOF
# $@每个参数独立输出(在引号中时，保留独立性) ./testCommand.sh "hello world" abc(结果是"hello world" 换行 abc)
echo "逐个参数输出"
for arg in "$@"; do
    echo "$arg"
done
EOF

:<< EOF
# $-：显示当前 Shell 的选项（如是否交互式、是否允许扩展等）
echo "Shell 当前选项是 $-"
EOF

# $?：上一条命令的返回值（退出状态码）
ls /not_exist_dir
echo "上一条命令的返回状态码是 $?"
ls ping.sh
echo "上一条命令的返回状态码是 $?"











