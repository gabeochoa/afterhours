FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -g
INCLUDES = -I../../.. -I../../../vendor -I../../../../../vendor

all:
	clang++ $(FLAGS) $(INCLUDES) main.cpp ../../../src/plugins/files.cpp ../../../src/plugins/settings.cpp -o settings_plugin.exe && ./settings_plugin.exe

