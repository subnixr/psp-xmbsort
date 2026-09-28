#ifndef SORTER_H
#define SORTER_H

#include <stddef.h>
#include <psptypes.h>

/* One entry (ISO file or game folder) inside a CAT_ folder. */
typedef struct {
	char *path;
	char *name;
	char *key;	/* lowercase name without .iso/.cso/.zso/.pbp */
	u64 tick;	/* current mtime */
	int used;
} Item;

/* A category: every CAT_<name> folder under ISO/ and PSP/GAME/,
 * matched case-insensitively and merged into one. */
typedef struct {
	char *name;
	char **dirs;
	int ndirs;
	u64 tick;	/* newest mtime among dirs */
	Item *items;
	int nitems;
	int used;
} Category;

typedef struct {
	Category *cats;
	int ncats;
} Library;

/* Assigns descending timestamps: first stamped = newest = shown first. */
typedef struct {
	u64 base;
	int count;
	int failed;
} Stamper;

void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
void warn(const char *fmt, ...);
int warnings(void);

void make_key(char *dst, size_t n, const char *name);

void library_scan(Library *lib, const char *root);
void library_free(Library *lib);
Category *library_find(Library *lib, const char *name);
/* Sort categories and their items by current XMB order (newest first). */
void library_sort_current(Library *lib);

void stamper_init(Stamper *s);

/* Stamp the merged items of cats: listed ones first in the given order,
 * the rest A-Z (Z-A if za), then the CAT_ dirs. Returns item count. */
int sort_group(Stamper *s, Category **cats, int ncats,
	       char **listed, int nlisted, int za);

#endif
