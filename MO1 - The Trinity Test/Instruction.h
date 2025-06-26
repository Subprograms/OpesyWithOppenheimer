#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <string>

enum class OpCode {
    PRINT,
    DECLARE,
    ADD,
    SUBTRACT,
    SLEEP,
    FOR_BEGIN,
    FOR_END
};

struct Instruction {
    OpCode op;
    std::string arg1;
    std::string arg2;
    std::string arg3;
    bool isArg2Var = true;
    bool isArg3Var = true;

    Instruction(OpCode opCode = OpCode::PRINT,
                const std::string& a1 = "",
                const std::string& a2 = "",
                const std::string& a3 = "",
                bool arg2Var = true,
                bool arg3Var = true)
        : op(opCode), arg1(a1), arg2(a2), arg3(a3),
          isArg2Var(arg2Var), isArg3Var(arg3Var) {}
};

#endif
