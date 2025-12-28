#include "codegen.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace std;

CodeGenerator::CodeGenerator() : tempCount(0) {
}

string CodeGenerator::newTemp() {
    return "T" + to_string(++tempCount);
}

void CodeGenerator::emit(const string& op, const string& a1, const string& a2, const string& res) {
    tacCode.push_back({ op, a1, a2, res, (int)tacCode.size() });
}

void CodeGenerator::emitQuad(const string& op, const string& a1, const string& a2, const string& res) {
    Quadruple q = { op, a1, a2, res };
    quads.push_back(q);
    if (!currentStepQuads.empty()) currentStepQuads += " ";
    currentStepQuads += q.toString();
}

void CodeGenerator::backpatch(int addr, const string& target) {
    if (addr >= 0 && addr < (int)tacCode.size()) {
        tacCode[addr].result = target;
    }
}

void CodeGenerator::enterLoop() {
    // 记录循环条件判断代码的起始地址
    // 这个地址是条件表达式代码开始的位置，continue 应该跳转到这里
    loopAddrStack.push((int)tacCode.size());
    breakLists.push(vector<int>());
    continueLists.push(vector<int>());
}

void CodeGenerator::exitLoop() {
    if (loopAddrStack.empty()) return;
    // testStart 是循环条件判断代码的起始地址（即外层 while 的开始标签）
    // 这个地址在 enterLoop() 时记录，正好是条件表达式代码开始的位置
    int testStart = loopAddrStack.top();
    loopAddrStack.pop();
    // 生成跳转到循环开始的指令（跳转到条件判断）
    emit("goto", "", "", "L" + to_string(testStart));
    emitQuad("j", "_", "_", to_string(testStart));
    // exitAddr 是循环结束的地址
    int exitAddr = (int)tacCode.size();
    // 回填 break：所有 break 语句跳转到循环结束标签
    vector<int> brks = breakLists.top();
    breakLists.pop();
    for (int addr : brks) backpatch(addr, "L" + to_string(exitAddr));
    // 回填 continue：所有 continue 语句跳转到条件判断的开始（testStart）
    vector<int> conts = continueLists.top();
    continueLists.pop();
    for (int addr : conts) backpatch(addr, "L" + to_string(testStart));
}

void CodeGenerator::handleBreak() {
    if (!breakLists.empty()) {
        int addr = (int)tacCode.size();
        emit("goto", "", "", "PENDING_EXIT");
        emitQuad("j", "_", "_", "PENDING_EXIT");
        breakLists.top().push_back(addr);
    }
}

void CodeGenerator::handleContinue() {
    // continue 语句应该跳转到当前循环的条件判断开始位置
    // 这个地址会在 exitLoop() 时被回填为循环条件判断的开始标签
    if (!continueLists.empty()) {
        int addr = (int)tacCode.size();
        emit("goto", "", "", "PENDING_TEST");
        emitQuad("j", "_", "_", "PENDING_TEST");
        continueLists.top().push_back(addr);
    }
}

void CodeGenerator::handleLoopCondition(const string& condition, const vector<SemItem>& semStack) {
    if (semStack.size() >= 2) {
        SemItem lResult = semStack[semStack.size() - 2];
        int jzIdx = (int)tacCode.size();
        emit("jz", lResult.name, "", "PENDING_EXIT");
        emitQuad("jz", lResult.name, "_", "PENDING_EXIT");
        if (!breakLists.empty()) breakLists.top().push_back(jzIdx);
    }
}

