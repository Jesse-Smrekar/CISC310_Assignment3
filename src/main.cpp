#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include "configreader.h"
#include "process.h"

// Shared data for all cores
typedef struct SchedulerData {
    std::mutex mutex;
    std::condition_variable condition;
    ScheduleAlgorithm algorithm;
    uint32_t context_switch;
    uint32_t time_slice;
    std::list<Process*> ready_queue;
    bool all_terminated;
} SchedulerData;

// -- Function declarations --
void coreRunProcesses(uint8_t core_id, SchedulerData *data);
int printProcessOutput(std::vector<Process*>& processes, std::mutex& mutex, SchedulerData* shared_data );
void clearOutput(int num_lines);
uint32_t currentTime();
std::string processStateToString(Process::State state);
void sort( std::list<Process*>& sdata, ScheduleAlgorithm& algo );
// ---------------------------


int main(int argc, char **argv)
{
    // ensure user entered a command line parameter for configuration file name
    if (argc < 2)
    {
        std::cerr << "Error: must specify configuration file" << std::endl;
        exit(1);
    }

    // declare variables used throughout main
    int i;
    SchedulerData *shared_data;
    std::vector<Process*> processes;

    // read configuration file for scheduling simulation
    SchedulerConfig *config = readConfigFile(argv[1]);

    // store configuration parameters in shared data object
    uint8_t num_cores = config->cores;
    shared_data = new SchedulerData();
    shared_data->algorithm = config->algorithm;
    shared_data->context_switch = config->context_switch;
    shared_data->time_slice = config->time_slice;
    shared_data->all_terminated = false;

    // create processes
    uint32_t start = currentTime();
    for (i = 0; i < config->num_processes; i++)
    {
        Process *p = new Process(config->processes[i], start);
        processes.push_back(p);
        if (p->getState() == Process::State::Ready)
        {
            shared_data->ready_queue.push_back(p);
           // printf("Pushed %d onto the queue\n", p->getPid()); 
        }
    }

    // free configuration data from memory
    deleteConfig(config);

    // launch 1 scheduling thread per cpu core
    std::thread *schedule_threads = new std::thread[num_cores];
    for (i = 0; i < num_cores; i++)
    {
        schedule_threads[i] = std::thread(coreRunProcesses, i, shared_data);
    }

    //schedule_threads[0] = std::thread(coreRunProcesses, 0, shared_data);

	std::list<Process*>::iterator it = shared_data->ready_queue.begin();

	//std::cout << "PRINTING READY QUEUE" << std::endl;
	for( int i=0; i < shared_data->ready_queue.size(); i++){

		Process* p = *it;
		//std::cout << "Process " << i << " PID: " << p->getPid() << std::endl;
		std::advance(it, 1);
	}


    // main thread work goes here:
    int num_lines = 0;
	uint32_t startIO = 0; 

    while (!(shared_data->all_terminated))
    {
        // clear output from previous iteration
        clearOutput(num_lines);
		int num_terminated = 0; 

		// start new processes at their appropriate start time -- ie. set Process::State = Ready
        std::lock_guard<std::mutex> lock(shared_data->mutex);
        uint32_t now = currentTime();
        for( Process* p : processes ){

            if( p->getState() == Process::State::NotStarted ){

                if( now - start >= p->getStartTime() ){
                    p->setState( Process::State::Ready, now ); 
					shared_data->ready_queue.push_back(p);
                }
         	}
            // determine when an I/O burst finishes and put the process back in the ready queue
            if( p->getState() == Process::State::IO ){
                uint32_t io_burst = p->getCurrentBurstTime();
                if( now - p->getLastUpdate() > io_burst  ){
                    p->updateProcess(now); 
                    p->setState( Process::State::Ready, now ); 
					shared_data->ready_queue.push_back(p);
                }
            }

			if( p->getState() == Process::State::Terminated ){
		     	num_terminated++; 
		    }
        }

		for( Process* p : processes ){

			if( p->getState() == Process::State::Ready){
				p->updateProcess(now); 
			}
		}
		 

        if( num_terminated == processes.size() ){
            shared_data->all_terminated = true; 
        }


        // sort the ready queue (if needed - based on scheduling algorithm)
        sort( shared_data->ready_queue, shared_data->algorithm);
        shared_data->mutex.unlock(); 

        // output process status table
        num_lines = printProcessOutput(processes, shared_data->mutex, shared_data);

        // sleep 1/60th of a second`
        usleep(16667);
    }

    // wait for threads to finish
    for (i = 0; i < num_cores; i++)
    {
        schedule_threads[i].join();
    }

    // print final statistics
    //  - CPU utilization
    //  - Throughput
    //     - Average for first 50% of processes finished
    //     - Average for second 50% of processes finished
    //     - Overall average
    //  - Average turnaround time
    //  - Average waiting time


    // Clean up before quitting program
    processes.clear();

    return 0;
}


