#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/cred.h>
#include <linux/tty.h>
#include <linux/uidgid.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/param.h>
#include <linux/timer.h>
#include <linux/ktime.h>
#include <linux/time_namespace.h>
#include <linux/time.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Babacar Diallo");
MODULE_DESCRIPTION("CSE330 Spring 2026 Project (Process Management)\n");
MODULE_VERSION("0.1");


#define MAX_BUFFER_SIZE 500 // means the buffer can hold at most 500 items.
#define MAX_NO_OF_PRODUCERS 1
#define MAX_NO_OF_CONSUMERS 100
#define PCINFO(s, ...) pr_info("###[%s]###" s, __FUNCTION__, ##__VA_ARGS__)



/************************************ */
#define MAX_TRACKED 2000
int handled_pids[MAX_TRACKED];
int handled_count = 0;
/************************************ */


//unsigned long long total_time_elapsed = 0;
int producer_thread_function(void *pv);
int consumer_thread_function(void *pv);
char *replace_char(char *str, char find, char replace);
void name_threads(void);
// use this struct to store the process information
struct  process_info {


 unsigned long pid; // WHO
 unsigned long long start_time; // WHEN THE process started
 unsigned long long boot_time;   // WHEN we measured it.
 } process_default_info = {0, 0, 0}; // 3 zero because process has 3 fields.


int total_no_of_process_produced = 0;
int total_no_of_process_consumed = 0;


int end_flag = 0;


char producers[MAX_NO_OF_PRODUCERS][12] = {"Producer-X"};
char consumers[MAX_NO_OF_CONSUMERS][12] = {"Consumer-X"};


static struct task_struct *ctx_producer_thread[MAX_NO_OF_PRODUCERS];
static struct task_struct *ctx_consumer_thread[MAX_NO_OF_CONSUMERS];




   /**********************************
               BUFFER
    **********************************/



// use fill and use to keep track of the buffer
struct task_struct * buffer[MAX_BUFFER_SIZE]; // buffer stores pointers to zombie processes
int fill = 0;  // where PRODUCER writes next
int use  = 0;  // where CONSUMER reads  next


// Variables to manage the  buffer
int buffSize = 0;   // Current buffer size
int prod = 0;       // Number of producer threads
int cons = 0;       // Number of consumer threads
int uid  = 0;       // Unique identifier



// Allows the user (via insmod / test.sh) to specify
// how many slots the shared producer–consumer buffer has
module_param_named(size, buffSize, int, 0);


// Allows the user to specify how many producer kernel threads
// should be created when the module is loaded
module_param(prod, int, 0);


// Allows the user to specify how many consumer kernel threads
// should be created when the module is loaded
module_param(cons, int, 0);


// Allows the user to specify the UID whose processes
// will be scanned and timed by the producer thread
module_param(uid, int, 0);


struct semaphore empty;
struct semaphore  full;
struct semaphore mutex;


void msleep(unsigned int msecs);


