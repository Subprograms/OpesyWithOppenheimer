#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <string>
#include <vector>

enum class OpCode {PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR, NOP};

struct Instruction {
    OpCode op;
    std::vector<Instruction> body;
    std::string arg1, arg2, arg3;
    bool isArg2Var{false}, isArg3Var{false};

    Instruction(OpCode o = OpCode::NOP) : op(o) {}
    Instruction(OpCode o, std::vector<Instruction> b,
                std::string a1, std::string a2)
        : op(o), body(std::move(b)), arg1(std::move(a1)), arg2(std::move(a2)) {}
    Instruction(OpCode o, std::string a1, std::string a2,
                std::string a3 = "", bool v2 = false, bool v3 = false)
        : op(o), arg1(std::move(a1)), arg2(std::move(a2)), arg3(std::move(a3)),
          isArg2Var(v2), isArg3Var(v3) {}
};
#endif
