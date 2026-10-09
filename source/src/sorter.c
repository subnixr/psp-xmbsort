#include <pspkernel.h>
#include <pspdebug.h>
#include <psprtc.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "sorter.h"

/* sceIoChstat bits (not in pspsdk headers). */
#define CST_CT	0x0008
#define CST_MT	0x0020

/* FAT32 mtime resolution is 2 seconds. */
#define STEP_US	(2ULL * 1000000ULL)

static const char *const bases[] = { "ISO", "PSP/GAME" };
static const char *const exts[] = { ".iso", ".cso", ".zso", ".pbp" };

static int nwarnings;

void *xrealloc(void *p, size_t n)
{
	void *q = realloc(p, n ? n : 1);

	if (!q) {
		pspDebugScreenPrintf("out of memory\n");
		sceKernelDelayThread(5 * 1000 * 1000);
		sceKernelExitGame();
	}
	return q;
}

char *xstrdup(const char *s)
{
	return strcpy(xrealloc(NULL, strlen(s) + 1), s);
}

void warn(const char *fmt, ...)
{
	va_list ap;
	char buf[256];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	pspDebugScreenPrintf("  ! %s\n", buf);
	nwarnings++;
}

int warnings(void)
{
	return nwarnings;
}

void make_key(char *dst, size_t n, const char *name)
{
	size_t i, len;

	for (i = 0; name[i] && i + 1 < n; i++)
		dst[i] = tolower((unsigned char)name[i]);
	dst[i] = '\0';
	len = i;

	for (i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
		size_t el = strlen(exts[i]);

		if (len > el && strcmp(dst + len - el, exts[i]) == 0) {
			dst[len - el] = '\0';
			break;
		}
	}
}

static u64 dt_tick(const ScePspDateTime *dt)
{
	u64 t = 0;

	sceRtcGetTick(dt, &t);
	return t;
}

static void add_item(Category *c, const char *dir, const SceIoDirent *d)
{
	char path[512], key[256];
	Item *it;

	snprintf(path, sizeof(path), "%s/%s", dir, d->d_name);
	make_key(key, sizeof(key), d->d_name);
	c->items = xrealloc(c->items, (c->nitems + 1) * sizeof(*it));
	it = &c->items[c->nitems++];
	it->path = xstrdup(path);
	it->name = xstrdup(d->d_name);
	it->key = xstrdup(key);
	it->tick = dt_tick(&d->d_stat.sce_st_mtime);
	it->used = 0;
}

static void scan_cat_dir(Category *c, const char *dir)
{
	SceIoDirent d;
	int fd = sceIoDopen(dir);

	if (fd < 0) {
		warn("cannot open %s", dir);
		return;
	}
	for (;;) {
		memset(&d, 0, sizeof(d));
		if (sceIoDread(fd, &d) <= 0)
			break;
		if (!strcmp(d.d_name, ".") || !strcmp(d.d_name, ".."))
			continue;
		add_item(c, dir, &d);
	}
	sceIoDclose(fd);
}

Category *library_find(Library *lib, const char *name)
{
	int i;

	for (i = 0; i < lib->ncats; i++)
		if (!strcasecmp(lib->cats[i].name, name))
			return &lib->cats[i];
	return NULL;
}

