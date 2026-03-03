.PHONY: default all clean distclean
.PHONY: dev doc test image run debug

default: test image
all: doc test dev image
dev: tags compile_commands.json

# Compiling the Project Source for Different Targets (Host or Cross-Compile)
# ======================================================================

.PHONY:

host	:= $(shell gcc -dumpmachine)
target	?= i386-elf

out/%/Makefile: src/configure
	@mkdir -p $(@D)
	cd $(@D) && ../../src/configure --target=$*

# Testing on Host
# ======================================================================

test: | out/$(host)/Makefile
	$(MAKE) -C out/$(host) test processes

# Creating Image for Target
# ======================================================================

image: | out/$(target)/Makefile
	$(MAKE) -C out/$(target) image

run: | out/$(target)/Makefile
	$(MAKE) -C out/$(target) run

debug: | out/$(target)/Makefile
	$(MAKE) -C out/$(target) debug

# Tags and IDE Configuration
# ======================================================================

# ctags database for old-school editors like vim
tags: $(shell find src/ -type f)
	ctags -R src/ || true

# Compilation Database for clang/LSP/VSCode
compile_commands.json: $(shell find src/ -type f) | out/$(target)/Makefile
	bear -- $(MAKE) -B -C out/$(target) image || true

# Documentation
# ======================================================================

# Doxygen: Generate Documentation from C Comments
# ----------------------------------------------------------------------

doxygen_html := out/doxygen/html/index.html
doxygen_srcs := $(shell find src out/$(target) -name '*.[ch]')
doxygen_docs := $(shell find . -name '*.md' -o -name '*.dox')

doc: $(doxygen_html)
$(doxygen_html): Doxyfile doxygen-filter.sh $(doxygen_srcs) $(doxygen_docs) \
		| out/$(target)/Makefile
	doxygen

# Code Formatting
# ======================================================================

fmt:
	find src -name '*.[ch]' | xargs clang-format -i

# Cleanup
# ======================================================================

clean:
	@# Gentle clean: Ask target and host directories to clean themselves.
	for outdir in "out/$(target)" "out/$(host)"; do \
		if [ -f "$$outdir/Makefile" ]; then \
			$(MAKE) -C "$$outdir" clean; \
		fi \
	done

	@# Remove generated documentation
	$(RM) -r out/doxygen

	@# Remove Markdown previews.
	find . -name '*.md.html' -delete

distclean:
	@# Full clean: Remove the entire output directory.
	$(RM) -r out/
	$(RM) compile_commands.json TAGS tags

	@# Remove possible caches and other tooling leftovers.
	find . -name '*.orig' -delete
	find . -name '*.pyc' -delete
	$(RM) -r .cache/ __pycache__/
