CC ?= cc
GCOV ?= gcov
SYSTEM := $(shell uname -s)-$(shell uname -m)
OUT := build/$(SYSTEM)
CFLAGS := -std=c11 -O0 -g -Wall -Wextra -Werror -I../firmware/Logic -I../firmware/Sensors
SOURCES := test_vendguard.c ../firmware/Logic/controller.c ../firmware/Logic/event_log.c ../firmware/Logic/impact.c ../firmware/Sensors/mpu6050.c
.PHONY: test sanitize coverage
$(OUT)/test: $(SOURCES)
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(SOURCES) -o $@
test: $(OUT)/test
	./$(OUT)/test
sanitize:
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(SOURCES) -o $(OUT)/sanitize
	./$(OUT)/sanitize
coverage:
	mkdir -p coverage/$(SYSTEM)
	rm -f coverage/$(SYSTEM)/*.gcda
	$(CC) $(CFLAGS) --coverage $(SOURCES) -o coverage/$(SYSTEM)/test
	./coverage/$(SYSTEM)/test
	$(GCOV) -b coverage/$(SYSTEM)/*.gcno
	mv *.gcov coverage/$(SYSTEM)/
