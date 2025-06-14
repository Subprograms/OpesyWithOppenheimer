#include "Commands.h"
#include "ProcessInfo.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <chrono>

static int nextProcessID = 1; // For unique process IDs

std::string Commands::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto epoch = now_ms.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch);
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(epoch - seconds);

    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;
#ifdef _WIN32
    localtime_s(&localTime, &now_c);
#else
    localtime_r(&now_c, &localTime);
#endif

    std::ostringstream oss;
    oss << std::put_time(&localTime, "%m/%d/%Y, %I:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << milliseconds.count();
    oss << (localTime.tm_hour < 12 ? " AM" : " PM");

    return oss.str();
}

// Constructor
Commands::Commands() : scheduler(nullptr) {}

void Commands::initialize() {
    if (scheduler == nullptr) {
        std::string filename;
        bool fileLoaded = false;

        while (!fileLoaded) {
            std::cout << "Please enter the path to the config file (e.g., config.txt): ";
            std::getline(std::cin, filename);

            std::ifstream file(filename);
            if (!file.is_open()) {
                std::cerr << "Error: Could not open the specified file. Please enter a valid path." << std::endl;
                continue;
            }
            file.close();

            try {
                config = parseConfigFile(filename);
                scheduler = std::make_unique<Scheduler>(config);
                std::cout << "Scheduler initialized with " << config.numCpu << " CPUs." << std::endl;
                fileLoaded = true;
            }
            catch (const std::exception& e) {
                std::cerr << "Error parsing config file: " << e.what() << std::endl;
            }
        }
    }
}

Config Commands::parseConfigFile(const std::string& filename) {
    Config config;
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open config file");
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        if (!(iss >> key)) continue;

        if (key == "num-cpu") iss >> config.numCpu;
        else if (key == "scheduler") iss >> config.scheduler;
        else if (key == "quantum-cycles") iss >> config.quantumCycles;
        else if (key == "batch-process-freq") iss >> config.batchProcessFreq;
        else if (key == "min-ins") iss >> config.minIns;
        else if (key == "max-ins") iss >> config.maxIns;
        else if (key == "delays-per-exec") iss >> config.delaysPerExec;
    }
    file.close();
    return config;
}

void Commands::initialScreen() {
    clearScreen();
    menuView();
}

void Commands::processCommand(const std::string& command) {
    if (command.find("screen") != std::string::npos) {
        screenCommand(command);
    }
    else if (command == "scheduler-test") {
        schedulerTestCommand();
    }
    else if (command == "scheduler-stop") {
        schedulerStopCommand();
    }
    else if (command == "report-util") {
        reportUtilCommand();
    }
    else if (command == "clear") {
        clearScreen();
        menuView();
    }
    else if (command == "exit") {
        std::cout << "Terminating Serial OS, Thank you!" << std::endl;
        exit(0);
    }
    else {
        std::cout << "ERROR: Unrecognized command." << std::endl;
    }
}

// Screen-related commands
void Commands::screenCommand(const std::string& command) {
    std::string subCommand, name;
    std::vector<std::string> subCommands = { "-r", "-s", "-ls" };

    std::istringstream iss(command);
    iss >> subCommand >> subCommand >> name;

    if (subCommand != "-ls" && name.empty()) {
        std::cout << "ERROR: Process Not Specified" << std::endl;
        return;
    }

    auto it = std::find(subCommands.begin(), subCommands.end(), subCommand);
    int found = (it != subCommands.end()) ? std::distance(subCommands.begin(), it) : -1;

    switch (found) {
    case 0: rSubCommand(name); break; // Reattach
    case 1: sSubCommand(name); break; // Start a new process
    case 2: lsSubCommand(); break; // List processes
    default: std::cout << "ERROR: Invalid Subcommand" << std::endl; break;
    }
}

void Commands::rSubCommand(const std::string& name) {
    clearScreen();
    std::cout << "Attempting to reattach to process: " << name << std::endl;

    try {
        ProcessInfo& process = scheduler->getProcess(name);
        enterProcessScreen(process);
    }
    catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
    }
}


