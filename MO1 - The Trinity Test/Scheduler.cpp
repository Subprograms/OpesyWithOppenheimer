#include "Scheduler.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <atomic>
#include <sstream>
#include <iomanip>
extern std::atomic<int> g_attachedPid;

namespace {
    std::mutex g_coutMx;

    inline std::string stripQuotes(const std::string& s) {
        return (s.size() >= 2 && s.front() == '"' && s.back() == '"')
             ? s.substr(1, s.size() - 2) : s;
    }
    inline std::string varVal(const ProcessInfo& p, const std::string& n) {
        auto it = p.vars.find(n);
        return (it != p.vars.end()) ? std::to_string(it->second) : "0";
    }
}

static uint16_t Stoi16(const std::string& s, const std::string& ctx  = "<unknown>") // Stoi with debugger
{
    try {
        long v = std::stol(s);
        if (v < 0)          v = 0;
        if (v > 65535)      v = 65535;
        return static_cast<uint16_t>(v);
    }
    catch (const std::exception& e) {
        std::cerr << " Debug: Stoi error in " << ctx
                  << " : \"" << s << "\" – " << e.what() << '\n';
        return 0;
    }
}

std::string Scheduler::utilisationString() const
{
    std::ostringstream os;
    double pct = (coresInUse.load() * 100.0) / config.numCpu;

    os << "CPU utilisation : "
       << std::fixed << std::setprecision(1) << pct << "%\n"
       << "Cores used      : " << coresInUse        << '\n'
       << "Cores available : " << (config.numCpu - coresInUse) << "\n\n";

    return os.str();
}

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

void Scheduler::addProcess(ProcessInfo&& proc)
{
    std::lock_guard<std::mutex> lk(queueMutex);
    processQueue.emplace_back(std::move(proc));   // move into queue
    cv.notify_one();
}

void Scheduler::addProcess(const ProcessInfo& proc)
{
    addProcess(ProcessInfo(proc));
}

