# Makefile for MDLedit
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:

#
# Default build expects a MinGW-w64 toolchain on PATH. Override CXX and WINDRES
# when using a different triplet, for example:
#   make CXX=i686-w64-mingw32-g++ WINDRES=i686-w64-mingw32-windres

CONFIG ?= release
PROGRAM ?= mdledit.exe
BUILD_ROOT ?= build
STATIC ?= 0
SANITIZE ?= 0
WARNINGS_AS_ERRORS ?= 0
SAFE_BUILD ?= 1
OPT_LEVEL ?= 1
CCACHE ?=
OBJDUMP ?= objdump
DIST_DIR ?= dist
BUNDLE_DIR ?= $(DIST_DIR)/mdledit
RM = rm -rf
MKDIR_P ?= mkdir -p
BUILD_DIR := $(BUILD_ROOT)/$(CONFIG)
OBJ_DIR := $(BUILD_DIR)/obj
BIN := $(BUILD_DIR)/$(PROGRAM)

# In MSYS2 MinGW/UCRT/Clang environments, the active compiler is exposed as
# g++/windres on PATH. Outside those shells, default to the MinGW-w64 cross
# tool names. Users can still override both from the command line.
ifneq (,$(filter MINGW% UCRT% CLANG%,$(MSYSTEM)))
    DEFAULT_CXX := g++
    DEFAULT_WINDRES := windres
else
    DEFAULT_CXX := x86_64-w64-mingw32-g++
    DEFAULT_WINDRES := x86_64-w64-mingw32-windres
endif

ifeq ($(origin CXX),default)
    CXX := $(DEFAULT_CXX)
endif
ifeq ($(origin WINDRES),undefined)
    WINDRES := $(DEFAULT_WINDRES)
endif

CPPFLAGS ?= -DNOMINMAX -include win32_compat.h
CXXFLAGS ?= -std=c++14 -Wall -Wextra -fno-strict-aliasing -pipe
LDFLAGS ?= -mwindows
LDLIBS ?= -lcomctl32 -lcomdlg32 -lshlwapi -lgdi32 -luser32 -lkernel32
WINDRESFLAGS ?= -I. -O coff
CXX_TOOL := $(firstword $(CXX))
WINDRES_TOOL := $(firstword $(WINDRES))
CCACHE_TOOL := $(firstword $(CCACHE))
OBJDUMP_TOOL := $(firstword $(OBJDUMP))
CXX_COMPILE := $(strip $(CCACHE) $(CXX))

ifeq ($(SANITIZE),1)
    STATIC := 0
endif

ifeq ($(SAFE_BUILD),1)
.NOTPARALLEL:
endif

ifeq ($(CONFIG),debug)
    CXXFLAGS += -O0 -g
else
    CXXFLAGS += -O$(OPT_LEVEL) -DNDEBUG
    ifeq ($(STATIC),1)
        LDFLAGS += -static -static-libgcc -static-libstdc++
    endif
endif

ifeq ($(SANITIZE),1)
    CXXFLAGS += -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
    LDFLAGS += -fsanitize=address,undefined
endif

ifeq ($(WARNINGS_AS_ERRORS),1)
    CXXFLAGS += -Werror
endif

SOURCES := $(sort $(wildcard *.cpp))
OBJECTS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)
RESOURCE_DEPS := resource.rc resource.h manifest.xml $(wildcard *.ico)
FORCED_HEADERS := win32_compat.h
RESOURCE_OBJECT := $(OBJ_DIR)/resource.o

.PHONY: all release debug safe safe-dynamic fast portable standalone bundle bundle-runtime \
        runtime-deps verify-portable test clean distclean print-vars check-tools rebuild help
.DELETE_ON_ERROR:

all: check-tools $(BIN)

