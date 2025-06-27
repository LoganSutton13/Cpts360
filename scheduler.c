#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Headers as needed

typedef enum {false, true} bool;        // Allows boolean types in C

/* Defines a job struct */
typedef struct Process {
    uint32_t A;                         // A: Arrival time of the process
    uint32_t B;                         // B: Upper Bound of CPU burst times of the given random integer list
    uint32_t C;                         // C: Total CPU time required
    uint32_t M;                         // M: Multiplier of CPU burst time
    uint32_t processID;                 // The process ID given upon input read

    uint8_t status;                     // 0 is unstarted, 1 is ready, 2 is running, 3 is blocked, 4 is terminated

    int32_t finishingTime;              // The cycle when the the process finishes (initially -1)
    uint32_t currentCPUTimeRun;         // The amount of time the process has already run (time in running state)
    uint32_t currentIOBlockedTime;      // The amount of time the process has been IO blocked (time in blocked state)
    uint32_t currentWaitingTime;        // The amount of time spent waiting to be run (time in ready state)

    uint32_t IOBurst;                   // The amount of time until the process finishes being blocked
    uint32_t CPUBurst;                  // The CPU availability of the process (has to be > 1 to move to running)

    int32_t quantum;                    // Used for schedulers that utilise pre-emption

    bool isFirstTimeRunning;            // Used to check when to calculate the CPU burst when it hits running mode

    struct Process* nextInBlockedList;  // A pointer to the next process available in the blocked list
    struct Process* nextInReadyQueue;   // A pointer to the next process available in the ready queue
    struct Process* nextInReadySuspendedQueue; // A pointer to the next process available in the ready suspended queue
} _process;


uint32_t CURRENT_CYCLE = 0;             // The current cycle that each process is on
uint32_t TOTAL_CREATED_PROCESSES = 0;   // The total number of processes constructed
uint32_t TOTAL_STARTED_PROCESSES = 0;   // The total number of processes that have started being simulated
uint32_t TOTAL_FINISHED_PROCESSES = 0;  // The total number of processes that have finished running
uint32_t TOTAL_NUMBER_OF_CYCLES_SPENT_BLOCKED = 0; // The total cycles in the blocked state

const char* RANDOM_NUMBER_FILE_NAME= "random-numbers";
const uint32_t SEED_VALUE = 200;  // Seed value for reading from file

// Additional variables as needed
int QUANTUM = 2;


/**
 * Reads a random non-negative integer X from a file with a given line named random-numbers (in the current directory)
 */
uint32_t getRandNumFromFile(uint32_t line, FILE* random_num_file_ptr){
    uint32_t end, loop;
    char str[512];

    rewind(random_num_file_ptr); // reset to be beginning
    for(end = loop = 0;loop<line;++loop){
        if(0==fgets(str, sizeof(str), random_num_file_ptr)){ //include '\n'
            end = 1;  //can't input (EOF)
            break;
        }
    }
    if(!end) {
        return (uint32_t) atoi(str);
    }

    // fail-safe return
    return (uint32_t) 1804289383;
}



/**
 * Reads a random non-negative integer X from a file named random-numbers.
 * Returns the CPU Burst: : 1 + (random-number-from-file % upper_bound)
 */
uint32_t randomOS(uint32_t upper_bound, uint32_t process_indx, FILE* random_num_file_ptr)
{
    char str[20];
    
    uint32_t unsigned_rand_int = (uint32_t) getRandNumFromFile(SEED_VALUE+process_indx, random_num_file_ptr);
    uint32_t returnValue = 1 + (unsigned_rand_int % upper_bound);

    return returnValue;
} 


/********************* SOME PRINTING HELPERS *********************/


/**
 * Prints to standard output the original input
 * process_list is the original processes inputted (in array form)
 */
void printStart(_process process_list[])
{
    printf("The original input was: %i", TOTAL_CREATED_PROCESSES);

    uint32_t i = 0;
    for (; i < TOTAL_CREATED_PROCESSES; ++i)
    {
        printf(" ( %i %i %i %i)", process_list[i].A, process_list[i].B,
               process_list[i].C, process_list[i].M);
    }
    printf("\n");
} 

/**
 * Prints to standard output the final output
 * finished_process_list is the terminated processes (in array form) in the order they each finished in.
 */
