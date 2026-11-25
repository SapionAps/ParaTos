/* Native pTOS test driver. Boot-configured binfmt/shebang handlers launch a
 * new native Paratos process for each ordinary compiled 68k TOS program. */
#include <stdint.h>
#include <stdio.h>
#include <mint/osbind.h>
#include <mint/mintbind.h>

static char *const demo_env[] = {"DEMO=present", NULL};
#if defined(__arm__)
static char *const vdi_env[] = {"VDI=kernel", NULL};
#elif defined(PTOS_TEST_VDI_AVAILABLE) && PTOS_TEST_VDI_AVAILABLE
static char *const vdi_env[] = {"VDI=required", NULL};
#else
static char *const vdi_env[] = {"VDI=absent", NULL};
#endif
static char *const cli_args[] = {"C:/CLI.TOS", "two words", "", NULL};
static char *const semantics_args[] = {"C:\\SEMANTIC.TOS", NULL};
static char *const filesystem_args[] = {"C:\\FSCOMP.TOS", NULL};
static char *const vdi_args[] = {"C:\\VDI.TOS", NULL};
static char *const read_fault_args[] = {"C:\\FAULT.TOS", "read", NULL};
static char *const write_fault_args[] = {"C:\\FAULT.TOS", "write", NULL};
static char *const script_args[] = {"C:/SCRIPT.SH", "two words", "", NULL};
static char *const explicit_args[] = {"C:\\BIN\\PARATOS.PRG", "C:/CLI.TOS", "two words", "", NULL};

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

int main(void)
{
#ifdef PTOS_TEST_EXEC_REPLACEMENT
	enum { semantics_status = 24 };
#else
	enum { semantics_status = 0 };
#endif
	static const struct { const char *name, *path; char *const *argv, *const *envp; long status; } cases[] = {
		{"CLI arguments/environment/files", "C:\\CLI.TOS", cli_args, demo_env, 37},
		{"privileges/overlays/native and guest Pexec", "C:\\SEMANTIC.TOS", semantics_args, demo_env, semantics_status},
		{"filesystem search/metadata/native errors", "C:\\FSCOMP.TOS", filesystem_args, demo_env, 0},
		{"VDI arrays/rasters/workstation cleanup", "C:\\VDI.TOS", vdi_args, vdi_env, 0},
		{"native-image guest read fault", "C:\\FAULT.TOS", read_fault_args, demo_env, 255},
		{"partial low-memory guest write fault", "C:\\FAULT.TOS", write_fault_args, demo_env, 255},
		{"shebang with 68k interpreter", "C:\\SCRIPT.SH", script_args, demo_env, 42},
		{"explicit emulator CLI", "C:\\BIN\\PARATOS.PRG", explicit_args, demo_env, 37}
	};
	puts("pTOS Paratos kernel integration: BEGIN");
	if (!userspace())
	{
		puts("pTOS Paratos kernel integration: FAIL (native privilege)");
		return 1;
	}
	for (int round = 0; round < 2; round++)
		for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
		{
#ifdef PTOS_TEST_VDI_ONLY
			if (cases[i].envp != vdi_env) continue;
#endif
			long result = Pexecve(0, cases[i].path, cases[i].argv, cases[i].envp);
			printf("round %d %s: %ld\n", round + 1, cases[i].name, result);
			if (result != cases[i].status)
			{
				puts("pTOS Paratos kernel integration: FAIL (exit status or cleanup)");
				return 1;
			}
		}
	puts("pTOS Paratos kernel integration: PASS");
	return 0;
}