ProcessInfo& Scheduler::getProcess(const std::string& name) {
    std::lock_guard<std::mutex> lock(queueMutex);

    // Check running processes
    for (auto& process : runningProcesses) {
        if (process.processName == name) {
            return process;
        }
    }

    // Check waiting queue (processQueue)
    for (auto& process : processQueue) {
        if (process.processName == name) {
            return process;
        }
    }

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

static void logInstr(ProcessInfo& p, int core, const Instruction& ins,
                     const std::string& printText = "")
{
    std::string line = "Core:" + std::to_string(core) +
                       " ["     + std::to_string(p.executedLines) + "] ";

    switch (ins.op) {
        case OpCode::PRINT:     line += "PRINT "     + ins.arg1 +
                                 (ins.arg2.empty() ? "" : (" + " + ins.arg2)); break;
        case OpCode::DECLARE:   line += "DECLARE "   + ins.arg1 + "=" + ins.arg2; break;
        case OpCode::ADD:       line += "ADD "       + ins.arg1 + " " + ins.arg2 + " " + ins.arg3; break;
        case OpCode::SUBTRACT:  line += "SUBTRACT "  + ins.arg1 + " " + ins.arg2 + " " + ins.arg3; break;
        case OpCode::SLEEP:     line += "SLEEP "     + ins.arg2; break;
        case OpCode::FOR_BEGIN: line += "FOR_BEGIN " + ins.arg2; break;
        case OpCode::FOR_END:   line += "FOR_END";   break;
        default:                line += "NOP";       break;
    }
    if (!printText.empty()) line += "  ->  " + printText;

    p.outBuf.emplace_back(std::move(line));
}

void Scheduler::coreFunction(int coreId)
{
    auto strip = [](const std::string& s){ return (s.size() >= 2 && s.front() == '"' && s.back() == '"') ? s.substr(1, s.size() - 2) : s; };
    auto valOf = [](const ProcessInfo& p,const std::string& n){ auto it = p.vars.find(n); return it != p.vars.end() ? std::to_string(it->second) : "0"; };
    auto logInstr = [](ProcessInfo& p,int c,const Instruction& ins,const std::string& extra=""){
        std::string l = "Core:" + std::to_string(c) + " [" + std::to_string(p.executedLines) + "] ";
        switch(ins.op){
            case OpCode::PRINT:     l += "PRINT "     + ins.arg1 + (ins.arg2.empty() ? "" : (" + " + ins.arg2)); break;
            case OpCode::DECLARE:   l += "DECLARE "   + ins.arg1 + "=" + ins.arg2;                               break;
            case OpCode::ADD:       l += "ADD "       + ins.arg1 + " " + ins.arg2 + " " + ins.arg3;              break;
            case OpCode::SUBTRACT:  l += "SUBTRACT "  + ins.arg1 + " " + ins.arg2 + " " + ins.arg3;              break;
            case OpCode::SLEEP:     l += "SLEEP "     + ins.arg2;                                                break;
            case OpCode::FOR_BEGIN: l += "FOR_BEGIN " + ins.arg2;                                                break;
            case OpCode::FOR_END:   l += "FOR_END";                                                               break;
            default:                l += "NOP";                                                                  break;
        }
        if(!extra.empty()) l += " -> " + extra;
        p.outBuf.emplace_back(std::move(l));
    };

    while (true) {
        ProcessInfo proc(-1,"",0,"");
        {
            std::unique_lock<std::mutex> lk(queueMutex);
            cv.wait(lk,[&]{ return !running || !processQueue.empty(); });
            if (!running && processQueue.empty()) return;
            proc = std::move(processQueue.front());
            processQueue.pop_front();
            proc.assignedCore = coreId;
            runningProcesses.push_back(proc);
            ++coresInUse;
        }

        bool fcfs  = schedulerType == "fcfs" || schedulerType == "FCFS";
        int  slice = fcfs ? INT_MAX : std::max(1, quantum);
        int  exec  = 0;

        while (proc.sleepTicks == 0 && proc.currentLine < proc.totalLine && exec < slice && running) {
            Instruction& in = proc.prog[proc.currentLine];

            switch (in.op) {
            case OpCode::PRINT: {
                std::string msg = !in.arg2.empty() ? strip(in.arg1) + valOf(proc,in.arg2)
                                   : proc.vars.count(in.arg1) ? valOf(proc,in.arg1)
                                   : strip(in.arg1);
                if (g_attachedPid.load() == proc.processID) {
                    std::lock_guard<std::mutex> g(g_coutMx);
                    std::cout << msg << '\n';
                }
                logInstr(proc, coreId, in, msg);
                break;
            }
            case OpCode::DECLARE:
                proc.vars[in.arg1] = Stoi16(in.arg2);
                logInstr(proc, coreId, in);
                break;
            case OpCode::ADD:
            case OpCode::SUBTRACT: {
                uint16_t v2 = in.isArg2Var ? proc.vars[in.arg2] : Stoi16(in.arg2);
                uint16_t v3 = in.isArg3Var ? proc.vars[in.arg3] : Stoi16(in.arg3);
                uint32_t r = in.op == OpCode::ADD ? v2 + v3 : (v2 >= v3 ? v2 - v3 : 0u);
                proc.vars[in.arg1] = static_cast<uint16_t>(std::min(r, 65535u));
                logInstr(proc, coreId, in);
                break;
            }
            case OpCode::SLEEP:
                proc.sleepTicks = Stoi16(in.arg2);
                logInstr(proc, coreId, in);
                break;
            case OpCode::FOR_BEGIN:
                if (proc.loopStack.size() < 3) {
                    int rpt = Stoi16(in.arg2);
                    proc.loopStack.push_back({ proc.currentLine + 1, rpt });
                }
                logInstr(proc, coreId, in);
                break;
            case OpCode::FOR_END:
                if (!proc.loopStack.empty()) {
                    auto& t = proc.loopStack.back();
                    if (--t.remaining > 0) proc.currentLine = t.startIdx - 1;
                    else proc.loopStack.pop_back();
                }
                logInstr(proc, coreId, in);
                break;
            default: break;
            }

            ++proc.currentLine;
            ++proc.executedLines;
            ++exec;
        }

        if (config.delaysPerExec > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(config.delaysPerExec));

        bool done = proc.currentLine >= proc.totalLine && proc.loopStack.empty() && proc.sleepTicks == 0;

        {
            std::lock_guard<std::mutex> lk(queueMutex);
            runningProcesses.erase(std::remove_if(runningProcesses.begin(), runningProcesses.end(),
                [&](const ProcessInfo& p){ return p.processID == proc.processID; }), runningProcesses.end());
            --coresInUse;

            if (!proc.outBuf.empty()) {
                std::ofstream f(proc.processName + ".txt", std::ios::app);
                for (auto& s : proc.outBuf) f << s << '\n';
                proc.outBuf.clear();
            }

            if (done) {
                finishedProcesses.emplace_back(proc, coreId);
            } else if (!fcfs) {
                processQueue.erase(std::remove_if(processQueue.begin(), processQueue.end(),
                    [&](const ProcessInfo& p){ return p.processID == proc.processID; }), processQueue.end());
                processQueue.emplace_back(std::move(proc));
            }
        }
        cv.notify_all();
    }
}