bool PPcmp( const Process* a, const Process* b){

    if( a->getPriority() < b->getPriority() ){
        return true;   
    }
    else {
        return false; 
    }
}

bool SJFcmp(const Process* a, const Process* b) {
    uint32_t burstTimeA = a->getBurstTimes()[ a->getCurrentBurst() ];  
    uint32_t burstTimeB = b->getBurstTimes()[ b->getCurrentBurst() ]; 

    if( burstTimeA < burstTimeB ){
        return true; 
    }
    else { 
        return false; 
    } 
}


void sort( std::list<Process*>& q, ScheduleAlgorithm& algo ){

    uint32_t now = currentTime();
    switch( algo ){
        case(FCFS): 
            //no changes to queue. 
            break; 
        case(SJF): 
            q.sort(SJFcmp);
            break; 
        case(RR): 
            //no changes. 
            //core process will boot process off 
            //if RR time slice has expired.
            break; 
        case(PP): 
            q.sort(PPcmp); 
            break; 
    }

}


void coreRunProcesses(uint8_t core_id, SchedulerData *shared_data)
{
	uint32_t start, curr;
	bool preempted = false;

	Process *p;
	Process::State state;

	while (!(shared_data->all_terminated)){

		// MUTEX ?
		//check if queue[0] is ready
		if( shared_data->ready_queue.size() < 1 ) continue;

		p = shared_data->ready_queue.front();
	
		if( p->getState() != Process::Ready ) continue;

		start = currentTime();
		curr = start;

		shared_data->ready_queue.erase(shared_data->ready_queue.begin());
		p->setCpuCore(core_id);
		p->setState( Process::Running, curr );

		//std::cout << "PID: " << p->getPid() << std::endl;
		//std::cout << "ready queue size: " << shared_data->ready_queue.size() << std::endl;

		//simulate process burst execution
		while( curr - start < p->getCurrentBurstTime()){

			if( shared_data->ready_queue.size() < 1 ) continue;

			// MUTEX ?

            if( shared_data->algorithm == ScheduleAlgorithm::PP ){
                
                if( p->getPriority() > shared_data->ready_queue.front()->getPriority() ){

                    std::lock_guard<std::mutex> lock(shared_data->mutex);
                    //update state 
                    p->updateProcess( curr );
                    p->setState( Process::Ready, curr );
					p->setCpuCore(-1);
                    shared_data->ready_queue.push_back( p );
                    shared_data->mutex.unlock();
                    break; 
                }

            }
            //check if time slice expired. 
            else if( shared_data->algorithm == ScheduleAlgorithm::RR ){

                if( curr - start > shared_data->time_slice  ){

                    std::lock_guard<std::mutex> lock(shared_data->mutex);
		            // update state
                    p->updateProcess( curr ); 
					p->setCpuCore(-1);
                    p->setState( Process::State::Ready, curr); 
			        shared_data->ready_queue.push_back( p );

                    shared_data->mutex.unlock();
                    break;
                }
            }

			curr = currentTime();
		}
        // terminate if no cpu bursts are left
        // we will always end with a CPU burst. 

        std::lock_guard<std::mutex> lock(shared_data->mutex);
        p->updateProcess(curr); 
		if( p->getRemainingTime() <= 0 ){
					
			//std::cout << "finished PID: " << p->getPid() << std::endl;
			p->setCpuCore(-1);
			p->incrementCurrentBurst();
			p->setState( Process::State::Terminated, curr );

		}
		else { 

            // do IO burst if there is stil bursts left. 
			//std::cout << "finished burst for PID: " << p->getPid() << std::endl;
			p->incrementCurrentBurst();
			p->setCpuCore(-1);
			p->setState( Process::State::IO, curr );
		}

        shared_data->mutex.unlock();

        //context switch time 
        usleep( shared_data->context_switch * 1000 );  

    // Work to be done by each core idependent of the other cores
    //  - Get process at front of ready queue
    //  - Simulate the processes running until one of the following:
    //     - CPU burst time has elapsed
    //     - RR time slice has elapsed
    //     - Process preempted by higher priority process
    //  - Place the process back in the appropriate queue
    //     - I/O queue if CPU burst finished (and process not finished)
    //     - Terminated if CPU burst finished and no more bursts remain
    //     - Ready queue if time slice elapsed or process was preempted
    //  - Wait context switching time
    //  * Repeat until all processes in terminated state
	}
}