void printFinal(_process finished_process_list[])
{
    printf("The (sorted) input is: %i", TOTAL_CREATED_PROCESSES);

    uint32_t i = 0;
    for (; i < TOTAL_FINISHED_PROCESSES; ++i)
    {
        printf(" ( %i %i %i %i)", finished_process_list[i].A, finished_process_list[i].B,
               finished_process_list[i].C, finished_process_list[i].M);
    }
    printf("\n");
} // End of the print final function

/**
 * Prints out specifics for each process.
 * @param process_list The original processes inputted, in array form
 */
void printProcessSpecifics(_process process_list[])
{
    uint32_t i = 0;
    printf("\n");
    for (; i < TOTAL_CREATED_PROCESSES; ++i)
    {
        printf("Process %i:\n", process_list[i].processID);
        printf("\t(A,B,C,M) = (%i,%i,%i,%i)\n", process_list[i].A, process_list[i].B,
               process_list[i].C, process_list[i].M);
        printf("\tFinishing time: %i\n", process_list[i].finishingTime);
        printf("\tTurnaround time: %i\n", process_list[i].finishingTime - process_list[i].A);
        printf("\tI/O time: %i\n", process_list[i].currentIOBlockedTime);
        printf("\tWaiting time: %i\n", process_list[i].currentWaitingTime);
        printf("\n");
    }
} // End of the print process specifics function

/**
 * Prints out the summary data
 * process_list The original processes inputted, in array form
 */
void printSummaryData(_process process_list[])
{
    uint32_t i = 0;
    double total_amount_of_time_utilizing_cpu = 0.0;
    double total_amount_of_time_io_blocked = 0.0;
    double total_amount_of_time_spent_waiting = 0.0;
    double total_turnaround_time = 0.0;
    uint32_t final_finishing_time = CURRENT_CYCLE - 1;
    for (; i < TOTAL_CREATED_PROCESSES; ++i)
    {
        total_amount_of_time_utilizing_cpu += process_list[i].currentCPUTimeRun;
        total_amount_of_time_io_blocked += process_list[i].currentIOBlockedTime;
        total_amount_of_time_spent_waiting += process_list[i].currentWaitingTime;
        total_turnaround_time += (process_list[i].finishingTime - process_list[i].A);
    }

    // Calculates the CPU utilisation
    double cpu_util = total_amount_of_time_utilizing_cpu / final_finishing_time;

    // Calculates the IO utilisation
    double io_util = (double) TOTAL_NUMBER_OF_CYCLES_SPENT_BLOCKED / final_finishing_time;

    // Calculates the throughput (Number of processes over the final finishing time times 100)
    double throughput =  100 * ((double) TOTAL_CREATED_PROCESSES/ final_finishing_time);

    // Calculates the average turnaround time
    double avg_turnaround_time = total_turnaround_time / TOTAL_CREATED_PROCESSES;

    // Calculates the average waiting time
    double avg_waiting_time = total_amount_of_time_spent_waiting / TOTAL_CREATED_PROCESSES;

    printf("Summary Data:\n");
    printf("\tFinishing time: %i\n", CURRENT_CYCLE - 1);
    printf("\tCPU Utilisation: %6f\n", cpu_util);
    printf("\tI/O Utilisation: %6f\n", io_util);
    printf("\tThroughput: %6f processes per hundred cycles\n", throughput);
    printf("\tAverage turnaround time: %6f\n", avg_turnaround_time);
    printf("\tAverage waiting time: %6f\n", avg_waiting_time);
} // End of the print summary data function

void print_cycle(_process process_list[])
{
    printf("Before cyle %d:\n", CURRENT_CYCLE);
    for(int i = 0; i < TOTAL_CREATED_PROCESSES; i++ )
    {
        switch(process_list[i].status)
        {
            case 0: printf("\tunstarted\t0"); break;
            case 1: printf("\tready\t\t0"); break;
            case 2: printf("\trunning\t\t%d", process_list[i].CPUBurst); break;
            case 3: printf("\tblocked\t\t%d", process_list[i].IOBurst); break;
            case 4: printf("\tfinished\t0"); break;
        }
    }

    printf("\n");
}

