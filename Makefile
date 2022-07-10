SUBDIRS = libiommu test
all:
	@for i in $(SUBDIRS); do \
	    echo "Building $$i";\
	    (cd $$i; $(MAKE) -s --no-print-directory); done
clean:
	@for i in $(SUBDIRS); do \
	    (cd $$i; $(MAKE) -s --no-print-directory clean); done
