.PHONY: all client server test clean

TEST_FILES := $(wildcard tests/*.sh)

# Проброс DEBUG в подпроекты: make DEBUG=1
DEBUG ?= 0

all: client server test

client:
	$(MAKE) -C client DEBUG=$(DEBUG)

server:
	$(MAKE) -C server DEBUG=$(DEBUG)

test: $(TEST_FILES)
	@fail=0; \
	for t in $(TEST_FILES); do \
		echo "=== RUN $$t ==="; \
		if bash "$$t"; then \
			echo "--- PASS $$t"; \
		else \
			echo "--- FAIL $$t"; \
			fail=1; \
		fi; \
	done; \
	exit $$fail

clean:
	$(MAKE) -C client clean
	$(MAKE) -C server clean