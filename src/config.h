#ifndef _CONFIG_H_
#define _CONFIG_H_


#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* Data model                                                           */
/* ------------------------------------------------------------------ */

/*
 * A single application entry:
 *
 *   { command = ["mpv", "--"], fork = true, silent = true, pager = false }
 */
#define CMD_ARGS_MAX  32   /* max argv slots per command */
#define CMD_GROUPS_MAX 64  /* max named groups in [defaults] */
#define CMD_LIST_MAX  16   /* max app entries per group     */
#define EXT_MAP_MAX   256  /* max extension mappings        */
#define MIME_MAP_MAX  32   /* max mimetype mappings         */

typedef struct {
	char  *argv[CMD_ARGS_MAX]; /* NULL-terminated argument list */
	int    argc;
	bool   fork;               /* run detached from terminal    */
	bool   silent;             /* redirect stdout/stderr to /dev/null */
	bool   pager;              /* pipe output through $PAGER    */
} app_entry_t;

/*
 * A named group of app entries (one [defaults] entry).
 *   audio_default = [ {…}, {…} ]
 */
typedef struct {
	char        name[128];
	app_entry_t entries[CMD_LIST_MAX];
	int         nentries;
} app_group_t;

/*
 * Config resolved for a single lookup key (extension or MIME type).
 * Points into the group stored in config_t — do not free members.
 */
typedef struct {
	const app_entry_t *entries;
	int                nentries;
} app_list_t;

/*
 * Top-level parsed configuration.
 */
typedef struct {
	/* [defaults] */
	app_group_t groups[CMD_GROUPS_MAX];
	int         ngroups;

	/* [extension] — maps "mp4" → group index */
	struct {
		char ext[32];
		int  group_idx;   /* index into groups[], or -1 if inline */
		/* inline app_list (for ext.app_list = […]) */
		app_group_t inline_group;
		bool        is_inline;
	} ext_map[EXT_MAP_MAX];
	int next_map;

	/* [mimetype] — maps "video" or "video/mp4" → group index */
	struct {
		char mime[64];   /* e.g. "video" or "video/mp4" or "text" */
		int  group_idx;
	} mime_map[MIME_MAP_MAX];
	int nmime_map;
} config_t;

/* ------------------------------------------------------------------ */
/* API                                                                  */
/* ------------------------------------------------------------------ */

/*
 * Parse the TOML config file at `path` into `cfg`.
 * Returns 0 on success, -1 on error (message written to stderr).
 */
int  config_load(const char *path, config_t *cfg);

/*
 * Look up the app list for a file extension (without leading dot).
 * Returns pointer into cfg on success, NULL if not found.
 */
const app_list_t *config_lookup_ext(const config_t *cfg, const char *ext);

/*
 * Look up the app list for a MIME type string (e.g. "video/mp4").
 * Tries exact match first, then major-type-only ("video").
 * Returns pointer into cfg on success, NULL if not found.
 */
const app_list_t *config_lookup_mime(const config_t *cfg, const char *mime);

/*
 * Free all heap memory owned by cfg.
 */
void config_free(config_t *cfg);


#endif  // _CONFIG_H_
