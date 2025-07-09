#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <deque>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include "ProcessInfo.h"
#include "Config.h"

struct MemoryBlock {
    int start;
    int end;
    std::string pid;
};

class Scheduler {
public:
    explicit Scheduler(const Config& config);
    ~Scheduler();
    void start();
    void stop();
    void addProcess(ProcessInfo&& proc);
    void addProcess(const ProcessInfo& proc);
    ProcessInfo& getProcess(const std::string& name);
    ProcessInfo snapshotProcess(const std::string& name);
    std::vector<std::pair<ProcessInfo,int>> getFinishedProcesses();
    std::vector<ProcessInfo> getRunningProcesses();
    std::vector<ProcessInfo> getWaitingProcesses();
    std::string utilisationString() const;

private:
    Config config;
    int quantum;
    std::string schedulerType;
    std::deque<ProcessInfo> processQueue;
    std::vector<ProcessInfo> runningProcesses;
    std::vector<std::pair<ProcessInfo,int>> finishedProcesses;
    std::vector<std::thread> coreThreads;
    std::mutex queueMutex;
    std::condition_variable cv;
    std::atomic<bool> running{true};
    std::atomic<int> coresInUse{0};
    int curQuantumCycle{0};
    std::vector<MemoryBlock> memoryBlocks;

    void coreFunction(int coreId);
    int getRandomInt(int floor, int ceiling);
    bool allocateMemory(ProcessInfo& proc);
    void deallocateMemory(const std::string& pid);
    void writeMemorySnapshot();
};

#endif