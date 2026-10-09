# Build xmbsort inside the pspdev Docker toolchain.
#
#   make            build PARAM.SFO and EBOOT.PBP
#   make eboot      build only EBOOT.PBP
#   make sfo        regenerate source/xmb/PARAM.SFO
#   make clean      remove build artifacts and root EBOOT.PBP
#   make shell      interactive shell in the toolchain container
#   make pull       pull/update the toolchain image
#   make deploy     rsync the distributable root to the PSP memory stick (deploy.sh)

IMAGE       ?= pspdev/pspdev:latest
PLATFORM    ?= linux/amd64
DOCKER      ?= docker

# XMB assets (icon0.png, pic1.png, PARAM.SFO) live here.
ASSETS = source/xmb

# Run as the host user so build outputs aren't root-owned.
RUN = $(DOCKER) run --rm --platform $(PLATFORM) -u $(shell id -u):$(shell id -g) \
      -v "$(CURDIR)":/src -w /src $(IMAGE)

.PHONY: all sfo eboot clean shell pull deploy

all: sfo eboot

sfo:
	$(RUN) make -C source/src ../xmb/PARAM.SFO

eboot:
	$(RUN) make -C source/src
	mv -f source/src/EBOOT.PBP EBOOT.PBP

clean:
	$(RUN) make -C source/src clean
	rm -f EBOOT.PBP $(ASSETS)/PARAM.SFO

shell:
	$(DOCKER) run --rm -it --platform $(PLATFORM) -u $(shell id -u):$(shell id -g) \
	  -v "$(CURDIR)":/src -w /src $(IMAGE) bash

pull:
	$(DOCKER) pull --platform $(PLATFORM) $(IMAGE)