#include "compiler.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace std;

// 语法错误诊断函数
static string diagnoseSyntaxError(const string& currentSymbol, const set<string>& expected, 
                                  const stack<string>& symbolStack, const vector<Word>& tokens, int ptr) {
    // 是否缺少分号
    if (expected.count(";")) {
        // 检查当前符号是否是语句的延续
        if (currentSymbol != ";" && currentSymbol != "#") {
            return "缺少分号 ';'。建议：在语句末尾添加分号";
        }
    }
    
    // 是否缺少右括号
    if (expected.count(")")) {
        // 检查符号栈中是否有未匹配的左括号
        stack<string> tmpCheck = symbolStack;  //复制栈
        vector<string> symbols; //创建一个向量
        while (!tmpCheck.empty()) {
            symbols.push_back(tmpCheck.top()); //将栈顶符号存入symbols
            tmpCheck.pop();
        }
        reverse(symbols.begin(), symbols.end()); //这样就与源代码里的顺序一样了
        
        int openParens = 0, closeParens = 0; //统计计数
        for (const auto& sym : symbols) {
            if (sym == "(") openParens++;
            else if (sym == ")") closeParens++;
        }
        
        if (openParens > closeParens) {
            return "缺少右括号 ')'。建议：检查是否有未闭合的左括号 '('";
        }
    }
    
    // 是否缺少右花括号
    if (expected.count("}")) {
        stack<string> tmpCheck = symbolStack;
        vector<string> symbols;
        while (!tmpCheck.empty()) {
            symbols.push_back(tmpCheck.top());
            tmpCheck.pop();
        }
        reverse(symbols.begin(), symbols.end());
        
        int openBraces = 0, closeBraces = 0;
        for (const auto& sym : symbols) {
            if (sym == "{") openBraces++;
            else if (sym == "}") closeBraces++;
        }
        
        if (openBraces > closeBraces) {
            // 这里只能接收到传参进来的符号栈，具体定位在主循环中
            return "缺少右花括号 '}'。建议：检查是否有未闭合的左花括号 '{'";
        }
    }
    
    // 检查运算符位置错误
    if (currentSymbol == "+" || currentSymbol == "-" || currentSymbol == "*" || currentSymbol == "/") {
        // 如果期望的是标识符、数字或左括号，可能是运算符位置错误
        if (expected.count("i") || expected.count("n") || expected.count("(")) {
            return "运算符位置错误：'" + currentSymbol + "' 出现在不期望的位置。建议：检查表达式语法";
        }
    }
    
    // 检查关键字拼写错误（如果当前符号是标识符，但期望的是关键字）
    set<string> keywords = {"while", "break", "continue", "int", "float", "true", "false"};
    if (currentSymbol == "i" && !expected.count("i")) {
        // 当前是标识符，但期望的不是标识符，可能是关键字拼写错误
        for (const auto& kw : keywords) {
            if (expected.count(kw)) {
                return "可能是关键字拼写错误。当前是标识符，但期望关键字 '" + kw + "'";
            }
        }
    }
    
    // 检查是否在表达式中间遇到意外的符号
    if (expected.count("i") || expected.count("n") || expected.count("(") || 
        expected.count("true") || expected.count("false")) {
        if (currentSymbol == "}" || currentSymbol == ";" || currentSymbol == ")") {
            return "表达式不完整。建议：检查表达式是否缺少操作数或运算符";
        }
    }
    
    return "";  // 未识别到特定模式，返回空字符串
}

WhileCompiler::WhileCompiler() {
}

