#ifndef CONFIG_H
#define CONFIG_H

#include "sorter.h"

/* One Name: block of config.txt. */
typedef struct {
	char *name;
	char **items;
	int nitems;
} Section;

typedef struct {
	Section *secs;
	int nsecs;
} Config;

/* Returns -1 if the file can't be opened. */
int config_load(Config *cfg, const char *path);
void config_free(Config *cfg);
/* Write a config listing lib's categories in order. Returns -1 on failure. */
int config_generate(const char *path, const Library *lib);

#endif
