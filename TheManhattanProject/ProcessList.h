// ProcessList.h

#ifndef PROCESSLIST_H
#define PROCESSLIST_H

#include "ProcessInfo.h"

#include <iostream>
#include <string>
#include <vector>

class ProcessList {
    std::vector<ProcessInfo> ProcList;

    void addProcess(const ProcessInfo& new_process) {
        for (auto& process : ProcList) {
            if (process.processName == new_process.processName) {
                std::cout << "Process: " << process.processName << "already exists." << std::endl;
                return;
            }
        }
        ProcList.push_back(new_process);
    }

    ProcessInfo* getProcess(const std::string& processName) {
        for (auto& process : ProcList) {
            if (process.processName == processName) {
                return &process;
            }
        }
        return nullptr;
    }

    void listAllProcess() {
        for (const auto& process : ProcList) {
            std::cout << "====================================================" << std::endl;
            std::cout << "PID:          " << process.processID << std::endl;
            std::cout << "Process Name: " << process.processName << std::endl;
            std::cout << "Arrival Time: " << process.arrivalTime << std::endl;
            std::cout << "Status:       " << (process.isFinished ? "Finished" : "Running") << std::endl;
            std::cout << "Lines:        " << process.currentLine << "/" << process.totalLine << std::endl;
            std::cout << "====================================================" << std::endl;
        }
    }
};

#endif

