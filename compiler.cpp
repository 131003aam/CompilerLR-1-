#include "compiler.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace std;

WhileCompiler::WhileCompiler() {
}

void WhileCompiler::run(const string& input) {
    hasError = false;
    errorMessages.clear();
    lexer.clearErrors();
    
    // 词法分析
    vector<Word> tokens = lexer.performLexicalAnalysis(input);

    cout << "--- 词法分析结果 ---" << endl;
    cout << left << setw(15) << "Token" << setw(15) << "符号码" << setw(15) << "类型" << setw(10) << "行号" << "列号" << endl;
    for (auto& t : tokens) {
        if (t.sym == -1) continue;
        cout << left << setw(15) << t.token << setw(15) << t.sym << setw(15) << t.typeLabel << setw(10) << t.line << t.col << endl;
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

    // 语法分析
    stack<int> stateStack; 
    stateStack.push(0);
    stack<string> symbolStack; 
    symbolStack.push("#");
    vector<SemItem> semStack;
    int ptr = 0;

    const auto& actionTable = parser.getActionTable();
    const auto& gotoTable = parser.getGotoTable();
    const auto& productions = parser.getProductions();
    const auto& states = parser.getStates();
    const auto& Vt = parser.getVt();
    const auto& Vn = parser.getVn();

    cout << left << setw(6) << "步骤" << setw(25) << "状态栈" << setw(20) << "符号栈" << setw(15) << "当前输入" << setw(15) << "动作" << "生成四元式" << endl;
    int step = 1;

    while (true) {
        codegen.clearCurrentStepQuads();
        int s = stateStack.top();
        Word w = tokens[ptr];

        string a;
        if (w.sym >= 36 && w.sym <= 42) a = w.token;
        else if (w.token == "true" || w.token == "false") a = w.token;
        else a = (w.sym == 0 ? "i" : (w.sym == 1 ? "n" : w.token));

        string stStr = ""; 
        stack<int> tmpS = stateStack; 
        vector<int> vS;
        while (!tmpS.empty()) { 
            vS.push_back(tmpS.top()); 
            tmpS.pop(); 
        }
        reverse(vS.begin(), vS.end());
        for (int x : vS) stStr += to_string(x) + " ";

        string syStr = ""; 
        stack<string> tmpSy = symbolStack; 
        vector<string> vSy;
        while (!tmpSy.empty()) { 
            vSy.push_back(tmpSy.top()); 
            tmpSy.pop(); 
        }
        reverse(vSy.begin(), vSy.end());
        for (auto& x : vSy) syStr += x + " ";

        if (!actionTable.at(s).count(a)) {
            hasError = true;
            string errorMsg = "[语法错误] 第" + to_string(w.line) + "行, 第" + to_string(w.col) + "列: ";
            errorMsg += "遇到意外的符号 '" + a + "'";
            
            // 收集期望的符号
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
            cout << left << setw(6) << step << setw(25) << stStr << setw(20) << syStr << setw(15) << a << "错误: 语法不匹配" << endl;
            return;
        }
        Action act = actionTable.at(s).at(a);

        if (act.type == ActionType::SHIFT) {
            if (a == "while") {
                codegen.enterLoop();
            }
            cout << left << setw(6) << step++ << setw(25) << stStr << setw(20) << syStr << setw(15) << a << setw(15) << "移进 S" + to_string(act.target) << endl;
            stateStack.push(act.target);
            symbolStack.push(a);
            semStack.push_back({ w.token });
            ptr++;
        }
        else if (act.type == ActionType::REDUCE) {
            Production p = productions[act.target];
            vector<SemItem> popped;
            for (int k = 0; k < (int)p.right.size(); k++) {
                stateStack.pop();
                symbolStack.pop();
                popped.push_back(semStack.back());
                semStack.pop_back();
            }
            reverse(popped.begin(), popped.end());
            
            SemItem res = codegen.handleProduction(act.target, popped, semStack);

            cout << left << setw(6) << step++ << setw(25) << stStr << setw(20) << syStr << setw(15) << a << setw(15) << "归约 r" + to_string(act.target) << codegen.getCurrentStepQuads() << endl;

            symbolStack.push(p.left);
            stateStack.push(gotoTable.at(stateStack.top()).at(p.left));
            semStack.push_back(res);
        }
        else if (act.type == ActionType::ACCEPT) {
            cout << left << setw(6) << step++ << setw(25) << stStr << setw(20) << syStr << setw(15) << a << setw(15) << "ACCEPT" << endl;
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