void initialize(_process process_list[], int num_of_proceses)
{
    for(int i = 0; i < num_of_proceses; i++)
    {
        process_list[i].status = 0;
        process_list[i].finishingTime = -1;
        process_list[i].currentCPUTimeRun = 0;
        process_list[i].currentIOBlockedTime = 0;
        process_list[i].currentWaitingTime = 0;
        process_list[i].IOBurst = 0;
        process_list[i].CPUBurst = 0;
        process_list[i].quantum = 0;
        process_list[i].isFirstTimeRunning = true;
        process_list[i].nextInBlockedList = NULL;
        process_list[i].nextInReadyQueue = NULL;
        process_list[i].nextInReadySuspendedQueue = NULL;
    }
}

void begin_process(_process *to_start, FILE* rand)
{
    // checks if we need to generate a new burst
    //printf("starting a process %d\n", to_start->processID);
    if(to_start->CPUBurst == 0)
    {
        // we need new burst
        uint32_t time_left = 0;
        to_start->CPUBurst = randomOS(to_start->B, to_start->processID, rand);
        time_left = to_start->C - to_start->currentCPUTimeRun;
        if(time_left < to_start->CPUBurst)
        {
            to_start->CPUBurst = time_left;
        }
        to_start->IOBurst = to_start->M * to_start->CPUBurst;
        //printf("burst updated it is %d\n", to_start->CPUBurst);
    }
    

    to_start->nextInBlockedList = to_start->nextInReadyQueue = to_start->nextInReadySuspendedQueue = NULL;
    to_start->status = 2;
    to_start->quantum = QUANTUM; // only for RR

    if(to_start->isFirstTimeRunning)
    {
        TOTAL_STARTED_PROCESSES++;
        to_start->isFirstTimeRunning = false;
    }
}

void add_blocked_process(_process *to_add, _process **head)
{
    _process *cur = *head;
    to_add->status = 3;
    to_add->nextInBlockedList = NULL;
    to_add->nextInReadyQueue = NULL;
    to_add->nextInReadySuspendedQueue = NULL;

    if(cur == NULL)
    {
        *head = to_add;
        return;
    }

    // check to insert at front
    if(to_add->IOBurst < (*head)->IOBurst)
    {
        *head = to_add;
        return;
    }


    // o.w. we will need to insert somewhere in middle to keep it sorted
    while(cur->nextInBlockedList != NULL)
    {
        if(to_add->IOBurst < cur->nextInBlockedList->IOBurst)
        {
            // found place to add
            to_add->nextInBlockedList = cur->nextInBlockedList;
            cur->nextInBlockedList = to_add;
            return;
        }
        cur = cur->nextInBlockedList;
    }
    // if we make it here, we need to insert at the end
    cur->nextInBlockedList = to_add;
}


/*FIFO Scheduling*/
void fifo_queue_ready_process(_process *to_add, _process **head)
{
    // adds a ready process to the queue of ready processes
 
    _process *cur = *head;
    if(cur == NULL)
    {
        *head = to_add;
        to_add->status = 1;
        return;
    }

    while(cur->nextInReadyQueue != NULL)
    {
        cur = cur->nextInReadyQueue;
    }

    cur->nextInReadyQueue = to_add;
    to_add->status = 1;
    to_add->nextInReadyQueue = NULL;
    to_add->nextInBlockedList = NULL;
    to_add->nextInReadySuspendedQueue = NULL;
}

