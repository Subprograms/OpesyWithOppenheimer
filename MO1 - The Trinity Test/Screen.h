#ifndef SCREEN_H
#define SCREEN_H

#include <string>
#include <vector>
#include <functional>
#include "ProcessInfo.h"

class Screen {

public:
    std::vector<std::function<void()>> screen;
    std::string currentScreen;
    std::string previousScreen;

    void initialScreen();

    void menuView();
    void lsScreenView(const ProcessInfo& process);
    void clearScreen() const;
    void screenLoop(const std::string& name, bool isNested);
    void updateScreen(const std::string& newScreen);

public:
    virtual ~Screen() = default;
};

#endif