int printProcessOutput(std::vector<Process*>& processes, std::mutex& mutex, SchedulerData* shared_data )
{
    int i;
    int num_lines = 2;
    std::lock_guard<std::mutex> lock(mutex);
    printf("|   PID | Priority |      State | Core | Turn Time | Wait Time | CPU Time | Remain Time |\n");
    printf("+-------+----------+------------+------+-----------+-----------+----------+-------------+\n");
    for (i = 0; i < processes.size(); i++)
    {
        if (processes[i]->getState() != Process::State::NotStarted)
        {
            uint16_t pid = processes[i]->getPid();
            uint8_t priority = processes[i]->getPriority();
            std::string process_state = processStateToString(processes[i]->getState());
            int8_t core = processes[i]->getCpuCore();
            std::string cpu_core = (core >= 0) ? std::to_string(core) : "--";
            double turn_time = processes[i]->getTurnaroundTime();
            double wait_time = processes[i]->getWaitTime();
            double cpu_time = processes[i]->getCpuTime();
            double remain_time = processes[i]->getRemainingTime();
            printf("| %5u | %8u | %10s | %4s | %9.1lf | %9.1lf | %8.1lf | %11.1lf |\n", 
                   pid, priority, process_state.c_str(), cpu_core.c_str(), turn_time, 
                   wait_time, cpu_time, (double)std::max((int)remain_time, 0));
            num_lines++;
        }
    }
	//std::cout << "PRINTING READY QUEUE" << std::endl;
    num_lines++;
	std::list<Process*>::iterator it = shared_data->ready_queue.begin();
	for( int i=0; i < shared_data->ready_queue.size(); i++){

		Process* p = *it;
		//std::cout << "Process " << i << " PID: " << p->getPid() << std::endl;
		std::advance(it, 1);
        num_lines++; 
	}

    return num_lines;
}

void clearOutput(int num_lines)
{
    int i;
    for (i = 0; i < num_lines; i++)
    {
        fputs("\033[A\033[2K", stdout);
    }
    rewind(stdout);
    fflush(stdout);
}

uint32_t currentTime()
{
    uint32_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count();
    return ms;
}

std::string processStateToString(Process::State state)
{
    std::string str;
    switch (state)
    {
        case Process::State::NotStarted:
            str = "not started";
            break;
        case Process::State::Ready:
            str = "ready";
            break;
        case Process::State::Running:
            str = "running";
            break;
        case Process::State::IO:
            str = "i/o";
            break;
        case Process::State::Terminated:
            str = "terminated";
            break;
        default:
            str = "unknown";
            break;
    }
    return str;
}
