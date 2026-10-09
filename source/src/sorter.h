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

/* Pseudo-category of everything outside the CAT_ folders: *: in config. */
#define NONE_NAME	""
#define NONE_MARK	"*"

/* A category: every CAT_<name> folder under ISO/ and PSP/GAME/,
 * matched case-insensitively and merged into one. NONE_NAME has no
 * dirs and holds the loose entries of ISO/ and PSP/GAME/. */
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
/* Sort categories A-Z, NONE_NAME last. */
void library_sort_name(Library *lib);

void stamper_init(Stamper *s);

/* Stamp the items of c: listed ones first in the given order,
 * the rest A-Z, then the CAT_ dirs. Returns item count. */
int sort_group(Stamper *s, Category *c, char **listed, int nlisted);

#endif
