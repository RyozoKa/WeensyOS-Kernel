#ifndef WEENSYOS_PROCESS_H
#define WEENSYOS_PROCESS_H
#include "lib.h"
#include "x86-64.h"
#if WEENSYOS_KERNEL
#error "process.h should not be used by kernel code."
#endif

// process.h
//
//    Support code for WeensyOS processes.


// SYSTEM CALLS

// sys_getpid
//    Return current process ID.
static inline pid_t sys_getpid(void) {
    pid_t result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_GETPID)
                  : "cc", "memory");
    return result;
}

// sys_yield
//    Yield control of the CPU to the kernel. The kernel will pick another
//    process to run, if possible.
static inline void sys_yield(void) {
    asm volatile ("int %0" : /* no result */
                  : "i" (INT_SYS_YIELD)
                  : "cc", "memory");
}

// sys_fork()
//    Fork the current process. On success, return the child's process ID to
//    the parent, and return 0 to the child. On failure, return -1.
static inline pid_t sys_fork(void) {
    pid_t result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_FORK)
                  : "cc", "memory");
    return result;
}

// sys_exit()
//    Exit this process. Does not return.
static inline void sys_exit(void) __attribute__((noreturn));
static inline void sys_exit(void) {
    asm volatile ("int %0" : /* no result */
                  : "i" (INT_SYS_EXIT)
                  : "cc", "memory");
 spinloop: goto spinloop;       // should never get here
}

// sys_panic(msg)
//    Panic.
static inline pid_t __attribute__((noreturn)) sys_panic(const char* msg) {
    asm volatile ("int %0"  : /* no result */
                  : "i" (INT_SYS_PANIC), "D" (msg)
                  : "cc", "memory");
 loop: goto loop;
}


// sys_brk(addr)
//     change the location of the program break to addr
//     program break defines the end of the process's data segment
//     increasing the program break has the effect of allocating memory to the process
//     decreasing the break deallocates memory
//     on success, returns 0
//     on failure, return -1
//     brk cannot exceed MEMSIZE_VIRTUAL, and cannot be lower than data segment (loaded
//     by the loader)

static inline int sys_brk(const void* addr) {
    static int result;
    asm volatile ("int %1" :  "=a" (result)
                  : "i" (INT_SYS_BRK), "D" /* %rdi */ (addr)
                  : "cc", "memory");
    return result;
}

// sys_sbrk(increment)
//     increment the location of the program break by `increment` bytes
//     program break defines the end of the process's data segment
//     Calling sbrk() with an increment of 0 can be used to find the current location of the program break
//     On success, sbrk() returns the previous program break
//     (If the break was increased, then this value is a pointer to the start of the newly allocated memory)
//      On error, (void *) -1 is returned
static inline void * sys_sbrk(const intptr_t increment) {
    static void * result;
    asm volatile ("int %1" :  "=a" (result)
                  : "i" (INT_SYS_SBRK), "D" /* %rdi */ (increment)
                  : "cc", "memory");
    return result;
}

// sys_thread_start(child_stack)
//    create a new lightweight process with the same address space
//    child_stack is the virtual address of the new process's stack
//    child stack should be page-aligned and in the process space
//    the process is responsible to give an address to a page that is
//    currently in its address space and allocated
//    For correct execution, process must ensure this page is not utilized
//    by anything else
//    On success return process ID to parent, and return 0 to the
//    child. On failure, return -1.
static inline pid_t sys_thread_start(void * child_stack) {
    pid_t result;
    asm volatile ("int %1" : "=a" (result)
                  : "i" (INT_SYS_THREAD_START), "D" /* %rdi */ (child_stack)
                  : "cc", "memory");
    return result;
}

// sys_thread_join(pid)
//     suspend current execution of the calling thread till the thread identified by
//     `pid` terminates. if `pid` is already terminated, then calling thread is
//     immediately runnable.
//     if `pid` is not in the same thread group, then the calling thread is immediately
//     runnable

static inline void sys_thread_join(pid_t pid) {
    asm volatile ("int %0" : /* no result */
                  : "i" (INT_SYS_THREAD_JOIN), "D" /* %rdi */ (pid)
                  : "cc", "memory");
}

// sys_futex
// futex(futex_word, val)
// provides processes a method to wait until a condition becomes true
// futex_word is a pointer to 32-bit integer referred to as a futex word
// val is an integer against which the data pointed by futex_word is compared against
// the kernel guarantees that after this syscall is called, the calling process
// is run again only if *futex_word == val
// In order to share a futex between processes, the futex is placed in a region of
// shared memory, created using thread_start

static inline void sys_futex(int32_t * futex_word, int val) {
    asm volatile ("int %0" : /* no result */
                  : "i" (INT_SYS_FUTEX), "D" /* %rdi */ (futex_word), "S" /* %rsi */ (val)
                  : "cc", "memory");
}

// sys_mem_tog
// toggles kernels printing of memory space for process if pid is its processID
// if pid == 0, toggles state globally (preference to global over local)
static inline void sys_mem_tog(pid_t p) {
    asm volatile ("int %0" : /* no result */
                  : "i" (INT_SYS_MEM_TOG), "D" /* %rdi */ (p)
                  : "cc", "memory");
}

// sys_mapping
// looks up the virtual memory mapping for addr for the current process 
// and stores it inside map. [map, sizeof(vampping)) address should be 
// allocated, writable addresses to the process, otherwise syscall will 
// not write anything to the variable
static inline void sys_mapping(uintptr_t addr, void * map){
    asm volatile ("int %0" : /* no result */
                  : "i" (INT_SYS_MAPPING), "D" /* %rdi */ (map), "S" /* %rsi */ (addr)
                  : "cc", "memory");
}


// OTHER HELPER FUNCTIONS

// app_printf(format, ...)
//    Calls console_printf() (see lib.h). The cursor position is read from
//    `cursorpos`, a shared variable defined by the kernel, and written back
//    into that variable. The initial color is based on the current process ID.
void app_printf(int colorid, const char* format, ...);

// read_line(str, max_size)
//     Calls the syscall read_serial() to read data from the serial port connceted to `input.txt`
//     Do NOT use the syscall directly, it will be incorrect as read_line has an internal datastructure
//     reads max_size number of bytes into str or till newline
//     Note: str should be at least max_size + 1 bytes. The function is designed to write max_size characters
//     +1 for the null terminator = (max_size + 1) total bytes.
int read_line(char * str, int max_size);

#endif
