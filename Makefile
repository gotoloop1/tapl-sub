NAME := tapl

CXX := g++ -g2 -Wextra -std=c++17

SOURCE_DIR := src
LIB_DIR    := lib
BUILD_DIR  := build
TMP_DIR    := build/src_tmp

SOURCE_NAMES := $(notdir $(wildcard $(SOURCE_DIR)/*.cpp)) \
								json11.cpp
HEADER_NAMES := $(notdir $(wildcard $(SOURCE_DIR)/*.hpp)) \
								json11.hpp
TMP_SOURCES  := $(addprefix $(TMP_DIR)/,$(SOURCE_NAMES))
TMP_HEADERS  := $(addprefix $(TMP_DIR)/,$(HEADER_NAMES))
OBJS         := $(addprefix $(BUILD_DIR)/,$(SOURCE_NAMES:.cpp=.o))
DEPENDS      := $(addprefix $(BUILD_DIR)/,$(SOURCE_NAMES:.cpp=.depend))

all: $(DEPENDS) $(NAME)

$(NAME): $(OBJS)
	$(CXX) -o $(NAME) $^

$(BUILD_DIR)/%.o: $(TMP_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) -c -o $@ $^

$(TMP_DIR)/%.cpp: $(SOURCE_DIR)/%.cpp
	@mkdir -p $(TMP_DIR)
	@cp $< $@
$(TMP_DIR)/%.hpp: $(SOURCE_DIR)/%.hpp
	@mkdir -p $(TMP_DIR)
	@cp $< $@

$(TMP_DIR)/json11.cpp: $(LIB_DIR)/json11/json11.cpp
	@mkdir -p $(TMP_DIR)
	@cp $< $@
$(TMP_DIR)/json11.hpp: $(LIB_DIR)/json11/json11.hpp
	@mkdir -p $(TMP_DIR)
	@cp $< $@

$(BUILD_DIR)/%.depend: $(TMP_DIR)/%.cpp $(TMP_HEADERS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) -MM $< > $@
ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDS)
endif

.PHONY: clean
clean:
	rm -rf $(TMP_DIR)
	rm -rf $(BUILD_DIR)
	rm -f $(NAME)
