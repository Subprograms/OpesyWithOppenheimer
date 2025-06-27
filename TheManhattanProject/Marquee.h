#ifndef MARQUEE_H
#define MARQUEE_H

#include <chrono>
#include <conio.h>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>


// Reference used: https://stackoverflow.com/questions/61514117/i-made-a-moving-screensaver-in-c-console-but-there-is-a-bug-when-it-hits-the-c

class Marquee {

private:

    std::mutex textMutex;
    std::string pollinput = "";

    std::vector<std::string> chatHistory = { "*Marquee*" };

    bool run;
    int refreshRate;
    int pollRate;

    int width = 100;
    int height = 20;

    int Xpos = rand() % width + 1;
    int Ypos = rand() % height + 1;

    enum Horizontal { LEFT, RIGHT } curX = LEFT;
    enum Vertical { UP, DOWN } curY = UP;

    int prevX = 0;
    int prevY = 0;

    const std::string martitle = R"(
                            ********************************************
                            *              Marquee Screen              *
                            ********************************************
    )";

public:

    Marquee(int refresh, int poll, bool start) : refreshRate(refresh), pollRate(poll), run(start) {}

    // Method for starting marquee thread
    void startThread();

    // Method for starting a non threaded marquee
    void startNonThread();

    // Method for looping moving and writing marquee
    void marqueeLoop(bool loop);

    // Method for writing marquee on screen 
    void writeMarquee();

    // Method for moving marquee position
    void moveMarquee();

    // Method for changing marquee text
    void editMarquee(bool loop);

    // Method to get string height
    size_t getStringHeight(const std::string& str);
};

#endif