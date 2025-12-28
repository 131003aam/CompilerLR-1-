#ifndef LEXER_H
#define LEXER_H

#include "types.h"
#include <vector>

// === 词法分析器 ===

class Lexer {
private:
    bool hasError = false;
    vector<string> errorMessages;

    // 字符判断函数
    bool isIdStart(char c);
    bool isIdPart(char c);
    
    // 错误报告
    void reportLexicalError(int line, int col, char c, const string& reason);

public:
    // 执行词法分析
    vector<Word> performLexicalAnalysis(const string& input);
    
    // 获取错误信息
    bool hasErrors() const { return hasError; }
    const vector<string>& getErrorMessages() const { return errorMessages; }
    void clearErrors() { hasError = false; errorMessages.clear(); }
};

#endif // LEXER_H




