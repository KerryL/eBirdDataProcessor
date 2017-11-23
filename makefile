# makefile (eBirdDataProcessor)
#
# Include the common definitions
include makefile.inc

# Name of the executable to compile and link
TARGET = eBirdDataProcessor
TARGET_DEBUG = $(TARGET)d

# Directories in which to search for source files
DIRS = \
	src \
	src/email \
    src/email/cJSON \
	src/utilities

# Source files
SRC = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.cpp))
SRC_C = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.c))

# Remove cJSON test files (which define their own main())
SRC_C := $(filter-out $(wildcard */*/*/test*),$(SRC_C))
# TODO:  Improve this so we don't have to specify form of path

# Object files
OBJS_DEBUG = $(addprefix $(OBJDIR_DEBUG),$(SRC:.cpp=.o))
OBJS_DEBUG_C = $(addprefix $(OBJDIR_DEBUG),$(SRC_C:.c=.o))
OBJS_RELEASE = $(addprefix $(OBJDIR_RELEASE),$(SRC:.cpp=.o))
OBJS_RELEASE_C = $(addprefix $(OBJDIR_RELEASE),$(SRC_C:.c=.o))
OBJS_DEBUG_ALL = $(OBJS_DEBUG) $(OBJS_DEBUG_C)
OBJS_RELEASE_ALL = $(OBJS_RELEASE) $(OBJS_RELEASE_C)

.PHONY: all debug clean

all: $(TARGET)
debug: $(TARGET_DEBUG)

$(TARGET): $(OBJS_RELEASE) $(OBJS_RELEASE_C)
	$(MKDIR) $(BINDIR)
	$(CC) $(OBJS_RELEASE_ALL) $(LDFLAGS_RELEASE) -L$(LIBOUTDIR) $(addprefix -l,$(PSLIB)) -o $(BINDIR)$@

$(TARGET_DEBUG): $(OBJS_DEBUG) $(OBJS_DEBUG_C)
	$(MKDIR) $(BINDIR)
	$(CC) $(OBJS_DEBUG_ALL) $(LDFLAGS_DEBUG) -L$(LIBOUTDIR) $(addprefix -l,$(PSLIB)) -o $(BINDIR)$@

$(OBJDIR_RELEASE)%.o: %.cpp
	$(MKDIR) $(dir $@)
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

$(OBJDIR_DEBUG)%.o: %.cpp
	$(MKDIR) $(dir $@)
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

$(OBJDIR_RELEASE)%.o: %.c
	$(MKDIR) $(dir $@)
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

$(OBJDIR_DEBUG)%.o: %.c
	$(MKDIR) $(dir $@)
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

clean:
	$(RM) -r $(OBJDIR)
	$(RM) $(BINDIR)$(TARGET)
	$(RM) $(BINDIR)$(TARGET_DEBUG)
