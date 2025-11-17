
SUBDIRS := client_libmini_sandbox client_libmini_tapbox

.PHONY: all clean $(SUBDIRS)


all: $(SUBDIRS)


$(SUBDIRS):
	$(MAKE) -C $@


clean:
	@echo "Cleaning .bin files in subdirectories..."	
	@for dir in $(SUBDIRS); do \
        	$(MAKE) -C $$dir clean; \
    	done

