// Tracker.h

#ifndef TRACKER_H
#define TRACKER_H

#include "ProcessInfo.h"

#include <iostream>
#include <string>
#include <vector>

class Tracker {

	void print_current_intstruction_line(const ProcessInfo& process) {
		std::cout << "Process ID:	" << process.processID << std::endl;
		std::cout << "Process Name: " << process.processName << std::endl;
		std::cout << "Current Line: " << process.currentLine << std::endl;
		std::cout << "Total Lines:	" << process.totalLine << std::endl;
		std::cout << "Instruction:  " << process.processInstructions[process.currentLine] << std::endl;
	}

	void execute_instruction(ProcessInfo& process) {

		std::cout << "Executing:  " << process.processInstructions[process.currentLine] << std::endl;

		// Do some instruction here
		int total = 0;
		for (int i = 0; i < 100; i++) {
			total += i;
		}

		// Return true if finished all instructions
		if (process.currentLine >= process.totalLine) {
			process.isFinished = true;
			return;
		}

		process.currentLine++;
	}
};


#endif