.PHONY: all client server clean

TEST_FILES := $(wildcard tests/*.sh)

all: client server test

client:
	$(MAKE) -C client

server:
	$(MAKE) -C server

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