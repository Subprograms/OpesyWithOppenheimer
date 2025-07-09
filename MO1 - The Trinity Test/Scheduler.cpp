#include "Scheduler.h"
#include "Commands.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <mutex>
extern std::atomic<int> g_attachedPid;
int curQuantumCycle = 1;

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

bool Scheduler::allocateMemory(ProcessInfo& proc) {
    const int frameSize = config.memPerFrame;
    const int blockReq = config.memPerProc;
    // Round up required bytes to nearest frame
    int reqFrames = (blockReq + frameSize - 1) / frameSize;
    int reqBytes  = reqFrames * frameSize;

    int lastEnd = 0;
    for (auto it = memoryBlocks.begin(); it != memoryBlocks.end(); ++it) {
        int alignedEnd = ((lastEnd + frameSize - 1) / frameSize) * frameSize;
        int gap = it->start - alignedEnd;
        if (gap >= reqBytes) {
            memoryBlocks.insert(it,
                MemoryBlock{ alignedEnd,
                             alignedEnd + reqBytes,
                             proc.processName });
            return true;
        }
        lastEnd = it->end;
    }

    int alignedEnd = ((lastEnd + frameSize - 1) / frameSize) * frameSize;
    if (config.maxOverallMem - alignedEnd >= reqBytes) {
        memoryBlocks.push_back(
            MemoryBlock{ alignedEnd,
                         alignedEnd + reqBytes,
                         proc.processName });
        return true;
    }

    return false;
}

void Scheduler::deallocateMemory(const std::string& pid) {
    memoryBlocks.erase(
        std::remove_if(
            memoryBlocks.begin(),
            memoryBlocks.end(),
            [&](auto& b){ return b.pid == pid; }
        ),
        memoryBlocks.end()
    );
}

void Scheduler::writeMemorySnapshot() {
    std::lock_guard<std::mutex> lk(queueMutex);

    std::ostringstream fn;
    fn << "memory_stamp_"
       << std::setw(2) << std::setfill('0') << curQuantumCycle
       << ".txt";

    std::ofstream file(fn.str());
    if (!file.is_open()) return;

    file << "Timestamp: (" << Commands::getCurrentTimestamp() << ")\n";
    file << "Number of processes in memory: "
         << memoryBlocks.size() << "\n";

    // Compute total external fragmentation (bytes)
    int totalFrag = 0;
    int lastEnd   = 0;
    std::vector<MemoryBlock> asc = memoryBlocks;
    std::sort(asc.begin(), asc.end(),
              [](auto const &a, auto const &b){ return a.start < b.start; });
    for (auto const &blk : asc) {
        if (blk.start > lastEnd)
            totalFrag += blk.start - lastEnd;
        lastEnd = blk.end;
    }
    if (lastEnd < config.maxOverallMem)
        totalFrag += config.maxOverallMem - lastEnd;

    file << "Total external fragmentation in KB: "
         << (totalFrag / 1024) << "\n\n";

    file << "----end---- = " << config.maxOverallMem << "\n\n";

    std::vector<MemoryBlock> desc = memoryBlocks;
    std::sort(desc.begin(), desc.end(),
              [](auto const &a, auto const &b){ return a.start > b.start; });
    for (auto const &blk : desc) {
        file << blk.end << "\n"
             << blk.pid << "\n"
             << blk.start << "\n\n";
    }

    file << "----start---- = 0\n";
    file.close();

    ++curQuantumCycle;
}

