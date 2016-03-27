#
# Makefile for Asterisk PicoTTS application
#
# This program is free software, distributed under the terms of
# the GNU General Public License Version 2. See the COPYING file
# at the top of the source tree.


INSTALL=install
ASTLIBDIR:=$(shell awk '/moddir/{print $$3}' /etc/asterisk/asterisk.conf)
ifeq ($(strip $(ASTLIBDIR)),)
        MODULES_DIR=$(INSTALL_PREFIX)/usr/lib/asterisk/modules
else
        MODULES_DIR=$(INSTALL_PREFIX)$(ASTLIBDIR)
endif
ASTETCDIR=$(INSTALL_PREFIX)/etc/asterisk
SAMPLENAME=app_picotts.conf.sample

CC=gcc
OPTIMIZE=-O2
DEBUG=-g

LIBS+=
CFLAGS+= -pipe -fPIC -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -D_REENTRANT  -D_GNU_SOURCE  

all: _all
	@echo " +-------- app_picotts Build Complete -------+"  
	@echo " + app_picotts has successfully been built,  +"  
	@echo " + and can be installed by running:          +"
	@echo " +                                           +"
	@echo " +               make install                +"  
	@echo " +-------------------------------------------+" 

_all: app_picotts.so


app_picotts.o: app_picotts.c
	$(CC) $(CFLAGS) $(DEBUG) $(OPTIMIZE) -c -o app_picotts.o app_picotts.c

app_picotts.so: app_picotts.o
	$(CC) -shared -Xlinker -x -o $@ $< $(LIBS)


clean:
	rm -f app_picotts.o app_picotts.so .*.d *.o *.so *~

install: _all
	$(INSTALL) -m 755 -d $(DESTDIR)$(MODULES_DIR)
	$(INSTALL) -m 755 app_picotts.so $(DESTDIR)$(MODULES_DIR)
	
	@echo " +---- app_picotts Installation Complete ------+"  
	@echo " +                                             +"
	@echo " + app_picotts has successfully been installed +"  
	@echo " + If you would like to install the sample     +"  
	@echo " + configuration file run:                     +"
	@echo " +                                             +"
	@echo " +              make samples                   +"
	@echo " +---------------------------------------------+"

as_flite: _flite
	@echo " +------ app_picotts Built as app_flite -------+"  
	@echo " + app_picotts has successfully been built as  +"  
	@echo " + app_flite.                                  +"
	@echo " +                                             +"
	@echo " + CAUTION:                                    +"
	@echo " +                                             +"
	@echo " + if installed it will overwrite and replace  +"
	@echo " + app_flite                                   +"
	@echo " +                                             +"
	@echo " +---------------------------------------------+" 
	@echo " + To replace app_flite, run:                  +"
	@echo " +         make install_over_flite             +"  
	@echo " +---------------------------------------------+" 

_flite: app_flite.so

app_flite.o: app_picotts.c
	$(CC) $(CFLAGS) -DAS_FLITE $(DEBUG) $(OPTIMIZE) -c -o app_flite.o app_picotts.c

app_flite.so: app_flite.o
	$(CC) -shared -Xlinker -x -o $@ $< $(LIBS)

install_over_flite: _flite
	$(INSTALL) -m 755 -d $(DESTDIR)$(MODULES_DIR)
	$(INSTALL) -m 755 app_flite.so $(DESTDIR)$(MODULES_DIR)
	
	@echo " +---- app_picotts has replaced app_flite -----+"  
	@echo " +                                             +"
	@echo " + app_picotts has successfully been installed +"  
	@echo " + over app_flite                              +"  
	@echo " +---------------------------------------------+"

samples:
	@mkdir -p $(DESTDIR)$(ASTETCDIR)
	@if [ -f $(DESTDIR)$(ASTETCDIR)/$(CONFNAME) ]; then \
		echo "Backing up previous config file as $(CONFNAME).old";\
		mv -f $(DESTDIR)$(ASTETCDIR)/$(CONFNAME) $(DESTDIR)$(ASTETCDIR)/$(CONFNAME).old ; \
	fi ;
	$(INSTALL) -m 644 $(SAMPLENAME) $(DESTDIR)$(ASTETCDIR)/$(CONFNAME)
	@echo " ------- app_picotts config Installed ---------"

