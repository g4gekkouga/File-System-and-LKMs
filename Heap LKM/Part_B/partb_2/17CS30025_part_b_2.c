// Programming Assignment 1 (Part B) - Loadable Heap Kernel Module
// Member 1 : P Amshumaan Varma - 17CS30025
// Member 2 : N Nikhil Thatha - 17CS10054

// Both read(), write() and ioctl() are present in this file. So we are submitting the same code for both part 1 and 2.
// This will work for both type of interaction from the userspace i.r ioctl and read-write functions.

#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
//#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");

#define PB2_SET_TYPE _IOW(0x10, 0x31, int32_t*)
#define PB2_INSERT _IOW(0x10, 0x32, int32_t*)
#define PB2_GET_INFO _IOR(0x10, 0x33, int32_t*)
#define PB2_EXTRACT _IOR(0x10, 0x34, int32_t*)

#define MIN_HEAP 0
#define MAX_HEAP 1

#define MAX 101
#define MIN -101

// Defined data structure

typedef struct pb2_set_type_arguments {
	int32_t heap_size; // size of the heap
	int32_t heap_type; // heap type: 0 for min-heap, 1 for max-heap
} pb2_set_type_arguments;

typedef struct obj_info {
	int32_t heap_size; // size of the heap
	int32_t heap_type; // heap type: 0 for min-heap, 1 for max-heap
	int32_t root; // value of the root node of the heap (null if size is 0).
	int32_t last_inserted; // value of the last element inserted in the heap.
} obj_info;

typedef struct result {
	int32_t result; // value of min/max element extracted.
	int32_t heap_size; // size of the heap after extracting.
} result;

// Heap Data Structure

typedef struct heapS
{
	int32_t *data; // Array Implementation of Heap
	int32_t minOrmax; // heap type: 0 for min-heap, 1 for max-heap
	int32_t size; // Maximum size of the heap
	int32_t end; // index of last occupied place in heap 
	int32_t hist; // store last inserted value
} heapS;

// Data Structure for PCB implementation for implementing locks and synchronization

typedef struct pcb_{
    pid_t proc_pid; // Stores the processID of the owner process. 
    heapS* heap; // Heap for that process
    struct pcb_* next; // Stores link to next Block in the List
}pcb;

// Function Declarations

static pcb* pcb_node_Create(pid_t pid);
static void pcb_node_Reset(pcb* node);
static pcb* pcb_list_Insert(pid_t pid);
static pcb* pcb_list_Get(pid_t pid);
static int pcb_list_Delete(pid_t pid);

obj_info* obj_info_Create(int32_t deg1, int32_t deg2, int32_t deg3, int32_t deg4);
result* result_Create(int32_t deg1, int32_t deg2);

static heapS* init_heap(int32_t size, int32_t type);
static void free_heap(heapS *heap);
static void get_heap_info(heapS *heap, obj_info* info);
static void extract_heap_top(heapS *heap, result* res);
static void heap_insert(heapS *heap, int32_t val);
static int32_t extract_top(heapS *heap);
static void bubble_up(heapS *heap, int32_t position);
static void bubble_down(heapS *heap, int32_t position);
static int32_t parent(int32_t position);
static int32_t left(int32_t position);
static int32_t right(int32_t position);

static long ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int open(struct inode *inodep, struct file *filep);
static int release(struct inode *inodep, struct file *filep);
static ssize_t write(struct file *file, const char *buf, size_t count, loff_t *pos);
static ssize_t read(struct file *file, char *buf, size_t count, loff_t *pos);

static struct file_operations file_ops;
static DEFINE_MUTEX(pcb_mutex);
static pcb* pcb_Head = NULL;

// Create pcb node

static pcb* pcb_node_Create(pid_t pid) {
    pcb* node = (pcb*)kmalloc(sizeof(pcb), GFP_KERNEL);

    node->proc_pid = pid;
    node->heap = NULL;
    node->next = NULL;
    return node;
}

// clear the data in pcb node

static void pcb_node_Reset(pcb* node) {
    free_heap(node->heap);
    return;
}

// Insert pcb of new process

static pcb* pcb_list_Insert(pid_t pid) {
    pcb* node = pcb_node_Create(pid);

    node->next = pcb_Head;
    pcb_Head = node;
    return node;
}

// Find pcb of respective process

static pcb* pcb_list_Get(pid_t pid) {
    pcb* tmp = pcb_Head;

    while(tmp){
        if (tmp->proc_pid == pid) {
            return tmp;
        }
        tmp = tmp->next;
    }
    return NULL;
}

// Remove a process

