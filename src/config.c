#include "config.h"
#include "log.h"
#include "toml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static int find_group(const config_t *cfg, const char *name)
{
	for (int i = 0; i < cfg->ngroups; i++) {
		if (strcmp(cfg->groups[i].name, name) == 0)
			return i;
	}
	return -1;
}

/*
 * Parse one app entry from a TOML inline table:
 *   { command = ["mpv", "--"], fork = true, silent = true }
 */
static int parse_app_entry(toml_table_t *tab, app_entry_t *entry)
{
	memset(entry, 0, sizeof(*entry));

	/* command — can be a string or an array of strings */
	toml_array_t *cmd_arr = toml_array_in(tab, "command");
	toml_datum_t  cmd_str = toml_string_in(tab, "command");

	if (cmd_arr) {
		int n = toml_array_nelem(cmd_arr);
		if (n >= CMD_ARGS_MAX) {
			LOG_ERROR("command has too many args (max %d)", CMD_ARGS_MAX - 1);
			return -1;
		}
		for (int i = 0; i < n; i++) {
			toml_datum_t d = toml_string_at(cmd_arr, i);
			if (!d.ok) {
				LOG_ERROR("command arg %d is not a string", i);
				return -1;
			}
			entry->argv[i] = d.u.s; /* caller must free via config_free */
		}
		entry->argc = n;
	} else if (cmd_str.ok) {
		entry->argv[0] = cmd_str.u.s;
		entry->argc    = 1;
	} else {
		LOG_ERROR("app entry missing 'command' field");
		return -1;
	}

	/* optional flags */
	toml_datum_t fork_d   = toml_bool_in(tab, "fork");
	toml_datum_t silent_d = toml_bool_in(tab, "silent");
	toml_datum_t pager_d  = toml_bool_in(tab, "pager");

	entry->fork   = fork_d.ok   && fork_d.u.b;
	entry->silent = silent_d.ok && silent_d.u.b;
	entry->pager  = pager_d.ok  && pager_d.u.b;

	return 0;
}

/*
 * Parse an array of app entry tables into an app_group_t.
 */
