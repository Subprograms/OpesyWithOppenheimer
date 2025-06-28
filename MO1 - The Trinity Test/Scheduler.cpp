void Scheduler::coreFunction(int nCoreId)
{
    auto strip = [](const std::string& s)->std::string{
        return (s.size()>=2 && s.front()=='"' && s.back()=='"')
               ? s.substr(1,s.size()-2):s;
    };
    auto varVal = [](const ProcessInfo& p,const std::string& v)->std::string{
        auto it=p.vars.find(v);
        return (it!=p.vars.end())?std::to_string(it->second):"0";
    };
    auto log=[&](ProcessInfo& p,int c,const std::string& t,int ind){
        p.outBuf.emplace_back("Core:"+std::to_string(c)+" ["+
                              std::to_string(p.executedLines)+"] "+
                              std::string(ind*4,' ')+t);
    };

    while(running)
    {
        ProcessInfo proc(-1,"",0,"");
        {
            std::unique_lock<std::mutex> lk(queueMutex);
            cv.wait(lk,[&]{return !processQueue.empty()||!running;});
            if(!running&&processQueue.empty()) return;
            proc=std::move(processQueue.front());
            processQueue.pop_front();
            proc.assignedCore=nCoreId;
            runningProcesses.push_back(proc);
            ++coresInUse;
        }

        const bool fcfs=(schedulerType=="fcfs"||schedulerType=="FCFS");
        const int  slice=fcfs?std::numeric_limits<int>::max():std::max(1,quantum);

        auto& loopStack=proc.loopStack;

        for(int used=0;used<slice && running;)
        {
            if(proc.sleepTicks){
                --proc.sleepTicks;
                ++proc.executedLines;
                ++used;
                if(!proc.sleepTicks) ++proc.currentLine;
                continue;
            }

            if(proc.currentLine>=static_cast<int>(proc.prog.size()))
                break;                                   // program finished?

            Instruction& ins=proc.prog[proc.currentLine];
            int indent=static_cast<int>(loopStack.size());

            switch(ins.op)
            {
                case OpCode::PRINT:{
                    std::string msg=ins.arg2.empty()
                                   ? strip(ins.arg1)
                                   : strip(ins.arg1)+'+'+ins.arg2+": "+varVal(proc,ins.arg2);
                    if(g_attachedPid.load()==proc.processID){
                        std::lock_guard<std::mutex> _(g_coutMx);
                        std::cout<<'\r'<<msg<<std::flush;
                    }
                    log(proc,nCoreId,"PRINT -> "+msg,indent);
                    break;
                }

                case OpCode::DECLARE:
                    proc.vars[ins.arg1]=Stoi16(ins.arg2);
                    log(proc,nCoreId,"DECLARE "+ins.arg1+'='+ins.arg2,indent);
                    break;

                case OpCode::ADD:
                case OpCode::SUBTRACT:{
                    uint16_t v2=ins.isArg2Var?proc.vars[ins.arg2]:Stoi16(ins.arg2);
                    uint16_t v3=ins.isArg3Var?proc.vars[ins.arg3]:Stoi16(ins.arg3);
                    uint32_t r=(ins.op==OpCode::ADD)?v2+v3:(v2>=v3?v2-v3:0u);
                    proc.vars[ins.arg1]=static_cast<uint16_t>(std::min(r,65535u));
                    log(proc,nCoreId,(ins.op==OpCode::ADD?"ADD ":"SUB ")+ins.arg1,indent);
                    break;
                }

                case OpCode::SLEEP:{
                    uint16_t t=Stoi16(ins.arg2);
                    proc.sleepTicks=t;
                    log(proc,nCoreId,"SLEEP "+ins.arg2,indent);
                    ++proc.executedLines;
                    ++used;
                    if(!proc.sleepTicks) ++proc.currentLine;
                    continue;
                }

                case OpCode::FOR:{
                    if(ins.body.empty()||ins.repetitions==0) break;
                    uint16_t reps=ins.repetitions;
                    int bodyCnt=static_cast<int>(ins.body.size());
                    int insertAt=proc.currentLine+1;

                    proc.prog.insert(proc.prog.begin()+insertAt,
                                     ins.body.begin(),ins.body.end());
                    proc.totalLine=static_cast<int>(proc.prog.size());

                    for(auto& f:loopStack) if(f.end>=insertAt) f.end+=bodyCnt;

                    loopStack.push_back({insertAt,
                                         insertAt+bodyCnt-1,
                                         static_cast<uint16_t>(reps-1),
                                         indent});

                    log(proc,nCoreId,"FOR ×"+std::to_string(reps)+
                                     " (body "+std::to_string(bodyCnt)+')',indent);
                    break;
                }

                default: break;
            }

            ++proc.currentLine;
            ++proc.executedLines;
            ++used;

            // for loop accounting in here
            if(!loopStack.empty()){
                auto& top=loopStack.back();
                if(proc.currentLine>top.end){
                    if(top.remain){
                        --top.remain;
                        proc.currentLine=top.start;
                    }else{
                        loopStack.pop_back();
                    }
                }
            }
        }

        if(config.delaysPerExec)
            std::this_thread::sleep_for(std::chrono::milliseconds(config.delaysPerExec));

        bool finished=(proc.currentLine>=proc.totalLine)&&proc.sleepTicks==0;

        {
            std::lock_guard<std::mutex> lk(queueMutex);
            runningProcesses.erase(
                std::remove_if(runningProcesses.begin(),runningProcesses.end(),
                               [&](const ProcessInfo& p){return p.processID==proc.processID;}),
                runningProcesses.end());
            --coresInUse;

            if(!proc.outBuf.empty()){
                std::ofstream f(proc.processName+".txt",std::ios::app);
                for(auto& s:proc.outBuf) f<<s<<'\n';
                proc.outBuf.clear();
            }

            if(finished)
                finishedProcesses.emplace_back(proc,nCoreId);
            else
                processQueue.emplace_back(std::move(proc));
        }
        cv.notify_all();
    }
}
