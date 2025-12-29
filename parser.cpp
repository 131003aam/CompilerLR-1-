#include "parser.h"
#include <fstream>
#include <algorithm>
#include <iostream>

using namespace std;

// Parser构造函数，初始化语法分析器
// 步骤:
//   1. 定义所有产生式规则（46个产生式）
//   2. 构建非终结符集合 Vn 和终结符集合 Vt
//   3. 计算所有非终结符的 First 集合
//   4. 构建 LR(1) 分析表
Parser::Parser() {
    // 定义所有产生式规则
    // 产生式编号从0开始，0是增广产生式S'->B
    productions = {
        {0, "S'", {"B"}},
        {1, "A", {"while", "(", "L", ")", "M", "{", "B", "}"}}, 
        {2, "L", {"L", "||", "M1"}}, // L是逻辑或表达式，左递归L||M1实现左结合
        {3, "L", {"M1"}}, 
        {4, "M1", {"M1", "&&", "N"}}, // M1是逻辑与表达式，左递归M1&&N实现左结合
        {5, "M1", {"N"}}, //N是逻辑基本单元
        {6, "N", {"!", "N"}}, //逻辑非
        {7, "N", {"C"}}, //关系运算
        {8, "N", {"(", "L", ")"}}, //括号
        {9, "C", {"E", "ROP", "E"}}, //比较运算，E是表达式，ROP是关系运算符
        {10, "B", {"S", ";", "B"}}, //左递归/右递归实现复合语句
        {11, "B", {"S", ";"}},
        {12, "B", {"A", "B"}},
        {13, "B", {"A"}},
        {14, "S", {"i", "=", "E"}}, //赋值语句
        {15, "E", {"E", "+", "F"}}, //算术运算
        {16, "E", {"E", "-", "F"}},
        {17, "E", {"F"}},
        {18, "F", {"F", "*", "G"}},
        {19, "F", {"F", "/", "G"}},
        {20, "F", {"G"}},
        {21, "G", {"-", "G"}}, //一元负号
        {22, "G", {"i"}}, //变量
        {23, "G", {"n"}}, //常量
        {24, "G", {"(", "E", ")"}}, //括号表达式
        {25, "ROP", {">"}}, {26, "ROP", {"<"}}, {27, "ROP", {"=="}}, //关系运算符
        {28, "ROP", {">="}}, {29, "ROP", {"<="}}, {30, "ROP", {"!="}},
        {31, "G", {"i", "++"}},
        {32, "G", {"++", "i"}},
        {33, "G", {"i", "--"}},
        {34, "G", {"--", "i"}},
        {35, "S", {"G"}}, //表达式语句
        {36, "S", {"break"}},
        {37, "S", {"continue"}},
        {38, "M", {}}, //用于while循环条件跳转回填，空产生式
        {39, "S", {"int", "i"}}, //类型声明
        {40, "S", {"float", "i"}},
        {41, "S", {"int", "i", "=", "E"}},
        {42, "S", {"float", "i", "=", "E"}},
        {43, "G", {"true"}}, //布尔常量
        {44, "G", {"false"}},
        {45, "N", {"G"}} //基本单元
    };

    // 构建Vn和Vt，同时保持顺序
    for (auto& p : productions) {
        if (find(VnOrder.begin(), VnOrder.end(), p.left) == VnOrder.end()) {
            Vn.insert(p.left);
            VnOrder.push_back(p.left);
        }
    }
    for (auto& p : productions) {
        for (auto& s : p.right) {
            if (!Vn.count(s) && find(VtOrder.begin(), VtOrder.end(), s) == VtOrder.end()) {
                Vt.insert(s);
                VtOrder.push_back(s);
            }
        }
    }
    if (find(VtOrder.begin(), VtOrder.end(), "#") == VtOrder.end()) {
        Vt.insert("#");
        VtOrder.push_back("#");
    }

    // 计算所有非终结符的First集合
    computeFirst();
    // 构建LR(1)分析表
    buildLR1Table();
}

