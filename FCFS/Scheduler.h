#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <deque>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "ProcessInfo.h"
#include "Config.h"

class Scheduler {
private:
    Config config;
    std::deque<ProcessInfo> processQueue;
    std::vector<std::pair<ProcessInfo, int>> finishedProcesses;
    std::vector<ProcessInfo> runningProcesses;
    std::vector<std::thread> coreThreads;
    std::mutex queueMutex;
    std::condition_variable cv;
    bool running;
    void coreFunction(int coreId);
    std::vector<ProcessInfo> waitingProcesses;

    int quantum;
    std::string schedulerType;

public:
    Scheduler(const Config& config);
    ~Scheduler();
    ProcessInfo& getProcess(const std::string& name);
    void start();
    void stop();
    void addProcess(const ProcessInfo& process);
    std::vector<std::pair<ProcessInfo, int>> getFinishedProcesses();
    std::vector<ProcessInfo> getRunningProcesses();
    std::vector<ProcessInfo> getWaitingProcesses();
};

#endif