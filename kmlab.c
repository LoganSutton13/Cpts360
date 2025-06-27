#define LINUX

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/sched.h>
#include "kmlab_given.h"
// Include headers as needed ...
void show_list(void);
void timer_callback(struct timer_list *timer);
int kmlab_init(void);
void kmlab_exit(void);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sutton");
MODULE_DESCRIPTION("CPTS360 KM PA");

#define DEBUG 1
#define PROCFS_MAX_SIZE 1024
#define PROCFS_NAME "status"
#define PROCDIR_NAME "kmlab"

// store per process data (you may modify it as needed)
typedef struct {
    struct list_head list;
    unsigned int pid;
    unsigned long cpu_time;
} proc_list;

// globals
LIST_HEAD(process_list);

static struct timer_list timer;

static struct workqueue_struct *queue = NULL;
static struct work_struct work;

static DEFINE_SPINLOCK(spinlock);

int time_interval = 5000;

static char procfs_buffer[PROCFS_MAX_SIZE] = "";

static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_file;

static unsigned long procfs_buffer_size = 0;

// used to show the contents of the list
void show_list(void)
{
   proc_list *entry = NULL;
   unsigned long flags;

   // check if the list is empty
   if (list_empty(&process_list)) 
   {
      //pr_info("List is empty!\n");
      return;
   }

   // list each entry
   spin_lock_irqsave(&spinlock, flags);
   list_for_each_entry(entry, &process_list, list) 
   {
      pr_info("PID: %d\tCPU: %lu\n", entry->pid, entry->cpu_time);
   }
   spin_unlock_irqrestore(&spinlock, flags);
}

// what to do at each timer interval
void timer_callback(struct timer_list *timer)
{
   // do what needs to be done at each interval

   // if the list isn't empty, we should schedule work
   if(!list_empty(&process_list))
   {
      // schedule the work to be done
      schedule_work(&work);
   }

   // periodic timer
   show_list();
   //pr_info("timer updated\n");
   mod_timer(timer, jiffies + msecs_to_jiffies(time_interval));
}

// work function
static void work_function(struct work_struct *work)
{
   unsigned long flags;
   proc_list *entry, *tmp;

   // update the cpu time

   // spin lock
   spin_lock_irqsave(&spinlock, flags);
   list_for_each_entry_safe(entry, tmp, &process_list, list)
   {
      if(get_cpu_use(entry->pid, &entry->cpu_time) == -1)
      {
         // remove process from list
         list_del(&entry->list);
         kfree(entry);
      }
   }
   // spin unlock
   spin_unlock_irqrestore(&spinlock, flags);
}

// called when procfile is read
static ssize_t procfile_read(struct file *file_pointer, char __user *buffer,
                             size_t buffer_length, loff_t *offset)
{  
   int max_size = 48;
   size_t list_size = 0, total_size = 0, bytes_used = 0;
   char * my_buffer;
   unsigned long flags;

   // check if offset exceeds buffer length
   if (*offset >= buffer_length || *offset) 
   {
      return 0;
   }

   // determine list size
   proc_list *entry; 
   spin_lock_irqsave(&spinlock, flags);
   list_for_each_entry(entry, &process_list, list)
   {
      list_size++;
   }
   spin_unlock_irqrestore(&spinlock, flags);

   total_size = max_size * list_size;

   // allocate the memory for my_buffer
   my_buffer = kmalloc(total_size, GFP_KERNEL);
   if (!my_buffer) 
   {
      pr_info("Buffer memory allocation failed\n");
      return 1;
   }

   // add the pid and cpu_time for each entry
   entry = NULL;
   spin_lock_irqsave(&spinlock, flags);
   list_for_each_entry(entry, &process_list, list)
   {
      // check that we are under the total size limit
      if(bytes_used >= total_size)
      {
         break;
      }
      // copy the pid and cpu time into the buffer, track how many bytes we've used
      bytes_used += scnprintf(my_buffer + bytes_used, total_size - bytes_used,
         "%d : %lu\n", entry->pid, entry->cpu_time);
   }
   spin_unlock_irqrestore(&spinlock, flags);

   if (bytes_used >= buffer_length) 
   {
      bytes_used = buffer_length;
   }

   if (copy_to_user(buffer, my_buffer, bytes_used)) 
   {
      pr_info("copy_to_user failed\n");

      // free the buffer mem
      kfree(my_buffer);
      return -EFAULT;
   } 
   else 
   {
      pr_info("procfile read %s\n", PROCFS_NAME);

      // free the buffer mem
      kfree(my_buffer);
      *offset += bytes_used;
      return bytes_used;
   }
}

