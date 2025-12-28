#include "lexer.h"
#include <iostream>
#include <cctype>

using namespace std;

// ============================================================================
// 词法分析器实现 (lexer.cpp)
// ============================================================================

// ----------------------------------------------------------------------------
// isIdStart - 判断字符是否为标识符的起始字符
// ----------------------------------------------------------------------------
// 参数: c - 待判断的字符
// 返回: true 如果是字母或下划线，false 否则
// 说明: 标识符必须以字母或下划线开头
bool Lexer::isIdStart(char c) {
    return isalpha(c) || c == '_';
}

// ----------------------------------------------------------------------------
// isIdPart - 判断字符是否为标识符的组成部分
// ----------------------------------------------------------------------------
// 参数: c - 待判断的字符
// 返回: true 如果是字母、数字或下划线，false 否则
// 说明: 标识符的后续字符可以是字母、数字或下划线
bool Lexer::isIdPart(char c) {
    return isalnum(c) || c == '_';
}

void Lexer::reportLexicalError(int line, int col, char c, const string& reason) {
    hasError = true;
    string msg = "[词法错误] 第" + to_string(line) + "行, 第" + to_string(col) + "列: " + reason;
    if (c != '\0') {
        msg += " (遇到字符: '";
        if (c == '\n') msg += "\\n";
        else if (c == '\t') msg += "\\t";
        else if (c == '\r') msg += "\\r";
        else msg += c;
        msg += "')";
    }
    errorMessages.push_back(msg);
    cout << "错误: " << msg << endl;
}

// ----------------------------------------------------------------------------
// performLexicalAnalysis - 执行词法分析的主函数
// ----------------------------------------------------------------------------
// 功能: 将源代码字符串分解为词法单元序列
// 算法: 使用状态机方式逐个字符扫描，识别不同类型的词法单元
// 返回: 词法单元向量
// 
// 识别规则:
//   1. 标识符/关键字: 以字母或下划线开头，后跟字母、数字或下划线
//   2. 数字: 整数或小数（支持小数点）
//   3. 运算符: 单字符（+, -, *, /）或双字符（++, --, &&, ||, ==, !=, >=, <=）
//   4. 分隔符: 括号、大括号、分号等
//   5. 关键字: while, break, continue, int, float, true, false
vector<Word> Lexer::performLexicalAnalysis(const string& input) {
    hasError = false;
    errorMessages.clear();
    
    vector<Word> tokens;
    int i = 0, len = input.length();
    int line = 1, col = 1;           // 当前行号和列号
    int startLine = 1, startCol = 1; // Token 起始位置（用于错误报告）
    
    // 主扫描循环：逐个字符处理
    while (i < len) {
        startLine = line;
        startCol = col;
        
        if (input[i] == '\n') {
            line++;
            col = 1;
            i++;
            continue;
        }
        if (isspace(input[i])) {
            if (input[i] == '\t') col += 4;
            else col++;
            i++;
            continue;
        }
        
        // 处理单行注释 //
        if (input[i] == '/' && i + 1 < len && input[i + 1] == '/') {
            // 跳过到行尾
            while (i < len && input[i] != '\n') {
                i++;
            }
            // 如果遇到换行符，在下一轮循环中处理
            continue;
        }
        
        // 处理多行注释 /* */
        if (input[i] == '/' && i + 1 < len && input[i + 1] == '*') {
            i += 2;  // 跳过 /*
            col += 2;
            bool foundEnd = false;
            while (i < len) {
                if (input[i] == '\n') {
                    line++;
                    col = 1;
                    i++;
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
                }
            }
            if (!foundEnd) {
                reportLexicalError(startLine, startCol, '\0', "多行注释未闭合，缺少 '*/'");
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
        else if (isdigit(input[i])) {
            int dot = 0;
            while (i < len && (isdigit(input[i]) || input[i] == '.')) {
                if (input[i] == '.') {
                    if (dot > 0) {
                        reportLexicalError(line, col, input[i], "数字中不能有多个小数点");
                        break;
                    }
                    dot++;
                }
                buf += input[i++];
                col++;
            }
            if (buf.back() == '.') {
                reportLexicalError(line, col - 1, '.', "数字不能以小数点结尾");
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
                reportLexicalError(line, col, input[i], "非法字符 '&'，期望 '&&'");
                buf += input[i++];
                tokens.push_back({ 3, buf, "符号", startLine, startCol });
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
                reportLexicalError(line, col, input[i], "非法字符 '|'，期望 '||'");
                buf += input[i++];
                tokens.push_back({ 3, buf, "符号", startLine, startCol });
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
                // 自增运算符 ++，符号码 5
                tokens.push_back({ 5, "++", "自增运算符", startLine, startCol });
                i += 2;
                col += 2;
            }
            else {
                // 算术运算符 +，符号码 2
                tokens.push_back({ 2, "+", "算术运算符", startLine, startCol });
                i++;
                col++;
            }
        }
        else if (input[i] == '-') {
            if (i + 1 < len && input[i + 1] == '-') {
                // 自减运算符 --，符号码 5
                tokens.push_back({ 5, "--", "自减运算符", startLine, startCol });
                i += 2;
                col += 2;
            }
            else {
                // 算术运算符 -，符号码 2
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
            tokens.push_back({ 3, buf, "符号", startLine, startCol });
            col++;
        }
        else {
            // 非法字符
            reportLexicalError(line, col, input[i], "非法字符");
            buf += input[i++];
            tokens.push_back({ 3, buf, "符号", startLine, startCol });
            col++;
        }
    }
    tokens.push_back({ -1, "#", "结束符", line, col });
    return tokens;
}


