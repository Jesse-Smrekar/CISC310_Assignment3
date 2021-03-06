#include "process.h"

// Process class methods
Process::Process(ProcessDetails details, uint32_t current_time)
{
    int i;
    pid = details.pid;
    start_time = details.start_time;
    num_bursts = details.num_bursts;
    current_burst = 0;
	last_update = current_time;
    burst_times = new uint32_t[num_bursts];
    for (i = 0; i < num_bursts; i++)
    {
        burst_times[i] = details.burst_times[i];
    }
    priority = details.priority;
    state = (start_time == 0) ? State::Ready : State::NotStarted;
    if (state == State::Ready)
    {
        launch_time = current_time;
    }
    core = -1;
    turn_time = 0;
    wait_time = 0;
    cpu_time = 0;
    remain_time = 0;
    for (i = 0; i < num_bursts; i+=2)
    {
        remain_time += burst_times[i];
    }
}

Process::~Process()
{
    delete[] burst_times;
}

uint32_t Process::getLastUpdate() const { 
    return last_update;
}

uint16_t Process::getPid() const
{
    return pid;
}

uint32_t Process::getStartTime() const
{
    return start_time;
}

uint8_t Process::getPriority() const
{
    return priority;
}

Process::State Process::getState() const
{
    return state;
}

int8_t Process::getCpuCore() const
{
    return core;
}

double Process::getTurnaroundTime() const
{
    return (double)turn_time / 1000.0;
}

double Process::getWaitTime() const
{
    return (double)wait_time / 1000.0;
}

double Process::getCpuTime() const
{
    return (double)cpu_time / 1000.0;
}

double Process::getRemainingTime() const
{
    return (double)remain_time / 1000.0;
}

void Process::setState(State new_state, uint32_t current_time)
{
    if (state == State::NotStarted && new_state == State::Ready)
    {
        launch_time = current_time;
    }
    last_update = current_time; 
    //printf("pid: %d, last_update: %u\n", pid, last_update ); 
    state = new_state;
}

void Process::setCpuCore(int8_t core_num)
{
    core = core_num;
}

void Process::updateProcess(uint32_t current_time){
    // use `current_time` to update turnaround time, wait time, burst times, 
    // cpu time, and remaining time
    // call this before switching state.
    int32_t elapsed;
    uint32_t new_burst_time;
    elapsed = current_time - last_update; 
    //printf("last_update: %u, currentTime: %u elapsed: %u\n", last_update, current_time, elapsed);
    
		//std::cout << "State= " << State::IO << std::endl;
        if( state == State::Ready ){ 
            //when the process goes from ready to running. 

            wait_time = wait_time + elapsed; 
            turn_time = turn_time + elapsed; 
        }
        else if( state == State::Running ){
            //when the process goes from running to io.
            //i.e. after process has finished executing (or is cut short).
            cpu_time = cpu_time + elapsed;  
            remain_time = remain_time - elapsed;  

            new_burst_time = getCurrentBurstTime() - elapsed;
            updateBurstTime( current_burst, new_burst_time); 
            turn_time = turn_time + elapsed; 
        }
        else if( state ==  State::IO ){
            //when the process goes from io to ready.
            turn_time = turn_time + elapsed; 
       	}
    
    last_update = current_time; 
}



// ------------------- ADDED FUNCTIONS -----------------------------

uint32_t Process::getCurrentBurstTime() const { 
    return burst_times[current_burst];
}

uint16_t Process::getCurrentBurst() const{

	return current_burst;

}

void Process::incrementCurrentBurst(){

	current_burst ++;
}
    

uint32_t* Process::getBurstTimes()const{

	return burst_times;

}


// --------------------------------------------------------------------



void Process::updateBurstTime(int burst_idx, uint32_t new_time)
{
    burst_times[burst_idx] = new_time;
}


// Comparator methods: used in std::list sort() method
// No comparator needed for FCFS or RR (ready queue never sorted)

// SJF - comparator for sorting read queue based on shortest remaining CPU time
bool SjfComparator::operator ()(const Process *p1, const Process *p2)
{
    // your code here!
    return false; // change this!
}

// PP - comparator for sorting read queue based on priority
bool PpComparator::operator ()(const Process *p1, const Process *p2)
{
    // your code here!
    return false; // change this!
}
