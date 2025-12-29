#include "codegen.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace std;

// 临时变量生成
CodeGenerator::CodeGenerator() : tempCount(0) { // 计数
}

string CodeGenerator::newTemp() {
    return "T" + to_string(++tempCount);
}

// 生成三地址码指令
void CodeGenerator::emit(const string& op, const string& a1, const string& a2, const string& res) {
    tacCode.push_back({ op, a1, a2, res, (int)tacCode.size() });
}

// 生成四元式
void CodeGenerator::emitQuad(const string& op, const string& a1, const string& a2, const string& res) {
    Quadruple q = { op, a1, a2, res };
    quads.push_back(q);
    if (!currentStepQuads.empty()) currentStepQuads += " ";
    currentStepQuads += q.toString();
}

// 回填跳转地址
void CodeGenerator::backpatch(int addr, const string& target) {
    if (addr >= 0 && addr < (int)tacCode.size()) {
        tacCode[addr].result = target;
    }
}

// 处理循环开始
void CodeGenerator::enterLoop() {
    // 记录循环条件判断代码的起始地址，continue的跳转地址
    loopAddrStack.push((int)tacCode.size());
    breakLists.push(vector<int>());
    continueLists.push(vector<int>());
}

// 处理循环结束
void CodeGenerator::exitLoop() {
    if (loopAddrStack.empty()) return;
    // 即外层while的开始标签
    int testStart = loopAddrStack.top();
    loopAddrStack.pop();
    // 生成跳转到循环开始的指令（跳转到条件判断）
    emit("goto", "", "", "L" + to_string(testStart));
    emitQuad("j", "_", "_", to_string(testStart));
    // exitAddr是循环结束的地址
    int exitAddr = (int)tacCode.size();
    // 回填break：所有break语句跳转到循环结束标签
    vector<int> brks = breakLists.top();
    breakLists.pop();
    for (int addr : brks) backpatch(addr, "L" + to_string(exitAddr));
    // 回填continue：所有continue语句跳转到条件判断的开始（testStart）
    vector<int> conts = continueLists.top();
    continueLists.pop();
    for (int addr : conts) backpatch(addr, "L" + to_string(testStart));
}

// 处理break语句
void CodeGenerator::handleBreak() {
    if (!breakLists.empty()) {
        int addr = (int)tacCode.size();
        emit("goto", "", "", "PENDING_EXIT");
        emitQuad("j", "_", "_", "PENDING_EXIT");
        breakLists.top().push_back(addr);
    }
}

// 处理continue语句
void CodeGenerator::handleContinue() {
    if (!continueLists.empty()) {
        int addr = (int)tacCode.size();
        emit("goto", "", "", "PENDING_TEST");
        emitQuad("j", "_", "_", "PENDING_TEST");
        continueLists.top().push_back(addr);
    }
}

// 处理循环条件
void CodeGenerator::handleLoopCondition(const string& condition, const vector<SemItem>& semStack) {
    if (semStack.size() >= 2) {
        SemItem lResult = semStack[semStack.size() - 2];
        int jzIdx = (int)tacCode.size();
        emit("jz", lResult.name, "", "PENDING_EXIT");
        emitQuad("jz", lResult.name, "_", "PENDING_EXIT");
        if (!breakLists.empty()) breakLists.top().push_back(jzIdx);
    }
}

// 处理产生式归约时的语义动作
SemItem CodeGenerator::handleProduction(int prodId, const vector<SemItem>& popped, const vector<SemItem>& semStack) {
    SemItem res = { "" };
    
    switch (prodId) {
    case 38: { // M->epsilon
        // enterLoop()记录的testStart就是条件表达式代码开始的位置，也就是循环条件判断的开始
        if (semStack.size() >= 2) {
            SemItem lResult = semStack[semStack.size() - 2]; // 条件表达式结果
            // 优化：如果条件是常量true，则不需要条件判断（无限循环）
            if (lResult.name == "true") { // while(true)
            } else {
                int jzIdx = (int)tacCode.size();
                emit("jz", lResult.name, "", "PENDING_EXIT");
                emitQuad("jz", lResult.name, "_", "PENDING_EXIT");
                if (!breakLists.empty()) breakLists.top().push_back(jzIdx);
            }
        }
        break;
    }
    case 1: { // A->while(L)M{B}，处理循环结束
        exitLoop();
        break;
    }
    case 2: //2,4,6逻辑运算
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
    case 9: //关系运算，C->E ROP E，返回临时变量
        res.name = newTemp(); 
        emit(popped[1].name, popped[0].name, popped[2].name, res.name); 
        emitQuad(popped[1].name, popped[0].name, popped[2].name, res.name); 
        break;
    case 14: //赋值语句，S->i=E，返回左边的变量名
        emit(":=", popped[2].name, "", popped[0].name); 
        emitQuad("=", popped[2].name, "_", popped[0].name); 
        res.name = popped[0].name; 
        break;
    case 15: case 16: case 18: case 19: //算术运算，E->E+F，返回临时变量
        res.name = newTemp();
        emit(popped[1].name, popped[0].name, popped[2].name, res.name);
        emitQuad(popped[1].name, popped[0].name, popped[2].name, res.name);
        break;
    case 21: //一元负号，G->-G，返回临时变量
        res.name = newTemp(); 
        emit("neg", popped[1].name, "", res.name); 
        emitQuad("neg", popped[1].name, "_", res.name); 
        break;
    case 22: case 23: //变量和常量，G->i或G->n，返回变量名或常量名
        res.name = popped[0].name; 
        break;
    case 24: case 8: //括号表达式，G->(E)，返回括号内的表达式结果
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
        emit("+", targetId, "1", t);// 计算新值
        emit(":=", t, "", targetId);  // 自增
        emitQuad("+", targetId, "1", t);
        emitQuad("=", t, "_", targetId);
        res.name = targetId; // 返回新值（自增后的值）
        break;
    }
    case 33: { 
        string targetId = popped[0].name;
        string oldValue = newTemp();
        emit(":=", targetId, "", oldValue); 
        emitQuad("=", targetId, "_", oldValue);
        string t = newTemp();
        emit("-", targetId, "1", t);  
        emit(":=", t, "", targetId); 
        emitQuad("-", targetId, "1", t);
        emitQuad("=", t, "_", targetId);
        res.name = oldValue;   
        break;
    }
    case 34: { 
        string targetId = popped[1].name;
        string t = newTemp();
        emit("-", targetId, "1", t); 
        emit(":=", t, "", targetId); 
        emitQuad("-", targetId, "1", t);
        emitQuad("=", t, "_", targetId);
        res.name = targetId; 
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