void library_scan(Library *lib, const char *root)
{
	SceIoDirent d;
	char base[256], dir[512];
	size_t b;

	/* cats[0] collects whatever is not inside a CAT_ folder. */
	lib->cats = xrealloc(NULL, sizeof(*lib->cats));
	lib->ncats = 1;
	memset(lib->cats, 0, sizeof(*lib->cats));
	lib->cats[0].name = xstrdup(NONE_NAME);

	for (b = 0; b < sizeof(bases) / sizeof(bases[0]); b++) {
		int fd;

		snprintf(base, sizeof(base), "%s/%s", root, bases[b]);
		fd = sceIoDopen(base);
		if (fd < 0)
			continue;
		for (;;) {
			Category *c;
			u64 t;

			memset(&d, 0, sizeof(d));
			if (sceIoDread(fd, &d) <= 0)
				break;
			if (!strcmp(d.d_name, ".") || !strcmp(d.d_name, ".."))
				continue;
			if (!FIO_S_ISDIR(d.d_stat.st_mode) ||
			    strncasecmp(d.d_name, "CAT_", 4) || !d.d_name[4]) {
				add_item(&lib->cats[0], base, &d);
				continue;
			}

			/* Same CAT_ name under ISO/ and PSP/GAME/ = one category. */
			c = library_find(lib, d.d_name + 4);
			if (!c) {
				lib->cats = xrealloc(lib->cats, (lib->ncats + 1) * sizeof(*c));
				c = &lib->cats[lib->ncats++];
				memset(c, 0, sizeof(*c));
				c->name = xstrdup(d.d_name + 4);
			}
			snprintf(dir, sizeof(dir), "%s/%s", base, d.d_name);
			c->dirs = xrealloc(c->dirs, (c->ndirs + 1) * sizeof(char *));
			c->dirs[c->ndirs++] = xstrdup(dir);
			t = dt_tick(&d.d_stat.sce_st_mtime);
			if (t > c->tick)
				c->tick = t;
			scan_cat_dir(c, dir);
		}
		sceIoDclose(fd);
	}
}

void library_free(Library *lib)
{
	int i, j;

	for (i = 0; i < lib->ncats; i++) {
		Category *c = &lib->cats[i];

		for (j = 0; j < c->nitems; j++) {
			free(c->items[j].path);
			free(c->items[j].name);
			free(c->items[j].key);
		}
		for (j = 0; j < c->ndirs; j++)
			free(c->dirs[j]);
		free(c->items);
		free(c->dirs);
		free(c->name);
	}
	free(lib->cats);
	lib->cats = NULL;
	lib->ncats = 0;
}

static int cmp_cat_name(const void *a, const void *b)
{
	const Category *x = a, *y = b;

	if (!*x->name != !*y->name)
		return !*x->name ? 1 : -1;
	return strcasecmp(x->name, y->name);
}

void library_sort_name(Library *lib)
{
	qsort(lib->cats, lib->ncats, sizeof(Category), cmp_cat_name);
}

void stamper_init(Stamper *s)
{
	ScePspDateTime dt;

	memset(&dt, 0, sizeof(dt));
	dt.year = 2035;
	dt.month = 1;
	dt.day = 1;
	s->base = dt_tick(&dt);
	s->count = 0;
	s->failed = 0;
}

static void stamp(Stamper *s, const char *path)
{
	SceIoStat st;
	u64 t = s->base - (u64)s->count * STEP_US;
	int ret;

	memset(&st, 0, sizeof(st));
	sceRtcSetTick(&st.sce_st_mtime, &t);
	st.sce_st_ctime = st.sce_st_mtime;
	s->count++;

	ret = sceIoChstat(path, &st, CST_MT | CST_CT);
	if (ret < 0) {
		s->failed++;
		warn("chstat %08X: %s", ret, path);
	}
}

static int cmp_az(const void *a, const void *b)
{
	return strcmp((*(Item *const *)a)->key, (*(Item *const *)b)->key);
}

int sort_group(Stamper *s, Category *c, char **listed, int nlisted)
{
	Item **order = xrealloc(NULL, c->nitems * sizeof(Item *));
	char key[256];
	int i, j, nord = 0;

	for (j = 0; j < c->nitems; j++)
		c->items[j].used = 0;

	/* Listed items first, in config order. */
	for (i = 0; i < nlisted; i++) {
		Item *found = NULL;

		make_key(key, sizeof(key), listed[i]);
		for (j = 0; j < c->nitems; j++) {
			Item *it = &c->items[j];

			if (!it->used && !strcmp(it->key, key)) {
				found = it;
				break;
			}
		}
		if (!found) {
			warn("not found: %s", listed[i]);
			continue;
		}
		found->used = 1;
		order[nord++] = found;
	}

	/* Then the rest, alphabetically. */
	j = nord;
	for (i = 0; i < c->nitems; i++)
		if (!c->items[i].used)
			order[nord++] = &c->items[i];
	qsort(order + j, nord - j, sizeof(Item *), cmp_az);

	for (i = 0; i < nord; i++)
		stamp(s, order[i]->path);
	for (i = 0; i < c->ndirs; i++)
		stamp(s, c->dirs[i]);

	free(order);
	return nord;
}
