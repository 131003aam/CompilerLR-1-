#ifndef COMPILER_H
#define COMPILER_H

#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include <string>
#include <vector>
#include <stack>

// === 编译器主类 ===

class WhileCompiler {
private:
    Lexer lexer;
    Parser parser;
    CodeGenerator codegen;
    
    // 错误处理
    bool hasError = false;
    vector<string> errorMessages;

public:
    WhileCompiler();
    
    // 运行编译器
    void run(const string& input);
    
    // 错误处理
    bool hasErrors() const { return hasError || lexer.hasErrors(); }
    const vector<string>& getErrorMessages() const { return errorMessages; }
};

#endif // COMPILER_H





