#ifndef CONFIG_H
#define CONFIG_H

#include <string>

struct Config {
    int numCpu;
    std::string scheduler;
    int quantumCycles;
    int batchProcessFreq;
    int minIns;
    int maxIns;
    int delaysPerExec;
};

#endif
