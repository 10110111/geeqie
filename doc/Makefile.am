## Process this file with automake to produce Makefile.in.

helpdir = @htmldir@

# empty help_DATA just installs the directory
help_DATA = 

EXTRA_DIST = docbook2html.sh docbook

html/GuideIndex.html: docbook/GuideIndex.xml
	rm -rf html; mkdir html && gnome-doc-tool html -o html $(srcdir)/docbook/GuideIndex.xml

html: html/GuideIndex.html

install-data-hook: html
	cd html; for f in * ; do $(INSTALL_DATA) "$$f" "$(DESTDIR)$(helpdir)/$$f" ; done
	ln -s GuideIndex.html "$(DESTDIR)$(helpdir)/index.html"
	
uninstall-hook:
	rm "$(DESTDIR)$(helpdir)/"*

clean-local:
	rm -rf html

