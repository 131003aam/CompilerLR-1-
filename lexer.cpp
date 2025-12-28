#include "lexer.h"
#include <iostream>
#include <cctype>

using namespace std;

bool Lexer::isIdStart(char c) {
    return isalpha(c) || c == '_';
}

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

vector<Word> Lexer::performLexicalAnalysis(const string& input) {
    hasError = false;
    errorMessages.clear();
    
    vector<Word> tokens;
    int i = 0, len = input.length();
    int line = 1, col = 1;
    int startLine = 1, startCol = 1;
    
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
                tokens.push_back({ 5, "++", "自增运算符", startLine, startCol });
                i += 2;
                col += 2;
            }
            else {
                tokens.push_back({ 2, "+", "运算符", startLine, startCol });
                i++;
                col++;
            }
        }
        else if (input[i] == '-') {
            if (i + 1 < len && input[i + 1] == '-') {
                tokens.push_back({ 5, "--", "自减运算符", startLine, startCol });
                i += 2;
                col += 2;
            }
            else {
                tokens.push_back({ 2, "-", "运算符", startLine, startCol });
                i++;
                col++;
            }
        }
        else if (string("<>=").find(input[i]) != string::npos) {
            buf += input[i++];
            col++;
            if (i < len && input[i] == '=') {
                buf += input[i++];
                col++;
            }
            tokens.push_back({ 2, buf, "关系运算符", startLine, startCol });
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

