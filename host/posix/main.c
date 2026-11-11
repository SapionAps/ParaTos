#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>

#include "common.h"
#include "gemdos.h"
#include "loader.h"
#include "cookiejar.h"
#include "runtime.h"
#include "host.h"

extern char **environ;

static sigjmp_buf return_from_signal_handler;
void handle_segv(int signum, siginfo_t * info, void * p)
{
	uintptr_t offset = (uintptr_t)info->si_addr - (uintptr_t)memory;
	printf("SIGSEGV Addr: %p (ST mem: %lx)\n", info->si_addr, (unsigned long)offset);
	siglongjmp(return_from_signal_handler, signum);
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "usage: %s <program.tos> [arguments...]\n", argv[0]);
		return 1;
	}

	FindExecutable();
	InitM68KMemory();
	InitMemory();
	InitCookieJar();

	current_process = LoadExe(argv[1], argv+1, argc-1, environ);
	if (current_process == 0)
	{
		fprintf(stderr, "error: cannot load %s.\n", argv[1]);
		return 1;
	}

	InitGuestCPU(current_process);

	struct sigaction act_segv;
	struct sigaction old_segv;
	act_segv.sa_sigaction = handle_segv;
	act_segv.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &act_segv, &old_segv);
	for (;;)
	{
		int ret = sigsetjmp(return_from_signal_handler, 1);
		if (!ret)
		{
			m68k_execute(10000);
		}
		else
		{
			printf("longjmp from exception handler returned %d\n", ret);
			// Handle return from signal handler
			if (ret == SIGSEGV)
			{
				sigaction(SIGSEGV, &old_segv, NULL);
				DumpGuestFault();
				exit(-1);
				// m68ki_exception_address_error();
			}
		}
	}

	return 0;
}
