#include <iostream>
#include <fstream>
using namespace std;

#define OUT 0
#define IN 1
# define INIT OUT

int CountWord(char* FileName){
    int status = INIT;  //状态机初始
    int word = 0;       //单词数量

    ifstream infile("b.txt");
    if(!infile) return -1;

    char ch;
    while(infile.get(ch)){
        if ((' ' == ch) || ('\n' == ch) || ('\t' == ch) || ('\"' == ch) || ('+' == ch) || 
            (',' == ch) || (';' == ch) || ('.' == ch) || ('=' == ch)){
            status = OUT;
        }else if(OUT == status){
            status = IN;
            word++;
        }
    }
    return word;
}

int main(int argc, char* argv[])
{
    if (argc < 2) return -1;     
    printf("word: %d\n", CountWord(argv[1]));

    return 0;
}  


