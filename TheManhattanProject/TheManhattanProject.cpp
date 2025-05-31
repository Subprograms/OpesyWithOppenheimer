// TheManhattanProject.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <map>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>


#ifdef _WIN32
#include <windows.h>
#endif

void printHeader() {
    std::cout << "*************************************************************************************" << std::endl;
    std::cout << "*    _______    _______     ________  _________   __________   _______  __      __  *" << std::endl;
    std::cout << "*   //======\\  //======\\   //======\\\\ \\=======\\\\  \\=========  //======\\  %\\    //   *" << std::endl;
    std::cout << "*  //*         []          11      11  11      ||  11         []          %\\  //    *" << std::endl;
    std::cout << "*  []*         \\\\_______   11      11  11      ||  11######   \\\\_______    %\\//     *" << std::endl;
    std::cout << "*  &]*          ^######\\\\  #1      11  11######7   11          ^######\\\\    11      *" << std::endl;
    std::cout << "*  %&$                 //  #&      11  #1          11_______          //    #1      *" << std::endl;
    std::cout << "*   %&$####%7  \\%######7   %&######7/  ##         /#########  \\%######7    /##      *" << std::endl;
    std::cout << "*___________________________________________________________________________________*" << std::endl;
    std::cout << "*************************************************************************************" << std::endl;
    std::cout << "" << std::endl;
}

void clearScreen(bool shouldPrintHeader = true) {
#ifdef _WIN32
    std::system("cls");
#else
    std::system("clear");
#endif
    if (shouldPrintHeader) {
        printHeader();
    }
}

class Console {
public:
    std::string processName;
    int currentLine;
    int totalLine;
    std::string timestamp;

    Console() : processName(""), currentLine(0), totalLine(0) {
        updateTimestamp();
    }

    Console(const std::string& name, int totalLines)
        : processName(name), currentLine(0), totalLine(totalLines) {
        updateTimestamp();
    }

    void updateTimestamp() {
        std::time_t now = std::time(nullptr);
        std::tm ltm;
        localtime_s(&ltm, &now);
        std::stringstream timestampStream;
        timestampStream << std::put_time(&ltm, "%m/%d/%Y, %H:%M:%S %p");
        timestamp = timestampStream.str();
    }

    void display() const {
        std::cout << "Process name: " << processName << std::endl;
        std::cout << "Current line of instruction: " << currentLine << "/" << totalLine << std::endl;
        std::cout << "Timestamp: " << timestamp << std::endl;
    }

    void run() {
        std::string command;
        bool active = true;
        display();
        while (active) {
            std::cout << "Screen " << processName << " - Enter command ('exit' to return): ";
            std::getline(std::cin, command);
            if (command == "exit") {
                active = false;
                clearScreen();
            } else {
                processCommand(command);
            }
        }
    }

    void processCommand(const std::string& command) {
        if (command == "update") {
            currentLine++;
            updateTimestamp();
            std::cout << "Updated line and timestamp." << std::endl;
        }
        display();
    }
};

std::map<std::string, Console> consoles;

static void showCommands() {
    std::cout << "Available commands:\n"
              << "  marquee      - Does something for marquee\n"
              << "  screen       - Manages screen instances\n"
              << "               - -s <name>: It will start a new screen given a name\n"
              << "               - -r <name>: Will reattach a previous screen\n"
              << "  clear        - Clears the screen\n"
              << "  exit         - Exits the current screen or the program\n"
              << "  ?            - Shows this list of commands\n";
}

static void handleScreenCommand(const std::string& command) {
    std::string subCommand, name;
    std::istringstream iss(command);
    iss >> subCommand >> subCommand >> name;

    if (subCommand == "-s") {
        if (consoles.find(name) == consoles.end()) {
            Console console(name, 100);
            consoles[name] = console;
            clearScreen(false);
            consoles[name].run();
        } else {
            std::cout << "A screen with the name '" << name << "' already exists." << std::endl;
        }
    } else if (subCommand == "-r" && consoles.find(name) != consoles.end()) {
        clearScreen(false);
        consoles[name].run();
    } else {
        std::cout << "Invalid screen command or screen does not exist." << std::endl;
    }
}

static std::string getTimestamp() {
    time_t now = time(0);
    tm localtime;

    std::stringstream ss;

    localtime_s(&localtime, &now);

    // Numeric month value should be mapped to their corresponding day of the week
    const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                             "Aug", "Sept", "Oct", "Nov", "Dec" };

    const char* daysofweek[] = { "Sun", "Mon", "Tues", "Wed", "Thurs",
                                "Fri", "Sat" };

    // Print the date and time
    ss << daysofweek[localtime.tm_wday] << " "             // Day of the week
        << months[localtime.tm_mon] << " "                 // Month
        << localtime.tm_mday << " "                        // Day of the month
        << (localtime.tm_year + 1900) << " "               // Year
        << localtime.tm_hour << ":"                        // Hour
        << localtime.tm_min << ":"                         // Minute
        << localtime.tm_sec;                               // Second                                      

    return ss.str();
}