void fifo_insert_arrivials_in_ready_queue(_process process_list[], _process **head)
{
    // this will be checked every cycle and will add processes to the ready queue
    for(int i = 0; i < TOTAL_CREATED_PROCESSES; i++)
    {
        if(process_list[i].A == CURRENT_CYCLE)
        {
            //printf("inserting arrival");
            fifo_queue_ready_process(&process_list[i], head);
        }
    }
}
void FIFO(_process process_list[], FILE* rand, _process ord_list[])
{
    _process *ready_process = NULL;
    _process *blocked_process = NULL;
    _process *running_process = NULL;
    _process *cur = NULL;

    // main cycle loop
    while(TOTAL_CREATED_PROCESSES != TOTAL_FINISHED_PROCESSES)
    {
        // print_cycle(process_list);
        // fail safe for testing
        if(CURRENT_CYCLE > 1000000)
        {
            printf("max cycle of 1,000,000 exceeded!\n");
            return;
        }
        cur = NULL;
        if(blocked_process != NULL)
        {
            // we have a blocked process
            TOTAL_NUMBER_OF_CYCLES_SPENT_BLOCKED ++;
            cur = blocked_process;

            // we need to update all blocked processes time spent blocked
            while(cur != NULL)
            {
                //printf("checking for loop\n");
                cur->currentIOBlockedTime ++;
                cur->IOBurst --; // decrement IO burst;
                if(cur->IOBurst == 0)
                {
                    // the process is no longer blocked
                    fifo_queue_ready_process(cur, &ready_process);
                    // remove from blocked list, works because of sorted block list
                    blocked_process = blocked_process->nextInBlockedList;
                    cur = blocked_process;
                }
                else
                {
                    cur = cur->nextInBlockedList;
                }
            }
        }

        // insert any arrivals
        fifo_insert_arrivials_in_ready_queue(process_list, &ready_process);

        if(running_process != NULL)
        {
            // there is currently a process running
            running_process->currentCPUTimeRun += 1; // incr run time
            running_process->CPUBurst -= 1; // decr cpu burst time

            // check if process is done
            if(running_process->currentCPUTimeRun == running_process->C)
            {
                //printf("process %d finished!\n", running_process->processID);
                // the process is finished!
                running_process->status = 4;
                running_process->finishingTime = CURRENT_CYCLE;

                // store the processes as they finish in order
                ord_list[TOTAL_FINISHED_PROCESSES] = *running_process;
                TOTAL_FINISHED_PROCESSES ++;
                running_process = NULL;
            }
            else if(running_process->CPUBurst == 0)
            {
                // time to switch process over to IO, we need to add to blocked
                add_blocked_process(running_process, &blocked_process);
                running_process = NULL;    
            }     
        }

        if(running_process == NULL && ready_process != NULL)
        {
            // we need to get the next ready process ready, if any
            running_process = ready_process;
            ready_process = ready_process->nextInReadyQueue;
            // start the next process
            begin_process(running_process, rand);
            //printf("process should have started\n");
        }
        
        // update waiting time
        cur = ready_process;
        while(cur != NULL)
        {
            //printf("updating waiting time process %d\n", cur->processID);
            cur->currentWaitingTime +=1;
            cur = cur->nextInReadyQueue;
        }
        // update the cycle counter
        CURRENT_CYCLE ++;
        //printf("cycle incremented\n");
    }
}

/*RR Scheduling*/
void rr_queue_ready_process(_process *to_add, _process **head)
{
    _process *cur = *head;
    if(cur == NULL)
    {
        *head = to_add;
        to_add->status = 1;
        return;
    }

    while(cur->nextInReadyQueue != NULL)
    {
        cur = cur->nextInReadyQueue;
    }

    cur->nextInReadyQueue = to_add;
    to_add->status = 1;
    to_add->nextInReadyQueue = NULL;
    to_add->nextInBlockedList = NULL;
    to_add->nextInReadySuspendedQueue = NULL;
}

void rr_queue_suspended_process(_process *to_add, _process **head)
{
    _process *cur = *head;
    if(cur == NULL)
    {
        *head = to_add;
        to_add->status = 1;
        return;
    }

    while(cur->nextInReadySuspendedQueue != NULL)
    {
        cur = cur->nextInReadySuspendedQueue;
    }

    cur->nextInReadySuspendedQueue = to_add;
    to_add->status = 1;
    to_add->nextInReadyQueue = NULL;
    to_add->nextInBlockedList = NULL;
    to_add->nextInReadySuspendedQueue = NULL;
}

void rr_insert_arrivals_in_ready_queue(_process process_list[], _process **head)
{
     // this will be checked every cycle and will add processes to the ready queue
    for(int i = 0; i < TOTAL_CREATED_PROCESSES; i++)
    {
        if(process_list[i].A == CURRENT_CYCLE)
        {
            //printf("inserting arrival");
            rr_queue_ready_process(&process_list[i], head);
        }
    }
}

