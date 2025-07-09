# M01 - Process Multiplexer and CLI

## Members (**Cross-Section S13-S15**)
1. DEL ROSARIO, MICHAEL GABRIEL A.  
2. GONZAGA, EROS ANEELOUV A.  
3. KIMHOKO, JAMUEL ERWIN C.  
4. MAGDAMO, BIEN RAFAEL O.  

## Instructions on How to Run

### Step 1. **Download the files under the repository**  
Open `CSOPESY_Group5_Project` > `CSOPESY_Group5_Project.sln`  

### Step 2. **Compile the project with Visual Studio**  
Ensure the following files are included in the project:  
- `CSOPESY-S16_Group5.cpp`  
- `Commands.h`  
- `Commands.cpp`  
- `Data.h`  
- `Data.cpp`  
- `Screen.h`  
- `Screen.cpp`  
- `Scheduler.h`  
- `Scheduler.cpp`  
- `Instruction.h`  
- `ProcessInfo.h`  
- `Config.h`  
- `config.txt`  

### Step 3. **Configure your Config.txt**  
Adjust the parameters as needed:  
- `num-cpu` – the amount of cores  
- `scheduler` – the scheduling algorithm  
- `quantum-cycles` – the time quantum for Round-Robin  
- `batch-process-freq` – frequency of batch process creation  
- `min-ins` – minimum instructions per process  
- `max-ins` – maximum instructions per process  
- `delays-per-exec` – delay per execution in the CPU  

### Step 4. **Run the project through Visual Studio**  
- Enter path to `config.txt` file  
- Type `scheduler-start` to run continuous process (and instruction) generation  
- Type `scheduler-stop` to stop the aforementioned process generation  
- Type `screen -s <process>` to create new process (or reattach to an existing one)  
- Type `screen -r <process>` to open an existing process  
- Type `process-smi` to see process progress on screen command  
- Type `screen -ls` to see all processes and CPU utilization statistics  
- Type `exit` to return to the main menu from screens  
- Type `report-util` to write the CPU utilization and the queues to a text file  

**Note:** The entry class file where the `main()` function is located is in: `CSOPESY-S16_Group5.cpp`
