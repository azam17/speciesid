CC ?= gcc
CFLAGS = -Wall -Wextra -Werror -O2 -std=c11 -Ilib -Isrc
LDFLAGS = -lz -lm -lpthread

SRC_DIR = src
LIB_DIR = lib
TEST_DIR = test
BUILD_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
MAIN_OBJ = $(BUILD_DIR)/main.o
LIB_OBJS = $(filter-out $(MAIN_OBJ),$(OBJS))

TARGET = speciesid

TEST_SRCS = $(wildcard $(TEST_DIR)/test_*.c)
TEST_BINS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/%,$(TEST_SRCS))

SDL2_CFLAGS = $(shell sdl2-config --cflags 2>/dev/null)
SDL2_LIBS   = $(shell sdl2-config --libs 2>/dev/null)

GUI_DIR = gui
GUI_SRCS = $(GUI_DIR)/main_gui.c $(GUI_DIR)/gui_analysis.c $(LIB_DIR)/tinyfiledialogs.c
GUI_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(GUI_SRCS)))

APP_BUNDLE  = SpeciesID.app
APP_BIN     = $(APP_BUNDLE)/Contents/MacOS/SpeciesID
APP_FW_DIR  = $(APP_BUNDLE)/Contents/Frameworks
APP_RES_DIR = $(APP_BUNDLE)/Contents/Resources

.PHONY: all clean test gui

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

# Test targets
$(BUILD_DIR)/test_%: $(TEST_DIR)/test_%.c $(LIB_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) $(LDFLAGS) -o $@

test: $(TEST_BINS)
	@echo "=== Running SpeciesID Tests ==="
	@pass=0; fail=0; \
	for t in $(TEST_BINS); do \
		name=$$(basename $$t); \
		printf "  %-30s " "$$name"; \
		if $$t > /dev/null 2>&1; then \
			echo "PASS"; \
			pass=$$((pass + 1)); \
		else \
			echo "FAIL"; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	echo "=== $$pass passed, $$fail failed ===";\
	[ $$fail -eq 0 ]

# --- GUI targets ---
$(BUILD_DIR)/main_gui.o: $(GUI_DIR)/main_gui.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SDL2_CFLAGS) -Igui -c $< -o $@

$(BUILD_DIR)/gui_analysis.o: $(GUI_DIR)/gui_analysis.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SDL2_CFLAGS) -Igui -c $< -o $@

$(BUILD_DIR)/tinyfiledialogs.o: $(LIB_DIR)/tinyfiledialogs.c | $(BUILD_DIR)
	$(CC) -Wall -O2 -c $< -o $@

gui: $(TARGET) $(LIB_OBJS) $(GUI_OBJS)
	mkdir -p $(APP_BUNDLE)/Contents/MacOS $(APP_FW_DIR) $(APP_RES_DIR)
	$(CC) $(LIB_OBJS) $(GUI_OBJS) $(LDFLAGS) $(SDL2_LIBS) \
	    -framework Cocoa -o $(APP_BIN)
	cp $(GUI_DIR)/Info.plist $(APP_BUNDLE)/Contents/
	@# --- Embed SDL2 dylib for self-contained distribution ---
	@SDL2_DYLIB=$$(otool -L $(APP_BIN) | sed -n 's/^[[:space:]]*\(.*libSDL2[^ ]*\.dylib\).*/\1/p' | head -1); \
	if [ -n "$$SDL2_DYLIB" ] && [ -f "$$SDL2_DYLIB" ]; then \
	    DYLIB_NAME=$$(basename "$$SDL2_DYLIB"); \
	    echo "Embedding $$SDL2_DYLIB -> $(APP_FW_DIR)/$$DYLIB_NAME"; \
	    cp "$$SDL2_DYLIB" "$(APP_FW_DIR)/$$DYLIB_NAME"; \
	    chmod 755 "$(APP_FW_DIR)/$$DYLIB_NAME"; \
	    install_name_tool -change "$$SDL2_DYLIB" \
	        "@executable_path/../Frameworks/$$DYLIB_NAME" $(APP_BIN); \
	    echo "Rewritten load path:"; \
	    otool -L $(APP_BIN) | grep SDL2; \
	else \
	    echo "Warning: SDL2 dylib not found or not a file, skipping embed"; \
	fi
	@# --- Build default index ---
	./$(TARGET) build-db -o /tmp/_sid_build.db
	./$(TARGET) index -d /tmp/_sid_build.db -o $(APP_RES_DIR)/default.idx
	rm -f /tmp/_sid_build.db
	@# --- Ad-hoc codesign (after all resources are in place) ---
	codesign --force --sign - --deep $(APP_BUNDLE) 2>/dev/null || true
	@echo "=== $(APP_BUNDLE) ready ==="

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(APP_BUNDLE)