void resume_suspended_process(_process *p)
{
    p->quantum = QUANTUM;
    p->nextInBlockedList = NULL;
    p->nextInReadyQueue = NULL;
    p->nextInReadySuspendedQueue = NULL;
    p->status = 2;
}

void RR(_process process_list[], FILE *rand, _process ord_list[])
{
    _process *blocked_process = NULL;
    _process *ready_process = NULL;
    _process *waiting_ready_process = NULL;
    _process *running_process = NULL;
    _process *cur = NULL;

    // main cycle loop
    while(TOTAL_CREATED_PROCESSES != TOTAL_FINISHED_PROCESSES)
    {
        //print_cycle(process_list);
        // fail safe for testing
        if(CURRENT_CYCLE > 1000000)
        {
            printf("max cycle of 1,000,000 exceeded!\n");
            return;
        }
        // check for blocked processes
        if(blocked_process != NULL)
        {
            // there is a blocked process
            TOTAL_NUMBER_OF_CYCLES_SPENT_BLOCKED ++;
            cur = blocked_process;
            while(cur != NULL)
            {
                cur->IOBurst--;
                cur->currentIOBlockedTime++;
                if(cur->IOBurst == 0)
                {
                    // done with IO, needs to be added to ready processes
                    rr_queue_ready_process(cur, &ready_process);
                    blocked_process = blocked_process->nextInBlockedList;
                    cur = blocked_process;
                }
                else
                {
                    // update IOBurst values and blocked time
                    cur = cur->nextInBlockedList;
                }
            }
        }

        rr_insert_arrivals_in_ready_queue(process_list, &ready_process);

        // check if theres a running process
        if(running_process != NULL)
        {
            running_process->currentCPUTimeRun ++;
            running_process->quantum --;
            running_process->CPUBurst --;
            // check if the process is done
            if(running_process->currentCPUTimeRun == running_process->C)
            {
                // process has finished
                running_process->status = 4;
                running_process->finishingTime = CURRENT_CYCLE;
                // store the processes as they finish in order
                ord_list[TOTAL_FINISHED_PROCESSES] = *running_process;
                TOTAL_FINISHED_PROCESSES++;
                running_process = NULL;
            }
            // check if burst ends
            else if(running_process->CPUBurst == 0)
            {
                // process needs to blocked
                add_blocked_process(running_process, &blocked_process);
                running_process = NULL;
            }
            // check to suspend
            else if(running_process->quantum == 0)
            {
                // process needs to be suspended
                rr_queue_suspended_process(running_process, &waiting_ready_process);
                running_process = NULL;
            }
        }

        if(running_process == NULL)
        {
            // we need to get the next ready process ready, if any

            // case where both queues are not empty
            if(ready_process != NULL && waiting_ready_process != NULL)
            {
                if(ready_process->A < waiting_ready_process->A)
                {
                    running_process = ready_process;
                    ready_process = ready_process->nextInReadyQueue;
                    begin_process(running_process, rand); // need to start
                }
                else if(ready_process->A > waiting_ready_process->A)
                {
                    running_process = waiting_ready_process;
                    waiting_ready_process = waiting_ready_process->nextInReadySuspendedQueue;
                    resume_suspended_process(running_process); // resume
                }
                else
                {
                    // arrival times are the same
                    // check process ID values
                    if(ready_process->processID < waiting_ready_process->processID)
                    {
                        running_process = ready_process;
                        ready_process = ready_process->nextInReadyQueue;
                        begin_process(running_process, rand); // need to start
                    }
                    else
                    {
                        running_process = waiting_ready_process;
                        waiting_ready_process = waiting_ready_process->nextInReadySuspendedQueue;
                        resume_suspended_process(running_process); // resume
                    }
                }
            }
            else if(ready_process != NULL)
            {
                running_process = ready_process;
                ready_process = ready_process->nextInReadyQueue;
                // this process needs to be started again
                begin_process(running_process, rand);
            }
            else if(waiting_ready_process != NULL)
            {
                running_process = waiting_ready_process;
                waiting_ready_process = waiting_ready_process->nextInReadySuspendedQueue;
                resume_suspended_process(running_process); // resume
            }
        }

        // update waiting time
        cur = ready_process;
        while(cur != NULL)
        {
            //printf("updating ready waiting time process %d\n", cur->processID);
            cur->currentWaitingTime +=1;
            cur = cur->nextInReadyQueue;
        }
        cur = waiting_ready_process;
        while(cur!=NULL)
        {
            //printf("updating waiting queue time process %d\n", cur->processID);
            cur->currentWaitingTime +=1;
            cur = cur->nextInReadyQueue;
        }
        // update the cycle counter
        CURRENT_CYCLE ++;
    }
}

