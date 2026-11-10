/* Native pTOS test driver. The kernel launches a new native Paratos process
 * for each case; that process runs an ordinary, compiled 68k TOS program. */
#include <stdint.h>
#include <stdio.h>
#include <mint/osbind.h>
#include <mint/basepage.h>

static const char cli_env[] = "DEMO=present\0ARGV=NULL:3\0C:\\PARATOS.PRG\0"
	"C:/CLI.TOS\0two words\0 \0";
static const char semantics_env[] = "DEMO=present\0ARGV=\0C:\\PARATOS.PRG\0C:\\SEMANTIC.TOS\0";
static const char read_fault_env[] = "DEMO=present\0ARGV=\0C:\\PARATOS.PRG\0C:\\FAULT.TOS\0read\0";
static const char write_fault_env[] = "DEMO=present\0ARGV=\0C:\\PARATOS.PRG\0C:\\FAULT.TOS\0write\0";

static int userspace(void)
{
#if defined(__arm__)
	uint32_t status;
	__asm__ volatile("mrs %0, cpsr" : "=r"(status));
	return (status & 0x1f) == 0x10;
#elif defined(__x86_64__)
	uint16_t selector;
	__asm__ volatile("mov %%cs, %0" : "=r"(selector));
	return (selector & 3) == 3;
#else
#error Unsupported native test architecture
#endif
}

static long launch(int mode, const char *environment)
{
	char tail[128] = {127, 0};
	if (!mode) return Pexec(0, "C:\\PARATOS.PRG", tail, environment);
	BASEPAGE *bp = (BASEPAGE *)Pexec(3, "C:\\PARATOS.PRG", tail, environment);
	if ((long)bp <= 0) return (long)bp;
	if ((uintptr_t)bp->p_tbase < 0x41000000UL || (uintptr_t)bp->p_tbase >= 0x42000000UL ||
		(uintptr_t)bp < 0x01000000UL || (uintptr_t)bp->p_env < 0x01000000UL)
	{
		puts("pTOS Paratos kernel integration: FAIL (native layout)");
		return -1;
	}
	void *env = bp->p_env;
	long result = Pexec(mode, "", bp, 0);
	if (mode == 4)
	{
		/* PE_GO lends the image and returns it as a Malloc block. */
		if (Mfree((void *)0x41000000UL) || Mfree(env) || Mfree(bp))
			return -1;
	}
	return result;
}

int main(void)
{
	static const struct { const char *name, *env; long status; } cases[] = {
		{"CLI arguments/environment/files", cli_env, 37},
		{"privileges/overlays/native Pexec", semantics_env, 0},
		{"native-image guest read fault", read_fault_env, 255},
		{"partial low-memory guest write fault", write_fault_env, 255}
	};
	static const int modes[] = {0, 4, 6};
	puts("pTOS Paratos kernel integration: BEGIN");
	if (!userspace())
	{
		puts("pTOS Paratos kernel integration: FAIL (native privilege)");
		return 1;
	}
	for (int round = 0; round < 2; round++)
		for (unsigned int mode = 0; mode < sizeof(modes) / sizeof(modes[0]); mode++)
			for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
			{
				long result = launch(modes[mode], cases[i].env);
				printf("round %d mode %d %s: %ld\n", round + 1, modes[mode], cases[i].name, result);
				if (result != cases[i].status)
				{
					puts("pTOS Paratos kernel integration: FAIL (exit status or cleanup)");
					return 1;
				}
			}
	puts("pTOS Paratos kernel integration: PASS");
	return 0;
}