void Scheduler::coreFunction(int nCoreId)
{
    auto nowStamp = []() -> std::string {
        using namespace std::chrono;
        auto tp = system_clock::now();
        auto ms = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;
        std::time_t tt = system_clock::to_time_t(tp);
        std::tm tm;
    #ifdef _WIN32
        localtime_s(&tm, &tt);
    #else
        localtime_r(&tt, &tm);
    #endif
        std::ostringstream os;
        os << std::put_time(&tm, "%H:%M:%S") << '.'
           << std::setw(3) << std::setfill('0') << ms.count();
        return os.str();
    };

    auto log = [&](ProcessInfo& p, const std::string& what, int indent) {
        p.outBuf.emplace_back(
            nowStamp() + " | Core:" + std::to_string(nCoreId)
          + " [" + std::to_string(p.executedLines) + "] "
          + std::string(indent * 4, ' ')
          + what
        );
    };

    while (running)
    {
        ProcessInfo proc(-1, "", 0, "");

        {
            std::unique_lock<std::mutex> lk(queueMutex);
            cv.wait(lk, [&]{ return !processQueue.empty() || !running; });
            if (!running && processQueue.empty()) return;

            proc = std::move(processQueue.front());
            processQueue.pop_front();

            bool alreadyInMem = std::any_of(
                memoryBlocks.begin(), memoryBlocks.end(),
                [&](auto const &blk){ return blk.pid == proc.processName; }
            );
            if (!alreadyInMem) {
                if (!allocateMemory(proc)) {
                    processQueue.push_back(std::move(proc));
                    continue;
                }
            }

            proc.assignedCore = nCoreId;
            runningProcesses.push_back(proc);
            ++coresInUse;
        }

        const bool fcfs = (schedulerType == "fcfs" || schedulerType == "FCFS");
        const int  slice = fcfs
                          ? std::numeric_limits<int>::max()
                          : std::max(1, quantum);
        auto&      loopStack = proc.loopStack;

        for (int used = 0; used < slice && running; )
        {
            if (proc.sleepTicks) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(config.delaysPerExec)
                );
                --proc.sleepTicks;
                ++used;
                if (!proc.sleepTicks) ++proc.currentLine;
                if (used % config.quantumCycles == 0) writeMemorySnapshot();
                continue;
            }

            if (proc.currentLine >= static_cast<int>(proc.prog.size()))
                break;

            Instruction& ins = proc.prog[proc.currentLine];
            int indent = static_cast<int>(loopStack.size());

            switch (ins.op)
            {
                case OpCode::PRINT: {
                    std::string txt = stripQuotes(ins.arg1);
                    if (ins.arg2.size()) txt += '+' + ins.arg2
                                          + ": " + varVal(proc, ins.arg2);
                    log(proc, "PRINT -> " + txt, indent);
                    break;
                }

                case OpCode::DECLARE:
                    proc.vars[ins.arg1] = Stoi16(ins.arg2, ins.arg1);
                    log(proc, "DECLARE " + ins.arg1 + '=' + ins.arg2, indent);
                    break;

                case OpCode::ADD:
                case OpCode::SUBTRACT: {
                    uint16_t v2 = ins.isArg2Var
                                ? proc.vars[ins.arg2]
                                : Stoi16(ins.arg2, ins.arg2);
                    uint16_t v3 = ins.isArg3Var
                                ? proc.vars[ins.arg3]
                                : Stoi16(ins.arg3, ins.arg3);
                    uint32_t r = (ins.op == OpCode::ADD)
                                   ? v2 + v3
                                   : (v2 >= v3 ? v2 - v3 : 0);
                    proc.vars[ins.arg1] = static_cast<uint16_t>(std::min(r, 65535u));
                    log(proc,
                        (ins.op == OpCode::ADD ? "ADD(" : "SUB(")
                      + ins.arg1 + ", " + ins.arg2 + ", " + ins.arg3 + ')',
                        indent);
                    break;
                }

                case OpCode::SLEEP: {
                    auto t = Stoi16(ins.arg2);
                    proc.sleepTicks = (t>0 ? t-1 : 0);
                    log(proc, "SLEEP " + std::to_string(t), indent);
                    ++proc.executedLines;
                    ++proc.currentLine;
                    used = slice;
                    if (used % config.quantumCycles == 0)
                        writeMemorySnapshot();
                    continue;
                }

                case OpCode::FOR: {
                    auto  body = ins.body;
                    auto  reps = ins.repetitions;
                    if (!body.empty() && reps > 0 && loopStack.size() < 3) {
                        int bodySz   = static_cast<int>(body.size());
                        int insertAt = proc.currentLine + 1;

                        proc.prog.insert(
                            proc.prog.begin() + insertAt,
                            body.begin(), body.end()
                        );

                        for (auto& f : loopStack)
                            if (f.end >= insertAt) f.end += bodySz;

                        loopStack.push_back({
                            uint16_t(insertAt),
                            uint16_t(insertAt + bodySz - 1),
                            uint16_t(reps - 1),
                            indent
                        });

                        log(proc,
                            "FOR×" + std::to_string(reps)
                        + " body=" + std::to_string(bodySz),
                            indent
                        );
                    }
                    break;
                }

                default: break;
            }

            ++proc.currentLine;
            ++proc.executedLines;
            if (proc.executedLines > proc.totalLine)
                proc.totalLine = proc.executedLines;
            ++used;
            if (used % config.quantumCycles == 0)
                writeMemorySnapshot();

            if (!loopStack.empty()) {
                auto& top = loopStack.back();
                if (proc.currentLine > top.end) {
                    if (top.remain) {
                        --top.remain;
                        proc.currentLine = top.start;
                    }
                    else {
                        loopStack.pop_back();
                    }
                }
            }
        }

        if (config.delaysPerExec)
            std::this_thread::sleep_for(
              std::chrono::milliseconds(config.delaysPerExec)
            );

        bool finished = (proc.currentLine >= static_cast<int>(proc.prog.size()))
                     && proc.sleepTicks == 0;

        {
            std::lock_guard<std::mutex> lk(queueMutex);

            runningProcesses.erase(
                std::remove_if(
                    runningProcesses.begin(),
                    runningProcesses.end(),
                    [&](auto const& p){ return p.processID == proc.processID; }
                ),
                runningProcesses.end()
            );
            --coresInUse;

            if (!proc.outBuf.empty()) {
                std::ofstream f(proc.processName + ".txt", std::ios::app);
                for (auto& line : proc.outBuf) f << line << "\n";
                proc.outBuf.clear();
            }

            if (finished) {
                deallocateMemory(proc.processName);
                finishedProcesses.emplace_back(proc, nCoreId);
            }
            else {
                processQueue.push_back(std::move(proc));
            }
        }

        cv.notify_all();
    }
}