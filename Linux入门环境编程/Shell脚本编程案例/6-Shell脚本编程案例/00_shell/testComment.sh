#!/bin/bash
#多行注释
:<<EOF
    echo "你好"
EOF

:<<!
    echo "这是一个测试"
!

: <<'COMMENT'
    echo "0Voice"
COMMENT

: '
    echo "hello,world"
'
