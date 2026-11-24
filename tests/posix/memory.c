#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"
#include "gemdos/gemdos.h"
#include "host.h"
#include "guest_memory.h"

static void wait_success(pid_t child)
{
	int status;
	assert(child > 0 && waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void check_mappings(void)
{
	uint8_t *private = memory;
	posix_heap *private_heap = PosixGuestHeap();
	private_heap->skipped = 11;
	const uint32_t marker = 0x100;
	m68k_write_memory_32(marker, 17);
	assert(!PosixGuestMemoryShared());
	pid_t child = fork();
	if (!child)
	{
		assert(!PosixGuestMemoryShared() && memory == private);
		assert(m68k_read_memory_32(marker) == 17);
		m68k_write_memory_32(marker, 33);
		_exit(0);
	}
	wait_success(child);
	assert(m68k_read_memory_32(marker) == 17);

	/* Fail after installing the first window in the candidate. The original
	 * reservation and all data must stay available. */
	PosixTestMappingFailure(2);
	assert(PosixRemapGuestMemory(1) == -1 && errno == ENOMEM);
	assert(memory == private && !PosixGuestMemoryShared() && m68k_read_memory_32(marker) == 17);
#if UINTPTR_MAX > UINT32_MAX
	m68k_write_memory_32(0xff000000, 91);
#endif
	posix_ram_change change;
	assert(PosixBeginRamChange(1, &change) == 0 && PosixGuestMemoryShared());
	PosixEndRamChange(&change, 0);
	assert(memory == private && PosixGuestHeap() == private_heap && !PosixGuestMemoryShared());
	assert(PosixRemapGuestMemory(1) == 0 && PosixGuestMemoryShared());
	uint8_t *shared = memory;
	posix_heap *shared_heap = PosixGuestHeap();
	assert(shared_heap != private_heap && shared_heap->skipped == 11);
	assert(PosixRemapGuestMemory(1) == 0 && memory == shared);

	child = fork();
	if (!child)
	{
		assert(PosixGuestMemoryShared() && m68k_read_memory_32(marker) == 17);
#if UINTPTR_MAX > UINT32_MAX
		assert(m68k_read_memory_32(0xff000000) == 91);
		m68k_write_memory_32(0xff000000, 92);
#endif
		m68k_write_memory_32(marker, 55);
		PosixGuestHeap()->skipped = 22;
		assert(PosixRemapGuestMemory(0) == 0 && !PosixGuestMemoryShared());
		assert(m68k_read_memory_32(marker) == 55);
		assert(PosixGuestHeap()->skipped == 22);
		PosixGuestHeap()->skipped = 33;
		m68k_write_memory_32(marker, 66);
		_exit(0);
	}
	wait_success(child);
	assert(PosixGuestHeap() == shared_heap && shared_heap->skipped == 22);
	assert(memory == shared && PosixGuestMemoryShared() && m68k_read_memory_32(marker) == 55);
#if UINTPTR_MAX > UINT32_MAX
	assert(m68k_read_memory_32(0xff000000) == 92);
#endif

	child = fork();
	if (!child)
	{
		struct rlimit cores = {0, 0};
		setrlimit(RLIMIT_CORE, &cores);
		volatile uint8_t gap = memory[POSIX_RAM_BYTES];
		_exit(gap);
	}
	int status;
	assert(child > 0 && waitpid(child, &status, 0) == child);
	assert(WIFSIGNALED(status) && (WTERMSIG(status) == SIGSEGV || WTERMSIG(status) == SIGBUS));
}

int main(void)
{
	static const uint8_t expected[] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
	InitM68KMemory();
	assert(memory_sz == 16 * 1024 * 1024);
	m68k_write_memory_64(0x100, UINT64_C(0x123456789abcdef0));
	assert(!memcmp(memory + 0x100, expected, sizeof(expected)));
	assert(m68k_read_memory_64(0x100) == UINT64_C(0x123456789abcdef0));
	assert(m68k_read_memory_32(0x100) == UINT32_C(0x12345678));
	assert(m68k_read_memory_16(0x104) == 0x9abc);
	assert(m68k_read_memory_8(0x107) == 0xf0);
	m68k_write_memory_32(0x100, UINT32_C(0x89abcdef));
	m68k_write_memory_16(0x104, 0x4321);
	m68k_write_memory_8(0x106, 0x55);
	assert(memory[0x100] == 0x89 && memory[0x103] == 0xef);
	assert(memory[0x104] == 0x43 && memory[0x105] == 0x21 && memory[0x106] == 0x55);
	char copy[sizeof(expected)];
	m68k_write_array8(0x200, (const char*)expected, sizeof(expected));
	m68k_read_array8(0x200, copy, sizeof(copy));
	assert(!memcmp(copy, expected, sizeof(copy)));
	current_process = 0x300;
	Fsetdta(0x500);
	assert(Fgetdta() == 0x500);
	assert(Pdomain(-1) == 0);
	assert(Pdomain(1) == 1 && Pdomain(-1) == 1);
	assert(Pdomain(2) == -1 && Pdomain(-1) == 1);
	check_mappings();
	return 0;
}
