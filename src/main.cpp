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
    int32_t context_switch;
    int32_t time_slice;
    std::list<Process*> ready_queue;
    bool all_terminated;
} SchedulerData;

// -- Function declarations --
void coreRunProcesses(int8_t core_id, SchedulerData *data);
int printProcessOutput(std::vector<Process*>& processes, std::mutex& mutex);
void clearOutput(int num_lines);
int32_t currentTime();
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
    int8_t num_cores = config->cores;
    shared_data = new SchedulerData();
    shared_data->algorithm = config->algorithm;
    shared_data->context_switch = config->context_switch;
    shared_data->time_slice = config->time_slice;
    shared_data->all_terminated = false;

    // create processes
    int32_t start = currentTime();
    for (i = 0; i < config->num_processes; i++)
    {
        Process *p = new Process(config->processes[i], start);
        processes.push_back(p);
        if (p->getState() == Process::State::Ready)
        {
            shared_data->ready_queue.push_back(p);
        }
    }

    // free configuration data from memory
    deleteConfig(config);

	usleep(500000);

    // launch 1 scheduling thread per cpu core
    std::thread *schedule_threads = new std::thread[num_cores];
    for (i = 0; i < num_cores; i++)
    {
        schedule_threads[i] = std::thread(coreRunProcesses, i, shared_data);
    }

/*	std::list<Process*>::iterator it = shared_data->ready_queue.begin();

	std::cout << "PRINTING READY QUEUE" << std::endl;

	for( int i=0; i < shared_data->ready_queue.size(); i++){

		Process* p = *it;
		std::cout << "Process " << i << " PID: " << p->getPid() << std::endl;
		std::advance(it, 1);

	}
*/

    //schedule_threads[0] = std::thread(coreRunProcesses, 0, shared_data);








     /*   for( Process* p : shared_data->ready_queue ){

			std::cout << "PID " << p->getPid() << std::endl;
		}
*/


    // main thread work goes here:
    int num_lines = 0;
	bool execIO = false;
	int32_t startIO = 0; 
	int32_t now;
	Process *IO_proc;
    
	while (!(shared_data->all_terminated))
    {
        // clear output from previous iteration
        clearOutput(num_lines);

		// start new processes at their appropriate start time -- ie. set Process::State = Ready
        for( Process* p : processes ){
            
                int32_t now = currentTime();
            if( p->getState() == Process::State::NotStarted ){


                if( now - start > p->getStartTime() ){
                    p->setState( Process::State::Ready, now ); 
					shared_data->ready_queue.push_back(p);
                }
         	}
        }


























        now = currentTime();

// ----- HANDLE I/O HERE -----	

		// if a process is currently doing IO
		if( execIO ){							

			//see if process doing IO is finished
			if( (currentTime() - startIO) > IO_proc->getStartTime() ){

	            IO_proc->setState( Process::State::Ready, now ); 
				execIO = false;
	        }
			
			std::cout << "doing IO !!!!" << std::endl;

		}

		// No IO is happening
		else{

			for( Process* p : shared_data->ready_queue ){
		        
				now = currentTime();	

				//If process is ready for IO
		       	if( p->getState() == Process::State::IO ){	

					startIO = currentTime();
					IO_proc = p;
					execIO = true;
				}
		    }
		}

		

		
		










		now = currentTime();	
		for( Process* p : shared_data->ready_queue ){
			p->updateProcess( now );
	    }





			






        // sort the ready queue (if needed - based on scheduling algorithm)
        sort( shared_data->ready_queue, shared_data->algorithm);

        // output process status table
        num_lines = printProcessOutput(processes, shared_data->mutex);

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




bool SJFcmp(const Process* a, const Process* b){
    int32_t burstTimeA = a->getBurstTimes()[ a->getCurrentBurst() ];  
    int32_t burstTimeB = b->getBurstTimes()[ b->getCurrentBurst() ]; 

    if( burstTimeA < burstTimeB ){
        return true; 
    }
    else { return false; } 
}


void sort( std::list<Process*>& q, ScheduleAlgorithm& algo ){

    int32_t now = currentTime();
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
            break; 
    }

}




























