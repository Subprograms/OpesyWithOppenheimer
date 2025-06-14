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

class Commands : protected Screen, public Data {
private:
    std::unique_ptr<Scheduler> scheduler;
    Config config;
    std::vector<ProcessInfo> processList;
    void writeProcessReport(std::ostream& os);
    void enterProcessScreen(ProcessInfo& process);
    void displayProcessSmi(ProcessInfo& process);
    Config parseConfigFile(const std::string& filename);
    std::mutex queueMutex;

public:
    Commands();

    static std::string getCurrentTimestamp();
    void initialize();
    void initialScreen();
    void processCommand(const std::string& command);
    void screenCommand(const std::string& command);
    void rSubCommand(const std::string& name);
    void sSubCommand(const std::string& name);
    void lsSubCommand();
    void schedulerTestCommand();
    void schedulerStopCommand();
    void reportUtilCommand();
    void displayProcess(const ProcessInfo& process);
    void createProcess(const std::string& name);
};

#endif
