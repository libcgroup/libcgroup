EXTRA_DIST = init.d/cgconfig.in init.d/cgred.in

if WITH_INITSCRIPT_INSTALL
INITSCRIPTDIR=$(DESTDIR)$(sysconfdir)/rc.d/init.d
install-exec-hook:
	mkdir -p $(INITSCRIPTDIR)
	cp init.d/cgconfig init.d/cgred $(INITSCRIPTDIR)

uninstall-hook:
	rm -f $(INITSCRIPTDIR)/cgconfig $(INITSCRIPTDIR)/cgred
endif
