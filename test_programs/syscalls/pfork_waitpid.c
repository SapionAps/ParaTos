#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
	pid_t pid = fork();
	if (pid < 0) {
		return 1;
	}
	if (pid == 0) {
		return 7;
	}
	int status = 0;
	if (waitpid(pid, &status, 0) < 0) {
		return 2;
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 7) {
		return 3;
	}
	return 0;
}
