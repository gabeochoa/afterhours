include ../../common.mk
FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -g
INCLUDES = -I../../.. -I../../../vendor -I../../../../../vendor

all:
	clang++ $(FLAGS) $(INCLUDES) main.cpp ../../../src/plugins/files.cpp ../../../src/plugins/settings.cpp -o $(OUT)/settings_plugin.exe && $(OUT)/settings_plugin.exe