void WhileCompiler::run(const string& input) {
    hasError = false;
    errorMessages.clear(); 
    lexer.clearErrors();
    
    // 阶段1:词法分析
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

    // 阶段2:语法分析和代码生成
    // 初始化LR(1)分析栈
    stack<int> stateStack;      // 状态栈：存储分析过程中的状态编号
    stateStack.push(0);         // 初始状态为 0
    stack<string> symbolStack;  // 符号栈：存储已识别的符号
    symbolStack.push("#");      // 栈底标记
    vector<SemItem> semStack;   // 语义栈：存储语义信息（变量名、临时变量等）
    stack<int> braceLineStack; // 代码块位置栈：记录每个{的行号
    int ptr = 0;                // 输入指针：指向当前处理的Token

    // 获取分析表和相关数据结构
    const auto& actionTable = parser.getActionTable();  // Action表：状态×终结符->动作
    const auto& gotoTable = parser.getGotoTable();      // Goto表：状态×非终结符->新状态
    const auto& productions = parser.getProductions();   // 产生式集合
    const auto& states = parser.getStates();            // LR(1)项目集集合
    const auto& Vt = parser.getVt();                    // 终结符集合
    const auto& Vn = parser.getVn();                    // 非终结符集合

    // 输出语法分析过程表头
    cout << left << setw(6) << "步骤" << setw(25) << "状态栈" << setw(20) << "符号栈" << setw(12) << "当前输入" << setw(15) << "动作" << endl;
    int step = 1;

    // LR(1)分析主循环
    while (true) {
        codegen.clearCurrentStepQuads();  // 清空当前步骤的四元式字符串
        
        // 获取当前状态和输入符号
        int s = stateStack.top();         // 当前状态
        Word w = tokens[ptr];             // 当前输入Token

        // 将Token转换为分析表中使用的符号
        // 标识符统一映射为 "i"，数字映射为 "n"
        // 关键字（36-42）或其他符号直接使用token值
        string a;
        if (w.sym >= 36 && w.sym <= 42) a = w.token;  // 关键字
        else if (w.token == "true" || w.token == "false") a = w.token;  // 布尔值
        else a = (w.sym == 0 ? "i" : (w.sym == 1 ? "n" : w.token));  // 标识符"i", 数字"n", 其他原值

        // 状态栈显示
        string stStr = ""; 
        stack<int> tmpS = stateStack; 
        vector<int> vS; //临时容器
        while (!tmpS.empty()) { 
            vS.push_back(tmpS.top()); //将栈顶状态存入vS
            tmpS.pop(); 
        }
        reverse(vS.begin(), vS.end()); //这样就与源代码里的顺序一样了
        for (int x : vS) {
            if (stStr.length() > 0) stStr += " ";
            stStr += to_string(x); //将状态转换为字符串并存入stStr
        }
        // 限制状态栈显示长度
        if (stStr.length() > 23) {
            stStr = "..." + stStr.substr(stStr.length() - 20);
        }

        // 符号栈显示
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

        // 查找Action表中的动作
        if (!actionTable.at(s).count(a)) {
            // ========== 语法错误处理 ==========
            hasError = true;
            string errorMsg;
            
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
            
            // 文件结束符特殊处理：如果遇到文件结束符#且仍在代码块内，优先报告缺少}
            if (a == "#") {
                // 检查符号栈中是否有未闭合的{
                // 从栈底到栈顶遍历，统计{和}的匹配情况
                stack<string> tmpCheck = symbolStack;
                vector<string> symbols;  // 从栈底到栈顶的符号序列
                while (!tmpCheck.empty()) {
                    symbols.push_back(tmpCheck.top());
                    tmpCheck.pop();
                }
                reverse(symbols.begin(), symbols.end());  // 反转得到从栈底到栈顶的顺序
                
                // 统计{和}的数量
                int openBraces = 0;
                int closeBraces = 0;
                for (const auto& sym : symbols) {
                    if (sym == "{") openBraces++;
                    else if (sym == "}") closeBraces++;
                }
                
                // 如果{的数量大于}的数量，或者期望的符号中包含}，说明缺少右花括号
                if (openBraces > closeBraces || expected.count("}")) {
                    errorMsg = "[语法错误] 缺少右花括号'}'";
                    // 如果位置栈不为空，提示未匹配的{的位置
                    if (!braceLineStack.empty()) {
                        int unclosedBraceLine = braceLineStack.top();
                        errorMsg += "\n提示：从第 " + to_string(unclosedBraceLine) + " 行开始的 '{' 未找到匹配的 '}'";
                    }
                    errorMessages.push_back(errorMsg);
                    cout << "\n" << errorMsg << endl;
                    cout << left << setw(6) << step << setw(25) << stStr << setw(20) << syStr << setw(12) << a << "错误: 缺少右花括号" << endl;
                    return;
                }
            }
            
            // 常规错误处理
            errorMsg = "[语法错误] 第" + to_string(w.line) + "行, 第" + to_string(w.col) + "列: ";
            errorMsg += "遇到意外的符号 '" + a + "'";
            
            // 尝试诊断常见错误模式
            string diagnosis = diagnoseSyntaxError(a, expected, symbolStack, tokens, ptr);
            if (!diagnosis.empty()) {
                errorMsg += "\n诊断: " + diagnosis;
            }
            
            // 如果期望的符号中包含}，且位置栈不为空，提示未匹配的 { 的位置
            if (expected.count("}") && !braceLineStack.empty()) {
                int unclosedBraceLine = braceLineStack.top();
                errorMsg += "\n提示：从第 " + to_string(unclosedBraceLine) + " 行开始的 '{' 未找到匹配的 '}'";
            }
            
            // 格式化期望符号列表（分组显示）
            if (!expected.empty()) {
                // 分组：关键字、运算符、分隔符、其他
                vector<string> keywords, operators, separators, others;
                set<string> kwSet = {"while", "break", "continue", "int", "float", "true", "false"};
                set<string> opSet = {"+", "-", "*", "/", "++", "--", "&&", "||", "!", 
                                    ">", "<", "==", ">=", "<=", "!=", "="};
                set<string> sepSet = {"(", ")", "{", "}", ";", ","};
                
                for (const auto& exp : expected) {
                    if (kwSet.count(exp)) keywords.push_back(exp);
                    else if (opSet.count(exp)) operators.push_back(exp);
                    else if (sepSet.count(exp)) separators.push_back(exp);
                    else others.push_back(exp);
                }
                
                errorMsg += "\n期望的符号: ";
                bool hasContent = false;
                
                if (!keywords.empty()) {
                    errorMsg += "关键字(";
                    for (size_t i = 0; i < keywords.size(); i++) {
                        if (i > 0) errorMsg += ", ";
                        errorMsg += "'" + keywords[i] + "'";
                    }
                    errorMsg += ")";
                    hasContent = true;
                }
                
                if (!operators.empty()) {
                    if (hasContent) errorMsg += ", ";
                    errorMsg += "运算符(";
                    for (size_t i = 0; i < operators.size(); i++) {
                        if (i > 0) errorMsg += ", ";
                        errorMsg += "'" + operators[i] + "'";
                    }
                    errorMsg += ")";
                    hasContent = true;
                }
                
                if (!separators.empty()) {
                    if (hasContent) errorMsg += ", ";
                    errorMsg += "分隔符(";
                    for (size_t i = 0; i < separators.size(); i++) {
                        if (i > 0) errorMsg += ", ";
                        errorMsg += "'" + separators[i] + "'";
                    }
                    errorMsg += ")";
                    hasContent = true;
                }
                
                if (!others.empty()) {
                    if (hasContent) errorMsg += ", ";
                    for (size_t i = 0; i < others.size(); i++) {
                        if (i > 0) errorMsg += ", ";
                        errorMsg += "'" + others[i] + "'";
                    }
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
            // 如果遇到while关键字，进入循环处理
            if (a == "while") {
                codegen.enterLoop();  // 记录循环开始地址，初始化break/continue列表
            }
            // 跟踪代码块位置：遇到{时记录行号
            if (a == "{") {
                braceLineStack.push(w.line);
            }
            // 遇到}时弹出对应的{
            else if (a == "}") {
                if (!braceLineStack.empty()) {
                    braceLineStack.pop();
                }
            }
            // 输出分析步骤
            cout << left << setw(6) << step++ << setw(25) << stStr << setw(20) << syStr << setw(12) << a << setw(15) << "移进 S" + to_string(act.target) << endl;
            // 执行移进：将新状态和符号压入栈
            stateStack.push(act.target);
            symbolStack.push(a);
            semStack.push_back({ w.token });  // 保存Token的原始值（用于代码生成）
            ptr++;  // 移动输入指针
        }
        // ========== 归约动作 ==========
        else if (act.type == ActionType::REDUCE) {
            // 获取产生式
            Production p = productions[act.target];
            
            // 从栈中弹出产生式右部长度的元素
            vector<SemItem> popped;
            for (int k = 0; k < (int)p.right.size(); k++) {
                stateStack.pop();  // 弹出状态
                symbolStack.pop();  // 弹出符号
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