//计算First集合
void Parser::computeFirst() {
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& p : productions) { //auto& p表示循环变量p是productions中的每个元素的引用
            set<string>& first = firstSets[p.left];
            size_t before = first.size();
            
            // 空产生式：epsilon∈First(A)
            if (p.right.empty()) first.insert("epsilon");
            // 右部第一个符号是终结符：a∈First(A) 
            else if (Vt.count(p.right[0])) first.insert(p.right[0]);
            // 右部第一个符号是非终结符：First(B)-{epsilon}⊆First(A)
            else {
                // First(B)-{epsilon}⊆First(A)
                for (auto& s : firstSets[p.right[0]]) if (s != "epsilon") first.insert(s);
            }
            if (first.size() > before) changed = true;
        }
    }
}

set<string> Parser::getFirst(const vector<string>& symbols) {
    set<string> res;
    for (const auto& s : symbols) {
        if (Vt.count(s)) { res.insert(s); return res; }
        bool hasEpsilon = false;
        if (firstSets.count(s)) {
            for (const auto& f : firstSets[s]) {
                if (f == "epsilon") hasEpsilon = true;
                else res.insert(f);
            }
        }
        if (!hasEpsilon) return res;
    }
    return res;
}

//计算LR(1)项目集的闭包
vector<LR1Item> Parser::getClosure(vector<LR1Item> items) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < (int)items.size(); i++) {
            LR1Item cur = items[i]; //当前项目
            if (cur.dotPos >= (int)productions[cur.prodId].right.size()) continue;
            string B = productions[cur.prodId].right[cur.dotPos]; 
            if (Vn.count(B)) {
                vector<string> beta; //β：当前项目中的非终结符后面的符号串
                for (int k = cur.dotPos + 1; k < (int)productions[cur.prodId].right.size(); k++)
                    beta.push_back(productions[cur.prodId].right[k]);
                set<string> nextLookahead; //当前项目中的展望符集合
                // 计算新的展望符
                for (auto& la : cur.lookahead) {
                    vector<string> betaLa = beta; betaLa.push_back(la);
                    set<string> f = getFirst(betaLa);
                    for (auto& s : f) nextLookahead.insert(s);
                }
                // 展开B的所有产生式
                for (int j = 0; j < (int)productions.size(); j++) {
                    if (productions[j].left == B) {
                        LR1Item newItem = { j, 0, nextLookahead }; // 圆点在最前面的新项目
                        // 合并与去重
                        auto it = find_if(items.begin(), items.end(), [&](const LR1Item& x) {
                            return x.prodId == newItem.prodId && x.dotPos == newItem.dotPos;
                            });
                        if (it == items.end()) { items.push_back(newItem); changed = true; }
                        else {
                            // 检查此时展望符还一不一样
                            size_t oldSize = it->lookahead.size();
                            for (auto& s : nextLookahead) it->lookahead.insert(s); // 合并展望符
                            if (it->lookahead.size() > oldSize) changed = true;
                        }
                    }
                }
            }
        }
    }
    return items;
}