static int parse_app_array(toml_array_t *arr, app_group_t *grp)
{
	int n = toml_array_nelem(arr);
	if (n > CMD_LIST_MAX) {
		LOG_WARN("group '%s' has %d entries; truncating to %d", grp->name, n, CMD_LIST_MAX);
		n = CMD_LIST_MAX;
	}

	grp->nentries = 0;
	for (int i = 0; i < n; i++) {
		toml_table_t *entry_tab = toml_table_at(arr, i);
		if (!entry_tab) {
			LOG_ERROR("app list entry %d is not a table", i);
			return -1;
		}
		if (parse_app_entry(entry_tab, &grp->entries[grp->nentries]) == 0)
			grp->nentries++;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* [defaults] section                                                   */
/* ------------------------------------------------------------------ */

static int parse_defaults(toml_table_t *defaults_tab, config_t *cfg)
{
	if (!defaults_tab)
		return 0;

	/* Each key in [defaults] is a group name pointing to an array */
	for (int ki = 0; ; ki++) {
		const char *key = toml_key_in(defaults_tab, ki);
		if (!key)
			break;

		toml_array_t *arr = toml_array_in(defaults_tab, key);
		if (!arr) {
			LOG_WARN("[defaults] key '%s' is not an array — skipping", key);
			continue;
		}

		if (cfg->ngroups >= CMD_GROUPS_MAX) {
			LOG_ERROR("too many groups (max %d)", CMD_GROUPS_MAX);
			return -1;
		}

		app_group_t *grp = &cfg->groups[cfg->ngroups];
		strncpy(grp->name, key, sizeof(grp->name) - 1);

		if (parse_app_array(arr, grp) < 0)
			return -1;

		LOG_DEBUG("loaded group '%s' (%d entries)", grp->name, grp->nentries);
		cfg->ngroups++;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* [extension] section                                                  */
/* ------------------------------------------------------------------ */

static int parse_extensions(toml_table_t *ext_tab, config_t *cfg)
{
	if (!ext_tab)
		return 0;

	for (int ki = 0; ; ki++) {
		const char *ext = toml_key_in(ext_tab, ki);
		if (!ext)
			break;

		if (cfg->next_map >= EXT_MAP_MAX) {
			LOG_ERROR("too many extension mappings (max %d)", EXT_MAP_MAX);
			return -1;
		}

		int idx = cfg->next_map;
		strncpy(cfg->ext_map[idx].ext, ext, sizeof(cfg->ext_map[idx].ext) - 1);
		cfg->ext_map[idx].group_idx = -1;
		cfg->ext_map[idx].is_inline = false;

		/* Case 1: ext = "group_name"  (a raw string value) */
		toml_raw_t raw = toml_raw_in(ext_tab, ext);
		if (raw) {
			char *group_name = NULL;
			if (toml_rtos(raw, &group_name) == 0) {
				int gi = find_group(cfg, group_name);
				if (gi < 0) {
					LOG_WARN("extension '%s' references unknown group '%s'", ext, group_name);
				} else {
					cfg->ext_map[idx].group_idx = gi;
					cfg->next_map++;
					LOG_DEBUG("ext '%s' → group '%s'", ext, group_name);
				}
				free(group_name);
			}
			continue;
		}

		/* Case 2: ext.app_list = [{...}, ...] — inline table with app_list key */
		toml_table_t *sub = toml_table_in(ext_tab, ext);
		if (sub) {
			toml_array_t *app_list = toml_array_in(sub, "app_list");
			if (app_list) {
				app_group_t *grp = &cfg->ext_map[idx].inline_group;
				snprintf(grp->name, sizeof(grp->name), "__ext_%s__", ext);
				if (parse_app_array(app_list, grp) < 0)
					return -1;
				cfg->ext_map[idx].is_inline = true;
				cfg->next_map++;
				LOG_DEBUG("ext '%s' → inline app_list (%d entries)", ext, grp->nentries);
			} else {
				LOG_WARN("extension '%s' table has no 'app_list' key — skipping", ext);
			}
		}
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* [mimetype] section                                                   */
/* ------------------------------------------------------------------ */

/*
 * The mimetype section has a nested structure:
 *   [mimetype.video]
 *     inherit = "video_default"
 *   [mimetype.application.subtype.octet-stream]
 *     inherit = "video_default"
 *
 * We flatten it: "video" → group, "application/octet-stream" → group.
 */
static int parse_mimetype_table(toml_table_t *tab, const char *prefix, config_t *cfg)
{
	if (!tab)
		return 0;

	/* Check if this table itself has an "inherit" key */
	toml_raw_t inherit_raw = toml_raw_in(tab, "inherit");
	if (inherit_raw && prefix && *prefix) {
		char *group_name = NULL;
		if (toml_rtos(inherit_raw, &group_name) == 0) {
			int gi = find_group(cfg, group_name);
			if (gi < 0) {
				LOG_WARN("mimetype '%s' references unknown group '%s'", prefix, group_name);
			} else if (cfg->nmime_map < MIME_MAP_MAX) {
				int mi = cfg->nmime_map++;
				strncpy(cfg->mime_map[mi].mime, prefix, sizeof(cfg->mime_map[mi].mime) - 1);
				cfg->mime_map[mi].group_idx = gi;
				LOG_DEBUG("mime '%s' → group '%s'", prefix, group_name);
			}
			free(group_name);
		}
	}

	/* Recurse into sub-tables, but skip special keys */
	for (int ki = 0; ; ki++) {
		const char *key = toml_key_in(tab, ki);
		if (!key)
			break;

		/* skip non-table keys we already handled */
		if (strcmp(key, "inherit") == 0)
			continue;

		/* "subtype" is a structural key meaning the next level is the real key */
		toml_table_t *child = toml_table_in(tab, key);
		if (!child)
			continue;

		char child_prefix[128] = {0};
		if (!prefix || !*prefix) {
			snprintf(child_prefix, sizeof(child_prefix), "%s", key);
		} else if (strcmp(key, "subtype") == 0) {
			/* keep the same prefix — "subtype" is just structural */
			snprintf(child_prefix, sizeof(child_prefix), "%s", prefix);
		} else {
			/* build "major/minor" mime string */
			snprintf(child_prefix, sizeof(child_prefix), "%s/%s", prefix, key);
		}

		if (parse_mimetype_table(child, child_prefix, cfg) < 0)
			return -1;
	}

	return 0;
}

static int parse_mimetype(toml_table_t *mime_tab, config_t *cfg)
{
	return parse_mimetype_table(mime_tab, "", cfg);
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

int config_load(const char *path, config_t *cfg)
{
	memset(cfg, 0, sizeof(*cfg));

	FILE *fp = fopen(path, "r");
	if (!fp) {
		LOG_ERROR("cannot open config file: %s", path);
		return -1;
	}

	char errbuf[256];
	toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));
	fclose(fp);

	if (!root) {
		LOG_ERROR("config parse error: %s", errbuf);
		return -1;
	}

	int rc = 0;

	toml_table_t *defaults = toml_table_in(root, "defaults");
	if (parse_defaults(defaults, cfg) < 0) { rc = -1; goto done; }

	toml_table_t *ext_tab = toml_table_in(root, "extension");
	if (parse_extensions(ext_tab, cfg) < 0) { rc = -1; goto done; }

	toml_table_t *mime_tab = toml_table_in(root, "mimetype");
	if (parse_mimetype(mime_tab, cfg) < 0) { rc = -1; goto done; }

	LOG_DEBUG("config loaded: %d groups, %d ext mappings, %d mime mappings",
	         cfg->ngroups, cfg->next_map, cfg->nmime_map);

done:
	toml_free(root);
	return rc;
}

const app_list_t *config_lookup_ext(const config_t *cfg, const char *ext)
{
	static app_list_t result; /* single-use: not thread-safe, but fine here */

	for (int i = 0; i < cfg->next_map; i++) {
		if (strcasecmp(cfg->ext_map[i].ext, ext) != 0)
			continue;

		if (cfg->ext_map[i].is_inline) {
			result.entries  = cfg->ext_map[i].inline_group.entries;
			result.nentries = cfg->ext_map[i].inline_group.nentries;
		} else {
			int gi = cfg->ext_map[i].group_idx;
			if (gi < 0 || gi >= cfg->ngroups)
				return NULL;
			result.entries  = cfg->groups[gi].entries;
			result.nentries = cfg->groups[gi].nentries;
		}
		return &result;
	}
	return NULL;
}

const app_list_t *config_lookup_mime(const config_t *cfg, const char *mime)
{
	static app_list_t result;

	/* 1. Try exact match: "video/mp4" */
	for (int i = 0; i < cfg->nmime_map; i++) {
		if (strcasecmp(cfg->mime_map[i].mime, mime) == 0) {
			int gi = cfg->mime_map[i].group_idx;
			result.entries  = cfg->groups[gi].entries;
			result.nentries = cfg->groups[gi].nentries;
			return &result;
		}
	}

	/* 2. Try major type only: "video" */
	char major[64] = {0};
	const char *slash = strchr(mime, '/');
	if (slash) {
		size_t len = (size_t)(slash - mime);
		if (len >= sizeof(major)) len = sizeof(major) - 1;
		memcpy(major, mime, len);

		for (int i = 0; i < cfg->nmime_map; i++) {
			if (strcasecmp(cfg->mime_map[i].mime, major) == 0) {
				int gi = cfg->mime_map[i].group_idx;
				result.entries  = cfg->groups[gi].entries;
				result.nentries = cfg->groups[gi].nentries;
				return &result;
			}
		}
	}

	return NULL;
}

void config_free(config_t *cfg)
{
	/* Free all argv strings allocated by tomlc99 (toml_rtos / toml_string_at) */
	for (int g = 0; g < cfg->ngroups; g++) {
		app_group_t *grp = &cfg->groups[g];
		for (int e = 0; e < grp->nentries; e++) {
			for (int a = 0; a < grp->entries[e].argc; a++)
				free(grp->entries[e].argv[a]);
		}
	}

	for (int i = 0; i < cfg->next_map; i++) {
		if (cfg->ext_map[i].is_inline) {
			app_group_t *grp = &cfg->ext_map[i].inline_group;
			for (int e = 0; e < grp->nentries; e++)
				for (int a = 0; a < grp->entries[e].argc; a++)
					free(grp->entries[e].argv[a]);
		}
	}

	memset(cfg, 0, sizeof(*cfg));
}
