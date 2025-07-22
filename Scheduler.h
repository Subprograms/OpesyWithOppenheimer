#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <deque>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include <cstdint>

#include "ProcessInfo.h"
#include "Config.h"
#include "MemoryManager.h"

struct MemoryBlock { int start, end; std::string pid; };

class Scheduler {
public:
    explicit Scheduler(const Config& cfg);
    ~Scheduler();
    void start();
    void stop();
    void addProcess(ProcessInfo&& proc);
    void addProcess(const ProcessInfo& proc);
    ProcessInfo&                    getProcess       (const std::string& name);
    ProcessInfo                     snapshotProcess  (const std::string& name);
    std::vector<std::pair<ProcessInfo,int>> getFinishedProcesses();
    std::vector<ProcessInfo>                getRunningProcesses();
    std::vector<ProcessInfo>                getWaitingProcesses();
    std::string utilisationString() const;
    std::string vmstatString() const;

private:
    Config         config;
    MemoryManager  memoryManager;
    int            quantum;
    std::string    schedulerType;
    std::deque<ProcessInfo>                 processQueue;
    std::vector<ProcessInfo>                runningProcesses;
    std::vector<std::pair<ProcessInfo,int>> finishedProcesses;
    std::vector<std::thread> coreThreads;
    std::mutex               queueMutex;
    std::condition_variable  cv;
    std::atomic<bool>        running{true};
    std::atomic<int>         coresInUse{0};
    std::atomic<std::uint64_t> idleCpuTicks  {0};
    std::atomic<std::uint64_t> activeCpuTicks{0};
    int                       curQuantumCycle{1};
    std::vector<MemoryBlock>  memoryBlocks;
    void coreFunction(int coreId);
    bool allocateMemory  (ProcessInfo& proc);
    void deallocateMemory(const std::string& pid);
    void writeMemorySnapshot();
};

#endif