struct GPUProcessInfo {
    int gpu;           // GPU index         (e.g., 0)
    std::string gi;    // GPU Instance ID   (e.g., "N/A")
    std::string ci;    // GPU Compute ID    (e.g., "N/A")
    int pid;           // process ID        (e.g., 1368)
    std::string type;  // process type      (e.g., "C+G")
    std::string name;  // full path+exe name
    std::string mem;   // GPU Memory usage  (e.g., "N/A")
};

static void nvidiaSMIView() {

    std::stringstream ss;
    std::string name;
    int maxlen = 38;
    
    ss << "" << std::endl;
    ss << getTimestamp() << std::endl;
    ss << "+-----------------------------------------------------------------------------------------+" << std::endl;
    ss << "| NVIDIA-SMI 551.86                 Driver Version: 551.86         CUDA Version: 12.4     |" << std::endl;
    ss << "+-----------------------------------------+------------------------+----------------------+" << std::endl;
    ss << "| GPU  Name                      TCC/WDDM | Bus-Id          Disp.A | Volatile Uncorr. ECC |" << std::endl;
    ss << "| Fan  Temp  Perf           Pwr:Usage/Cap |           Memory-Usage | GPU-Util  Compute M. |" << std::endl;
    ss << "|                                         |                        |               MIG M. |" << std::endl;
    ss << "|=========================================+========================+======================|" << std::endl;
    ss << "|   0  NVIDIA GeForce GTX 1080      WDDM  |   00000000:26:00.0  On |                  N/A |" << std::endl;
    ss << "| 28%   37C    P8             11W /  180W |     701MiB /   8192MiB |      0%      Default |" << std::endl;
    ss << "|                                         |                        |                  N/A |" << std::endl;
    ss << "+-----------------------------------------+------------------------+----------------------+" << std::endl;
    ss << "" << std::endl;
    ss << "+-----------------------------------------------------------------------------------------+" << std::endl;
    ss << "| Processes:                                                                              |" << std::endl;
    ss << "|  GPU   GI   CI        PID   Type   Process name                              GPU Memory |" << std::endl;
    ss << "|        ID   ID                                                               Usage      |" << std::endl;
    ss << "|=========================================================================================|" << std::endl;
    
    std::vector<GPUProcessInfo> processes = {
        { 0, "N/A", "N/A", 1368, "C+G", R"(C:\Users\user\Videos\Oppenheimer (2023).mp4)", "N/A" },
        { 0, "N/A", "N/A", 5001, "C+G", R"(C:\Users\Nitro 5\Documents\web)", "N/A" },
        { 0, "N/A", "N/A", 8243, "C+G", R"(C:\Users\user\Downloads\MarvelRivals.exe)",  "N/A" },
        { 0, "N/A", "N/A", 7241, "C+G", R"(C:\Users\user\Downloads\password.txt)", "N/A" },
        { 0, "N/A", "N/A", 8043, "C+G", R"(C:\Users\Nitro 5\Downloads\CSOPESY 2024\reallylongfilename.txt.)", "N/A" }
    };

    for (const auto& p : processes) {
        name = (p.name.length() > maxlen) ? "..." + p.name.substr(p.name.length() - (maxlen - 3)) : p.name;

        ss  << "|"
            << "  "
            << std::setw(3) << p.gpu
            << std::string(3, ' ')
            << std::setw(3) << p.gi
            << std::string(3, ' ')
            << std::setw(3) << p.ci
            << std::string(5, ' ')
            << std::setw(4) << p.pid
            << std::string(4, ' ')
            << std::setw(3) << p.type
            << std::string(3, ' ')
            << std::left << std::setw(49) << name
            << std::right << std::setw(3) << p.mem
            << " "
            << "|"
            << std::endl;
    }

    ss << "|                                                                                         |" << std::endl;
    ss << "+-----------------------------------------------------------------------------------------+" << std::endl;

    std::cout << ss.str() << std::endl;
}

int main() {
    std::string command;
    printHeader();

    while (true) {
        std::cout << "Main Menu - Enter command: ";

        if (!std::getline(std::cin, command)) {
            std::cout << "\nInput error or EOF detected. Exiting program." << std::endl;
            break;
        }

        if (command == "exit") {
            std::cout << "Exiting program." << std::endl;
            break;
        } else if (command.rfind("screen ", 0) == 0) {
            handleScreenCommand(command);
        } else if (command == "?") {
            showCommands();
        } else if (command == "clear") {
            clearScreen();
        } else if (command == "nvidia-smi") {
            nvidiaSMIView();
        } else {
            std::cout << "Unrecognized command." << std::endl;
        }
    }
    return 0;
}
