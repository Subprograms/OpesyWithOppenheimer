#include "Scheduler.h"
#include "Instruction.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <iomanip>

Scheduler::Scheduler(const Config& config)
    : config(config), running(true), schedulerType(config.scheduler), quantum(config.quantumCycles) {
    for (int i = 0; i < config.numCpu; ++i) {
        coreThreads.emplace_back(&Scheduler::coreFunction, this, i + 1);  // Start core IDs from 1
    }
}

Scheduler::~Scheduler() {
    stop();
    for (auto& thread : coreThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void Scheduler::start() {
    running = true;
}

void Scheduler::stop() {
    running = false;
    cv.notify_all();
}

void Scheduler::addProcess(const ProcessInfo& process) {
    std::lock_guard<std::mutex> lock(queueMutex);
    processQueue.push_back(process);
    cv.notify_one();
}

ProcessInfo& Scheduler::getProcess(const std::string& name)
{
    std::lock_guard<std::mutex> lock(queueMutex);

    for (auto& p : runningProcesses)
        if (p.processName == name) return p;

    for (auto& p : processQueue)
        if (p.processName == name) return p;

    for (auto& entry : finishedProcesses)
        if (entry.first.processName == name) return entry.first;

    throw std::runtime_error("Process not found: " + name);
}

std::vector<std::pair<ProcessInfo, int>> Scheduler::getFinishedProcesses() {
    std::lock_guard<std::mutex> lock(queueMutex);
    return finishedProcesses;
}

std::vector<ProcessInfo> Scheduler::getRunningProcesses() {
    std::lock_guard<std::mutex> lock(queueMutex);
    return runningProcesses;
}

std::vector<ProcessInfo> Scheduler::getWaitingProcesses() {
    std::lock_guard<std::mutex> lock(queueMutex);
    return std::vector<ProcessInfo>(processQueue.begin(), processQueue.end());
}

void Scheduler::coreFunction(int coreId)
{
    while (running) {
        ProcessInfo proc(-1,"",0,"");
        bool hadWork = false;

        {
            std::unique_lock<std::mutex> lk(queueMutex);
            cv.wait(lk,[&]{ return !processQueue.empty()||!running; });
            if (!running && processQueue.empty()) break;

            proc = processQueue.front();
            processQueue.pop_front();
            proc.assignedCore = coreId;
            runningProcesses.push_back(proc);
            ++coresInUse;
            hadWork = true;
        }
        if (!hadWork) continue;

        if (proc.sleepTicks>0) {
            --proc.sleepTicks;
        } else if (proc.currentLine < proc.totalLine) {

            Instruction& ins = proc.prog[proc.currentLine];

            switch (ins.op) {
            case OpCode::PRINT: {
                std::ofstream f(proc.processName+".txt",std::ios::app);
                if (f.is_open()) f << "Core:"<<coreId<<" "<<ins.arg1<<"\n";
                break;
            }
            case OpCode::DECLARE:
                proc.vars[ins.arg1] = static_cast<uint16_t>(ins.arg2);
                break;
            case OpCode::ADD:
                proc.vars[ins.arg1] =
                    static_cast<uint16_t>(
                        std::min<uint32_t>(65535u,
                                        static_cast<uint32_t>(proc.vars[ins.arg1] + ins.arg2)));
                break;

            case OpCode::SUBTRACT:
                proc.vars[ins.arg1] =
                    static_cast<uint16_t>(
                        std::max<int32_t>(0,
                                        static_cast<int32_t>(proc.vars[ins.arg1]) - ins.arg2));
                break;
            case OpCode::SLEEP:
                proc.sleepTicks = ins.arg2; // set counter
                break;
            case OpCode::FOR_BEGIN:
                if (proc.loopStack.size()<3)
                    proc.loopStack.push_back({proc.currentLine+1, ins.arg2});
                break;
            case OpCode::FOR_END:
                if (!proc.loopStack.empty()) {
                    auto& top = proc.loopStack.back();
                    if (--top.remaining > 0)
                        proc.currentLine = top.startIdx - 1; // jump back
                    else
                        proc.loopStack.pop_back();
                }
                break;
            }
            ++proc.currentLine;
        }

        bool done = proc.currentLine >= proc.totalLine &&
                    proc.loopStack.empty() && proc.sleepTicks==0;

        {
            std::lock_guard<std::mutex> lk(queueMutex);

            runningProcesses.erase(std::remove_if(runningProcesses.begin(),
                                   runningProcesses.end(),
                                   [&](const ProcessInfo& p){
                                     return p.processID==proc.processID;} ),
                                   runningProcesses.end());
            --coresInUse;

            if (done) {
                proc.isFinished = true;
                finishedProcesses.push_back({proc, coreId});
            } else {
                processQueue.push_back(proc); // re-queue for next tick
            }
        }
        cv.notify_all();
        std::this_thread::sleep_for(
            std::chrono::milliseconds(config.delaysPerExec)); // simulate tick
    }
}

std::string Scheduler::utilisationString() const
{
    std::ostringstream os;
    double pct = (coresInUse.load() * 100.0) / config.numCpu;
    os << "CPU utilisation : " << std::fixed << std::setprecision(1) << pct << "%\n"
       << "Cores used      : " << coresInUse        << '\n'
       << "Cores available : " << (config.numCpu - coresInUse) << "\n\n";
    return os.str();
}