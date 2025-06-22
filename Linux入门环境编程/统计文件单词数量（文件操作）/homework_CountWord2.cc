#include <iostream>
#include <cctype>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
using namespace std;

int CountWord2(const char* filename){
    ifstream infile(filename);
    if(!infile) return -1;

    //清洗数据,新的数据保存在content中
    string content;
    char ch;
    while(infile.get(ch)){
        //如果字符或者数字 替换成 空格
        if(!isalpha(ch)){
            ch = ' ';
        }else if(isupper(ch)){
            //如果是大写字母，改为小写字母
            ch = tolower(ch);
        }
        
        content += ch;
    }
    infile.close();

    //词频统计
    istringstream iss(content);
    vector<pair<string, int>> WordRecord;
    string word;
    while(iss >> word){
        bool found = false;
        for(auto& OldWord: WordRecord){
            if(word == OldWord.first){
                found = true;
                OldWord.second++;
                break;
            }
        }
        //这个单词没有出现，加入vector容器中 
        if(!found){    
            WordRecord.push_back(make_pair(word, 1));
        }
    }

    //将结果写入文件中
    ofstream outfile("a.txt");
    for(const auto& result: WordRecord){
        outfile  << result.first << " " << result.second << endl;
    }
    outfile.close();

    return 0;
}

int main(int argc, char* argv[])
{
    if (argc < 2) return -1;
    CountWord2(argv[1]);
    return 0;
}
