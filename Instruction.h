// Found this in the specs
#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <string>

enum class OpCode {
    PRINT,          // arg1 = string msg
    DECLARE,        // arg1 = var name , arg2 = value
    ADD,            // arg1 = var name , arg2 = value
    SUBTRACT,       // arg1 = var name , arg2 = value
    SLEEP,          // arg2 = ticks
    FOR_BEGIN,      // arg2 = repetitions
    FOR_END
};

struct Instruction {
    OpCode op;
    std::string arg1;
    int arg2; // msg length, value, repetitions, or ticks
};

#endif
