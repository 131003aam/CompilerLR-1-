# CodeGenerator 数据结构详细说明

## 概述
本文档详细说明 `CodeGenerator` 类中使用的所有数据结构，包括它们的定义、作用、使用场景等。

---

## 一、核心代码存储数据结构

### 1. `vector<TAC> tacCode` - 三地址码存储

#### 定义
```cpp
vector<TAC> tacCode;  // 在 codegen.h 第12行定义
```

#### TAC 结构体定义（types.h）
```cpp
struct TAC {
    string op;      // 操作符：如 "+", "-", "=", "jz", "goto" 等
    string arg1;    // 第一个操作数
    string arg2;    // 第二个操作数
    string result;  // 结果变量或跳转目标
    int addr;       // 指令地址：该指令在三地址码中的索引位置
};
```

#### 作用
- **存储最终生成的三地址码中间代码**
- 每条指令包含操作符、操作数和结果
- `addr` 字段用于标识指令位置，方便跳转指令的地址回填

#### 使用场景

**1. 添加指令（emit 函数）**
```17:19:codegen.cpp
void CodeGenerator::emit(const string& op, const string& a1, const string& a2, const string& res) {
    tacCode.push_back({ op, a1, a2, res, (int)tacCode.size() });
}
```
- 每次调用 `emit()` 都会向 `tacCode` 添加一条新指令
- `addr` 自动设置为当前向量的长度（即新指令的索引）

**2. 回填跳转地址（backpatch 函数）**
```29:33:codegen.cpp
void CodeGenerator::backpatch(int addr, const string& target) {
    if (addr >= 0 && addr < (int)tacCode.size()) {
        tacCode[addr].result = target;
    }
}
```
- 修改之前生成的跳转指令的 `result` 字段，将占位符替换为实际跳转目标

**3. 获取当前代码位置（用于循环管理）**
```38:39:codegen.cpp
    loopAddrStack.push((int)tacCode.size());
```
```55:55:codegen.cpp
    int exitAddr = (int)tacCode.size();
```
- `tacCode.size()` 表示当前已生成的指令数量
- 用于记录循环开始位置、break/continue 跳转指令的位置等

**4. 输出最终代码（printTAC 函数）**
```257:265:codegen.cpp
void CodeGenerator::printTAC() const {
    cout << "\n--- 生成的三地址码 (TAC) ---" << endl;
    for (auto& t : tacCode) {
        cout << "L" << right << setw(3) << t.addr << " | ";
        if (t.op == "goto") {
            cout << "goto " << t.result << endl;
        }
        else if (t.op == "jz") {
            cout << "if " << left << setw(10) << t.arg1 << " == 0 goto " << t.result << endl;
```
- 遍历所有指令并格式化输出
- 在 `compiler.cpp` 的 `run()` 函数末尾调用：`codegen.printTAC()`

**5. 外部访问（getTACCode 函数）**
```56:56:codegen.h
    const vector<TAC>& getTACCode() const { return tacCode; }
```
- 提供只读访问接口，供外部获取生成的三地址码

---

### 2. `vector<Quadruple> quads` - 四元式存储

#### 定义
```cpp
vector<Quadruple> quads;  // 在 codegen.h 第13行定义
```

#### Quadruple 结构体定义（types.h）
```cpp
struct Quadruple {
    string op;      // 操作符
    string arg1;    // 第一个操作数
    string arg2;    // 第二个操作数
    string result;  // 结果变量
    string toString() const {
        return "(" + op + ", " + arg1 + ", " + arg2 + ", " + result + ")";
    }
};
```

#### 作用
- **存储四元式形式的中间代码**（与 TAC 并行生成）
- 用于在语法分析过程中显示每一步生成的中间代码
- 格式：(操作符, 操作数1, 操作数2, 结果)

#### 使用场景

**1. 添加四元式（emitQuad 函数）**
```21:26:codegen.cpp
void CodeGenerator::emitQuad(const string& op, const string& a1, const string& a2, const string& res) {
    Quadruple q = { op, a1, a2, res };
    quads.push_back(q);
    if (!currentStepQuads.empty()) currentStepQuads += " ";
    currentStepQuads += q.toString();
}
```
- 同时生成四元式和字符串表示
- 字符串用于在语法分析步骤中实时显示