/*SJF Scheduling*/
void sjf_queue_ready_process(_process *to_add, _process **head)
{
     _process *cur = *head;
    to_add->status = 1;
    to_add->nextInBlockedList = NULL;
    to_add->nextInReadySuspendedQueue = NULL;
    to_add->nextInReadyQueue = NULL;
    // should we insert at front
    if(cur == NULL || to_add->C - to_add->currentCPUTimeRun < cur->C - cur->currentCPUTimeRun)
    {
        to_add->nextInReadyQueue = *head;
        *head = to_add;
        return;
    }


    // o.w. we need to insert in order with shortest time left
    while(cur->nextInReadyQueue != NULL)
    {
        if((to_add->C - to_add->currentCPUTimeRun) < (cur->nextInReadyQueue->C 
        - cur->nextInReadyQueue->currentCPUTimeRun))
        {
            to_add->nextInReadyQueue = cur->nextInReadyQueue;
            cur->nextInReadyQueue = to_add;
            return;
        }
        cur = cur->nextInReadyQueue;
    }

    cur->nextInReadyQueue = to_add;
}
void sjf_insert_arrivals_in_ready_queue(_process process_list[], _process **head)
{
     // this will be checked every cycle and will add processes to the ready queue
    for(int i = 0; i < TOTAL_CREATED_PROCESSES; i++)
    {
        if(process_list[i].A == CURRENT_CYCLE)
        {
            //printf("inserting arrival");
            sjf_queue_ready_process(&process_list[i], head);
        }
    }
}

void SJF(_process process_list[], FILE* rand, _process ord_list[])
{
    _process *ready_process = NULL;
    _process *blocked_process = NULL;
    _process *running_process = NULL;
    _process *cur = NULL;

    // main cycle loop
    while(TOTAL_CREATED_PROCESSES != TOTAL_FINISHED_PROCESSES)
    {
        //print_cycle(process_list);

        // fail safe for testing
        if(CURRENT_CYCLE > 1000000)
        {
            printf("max cycle of 1,000,000 exceeded!\n");
            return;
        }
        cur = NULL;
        if(blocked_process != NULL)
        {
            // we have a blocked process
            TOTAL_NUMBER_OF_CYCLES_SPENT_BLOCKED ++;
            cur = blocked_process;

            // we need to update all blocked processes time spent blocked
            while(cur != NULL)
            {
                //printf("checking for loop\n");
                cur->currentIOBlockedTime ++;
                cur->IOBurst --; // decrement IO burst;
                if(cur->IOBurst == 0)
                {
                    // the process is no longer blocked
                    sjf_queue_ready_process(cur, &ready_process);
                    // remove from blocked list, works because of sorted block list
                    blocked_process = blocked_process->nextInBlockedList;
                    cur = blocked_process;
                }
                else
                {
                    cur = cur->nextInBlockedList;
                }
            }
        }

        // insert any arrivals
        sjf_insert_arrivals_in_ready_queue(process_list, &ready_process);

        if(running_process != NULL)
        {
            // there is currently a process running
            running_process->currentCPUTimeRun += 1; // incr run time
            running_process->CPUBurst -= 1; // decr cpu burst time

            // check if process is done
            if(running_process->currentCPUTimeRun == running_process->C)
            {
                //printf("process %d finished!\n", running_process->processID);
                // the process is finished!
                running_process->status = 4;
                running_process->finishingTime = CURRENT_CYCLE;
                // store the processes as they finish in order
                ord_list[TOTAL_FINISHED_PROCESSES] = *running_process;
                TOTAL_FINISHED_PROCESSES ++;
                running_process = NULL;
            }
            else if(running_process->CPUBurst == 0)
            {
                // time to switch process over to IO, we need to add to blocked
                add_blocked_process(running_process, &blocked_process);
                running_process = NULL;    
            }     
        }

        if(running_process == NULL && ready_process != NULL)
        {
            // we need to get the next ready process ready, if any
            running_process = ready_process;
            ready_process = ready_process->nextInReadyQueue;
            // start the next process
            begin_process(running_process, rand);
            //printf("process should have started\n");
        }
        
        // update waiting time
        cur = ready_process;
        while(cur != NULL)
        {
            //printf("updating waiting time process %d\n", cur->processID);
            cur->currentWaitingTime +=1;
            cur = cur->nextInReadyQueue;
        }
        // update the cycle counter
        CURRENT_CYCLE ++;
        //printf("cycle incremented\n");
    }
}

