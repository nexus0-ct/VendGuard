GCOV ?= gcov
IMAGE ?= vendguard-build:lab3

.PHONY: image build clean

image:
	docker build -t $(IMAGE) docker

build: image
	docker run --rm -v "$(CURDIR):/workspace" -w /workspace \
		--user "$(shell id -u):$(shell id -g)" $(IMAGE) \
		make -C firmware all

clean:
	$(MAKE) -C firmware clean

.PHONY: test sanitize coverage coverage-lab3 check
check:
	python3 tools/check_layers.py
	python3 tools/check_wiring.py
test: check
	$(MAKE) -C tests test
sanitize: check
	$(MAKE) -C tests sanitize
coverage: check
	$(MAKE) -C tests coverage GCOV="$(GCOV)"
coverage-lab3: check
	$(MAKE) -C tests coverage-lab3 GCOV="$(GCOV)"
