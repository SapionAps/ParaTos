/* Native pTOS test driver. Boot-configured binfmt/shebang handlers launch a
 * new native Paratos process for each ordinary compiled 68k TOS program. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <mint/osbind.h>
#include <mint/mintbind.h>
#include "../../host/ptos/host.h"

/* The ELF startup stack carries both live and saved launch vectors. */
long _stksize = 128L * 1024;

static char *const demo_env[] = {"DEMO=present", NULL};
static char *const cli_env[] = {"DEMO=present", "CHECKARGV0=guest display name", NULL};
static char *const cli_classic_env[] = {"DEMO=present", "CHECKARGV0=", NULL};
static char *const cli_relaunch_env[] = {"DEMO=relaunch", NULL};
#if defined(__arm__)
static char *const vdi_env[] = {"VDI=kernel", NULL};
#elif defined(PTOS_TEST_VDI_AVAILABLE) && PTOS_TEST_VDI_AVAILABLE
static char *const vdi_env[] = {"VDI=required", NULL};
#else
static char *const vdi_env[] = {"VDI=absent", NULL};
#endif
static char *const cli_args[] = {"guest display name", "two words", "", NULL};
static char *const cli_named_no_args[] = {"guest display name", NULL};
static char *const cli_named_short[] = {"guest display name", "plain", NULL};
static char *const cli_classic[] = {"", "plain", NULL};
static char *const cli_inherit[] = {"", NULL};
/* Leave exactly enough room for the nested launch's ARGV block. */
static char inherited_padding[PTOS_GUEST_ENV_MAX - sizeof("DEMO=inherit-limit") -
	sizeof("ARGV=") - sizeof("C:\\CLI.TOS") - sizeof("child") - 1] = "PAD=";
static char *const inherited_env[] = {"DEMO=inherit-limit", inherited_padding, NULL};
static char *const semantics_args[] = {"C:\\SEMANTIC.TOS", NULL};
static char *const filesystem_args[] = {"C:\\FSCOMP.TOS", NULL};
static char *const vdi_args[] = {"C:\\VDI.TOS", NULL};
static char *const read_fault_args[] = {"C:\\FAULT.TOS", "read", NULL};
static char *const write_fault_args[] = {"C:\\FAULT.TOS", "write", NULL};
static char *const script_args[] = {"C:/SCRIPT.SH", "two words", "", NULL};
static char *const explicit_args[] = {"C:\\BIN\\PARATOS.PRG", "C:/CLI.TOS", "two words", "", NULL};
static char *const relaunch_args[] = {"arbitrary native argv zero", "C:/CLI.TOS", NULL};
#ifdef PTOS_TEST_EXEC_REPLACEMENT
static char *const replace_prg_env[] = {"DEMO=replace-prg", NULL};
#endif

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
		{"CLI arguments/environment/files", "C:\\CLI.TOS", cli_args, cli_env, 37},
		{"CLI named argv without operands", "C:\\CLI.TOS", cli_named_no_args, cli_env, 38},
		{"CLI named argv with a short operand", "C:\\CLI.TOS", cli_named_short, cli_env, 38},
		{"CLI classic unnamed tail", "C:\\CLI.TOS", cli_classic, cli_classic_env, 38},
		{"CLI inherited environment at the PRG limit", "C:\\CLI.TOS", cli_inherit, inherited_env, 42},
		{"privileges/overlays/native and guest Pexec", "C:\\SEMANTIC.TOS", semantics_args, demo_env, semantics_status},
		{"filesystem search/metadata/native errors", "C:\\FSCOMP.TOS", filesystem_args, demo_env, 0},
		{"VDI arrays/rasters/workstation cleanup", "C:\\VDI.TOS", vdi_args, vdi_env, 0},
		{"native-image guest read fault", "C:\\FAULT.TOS", read_fault_args, demo_env, 255},
		{"partial low-memory guest write fault", "C:\\FAULT.TOS", write_fault_args, demo_env, 255},
		{"shebang with 68k interpreter", "C:\\SCRIPT.SH", script_args, demo_env, 42},
		{"explicit emulator CLI", "C:\\BIN\\PARATOS.PRG", explicit_args, demo_env, 37},
		{"renamed emulator relaunch without binfmt", "C:\\BIN\\RENAMED.TOS", relaunch_args, cli_relaunch_env, 43},
#ifdef PTOS_TEST_EXEC_REPLACEMENT
		{"guest PRG replacement with the same PID", "C:\\BIN\\RENAMED.TOS", relaunch_args, replace_prg_env, 44},
#endif
	};
	puts("pTOS Paratos kernel integration: BEGIN");
	memset(inherited_padding + 4, 'x', sizeof(inherited_padding) - 6);
	inherited_padding[sizeof(inherited_padding) - 2] = 'z';
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
