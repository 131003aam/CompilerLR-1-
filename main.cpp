#include "compiler.h"
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

// 从文件读取代码
string readCodeFromFile(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "错误: 无法打开文件 '" << filename << "'" << endl;
        return "";
    }
    
    stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return buffer.str();
}

int main(int argc, char* argv[]) {
    WhileCompiler compiler;
    string code;
    string filename;
    
    // 如果提供了命令行参数，使用指定的文件
    if (argc > 1) {
        filename = argv[1];
    } else {
        filename = "2.txt";  // 默认测试文件名，可以修改为其他文件名
    }
    
    // 从文件读取代码
    code = readCodeFromFile(filename);
    
    if (code.empty()) {
        cerr << "错误: 文件为空或无法读取" << endl;
        return 1;
    }
    
    cout << "从文件读取: " << filename << endl;
    cout << "输入代码:\n" << code << "\n" << endl;
    compiler.run(code);
    return 0;
}
