TARGET = xmbsort
OBJS = main.o config.o sorter.o

INCDIR =
CFLAGS = -O2 -G0 -Wall -std=gnu99
CFLAGS += $(EXTRA_CFLAGS)
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PSP_FW_VERSION = 661

LIBDIR =
# build.mak already appends -lpspdebug -lpspdisplay -lpspge -lpspctrl
# -lpspnet -lpspnet_apctl; don't repeat those.
# psprtc: timestamp math for sceIoChstat.
LIBS = -lpsprtc
LDFLAGS =

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = xmbsort
PSP_EBOOT_SFO = ../xmb/PARAM.SFO
# Optional XMB images: used only if present in xmb/.
PSP_EBOOT_ICON = $(if $(wildcard ../xmb/icon0.png),../xmb/icon0.png,NULL)
PSP_EBOOT_PIC1 = $(if $(wildcard ../xmb/pic1.png),../xmb/pic1.png,NULL)

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

# build.mak's PARAM.SFO rule has no prerequisites; always regenerate it
# so title/flag changes are picked up.
.PHONY: $(PSP_EBOOT_SFO)