// PRODUCER KERNEL THREAD
// says the producer should sleep 250 ms after iterating
// through the task list and then start the next iteration.
int producer_thread_function(void *pv) {


   allow_signal(SIGKILL);         // Allow this thread to be killed by a signal
   struct task_struct *task;      // Used by for_each_process() to walk the process list


 while (!kthread_should_stop()) { // while (!kthread_should_stop() && !end_flag) { 

   // Walk every process in the system exactly once

      for_each_process(task) {
 
    // If this process is NOT owned by uid, skip it
       if (task->cred->uid.val != uid) {
       continue; 
       }

     //  we skip  processes that aren't zombies
     if ( !(task->exit_state & EXIT_ZOMBIE)) {
        continue;
    }
    get_task_struct(task); //  “hey kernel, don’t delete this process yet

/***************************************/
// check duplicate in buffer
int already_handled = 0;


// checking handled pid
for (int i = 0; i < handled_count; i++) {
   if (handled_pids[i] == task->pid) {
       already_handled = 1;
       break;
   }
}


if (already_handled) {
   continue;
}
/***************************************/




       // =========================
       // 1) Wait for an empty slot
       // =========================
       // empty counts how many EMPTY buffer slots are available.
       // If buffer is full, producer sleeps here until a consumer frees a slot.
       down(&empty);


       // Safe exit point AFTER we might have slept
       if (kthread_should_stop()) { //if (kthread_should_stop() || end_flag) {
           up(&empty);             // Give back the empty slot reservation
           return 0;
        }

       // =========================
       // STEP 2 Lock the shared buffer
       // =========================
       // mutex makes sure only one thread edits buffer[] / fill at a time.
       down(&mutex);


       // Safe exit point AFTER taking the mutex
       if (kthread_should_stop()) { //if (kthread_should_stop() || end_flag) {
           up(&mutex);  // Unlock buffer
           up(&empty);  // Give back empty-slot reservation
            return 0; // NOTe: asked to to change it from continue; to return 0
        }


   // =========================
   // STEP 3 PRODUCE 1 ITEM
   // =========================
   // A- check if this task is already in the buffer
     int task_exist = 0;     


    for (int i = 0; i < buffSize; i++) {
    if (buffer[i] != NULL && buffer[i] == task) {
       task_exist = 1;
       break;
     }
   }
   // skip if duplicate
   if (task_exist) {
       up(&mutex);
       up(&empty);
       continue;
   }
  
   // Othewise add the zombie process into the buffer
    buffer[fill] = task;  // Task = Pointer to a process task: task → struct task_struct *
   
   /*******************************/ 
    if (handled_count < MAX_TRACKED) {
     handled_pids[handled_count++] = task->pid;
    }
    /******************************/


     // Print required message
     pr_info("[%s] has produced a zombie process with pid %d and parent pid %d\n",
          current->comm, task->pid, task->parent->pid);  // current -> comm = thread name [Producer-1]
  
   // Move fill forward (circular buffer)
   fill = fill + 1;
   if (fill == buffSize) // if it reach the end
       fill = 0;         // go back to the beggining.


   // We Track how many we produced.
   total_no_of_process_produced++; 


   //  =========================
   //  4) Unlock + signal "full"
   //  =========================

   up(&mutex);  // Unlock buffer
   up(&full);   // Tell consumers: "1 item is available"
  }

     msleep(250);  
 }
    PCINFO("[%s] Producer Thread stopped.\n", current->comm);
    return 0;
  }


 /************************************************************************************************************/
 /************************************************************************************************************/
 /************************************************************************************************************/


 // - Consumer kernel thread:
 // - Waits for "full" (an available produced item)
 // - Takes mutex to safely read from buffer[use]
 // - Releases mutex and signals "empty" (one free slot)
 // - Updates total_time_elapsed (in ns)


int consumer_thread_function(void *pv) {
    allow_signal(SIGKILL);  


   int no_of_process_consumed = 0;   // local counter for THIS consumer thread
   struct task_struct *item;         // holds ONE consumed item


   while (!kthread_should_stop()) { //while (!kthread_should_stop() && !end_flag)  { // while the thread still running
   down(&full);  // Wait for an available produced item
      
   // If module is exiting, undo and stop
    if (kthread_should_stop()) { //if (kthread_should_stop() || end_flag) {
        up(&full);
         break;
     }

    down(&mutex); // Lock buffer so only one thread touches it

   // Safe exit after locking
    if (kthread_should_stop()) { //if (kthread_should_stop() || end_flag) {    
    up(&mutex); //  Unlocks the buffer
     up(&full);  //  return the reserved  item
         break;
       }
     // Take ONE item from buffer
     item = buffer[use];

    

    
     if (item == NULL) {   // if we read an empty slot on the buffer

         up(&mutex); // release the lock
          up(&full);  // signal an item inside the buffer
          continue;   // try again.
      }


     // Marking the slot as empty after after consumer reads
     buffer[use] = NULL;


// Move to next read slot (circular)
use = use + 1;
if (use == buffSize)
   use = 0;


// Update counters (shared) while still holding mutex
no_of_process_consumed++;
total_no_of_process_consumed++;




// Unlock + signal one empty slot is free now
up(&mutex);
up(&empty);
 if (item->parent) {    // if this is the items parent
    kill_pid(item->parent->thread_pid, SIGKILL, 0); // killed the  zombie process (by killing its parent)
    pr_info("[%s] has consumed a zombie process with pid %d and parent pid %d\n",
    current->comm, item->pid, item->parent->pid);
   }
   put_task_struct(item); // now you can delete the task 
 }


 PCINFO("Consumer Thread stopped.\n");  // must be outside the while loop
   return 0;
}
char *replace_char(char *str, char find, char replace) {

    // IMPORTANT FIX:
    // If the character we want to find is the SAME as the one we want to replace it with,
    // then nothing should change.
    // Without this, strchr() will keep finding the same character forever → infinite loop.
    if (find == replace)
        return str;

    // Find the FIRST occurrence of 'find' in the string
    char *current_pos = strchr(str, find);

    // Loop as long as we keep finding that character
    while (current_pos) {

        // Replace the character at that position
        *current_pos = replace;

        // Look for the NEXT occurrence of 'find'
        // (starting from current position again)
        current_pos = strchr(current_pos, find);
    }

    // Return the modified string
    return str;
}


