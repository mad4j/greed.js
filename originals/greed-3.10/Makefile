# Makefile for Greed

VERS=3.10

SFILE=/usr/games/lib/greed.hs
# Location of game executable
BIN=/usr/games

greed: greed.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -DSCOREFILE=\"$(SFILE)\" -DRELEASE=\"$(VERS)\" -o greed greed.c -O3 -lcurses

greed.6: greed.xml
	xmlto man greed.xml

greed.html: greed.xml
	xmlto html-nochunks greed.xml

install: greed.6 uninstall
	cp greed $(BIN)
	cp greed.6 /usr/share/man/man6/greed.6

uninstall:
	rm -f $(BIN)/install /usr/share/man/man6/greed.6

clean:
	rm -f *~ *.o greed greed-*.tar.gz  greed*.rpm *.html
	rm -f greed.6 manpage.links manpage.refs

SOURCES = README NEWS COPYING Makefile greed.c greed.xml control

greed-$(VERS).tar.gz: $(SOURCES) greed.6
	@ls $(SOURCES) greed.6 | sed s:^:greed-$(VERS)/: >MANIFEST
	@(cd ..; ln -s greed greed-$(VERS))
	(cd ..; tar -czf greed/greed-$(VERS).tar.gz `cat greed/MANIFEST`)
	@(cd ..; rm greed-$(VERS))

dist: greed-$(VERS).tar.gz

release: greed-$(VERS).tar.gz greed.html
	shipper version=$(VERS) | sh -e -x
