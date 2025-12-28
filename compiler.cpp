#include "compiler.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace std;

// ============================================================================
// 编译器主类实现 (compiler.cpp)
// ============================================================================

// ----------------------------------------------------------------------------
// WhileCompiler 构造函数
// ----------------------------------------------------------------------------
// 功能: 初始化编译器，创建词法分析器、语法分析器和代码生成器
// 说明: Parser 构造函数会自动构建 LR(1) 分析表
WhileCompiler::WhileCompiler() {
}

// ----------------------------------------------------------------------------
// run - 执行完整的编译过程
// ----------------------------------------------------------------------------
// 功能: 将源代码编译为三地址码
// 流程:
//   1. 词法分析：将源代码转换为 Token 序列
//   2. 语法分析：使用 LR(1) 方法验证语法并生成代码
//   3. 输出结果：显示词法分析结果、语法分析过程和生成的三地址码
void WhileCompiler::run(const string& input) {
    hasError = false;
    errorMessages.clear();
    lexer.clearErrors();
    
    // ========== 阶段 1: 词法分析 ==========
    // 将源代码字符串分解为词法单元序列
    vector<Word> tokens = lexer.performLexicalAnalysis(input);

    cout << "--- 词法分析结果 ---" << endl;
    cout << left << setw(15) << "Token" << setw(10) << "符号码" << setw(15) << "类型" << setw(8) << "行号" << setw(8) << "列号" << endl;
    for (auto& t : tokens) {
        if (t.sym == -1) continue;
        cout << left << setw(15) << t.token << setw(10) << t.sym << setw(15) << t.typeLabel << setw(8) << t.line << setw(8) << t.col << endl;
    }
    cout << string(100, '-') << endl;
    
    if (lexer.hasErrors()) {
        cout << "\n--- 错误汇总 ---" << endl;
        for (auto& err : lexer.getErrorMessages()) {
            cout << err << endl;
        }
        cout << string(100, '-') << endl;
        return;
    }

    // ========== 阶段 2: 语法分析和代码生成 ==========
    // 初始化 LR(1) 分析栈
    stack<int> stateStack;      // 状态栈：存储分析过程中的状态编号
    stateStack.push(0);         // 初始状态为 0
    stack<string> symbolStack;  // 符号栈：存储已识别的符号
    symbolStack.push("#");      // 栈底标记
    vector<SemItem> semStack;   // 语义栈：存储语义信息（变量名、临时变量等）
    int ptr = 0;                // 输入指针：指向当前处理的 Token

    // 获取分析表和相关数据结构
    const auto& actionTable = parser.getActionTable();  // Action 表：状态 × 终结符 → 动作
    const auto& gotoTable = parser.getGotoTable();      // Goto 表：状态 × 非终结符 → 新状态
    const auto& productions = parser.getProductions();   // 产生式集合
    const auto& states = parser.getStates();            // LR(1) 项目集集合
    const auto& Vt = parser.getVt();                    // 终结符集合
    const auto& Vn = parser.getVn();                    // 非终结符集合

    // 输出语法分析过程表头
    cout << left << setw(6) << "步骤" << setw(25) << "状态栈" << setw(20) << "符号栈" << setw(12) << "当前输入" << setw(15) << "动作" << "生成四元式" << endl;
    int step = 1;

    // LR(1) 分析主循环
    while (true) {
        codegen.clearCurrentStepQuads();  // 清空当前步骤的四元式字符串
        
        // 获取当前状态和输入符号
        int s = stateStack.top();         // 当前状态
        Word w = tokens[ptr];             // 当前输入 Token

        // 将 Token 转换为分析表中使用的符号
        // 关键字（36-42）直接使用 token 值
        // 标识符统一映射为 "i"，数字映射为 "n"
        // 其他符号直接使用 token 值
        string a;
        if (w.sym >= 36 && w.sym <= 42) a = w.token;  // 关键字
        else if (w.token == "true" || w.token == "false") a = w.token;  // 布尔值
        else a = (w.sym == 0 ? "i" : (w.sym == 1 ? "n" : w.token));  // 标识符→"i", 数字→"n", 其他→原值

        string stStr = ""; 
        stack<int> tmpS = stateStack; 
        vector<int> vS;
        while (!tmpS.empty()) { 
            vS.push_back(tmpS.top()); 
            tmpS.pop(); 
        }
        reverse(vS.begin(), vS.end());
        for (int x : vS) {
            if (stStr.length() > 0) stStr += " ";
            stStr += to_string(x);
        }
        // 限制状态栈显示长度
        if (stStr.length() > 23) {
            stStr = "..." + stStr.substr(stStr.length() - 20);
        }

        string syStr = ""; 
        stack<string> tmpSy = symbolStack; 
        vector<string> vSy;
        while (!tmpSy.empty()) { 
            vSy.push_back(tmpSy.top()); 
            tmpSy.pop(); 
        }
        reverse(vSy.begin(), vSy.end());
        for (auto& x : vSy) {
            if (syStr.length() > 0) syStr += " ";
            syStr += x;
        }
        // 限制符号栈显示长度
        if (syStr.length() > 18) {
            syStr = "..." + syStr.substr(syStr.length() - 15);
        }

        // 查找 Action 表中的动作
        if (!actionTable.at(s).count(a)) {
            // ========== 语法错误处理 ==========
            hasError = true;
            string errorMsg = "[语法错误] 第" + to_string(w.line) + "行, 第" + to_string(w.col) + "列: ";
            errorMsg += "遇到意外的符号 '" + a + "'";
            
            // 收集期望的符号（用于错误提示）
            // 从当前状态的所有项目中提取期望的符号
            set<string> expected;
            for (auto& it : states[s]) {
                if (it.dotPos < (int)productions[it.prodId].right.size()) {
                    string nextSym = productions[it.prodId].right[it.dotPos];
                    if (Vt.count(nextSym)) expected.insert(nextSym);
                }
            }
            // 也检查归约项
            for (auto& it : states[s]) {
                if (it.dotPos == (int)productions[it.prodId].right.size() || productions[it.prodId].right.empty()) {
                    for (auto& la : it.lookahead) {
                        expected.insert(la);
                    }
                }
            }
            
            if (!expected.empty()) {
                errorMsg += "\n期望的符号: ";
                bool first = true;
                for (auto& exp : expected) {
                    if (!first) errorMsg += ", ";
                    errorMsg += "'" + exp + "'";
                    first = false;
                }
            }
            
            errorMessages.push_back(errorMsg);
            cout << "\n" << errorMsg << endl;
            cout << left << setw(6) << step << setw(25) << stStr << setw(20) << syStr << setw(12) << a << "错误: 语法不匹配" << endl;
            return;
        }
        // 获取动作
        Action act = actionTable.at(s).at(a);

        // ========== 移进动作 ==========
        if (act.type == ActionType::SHIFT) {
            // 如果遇到 while 关键字，进入循环处理
            if (a == "while") {
                codegen.enterLoop();  // 记录循环开始地址，初始化 break/continue 列表
            }
            // 输出分析步骤
            cout << left << setw(6) << step++ << setw(25) << stStr << setw(20) << syStr << setw(12) << a << setw(15) << "移进 S" + to_string(act.target) << endl;
            // 执行移进：将新状态和符号压入栈
            stateStack.push(act.target);
            symbolStack.push(a);
            semStack.push_back({ w.token });  // 保存 Token 的原始值（用于代码生成）
            ptr++;  // 移动输入指针
        }
        // ========== 归约动作 ==========
        else if (act.type == ActionType::REDUCE) {
            // 获取产生式
            Production p = productions[act.target];
            
            // 从栈中弹出产生式右部长度的元素
            vector<SemItem> popped;
            for (int k = 0; k < (int)p.right.size(); k++) {
                stateStack.pop();           // 弹出状态
                symbolStack.pop();          // 弹出符号
                popped.push_back(semStack.back());  // 保存语义信息
                semStack.pop_back();
            }
            reverse(popped.begin(), popped.end());  // 反转顺序（栈是后进先出）
            
            // 执行语义动作：生成代码
            SemItem res = codegen.handleProduction(act.target, popped, semStack);

            cout << left << setw(6) << step++ << setw(25) << stStr << setw(20) << syStr << setw(12) << a << setw(15) << "归约 r" + to_string(act.target) << codegen.getCurrentStepQuads() << endl;

            symbolStack.push(p.left);
            stateStack.push(gotoTable.at(stateStack.top()).at(p.left));
            semStack.push_back(res);
        }
        else if (act.type == ActionType::ACCEPT) {
            cout << left << setw(6) << step++ << setw(25) << stStr << setw(20) << syStr << setw(12) << a << setw(15) << "ACCEPT" << endl;
            break;
        }
    }

    cout << string(100, '-') << endl;
    
    if (hasError) {
        cout << "\n--- 错误汇总 ---" << endl;
        for (auto& err : errorMessages) {
            cout << err << endl;
        }
        return;
    }
    
    // 打印生成的三地址码
    codegen.printTAC();
}