**2. 在语法分析过程中显示**
```393:393:compiler.cpp
            cout << left << setw(6) << step++ << setw(25) << stStr << setw(20) << syStr << setw(12) << a << setw(15) << "归约 r" + to_string(act.target) << codegen.getCurrentStepQuads() << endl;
```
- 每次归约时，在分析表中显示该步骤生成的四元式

**3. 外部访问（getQuads 函数）**
```57:57:codegen.h
    const vector<Quadruple>& getQuads() const { return quads; }
```
- 提供只读访问接口

---

### 3. `string currentStepQuads` - 当前步骤四元式字符串

#### 定义
```cpp
string currentStepQuads;  // 在 codegen.h 第21行定义
```

#### 作用
- **临时存储当前语法分析步骤生成的四元式字符串**
- 用于在语法分析表中实时显示每一步的代码生成结果

#### 使用场景

**1. 添加四元式时更新**
```24:25:codegen.cpp
    if (!currentStepQuads.empty()) currentStepQuads += " ";
    currentStepQuads += q.toString();
```
- 每次调用 `emitQuad()` 时，将四元式字符串追加到 `currentStepQuads`

**2. 获取当前步骤的四元式**
```39:39:codegen.h
    string getCurrentStepQuads() const { return currentStepQuads; }
```
- 在 `compiler.cpp` 中调用，用于显示语法分析步骤

**3. 清空（每步开始时）**
```40:40:codegen.h
    void clearCurrentStepQuads() { currentStepQuads = ""; }
```
```159:159:compiler.cpp
        codegen.clearCurrentStepQuads();  // 清空当前步骤的四元式字符串
```
- 每个语法分析步骤开始时清空，只记录当前步骤生成的代码

---

## 二、循环控制数据结构

### 4. `stack<int> loopAddrStack` - 循环地址栈

#### 定义
```cpp
stack<int> loopAddrStack;  // 在 codegen.h 第17行定义
```

#### 作用
- **存储每个嵌套循环的起始地址**
- 支持嵌套循环的处理
- 每个循环开始时记录其条件判断代码的起始位置

#### 使用场景

**1. 进入循环时记录地址**
```38:39:codegen.cpp
    loopAddrStack.push((int)tacCode.size());
```
- 在 `enterLoop()` 中调用，记录当前代码位置
- 该位置是循环条件表达式代码的起始位置

**2. 退出循环时使用**
```47:47:codegen.cpp
    int testStart = loopAddrStack.top();
```
- 在 `exitLoop()` 中使用，获取循环起始地址
- 用于生成跳回循环开始的 `goto` 指令
- 用于回填 `continue` 语句的跳转目标

**3. 嵌套循环支持**
- 使用栈结构支持多层嵌套
- 内层循环的地址在栈顶，外层循环的地址在下方

---

### 5. `stack<vector<int>> breakLists` - Break 地址列表栈

#### 定义
```cpp
stack<vector<int>> breakLists;  // 在 codegen.h 第18行定义
```

#### 作用
- **存储每个循环层中所有 `break` 语句的指令地址**
- 每个循环层对应一个地址列表（vector）
- 循环结束时统一回填这些地址

#### 使用场景

**1. 初始化（进入循环时）**
```39:39:codegen.cpp
    breakLists.push(vector<int>());
```
- 在 `enterLoop()` 中为新的循环层创建空的地址列表

**2. 收集 break 地址**
```69:69:codegen.cpp
        breakLists.top().push_back(addr);
```
- 在 `handleBreak()` 中，生成 `goto` 指令后保存其地址
- 在 `case 38`（M -> epsilon）中，生成条件判断的 `jz` 指令后也保存地址

**3. 回填（退出循环时）**
```55:57:codegen.cpp
    vector<int> brks = breakLists.top();
    breakLists.pop();
    for (int addr : brks) backpatch(addr, "L" + to_string(exitAddr));
```
- 在 `exitLoop()` 中取出所有地址并回填到循环结束位置

---

### 6. `stack<vector<int>> continueLists` - Continue 地址列表栈

