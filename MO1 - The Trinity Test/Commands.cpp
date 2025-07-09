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
#include <algorithm>
std::atomic<int> g_attachedPid{-1};

static std::string randVar() {
    static std::mt19937 rng{ std::random_device{}() };
    return std::string(1, static_cast<char>('a' + (rng() % 26)));
}

static Instruction makeLeafInstr(std::vector<std::string>& vars)
{
    static std::mt19937 rng{ std::random_device{}() };
    int code = rng() % 5;

    /* ---------- PRINT ---------- */
    if (code == 0) {
        /* 50 % chance to emit a blank PRINT */
        if (rng() & 1)
            return Instruction(OpCode::PRINT, "\"\"", "", "", false, false);

        if (vars.empty()) return makeLeafInstr(vars);
        std::string v = vars[rng() % vars.size()];
        return Instruction(OpCode::PRINT, "\"Value from: \"", v, "", false, true);
    }

    /* ---------- DECLARE ---------- */
    if (code == 1) {
        std::string v = randVar(); vars.push_back(v);
        return Instruction(OpCode::DECLARE, v,
                           std::to_string(Commands::getRandomInt(0, 65535)));
    }

    /* ---------- ADD / SUB ---------- */
    if (code == 2 || code == 3) {
        if (vars.empty()) return makeLeafInstr(vars);
        std::string v1 = vars[rng() % vars.size()];
        std::string a2 = std::to_string(Commands::getRandomInt(1, 500));
        std::string a3 = std::to_string(Commands::getRandomInt(1, 500));
        return Instruction(code == 2 ? OpCode::ADD : OpCode::SUBTRACT,
                           v1, a2, a3);
    }

    /* ---------- SLEEP ---------- */
    return Instruction(OpCode::SLEEP, "",
                       std::to_string(Commands::getRandomInt(1, 5)));
}

static std::vector<Instruction>
buildRandomProgram(int                       maxLogical,
                   const Config&             cfg,
                   std::vector<std::string>& vars,
                   int                       depth      = 3,
                   int                       fixedBody  = 3)
{
    static std::mt19937 rng{ std::random_device{}() };
    std::vector<Instruction> prog;

    auto used = [&]()->std::size_t { return logicalSize(prog); };

    while (used() < static_cast<std::size_t>(maxLogical))
    {
        bool makeLoop = depth > 0 && (rng() % 4 == 0);
        Instruction next;
        std::size_t cost = 1;

        if (makeLoop)
        {
            uint8_t reps = static_cast<uint8_t>(Commands::getRandomInt(2, 3));
            auto body    = buildRandomProgram(fixedBody, cfg, vars,
                                              depth - 1, fixedBody);
            std::size_t bodyCost = logicalSize(body);
            cost = 1 + bodyCost * reps;

            if (used() + cost <= static_cast<std::size_t>(maxLogical))
                next = Instruction(std::move(body), reps);
            else
                makeLoop = false;
        }

        if (!makeLoop)
        {
            next = makeLeafInstr(vars);
            cost = 1;
            if (used() + cost > static_cast<std::size_t>(maxLogical)) break;
        }

        prog.emplace_back(std::move(next));
    }
    return prog;
}

