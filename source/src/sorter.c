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

static void scan_cat_dir(Category *c, const char *dir)
{
	SceIoDirent d;
	char path[512], key[256];
	int fd = sceIoDopen(dir);

	if (fd < 0) {
		warn("cannot open %s", dir);
		return;
	}
	for (;;) {
		Item *it;

		memset(&d, 0, sizeof(d));
		if (sceIoDread(fd, &d) <= 0)
			break;
		if (!strcmp(d.d_name, ".") || !strcmp(d.d_name, ".."))
			continue;

		snprintf(path, sizeof(path), "%s/%s", dir, d.d_name);
		make_key(key, sizeof(key), d.d_name);
		c->items = xrealloc(c->items, (c->nitems + 1) * sizeof(*it));
		it = &c->items[c->nitems++];
		it->path = xstrdup(path);
		it->name = xstrdup(d.d_name);
		it->key = xstrdup(key);
		it->tick = dt_tick(&d.d_stat.sce_st_mtime);
		it->used = 0;
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

	lib->cats = NULL;
	lib->ncats = 0;

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
			if (!FIO_S_ISDIR(d.d_stat.st_mode) ||
			    strncasecmp(d.d_name, "CAT_", 4) || !d.d_name[4])
				continue;

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

static int cmp_item_current(const void *a, const void *b)
{
	const Item *x = a, *y = b;

	if (x->tick != y->tick)
		return x->tick > y->tick ? -1 : 1;
	return strcmp(x->key, y->key);
}

static int cmp_cat_current(const void *a, const void *b)
{
	const Category *x = a, *y = b;

	if (x->tick != y->tick)
		return x->tick > y->tick ? -1 : 1;
	return strcasecmp(x->name, y->name);
}

void library_sort_current(Library *lib)
{
	int i;

	qsort(lib->cats, lib->ncats, sizeof(Category), cmp_cat_current);
	for (i = 0; i < lib->ncats; i++)
		qsort(lib->cats[i].items, lib->cats[i].nitems, sizeof(Item),
		      cmp_item_current);
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

static int cmp_za(const void *a, const void *b)
{
	return -cmp_az(a, b);
}

int sort_group(Stamper *s, Category **cats, int ncats,
	       char **listed, int nlisted, int za)
{
	Item **order;
	char key[256];
	int i, j, n = 0, nord = 0;

	for (i = 0; i < ncats; i++)
		n += cats[i]->nitems;
	order = xrealloc(NULL, n * sizeof(Item *));

	for (i = 0; i < ncats; i++)
		for (j = 0; j < cats[i]->nitems; j++)
			cats[i]->items[j].used = 0;

	/* Listed items first, in config order. */
	for (i = 0; i < nlisted; i++) {
		Item *found = NULL;
		int c;

		make_key(key, sizeof(key), listed[i]);
		for (c = 0; c < ncats && !found; c++)
			for (j = 0; j < cats[c]->nitems; j++) {
				Item *it = &cats[c]->items[j];

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
	for (i = 0; i < ncats; i++) {
		int k;

		for (k = 0; k < cats[i]->nitems; k++)
			if (!cats[i]->items[k].used)
				order[nord++] = &cats[i]->items[k];
	}
	qsort(order + j, nord - j, sizeof(Item *), za ? cmp_za : cmp_az);

	for (i = 0; i < nord; i++)
		stamp(s, order[i]->path);
	for (i = 0; i < ncats; i++)
		for (j = 0; j < cats[i]->ndirs; j++)
			stamp(s, cats[i]->dirs[j]);

	free(order);
	return nord;
}
