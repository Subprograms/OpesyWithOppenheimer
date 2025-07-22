#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

enum class OpCode { PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR, READ, WRITE, NOP };

struct Instruction
{
    OpCode  op{OpCode::NOP};

    std::string arg1, arg2, arg3;
    bool  isArg2Var{false}, isArg3Var{false};

    std::vector<Instruction> body;
    uint8_t repetitions{0};

    Instruction() = default;

    Instruction(OpCode o,
                std::string a1 = "",
                std::string a2 = "",
                std::string a3 = "",
                bool v2 = false,
                bool v3 = false)
        : op(o), arg1(std::move(a1)), arg2(std::move(a2)), arg3(std::move(a3)),
          isArg2Var(v2), isArg3Var(v3) {}

    Instruction(std::vector<Instruction> loopBody, uint8_t reps)
        : op(OpCode::FOR), body(std::move(loopBody)), repetitions(reps) {}
};

inline std::size_t logicalSize(const std::vector<Instruction>& prog)
{
    std::size_t total = 0;
    for (const auto& ins : prog) {
        if (ins.op == OpCode::FOR)
            total += logicalSize(ins.body) * ins.repetitions;
        else
            ++total;
    }
    return total;
}

#endif