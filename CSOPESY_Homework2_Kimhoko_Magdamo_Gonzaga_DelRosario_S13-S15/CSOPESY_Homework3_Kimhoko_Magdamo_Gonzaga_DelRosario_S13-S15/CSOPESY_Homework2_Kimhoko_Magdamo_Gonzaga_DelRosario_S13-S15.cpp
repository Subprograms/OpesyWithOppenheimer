#include <iostream>
#include <string>
#include <map>
#include <ctime>
#include <iomanip>
#include <sstream>

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
        std::tm *ltm = std::localtime(&now);
        std::stringstream timestampStream;
        timestampStream << std::put_time(ltm, "%m/%d/%Y, %H:%M:%S %p");
        timestamp = timestampStream.str();
    }

    void display() {
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

void showCommands() {
    std::cout << "Available commands:\n"
              << "  marquee      - Does something for marquee\n"
              << "  screen       - Manages screen instances\n"
              << "               - -s <name>: It will start a new screen given a name\n"
              << "               - -r <name>: Will reattach a previous screen\n"
              << "  clear        - Clears the screen\n"
              << "  exit         - Exits the current screen or the program\n"
              << "  ?            - Shows this list of commands\n";
}

void handleScreenCommand(const std::string& command) {
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

int main() {
    std::string command;
    printHeader();

    while (true) {
        std::cout << "Main Menu - Enter command: ";
        std::getline(std::cin, command);
        if (command == "exit") {
            std::cout << "Exiting program." << std::endl;
            break;
        } else if (command.rfind("screen ", 0) == 0) {
            handleScreenCommand(command);
        } else if (command == "?") {
            showCommands();
        } else if (command == "clear") {
            clearScreen();
        } else {
            std::cout << "Unrecognized command." << std::endl;
        }
    }
    return 0;
}


