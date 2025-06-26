#include "Commands.h"
#include "ProcessInfo.h"
#include "Instruction.h"
#include "Scheduler.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <chrono>
#include <random>
#include <atomic>
std::atomic<int> g_attachedPid{-1};

static std::string randVar()
{
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> pick('a', 'z');
    return std::string(1, static_cast<char>(pick(rng)));
}

static Instruction randomInstr(const Config& cfg,
                               std::vector<std::string>& vars)
{
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> pick(0, 5);
    int code = pick(rng);

    switch (code)
    {
    /* ---------- PRINT ------------------------------------ */
    case 0:
        if (vars.empty()) { // no var yet? force DECLARE one
            std::string v = randVar();
            vars.push_back(v);
            std::string val = std::to_string(Commands::getRandomInt(0, 65535));
            return Instruction{OpCode::DECLARE, v, val, "", false, true };
        } else {
            std::string v = vars[rng() % vars.size()];
            return Instruction{OpCode::PRINT, "", v, "", false, true };
        }

    /* ---------- DECLARE ---------------------------------- */
    case 1: {
        std::string v = randVar();
        vars.push_back(v);
        std::string val = std::to_string(Commands::getRandomInt(0, 65535));
        return Instruction{OpCode::DECLARE, v, val, "", false, true };
    }

    /* ---------- ADD / SUBTRACT --------------------------- */
    case 2:
    case 3: {
        if (vars.empty()) return randomInstr(cfg, vars);   // ensure a var exists

        bool rhs2Var = (rng() & 1);
        bool rhs3Var = (rng() & 1);

        std::string var1 = vars[rng() % vars.size()];      // target is a var
        std::string arg2 = rhs2Var ? vars[rng() % vars.size()]
                                   : std::to_string(Commands::getRandomInt(0,500));
        std::string arg3 = rhs3Var ? vars[rng() % vars.size()]
                                   : std::to_string(Commands::getRandomInt(0,500));

        return Instruction{ code == 2 ? OpCode::ADD : OpCode::SUBTRACT,
                            var1, arg2, arg3, rhs2Var, rhs3Var };
    }

    /* ---------- SLEEP ------------------------------------ */
    case 4: {
        std::string ticks = std::to_string(Commands::getRandomInt(1, 5));
        return Instruction{ OpCode::SLEEP, "", ticks, "", false, true };
    }

    /* ---------- FOR_BEGIN -------------------------------- */
    default: {
        std::string reps = std::to_string(Commands::getRandomInt(2, 5));
    }
    }
}

static Instruction makeLeaf(const Config&,std::vector<std::string>&);

static Instruction makeLeaf(const Config& cfg,std::vector<std::string>& vars)
{
    static std::mt19937 rng{ std::random_device{}() };
    int code = rng()%5;

    if (code==0){                                // PRINT
        if(vars.empty()) return makeLeaf(cfg,vars);
        std::string v = vars[rng()%vars.size()];
        return Instruction{
                OpCode::PRINT,                // opcode
                "\"Value from: \"",           // arg1 – literal prefix
                v,                            // arg2 – variable name
                "",                           // arg3
                false,
                true
        };

    }
    if(code==1){                                 // DECLARE
        std::string v = std::string(1,char('a'+rng()%26));
        vars.push_back(v);
        return { OpCode::DECLARE,v,
                 std::to_string(Commands::getRandomInt(0,65535)) };
    }
    if(code==2||code==3){                        // ADD/SUB
        if(vars.empty()) return makeLeaf(cfg,vars);
        std::string v1 = vars[rng()%vars.size()];
        std::string a2 = std::to_string(Commands::getRandomInt(1,500));
        std::string a3 = std::to_string(Commands::getRandomInt(1,500));
        return { code==2?OpCode::ADD:OpCode::SUBTRACT,v1,a2,a3 };
    }
    return { OpCode::SLEEP,"",std::to_string(Commands::getRandomInt(1,5)) };
}

static std::vector<Instruction>
buildRandomProgram(int len, const Config& cfg, std::vector<std::string>& vars, int depth = 3)
{
    static std::mt19937 rng{ std::random_device{}() };
    std::vector<Instruction> prog;

    while (int(prog.size()) < len) {
        bool mkLoop = depth > 0 && rng() % 4 == 0;
        if (mkLoop) {
            int bodyLen = Commands::getRandomInt(2, 3);
            int reps    = Commands::getRandomInt(2, 5);
            auto body   = buildRandomProgram(bodyLen, cfg, vars, depth - 1);
            prog.emplace_back(OpCode::FOR, std::move(body), "", std::to_string(reps));
        } else {
            prog.push_back(makeLeaf(cfg, vars));
        }
    }
    return prog;
}

static int nextProcessID = 1; // For unique process IDs

