#include "get_mime.h"
#include "log.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/*
 * Detect MIME type safely using execvp — no shell involved,
 * so filenames with special characters cannot break out.
 */
int get_mime_type(const char *path, char *mime, size_t mime_size)
{
	if (!path || !mime || mime_size == 0)
		return -1;

	int pipefd[2];
	if (pipe(pipefd) < 0) {
		LOG_ERROR("pipe: %s", strerror(errno));
		return -1;
	}

	pid_t pid = fork();
	if (pid < 0) {
		LOG_ERROR("fork: %s", strerror(errno));
		return -1;
	}

	if (pid == 0) {
		/* child: exec `file --brief --mime-type <path>` */
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[1]);

		execlp("file", "file", "--brief", "--mime-type", "--", path, NULL);
		_exit(127);
	}

	/* parent: read result */
	close(pipefd[1]);

	size_t total = 0;
	ssize_t n;
	while (total < mime_size - 1 &&
	       (n = read(pipefd[0], mime + total, mime_size - 1 - total)) > 0)
		total += (size_t)n;

	close(pipefd[0]);

	int status;
	waitpid(pid, &status, 0);

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		LOG_ERROR("'file' command failed for path: %s", path);
		return -1;
	}

	mime[total] = '\0';

	/* strip trailing newline */
	size_t len = strlen(mime);
	while (len > 0 && (mime[len-1] == '\n' || mime[len-1] == '\r'))
		mime[--len] = '\0';

	return 0;
}
