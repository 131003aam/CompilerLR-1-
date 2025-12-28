#include <iostream>
#include <vector>
#include <string>
#include <stack>
#include <set>
#include <map>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <sstream>

using namespace std;

// === 数据结构定义 ===

struct Word {
    int sym;            // 符号码
    string token;       // 词法值
    string typeLabel;   // 类型标签
};

struct Production {
    int id;
    string left;
    vector<string> right;
};

// 四元式结构体(op, arg1, arg2, result)
struct Quadruple {
    string op;
    string arg1;
    string arg2;
    string result;
    string toString() const {
        return "(" + op + ", " + arg1 + ", " + arg2 + ", " + result + ")";
    }
};

struct LR1Item {
    int prodId;
    int dotPos;
    set<string> lookahead;

    bool operator<(const LR1Item& other) const {
        if (prodId != other.prodId) return prodId < other.prodId;
        if (dotPos != other.dotPos) return dotPos < other.dotPos;
        return lookahead < other.lookahead;
    }
    bool operator==(const LR1Item& other) const {
        return prodId == other.prodId && dotPos == other.dotPos && lookahead == other.lookahead;
    }
};

enum class ActionType { SHIFT, REDUCE, ACCEPT, ERROR };

struct Action {
    ActionType type = ActionType::ERROR;
    int target = -1;
};

struct TAC {
    string op;
    string arg1;
    string arg2;
    string result;
    int addr;
};

struct SemItem {
    string name;
};

// === 字符判断函数 ===

bool isIdStart(char c) {
    return isalpha(c) || c == '_';
}

bool isIdPart(char c) {
    return isalnum(c) || c == '_';
}

// === 编译器主类 ===

class WhileCompiler {
private:
    vector<Production> productions;
    set<string> Vn, Vt;
    map<string, set<string>> firstSets;
    vector<vector<LR1Item>> states;
    map<int, map<string, Action>> actionTable;
    map<int, map<string, int>> gotoTable;

    vector<TAC> tacCode;
    vector<Quadruple> quads;
    int tempCount = 0;

    // 循环控制相关栈
    stack<int> loopAddrStack;
    stack<vector<int>> breakLists;
    stack<vector<int>> continueLists;

    string currentStepQuads; // 保存当前步骤生成的四元式字符串

    string newTemp() { return "T" + to_string(++tempCount); }

    void emit(string op, string a1, string a2, string res) {
        tacCode.push_back({ op, a1, a2, res, (int)tacCode.size() });
    }

    void emitQuad(string op, string a1, string a2, string res) {
        Quadruple q = { op, a1, a2, res };
        quads.push_back(q);
        if (!currentStepQuads.empty()) currentStepQuads += " ";
        currentStepQuads += q.toString();
    }

    void backpatch(int addr, string target) {
        if (addr >= 0 && addr < (int)tacCode.size()) {
            tacCode[addr].result = target;
        }
    }