void reset_globals()
{
    TOTAL_FINISHED_PROCESSES = 0;
    TOTAL_STARTED_PROCESSES = 0;
    TOTAL_NUMBER_OF_CYCLES_SPENT_BLOCKED = 0;
    CURRENT_CYCLE = 0;
}
/**
 * The magic starts from here
 */
int main(int argc, char *argv[])
{
    uint32_t total_num_of_process; // Read from the file -- number of process to create

    // open file to read input
    FILE *input = fopen(argv[1], "r");
    if(!input) // check if the file is open
    {
        printf("Failed to open file!\n");
        return 1;
    }

    // read the file
    char line[512] = "";
    char *tok; 
    if(!fgets(line,512, input))
    {
        printf("Failed to read line!\n");
        return 1;
    }
    tok = strtok(line, " ");
    TOTAL_CREATED_PROCESSES = atoi(tok);
    //printf("total processes %d\n", TOTAL_CREATED_PROCESSES);

    _process process_list[TOTAL_CREATED_PROCESSES]; // Creates a container for all processes

    // load processes
    for(int i = 0; i < TOTAL_CREATED_PROCESSES; i++)
    {
        tok = strtok(NULL, " "); // get A
        //printf("%s\n", tok);

        process_list[i].A = atoi(tok + 1);
        tok = strtok(NULL, " "); // get C
        //printf("%s\n", tok);

        process_list[i].B = atoi(tok);
        tok = strtok(NULL, " "); // get B
        //printf("%s\n", tok);

        process_list[i].C = atoi(tok);
        tok = strtok(NULL, ")"); // get M
        //printf("%s\n", tok);

        process_list[i].M = atoi(tok);
        process_list[i].processID = i;
    }

    fclose(input);

    // DONE READING INPUT //


    // Open the random file
    FILE* random_file = fopen("random-numbers", "r");
    if(!random_file)
    {
        printf("error opening random file\n");
        return 1;
    }

    // used for storing order of processes in which they finished
    _process ordered_process_list[TOTAL_CREATED_PROCESSES];

    // fifo run
    initialize(process_list, TOTAL_CREATED_PROCESSES);
    FIFO(process_list, random_file, ordered_process_list);
    printf("/*START OF FIRST IN FIRST OUT*/\n");
    printStart(process_list);
    printFinal(ordered_process_list); // need to store final process order
    printf("\nThe process used was FIFO.\n\n");
    printProcessSpecifics(process_list);
    printSummaryData(process_list);
    printf("/*END OF FIRST IN FIRST OUT*/\n");

    // rr run
    initialize(process_list, TOTAL_CREATED_PROCESSES);
    reset_globals();
    RR(process_list, random_file, ordered_process_list);
    printf("/*START OF ROUND ROBIN*/\n");
    printStart(process_list);
    printFinal(ordered_process_list);
    printf("\nThe process used was RR.\n\n");
    printProcessSpecifics(process_list);
    printSummaryData(process_list);
    printf("/*END OF ROUND ROBIN*/\n");

    // sjf run
    initialize(process_list, TOTAL_CREATED_PROCESSES);
    reset_globals();
    SJF(process_list, random_file, ordered_process_list);
    printf("/*START OF SHORTEST JOB FIRST*/\n");
    printStart(process_list);
    printFinal(ordered_process_list);
    printf("\nThe process used was SJF.\n\n");
    printProcessSpecifics(process_list);
    printSummaryData(process_list);
    printf("/*END OF SHORTEST JOB FIRST*/\n");
    
    fclose(random_file); // close the random file
    return 0;
} 