#!/bin/bash

for i in {1..254}; do
    ping -c 2 -i 0.5 192.168.92.129.$i &> /dev/null   #把标准输出和标准错误都重定向到同一个地方
    if [ $? -eq 0 ]; then                             #eq是上一条命令的返回值 退出码为0代表ping成功
        echo "192.168.92.129.$i is up"
    else
        echo "192.168.92.129.$i is down"
    fi
done
