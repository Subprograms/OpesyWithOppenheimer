#include "Scheduler.h"
#include "Commands.h"
#include "MemoryManager.h"
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

static bool ParseUint16(const std::string& s, uint16_t& v)
{
    if (s.empty())
        return false;
    bool hex = (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'));
    const char* p = s.c_str() + (hex ? 2 : 0);
    char* end{};
    unsigned long tmp = std::strtoul(p, &end, hex ? 16 : 10);
    if (*end != '\0')
        return false;
    v = static_cast<uint16_t>(std::min<unsigned long>(tmp, 65535));
    return true;
}

static uint16_t Stoi16(const std::string& s, const std::string& ctx = "<unknown>")
{
    uint16_t v;
    return ParseUint16(s, v) ? v : 0;
}

static bool Hex32(const std::string& s, uint32_t& v)
{
    std::string t = (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
                  ? s.substr(2) : s;
    if (t.empty())
        return false;
    char* end{};
    unsigned long long tmp = std::strtoull(t.c_str(), &end, 16);
    if (*end != '\0' || tmp > 0xFFFFFFFFULL)
        return false;
    v = static_cast<uint32_t>(tmp);
    return true;
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

Scheduler::Scheduler(const Config& cfg): 
  config(cfg), 
  memoryManager(cfg.maxOverallMem, cfg.memPerFrame), 
  running(true), 
  schedulerType(cfg.scheduler), 
  quantum(cfg.quantumCycles)
{
    for(int i=0; i<config.numCpu; ++i)
        coreThreads.emplace_back(&Scheduler::coreFunction, this, i+1);
}

Scheduler::~Scheduler() {
    stop();
    for (auto& t : coreThreads)
        if (t.joinable()) t.join();
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

bool Scheduler::allocateMemory(ProcessInfo& proc)
{
    const int frameSize = config.memPerFrame;
    int reqBytesRaw = proc.memSize;
    if (reqBytesRaw < 64 || reqBytesRaw > 65536 || (reqBytesRaw & (reqBytesRaw - 1))) return false;

    int reqFrames = (reqBytesRaw + frameSize - 1) / frameSize;
    int reqBytes  = reqFrames * frameSize;

    int lastEnd = 0;
    for (auto it = memoryBlocks.begin(); it != memoryBlocks.end(); ++it)
    {
        int aligned = ((lastEnd + frameSize - 1) / frameSize) * frameSize;
        int gap = it->start - aligned;
        if (gap >= reqBytes)
        {
            memoryBlocks.insert(it, { aligned, aligned + reqBytes, proc.processName });
            return true;
        }
        lastEnd = it->end;
    }

    int aligned = ((lastEnd + frameSize - 1) / frameSize) * frameSize;
    if (config.maxOverallMem - aligned >= reqBytes)
    {
        memoryBlocks.push_back({ aligned, aligned + reqBytes, proc.processName });
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
    auto nowStamp = []()
    {
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
        os << std::put_time(&tm, "%H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms.count();
        return os.str();
    };

    auto log = [&](ProcessInfo& p, const std::string& msg, int ind)
    {
        p.outBuf.emplace_back(nowStamp() + " | Core:" + std::to_string(nCoreId) +
                              " [" + std::to_string(p.executedLines) + "] " +
                              std::string(ind * 4, ' ') + msg);
    };

    auto accessMem = [&](ProcessInfo& p, uint32_t a, bool w, uint16_t& v)
    {
        if (a >= static_cast<uint32_t>(p.memSize)) return false;
        return memoryManager.access(p.processID, a, w, v);
    };

    while (running)
    {
        ProcessInfo proc(-1, "", 0, config.memPerProc, "", false);

        {
            std::unique_lock<std::mutex> lk(queueMutex);
            if (processQueue.empty())
            {
                ++idleCpuTicks;
                cv.wait_for(lk, std::chrono::milliseconds(config.delaysPerExec),
                            [&]{ return !processQueue.empty() || !running; });
                if (!running && processQueue.empty()) return;
            }
            if (processQueue.empty()) continue;
            proc = std::move(processQueue.front());
            processQueue.pop_front();
            bool resident = std::any_of(memoryBlocks.begin(), memoryBlocks.end(),
                                        [&](auto& b){ return b.pid == proc.processName; });
            if (!resident && !allocateMemory(proc))
            {
                processQueue.push_back(std::move(proc));
                continue;
            }
            proc.assignedCore = nCoreId;
            runningProcesses.push_back(proc);
            ++coresInUse;
        }

        const bool fcfs = (schedulerType == "fcfs" || schedulerType == "FCFS");
        const int slice = fcfs ? std::numeric_limits<int>::max() : std::max(1, quantum);
        auto& loopStack = proc.loopStack;

        for (int used = 0; used < slice && running; )
        {
            if (proc.sleepTicks)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(config.delaysPerExec));
                --proc.sleepTicks;
                ++used;
                ++activeCpuTicks;
                if (!proc.sleepTicks) ++proc.currentLine;
                if (used % config.quantumCycles == 0) writeMemorySnapshot();
                continue;
            }

            if (proc.currentLine >= static_cast<int>(proc.prog.size())) break;
            Instruction& ins = proc.prog[proc.currentLine];
            int ind = static_cast<int>(loopStack.size());

            switch (ins.op)
            {
                case OpCode::PRINT:
                {
                    std::string txt = stripQuotes(ins.arg1);
                    if (!ins.arg2.empty()) txt += '+' + ins.arg2 + ": " + varVal(proc, ins.arg2);
                    log(proc, "PRINT -> " + txt, ind);
                    break;
                }

                case OpCode::DECLARE:
                    if (proc.vars.size() < 32) proc.vars[ins.arg1] = Stoi16(ins.arg2, ins.arg1);
                    log(proc, "DECLARE " + ins.arg1 + '=' + ins.arg2, ind);
                    break;

                case OpCode::ADD:
                case OpCode::SUBTRACT:
                {
                    uint16_t v2 = ins.isArg2Var ? proc.vars[ins.arg2] : Stoi16(ins.arg2);
                    uint16_t v3 = ins.isArg3Var ? proc.vars[ins.arg3] : Stoi16(ins.arg3);
                    uint32_t r = (ins.op == OpCode::ADD) ? v2 + v3 : (v2 >= v3 ? v2 - v3 : 0);
                    proc.vars[ins.arg1] = static_cast<uint16_t>(std::min(r, 65535u));
                    log(proc, (ins.op == OpCode::ADD ? "ADD(" : "SUB(") +
                               ins.arg1 + ", " + ins.arg2 + ", " + ins.arg3 + ')', ind);
                    break;
                }

                case OpCode::SLEEP:
                {
                    uint16_t t = Stoi16(ins.arg2);
                    proc.sleepTicks = t ? t - 1 : 0;
                    log(proc, "SLEEP " + std::to_string(t), ind);
                    ++proc.executedLines;
                    ++proc.currentLine;
                    used = slice;
                    ++activeCpuTicks;
                    if (used % config.quantumCycles == 0) writeMemorySnapshot();
                    continue;
                }

                case OpCode::FOR:
                {
                    if (!ins.body.empty() && ins.repetitions && loopStack.size() < 3)
                    {
                        int bodySz = static_cast<int>(ins.body.size());
                        uint16_t rep = ins.repetitions;
                        int insertAt = proc.currentLine + 1;
                        proc.prog.insert(proc.prog.begin() + insertAt,
                                         ins.body.begin(), ins.body.end());
                        for (auto& f : loopStack) if (f.end >= insertAt) f.end += bodySz;
                        loopStack.push_back({ uint16_t(insertAt),
                                              uint16_t(insertAt + bodySz - 1),
                                              uint16_t(rep - 1), ind });
                        log(proc, "FOR×" + std::to_string(rep) + " body=" +
                                   std::to_string(bodySz), ind);
                    }
                    break;
                }

                case OpCode::WRITE:
                {
                    uint32_t addr; bool isVar = ins.isArg2Var;
                    uint16_t val = isVar ? proc.vars[ins.arg2] : Stoi16(ins.arg2);
                    if (!Hex32(ins.arg1, addr) || !accessMem(proc, addr, true, val))
                    {
                        {
                            std::lock_guard<std::mutex> lk(g_coutMx);
                            std::cout << "Process " << proc.processName
                                      << " shut down due to memory access violation error that occurred at "
                                      << nowStamp() << ", " << ins.arg1 << " invalid.\n";
                        }
                        proc.isFinished = true;
                        used = slice;
                        ++activeCpuTicks;
                        break;
                    }
                    std::string rhs = isVar ? ins.arg2 + '(' + std::to_string(val) + ')' : ins.arg2;
                    log(proc, "WRITE " + ins.arg1 + " = " + rhs, ind);
                    break;
                }

                case OpCode::READ:
                {
                    uint32_t addr; uint16_t val = 0;
                    if (!Hex32(ins.arg2, addr) || !accessMem(proc, addr, false, val))
                    {
                        {
                            std::lock_guard<std::mutex> lk(g_coutMx);
                            std::cout << "Process " << proc.processName
                                      << " shut down due to memory access violation error that occurred at "
                                      << nowStamp() << ", " << ins.arg2 << " invalid.\n";
                        }
                        proc.isFinished = true;
                        used = slice;
                        ++activeCpuTicks;
                        break;
                    }
                    if (proc.vars.size() < 32) proc.vars[ins.arg1] = val;
                    log(proc, "READ " + ins.arg2 + " -> " + ins.arg1 +
                               " = " + std::to_string(val), ind);
                    break;
                }

                default: break;
            }

            if (proc.isFinished) break;
            ++proc.currentLine;
            ++proc.executedLines;
            if (proc.executedLines > proc.totalLine) proc.totalLine = proc.executedLines;
            ++used;
            ++activeCpuTicks;
            if (used % config.quantumCycles == 0) writeMemorySnapshot();
            if (!loopStack.empty())
            {
                auto& top = loopStack.back();
                if (proc.currentLine > top.end)
                {
                    if (top.remain) { --top.remain; proc.currentLine = top.start; }
                    else loopStack.pop_back();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(config.delaysPerExec));
        }

        bool finished = proc.isFinished ||
                        ((proc.currentLine >= static_cast<int>(proc.prog.size())) && proc.sleepTicks == 0);

        {
            std::lock_guard<std::mutex> lk(queueMutex);
            runningProcesses.erase(std::remove_if(runningProcesses.begin(),
                                                  runningProcesses.end(),
                                                  [&](auto& p){ return p.processID == proc.processID; }),
                                   runningProcesses.end());
            --coresInUse;
            if (!proc.outBuf.empty())
            {
                std::ofstream f(proc.processName + ".txt", std::ios::app);
                for (auto& l : proc.outBuf) f << l << '\n';
                proc.outBuf.clear();
            }
            if (finished)
            {
                deallocateMemory(proc.processName);
                finishedProcesses.emplace_back(proc, nCoreId);
            }
            else processQueue.push_back(std::move(proc));
        }
        cv.notify_all();
    }
}

std::string Scheduler::vmstatString() const
{
    std::ostringstream os;

    // memory usage
    std::size_t bytesUsed = 0;
    for (const auto& blk : memoryBlocks)
        bytesUsed += (blk.end - blk.start);
    std::size_t bytesFree = (bytesUsed >= static_cast<std::size_t>(config.maxOverallMem))
                          ? 0
                          : (config.maxOverallMem - bytesUsed);

    os << "====================  VMSTAT  ====================\n"
       << std::left << std::setw(20) << "Total memory" << ": "
       << config.maxOverallMem << " bytes\n"
       << std::setw(20) << "Used memory"  << ": " << bytesUsed  << " bytes\n"
       << std::setw(20) << "Free memory"  << ": " << bytesFree  << " bytes\n"
       << '\n';

    std::uint64_t idle  = idleCpuTicks.load();
    std::uint64_t busy  = activeCpuTicks.load();
    std::uint64_t total = idle + busy;

    os << std::setw(20) << "Idle cpu ticks"   << ": " << idle  << '\n'
       << std::setw(20) << "Active cpu ticks" << ": " << busy  << '\n'
       << std::setw(20) << "Total cpu ticks"  << ": " << total << '\n'
       << '\n';

    std::uint64_t pin  = memoryManager.pagesPagedIn();
    std::uint64_t pout = memoryManager.pagesPagedOut();

    os << std::setw(20) << "Num paged in"  << ": " << pin  << '\n'
       << std::setw(20) << "Num paged out" << ": " << pout << '\n'
       << "=================================================\n";

    return os.str();
}