static int pcb_list_Delete(pid_t pid) {
    pcb* prev = NULL;
    pcb* curr = NULL;

    if (pcb_Head->proc_pid == pid) {
        curr = pcb_Head;
        pcb_Head = pcb_Head->next;
        kfree(curr);
        return 0;
    }

    prev = pcb_Head;
    curr = prev->next;
    while(curr) {
        if (curr->proc_pid == pid) {
            prev->next = curr->next;
            kfree(curr);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    return -1;
}

// Initialize default return objects for GET_INFO and GET_RESULT

obj_info* obj_info_Create(int32_t deg1, int32_t deg2, int32_t deg3, int32_t deg4) {

    obj_info* node = (obj_info*)kmalloc(sizeof(obj_info), GFP_KERNEL);

    node->heap_size = deg1;
    node->heap_type = deg2;
    node->root = deg3;
    node->last_inserted = deg4;

    return node;
}

result* result_Create(int32_t deg1, int32_t deg2) {

    result* node = (result*)kmalloc(sizeof(result), GFP_KERNEL);

    node->result = deg1;
    node->heap_size = deg2;

    return node;
}

// Initialize heap

static heapS* init_heap(int32_t size, int32_t type)
{
	heapS *heap = (heapS *)kmalloc(sizeof(heapS), GFP_KERNEL);
	if (unlikely(heap == NULL)) {
		return heap;
	}

	heap->data = (int32_t *)kmalloc(sizeof(int32_t) * (size + 1), GFP_KERNEL);
	if (unlikely(heap->data == NULL)) {
		kfree(heap);
		return heap;
	}
	heap->size = size;
	heap->end = 0;
	heap->minOrmax = type;
	heap->data[heap->end] = (heap->minOrmax == MIN_HEAP ? MAX : MIN);
	return heap;
}

// Clear heap

static void free_heap(heapS *heap)
{
	kfree(heap->data);
	kfree(heap);
}

// Read heap information into info object

static void get_heap_info(heapS *heap, obj_info* info) {
	if (heap->end <= 0) {
		printk(KERN_ALERT "ioctl() - Error getting Heap Info as Empty Heap\n");
		return;
	}

	info->heap_size = heap->end;
	info->heap_type = heap->minOrmax;
	info->root = heap->data[1];
	info->last_inserted = heap->hist;
}

// Read heap top into result structure and remove top element

static void extract_heap_top(heapS *heap, result* res) {
	if (heap->end <= 0) {
		printk(KERN_ALERT "ioctl() - Error getting Heap Info as Empty Heap\n");
		return;
	}

	res->result = extract_top(heap);
	res->heap_size = heap->end;
}

// Insert element into the heap

static void heap_insert(heapS *heap, int32_t val)
{
	++heap->end;
	heap->data[heap->end] = val;
	heap->hist = val;
	bubble_up(heap, heap->end);
}

// Remove top element in heap and return that value

static int32_t extract_top(heapS *heap)
{
	int32_t top;
	if (heap->end == 0)
		return -EACCES;
	top = heap->data[1];
	heap->data[1] = heap->data[heap->end]; 
	heap->data[heap->end] = (heap->minOrmax == MIN_HEAP ? MAX : MIN);
	--heap->end;
	bubble_down(heap, 1);

	return top;
}

// Heapify functions to adjust the heap after insertion at leaf node

static void bubble_up(heapS *heap, int32_t position)
{

	if (position == 1)
		return;

	// Check min or max heap and then accordingly
	else if ((heap->minOrmax == MIN_HEAP && heap->data[parent(position)] > heap->data[position]) ||
			 (heap->minOrmax == MAX_HEAP && heap->data[parent(position)] < heap->data[position]))
	{
		int32_t parentIndex = parent(position);
		int32_t parent = heap->data[parentIndex];
		heap->data[parentIndex] = heap->data[position];
		heap->data[position] = parent;
		bubble_up(heap, parentIndex);
		return;
	}
}

static void bubble_down(heapS *heap, int32_t position)
{
	// base case
	if ((position >= heap->end) || (left(position) > heap->end && right(position) > heap->end) )
	{
		return;
	}
	// 1 child case
	// Check min or max heap and then accordingly
	else if (left(position) <= heap->end && right(position) > heap->end)
	{
		if ((heap->minOrmax == MIN_HEAP && heap->data[position] > heap->data[left(position)]) || 
		    (heap->minOrmax == MAX_HEAP && heap->data[position] < heap->data[left(position)])) // violation of heap property
		{
			int32_t temp_child = heap->data[left(position)];
			heap->data[left(position)] = heap->data[position];
			heap->data[position] = temp_child;
		}
		return;
	}
	else // normal case, both children non-void 
	{	
		// Check min or max heap and then accordingly
		if (heap->minOrmax == MIN_HEAP && (heap->data[position] > heap->data[left(position)]
				|| heap->data[position] > heap->data[right(position)])) // violation of heap property 
		{
			int32_t childIndex = (heap->data[left(position)] < heap->data[right(position)] ? left(position) : right(position));
			int32_t child = heap->data[childIndex];
			heap->data[childIndex] = heap->data[position];
			heap->data[position] = child;
			bubble_down(heap, childIndex); // update position to reflect swapped node's location 
		}
		if (heap->minOrmax == MAX_HEAP && (heap->data[position] < heap->data[left(position)]
				|| heap->data[position] < heap->data[right(position)])) // violation of heap property 
		{
			int32_t childIndex = (heap->data[left(position)] > heap->data[right(position)] ? left(position) : right(position));
			int32_t child = heap->data[childIndex];
			heap->data[childIndex] = heap->data[position];
			heap->data[position] = child;
			bubble_down(heap, childIndex); // update position to reflect swapped node's location 
		}
	}
}

// Helper functions to get required nodes in the heap

static int32_t parent(int32_t position) {
	return position/2;
}

static int32_t left(int32_t position) {
	return 2*position;
}

static int32_t right(int32_t position) {
	return 2*position + 1;
}

// Open file and create pcb for that

static int open(struct inode *inodep, struct file *filep) {
    pid_t pid = current->pid;

    while(!mutex_trylock(&pcb_mutex));

    if (pcb_list_Get(pid)) {
        mutex_unlock(&pcb_mutex);
        printk(KERN_ERR "open() - File already Open in Process: %d\n", pid);
        return -EACCES;
    }

    pcb_list_Insert(pid);
    mutex_unlock(&pcb_mutex);

    printk(KERN_NOTICE "open() - File Opened by Process: %d\n", pid);

    return 0;
}

// Close a file and delete respective pcb

static int release(struct inode *inodep, struct file *filep) {
    pid_t pid = current->pid;
    pcb* curr_proc = NULL;

    while (!mutex_trylock(&pcb_mutex));
    curr_proc = pcb_list_Get(pid);
    mutex_unlock(&pcb_mutex);

    if (curr_proc == NULL) {
        printk(KERN_ERR "close() - Process: %d has not Opened File\n", pid);
        return -EACCES;
    }

    pcb_node_Reset(curr_proc);

    while (!mutex_trylock(&pcb_mutex));
    pcb_list_Delete(pid);
    mutex_unlock(&pcb_mutex);

    printk(KERN_NOTICE "close() - File Closed by Process: %d\n", pid);

    return 0;
}

// IOCTL Function 

static long ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	pid_t pid = current->pid;
    pcb* curr_proc = NULL;
    pb2_set_type_arguments* arguments = NULL;
    obj_info* info = NULL;
    result* res = NULL;
    int8_t error = 0;
    int32_t* insert_val = NULL;
    int8_t return_value = 0;
    unsigned char* buffer = NULL;

    // Wait and Acquire lock for synchronization

    while (!mutex_trylock(&pcb_mutex));
    curr_proc = pcb_list_Get(pid);
    mutex_unlock(&pcb_mutex);

    if (curr_proc == NULL) {
        printk(KERN_ERR "ioctl() - Process: %d has not Opened File\n", pid);
        return -EACCES;
    }

    // Buffer to either read from user or write to user

    buffer = (unsigned char*)kcalloc(256, sizeof(unsigned char), GFP_KERNEL);

    // Initialize structures for different requests

    info = obj_info_Create(0, 0, NULL, NULL);
    res = result_Create(NULL, 0);

    switch (cmd) {

    	// Initialize heap

    	case PB2_SET_TYPE: {	

    		if (copy_from_user(buffer, (unsigned char *)arg, sizeof(pb2_set_type_arguments))) {
    			printk(KERN_ALERT "ioctl() - Copy from user error in Process: %d\n", pid);
                error = -EINVAL;
                break;
            }

            arguments = (pb2_set_type_arguments *)buffer;

            // Error in input datatype
            if (arguments == NULL) {
                printk(KERN_ALERT "ioctl() - Invalid Heap -NULL- in Process: %d\n", pid);
                error = -EINVAL;
                break;
            }

            // Initializing heap with max size 0
            if (arguments->heap_size <= 0) {

            	printk(KERN_ALERT "ioctl() - Invalid Heap Size in Process: %d\n", pid);
            	error = -EINVAL;
                break;
            }

            // Invalid heap-type argument
            if (arguments->heap_type != 0 && arguments->heap_type != 1) {
            	printk(KERN_ALERT "ioctl() -  Invalid Heap Type in Process: %d\n", pid);
            	error = -EINVAL;
                break;
            }

            // I already initialized a heap, clear it first
            if (curr_proc->heap != NULL) {
            	pcb_node_Reset(curr_proc);
            }

            // Initialize a heap
			curr_proc->heap = init_heap(arguments->heap_size, arguments->heap_type);

            if (error < 0) {
                break;
            }
            break;

    	}

    	// Insert a value into the heap

    	case PB2_INSERT: {

    		if (copy_from_user(buffer, (unsigned char *)arg, sizeof(int32_t))) {
                printk(KERN_ALERT "ioctl() - Copy from user error in Process: %d\n", pid);
                error = -EINVAL;
                break;
            }

            // Heap not initialized yet
    		if (curr_proc->heap == NULL) {
                printk(KERN_ALERT "ioctl() - PB2_SET_TYPE not called by Process: %d\n", pid);
                error = -EACCES;
                break;
            }

         //    if (copy_from_user(insert_val, (int32_t *)arg, sizeof(int32_t))) {
         //    	printk(KERN_ALERT "ioctl() - Copy from user error in Process: %d\n", pid);
	        //     error = -EINVAL;
	        //     break;
	        // }

            // Heap is Full
            if (curr_proc->heap->end >= curr_proc->heap->size) {
            	printk(KERN_ALERT "ioctl() - Heap is Full in Process: %d\n", pid);
                error = -EACCES;
                break;
            }

            insert_val = (int32_t *)buffer;         

	        heap_insert(curr_proc->heap, *insert_val);

	        if (error < 0) {
                break;
            }
            break;
    	}

    	// Read heap info into an object info

    	case PB2_GET_INFO: {

    		if (curr_proc->heap == NULL) {
                printk(KERN_ALERT "ioctl() - PB2_SET_TYPE not called by Process: %d\n", pid);
                error = -EACCES;
                break;
            }

            printk(KERN_ALERT "ioctl() - Process: %d Getting Heap Info\n", pid);

            get_heap_info(curr_proc->heap, info);

            if (info->root == (int32_t)NULL) {
                error = -EINVAL;
            }

            if (copy_to_user((obj_info*)arg, info, sizeof(obj_info))) {
                printk(KERN_ALERT "ioctl() - Copy to user error in Process: %d\n", pid);
                error = -ENOBUFS;
                break;
            }

            break;
    	}

    	// Extract heap top and resultant heap size

    	case PB2_EXTRACT : {

    		if (curr_proc->heap == NULL) {
                printk(KERN_ALERT "ioctl() - PB2_SET_TYPE not called by Process: %d\n", pid);
                error = -EACCES;
                break;
            }

            printk(KERN_ALERT "ioctl() - Process: %d Extracting Heap Top\n", pid);

            extract_heap_top(curr_proc->heap, res);

            // If empty heap
            if (res->result == (int32_t) NULL) {
                error = -EINVAL;
            }

            if (copy_to_user((result*)arg, res, sizeof(result))) {
                printk(KERN_ALERT "ioctl() - Copy to user error in Process: %d\n", pid);
                error = -ENOBUFS;
                break;
            }

            break;
    	}

    	default: {
            printk(KERN_ALERT "ioctl() - Invalid Command in Process: %d\n", pid);
            error = -EINVAL;
            break;
        }
    }

    // Free memory
    kfree(buffer);
    kfree(res);
    kfree(info);

    if (error < 0) {
        return error;
    }

    return return_value;
}

// WRITE Fucntion

static ssize_t write(struct file *file, const char *buf, size_t count, loff_t *pos) {
    pid_t pid = current->pid;
    pcb* curr_proc = NULL;
//  static unsigned char buffer[256] = {0};
    unsigned char* buffer = NULL;
    int8_t error = 0;
    int type = 0; 
    int32_t size = 0; 

    if (buf == NULL || count == 0) {
        return -EINVAL;
    }

    // Wait and Acquire lock for synchronization

    while (!mutex_trylock(&pcb_mutex));
    curr_proc = pcb_list_Get(pid);
    mutex_unlock(&pcb_mutex);

    if (curr_proc == NULL) {
        printk(KERN_ERR "write() - Process: %d has not Opened File\n", pid);
        return -EACCES;
    }

    // error write by user program

    if (count <= 0) {
    	printk(KERN_ERR "write() - Process: %d Error Count Value\n", pid);
        return -EACCES;
    }

    buffer = (unsigned char*)kcalloc(256, sizeof(unsigned char), GFP_KERNEL);
    if (copy_from_user(buffer, buf, count < 256 ? count:256)) {
    	printk(KERN_ERR "write() - Process: %d Error in copy from user\n", pid);
        return -ENOBUFS;
    }

    // error write by user program

    if (buffer == NULL) {
        printk(KERN_ERR "write() - Process: %d Error as Buffer NULL\n", pid);
        return -ENOBUFS;
    }

    // Heap uninitialized - Then use the info to initialize heap
    if (curr_proc->heap == NULL) {
    	// Not sufficient info to initialize heap
    	if (count < 2) {
    		printk(KERN_ERR "write() - Process: %d Error in initialization\n", pid);
        	return -EACCES;
    	}

	    
        type = (int)(buffer[0]);
        size = (int32_t)(buffer[1]);

        // Error in heap type value
	    if (type != 240 && type != 255) {
	    	printk(KERN_ERR "write() - Process: %d Error in Type Value : %d , Size : %d\n", pid, type, (int)size);
        	return -EINVAL;
	    }

	    // Error in heap size specification
	    if (size < 1 || size > 100) {
	    	printk(KERN_ERR "write() - Process: %d Error in Insert Value\n", pid);
        	return -EINVAL;
	    }

	    // Initialize heap
	    curr_proc->heap = init_heap(size, type == 255 ? 0 : 1);
	    printk(KERN_ERR "write() - Process: %d Heap Initialized\n", pid);
    } 

    // If heap already present, Insert the specified value
    else {
    	int32_t ins_val = *((int32_t *)buffer);
    	if (curr_proc->heap->end >= curr_proc->heap->size) {
    		printk(KERN_ERR "write() - Process: %d Heap FULL, cannot Insert\n", pid);
        	return -EACCES;
    	}
    	heap_insert(curr_proc->heap, ins_val);
    	printk(KERN_ERR "write() - Process: %d Value Inserted in Heap\n", pid);
    }

    kfree(buffer);

    if (error < 0) {
        return error;
    }
    return count < 256 ? count:256;
}

// READ Function

static ssize_t read(struct file *file, char *buf, size_t count, loff_t *pos) {
    pid_t pid = current->pid;
    pcb* curr_proc = NULL;
    unsigned char* buffer = NULL;
    int8_t error = 0;
    size_t bytes = 0;
    int32_t* top_val = NULL;

    if (buf == NULL || count == 0) {
        return -EINVAL;
    }

    // Wait and Acquire lock for synchronization

    while (!mutex_trylock(&pcb_mutex));
    curr_proc = pcb_list_Get(pid);
    mutex_unlock(&pcb_mutex);

    if (curr_proc == NULL) {
        printk(KERN_ERR "read() - Process: %d has not Opened File\n", pid);
        return -EACCES;
    }


    if (curr_proc->heap == NULL) {
        printk(KERN_ERR "read() - Process: %d Heap not Initialized\n", pid);
        return -EACCES;
    }

    // If empty heap
    if (curr_proc->heap->end <= 0) {
        printk(KERN_ERR "read() - Process: %d Heap Empty\n", pid);
        return -EACCES;
    }

    
    top_val = (int32_t*)kmalloc(sizeof(int32_t), GFP_KERNEL);
    *top_val = extract_top(curr_proc->heap);
    buffer = (unsigned char*)top_val;
    bytes = sizeof(int32_t);

    if (error < 0) {
        return error;
    }

    if (copy_to_user(buf, buffer, count)) {
    	printk(KERN_ERR "read() - Process: %d Error in copy to user\n", pid);
        error = -ENOBUFS;
    }
    kfree(buffer);

    if (error < 0) {
        return error;
    }
    return bytes;
}

// Default load and release module functions

static int load_module(void)
{
    struct proc_dir_entry *entry = proc_create("partb_2_17CS30025", 0, NULL, &file_ops);
    if(entry == NULL) {
        return -ENOENT;
    }

    file_ops.owner = THIS_MODULE;
    file_ops.write = write;
    file_ops.read = read;
    file_ops.open = open;
    file_ops.release = release;
    file_ops.unlocked_ioctl = ioctl;

    mutex_init(&pcb_mutex);
    printk(KERN_ALERT "Heap Module is Loaded\n");

    return 0;
}

static void release_module(void)
{
    remove_proc_entry("partb_2_17CS30025", NULL);

    mutex_destroy(&pcb_mutex);
    printk(KERN_ALERT "Heap Module is Removed\n");
}

module_init(load_module);
module_exit(release_module);
