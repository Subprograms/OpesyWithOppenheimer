# M01 - Process Multiplexer and CLI

## Members (**Cross-Section S13-S15**)
1. DEL ROSARIO, MICHAEL GABRIEL A.
2. GONZAGA, EROS ANEELOUV A.
3. KIMHOKO, JAMUEL ERWIN C.
4. MAGDAMO, BIEN RAFAEL O.

## Instructions on How to Run
Step 1. **Download the files under the repository**
        Open "CSOPESY_Group5_Project" > "CSOPESY_Group5_Project.sln"

Step 2. **Complie the project with Visual Studio**
        Ensure the following files are included in the project:
            - CSOPESY-S16_Group5.cpp
	        - Commands.h
            - Commands.cpp
	        - Data.h
            - Data.cpp
	        - Screen.h
            - Screen.cpp
	        - Scheduler.h
            - Scheduler.cpp
          - Instruction.h
	        - ProcessInfo.h
	        - Config.h
	        - config.txt

Step 3. **Configure your Config.txt**
        3.1. Adjust the parameters as needed:
             num-cpu is the amount of cores
             scheduler is the scheduling algorithm
             quantum-cycles is the time quantum for Round-Robin
             batch-process-freq is the frequency of batch process creation
             min-ins is the minimum instructions per process
             max-ins is the maximum instructions per process
             delays-per-exec is the delay per execution in the CPU

Step 4. **Run the project through Visual Studio**
        4.1.    Enter path to config.txt file
        4.2.	Type "scheduler-start" to run continuous process (and instruction) generation
        4.3   Type "scheduler-stop" to stop the aforementioned process generation
        4.4.	Type "screen -s <process>" to create new process (or as a fallback for existing processes, to reattach such)
        4.5. 	Type "screen -r <process>" to open existing process
        4.6. 	Type "process-smi" to see process progress on screen command
        4.7. 	Type "screen -ls" to see all processes and CPU utilization statistics
        4.8. 	Type "exit" to return to main menu from screens.
        4.9.	Type "report-util" to write the CPU utilization and the queues to a text file

**Note:** The entry class file where the main function is located is in: `CSOPESY-S16_Group5.cpp`
