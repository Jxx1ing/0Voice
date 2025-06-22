#!/bin/bash

ifconfig > file
cat file | grep -oP 'inet \K([0-9]{1,3}\.){3}[0-9]{1,3}' | grep -v '^127\.' #\K 之后不要有空格
