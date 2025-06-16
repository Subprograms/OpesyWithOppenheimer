// MAIN
#include <iostream>
#include <string>
#include "Commands.h"
#include "Screen.h"

int main() {
    Commands commands;
    std::string command;
    Screen screen;
    screen.clearScreen();
    screen.menuView();
    commands.initialize();

    // Main loop to continuously take in commands, until exit.
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, command);
        if (command == "exit") break;
        commands.processCommand(command);
    }

    return 0;
}
