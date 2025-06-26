#ifndef PROCESSINFO_H
#define PROCESSINFO_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>          // uint16_t
#include "Instruction.h"

struct ProcessInfo
{
    int         processID;
    std::string processName;

    int  totalLine;
    int  currentLine{0};
    int  executedLines{0};
    int  assignedCore{-1};
    std::string timeStamp;
    bool isFinished{false};
    int  sleepTicks{0};
    std::vector<Instruction> prog;
    std::unordered_map<std::string,uint16_t> vars;
    struct LoopFrame { int startIdx; int remaining; };
    std::vector<LoopFrame> loopStack;
    std::vector<std::string> outBuf;

    ProcessInfo(int id,
                const std::string& name,
                int lines,
                const std::string& ts,
                bool finished = false)
        : processID(id),
          processName(name),
          totalLine(lines),
          timeStamp(ts),
          isFinished(finished) {}
};

#endif /* PROCESSINFO_H */