void name_threads(void) {
   for (int index = 0; index < prod; index++) {
        char id = (index + 1) + '0';
        strcpy(producers[index], "Producer-X");
        replace_char(producers[index], 'X', id);  
   }


   for (int index = 0; index < cons; index++) {
       char id = (index + 1) + '0';
       strcpy(consumers[index], "Consumer-X");
       replace_char(consumers[index], 'X', id); 
   }
}


 static int __init thread_init_module(void) {


 PCINFO("CSE330 Project Kernel Module Inserted\n");
 PCINFO("Kernel module received the following inputs: UID:%d, Buffer-Size:%d, No of Producer:%d, No of Consumer:%d", uid, buffSize, prod, cons);


   if ( (buffSize > 0 && buffSize <=  MAX_BUFFER_SIZE) &&
      ( prod >= 0  && prod < 2) && (cons > 0 && cons <= MAX_NO_OF_CONSUMERS) ) {


  
  sema_init(&empty, buffSize); // empty slots available at start
  sema_init(&full,  0);        // no items produced yet
  sema_init(&mutex, 1);        // lock for buffer (1 = unlocked)
  name_threads();

   for (int index = 0; index < buffSize; index++)
     buffer[index] = NULL; //me: now each buffer slot starts empty

// initialized the thread poiunters to NULL before using them 
for (int i = 0; i < MAX_NO_OF_CONSUMERS; i++) {
    ctx_consumer_thread[i] = NULL;
}

for (int i = 0; i < MAX_NO_OF_PRODUCERS; i++) {
    ctx_producer_thread[i] = NULL;
}
  // ========================================
  // Create PRODUCER thread (only 1 allowed)
  // ========================================
if (prod == 1) {

    // Start producer thread
    ctx_producer_thread[0] =
        kthread_run(producer_thread_function, NULL, producers[0]);

    // Check if creation failed
    if (IS_ERR(ctx_producer_thread[0])) {
        pr_err("Failed to create producer thread\n");

        // Set pointer to NULL so we don’t use invalid thread
        ctx_producer_thread[0] = NULL;

        // Stop module loading because producer is required
        return -1;
    }
}


// ==============================
// Create CONSUMER threads
// ==============================
for (int i = 0; i < cons; i++) {

    // Start one consumer thread
    ctx_consumer_thread[i] =
        kthread_run(consumer_thread_function, NULL, consumers[i]);

    // Check if creation failed
    if (IS_ERR(ctx_consumer_thread[i])) {
        pr_err("Failed to create consumer thread %d\n", i);

        // Set pointer to NULL to avoid invalid access later
        ctx_consumer_thread[i] = NULL;

        // Stop module loading if any consumer fails
        return -1;
    }
}


   // Hint: Please refer to sample code to see how to use kthread_run, kthread_should_stop, kthread_stop, etc.
   // Hint: use ctx_consumer_thread[index] to store the return value of kthread_run
     } else {
    // Input Validation Failed
   PCINFO("Incorrect Input Parameter Configuration Received. No kernel threads started. Please check input parameters.");
   PCINFO("The kernel module expects buffer size (a positive number) and # of producers(0 or 1) and # of consumers > 0");
  }

   return 0;
 }


 static void __exit thread_exit_module(void) {

    if (buffSize > 0)  {

       end_flag = 1;   // tell consumers to stop

       // Wake up threads that may be sleeping so they can exit
       // for (int i = 0; i < prod; i++)  {
            //up(&empty);          // wake producer
             //up(&full);
        //}

       for (int i = 0; i < buffSize + cons + 5; i++) { // for (int i = 0; i < cons; i++) {
             up(&empty);  // wakes a producer (if one is sleeping)
             up(&full);   // wakes a consumer (if one is sleeping)
       }
        
       // Stop producer
       for (int i = 0; i < prod; i++)  {
           if (ctx_producer_thread[i] != NULL && !IS_ERR(ctx_producer_thread[i])) { //if (ctx_producer_thread[i]) { 
               kthread_stop(ctx_producer_thread[i]);  // stop thread
               ctx_producer_thread[i] = NULL;         // delete pointer
         }
       }
       // Stop consumers
       for (int i = 0; i < cons; i++)  {
           if (ctx_consumer_thread[i] != NULL && !IS_ERR(ctx_consumer_thread[i])) {
               kthread_stop(ctx_consumer_thread[i]);  // stop thread
               ctx_consumer_thread[i] = NULL;         // delete pointer
         }
      }
      // SECOND wake (VERY IMPORTANT safety net)
    for (int i = 0; i < buffSize + cons + 5; i++) {
       up(&empty);
       up(&full);
      }
   } 
   PCINFO("CSE330 Project Kernel Module Removed\n");
 }
module_init(thread_init_module);
module_exit(thread_exit_module);



