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

void Scheduler::coreFunction(int coreId)
{
    auto strip = [](const std::string& s)
                 { return (s.size() >= 2 && s.front() == '"' && s.back() == '"')
                          ? s.substr(1, s.size() - 2) : s; };

    auto valOf = [](const ProcessInfo& p, const std::string& n)
                 { auto it = p.vars.find(n);
                   return it != p.vars.end() ? std::to_string(it->second) : "0"; };

    auto log = [](ProcessInfo& p, int core, const std::string& line)
               {
                   p.outBuf.emplace_back(
                       "Core:" + std::to_string(core) +
                       " ["    + std::to_string(p.executedLines) + "] " + line);
               };

    while (true)
    {
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

        const bool fcfs  = (schedulerType=="fcfs"||schedulerType=="FCFS");
        const int  slice = fcfs ? INT_MAX : std::max(1,quantum);

        int executed = 0;
        while (proc.sleepTicks==0 &&
               proc.currentLine < proc.totalLine &&
               executed < slice && running)
        {
            Instruction ins = proc.prog[proc.currentLine];

            switch (ins.op)
            {
                case OpCode::PRINT:
                {
                    std::string msg = !ins.arg2.empty()       ? strip(ins.arg1)+valOf(proc,ins.arg2)
                                        : proc.vars.count(ins.arg1) ? valOf(proc,ins.arg1)
                                        : strip(ins.arg1);

                    if (g_attachedPid.load()==proc.processID)
                    {
                        std::lock_guard<std::mutex> _(g_coutMx);
                        std::cout << msg << '\n';
                    }
                    log(proc,coreId,"PRINT " + ins.arg2 + " -> " + msg);
                    break;
                }

                case OpCode::DECLARE:
                    proc.vars[ins.arg1] = Stoi16(ins.arg2);
                    log(proc,coreId,"DECLARE "+ins.arg1+'='+ins.arg2);
                    break;

                case OpCode::ADD:
                case OpCode::SUBTRACT:
                {
                    uint16_t v2 = ins.isArg2Var ? proc.vars[ins.arg2] : Stoi16(ins.arg2);
                    uint16_t v3 = ins.isArg3Var ? proc.vars[ins.arg3] : Stoi16(ins.arg3);
                    uint32_t r = (ins.op==OpCode::ADD) ? v2+v3 : (v2>=v3 ? v2-v3 : 0u);
                    proc.vars[ins.arg1] = static_cast<uint16_t>(std::min(r,65535u));
                    log(proc,coreId,(ins.op==OpCode::ADD? "ADD ":"SUB ") + ins.arg1);
                    break;
                }

                case OpCode::SLEEP:
                    proc.sleepTicks = Stoi16(ins.arg2);
                    log(proc,coreId,"SLEEP "+ins.arg2);
                    break;

                case OpCode::FOR:        /* our single-instruction loop */
                {
                    int reps = Stoi16(ins.arg2);
                    if (reps>0 && !ins.body.empty())
                    {
                        std::size_t insertPos = proc.currentLine + 1;
                        for (int r=0;r<reps && running;++r)
                        {
                            proc.prog.insert(proc.prog.begin()+insertPos,
                                             ins.body.begin(), ins.body.end());
                            insertPos += ins.body.size();
                        }
                        proc.totalLine = static_cast<int>(proc.prog.size());
                        log(proc,coreId,"FOR ×"+ins.arg2+
                                        " (body "+std::to_string(ins.body.size())+')');
                    }
                    break;
                }

                default: break;   // NOP
            }

            ++proc.currentLine;
            ++proc.executedLines;
            ++executed;

            /* keep mirror copy up-to-date so screen-smi shows progress */
            for (auto& p : runningProcesses)
                if (p.processID == proc.processID)
                {   p.currentLine   = proc.currentLine;
                    p.executedLines = proc.executedLines;
                    p.assignedCore  = coreId;
                    break;   }
        }

        if (proc.sleepTicks>0) --proc.sleepTicks;
        if (config.delaysPerExec>0)
            std::this_thread::sleep_for(std::chrono::milliseconds(config.delaysPerExec));

        const bool finished = (proc.currentLine>=proc.totalLine) &&
                              proc.sleepTicks==0;

        {
            std::lock_guard<std::mutex> lk(queueMutex);

            runningProcesses.erase(
                std::remove_if(runningProcesses.begin(),runningProcesses.end(),
                    [&](const ProcessInfo& p){return p.processID==proc.processID;}),
                runningProcesses.end());
            --coresInUse;

            if (!proc.outBuf.empty())
            {
                std::ofstream f(proc.processName+".txt",std::ios::app);
                for (auto& s:proc.outBuf) f<<s<<'\n';
                proc.outBuf.clear();
            }

            if (finished)
                finishedProcesses.emplace_back(proc,coreId);
            else if (!fcfs)
            {
                proc.assignedCore = -1;
                processQueue.emplace_back(std::move(proc));
            }
        }
        cv.notify_all();
    }
}