static Instruction randomInstr(const Config& cfg,
                               std::vector<std::string>& vars)
{
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> pickOp(0, 5);
    int code = pickOp(rng);

    switch (code)
    {
        /* ---------- PRINT ---------- */
        case 0: {
            if ((rng() & 3) == 0) {
                return Instruction{OpCode::PRINT, "\"\"", "", "", false, false};
            }
            if (vars.empty()) {
                std::string v = randVar();
                vars.push_back(v);
                return Instruction{
                    OpCode::DECLARE,
                    v,
                    std::to_string(Commands::getRandomInt(0, 65535)),
                    "",
                    false,
                    true
                };
            }
            {
                std::string v = vars[rng() % vars.size()];
                return Instruction{
                    OpCode::PRINT,
                    "\"Value from: \"",
                    v,
                    "",
                    false,
                    true
                };
            }
        }

        /* ---------- DECLARE ---------- */
        case 1: {
            std::string v = randVar();
            vars.push_back(v);
            return Instruction{
                OpCode::DECLARE,
                v,
                std::to_string(Commands::getRandomInt(0, 65535)),
                "",
                false,
                true
            };
        }

        /* ---------- ADD / SUBTRACT ---------- */
        case 2:
        case 3: {
            if (vars.empty()) {
                // fall back to something simpler
                return randomInstr(cfg, vars);
            }

            bool rhs2Var = (rng() & 1);
            bool rhs3Var = (rng() & 1);
            std::string target = vars[rng() % vars.size()];
            std::string arg2 = rhs2Var
                ? vars[rng() % vars.size()]
                : std::to_string(Commands::getRandomInt(0, 500));
            std::string arg3 = rhs3Var
                ? vars[rng() % vars.size()]
                : std::to_string(Commands::getRandomInt(0, 500));

            return Instruction{
                code == 2 ? OpCode::ADD : OpCode::SUBTRACT,
                target,
                arg2,
                arg3,
                rhs2Var,
                rhs3Var
            };
        }

        /* ---------- SLEEP ---------- */
        case 4: {
            return Instruction{
                OpCode::SLEEP,
                "",
                std::to_string(Commands::getRandomInt(1, 5)),
                "",
                false,
                true
            };
        }

        /* ---------- FOR (loop) ---------- */
        default: {
            // build a tiny one-instruction body
            uint8_t reps = static_cast<uint8_t>(
                Commands::getRandomInt(1, 3));
            std::vector<Instruction> body;
            body.push_back(makeLeafInstr(vars));
            return Instruction(std::move(body), reps);
        }
    }
}

static int nextProcessID = 1; // For unique process IDs

int Commands::getRandomInt(int floor, int ceiling) {
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dist(floor, ceiling);
    return dist(rng);
}

std::string Commands::getCurrentTimestamp() {
    using namespace std::chrono;

    // current time
    auto tp    = system_clock::now();
    auto ms    = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;
    std::time_t tt = system_clock::to_time_t(tp);
    std::tm    tm;

#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%m/%d/%Y %I:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count()
        << (tm.tm_hour < 12 ? "AM" : "PM");

    return oss.str();
}

// Constructor
Commands::Commands() : scheduler(nullptr) {}

void Commands::initialize(std::string filename) {
    if (scheduler == nullptr) {
        //bool fileLoaded = false;

        while (true) {
            /*std::cout << "Please enter the path to the config file (e.g., config.txt): ";
            std::getline(std::cin, filename);*/

            std::ifstream file(filename);
            if (!file.is_open()) {
                std::cerr << "Error: Could not open the specified file. Please enter a valid path."
                          << std::endl;
                break;
            }
            file.close();

            try {
                config = parseConfigFile(filename);
                scheduler = std::make_unique<Scheduler>(config);
                std::cout << "Scheduler initialized with "
                          << config.numCpu << " CPUs." << std::endl;
                break;
            }
            catch (const std::exception& e) {
                std::cerr << "Error parsing config file: "
                          << e.what() << std::endl;
                std::exit(0);
            }
        }
    }
}

Config Commands::parseConfigFile(const std::string& filename) {
    Config cfg{};  // Initialize

    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Could not open config file");

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        if (!(iss >> key))
            continue;

        if      (key == "num-cpu")                iss >> cfg.numCpu;
        else if (key == "scheduler")               iss >> cfg.scheduler;
        else if (key == "quantum-cycles")          iss >> cfg.quantumCycles;
        else if (key == "batch-process-freq")      iss >> cfg.batchProcessFreq;
        else if (key == "min-ins")                 iss >> cfg.minIns;
        else if (key == "max-ins")                 iss >> cfg.maxIns;
        else if (key == "delays-per-exec")         iss >> cfg.delaysPerExec;
        else if (key == "max-overall-mem"  ||
                 key == "maxOverallMem")          iss >> cfg.maxOverallMem;
        else if (key == "mem-per-frame"     ||
                 key == "memPerFrame")           iss >> cfg.memPerFrame;
        else if (key == "mem-per-proc"      ||
                 key == "memPerProc")            iss >> cfg.memPerProc;
    }

    cfg.delaysPerExec++;
    return cfg;
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
    proc.totalLine = static_cast<int>(logicalSize(proc.prog));

    scheduler->addProcess(std::move(proc));
    std::cout << "Created process \"" << name << "\" (" << lines << " lines)\n";

    enterProcessScreen(scheduler->getProcess(name));
}