    set<string> getFirst(const vector<string>& symbols) {
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

public:
    WhileCompiler() {
        productions = {
            {0, "S'", {"B"}},
            {1, "A", {"while", "(", "L", ")", "M", "{", "B", "}"}},
            {2, "L", {"L", "||", "M1"}},
            {3, "L", {"M1"}},
            {4, "M1", {"M1", "&&", "N"}},
            {5, "M1", {"N"}},
            {6, "N", {"!", "N"}},
            {7, "N", {"C"}},
            {8, "N", {"(", "L", ")"}},
            {9, "C", {"E", "ROP", "E"}},
            {10, "B", {"S", ";", "B"}},
            {11, "B", {"S", ";"}},
            {12, "B", {"A", "B"}},
            {13, "B", {"A"}},
            {14, "S", {"i", "=", "E"}},
            {15, "E", {"E", "+", "F"}},
            {16, "E", {"E", "-", "F"}},
            {17, "E", {"F"}},
            {18, "F", {"F", "*", "G"}},
            {19, "F", {"F", "/", "G"}},
            {20, "F", {"G"}},
            {21, "G", {"-", "G"}},
            {22, "G", {"i"}},
            {23, "G", {"n"}},
            {24, "G", {"(", "E", ")"}},
            {25, "ROP", {">"}}, {26, "ROP", {"<"}}, {27, "ROP", {"=="}},
            {28, "ROP", {">="}}, {29, "ROP", {"<="}}, {30, "ROP", {"!="}},
            {31, "G", {"i", "++"}},
            {32, "G", {"++", "i"}},
            {33, "G", {"i", "--"}},
            {34, "G", {"--", "i"}},
            {35, "S", {"G"}},
            {36, "S", {"break"}},
            {37, "S", {"continue"}},
            {38, "M", {}},
            {39, "S", {"int", "i"}},
            {40, "S", {"float", "i"}},
            {41, "S", {"int", "i", "=", "E"}},
            {42, "S", {"float", "i", "=", "E"}},
            {43, "G", {"true"}},
            {44, "G", {"false"}},
            {45, "N", {"G"}}
        };

        for (auto& p : productions) Vn.insert(p.left);
        for (auto& p : productions) {
            for (auto& s : p.right) if (!Vn.count(s)) Vt.insert(s);
        }
        Vt.insert("#");

        computeFirst();
        buildLR1Table();
    }

    void computeFirst() {
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto& p : productions) {
                set<string>& first = firstSets[p.left];
                size_t before = first.size();
                if (p.right.empty()) first.insert("epsilon");
                else if (Vt.count(p.right[0])) first.insert(p.right[0]);
                else {
                    for (auto& s : firstSets[p.right[0]]) if (s != "epsilon") first.insert(s);
                }
                if (first.size() > before) changed = true;
            }
        }
    }

    vector<LR1Item> getClosure(vector<LR1Item> items) {
        bool changed = true;
        while (changed) {
            changed = false;
            for (int i = 0; i < (int)items.size(); i++) {
                LR1Item cur = items[i];
                if (cur.dotPos >= (int)productions[cur.prodId].right.size()) continue;
                string B = productions[cur.prodId].right[cur.dotPos];
                if (Vn.count(B)) {
                    vector<string> beta;
                    for (int k = cur.dotPos + 1; k < (int)productions[cur.prodId].right.size(); k++)
                        beta.push_back(productions[cur.prodId].right[k]);
                    set<string> nextLookahead;
                    for (auto& la : cur.lookahead) {
                        vector<string> betaLa = beta; betaLa.push_back(la);
                        set<string> f = getFirst(betaLa);
                        for (auto& s : f) nextLookahead.insert(s);
                    }
                    for (int j = 0; j < (int)productions.size(); j++) {
                        if (productions[j].left == B) {
                            LR1Item newItem = { j, 0, nextLookahead };
                            auto it = find_if(items.begin(), items.end(), [&](const LR1Item& x) {
                                return x.prodId == newItem.prodId && x.dotPos == newItem.dotPos;
                                });
                            if (it == items.end()) { items.push_back(newItem); changed = true; }
                            else {
                                size_t oldSize = it->lookahead.size();
                                for (auto& s : nextLookahead) it->lookahead.insert(s);
                                if (it->lookahead.size() > oldSize) changed = true;
                            }
                        }
                    }
                }
            }
        }
        return items;
    }

    void buildLR1Table() {
        vector<LR1Item> i0 = getClosure({ {0, 0, {"#"}} });
        states.push_back(i0);
        for (int i = 0; i < (int)states.size(); i++) {
            set<string> symbols;
            for (auto& it : states[i]) {
                if (it.dotPos < (int)productions[it.prodId].right.size())
                    symbols.insert(productions[it.prodId].right[it.dotPos]);
            }
            for (auto& sym : symbols) {
                vector<LR1Item> next;
                for (auto& it : states[i]) {
                    if (it.dotPos < (int)productions[it.prodId].right.size() && productions[it.prodId].right[it.dotPos] == sym)
                        next.push_back({ it.prodId, it.dotPos + 1, it.lookahead });
                }
                next = getClosure(next);
                int nextId = -1;
                for (int k = 0; k < (int)states.size(); k++) if (states[k] == next) { nextId = k; break; }
                if (nextId == -1) { states.push_back(next); nextId = (int)states.size() - 1; }
                if (Vt.count(sym)) actionTable[i][sym] = { ActionType::SHIFT, nextId };
                else gotoTable[i][sym] = nextId;
            }
            for (auto& it : states[i]) {
                if (it.dotPos == (int)productions[it.prodId].right.size() || productions[it.prodId].right.empty()) {
                    for (auto& la : it.lookahead) {
                        if (it.prodId == 0) actionTable[i][la] = { ActionType::ACCEPT, 0 };
                        else actionTable[i][la] = { ActionType::REDUCE, it.prodId };
                    }
                }
            }
        }
        saveItemsToFile("items.txt");
        saveTableToCSV("table.csv");
    }

    void saveItemsToFile(string filename) {
        ofstream out(filename);
        if (!out) return;
        out << "LR(1) 项目集合" << endl;
        for (int i = 0; i < (int)states.size(); i++) {
            out << "I" << i << ":" << endl;
            for (auto& item : states[i]) {
                out << "  " << productions[item.prodId].left << " -> ";
                for (int k = 0; k < (int)productions[item.prodId].right.size(); k++) {
                    if (k == item.dotPos) out << ".";
                    out << productions[item.prodId].right[k] << " ";
                }
                if (item.dotPos == (int)productions[item.prodId].right.size()) out << ".";
                out << " , { ";
                for (auto& la : item.lookahead) out << la << " ";
                out << "}" << endl;
            }
            out << endl;
        }
        out.close();
    }

    void saveTableToCSV(string filename) {
        ofstream out(filename);
        if (!out) return;
        out << "State,";
        for (auto& t : Vt) out << t << ",";
        for (auto& n : Vn) if (n != "S'") out << n << ",";
        out << endl;
        for (int i = 0; i < (int)states.size(); i++) {
            out << i << ",";
            for (auto& t : Vt) {
                if (actionTable[i].count(t)) {
                    Action act = actionTable[i][t];
                    if (act.type == ActionType::SHIFT) out << "S" << act.target;
                    else if (act.type == ActionType::REDUCE) out << "r" << act.target;
                    else if (act.type == ActionType::ACCEPT) out << "acc";
                }
                out << ",";
            }
            for (auto& n : Vn) {
                if (n == "S'") continue;
                if (gotoTable[i].count(n)) out << gotoTable[i][n];
                out << ",";
            }
            out << endl;
        }
        out.close();
    }

    void run(string input) {
        vector<Word> tokens = performLexicalAnalysis(input);

        cout << "--- 词法分析结果 ---" << endl;
        cout << left << setw(15) << "Token" << setw(15) << "符号码" << "类型" << endl;
        for (auto& t : tokens) {
            if (t.sym == -1) continue;
            cout << left << setw(15) << t.token << setw(15) << t.sym << t.typeLabel << endl;
        }
        cout << string(100, '-') << endl;

        stack<int> stateStack; stateStack.push(0);
        stack<string> symbolStack; symbolStack.push("#");
        vector<SemItem> semStack;
        int ptr = 0;

        cout << left << setw(6) << "步骤" << setw(25) << "状态栈" << setw(20) << "符号栈" << setw(15) << "当前输入" << setw(15) << "动作" << "生成四元式" << endl;
        int step = 1;

        while (true) {
            currentStepQuads = "";
            int s = stateStack.top();
            Word w = tokens[ptr];

            string a;
            if (w.sym >= 36 && w.sym <= 42) a = w.token;
            else if (w.token == "true" || w.token == "false") a = w.token;
            else a = (w.sym == 0 ? "i" : (w.sym == 1 ? "n" : w.token));

            string stStr = ""; stack<int> tmpS = stateStack; vector<int> vS;
            while (!tmpS.empty()) { vS.push_back(tmpS.top()); tmpS.pop(); }
            reverse(vS.begin(), vS.end());
            for (int x : vS) stStr += to_string(x) + " ";

            string syStr = ""; stack<string> tmpSy = symbolStack; vector<string> vSy;
            while (!tmpSy.empty()) { vSy.push_back(tmpSy.top()); tmpSy.pop(); }
            reverse(vSy.begin(), vSy.end());
            for (auto& x : vSy) syStr += x + " ";

            if (!actionTable[s].count(a)) {
                cout << left << setw(6) << step << setw(25) << stStr << setw(20) << syStr << setw(15) << a << "错误: 语法不匹配" << endl;
                return;
            }
            Action act = actionTable[s][a];

            if (act.type == ActionType::SHIFT) {
                if (a == "while") {
                    loopAddrStack.push((int)tacCode.size());
                    breakLists.push(vector<int>());
                    continueLists.push(vector<int>());
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
                SemItem res = { "" };
                switch (act.target) {
                case 38: { // M -> epsilon
                    if (semStack.size() >= 2) {
                        SemItem lResult = semStack[semStack.size() - 2];
                        int jzIdx = (int)tacCode.size();
                        emit("jz", lResult.name, "", "PENDING_EXIT");
                        emitQuad("jz", lResult.name, "_", "PENDING_EXIT");
                        if (!breakLists.empty()) breakLists.top().push_back(jzIdx);
                    }
                    break;
                }
                case 1: { // A -> while ( L ) M { B }
                    int testStart = loopAddrStack.top(); loopAddrStack.pop();
                    emit("goto", "", "", "L" + to_string(testStart));
                    emitQuad("j", "_", "_", to_string(testStart));
                    int exitAddr = (int)tacCode.size();
                    vector<int> brks = breakLists.top(); breakLists.pop();
                    for (int addr : brks) backpatch(addr, "L" + to_string(exitAddr));
                    vector<int> conts = continueLists.top(); continueLists.pop();
                    for (int addr : conts) backpatch(addr, "L" + to_string(testStart));
                    break;
                }
                case 2: res.name = newTemp(); emit("||", popped[0].name, popped[2].name, res.name); emitQuad("||", popped[0].name, popped[2].name, res.name); break;
                case 4: res.name = newTemp(); emit("&&", popped[0].name, popped[2].name, res.name); emitQuad("&&", popped[0].name, popped[2].name, res.name); break;
                case 6: res.name = newTemp(); emit("!", popped[1].name, "", res.name); emitQuad("!", popped[1].name, "_", res.name); break;
                case 9: res.name = newTemp(); emit(popped[1].name, popped[0].name, popped[2].name, res.name); emitQuad(popped[1].name, popped[0].name, popped[2].name, res.name); break;
                case 14: emit(":=", popped[2].name, "", popped[0].name); emitQuad("=", popped[2].name, "_", popped[0].name); res.name = popped[0].name; break;
                case 15: case 16: case 18: case 19:
                    res.name = newTemp();
                    emit(popped[1].name, popped[0].name, popped[2].name, res.name);
                    emitQuad(popped[1].name, popped[0].name, popped[2].name, res.name);
                    break;
                case 21: res.name = newTemp(); emit("neg", popped[1].name, "", res.name); emitQuad("neg", popped[1].name, "_", res.name); break;
                case 22: case 23: res.name = popped[0].name; break;
                case 24: case 8: res.name = popped[1].name; break;
                case 31: case 32: {
                    string targetId = (act.target == 31 ? popped[0].name : popped[1].name);
                    string t = newTemp(); emit("+", targetId, "1", t); emit(":=", t, "", targetId);
                    emitQuad("+", targetId, "1", t); emitQuad("=", t, "_", targetId);
                    res.name = targetId; break;
                }
                case 33: case 34: {
                    string targetId = (act.target == 33 ? popped[0].name : popped[1].name);
                    string t = newTemp(); emit("-", targetId, "1", t); emit(":=", t, "", targetId);
                    emitQuad("-", targetId, "1", t); emitQuad("=", t, "_", targetId);
                    res.name = targetId; break;
                }
                case 36: { // break
                    if (!breakLists.empty()) {
                        int addr = (int)tacCode.size();
                        emit("goto", "", "", "PENDING_EXIT");
                        emitQuad("j", "_", "_", "PENDING_EXIT");
                        breakLists.top().push_back(addr);
                    }
                    break;
                }
                case 37: { // continue
                    if (!continueLists.empty()) {
                        int addr = (int)tacCode.size();
                        emit("goto", "", "", "PENDING_TEST");
                        emitQuad("j", "_", "_", "PENDING_TEST");
                        continueLists.top().push_back(addr);
                    }
                    break;
                }
                case 39: case 40: { // int i; float i;
                    string type = popped[0].name;
                    string id = popped[1].name;
                    emit("decl", type, "", id);
                    emitQuad("decl", type, "_", id);
                    res.name = id;
                    break;
                }
                case 41: case 42: { // int i = E;
                    string type = popped[0].name;
                    string id = popped[1].name;
                    emit("decl", type, "", id);
                    emit(":=", popped[3].name, "", id);
                    emitQuad("decl", type, "_", id);
                    emitQuad("=", popped[3].name, "_", id);
                    res.name = id;
                    break;
                }
                case 43: res.name = "true"; break;
                case 44: res.name = "false"; break;
                case 45: res.name = popped[0].name; break;
                case 35: res.name = popped[0].name; break;
                default: if (!popped.empty()) res.name = popped[0].name;
                }

                cout << left << setw(6) << step++ << setw(25) << stStr << setw(20) << syStr << setw(15) << a << setw(15) << "归约 r" + to_string(act.target) << currentStepQuads << endl;

                symbolStack.push(p.left);
                stateStack.push(gotoTable[stateStack.top()][p.left]);
                semStack.push_back(res);
            }
            else if (act.type == ActionType::ACCEPT) {
                cout << left << setw(6) << step++ << setw(25) << stStr << setw(20) << syStr << setw(15) << a << setw(15) << "ACCEPT" << endl;
                break;
            }
        }

        cout << string(100, '-') << endl;
        cout << "\n--- 生成的三地址码 (TAC) ---" << endl;
        for (auto& t : tacCode) {
            cout << "L" << left << setw(3) << t.addr << "| ";
            if (t.op == "goto") cout << "goto " << t.result << endl;
            else if (t.op == "jz") cout << "if " << t.arg1 << " == 0 goto " << t.result << endl;
            else if (t.op == "jnz") cout << "if " << t.arg1 << " != 0 goto " << t.result << endl;
            else if (t.op == "decl") cout << "decl " << t.arg1 << " " << t.result << endl;
            else if (t.op == ":=") cout << t.result << " := " << t.arg1 << endl;
            else if (t.op == "neg") cout << t.result << " := neg " << t.arg1 << endl;
            else if (t.op == "!") cout << t.result << " := ! " << t.arg1 << endl;
            else cout << t.result << " := " << t.arg1 << " " << t.op << " " << t.arg2 << endl;
        }
    }

    vector<Word> performLexicalAnalysis(string input) {
        vector<Word> tokens;
        int i = 0, len = input.length();
        while (i < len) {
            if (isspace(input[i])) { i++; continue; }
            string buf = "";
            if (isIdStart(input[i])) {
                while (i < len && isIdPart(input[i])) buf += input[i++];
                if (buf == "while") tokens.push_back({ 36, buf, "关键字" });
                else if (buf == "break") tokens.push_back({ 37, buf, "关键字" });
                else if (buf == "continue") tokens.push_back({ 38, buf, "关键字" });
                else if (buf == "int") tokens.push_back({ 39, buf, "关键字" });
                else if (buf == "float") tokens.push_back({ 40, buf, "关键字" });
                else if (buf == "true") tokens.push_back({ 41, buf, "关键字" });
                else if (buf == "false") tokens.push_back({ 42, buf, "关键字" });
                else tokens.push_back({ 0, buf, "标识符" });
            }
            else if (isdigit(input[i])) {
                int dot = 0;
                while (i < len && (isdigit(input[i]) || input[i] == '.')) {
                    if (input[i] == '.') dot++;
                    buf += input[i++];
                }
                tokens.push_back({ 1, buf, "数字" });
            }
            else if (input[i] == '&') {
                if (i + 1 < len && input[i + 1] == '&') { tokens.push_back({ 4, "&&", "逻辑运算符" }); i += 2; }
                else { buf += input[i++]; tokens.push_back({ 3, buf, "符号" }); }
            }
            else if (input[i] == '|') {
                if (i + 1 < len && input[i + 1] == '|') { tokens.push_back({ 4, "||", "逻辑运算符" }); i += 2; }
                else { buf += input[i++]; tokens.push_back({ 3, buf, "符号" }); }
            }
            else if (input[i] == '!') {
                if (i + 1 < len && input[i + 1] == '=') { tokens.push_back({ 2, "!=", "关系运算符" }); i += 2; }
                else { tokens.push_back({ 4, "!", "逻辑运算符" }); i++; }
            }
            else if (input[i] == '+') {
                if (i + 1 < len && input[i + 1] == '+') { tokens.push_back({ 5, "++", "自增运算符" }); i += 2; }
                else { tokens.push_back({ 2, "+", "运算符" }); i++; }
            }
            else if (input[i] == '-') {
                if (i + 1 < len && input[i + 1] == '-') { tokens.push_back({ 5, "--", "自减运算符" }); i += 2; }
                else { tokens.push_back({ 2, "-", "运算符" }); i++; }
            }
            else if (string("<>=").find(input[i]) != string::npos) {
                buf += input[i++];
                if (i < len && input[i] == '=') buf += input[i++];
                tokens.push_back({ 2, buf, "关系运算符" });
            }
            else if (input[i] == '*' || input[i] == '/') {
                buf += input[i++];
                tokens.push_back({ 2, buf, "算术运算符" });
            }
            else {
                buf += input[i++];
                tokens.push_back({ 3, buf, "符号" });
            }
        }
        tokens.push_back({ -1, "#", "结束符" });
        return tokens;
    }
};

int main() {
    WhileCompiler compiler;
    string code = "while ( true ) { float b_flag = 1.5 ; if_val = a_var ; while ( b < 1 ) { break ; } continue ; b = a_var ++ ; }";
    cout << "输入代码: " << code << "\n" << endl;
    compiler.run(code);
    return 0;
}
