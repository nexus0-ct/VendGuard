GCOV ?= gcov
IMAGE ?= stm32c0-wokwi-build:local

.PHONY: image build clean

image:
	docker build -t $(IMAGE) docker

build: image
	docker run --rm -v "$(CURDIR):/workspace" -w /workspace \
		--user "$(shell id -u):$(shell id -g)" $(IMAGE) \
		make -C firmware all

clean:
	$(MAKE) -C firmware clean

.PHONY: test sanitize coverage check
check:
	python3 tools/check_layers.py
	python3 tools/check_wiring.py
test: check
	$(MAKE) -C tests test
sanitize: check
	$(MAKE) -C tests sanitize
coverage: check
	$(MAKE) -C tests coverage GCOV="$(GCOV)"
