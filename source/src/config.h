#ifndef CONFIG_H
#define CONFIG_H

#include "sorter.h"

/* One [Name, Alias...] section of config.txt. */
typedef struct {
	char **names;
	int nnames;
	char **items;
	int nitems;
	int za;
} Section;

typedef struct {
	Section *secs;
	int nsecs;
} Config;

/* Returns -1 if the file can't be opened. */
int config_load(Config *cfg, const char *path);
void config_free(Config *cfg);
/* Write a config reflecting lib's current order. Returns -1 on failure. */
int config_generate(const char *path, const Library *lib);

#endif