check-tools:
	@command -v "$(CXX_TOOL)" >/dev/null 2>&1 || { echo "error: CXX not found: $(CXX)"; exit 1; }
	@command -v "$(WINDRES_TOOL)" >/dev/null 2>&1 || { echo "error: WINDRES not found: $(WINDRES)"; exit 1; }
	@if [ -n "$(strip $(CCACHE))" ]; then command -v "$(CCACHE_TOOL)" >/dev/null 2>&1 || { echo "error: CCACHE not found: $(CCACHE)"; exit 1; }; fi

release:
	$(MAKE) CONFIG=release all

debug:
	$(MAKE) CONFIG=debug all

safe: safe-dynamic

# Serial dynamic-runtime build. This is useful for development inside the same
# MSYS2 environment, but the resulting executable requires MinGW runtime DLLs.
safe-dynamic:
	$(MAKE) clean
	$(MAKE) STATIC=0 SAFE_BUILD=1 OPT_LEVEL=1 CONFIG=release -j1 all

# Standalone release build for distribution. Full static linking embeds the
# MinGW GCC/libstdc++/winpthread runtimes while Windows system DLLs remain normal
# imports. A separate CONFIG prevents an older dynamic executable from being
# considered up to date when only linker flags have changed.
portable:
	$(MAKE) CONFIG=portable STATIC=1 SAFE_BUILD=1 OPT_LEVEL=1 -j1 all
	$(MAKE) CONFIG=portable STATIC=1 verify-portable

standalone: portable

# Dynamic alternative: place the executable and any non-system DLLs found next
# to the active compiler in dist/mdledit. Do not download DLLs from third-party
# sites; this target copies the matching files from the toolchain used to link.
bundle:
	$(MAKE) CONFIG=bundle STATIC=0 SAFE_BUILD=1 OPT_LEVEL=1 -j1 all
	$(MAKE) CONFIG=bundle bundle-runtime

fast:
	$(MAKE) STATIC=0 SAFE_BUILD=0 OPT_LEVEL=2 CONFIG=release all

rebuild:
	$(MAKE) clean
	$(MAKE) all

$(BIN): $(OBJECTS) $(RESOURCE_OBJECT) | $(BUILD_DIR)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: %.cpp $(FORCED_HEADERS) | $(OBJ_DIR)
	$(CXX_COMPILE) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(RESOURCE_OBJECT): $(RESOURCE_DEPS) | $(OBJ_DIR)
	$(WINDRES) $(WINDRESFLAGS) -i resource.rc -o $@

$(BUILD_DIR) $(OBJ_DIR):
	$(MKDIR_P) $@

runtime-deps: $(BIN)
	@command -v "$(OBJDUMP_TOOL)" >/dev/null 2>&1 || { echo "error: OBJDUMP not found: $(OBJDUMP)"; exit 1; }
	@echo "Imported DLLs for $(BIN):"
	@$(OBJDUMP) -p "$(BIN)" | sed -n 's/^[[:space:]]*DLL Name:[[:space:]]*//p' | tr -d '\r'

verify-portable: $(BIN)
	@command -v "$(OBJDUMP_TOOL)" >/dev/null 2>&1 || { echo "error: OBJDUMP not found: $(OBJDUMP)"; exit 1; }
	@deps="$$( $(OBJDUMP) -p "$(BIN)" | sed -n 's/^[[:space:]]*DLL Name:[[:space:]]*//p' | tr -d '\r' )"; \
	 echo "Imported DLLs for $(BIN):"; \
	 printf '%s\n' "$$deps"; \
	 if printf '%s\n' "$$deps" | grep -Eiq '^lib.*\.dll$$'; then \
	     echo "error: portable build still imports a non-system lib*.dll runtime" >&2; \
	     exit 1; \
	 fi; \
	 echo "Portable runtime check passed: no imported lib*.dll runtime was found."