void coreRunProcesses(int8_t core_id, SchedulerData *shared_data)
{
	int start = currentTime();
	int curr = currentTime();
	//bool preempted = false;
	bool ready = false;
	Process *p;
	Process::State state;
	bool timeSliceExpired;
	//std::unique_lock<std::mutex> lock(shared_data->mutex, std::defer_lock);

	while (!(shared_data->all_terminated)){

		sleep(2);

    	timeSliceExpired = false; 

		// Get lock and pop Ready Process
		{

   		 	std::lock_guard<std::mutex> lock(shared_data->mutex);
			if( shared_data->ready_queue.size() < 1 ) continue;

			// find the highest priority process that is ready 
			for( Process *proc : shared_data->ready_queue ){

				//std::cout << "proc " << proc->getPid() << std::endl;
		        
		        if( proc->getState() == Process::State::Ready ){
					printf( "assigned process in core %d\n", core_id );
					p = proc;
					break;
		     	}
		    } 

			//make sure it didnt just loop through all processes
		   	if( p->getState() == Process::State::Ready ){
				p->setCpuCore(core_id);
				p->setState( Process::State::Running, curr );
		    }

			else{
				std::cout << "continuing" << std::endl;
				continue;

			}
		}

		start = currentTime();
		curr = currentTime();
		uint32_t *burst_array = p->getBurstTimes();
		int this_burst = p->getCurrentBurst();
		

		//printf( "start: %d currBurst: %d", start, p->getBurstTimes()[p->getCurrentBurst()]);
		//sleep(10);
		//std::cout << "start" << start << "compared to " << p->getBurstTimes()[p->getCurrentBurst()] << std::endl;

		while( (curr - start) < burst_array[this_burst] ){

			//std::cout << burst_array[this_burst]  << std::endl;

			if( shared_data->ready_queue.size() > 0 ){

				//check if a process has higher priority
				{
				   	std::lock_guard<std::mutex> lock(shared_data->mutex);

					for( Process *other : shared_data->ready_queue ){

						if( other == p ) break; 

						if( other->getState() == Process::State::Ready ){

							p->updateBurstTime( this_burst, burst_array[this_burst] - (curr - start) );
							p->setState( Process::State::Ready, curr );
							p->setCpuCore(-1);
							
							start = currentTime();
							other->setCpuCore(core_id);
							other->setState( Process::State::Running, start );

							p = other;
						}
					}
				}
			}
			

            //check if time slice expired. 
            if( shared_data->algorithm == ScheduleAlgorithm::RR ){

                curr = currentTime();
                if( curr - start > shared_data->time_slice  ){
                    break;
                }
            }
			
			curr = currentTime();
		}


		if( sizeof(p->getBurstTimes()) - p->getCurrentBurst() == 0){
					
			//std::cout << "finished PID: " << p->getPid() << std::endl;
			p->incrementCurrentBurst();
			p->setCpuCore(-1);
			p->setState( Process::State::Terminated, curr );
		}

		else{

			//std::cout << "finished burst for PID: " << p->getPid() << std::endl;
			p->updateBurstTime( p->getCurrentBurst(), 0 );
			p->incrementCurrentBurst();
			p->setCpuCore(-1);
			p->setState( Process::State::IO, curr );
			shared_data->ready_queue.push_back( p );
		}

		sleep( shared_data->context_switch );



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

































int printProcessOutput(std::vector<Process*>& processes, std::mutex& mutex)
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
            int16_t pid = processes[i]->getPid();
            int8_t priority = processes[i]->getPriority();
            std::string process_state = processStateToString(processes[i]->getState());
            int8_t core = processes[i]->getCpuCore();
            std::string cpu_core = (core >= 0) ? std::to_string(core) : "--";
            double turn_time = processes[i]->getTurnaroundTime();
            double wait_time = processes[i]->getWaitTime();
            double cpu_time = processes[i]->getCpuTime();
            double remain_time = processes[i]->getRemainingTime();
            printf("| %5u | %8u | %10s | %4s | %9.1lf | %9.1lf | %8.1lf | %11.1lf |\n", 
                   pid, priority, process_state.c_str(), cpu_core.c_str(), turn_time, 
                   wait_time, cpu_time, remain_time);
            num_lines++;
        }
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

int32_t currentTime()
{
    int32_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
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
