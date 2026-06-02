#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#include "runner.h"
#include "log.h"
#include "colors.h"

/* ------------------------------------------------------------------ */
/* Internal: build argv and exec                                        */
/* ------------------------------------------------------------------ */

/*
 * Launch a single app_entry_t with `filepath` appended.
 *
 * fork  = true  → detach child, return immediately
 * silent= true  → redirect child stdout/stderr to /dev/null
 * pager = true  → pipe child stdout through $PAGER (or `less`)
 */
static int launch(const app_entry_t *entry, const char *filepath)
{
	/* Build final argv: entry->argv[] + filepath + NULL */
	const char *argv[CMD_ARGS_MAX + 2];
	int argc = 0;

	for (int i = 0; i < entry->argc; i++)
		argv[argc++] = entry->argv[i];
	argv[argc++] = filepath;
	argv[argc]   = NULL;

	LOG_DEBUG("launching: %s %s (fork=%d silent=%d pager=%d)",
	          argv[0], filepath, entry->fork, entry->silent, entry->pager);

	/* ---- pager mode: pipe child → pager ---- */
	if (entry->pager) {
		int pipefd[2];
		if (pipe(pipefd) < 0) {
			LOG_ERROR("pipe: %s", strerror(errno));
			return -1;
		}

		pid_t child = fork();
		if (child < 0) {
			LOG_ERROR("fork: %s", strerror(errno));
			return -1;
		}

		if (child == 0) {
			/* child: write to pipe */
			close(pipefd[0]);
			dup2(pipefd[1], STDOUT_FILENO);
			close(pipefd[1]);
			if (entry->silent) {
				int devnull = open("/dev/null", O_WRONLY);
				if (devnull >= 0) dup2(devnull, STDERR_FILENO);
			}
			execvp(argv[0], (char *const *)argv);
			_exit(127);
		}

		/* parent: read from pipe, feed pager */
		close(pipefd[1]);

		pid_t pager_pid = fork();
		if (pager_pid == 0) {
			dup2(pipefd[0], STDIN_FILENO);
			close(pipefd[0]);
			const char *pager = getenv("PAGER");
			if (!pager || !*pager) pager = "less";
			execlp(pager, pager, NULL);
			_exit(127);
		}
		close(pipefd[0]);

		int status;
		waitpid(child, &status, 0);
		waitpid(pager_pid, &status, 0);
		return 0;
	}

	/* ---- normal / fork mode ---- */
	pid_t pid = fork();
	if (pid < 0) {
		LOG_ERROR("fork: %s", strerror(errno));
		return -1;
	}

	if (pid == 0) {
		/* child */
		if (entry->silent) {
			int devnull = open("/dev/null", O_WRONLY);
			if (devnull >= 0) {
				dup2(devnull, STDOUT_FILENO);
				dup2(devnull, STDERR_FILENO);
				close(devnull);
			}
		}

		if (entry->fork) {
			/* detach: become session leader so parent can exit */
			setsid();
		}

		execvp(argv[0], (char *const *)argv);
		/* exec failed */
		fprintf(stderr, "openr: exec '%s' failed: %s\n", argv[0], strerror(errno));
		_exit(127);
	}

	/* parent */
	if (entry->fork) {
		/* detached — don't wait */
		LOG_DEBUG("detached pid %d", pid);
		return 0;
	}

	int status;
	if (waitpid(pid, &status, 0) < 0) {
		LOG_ERROR("waitpid: %s", strerror(errno));
		return -1;
	}

	if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
		LOG_ERROR("command '%s' not found or failed to exec", argv[0]);
		return -1;
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/* Internal: interactive menu                                           */
/* ------------------------------------------------------------------ */

static int show_menu(const app_list_t *list)
{
	fprintf(stderr, "\nChoose an application:\n");
	for (int i = 0; i < list->nentries; i++) {
		const app_entry_t *e = &list->entries[i];

		fprintf(stderr, "  [%d] %s", i + 1, e->argv[0]);
			if (e->fork)   fprintf(stderr, COLOR_YELLOW  " [fork]"   COLOR_RESET);
			if (e->silent) fprintf(stderr, COLOR_CYAN    " [silent]" COLOR_RESET);
			if (e->pager)  fprintf(stderr, COLOR_MAGENTA " [pager]"  COLOR_RESET);
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "  [0] Cancel\n");
	fprintf(stderr, "> ");
	fflush(stderr);

	int choice = -1;
	if (scanf("%d", &choice) != 1)
		return -1;

	if (choice == 0)
		return -1; /* cancelled */

	if (choice < 1 || choice > list->nentries) {
		LOG_ERROR("Invalid choice.\n");
		return -1;
	}

	return choice - 1; /* 0-based index */
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

int runner_run(const app_list_t *list, const char *filepath, bool list_mode)
{
	if (!list || list->nentries == 0) {
		LOG_ERROR("no app entries to run");
		return -1;
	}

	/* Single entry, no menu requested → run directly */
	if (!list_mode && list->nentries == 1)
		return launch(&list->entries[0], filepath);

	/* Multiple entries without list_mode → run the first one */
	if (!list_mode)
		return launch(&list->entries[0], filepath);

	/* list_mode → show interactive menu */
	int idx = show_menu(list);
	if (idx < 0)
		return 0; /* cancelled — not an error */

	return launch(&list->entries[idx], filepath);
}