// called when procfile is written to
static ssize_t procfile_write(struct file *file, const char __user *buff,
                              size_t len, loff_t *off)
{
   int pid;
   int result_of_str_to_int;

	// clear internal buffer
	memset(&procfs_buffer[0], 0, sizeof(procfs_buffer));
	
   procfs_buffer_size = len;
   if (procfs_buffer_size > PROCFS_MAX_SIZE)
      procfs_buffer_size = PROCFS_MAX_SIZE;

   if (copy_from_user(procfs_buffer, buff, procfs_buffer_size))
      return -EFAULT;
   
   procfs_buffer[procfs_buffer_size & (PROCFS_MAX_SIZE - 1)] = '\0';
   *off += procfs_buffer_size;

   // convert the string to an int
   if((result_of_str_to_int = kstrtoint(procfs_buffer, 10, &pid)))
   {
     pr_info("Failed to convert string to int\n");
     return result_of_str_to_int;
   }

   // allocate memory
   proc_list *new_node = kmalloc((size_t) (sizeof(proc_list)), GFP_KERNEL);

   if (!new_node) 
   {
      pr_info("Memory allocation failed\n");
      return 1;
   }

   // initialize values 
   new_node->pid = pid;
   new_node->cpu_time = 0;

   pr_info("%s created\n", procfs_buffer);
   list_add_tail(&(new_node)->list, &process_list);
   //show_list();
   return procfs_buffer_size;
}

// proc operations struct
static const struct proc_ops proc_file_fops = {
    .proc_read = procfile_read,
    .proc_write = procfile_write,
};


// kmlab_init - Called when the module is loaded
int __init kmlab_init(void)
{
   #ifdef DEBUG
   pr_info("KMLAB MODULE LOADING\n");
   #endif

   // set up proc fs
   // make the dir
   proc_dir = proc_mkdir(PROCDIR_NAME, NULL);
   if (NULL == proc_dir) {
      pr_alert("Error:Could not initialize /proc/%s\n", PROCDIR_NAME);
      return -ENOMEM;
   }

   // make the file
   proc_file = proc_create(PROCFS_NAME, 0666, proc_dir, &proc_file_fops);
   if (NULL == proc_file) {
      pr_alert("Error:Could not initialize /proc/kmlab/%s\n", PROCFS_NAME);
      return -ENOMEM;
   }

   pr_info("/proc/%s created\n", PROCDIR_NAME);
   pr_info("/proc/kmlab/%s created\n", PROCFS_NAME);
   // end proc fs set up


   // start timer setup
   timer_setup(&timer, timer_callback, 0);
   mod_timer(&timer, jiffies + msecs_to_jiffies(time_interval));
   pr_info("Timer setup complete\n");
   // end timer setup

   queue = alloc_workqueue("kmlab_wq", WQ_UNBOUND, 1);
   INIT_WORK(&work, work_function);

   pr_info("KMLAB MODULE LOADED\n");
   return 0;   
}

// kmlab_exit - Called when the module is unloaded
void __exit kmlab_exit(void)
{
   #ifdef DEBUG
   pr_info("KMLAB MODULE UNLOADING\n");
   #endif
   // Insert your code here ...

   del_timer(&timer);
   // destroy the workqueue
   destroy_workqueue(queue);

   // clean up list entries, need to free allocated mem
   proc_list *entry, *tmp;
   list_for_each_entry_safe(entry, tmp, &process_list, list) 
   {
        list_del(&entry->list);
        kfree(entry);
   }

   // remove proc file and dir
   proc_remove(proc_file);
   proc_remove(proc_dir);
   pr_info("KMLAB MODULE UNLOADED\n");
}

// Register init and exit functions
module_init(kmlab_init);
module_exit(kmlab_exit);
