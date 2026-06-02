#include <argp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>  /* basename() */

#include "log.h"
#include "project_config.h"
#include "get_mime.h"
#include "config.h"
#include "runner.h"

/* ------------------------------------------------------------------ */
/* argp setup                                                           */
/* ------------------------------------------------------------------ */

const char *argp_program_version     = OPENR_BINARY_NAME " " OPENR_VERSION;
const char *argp_program_bug_address = OPENR_HOMEPAGE_URL "/issues";

static char doc[]      = OPENR_BINARY_NAME " - " OPENR_DESCRIPTION;
static char args_doc[] = "FILE";

static struct argp_option options[] = {
	{ "log-level", 'L', "LEVEL", 0, "Set log level: error|warn|info|debug (default: info)" },
	{ "mime",      'M', 0,       0, "Force MIME-type-based command selection"               },
	{ "list",      'l', 0,       0, "Show all available commands and let user choose"       },
	{ 0 }
};

typedef struct {
	bool        mime;
	bool        list;
	char       *file;
	Log_level_t log_level;
} Arguments;

static Arguments G_Arguments = {
	.mime      = false,
	.list      = false,
	.file      = NULL,
	.log_level = LOG_LEVEL_INFO,
};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	Arguments *arguments = state->input;

	switch (key) {
	case 'L':
		if      (strcmp(arg, "error") == 0) log_set_level(LOG_LEVEL_ERROR);
		else if (strcmp(arg, "warn")  == 0) log_set_level(LOG_LEVEL_WARN);
		else if (strcmp(arg, "info")  == 0) log_set_level(LOG_LEVEL_INFO);
		else if (strcmp(arg, "debug") == 0) log_set_level(LOG_LEVEL_DEBUG);
		else argp_error(state, "unknown log level: '%s'", arg);
		arguments->log_level = log_get_level();
		break;
	case 'M':
		arguments->mime = true;
		break;
	case 'l':
		arguments->list = true;
		break;
	case ARGP_KEY_ARG:
		if (state->arg_num == 0)
			arguments->file = arg;
		else
			argp_usage(state);
		break;
	case ARGP_KEY_END:
		if (arguments->file == NULL)
			argp_usage(state);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct argp argp = {
	.options  = options,
	.parser   = parse_opt,
	.doc      = doc,
	.args_doc = args_doc,
};

/* ------------------------------------------------------------------ */
/* Config path resolution                                               */
/* ------------------------------------------------------------------ */

static const char *get_config_path(void)
{
	static char path[PATH_MAX];

	const char *xdg = getenv("XDG_CONFIG_HOME");
	if (xdg && *xdg) {
		snprintf(path, sizeof(path), "%s/openr/config.toml", xdg);
		if (access(path, R_OK) == 0)
			return path;
	}

	const char *home = getenv("HOME");
	if (home && *home) {
		snprintf(path, sizeof(path), "%s/.config/openr/config.toml", home);
		if (access(path, R_OK) == 0)
			return path;
	}

	/* System-wide fallback (set at compile time) */
	snprintf(path, sizeof(path), "%s/etc/openr/config.toml",
	         COMPILED_TIME_PREFIX);

	if (access(path, R_OK) != 0) {
		LOG_ERROR("No valid config found. config example " OPENR_HOMEPAGE_URL "/etc/config.toml");
		return NULL;
	}

	LOG_DEBUG("Using %s cofnig", path);
	return path;
}

/* ------------------------------------------------------------------ */
/* Extension extraction                                                 */
/* ------------------------------------------------------------------ */

static const char *get_extension(const char *filepath)
{
	/* basename so we don't get confused by dots in directory names */
	char buf[PATH_MAX];
	strncpy(buf, filepath, sizeof(buf) - 1);
	const char *base = basename(buf);

	const char *dot = strrchr(base, '.');
	if (!dot || dot == base)
		return "";          /* no extension */

	return dot + 1;         /* skip the '.' */
}

/* ------------------------------------------------------------------ */
/* Main                                                                 */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
	argp_parse(&argp, argc, argv, 0, 0, &G_Arguments);

	const char *config_path = get_config_path();
	LOG_DEBUG("config path: %s", config_path);

	/* Load config */
	config_t cfg;
	if (config_load(config_path, &cfg) < 0) {
		LOG_ERROR("openr: failed to load config from %s", config_path);
		return 1;
	}

	const char *filepath = G_Arguments.file;
	const app_list_t *list = NULL;

	/* ---- Lookup by extension (unless --mime forced) ---- */
	if (!G_Arguments.mime) {
		const char *ext = get_extension(filepath);
		LOG_DEBUG("file extension: '%s'", ext);

		if (*ext) {
			list = config_lookup_ext(&cfg, ext);
			if (list)
				LOG_DEBUG("matched by extension '%s'", ext);
		}
	}

	/* ---- Fallback / forced: lookup by MIME type ---- */
	if (!list) {
		char mime[128] = {0};
		if (get_mime_type(filepath, mime, sizeof(mime)) < 0) {
			fprintf(stderr, "openr: could not determine MIME type of '%s'\n", filepath);
			config_free(&cfg);
			return 1;
		}
		LOG_DEBUG("MIME type: %s", mime);

		list = config_lookup_mime(&cfg, mime);
		if (list) LOG_INFO("matched by MIME type '%s'", mime);
	}

	if (!list) {
		fprintf(stderr, "openr: no handler found for '%s'\n", filepath);
		config_free(&cfg);
		return 1;
	}

	int rc = runner_run(list, filepath, G_Arguments.list);

	config_free(&cfg);
	return rc == 0 ? 0 : 1;
}
