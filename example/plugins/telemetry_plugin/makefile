FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -g
INCLUDES = -I../../.. -I../../../vendor -I../../../../../vendor
EXE = telemetry_plugin.exe
LOG = telemetry_requests.log

all:
	clang++ $(FLAGS) $(INCLUDES) main.cpp -o $(EXE) && ./$(EXE)

validate-localhost:
	clang++ $(FLAGS) $(INCLUDES) main.cpp -o $(EXE)
	rm -f $(LOG)
	@trap 'kill $$SERVER_PID 2>/dev/null || true' EXIT; \
	python3 ingest_server.py $(LOG) 8787 >/dev/null 2>&1 & \
	SERVER_PID=$$!; \
	until curl -fsS --max-time 1 http://127.0.0.1:8787/health >/dev/null 2>&1; do sleep 0.05; done; \
	AH_TELEMETRY_USE_LOCALHOST=1 ./$(EXE); \
	sleep 0.3; \
	echo "\nCaptured telemetry requests:"; \
	cat $(LOG); \
	echo "\nTelemetry ingest lines (without /health):"; \
	grep "/v1/ingest" $(LOG) || echo "(none yet: expected until HttpGet transport is implemented)"
