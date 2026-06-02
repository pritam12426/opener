#ifndef _RUNNER_H_
#define _RUNNER_H_


#include "config.h"
#include <stdbool.h>

/*
 * Present the app list to the user and let them pick one,
 * then run it with `filepath` appended to its argv.
 *
 * If list_mode is false and there is exactly one entry, run it directly.
 * If list_mode is true, always show an interactive numbered menu.
 *
 * Returns 0 on success, -1 on error.
 */
int runner_run(
	const app_list_t *list,
	const char       *filepath,
	bool              list_mode);


#endif  // _RUNNER_H_
