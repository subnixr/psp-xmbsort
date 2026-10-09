#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "config.h"

static const char header[] =
	"# Name: = CAT_Name folders, *: = uncategorized.\n"
	"# List games under category in desired order.\n"
	"# Unlisted items go A-Z.\n";

static char *trim(char *s)
{
	char *e;

	while (isspace((unsigned char)*s))
		s++;
	e = s + strlen(s);
	while (e > s && isspace((unsigned char)e[-1]))
		*--e = '\0';
	return s;
}

static void push(char ***list, int *n, const char *s)
{
	*list = xrealloc(*list, (*n + 1) * sizeof(char *));
	(*list)[(*n)++] = xstrdup(s);
}

int config_load(Config *cfg, const char *path)
{
	FILE *f = fopen(path, "r");
	char line[512];
	Section *cur = NULL;
	int lineno = 0;

	cfg->secs = NULL;
	cfg->nsecs = 0;
	if (!f)
		return -1;

	while (fgets(line, sizeof(line), f)) {
		char *s = line;
		size_t len;

		/* UTF-8 BOM from Windows editors. */
		if (++lineno == 1 && !strncmp(s, "\xEF\xBB\xBF", 3))
			s += 3;
		s = trim(s);
		if (!*s || *s == '#')
			continue;

		/* No file name has a colon. */
		len = strlen(s);
		if (s[len - 1] == ':') {
			s[len - 1] = '\0';
			s = trim(s);
			if (!*s) {
				warn("line %d: missing category name", lineno);
				cur = NULL;
				continue;
			}
			if (!strcmp(s, NONE_MARK))
				*s = '\0';
			else if (!strncasecmp(s, "CAT_", 4))
				s += 4;
			cfg->secs = xrealloc(cfg->secs, (cfg->nsecs + 1) * sizeof(Section));
			cur = &cfg->secs[cfg->nsecs++];
			memset(cur, 0, sizeof(*cur));
			cur->name = xstrdup(s);
			continue;
		}

		if (!cur) {
			warn("line %d: item before any category", lineno);
			continue;
		}
		push(&cur->items, &cur->nitems, s);
	}
	fclose(f);
	return 0;
}

void config_free(Config *cfg)
{
	int i, j;

	for (i = 0; i < cfg->nsecs; i++) {
		Section *sec = &cfg->secs[i];

		for (j = 0; j < sec->nitems; j++)
			free(sec->items[j]);
		free(sec->name);
		free(sec->items);
	}
	free(cfg->secs);
	cfg->secs = NULL;
	cfg->nsecs = 0;
}

int config_generate(const char *path, const Library *lib)
{
	FILE *f = fopen(path, "w");
	int i;

	if (!f)
		return -1;
	fputs(header, f);
	for (i = 0; i < lib->ncats; i++) {
		const char *name = lib->cats[i].name;

		fprintf(f, "\n%s:\n", *name ? name : NONE_MARK);
	}
	return fclose(f) ? -1 : 0;
}