//构建LR(1)分析表
void Parser::buildLR1Table() {
    vector<LR1Item> i0 = getClosure({ {0, 0, {"#"}} });
    states.push_back(i0); //每个states[i]是一个LR1Item集合
    for (int i = 0; i < (int)states.size(); i++) {
        set<string> symbols;
        for (auto& it : states[i]) { //找到所有可能的移进符号
            if (it.dotPos < (int)productions[it.prodId].right.size())
                symbols.insert(productions[it.prodId].right[it.dotPos]);
        }
        for (auto& sym : symbols) { //对于每个可能的移进符号，构建新的项目集
            vector<LR1Item> next;
            for (auto& it : states[i]) {
                if (it.dotPos < (int)productions[it.prodId].right.size() && productions[it.prodId].right[it.dotPos] == sym)
                    next.push_back({ it.prodId, it.dotPos + 1, it.lookahead }); //移进
            }
            next = getClosure(next);
            int nextId = -1;
            for (int k = 0; k < (int)states.size(); k++) if (states[k] == next) { nextId = k; break; }
            if (nextId == -1) { states.push_back(next); nextId = (int)states.size() - 1; }
            //更新ACTION、GOTO表
            if (Vt.count(sym)) {
                Action act; //临时对象
                act.type = ActionType::SHIFT;
                act.target = nextId;
                actionTable[i][sym] = act; //行：状态编号i；列：移进的sym；值：Action
            }
            else gotoTable[i][sym] = nextId;
        }
        // 处理归约或接受操作
        for (auto& it : states[i]) {
            if (it.dotPos == (int)productions[it.prodId].right.size() || productions[it.prodId].right.empty()) {
                for (auto& la : it.lookahead) {
                    Action act;
                    if (it.prodId == 0) {
                        act.type = ActionType::ACCEPT;
                        act.target = 0;
                    } else {
                        act.type = ActionType::REDUCE;
                        act.target = it.prodId;
                    }
                    actionTable[i][la] = act;
                }
            }
        }
    }
    saveItemsToFile("items.txt");
    saveTableToCSV("table.csv");
}

void Parser::saveItemsToFile(const string& filename) {
    ofstream out(filename);
    if (!out) {
        cerr << "错误: 无法打开文件 " << filename << endl;
        return;
    }
    out << "LR(1) 项目集合" << endl;
    for (int i = 0; i < (int)states.size(); i++) {
        out << "I" << i << ":" << endl;
        for (auto& item : states[i]) {
            out << "  " << productions[item.prodId].left << " -> ";
            // 计算最大宽度用于对齐
            int maxWidth = 0;
            for (int k = 0; k < (int)productions[item.prodId].right.size(); k++) {
                maxWidth = max(maxWidth, (int)productions[item.prodId].right[k].length());
            }
            // 输出右部，对齐显示
            for (int k = 0; k < (int)productions[item.prodId].right.size(); k++) {
                if (k == item.dotPos) {
                    out << " .";
                    // 对齐符号
                    for (int w = 0; w < maxWidth; w++) out << " ";
                } else {
                    out << " ";
                }
                out << productions[item.prodId].right[k];
                // 补齐空格对齐
                int padding = maxWidth - productions[item.prodId].right[k].length();
                for (int w = 0; w < padding; w++) out << " ";
            }
            if (item.dotPos == (int)productions[item.prodId].right.size()) {
                out << " .";
            }
            out << " , { ";
            for (auto& la : item.lookahead) out << la << " ";
            out << "}" << endl;
        }
        out << endl;
    }
    out.close();
}

void Parser::saveTableToCSV(const string& filename) {
    ofstream out(filename);
    if (!out) {
        cerr << "错误: 无法打开文件 " << filename << endl;
        return;
    }
    out << "State,";
    // 使用vector保持顺序，而不是set
    for (auto& t : VtOrder) out << t << ",";
    for (auto& n : VnOrder) if (n != "S'") out << n << ",";
    out << endl;
    for (int i = 0; i < (int)states.size(); i++) {
        out << i << ",";
        for (auto& t : VtOrder) {
            if (actionTable[i].count(t)) {
                Action act = actionTable[i][t];
                if (act.type == ActionType::SHIFT) out << "S" << act.target;
                else if (act.type == ActionType::REDUCE) out << "r" << act.target;
                else if (act.type == ActionType::ACCEPT) out << "acc";
            }
            out << ",";
        }
        for (auto& n : VnOrder) {
            if (n == "S'") continue;
            if (gotoTable[i].count(n)) out << gotoTable[i][n];
            out << ",";
        }
        out << endl;
    }
    out.close();
}