bundle-runtime: $(BIN)
	@command -v "$(OBJDUMP_TOOL)" >/dev/null 2>&1 || { echo "error: OBJDUMP not found: $(OBJDUMP)"; exit 1; }
	$(RM) "$(BUNDLE_DIR)"
	$(MKDIR_P) "$(BUNDLE_DIR)"
	cp "$(BIN)" "$(BUNDLE_DIR)/$(PROGRAM)"
	@compiler_dir="$$(dirname "$$(command -v "$(CXX_TOOL)")")"; \
	 deps="$$( $(OBJDUMP) -p "$(BIN)" | sed -n 's/^[[:space:]]*DLL Name:[[:space:]]*//p' | tr -d '\r' )"; \
	 copied=0; \
	 for dll in $$deps; do \
	     if [ -f "$$compiler_dir/$$dll" ]; then \
	         cp "$$compiler_dir/$$dll" "$(BUNDLE_DIR)/$$dll"; \
	         echo "copied $$dll"; \
	         copied=$$((copied + 1)); \
	     fi; \
	 done; \
	 echo "Bundle created at $(BUNDLE_DIR) ($$copied runtime DLL(s) copied)."

test: $(ASCII_COMPAT_TEST)
	"$(ASCII_COMPAT_TEST)"

$(ASCII_COMPAT_TEST): tests/ascii_compat_test.cpp ascii_compat.h | $(TEST_DIR)
	$(CXX) -std=c++14 -Wall -Wextra -Werror -pipe $< -o $@

$(TEST_DIR):
	$(MKDIR_P) $@

clean:
	$(RM) $(BUILD_ROOT)

distclean: clean
	$(RM) mdledit.exe

print-vars:
	@echo CONFIG=$(CONFIG)
	@echo CXX=$(CXX)
	@echo WINDRES=$(WINDRES)
	@echo MSYSTEM=$(MSYSTEM)
	@echo STATIC=$(STATIC)
	@echo SAFE_BUILD=$(SAFE_BUILD)
	@echo OPT_LEVEL=$(OPT_LEVEL)
	@echo SANITIZE=$(SANITIZE)
	@echo WARNINGS_AS_ERRORS=$(WARNINGS_AS_ERRORS)
	@echo CCACHE=$(CCACHE)
	@echo OBJDUMP=$(OBJDUMP)
	@echo DIST_DIR=$(DIST_DIR)
	@echo BUNDLE_DIR=$(BUNDLE_DIR)
	@echo CPPFLAGS=$(CPPFLAGS)
	@echo CXXFLAGS=$(CXXFLAGS)
	@echo LDFLAGS=$(LDFLAGS)
	@echo LDLIBS=$(LDLIBS)
	@echo WINDRESFLAGS=$(WINDRESFLAGS)
	@echo BIN=$(BIN)

help:
	@echo "Targets:"
	@echo "  make            Build release"
	@echo "  make debug      Build debug"
	@echo "  make safe       Serial dynamic-runtime build (development only)"
	@echo "  make portable   Standalone static-runtime build; output build/portable/mdledit.exe"
	@echo "  make standalone Alias for make portable"
	@echo "  make bundle     Dynamic build plus matching runtime DLLs in dist/mdledit"
	@echo "  make fast       Parallel-friendly dynamic-runtime build"
	@echo "  make rebuild    Clean then build"
	@echo "  make clean      Remove build output"
	@echo "  make runtime-deps List DLL imports for the selected CONFIG"
	@echo "  make test       Run parser compatibility regression tests"
	@echo "  make print-vars Show selected tools and flags"
	@echo "  make check-tools Verify compiler/resource compiler are on PATH"
	@echo "Variables:"
	@echo "  CXX=<compiler> WINDRES=<resource compiler> CONFIG=release|debug"
	@echo "  STATIC=1      Embed MinGW runtime libraries using static link flags"
	@echo "  SAFE_BUILD=0  Allow parallel make execution inside this makefile"
	@echo "  OPT_LEVEL=2   Use -O2 instead of the safer/lower-memory default -O1"
	@echo "  SANITIZE=1    Build with address/undefined sanitizers for debugging"
	@echo "  WARNINGS_AS_ERRORS=1 Treat warnings as errors for cleanup work"
	@echo "  CCACHE=ccache  Use ccache for compile steps when installed"
	@echo "  OBJDUMP=objdump Select the PE import inspection tool"

-include $(DEPS)