#### 定义
```cpp
stack<vector<int>> continueLists;  // 在 codegen.h 第19行定义
```

#### 作用
- **存储每个循环层中所有 `continue` 语句的指令地址**
- 与 `breakLists` 结构相同，但用于 `continue` 语句

#### 使用场景

**1. 初始化（进入循环时）**
```40:40:codegen.cpp
    continueLists.push(vector<int>());
```
- 在 `enterLoop()` 中创建新的地址列表

**2. 收集 continue 地址**
```80:80:codegen.cpp
        continueLists.top().push_back(addr);
```
- 在 `handleContinue()` 中保存地址

**3. 回填（退出循环时）**
```59:61:codegen.cpp
    vector<int> conts = continueLists.top();
    continueLists.pop();
    for (int addr : conts) backpatch(addr, "L" + to_string(testStart));
```
- 回填到循环条件判断的开始位置（testStart），而不是循环结束

---

## 三、辅助数据结构

### 7. `int tempCount` - 临时变量计数器

#### 定义
```cpp
int tempCount = 0;  // 在 codegen.h 第14行定义
```

#### 作用
- **生成唯一临时变量名**
- 每次生成临时变量时递增，确保名称唯一

#### 使用场景

**1. 初始化**
```9:9:codegen.cpp
CodeGenerator::CodeGenerator() : tempCount(0) { // 初始化临时变量计数器
}
```

**2. 生成临时变量名**
```12:14:codegen.cpp
string CodeGenerator::newTemp() {
    return "T" + to_string(++tempCount);
}
```
- 生成如 "T1", "T2", "T3" 等临时变量名
- 在 `handleProduction()` 中大量使用，如算术运算、逻辑运算等

---

## 四、数据结构关系图

```
CodeGenerator
│
├─ 代码存储
│  ├─ tacCode (vector<TAC>)          ← 最终输出使用
│  │  └─ 每条指令包含 addr 用于回填
│  │
│  ├─ quads (vector<Quadruple>)      ← 并行存储四元式
│  │  └─ 用于显示语法分析过程
│  │
│  └─ currentStepQuads (string)      ← 临时字符串
│     └─ 当前步骤的四元式显示
│
├─ 循环控制（支持嵌套）
│  ├─ loopAddrStack (stack<int>)     ← 记录循环起始地址
│  ├─ breakLists (stack<vector<int>>) ← 存储 break 地址
│  └─ continueLists (stack<vector<int>>) ← 存储 continue 地址
│
└─ 辅助
   └─ tempCount (int)                ← 临时变量计数
```

---

## 五、数据流向

### 代码生成流程

```
语法分析 (compiler.cpp)
    │
    ├─→ handleProduction()  [归约时调用]
    │       │
    │       ├─→ emit()           → tacCode.push_back()
    │       ├─→ emitQuad()       → quads.push_back()
    │       │                      → currentStepQuads += ...
    │       └─→ newTemp()        → tempCount++
    │
    ├─→ enterLoop()  [遇到 while]
    │       ├─→ loopAddrStack.push(tacCode.size())
    │       ├─→ breakLists.push(vector<int>())
    │       └─→ continueLists.push(vector<int>())
    │
    ├─→ handleBreak() / handleContinue()
    │       ├─→ emit("goto", ...)
    │       └─→ breakLists/continueLists.top().push_back(addr)
    │
    └─→ exitLoop()  [循环结束时]
            ├─→ 从栈中取出地址列表
            ├─→ backpatch() 修改 tacCode[addr].result
            └─→ 弹出栈顶元素

最终输出
    └─→ printTAC() 遍历 tacCode 输出
```

---

## 六、典型使用示例

### 示例：处理 `while (x > 0) { x--; }`

