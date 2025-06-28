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

static uint16_t Stoi16(const std::string& s, const std::string& ctx = "<unknown>") {
    try {
        long v = std::stol(s);
        if (v < 0) v = 0;
        if (v > 65535) v = 65535;
        return static_cast<uint16_t>(v);
    } catch (const std::exception& e) {
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

ProcessInfo Scheduler::snapshotProcess(const std::string& name)
{
    std::lock_guard<std::mutex> lk(queueMutex);

    for (auto& p : runningProcesses) if (p.processName == name) return p;
    for (auto& p : processQueue)    if (p.processName == name) return p;
    for (auto& e : finishedProcesses) if (e.first.processName == name) return e.first;

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

void Scheduler::coreFunction(int nCoreId)
{
    auto strip = [](const std::string& s) -> std::string {
        return (s.size() >= 2 && s.front() == '"' && s.back() == '"')
             ? s.substr(1, s.size() - 2) : s;
    };
    auto varVal = [](const ProcessInfo& p, const std::string& v) -> std::string {
        auto it = p.vars.find(v);
        return (it != p.vars.end()) ? std::to_string(it->second) : "0";
    };
    auto log = [&](ProcessInfo& p, int c, const std::string& txt, int ind) {
        p.outBuf.emplace_back("Core:" + std::to_string(c) + " [" +
                              std::to_string(p.executedLines) + "] " +
                              std::string(ind * 4, ' ') + txt);
    };

    while (running)
    {
        ProcessInfo proc(-1,"",0,"");
        {
            std::unique_lock<std::mutex> lk(queueMutex);
            cv.wait(lk,[&]{ return !processQueue.empty() || !running; });
            if (!running && processQueue.empty()) return;

            proc = std::move(processQueue.front());
            processQueue.pop_front();
            proc.assignedCore = nCoreId;
            runningProcesses.push_back(proc);
            ++coresInUse;
        }

        const bool fcfs  = (schedulerType == "fcfs" || schedulerType == "FCFS");
        const int  slice = fcfs ? std::numeric_limits<int>::max()
                                : std::max(1, quantum);

        auto& loopStack = proc.loopStack;

        for (int used = 0; used < slice && running; )
        {
            if (proc.sleepTicks) {
                --proc.sleepTicks;
                ++proc.executedLines;
                ++used;
                break;
            }

            // program finished?
            if (proc.currentLine >= static_cast<int>(proc.prog.size()))
                break;

            Instruction& ins = proc.prog[proc.currentLine];
            int indent = static_cast<int>(loopStack.size());

            switch (ins.op)
            {
                case OpCode::PRINT:
                {
                    std::string msg;
                    if (!ins.arg2.empty())
                        msg = strip(ins.arg1) + "+" + ins.arg2 + ": "
                            + varVal(proc, ins.arg2);
                    else
                        msg = strip(ins.arg1);

                    if (g_attachedPid.load() == proc.processID) {
                        std::lock_guard<std::mutex> _(g_coutMx);
                        std::cout << '\r' << msg << std::flush; // marquee
                    }
                    log(proc, nCoreId, "PRINT -> " + msg, indent);
                    break;
                }

                case OpCode::DECLARE:
                    proc.vars[ins.arg1] = Stoi16(ins.arg2);
                    log(proc, nCoreId, "DECLARE " + ins.arg1 + '=' + ins.arg2, indent);
                    break;

                case OpCode::ADD:
                case OpCode::SUBTRACT:
                {
                    uint16_t v2 = ins.isArg2Var ? proc.vars[ins.arg2] : Stoi16(ins.arg2);
                    uint16_t v3 = ins.isArg3Var ? proc.vars[ins.arg3] : Stoi16(ins.arg3);
                    uint32_t r  = (ins.op == OpCode::ADD) ? v2 + v3
                                                           : (v2 >= v3 ? v2 - v3 : 0u);
                    proc.vars[ins.arg1] = static_cast<uint16_t>(std::min(r, 65535u));
                    log(proc, nCoreId,
                        (ins.op == OpCode::ADD ? "ADD " : "SUB ") + ins.arg1, indent);
                    break;
                }

                case OpCode::SLEEP:
                {
                    uint16_t ticks = Stoi16(ins.arg2);
                    proc.sleepTicks = ticks ? ticks - 1 : 0;
                    log(proc, nCoreId, "SLEEP " + ins.arg2, indent);
                    ++proc.currentLine; ++proc.executedLines; ++used;
                    continue; // next tick
                }

                case OpCode::FOR:
                {
                    if (ins.body.empty() || ins.repetitions == 0)
                        break;

                    const uint16_t reps = ins.repetitions;
                    const int      body = static_cast<int>(ins.body.size());
                    const int      insert = proc.currentLine + 1;

                    proc.prog.insert(proc.prog.begin() + insert,
                                    ins.body.begin(), ins.body.end());
                    proc.totalLine = static_cast<int>(proc.prog.size());

                    for (auto& f : loopStack)
                        if (f.end >= insert) f.end += body;

                    loopStack.push_back({ insert,
                                        insert + body - 1,
                                        static_cast<uint16_t>(reps - 1),
                                        indent });

                    log(proc, nCoreId,
                        "FOR ×" + std::to_string(reps) +
                        " (body " + std::to_string(body) + ')',
                        indent);
                    break;
                }

                default: break;
            }

            ++proc.currentLine;
            ++proc.executedLines;
            ++used;

            // for loop accounting in here
            if (!loopStack.empty()) {
                auto& top = loopStack.back();
                if (proc.currentLine > top.end) {
                    if (top.remain) {
                        --top.remain;
                        proc.currentLine = top.start;
                    } else {
                        loopStack.pop_back();
                    }
                }
            }
        } // end of this time-slice for RR or block for FCFS

        if (config.delaysPerExec)
            std::this_thread::sleep_for(std::chrono::milliseconds(config.delaysPerExec));

        bool done = (proc.currentLine >= proc.totalLine) && (proc.sleepTicks == 0);

        // process removal, logging, re-queuing
        {
            std::lock_guard<std::mutex> lk(queueMutex);
            runningProcesses.erase(
                std::remove_if(runningProcesses.begin(), runningProcesses.end(),
                               [&](const ProcessInfo& p){ return p.processID == proc.processID; }),
                runningProcesses.end());
            --coresInUse;

            if (!proc.outBuf.empty()) {
                std::ofstream f(proc.processName + ".txt", std::ios::app);
                for (auto& s : proc.outBuf) f << s << '\n';
                proc.outBuf.clear();
            }

            if (done)
                finishedProcesses.emplace_back(proc, nCoreId);
            else
                processQueue.emplace_back(std::move(proc)); // back into ready-queue
        }
        cv.notify_all();
    }
}