int Commands::getRandomInt(int floor, int ceiling) {
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dist(floor, ceiling);
    return dist(rng);
}

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
    else if (command == "scheduler-start") {
        schedulerStartCommand();
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
void Commands::screenCommand(const std::string& cmdLine)
{
    std::istringstream iss(cmdLine);
    std::string token, subCmd, procName;

    iss >> token; // first token is "screen"  (skip it)
    iss >> subCmd; // "-r", "-s", or "-ls"
    if (subCmd != "-ls") // only need a name for -r or -s
        iss >> procName;

    if (subCmd.empty()) {
        std::cout << "ERROR: Missing subcommand. Use -r | -s | -ls\n";
        return;
    }
    if (subCmd != "-ls" && procName.empty()) {
        std::cout << "ERROR: Process name required for " << subCmd << "\n";
        return;
    }

    if      (subCmd == "-r")  rSubCommand(procName);
    else if (subCmd == "-s")  sSubCommand(procName);
    else if (subCmd == "-ls") lsSubCommand();
    else   std::cout << "ERROR: Invalid subcommand. Use -r | -s | -ls\n";
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

void Commands::sSubCommand(const std::string& name)
{
    try {
        ProcessInfo& existing = scheduler->getProcess(name);
        std::cout << "Reattaching to existing process: " << name << '\n';
        enterProcessScreen(existing);
        return;
    }
    catch (const std::runtime_error&) { }

    int lines = Commands::getRandomInt(config.minIns, config.maxIns);

    ProcessInfo proc(nextProcessID++, name, lines, getCurrentTimestamp(), false);

    std::vector<std::string> vars;
    proc.prog = buildRandomProgram(lines, config, vars);

    scheduler->addProcess(std::move(proc));
    std::cout << "Created process \"" << name << "\" (" << lines << " lines)\n";

    enterProcessScreen(scheduler->getProcess(name));
}

void Commands::enterProcessScreen(ProcessInfo& dummyRef)
{
    clearScreen();
    const std::string procName = dummyRef.processName;

    g_attachedPid = dummyRef.processID;

    auto showHeader = [&](){
        ProcessInfo& p = scheduler->getProcess(procName);
        std::cout << "\nProcess: "     << p.processName
                  << "\nID: "          << p.processID
                  << "\nTotal Lines: " << p.totalLine << "\n";
    };

    showHeader();
    displayProcessSmi(scheduler->getProcess(procName));

    bool runningScreen = true;
    while (runningScreen) {
        ProcessInfo& live = scheduler->getProcess(procName);
        if (live.isFinished) {
            std::cout << "\nProcess has finished.\n";
            break;
        }

        std::cout << "> ";
        std::string cmd;
        std::getline(std::cin, cmd);

        if (cmd == "process-smi") {
            auto snap = scheduler->snapshotProcess(procName);
            displayProcessSmi(snap);
        }
        else if (cmd == "exit")        runningScreen = false;
        else if (!cmd.empty())
            std::cout << "Invalid command. Available: process-smi | exit\n";
    }

    g_attachedPid = -1;
    clearScreen();
    menuView();
}

// Scheduler-related commands
void Commands::schedulerStartCommand()
{
    if (!scheduler) { std::cout << "Run 'initialize' first.\n"; return; }
    if (batchRunning) { std::cout << "scheduler-start already active.\n"; return; }

    batchRunning = true;
    batchThread  = std::thread(&Commands::batchLoop, this);
    std::cout << "Continuous dummy-process generation...\n";
}

void Commands::schedulerStopCommand()
{
    if (!batchRunning) { std::cout << "scheduler-start not active.\n"; return; }

    batchRunning = false;
    if (batchThread.joinable()) batchThread.join();
    std::cout << "Stopped dummy-process generation...\n";
}

// Process reporting and display
void Commands::writeProcessReport(std::ostream& os) {
    os << scheduler->utilisationString();
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

void Commands::displayProcessSmi(ProcessInfo& p)
{
    constexpr const char* border =
        "====================  PROCESS SMI  ====================\n";

    std::cout << '\n' << border
              << std::left << std::setw(15) << "Name"          << " : " << p.processName   << '\n'
              << std::setw(15) << "PID"                        << " : " << p.processID     << '\n'
              << std::setw(15) << "Assigned Core"             << " : " << (p.assignedCore == -1 ? "N/A"
                                                                                               : std::to_string(p.assignedCore)) << '\n'
              << std::setw(15) << "Progress"                  << " : " << p.executedLines << " / " << p.totalLine << '\n'
              << std::setw(15) << "Status"                    << " : "
              << (p.isFinished ? "Finished" : (p.assignedCore == -1 ? "Waiting" : "Running")) << '\n'
              << border << std::endl;
}

void Commands::batchLoop()
{
    const int tickMs = config.delaysPerExec;

    while (batchRunning.load()) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(config.batchProcessFreq * tickMs));

        std::string pname = "process" + std::to_string(nextProcessID);

        try { scheduler->getProcess(pname); ++nextProcessID; continue; }
        catch (...) {} // name is unique

        int lines = Commands::getRandomInt(config.minIns, config.maxIns);

        ProcessInfo p(nextProcessID++, pname, lines, getCurrentTimestamp(), false);

        std::vector<std::string> vars;
        p.prog = buildRandomProgram(lines, config, vars);

        scheduler->addProcess(std::move(p));
    }
}
