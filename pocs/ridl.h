#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <ctype.h>

#include "leak_code.h"

#ifndef ITERS
#define ITERS 10000
#endif

#include <sys/prctl.h>
#ifndef PR_SET_SPECULATION_CTRL
#define PR_SET_SPECULATION_CTRL 53
#endif
#ifndef PR_SPEC_DISABLE
#define PR_SPEC_DISABLE 4
#endif

#define SEQ_ACCESS

/**
 * The flags argument
 *     MAP_ANONYMOUS
 *         The mapping is not backed by any file; its contents are
 *         initialized to zero.  The fd argument is ignored; however,
 *         some implementations require fd to be -1 if MAP_ANONYMOUS (or
 *         MAP_ANON) is specified, and portable applications should
 *         ensure this.  The offset argument should be zero.  The use of
 *         MAP_ANONYMOUS in conjunction with MAP_SHARED is supported on
 *         Linux only since kernel 2.4.
 *     
 *     MAP_PRIVATE
 *         Create a private copy-on-write mapping.  Updates to the
 *         mapping are not visible to other processes mapping the same
 *         file, and are not carried through to the underlying file.  It
 *         is unspecified whether changes made to the file after the
 *         mmap() call are visible in the mapped region.
 *     
 *     MAP_POPULATE (since Linux 2.5.46)
 *         Populate (prefault) page tables for a mapping.  For a file
 *         mapping, this causes read-ahead on the file.  This will help
 *         to reduce blocking on page faults later.  MAP_POPULATE is sup‐
 *         ported for private mappings only since Linux 2.6.23. 
 *     
 *     MAP_HUGETLB (since Linux 2.6.32)
 *         Allocate the mapping using "huge pages."  See the Linux kernel
 *         source file Documentation/admin-guide/mm/hugetlbpage.rst for
 *         further information, as well as NOTES, below.
 *
 */

#define MMAP_FLAGS (MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE | MAP_HUGETLB)


/**
 * mmap - map files or devices into memory
 *     #include <sys/mman.h>
 *     void *mmap(void *addr, size_t length, int prot, int flags,
 *                int fd, off_t offset);
 * 
 *     mmap() creates a new mapping in the virtual address space of the
 *     calling process.  The starting address for the new mapping is
 *     specified in addr.  The length argument specifies the length of the
 *     mapping (which must be greater than 0).
 * 
 *     If addr is NULL, then the kernel chooses the (page-aligned) address
 *     at which to create the mapping; this is the most portable method of
 *     creating a new mapping.  If addr is not NULL, then the kernel takes
 *     it as a hint about where to place the mapping; on Linux, the kernel
 *     will pick a nearby page boundary (but always above or equal to the
 *     value specified by /proc/sys/vm/mmap_min_addr) and attempt to create
 *     the mapping there.  If another mapping already exists there, the
 *     kernel picks a new address that may or may not depend on the hint.
 *     The address of the new mapping is returned as the result of the call.
 * 
 *     The contents of a file mapping (as opposed to an anonymous mapping;
 *     see MAP_ANONYMOUS below), are initialized using length bytes starting
 *     at offset offset in the file (or other object) referred to by the
 *     file descriptor fd.  offset must be a multiple of the page size as
 *     returned by sysconf(_SC_PAGE_SIZE).
 * 
 *     After the mmap() call has returned, the file descriptor, fd, can be
 *     closed immediately without invalidating the mapping.
 *
 *     The prot argument describes the desired memory protection of the
 *     mapping (and must not conflict with the open mode of the file).  It
 *     is either PROT_NONE or the bitwise OR of one or more of the following
 *     flags:
 * 
 *     PROT_EXEC  Pages may be executed.
 *     PROT_READ  Pages may be read.
 *     PROT_WRITE Pages may be written.
 *     PROT_NONE  Pages may not be accessed.
 */

// REVIEW +0x80? (void)privatebuf?
/**
 * Allocate size_t results[256], unsigned char reloadbuffer[2 * 256 * 4096], 
 * unsigned char leak[2 * 256 * 4096], unsigned charprivatebuf[128 * 4096].
 * 
 */
#define ALLOC_BUFFERS()\
    __attribute__((aligned(4096))) size_t results[256] = {0};\
    unsigned char *reloadbuffer = (unsigned char *)mmap(NULL, 2*4096*256, PROT_READ | PROT_WRITE, MMAP_FLAGS, -1, 0) + 0x80;\
    unsigned char *leak = mmap(NULL, 2*4096*256, PROT_READ | PROT_WRITE, MMAP_FLAGS, -1, 0);\
    unsigned char *privatebuf = mmap(NULL, 4096*128, PROT_READ | PROT_WRITE, MMAP_FLAGS, -1, 0);\
    (void)privatebuf;

static inline void enable_SSBM() {
        prctl(PR_SET_SPECULATION_CTRL, 0, 8, 0, 0);
}

static inline __attribute__((always_inline)) void enable_alignment_checks() {
    asm volatile(
        "pushf\n"
        "orl $(1<<18), (%rsp)\n"
        "popf\n"
    );
}

static inline __attribute__((always_inline)) void disable_alignment_checks() {
    asm volatile(
        "pushf\n"
        "andl $~(1<<18), (%rsp)\n"
        "popf\n"
    );
}

static inline __attribute__((always_inline)) uint64_t rdtscp(void) {
    uint64_t lo, hi;
    asm volatile("rdtscp\n" : "=a" (lo), "=d" (hi) :: "rcx");
    return (hi << 32) | lo;
}

/**
 * Flush all lines of the reloadbuffer.
 * 
 * @param reloadbuffer reloadbuffer to be flushed
 * 
 */
static inline __attribute__((always_inline)) void flush(unsigned char *reloadbuffer) {
    for (size_t k = 0; k < 256; ++k) {
        #ifdef SEQ_ACCESS
            size_t x = k;
        #else
            size_t x = ((k * 167) + 13) & (0xff);
        #endif

        volatile void *p = reloadbuffer + x * 1024;
        asm volatile("clflush (%0)\n"::"r"(p));
    }
}

/**
 * Update results based on timing of reloads.
 * 
 * @param reloadbuffer reloadbuffer to be flushed
 * @param results counts of timing of reloads exceeding TIME_LIMIT
 * 
 * @todo This random function is awful.
 *     0 3 6 9 12 15 18 21 mod 8
 *     = 0 3 6 1 4 7 2 3 mod 8
 * 
 */
static inline __attribute__((always_inline)) void reload(unsigned char *reloadbuffer, size_t *results) {
    asm volatile("mfence\n");
    for (size_t k = 0; k < 256; ++k) {
        #ifdef SEQ_ACCESS
            size_t x = k;
        #else
            size_t x = ((k * 167) + 13) & (0xff);
        #endif

        unsigned char *p = reloadbuffer + (1024 * x);

        uint64_t t0 = rdtscp();
        *(volatile unsigned char *)p;
        uint64_t dt = rdtscp() - t0;

        if (dt < 160) results[x]++;
    }
}

// REVIEW RECOVERY_RATE
/**
 * Print results if it succeeds RECOVERY_RATE.
 * 
 * @param results results of timing of reloads
 * 
 */
void print_results(size_t *results) {
    for (size_t c = 0; c < 256; ++c) {
        if (results[c] >= ITERS/100) {
            printf("%08zu: %02x (%c)\n", results[c], (unsigned int)c, isprint(c) ? (unsigned int)c : '?');
        }
    }
}
