#!/bin/bash

printf " %-10s %-8s %-4s\n" 姓名 性别 体重kg

printf %s abc def
printf "%s\n"
printf "%s\n" abc defi

# 如果没有 arguments，那么 %s 用 NULL 代替，%d 用 0 代替
printf "%s and %d \n"
