// MAIN
#include <iostream>
#include <string>
#include "Commands.h"
#include "Screen.h"

int main() {
    Commands commands;
    Screen screen;

    std::string command;
   
    screen.clearScreen();
    screen.menuView();

    std::string configFile = "Config.txt";

    commands.initialize(configFile);

    // Main loop to continuously take in commands, until exit.
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, command);
        commands.processCommand(command);
    }

    return 0;
}
