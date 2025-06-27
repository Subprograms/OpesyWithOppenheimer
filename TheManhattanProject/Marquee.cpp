// MODEL - MARQUEE

#include "Marquee.h"

// Method for starting marquee thread
void Marquee::startThread() {
    std::thread marqueeThread(&Marquee::marqueeLoop, this, true);
    std::thread inputThread(&Marquee::editMarquee, this, true);

    marqueeThread.join();
    inputThread.join();
}

// Method for starting a non threaded marquee
void Marquee::startNonThread() {
    while (run) {
        marqueeLoop(false);
        editMarquee(false);
    }
}

// Method for looping moving and writing marquee
void Marquee::marqueeLoop(bool loop) {
    while (run) {
        moveMarquee();
        writeMarquee();
        std::this_thread::sleep_for(std::chrono::milliseconds(refreshRate));
        if (!loop) { break; }
    }
}

// Method for writing marquee on screen
void Marquee::writeMarquee() {
    std::lock_guard<std::mutex> lock(textMutex);

    std::ostringstream buffer;
    clearScreen();

    buffer << martitle << std::endl;
    for (int y = 0; y < height + 2; y++) {
        for (int x = 0; x < width + 2; x++) {
            if ((y == 0 && x == 0) || (y == 0 && x == width + 1) || (y == height + 1 && x == 0) || (y == height + 1 && x == width + 1)) {
                buffer << "+";
            }
            else if (y == 0 || y == height + 1) {
                buffer << "-";
            }
            else if (x == 0 || x == width + 1) {
                buffer << "|";
            }
            else if (x == Xpos && y == Ypos) {
                x += (int)(chatHistory.back().length()) - 1;
                buffer << chatHistory.back();
            }
            else { buffer << " "; }
        }
        buffer << std::endl;
    }

    for (size_t i = 1; i < chatHistory.size(); ++i) {
        buffer << "\nEntered marquee text: " << chatHistory[i];
    }

    buffer << "\nEnter new marquee text (type 'exit' to stop): " << pollinput;

    std::cout << buffer.str();
}

// Method for moving marquee position
void Marquee::moveMarquee() {

    // 0 is leftward direction
    if (Xpos == 1 || Xpos == width - ((int)(chatHistory.back().length()) - 1)) {
        curX = (prevX == RIGHT) ? LEFT : RIGHT;
    }

    // 0 is upward direction
    if (Ypos == 1 || Ypos == height) {
        curY = (prevY == DOWN) ? UP : DOWN;
    }

    Xpos = (curX == LEFT) ? Xpos - 1 : Xpos + 1;
    Ypos = (curY == UP) ? Ypos - 1 : Ypos + 1;

    prevX = curX;
    prevY = curY;
}

// Method for changing marquee text
void Marquee::editMarquee(bool loop) {
    char key;
    while (run) {
        key = _getch();

        std::lock_guard<std::mutex> lock(textMutex);

        switch (key) {
        case '\b':  // Backspace Key
            if (!pollinput.empty()) { pollinput.pop_back(); }
            break;
        case ' ':   // Space Key
            pollinput += ' ';
            break;
        case '\r':  // Enter Key
            if (pollinput == "exit") { run = false; }
            else { chatHistory.push_back(pollinput); }
            pollinput = "";
            break;
        default:
            pollinput += key;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(pollRate));
        if (!loop) { break; }
    }
}

// Method to get string height
size_t Marquee::getStringHeight(const std::string& str) {

    size_t lines = 1;

    for (char ch : str) {
        if (ch == '\n') {
            ++lines;
        }
    }

    return lines;
}

static void clearScreen() {
#ifdef _WIN32
    system("CLS");
#else
    system("clear");
#endif
}