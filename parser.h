#ifndef PARSER_H
#define PARSER_H

#include "types.h"
#include <vector>
#include <set>
#include <map>

// === LR(1) 语法分析器 ===

class Parser {
private:
    vector<Production> productions;
    set<string> Vn, Vt;  // 非终结符和终结符集合
    map<string, set<string>> firstSets;
    vector<vector<LR1Item>> states;
    map<int, map<string, Action>> actionTable;
    map<int, map<string, int>> gotoTable;

    // 计算 First 集
    void computeFirst();
    set<string> getFirst(const vector<string>& symbols);
    
    // LR(1) 项目集闭包
    vector<LR1Item> getClosure(vector<LR1Item> items);
    
    // 构建 LR(1) 分析表
    void buildLR1Table();
    
    // 保存分析表到文件
    void saveItemsToFile(const string& filename);
    void saveTableToCSV(const string& filename);

public:
    Parser();
    
    // 获取分析表
    const map<int, map<string, Action>>& getActionTable() const { return actionTable; }
    const map<int, map<string, int>>& getGotoTable() const { return gotoTable; }
    const vector<Production>& getProductions() const { return productions; }
    const vector<vector<LR1Item>>& getStates() const { return states; }
    const set<string>& getVt() const { return Vt; }
    const set<string>& getVn() const { return Vn; }
};

#endif // PARSER_H

