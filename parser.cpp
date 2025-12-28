#include "parser.h"
#include <fstream>
#include <algorithm>

using namespace std;

Parser::Parser() {
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

void Parser::computeFirst() {
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

vector<LR1Item> Parser::getClosure(vector<LR1Item> items) {
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

void Parser::buildLR1Table() {
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
            if (Vt.count(sym)) {
                Action act;
                act.type = ActionType::SHIFT;
                act.target = nextId;
                actionTable[i][sym] = act;
            }
            else gotoTable[i][sym] = nextId;
        }
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

void Parser::saveTableToCSV(const string& filename) {
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

