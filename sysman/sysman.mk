PAGES := $(shell find $(SRCDIR)/pages/*)
HTML := $(patsubst $(SRCDIR)/pages/%, html/%.html, $(PAGES))

html/index.html: $(HTML)
	@mkdir -p html
	cd html && ../../build-tools/sysman --index $^

html/%.html: $(SRCDIR)/pages/%
	@mkdir -p html
	../build-tools/sysman --html $< $@

