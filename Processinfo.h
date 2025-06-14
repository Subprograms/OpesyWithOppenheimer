#ifndef PROCESSINFO_H
#define PROCESSINFO_H

#include <string>

class ProcessInfo {
public:
    int processID;
    std::string processName;
    int totalLine;
    int currentLine;
    int assignedCore;
    std::string timeStamp;
    bool isFinished;
    int remainingQuantum;

    ProcessInfo(int id, const std::string& name, int lines, const std::string& timestamp, bool finished = false)
        : processID(id), processName(name), totalLine(lines), currentLine(0), assignedCore(-1),
        timeStamp(timestamp), isFinished(finished), remainingQuantum(0) {}
};

#endif