void Commands::sSubCommand(const std::string& name) {
    try {
        ProcessInfo& existingProcess = scheduler->getProcess(name);
        std::cout << "Reattaching to existing process: " << name << std::endl;
        enterProcessScreen(existingProcess);
    }
    catch (const std::runtime_error& e) {
        std::cout << "Creating new process \"" << name << "\".\n";

        ProcessInfo newProcess(nextProcessID++, name, 100, getCurrentTimestamp(), false);
        scheduler->addProcess(newProcess);

        enterProcessScreen(newProcess);
    }
}

void Commands::enterProcessScreen(ProcessInfo& process) {
    clearScreen();
    bool isRunning = true;

    std::cout << "Process: " << process.processName << "\nID: " << process.processID
        << "\nTotal Lines: " << process.totalLine << std::endl;

    while (isRunning) {
        if (process.isFinished) {
            std::cout << "Process has finished.\n";
            isRunning = false;
            continue;
        }

        std::string command;
        std::cout << "> ";
        std::getline(std::cin, command);

        if (command == "process-smi") {
            displayProcessSmi(process);
        }
        else if (command == "exit") {
            std::cout << "Exiting process screen...\n";
            isRunning = false;
        }
        else {
            std::cout << "Invalid command. Available commands: 'process-smi', 'exit'.\n";
        }
    }
}

void Commands::displayProcessSmi(ProcessInfo& process) {
    std::cout << "Process: " << process.processName << "\nID: " << process.processID
        << "\nCurrent instruction line: " << process.currentLine
        << "\nLines of code: " << process.totalLine << "\n\n";
}

// Scheduler-related commands
void Commands::schedulerTestCommand() {
    if (!scheduler) {
        std::cout << "Scheduler is not initialized. Please run 'initialize' first." << std::endl;
        return;
    }

    std::cout << "Scheduling 10 Processes on " << config.numCpu << " CPU Cores (Check via screen -ls)" << std::endl;

    for (int i = 1; i <= 10; ++i) {
        ProcessInfo process(nextProcessID++, "process" + std::to_string(i), 100, getCurrentTimestamp(), false);
        scheduler->addProcess(process); // Only add to scheduler
        std::this_thread::sleep_for(std::chrono::milliseconds(1));  // Add 1 ms delay for different timestamps
    }
}

void Commands::schedulerStopCommand() {
    scheduler->stop();
}

// Process reporting and display
void Commands::writeProcessReport(std::ostream& os) {
    os << "Waiting Queue:\n";
    for (const auto& process : scheduler->getWaitingProcesses()) {
        os << process.processName << "\t(" << process.timeStamp
            << ")\tCore: N/A\t" << process.currentLine << " / " << process.totalLine << "\n";
    }

    os << "\nRunning Queue:\n";
    for (const auto& process : scheduler->getRunningProcesses()) {
        os << process.processName << "\t(" << process.timeStamp
            << ")\tCore: " << process.assignedCore << "\t" << process.currentLine << " / " << process.totalLine << "\n";
    }

    os << "\nFinished processes:\n";
    for (const auto& entry : scheduler->getFinishedProcesses()) {
        const auto& process = entry.first;
        int coreId = entry.second;
        os << process.processName << "\t(" << process.timeStamp
            << ")\tFinished\tCore: " << coreId
            << "\t" << process.totalLine << " / " << process.totalLine << "\n";
    }
}

void Commands::lsSubCommand() {
    writeProcessReport(std::cout);
}

void Commands::reportUtilCommand() {
    std::ofstream logFile("csopesy-log.txt");
    if (logFile.is_open()) {
        writeProcessReport(logFile);
        logFile.close();
        std::cout << "Report saved to csopesy-log.txt\n";
    }
    else {
        std::cerr << "Error: Could not open csopesy-log.txt for writing.\n";
    }
}

// Function to display process details
void Commands::displayProcess(const ProcessInfo& process) {
    std::cout << "Displaying Process Details:" << std::endl;
    std::cout << "Process Name: " << process.processName << std::endl;
    std::cout << "Current Line: " << process.currentLine << std::endl;
    std::cout << "Total Lines: " << process.totalLine << std::endl;
    std::cout << "Timestamp: " << process.timeStamp << std::endl;
    std::cout << "Status: " << (process.isFinished ? "Finished" : "Running") << std::endl;
}