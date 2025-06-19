#ifndef PROCESSINFO_H
#define PROCESSINFO_H

#include <string>
#include <vector>
#include <unordered_map>
#include "Instruction.h"

class ProcessInfo {
public:
    int  processID;
    std::string processName;
    int  totalLine;
    int  currentLine{0};
    int  assignedCore{-1};
    std::string timeStamp;
    bool isFinished{false};
    int  sleepTicks{0};
    std::vector<Instruction>                   prog;
    std::unordered_map<std::string,uint16_t>   vars;
    struct LoopFrame { int startIdx; int remaining; };
    std::vector<LoopFrame> loopStack; // depth <= 3 as per specs

    ProcessInfo(int id,const std::string& name,int lines,
                const std::string& ts,bool finished=false)
        : processID(id),processName(name),
          totalLine(lines),timeStamp(ts),isFinished(finished) {}
};

#endif