#include "lexer.h"
#include <iostream>
#include <cctype>

using namespace std;

// 制表符宽度（一个\t视为4列宽，用于列号计算）
static const int TAB_WIDTH = 4;

// 标识符必须以字母或下划线开头
bool Lexer::isIdStart(char c) {
    return isalpha(c) || c == '_';
}

// 标识符的后续字符可以是字母、数字或下划线
bool Lexer::isIdPart(char c) {
    return isalnum(c) || c == '_';
}


// 词法错误报告函数
void Lexer::reportLexicalError(int line, int col, char c, const string& reason) {
    hasError = true;
    string msg = "[词法错误] 第" + to_string(line) + "行, 第" + to_string(col) + "列: " + reason;
    if (c != '\0') { // 如果传入的不是空字符，则需要输出具体的错误字符
        msg += " (遇到字符: '";
        if (c == '\n') msg += "\\n"; //文本形式输出
        else if (c == '\t') msg += "\\t";
        else if (c == '\r') msg += "\\r";
        else msg += c; // 否则直接拼接
        msg += "')";
    }
    errorMessages.push_back(msg);
    cout << "错误: " << msg << endl;
}

// 返回的token是vector对象本身。vector内部的堆内存：已被转移/直接构造在返回对象中
vector<Word> Lexer::performLexicalAnalysis(const string& input) {
    hasError = false; // 重置错误标志
    errorMessages.clear(); // 清空错误信息
    
    vector<Word> tokens;
    int i = 0, len = input.length();
    int line = 1, col = 1;           // 当前行号和列号
    int startLine = 1, startCol = 1; // Token起始位置（用于错误报告）
    
    // 主扫描循环：逐个字符处理
    while (i < len) { // 遍历输入字符串中的每个字符
        startLine = line;
        startCol = col;
        
        if (input[i] == '\n') { // 换行符
            line++;
            col = 1;
            i++;
            continue;
        }
        if (isspace(input[i])) { // 制表符或空白字符
            if (input[i] == '\t') {
                // 制表符：移动到下一个制表符停止位
                // 计算下一个制表符停止位置：((col-1)/TAB_WIDTH+1)*TAB_WIDTH+1
                int nextTabStop = ((col - 1) / TAB_WIDTH + 1) * TAB_WIDTH + 1;
                col = nextTabStop;
            } else {
                col++;
            }
            i++; // 移动到下一个字符
            continue;
        }
        
        // 单行注释 //
        if (input[i] == '/' && i + 1 < len && input[i + 1] == '/') {
            // 跳过到行尾
            while (i < len && input[i] != '\n') {
                i++;
            }
            // 如果遇到换行符，会在下一轮循环中被吃掉
            continue;
        }
        
        // 多行注释 /* */
        if (input[i] == '/' && i + 1 < len && input[i + 1] == '*') {
            int commentStartLine = startLine;
            int commentStartCol = startCol;
            i += 2;  // 跳过 /*
            col += 2;
            bool foundEnd = false;
            int lastLine = line, lastCol = col;  // 记录最后扫描到的位置
            while (i < len) {
                if (input[i] == '\n') {
                    line++;
                    col = 1;
                    i++;
                    lastLine = line;
                    lastCol = col;
                }
                else if (input[i] == '*' && i + 1 < len && input[i + 1] == '/') {
                    // 找到注释结束
                    i += 2;  // 跳过 */
                    col += 2;
                    foundEnd = true;
                    break;
                }
                else {
                    i++;
                    col++;
                    lastLine = line;
                    lastCol = col;
                }
            }
            if (!foundEnd) {
                // 显示注释开始位置和文件结束位置
                string msg = "多行注释未闭合：注释从第" + to_string(commentStartLine) + "行第" + 
                            to_string(commentStartCol) + "列开始（/*），但未找到结束标记（*/）";
                if (lastLine > commentStartLine) {
                    msg += "。注释跨越了" + to_string(lastLine - commentStartLine + 1) + "行，在文件末尾仍未闭合";
                }
                msg += "。提示：从第 " + to_string(commentStartLine) + " 行开始的 '/*' 未找到匹配的 '*/'";
                reportLexicalError(commentStartLine, commentStartCol, '\0', msg);
            }
            continue;
        }
        
        string buf = "";
        if (isIdStart(input[i])) {
            while (i < len && isIdPart(input[i])) {
                buf += input[i++];
                col++;
            }
            if (buf == "while") tokens.push_back({ 36, buf, "关键字", startLine, startCol });
            else if (buf == "break") tokens.push_back({ 37, buf, "关键字", startLine, startCol });
            else if (buf == "continue") tokens.push_back({ 38, buf, "关键字", startLine, startCol });
            else if (buf == "int") tokens.push_back({ 39, buf, "关键字", startLine, startCol });
            else if (buf == "float") tokens.push_back({ 40, buf, "关键字", startLine, startCol });
            else if (buf == "true") tokens.push_back({ 41, buf, "关键字", startLine, startCol });
            else if (buf == "false") tokens.push_back({ 42, buf, "关键字", startLine, startCol });
            else tokens.push_back({ 0, buf, "标识符", startLine, startCol });
        }
        else if (isdigit(input[i]) || (input[i] == '.' && i + 1 < len && isdigit(input[i + 1]))) {
            bool startsWithDot = (input[i] == '.');
            int dot = 0; // 小数点计数
            int firstDotLine = startLine, firstDotCol = startCol;  // 待会记录第一个小数点的位置
            
            // 如果以小数点开头，先添加0到buf，但不移动i指针（因为还要读取这个点号）
            if (startsWithDot) { // 以小数点开头
                buf += '0';
                dot++;
                firstDotLine = line;
                firstDotCol = col;
            }
            
            while (i < len && (isdigit(input[i]) || input[i] == '.')) {
                if (input[i] == '.') {
                    if (dot > 0) {
                        // 遇到第二个小数点，报告错误并跳过剩余的错误部分
                        reportLexicalError(startLine, startCol, input[i], 
                            "数字中不能有多个小数点（数字从第" + to_string(startLine) + "行第" + to_string(startCol) + 
                            "列开始，第一个小数点在第" + to_string(firstDotLine) + "行第" + to_string(firstDotCol) + "列）");
                        dot = 2;  // 以此标记有多个小数点
                        // 跳过剩余的错误部分：继续读取直到遇到非数字非点号的字符
                        i++;  // 跳过当前的点号
                        col++;
                        while (i < len && (isdigit(input[i]) || input[i] == '.')) {
                            i++;
                            col++;
                        }
                        // 跳出内层循环，不再处理这个错误的数字
                        break;
                    }
                    dot++; // 第一次遇到小数点
                    firstDotLine = line;
                    firstDotCol = col;
                }
                buf += input[i++];
                col++;
            }
            
            // 如果有多个小数点（dot > 1），已经报告错误并跳过，不添加token
            if (dot > 1) {
                // 直接进入下一轮外层循环
                continue;
            }
            
            if (buf.back() == '.') {
                // 数字以小数点结尾
                reportLexicalError(startLine, startCol, '.', 
                    "数字不能以小数点结尾（数字从第" + to_string(startLine) + "行第" + to_string(startCol) + "列开始）");
            }
            
            tokens.push_back({ 1, buf, "数字", startLine, startCol });
        }
        else if (input[i] == '&') {
            if (i + 1 < len && input[i + 1] == '&') {
                tokens.push_back({ 4, "&&", "逻辑运算符", startLine, startCol });
                i += 2;
                col += 2;
            }
            else {
                reportLexicalError(startLine, startCol, input[i], 
                    "缺少运算符：期望 '&&'（逻辑与），但遇到单个'&'。建议：检查是否遗漏了第二个'&'");
                buf += input[i++];
                tokens.push_back({ 3, buf, "非法符号", startLine, startCol });
                col++;
            }
        }
        else if (input[i] == '|') {
            if (i + 1 < len && input[i + 1] == '|') {
                tokens.push_back({ 4, "||", "逻辑运算符", startLine, startCol });
                i += 2;
                col += 2;
            }
            else {
                reportLexicalError(startLine, startCol, input[i], 
                    "缺少运算符：期望 '||'（逻辑或），但遇到单个 '|'。建议：检查是否遗漏了第二个 '|'");
                buf += input[i++];
                tokens.push_back({ 3, buf, "非法符号", startLine, startCol });
                col++;
            }
        }
        else if (input[i] == '!') {
            if (i + 1 < len && input[i + 1] == '=') {
                tokens.push_back({ 2, "!=", "关系运算符", startLine, startCol });
                i += 2;
                col += 2;
            }
            else {
                tokens.push_back({ 4, "!", "逻辑运算符", startLine, startCol });
                i++;
                col++;
            }
        }
        else if (input[i] == '+') {
            if (i + 1 < len && input[i + 1] == '+') {
                // 自增运算符++，符号码5
                tokens.push_back({ 5, "++", "自增运算符", startLine, startCol });
                i += 2;
                col += 2;
            }
            else {
                // 算术运算符+，符号码2
                tokens.push_back({ 2, "+", "算术运算符", startLine, startCol });
                i++;
                col++;
            }
        }
        else if (input[i] == '-') {
            if (i + 1 < len && input[i + 1] == '-') {
                // 自减运算符--，符号码5
                tokens.push_back({ 5, "--", "自减运算符", startLine, startCol });
                i += 2;
                col += 2;
            }
            else {
                // 算术运算符-，符号码2
                tokens.push_back({ 2, "-", "算术运算符", startLine, startCol });
                i++;
                col++;
            }
        }
        else if (input[i] == '<' || input[i] == '>') {
            buf += input[i++];
            col++;
            if (i < len && input[i] == '=') {
                buf += input[i++];
                col++;
            }
            tokens.push_back({ 2, buf, "关系运算符", startLine, startCol });
        }
        else if (input[i] == '=') {
            buf += input[i++];
            col++;
            if (i < len && input[i] == '=') {
                buf += input[i++];
                col++;
                tokens.push_back({ 2, buf, "关系运算符", startLine, startCol });
            }
            else {
                tokens.push_back({ 2, buf, "赋值运算符", startLine, startCol });
            }
        }
        else if (input[i] == '*' || input[i] == '/') {
            buf += input[i++];
            tokens.push_back({ 2, buf, "算术运算符", startLine, startCol });
            col++;
        }
        else if (input[i] == '(' || input[i] == ')' || input[i] == '{' || input[i] == '}' || 
                 input[i] == ';' || input[i] == ',' || input[i] == '.') {
            buf += input[i++];
            tokens.push_back({ 3, buf, "分隔符", startLine, startCol });
            col++;
        }
        else {
            // 非法字符：提供更具体的错误信息
            string charDesc = "";
            if (input[i] == '\0') charDesc = "空字符";
            else if (input[i] < 32) charDesc = "控制字符（ASCII码: " + to_string((int)input[i]) + "）"; //ASCII码小于32的为 不可见的控制字符
            else charDesc = string("'") + input[i] + "'"; // 普通非法字符直接拼接
            
            reportLexicalError(startLine, startCol, input[i], 
                "非法字符 " + charDesc + "。建议：检查是否使用了不支持的字符，或是否遗漏了运算符/分隔符");
            buf += input[i++];
            tokens.push_back({ 3, buf, "非法符号", startLine, startCol });
            col++;
        }
    }
    tokens.push_back({ -1, "#", "结束符", line, col });
    return tokens;
}