void Commands::enterProcessScreen(ProcessInfo& proc)
{
    const std::string name = proc.processName;

    g_attachedPid = proc.processID; // attach

    {
        ProcessInfo snap = scheduler->snapshotProcess(name);
        clearScreen();
        std::cout << "Process: "     << snap.processName << '\n'
                  << "ID: "          << snap.processID   << '\n'
                  << "Total Lines: " << snap.totalLine   << '\n';
        displayProcessSmi(snap);
    }

    while (true)
    {
        std::cout << "\n> ";
        std::string cmd;
        std::getline(std::cin, cmd);

        if (cmd == "process-smi")
        {
            ProcessInfo snap = scheduler->snapshotProcess(name);
            displayProcessSmi(snap);

            if (snap.isFinished)
            {
                std::cout << "\nProcess has finished.\n"
                          << "Press Enter to return to menu...";
                std::string _;
                std::getline(std::cin, _);
                break;
            }
        }
        else if (cmd == "exit")
        {
            break;
        }
        else if (!cmd.empty())
        {
            std::cout << "Invalid command. Available: process-smi | exit\n";
        }
    }

    g_attachedPid = -1; // dettach
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

void Commands::displayProcessSmi(ProcessInfo& process)
{
    ProcessInfo cur = scheduler->snapshotProcess(process.processName);

    int coreId = -1;
    std::string status;

    const auto& finished = scheduler->getFinishedProcesses();
    auto fit = std::find_if(finished.begin(), finished.end(),
                            [&](const auto& e){
                                return e.first.processID == cur.processID;
                            });
    if (fit != finished.end()) {
        status = "Finished";
        coreId = fit->second;
        cur.executedLines = cur.totalLine;
    }

    if (status.empty()) {
        for (const auto& p : scheduler->getRunningProcesses()) {
            if (p.processID == cur.processID) {
                status = "Running";
                coreId = p.assignedCore;
                break;
            }
        }
    }

    if (status.empty()) status = "Waiting";

    const int shown = std::min(cur.executedLines, cur.totalLine);

    constexpr const char* border =
        "====================  PROCESS SMI  ====================\n";

    std::stringstream out;
    out << '\n' << border << '\n'
        << std::left << std::setw(15) << "Name"          << " : " << cur.processName  << '\n'
        << std::setw(15)               << "PID"           << " : " << cur.processID    << '\n'
        << std::setw(15)               << "Assigned Core" << " : "
        << (coreId == -1 ? "N/A" : std::to_string(coreId))             << '\n'
        << std::setw(15)               << "Progress"      << " : "
        << shown << " / " << cur.totalLine                               << '\n'
        << std::setw(15)               << "Status"        << " : " << status << '\n'
        << border << '\n';

    std::cout << out.str();
}

void Commands::batchLoop()
{
    const int tickMultiplier = config.delaysPerExec * 10;

    while (batchRunning.load()) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(config.batchProcessFreq * tickMultiplier)
        );

        std::string pname = "process" + std::to_string(nextProcessID);

        bool exists = false;

        for (auto const& p : scheduler->getWaitingProcesses()) {
            if (p.processName == pname) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            for (auto const& p : scheduler->getRunningProcesses()) {
                if (p.processName == pname) {
                    exists = true;
                    break;
                }
            }
        }
        if (!exists) {
            for (auto const& e : scheduler->getFinishedProcesses()) {
                if (e.first.processName == pname) {
                    exists = true;
                    break;
                }
            }
        }

        if (exists) {
            ++nextProcessID;
            continue;
        }

        int lines = Commands::getRandomInt(config.minIns, config.maxIns);
        ProcessInfo p(
            nextProcessID++,
            pname,
            lines,
            getCurrentTimestamp(),
            false
        );

        std::vector<std::string> vars;
        p.prog      = buildRandomProgram(lines, config, vars);
        p.totalLine = static_cast<int>(logicalSize(p.prog));

        scheduler->addProcess(std::move(p));
    }
}