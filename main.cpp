#include "compiler.h"
#include <iostream>

using namespace std;

int main() {
    WhileCompiler compiler;
    string code = "while ( true ) { float b_flag = 1.5 ; if_val = a_var ; while ( b < 1 ) { break ; } continue ; b = a_var ++ ; }";
    cout << "输入代码: " << code << "\n" << endl;
    compiler.run(code);
    return 0;
}
