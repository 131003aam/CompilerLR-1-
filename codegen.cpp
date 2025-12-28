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
    loopAddrStack.push((int)tacCode.size());
    breakLists.push(vector<int>());
    continueLists.push(vector<int>());
}

void CodeGenerator::exitLoop() {
    if (loopAddrStack.empty()) return;
    int testStart = loopAddrStack.top();
    loopAddrStack.pop();
    emit("goto", "", "", "L" + to_string(testStart));
    emitQuad("j", "_", "_", to_string(testStart));
    int exitAddr = (int)tacCode.size();
    vector<int> brks = breakLists.top();
    breakLists.pop();
    for (int addr : brks) backpatch(addr, "L" + to_string(exitAddr));
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
    case 31: case 32: {
        string targetId = (prodId == 31 ? popped[0].name : popped[1].name);
        string t = newTemp(); 
        emit("+", targetId, "1", t); 
        emit(":=", t, "", targetId);
        emitQuad("+", targetId, "1", t); 
        emitQuad("=", t, "_", targetId);
        res.name = targetId; 
        break;
    }
    case 33: case 34: {
        string targetId = (prodId == 33 ? popped[0].name : popped[1].name);
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

