
## Makefile.am -- Process this file with automake to produce Makefile.in

# Do not create dependencies while compiling
AUTOMAKE_OPTIONS = no-dependencies

# Subdirectories to MAKE recursively
SUBDIRS          = libmseed evalresp fern sacio src utils t

# Name of the SAC auxiliary data directory.  A single name is used on every
# platform: "aux" cannot be used because it is a reserved DOS device name,
# which Windows does not allow as a directory name.
SACAUX_DIR       = sacaux

EXTRA_ALWAYS = CHANGES Editline.license HISTORY License README.md 

# Extra Subdirectories/Files to Distribute
#   File included here will be included in a Distribution:
#   % make dist
EXTRA_DIST        = $(EXTRA_ALWAYS) \
		    Readme.buildsac \
		    bin inc lib macros include \
                    doc/README doc/examples \
                    $(SACAUX_DIR)

# Extra Subdirectories/Files to install 
#   Files included here will be included in a Binary Distribution:
#   % make install
#   % make dist-binary
EXTRA_INSTALL    = macros $(EXTRA_ALWAYS) \
	           utils/saclst.c utils/sacswap.c utils/sgftops.c \
		   utils/bbfswap.c utils/sgfswap.c utils/Makefile.utils \
		   utils/readsac.m utils/writesac.m \
		   utils/padcat.m utils/getsacdata.m utils/sacset.c \
		   utils/unvis.c utils/README_utils utils/sacdiff.c \
		   lib/README_lib \
		   include doc/examples doc/README $(SACAUX_DIR)

include_HEADERS = include/sacio.h include/evalresp.h \
                  include/sac.h include/sacf.h \
                  include/timespec.h

bin_SCRIPTS = bin/sacinit.sh \
	      bin/sacinit.csh \
	      bin/sgftoeps.csh \
	      bin/sgftox.csh \
	      bin/sac-config

include/sacio.h: $(top_srcdir)/sacio/sacio.h
	mkdir -p include
	cp $(top_srcdir)/sacio/sacio.h include/sacio.h
include/timespec.h: $(top_srcdir)/sacio/timespec.h
	mkdir -p include
	cp $(top_srcdir)/sacio/timespec.h include/timespec.h


CLEANFILES =  bin/sacinit.sh bin/sacinit.csh bin/sac-config

# Rules to install Extra Subdirectories and Files
install-data-hook:
	$(INSTALL) -d $(prefix)/utils
	if [ -e $(prefix)/lib64 ]; then \
     cd $(prefix); ln -s lib64 lib; \
  fi
	cd $(srcdir); \
	for d in $(EXTRA_INSTALL); do \
	   for f in `(find $$d -type d)`; do \
	      $(INSTALL) -d $(prefix)/$$f; \
	   done; \
	   for f in `(find $$d -type f)`; do \
        $(INSTALL_DATA) $$f $(prefix)/$$f; \
	   done; \
	done; \
	cd -
	( cd $(prefix)/lib && rm -f sacio.a )
	( cd $(prefix)/lib && $(LN_S) libsacio.a sacio.a )
	$(MAKE) dist-hook

dist-save-time: distdir
	tardir=$(distdir) && ${AMTAR} --atime-preserve -chof - "$$tardir" | GZIP=$(GZIP_ENV) gzip -c > $(distdir).tar.gz
	$(am__remove_distdir)

dist-binary: dist-binary-gzip

dist-binary-dir:
	$(am__remove_distdir)
	mkdir $(distdir)

dist-binary-install: dist-binary-dir 
	$(MAKE) prefix=$(PWD)/$(distdir) install 
	mkdir -p bin
	$(SED) "s,__SAC_PREFIX__,/usr/local/sac," $(abs_top_srcdir)/bin/sacinit.csh.in > $(PWD)/$(distdir)/bin/sacinit.csh
	$(SED) "s,__SAC_PREFIX__,/usr/local/sac," $(abs_top_srcdir)/bin/sacinit.sh.in > $(PWD)/$(distdir)/bin/sacinit.sh
	$(SED) -e "s,__SAC_PREFIX__,/usr/local/sac," -e "s,__SAC_VERSION__,$(VERSION)," $(abs_top_srcdir)/bin/sac-config.in > $(PWD)/$(distdir)/bin/sac-config
	mv $(PWD)/$(distdir) $(PWD)/$(PACKAGE)

