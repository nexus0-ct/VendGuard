CC ?= cc
GCOV ?= gcov
SYSTEM := $(shell uname -s)-$(shell uname -m)
OUT := build/$(SYSTEM)
CFLAGS := -std=c11 -O0 -g -Wall -Wextra -Werror -I../firmware/Logic -I../firmware/Sensors
SOURCES := test_vendguard.c ../firmware/Logic/controller.c ../firmware/Logic/event_log.c ../firmware/Logic/impact.c ../firmware/Sensors/mpu6050.c
LAB3 := test_lab3.c ../firmware/Logic/audit.c ../firmware/Logic/cadence.c ../firmware/Logic/controller.c ../firmware/Logic/event_log.c ../firmware/Logic/impact.c
RUNTIME := test_runtime.c ../firmware/Board/runtime.c ../firmware/Logic/audit.c ../firmware/Logic/cadence.c ../firmware/Logic/controller.c ../firmware/Logic/event_log.c ../firmware/Logic/impact.c ../firmware/Sensors/mpu6050.c
.PHONY: test sanitize coverage coverage-lab3
$(OUT)/test: $(SOURCES)
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(SOURCES) -o $@
$(OUT)/lab3: $(LAB3)
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(LAB3) -o $@
$(OUT)/runtime: $(RUNTIME)
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) -I../firmware/Board -Dprintf=vg_test_printf $(RUNTIME) -o $@
test: $(OUT)/test $(OUT)/lab3 $(OUT)/runtime
	./$(OUT)/test
	./$(OUT)/lab3
	./$(OUT)/runtime
sanitize:
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(SOURCES) -o $(OUT)/sanitize
	./$(OUT)/sanitize
	$(CC) $(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(LAB3) -o $(OUT)/sanitize-lab3
	./$(OUT)/sanitize-lab3
	$(CC) $(CFLAGS) -I../firmware/Board -Dprintf=vg_test_printf -fsanitize=address,undefined -fno-omit-frame-pointer $(RUNTIME) -o $(OUT)/sanitize-runtime
	./$(OUT)/sanitize-runtime
coverage:
	mkdir -p coverage/$(SYSTEM)
	rm -f coverage/$(SYSTEM)/*.gcda
	$(CC) $(CFLAGS) --coverage $(SOURCES) -o coverage/$(SYSTEM)/test
	./coverage/$(SYSTEM)/test
	$(GCOV) -b coverage/$(SYSTEM)/*.gcno
	mv *.gcov coverage/$(SYSTEM)/
coverage-lab3:
	mkdir -p coverage/$(SYSTEM)/lab3
	rm -f coverage/$(SYSTEM)/lab3/*.gcda
	$(CC) $(CFLAGS) --coverage $(LAB3) -o coverage/$(SYSTEM)/lab3/test
	./coverage/$(SYSTEM)/lab3/test
	$(GCOV) -b coverage/$(SYSTEM)/lab3/*audit.gcno coverage/$(SYSTEM)/lab3/*cadence.gcno
	mv audit.c.gcov cadence.c.gcov coverage/$(SYSTEM)/lab3/