SemItem CodeGenerator::handleProduction(int prodId, const vector<SemItem>& popped, const vector<SemItem>& semStack) {
    SemItem res = { "" };
    
    switch (prodId) {
    case 38: { // M -> epsilon
        // 在归约 M -> epsilon 时生成条件判断的 jz 指令
        // 注意：条件表达式的计算代码（L 的代码）在此之前已经生成
        // enterLoop() 记录的 testStart 就是条件表达式代码开始的位置，也就是循环条件判断的开始
        if (semStack.size() >= 2) {
            SemItem lResult = semStack[semStack.size() - 2];
            // 优化：如果条件是常量 true，则不需要条件判断（无限循环）
            if (lResult.name == "true") {
                // 对于 while(true)，不需要生成条件判断，直接进入循环体
                // break 语句会在 exitLoop() 中正确回填到循环结束标签
            } else {
                int jzIdx = (int)tacCode.size();
                emit("jz", lResult.name, "", "PENDING_EXIT");
                emitQuad("jz", lResult.name, "_", "PENDING_EXIT");
                if (!breakLists.empty()) breakLists.top().push_back(jzIdx);
            }
        }
        break;
    }
    case 1: { // A -> while ( L ) M { B }
        exitLoop();
        break;
    }
    case 2: 
        res.name = newTemp(); 
        emit("||", popped[0].name, popped[2].name, res.name); 
        emitQuad("||", popped[0].name, popped[2].name, res.name); 
        break;
    case 4: 
        res.name = newTemp(); 
        emit("&&", popped[0].name, popped[2].name, res.name); 
        emitQuad("&&", popped[0].name, popped[2].name, res.name); 
        break;
    case 6: 
        res.name = newTemp(); 
        emit("!", popped[1].name, "", res.name); 
        emitQuad("!", popped[1].name, "_", res.name); 
        break;
    case 9: 
        res.name = newTemp(); 
        emit(popped[1].name, popped[0].name, popped[2].name, res.name); 
        emitQuad(popped[1].name, popped[0].name, popped[2].name, res.name); 
        break;
    case 14: 
        emit(":=", popped[2].name, "", popped[0].name); 
        emitQuad("=", popped[2].name, "_", popped[0].name); 
        res.name = popped[0].name; 
        break;
    case 15: case 16: case 18: case 19:
        res.name = newTemp();
        emit(popped[1].name, popped[0].name, popped[2].name, res.name);
        emitQuad(popped[1].name, popped[0].name, popped[2].name, res.name);
        break;
    case 21: 
        res.name = newTemp(); 
        emit("neg", popped[1].name, "", res.name); 
        emitQuad("neg", popped[1].name, "_", res.name); 
        break;
    case 22: case 23: 
        res.name = popped[0].name; 
        break;
    case 24: case 8: 
        res.name = popped[1].name; 
        break;
    case 31: { // i++ (后缀自增)
        // 后缀自增：先保存原值，再自增，然后返回原值
        string targetId = popped[0].name;
        string oldValue = newTemp();
        emit(":=", targetId, "", oldValue);  // 保存原值
        emitQuad("=", targetId, "_", oldValue);
        string t = newTemp();
        emit("+", targetId, "1", t);         // 计算新值
        emit(":=", t, "", targetId);         // 自增
        emitQuad("+", targetId, "1", t);
        emitQuad("=", t, "_", targetId);
        res.name = oldValue;                  // 返回原值
        break;
    }
    case 32: { // ++i (前缀自增)
        // 前缀自增：先自增，然后返回新值
        string targetId = popped[1].name;
        string t = newTemp();
        emit("+", targetId, "1", t);         // 计算新值
        emit(":=", t, "", targetId);         // 自增
        emitQuad("+", targetId, "1", t);
        emitQuad("=", t, "_", targetId);
        res.name = targetId;                  // 返回新值（自增后的值）
        break;
    }
    case 33: { // i-- (后缀自减)
        // 后缀自减：先保存原值，再自减，然后返回原值
        string targetId = popped[0].name;
        string oldValue = newTemp();
        emit(":=", targetId, "", oldValue);  // 保存原值
        emitQuad("=", targetId, "_", oldValue);
        string t = newTemp();
        emit("-", targetId, "1", t);         // 计算新值
        emit(":=", t, "", targetId);         // 自减
        emitQuad("-", targetId, "1", t);
        emitQuad("=", t, "_", targetId);
        res.name = oldValue;                  // 返回原值
        break;
    }
    case 34: { // --i (前缀自减)
        // 前缀自减：先自减，然后返回新值
        string targetId = popped[1].name;
        string t = newTemp();
        emit("-", targetId, "1", t);         // 计算新值
        emit(":=", t, "", targetId);         // 自减
        emitQuad("-", targetId, "1", t);
        emitQuad("=", t, "_", targetId);
        res.name = targetId;                  // 返回新值（自减后的值）
        break;
    }
    case 36: { // break
        handleBreak();
        break;
    }
    case 37: { // continue
        handleContinue();
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
    case 43: 
        res.name = "true"; 
        break;
    case 44: 
        res.name = "false"; 
        break;
    case 45: 
        res.name = popped[0].name; 
        break;
    case 35: 
        res.name = popped[0].name; 
        break;
    default: 
        if (!popped.empty()) res.name = popped[0].name;
    }
    
    return res;
}

void CodeGenerator::printTAC() const {
    cout << "\n--- 生成的三地址码 (TAC) ---" << endl;
    for (auto& t : tacCode) {
        cout << "L" << right << setw(3) << t.addr << " | ";
        if (t.op == "goto") {
            cout << "goto " << t.result << endl;
        }
        else if (t.op == "jz") {
            cout << "if " << left << setw(10) << t.arg1 << " == 0 goto " << t.result << endl;
        }
        else if (t.op == "jnz") {
            cout << "if " << left << setw(10) << t.arg1 << " != 0 goto " << t.result << endl;
        }
        else if (t.op == "decl") {
            cout << "decl " << left << setw(8) << t.arg1 << " " << t.result << endl;
        }
        else if (t.op == ":=") {
            cout << left << setw(12) << t.result << " := " << t.arg1 << endl;
        }
        else if (t.op == "neg") {
            cout << left << setw(12) << t.result << " := neg " << t.arg1 << endl;
        }
        else if (t.op == "!") {
            cout << left << setw(12) << t.result << " := ! " << t.arg1 << endl;
        }
        else {
            cout << left << setw(12) << t.result << " := " << setw(10) << t.arg1 << " " << setw(4) << t.op << " " << t.arg2 << endl;
        }
    }
}

