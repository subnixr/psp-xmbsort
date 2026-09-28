#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "config.h"

static const char header[] =
	"# pspxmbsort config\n"
	"#\n"
	"# Category order = order of the [sections]. [Name] matches the CAT_Name\n"
	"# folders in ISO/ and PSP/GAME/ (case-insensitive), merged into one.\n"
	"# Several names can form one category:  [MINI, MINIS]\n"
	"#\n"
	"# Inside a section, list items (file or folder names; .iso/.cso/.zso/.pbp\n"
	"# optional) in the order they should appear. Items not listed follow A-Z,\n"
	"# or Z-A with the line:  sort = za\n"
	"#\n"
	"# Lines starting with # are comments.\n";

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

/* "sort = az|za" directive; returns 1 if line was one. */
static int parse_sort(Section *sec, char *s, int lineno)
{
	char *p;

	if (strncasecmp(s, "sort", 4))
		return 0;
	p = trim(s + 4);
	if (*p != '=')
		return 0;
	p = trim(p + 1);
	if (!strcasecmp(p, "za"))
		sec->za = 1;
	else if (!strcasecmp(p, "az"))
		sec->za = 0;
	else
		warn("line %d: sort must be az or za", lineno);
	return 1;
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

		/* UTF-8 BOM from Windows editors. */
		if (++lineno == 1 && !strncmp(s, "\xEF\xBB\xBF", 3))
			s += 3;
		s = trim(s);
		if (!*s || *s == '#')
			continue;

		if (*s == '[') {
			char *e = strchr(s, ']'), *tok;

			if (!e) {
				warn("line %d: missing ]", lineno);
				continue;
			}
			*e = '\0';
			cfg->secs = xrealloc(cfg->secs, (cfg->nsecs + 1) * sizeof(Section));
			cur = &cfg->secs[cfg->nsecs++];
			memset(cur, 0, sizeof(*cur));
			for (tok = strtok(s + 1, ","); tok; tok = strtok(NULL, ",")) {
				tok = trim(tok);
				if (!strncasecmp(tok, "CAT_", 4))
					tok += 4;
				if (*tok)
					push(&cur->names, &cur->nnames, tok);
			}
			if (!cur->nnames)
				warn("line %d: empty section name", lineno);
			continue;
		}

		if (!cur) {
			warn("line %d: item outside [section]", lineno);
			continue;
		}
		if (parse_sort(cur, s, lineno))
			continue;
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

		for (j = 0; j < sec->nnames; j++)
			free(sec->names[j]);
		for (j = 0; j < sec->nitems; j++)
			free(sec->items[j]);
		free(sec->names);
		free(sec->items);
	}
	free(cfg->secs);
	cfg->secs = NULL;
	cfg->nsecs = 0;
}

int config_generate(const char *path, const Library *lib)
{
	FILE *f = fopen(path, "w");
	int i, j;

	if (!f)
		return -1;
	fputs(header, f);
	for (i = 0; i < lib->ncats; i++) {
		const Category *c = &lib->cats[i];

		fprintf(f, "\n[%s]\n", c->name);
		for (j = 0; j < c->nitems; j++)
			fprintf(f, "%s\n", c->items[j].name);
	}
	return fclose(f) ? -1 : 0;
}
