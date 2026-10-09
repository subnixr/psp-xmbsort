#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "config.h"
#include "sorter.h"

PSP_MODULE_INFO("xmbsort", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define EXIT_DELAY_S	3
#define USER_BUTTONS	(PSP_CTRL_SELECT | PSP_CTRL_START | PSP_CTRL_UP | \
			 PSP_CTRL_RIGHT | PSP_CTRL_DOWN | PSP_CTRL_LEFT | \
			 PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | \
			 PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE | \
			 PSP_CTRL_CROSS | PSP_CTRL_SQUARE)

static int exit_callback(int arg1, int arg2, void *common)
{
	sceKernelExitGame();
	return 0;
}

static int callback_thread(SceSize args, void *argp)
{
	int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);
	sceKernelSleepThreadCB();
	return 0;
}

static void setup_callbacks(void)
{
	int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, NULL);
	if (thid >= 0)
		sceKernelStartThread(thid, 0, NULL);
}

/* Device root ("ms0:" / "ef0:") and config.txt next to the EBOOT. */
static void get_paths(int argc, char *argv[], char *root, size_t rn,
		      char *cfg, size_t cn)
{
	const char *self = argc > 0 && argv[0] ? argv[0] : "";
	const char *colon = strchr(self, ':');
	const char *slash = strrchr(self, '/');

	if (!colon || !slash) {
		snprintf(root, rn, "ms0:");
		snprintf(cfg, cn, "ms0:/PSP/GAME/xmbsort/config.txt");
		return;
	}
	snprintf(root, rn, "%.*s", (int)(colon - self + 1), self);
	snprintf(cfg, cn, "%.*s/config.txt", (int)(slash - self), self);
}

/* Wait for a fresh button press; forever if secs <= 0. */
static void wait_button(int secs)
{
	SceCtrlData pad;
	unsigned int prev;
	int i;

	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
	sceCtrlPeekBufferPositive(&pad, 1);
	prev = pad.Buttons;
	for (i = 0; secs <= 0 || i < secs * 60; i++) {
		sceCtrlPeekBufferPositive(&pad, 1);
		if (pad.Buttons & ~prev & USER_BUTTONS)
			break;
		prev = pad.Buttons;
		sceDisplayWaitVblankStart();
	}
}

static int cmp_cat_name(const void *a, const void *b)
{
	return strcasecmp((*(Category *const *)a)->name,
			  (*(Category *const *)b)->name);
}

/* On-screen name; the uncategorized one shows as in config.txt. */
static const char *label(const char *name)
{
	return *name ? name : NONE_MARK;
}

static void run(const Config *cfg, Library *lib)
{
	Stamper st;
	Category **group = xrealloc(NULL, lib->ncats * sizeof(Category *));
	int i, n, ng;

	stamper_init(&st);

	for (i = 0; i < cfg->nsecs; i++) {
		const Section *sec = &cfg->secs[i];
		Category *c = library_find(lib, sec->name);

		pspDebugScreenPrintf("%-16s", label(sec->name));
		if (!c) {
			pspDebugScreenPrintf("\n");
			warn("no CAT_ folder");
			continue;
		}
		if (c->used) {
			pspDebugScreenPrintf("\n");
			warn("%s: already used above", label(c->name));
			continue;
		}
		c->used = 1;
		n = sort_group(&st, c, sec->items, sec->nitems);
		pspDebugScreenPrintf("%3d items\n", n);
	}

	/* Categories missing from config go last, A-Z. */
	ng = 0;
	for (i = 0; i < lib->ncats; i++) {
		Category *c = &lib->cats[i];

		/* Nothing uncategorized: nothing to report. */
		if (c->used || (!c->nitems && !strcmp(c->name, NONE_NAME)))
			continue;
		group[ng++] = c;
	}
	qsort(group, ng, sizeof(Category *), cmp_cat_name);
	for (i = 0; i < ng; i++) {
		pspDebugScreenPrintf("%-16s", label(group[i]->name));
		n = sort_group(&st, group[i], NULL, 0);
		pspDebugScreenPrintf("%3d items\n", n);
		warn("not in config.txt");
	}

	pspDebugScreenPrintf("\n%d timestamps set, %d failed.\n",
			     st.count - st.failed, st.failed);
	free(group);
}

int main(int argc, char *argv[])
{
	char root[16], cfgpath[256];
	Library lib;
	Config cfg;

	setup_callbacks();
	pspDebugScreenInit();
	pspDebugScreenPrintf("xmbsort\n\n");

	get_paths(argc, argv, root, sizeof(root), cfgpath, sizeof(cfgpath));
	library_scan(&lib, root);

	if (config_load(&cfg, cfgpath) < 0) {
		library_sort_name(&lib);
		if (config_generate(cfgpath, &lib) == 0)
			pspDebugScreenPrintf("config.txt not found; generated\n%s\n"
					     "Edit it and run again.\n",
					     cfgpath);
		else
			warn("cannot write %s", cfgpath);
	} else {
		run(&cfg, &lib);
		config_free(&cfg);
	}
	library_free(&lib);

	/* Keep warnings on screen until acknowledged. */
	if (warnings()) {
		pspDebugScreenPrintf("\n%d warning(s). Press any button to exit.\n",
				     warnings());
		wait_button(0);
	} else {
		pspDebugScreenPrintf("\nExiting in %d s...\n", EXIT_DELAY_S);
		wait_button(EXIT_DELAY_S);
	}
	sceKernelExitGame();
	return 0;
}
