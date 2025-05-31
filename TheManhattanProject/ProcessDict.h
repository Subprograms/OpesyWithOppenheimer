// ProcessDict.h

#ifndef PROCESSDICT_H
#define PROCESSDICT_H

#include "ProcessInfo.h"

#include <iostream>
#include <unordered_map>

class ProcessDict {
    std::unordered_map<int, ProcessInfo> ProcDict;

    void addProcessbyKey(const ProcessInfo& new_process) {
        ProcDict[new_process.processID] = new_process;
    }

    ProcessInfo* getProcessbyKey(const int process_id) {
        auto it = ProcDict.find(process_id);
        if (it != ProcDict.end()) {
            return &it->second;
        }
        return nullptr;
    }
};

#endif
