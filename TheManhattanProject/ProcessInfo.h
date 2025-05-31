// ProcessInfo.h

#ifndef PROCESSINFO_H
#define PROCESSINFO_H

#include <string>
#include <vector>
#include <unordered_map>

struct ProcessInfo {
    int processID;
    std::string processName;
    std::string arrivalTime;
    std::vector<std::string> processInstructions;
    int procMemsize;
    int currentLine;
    int totalLine;
    bool isFinished;
    int assignedCore;

    ProcessInfo(
        int id,
        const std::string& name,
        const std::string& arrivetime,
        const std::vector<std::string>& instructions,
        int size,
        int currentline,
        int totallines,
        bool finished

    ) : processID(id),
        processName(name),
        arrivalTime(arrivetime),
        processInstructions(instructions),
        procMemsize(size),
        currentLine(currentline),
        totalLine(totallines),
        isFinished(finished),
        assignedCore(-1)
    {}
};

#endif