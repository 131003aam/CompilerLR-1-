#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <vector>
#include <set>

using namespace std;

// === 数据结构定义 ===

// 词法单元
struct Word {
    int sym;            // 符号码
    string token;       // 词法值
    string typeLabel;   // 类型标签
    int line;           // 行号
    int col;            // 列号
};

// 产生式
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

// LR(1) 项目
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

// 动作类型
enum class ActionType { SHIFT, REDUCE, ACCEPT, ERROR };

// 分析表动作
struct Action {
    ActionType type = ActionType::ERROR;
    int target = -1;
};

// 三地址码
struct TAC {
    string op;
    string arg1;
    string arg2;
    string result;
    int addr;
};

// 语义栈项
struct SemItem {
    string name;
};

#endif // TYPES_H