dist-binary-gzip: dist-binary-install
	tardir=$(PACKAGE) && $(am__tar) |  GZIP=$(GZIP_ENV) gzip -c > $(distdir)-$(BINARY_DIST).tar.gz
	mv $(PWD)/$(PACKAGE) $(PWD)/$(distdir)
	$(am__remove_distdir)

bin/sacinit.sh: $(abs_top_srcdir)/bin/sacinit.sh.in
	@mkdir -p bin
	$(SED) "s,__SAC_PREFIX__,$(prefix)," $(abs_top_srcdir)/bin/sacinit.sh.in > bin/sacinit.sh

bin/sacinit.csh: $(abs_top_srcdir)/bin/sacinit.csh.in
	@mkdir -p bin
	$(SED) "s,__SAC_PREFIX__,$(prefix)," $(abs_top_srcdir)/bin/sacinit.csh.in > bin/sacinit.csh

bin/sac-config: $(abs_top_srcdir)/bin/sac-config.in
	@mkdir -p bin
	$(SED) -e "s,__SAC_PREFIX__,$(prefix)," -e "s,__SAC_VERSION__,$(VERSION)," $(abs_top_srcdir)/bin/sac-config.in > bin/sac-config

# Run when creating a distribtion
#   - Remove the CVS files
dist-hook:
	test ! -d $(distdir) || rm -rf `find $(distdir) -name CVS -type d `
	test ! -d $(distdir)/inc || rm -rf `find $(distdir)/inc -name config.h  `

# Cygwin source distribution.
# This used to rename the aux directory to "winaux" because "aux" is not a
# valid directory name on Windows.  The tree now ships a single "sacaux" name
# on every platform, so the distribution can be shipped as-is.
dist-cygwin: distdir
	tardir=$(distdir) && $(am__tar) | GZIP=$(GZIP_ENV) gzip -c >$(distdir)-cygwin.tar.gz
	$(am__remove_distdir)

check-clean:
	cd t && $(MAKE) clean

check-libsac:
	cd t && $(MAKE) check-TESTS

coverage:
	rm -f sac_coverage.info
	lcov --base-directory src --directory src -c -o sac_coverage.info
	lcov --remove sac_coverage.info "/usr*" -o sac_coverage.info
	lcov --remove sac_coverage.info "*/expr_*" -o sac_coverage.info
	lcov --remove sac_coverage.info "*<stdout>*" -o sac_coverage.info
	genhtml -o sac_coverage sac_coverage.info

if OSX_APP

all-local: Sacapp

Sacapp: 
	@echo " CREATE " Sac.app
	rm -rf Sac.app
	cp -fpR $(abs_top_srcdir)/src/osx/Sac.app .
	cp -fpR $(abs_top_srcdir)/$(SACAUX_DIR) Sac.app/Contents/Resources
	mkdir $(SACAPP_RSC)/doc
	cp -fpR $(abs_top_srcdir)/doc/README $(SACAPP_RSC)/doc
	cp -fpR $(abs_top_srcdir)/doc/examples $(SACAPP_RSC)/doc
	cd utils && $(MAKE) sacapp
	cd src && $(MAKE) sacapp

clean-local:
	rm -rf Sac.app

dmg: Sac.app
	mkdir dmgdir
	cp -fpR Sac.app dmgdir
	cd dmgdir && ln -s /Applications .
	hdiutil create -format UDZO -volname "SAC Community Edition v$(PACKAGE_VERSION)" -srcfolder dmgdir ./SAC-$(PACKAGE_VERSION).dmg
	rm -fr dmgdir
endif