```cpp
// 1. 遇到 while 关键字
enterLoop();  
// loopAddrStack.push(0)  // 假设当前 tacCode.size() = 0
// breakLists.push([])
// continueLists.push([])

// 2. 生成条件表达式 x > 0 的代码
emit(">", "x", "0", "T1");  // tacCode[0]: { ">", "x", "0", "T1", 0 }

// 3. 归约 M -> epsilon，生成条件判断
emit("jz", "T1", "", "PENDING_EXIT");  // tacCode[1]: { "jz", "T1", "", "PENDING_EXIT", 1 }
breakLists.top().push_back(1);  // 保存地址 1

// 4. 生成循环体 x-- 的代码
// ... (省略具体代码)

// 5. 退出循环
exitLoop();
// testStart = loopAddrStack.top() = 0
// emit("goto", "", "", "L0")  // tacCode[2]
// exitAddr = 3
// backpatch(1, "L3")  // 修改 tacCode[1].result = "L3"
```

---

## 八、语义栈（SemStack）- 语法分析与代码生成的桥梁

### 定义
```cpp
vector<SemItem> semStack;  // 在 compiler.cpp 第141行定义
```

### SemItem 结构体定义（types.h）
```cpp
struct SemItem {
    string name;    // 名称：变量名或临时变量名
};
```

### 作用
- **在语法分析过程中传递语义信息**（变量名、常量值、临时变量名等）
- **与状态栈、符号栈同步维护**，保持三者对应关系
- **作为代码生成的输入**，`handleProduction()` 从语义栈获取操作数信息

### 工作原理

语义栈与语法分析栈（状态栈、符号栈）保持同步：

```
状态栈 (stateStack)  ←→  符号栈 (symbolStack)  ←→  语义栈 (semStack)
     [状态编号]              [语法符号]              [语义信息]
```

### 使用场景

#### 1. 移进动作（SHIFT）时
```370:372:compiler.cpp
            stateStack.push(act.target);
            symbolStack.push(a);
            semStack.push_back({ w.token });  // 保存 Token 的原始值（用于代码生成）
```
- 移进时，同时向三个栈压入元素
- 语义栈保存 Token 的原始值（变量名、常量值等），用于后续代码生成

#### 2. 归约动作（REDUCE）时

**步骤1：弹出产生式右部的元素**
```381:388:compiler.cpp
            vector<SemItem> popped;
            for (int k = 0; k < (int)p.right.size(); k++) {
                stateStack.pop();           // 弹出状态
                symbolStack.pop();          // 弹出符号
                popped.push_back(semStack.back());  // 保存语义信息
                semStack.pop_back();
            }
            reverse(popped.begin(), popped.end());  // 反转顺序（栈是后进先出）
```
- 从三个栈中同时弹出产生式右部长度的元素
- 将弹出的语义信息保存到 `popped` 向量中
- **反转顺序**：因为栈是后进先出，需要反转才能得到正确的顺序

**步骤2：执行语义动作生成代码**
```391:391:compiler.cpp
            SemItem res = codegen.handleProduction(act.target, popped, semStack);
```
- 调用 `handleProduction()`，传入：
  - `popped`：产生式右部的语义信息
  - `semStack`：当前剩余的语义栈（用于访问未弹出的语义信息，如循环条件）

**步骤3：压入归约后的结果**
```395:397:compiler.cpp
            symbolStack.push(p.left);
            stateStack.push(gotoTable.at(stateStack.top()).at(p.left));
            semStack.push_back(res);
```
- 归约后，将新的非终结符和对应的语义结果压入栈
- `res` 是 `handleProduction()` 返回的语义项，包含生成的临时变量名或传递的变量名

### 在代码生成中的使用

#### 示例1：算术运算 `E -> E + F`

```cpp
// 假设语义栈状态：
// semStack = [..., "x", "+", "5"]  // x 和 5 是从子表达式得到的

// 归约 E -> E + F
// popped[0] = { "x" }      // E 的结果
// popped[1] = { "+" }      // 运算符（通常不使用）
// popped[2] = { "5" }      // F 的结果

case 15:  // E -> E + F
    res.name = newTemp();  // 生成临时变量 T1
    emit("+", popped[0].name, popped[2].name, res.name);  
    // 生成：T1 := x + 5
    // 返回：res = { "T1" }
```

#### 示例2：赋值语句 `S -> i = E`

