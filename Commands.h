#ifndef COMMANDS_H
#define COMMANDS_H

#include "Screen.h"
#include "Data.h"
#include "Scheduler.h"
#include "Config.h"

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>

extern std::atomic<int> g_attachedPid;

class Commands : protected Screen, public Data {
private:
    std::unique_ptr<Scheduler> scheduler;
    Config                     config;
    std::vector<ProcessInfo>   processList;
    void writeProcessReport(std::ostream& os);
    void startThread  (ProcessInfo& proc);
    void editProcessScreen(ProcessInfo& proc);
    void enterProcessScreen(ProcessInfo& proc);
    void displayProcessSmi  (ProcessInfo& process);
    Config parseConfigFile  (const std::string& filename);
    std::mutex          queueMutex;
    std::atomic<bool>   batchRunning{false};
    std::thread         batchThread;
    void batchLoop();

public:
    Commands();
    static int         getRandomInt        (int floor, int ceiling);
    static std::string getCurrentTimestamp();
    void initialize      (std::string configFile);
    void initialScreen   ();
    void processCommand  (const std::string& command);
    void rSubCommand (const std::string& name);
    void sSubCommand (const std::string& name, int memSize);
    void lsSubCommand ();
    void screenCommand(const std::string& cmdLine);
    void schedulerStartCommand();
    void schedulerStopCommand();
    void reportUtilCommand();
    void vmstatCommand();
    void displayProcess(const ProcessInfo& process);
    void createProcess (const std::string& name);
};

#endif
