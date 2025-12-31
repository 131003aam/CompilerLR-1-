#ifndef CODEGEN_H
#define CODEGEN_H

#include "types.h"
#include <vector>
#include <stack>
#include <set>

// === 代码生成器 ===

class CodeGenerator {
private:
    vector<TAC> tacCode;
    vector<Quadruple> quads;
    int tempCount = 0;

    // 循环控制相关栈
    stack<int> loopAddrStack;
    stack<vector<int>> breakLists;
    stack<vector<int>> continueLists;

    string currentStepQuads; // 保存当前步骤生成的四元式字符串
    
    // 已声明变量集合（用于隐式声明）
    set<string> declaredVars;

    // 生成临时变量名
    string newTemp();
    
    // 回填地址
    void backpatch(int addr, const string& target);

public:
    CodeGenerator();
    
    // 生成三地址码
    void emit(const string& op, const string& a1, const string& a2, const string& res);
    
    // 生成四元式
    void emitQuad(const string& op, const string& a1, const string& a2, const string& res);
    
    // 获取当前步骤的四元式字符串
    string getCurrentStepQuads() const { return currentStepQuads; }
    void clearCurrentStepQuads() { currentStepQuads = ""; }
    
    // 循环控制
    void enterLoop();
    void exitLoop();
    void handleBreak();
    void handleContinue();
    void handleLoopCondition(const string& condition);
    
    // 语义动作
    SemItem handleProduction(int prodId, const vector<SemItem>& popped, const vector<SemItem>& semStack);
    
    // 处理循环条件（在归约 M -> epsilon 时调用）
    void handleLoopCondition(const string& condition, const vector<SemItem>& semStack);
    
    // 获取生成的三地址码
    const vector<TAC>& getTACCode() const { return tacCode; }
    const vector<Quadruple>& getQuads() const { return quads; }
    
    // 打印三地址码
    void printTAC() const;
};

#endif // CODEGEN_H