```cpp
// 归约 S -> i = E
// popped[0] = { "x" }      // 变量名 i
// popped[1] = { "=" }      // 运算符（不使用）
// popped[2] = { "T1" }     // 表达式 E 的结果

case 14:  // S -> i = E
    emit(":=", popped[2].name, "", popped[0].name);
    // 生成：x := T1
    res.name = popped[0].name;  // 返回变量名
```

#### 示例3：循环条件（使用 semStack 而非 popped）

```104:106:codegen.cpp
        if (semStack.size() >= 2) {
            SemItem lResult = semStack[semStack.size() - 2];
```
- 在归约 `M -> epsilon` 时，需要访问**尚未弹出的**语义栈中的循环条件
- `semStack[semStack.size() - 2]` 获取条件表达式 `L` 的结果
- 因为 `M` 是空产生式，`popped` 为空，所以需要直接从 `semStack` 中读取

### 语义栈与三栈同步示例

处理 `x = y + 3;` 的过程：

```
步骤1: 移进 'x'
状态栈: [0, 状态1]
符号栈: [#, "i"]
语义栈: [{"x"}]

步骤2: 移进 '='
状态栈: [0, 状态1, 状态2]
符号栈: [#, "i", "="]
语义栈: [{"x"}, {"="}]

步骤3: 移进 'y'
状态栈: [0, ..., 状态3]
符号栈: [#, "i", "=", "i"]
语义栈: [{"x"}, {"="}, {"y"}]

步骤4: 移进 '+'
...

步骤5: 归约 G -> i (y 识别为基本表达式)
弹出: popped = [{"y"}]
生成: res = {"y"}  // 直接传递
压入: 语义栈 = [{"x"}, {"="}, {"y"}]

步骤6: 归约 E -> E + F (计算 y + 3)
弹出: popped = [{"y"}, {"+"}, {"3"}]
生成: res = {"T1"}  // 生成临时变量
生成代码: T1 := y + 3
压入: 语义栈 = [{"x"}, {"="}, {"T1"}]

步骤7: 归约 S -> i = E (完成赋值)
弹出: popped = [{"x"}, {"="}, {"T1"}]
生成代码: x := T1
压入: 语义栈 = [{"x"}]
```

### 特殊说明

1. **使用 vector 而非 stack**
   - 语义栈使用 `vector<SemItem>` 而非 `stack<SemItem>`
   - 原因：需要访问栈中特定位置的元素（如 `semStack[semStack.size() - 2]`）
   - 使用 `push_back()` 和 `pop_back()` 模拟栈的行为

2. **popped 需要反转**
   - 因为栈是后进先出，弹出时顺序是反的
   - 例如：栈顶到栈底是 `["x", "+", "5"]`，弹出后 `popped` 是 `["5", "+", "x"]`
   - 反转后得到正确顺序：`["x", "+", "5"]`

3. **语义信息的传递**
   - 变量名、常量值：直接传递
   - 表达式结果：使用临时变量名传递
   - 某些产生式（如括号、单位产生式）：直接传递子表达式的语义值

---

## 九、总结

| 数据结构 | 类型 | 作用 | 生命周期 |
|---------|------|------|---------|
| `tacCode` | `vector<TAC>` | 存储最终三地址码 | 整个编译过程 |
| `quads` | `vector<Quadruple>` | 存储四元式 | 整个编译过程 |
| `currentStepQuads` | `string` | 当前步骤显示 | 每个语法分析步骤 |
| `loopAddrStack` | `stack<int>` | 循环起始地址 | 循环嵌套期间 |
| `breakLists` | `stack<vector<int>>` | break 地址 | 循环嵌套期间 |
| `continueLists` | `stack<vector<int>>` | continue 地址 | 循环嵌套期间 |
| `tempCount` | `int` | 临时变量计数 | 整个编译过程 |
| `semStack` | `vector<SemItem>` | 传递语义信息 | 整个语法分析过程 |

这些数据结构协同工作，实现了：
1. **代码生成**：`tacCode` 和 `quads` 存储中间代码
2. **循环管理**：三个栈结构支持嵌套循环和跳转回填
3. **过程可视化**：`currentStepQuads` 实时显示代码生成过程
4. **资源管理**：`tempCount` 确保临时变量名唯一
5. **语义传递**：`semStack` 在语法分析和代码生成之间传递语义信息

