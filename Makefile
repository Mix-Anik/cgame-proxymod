.PHONY: build-unix clean help

help:
	@echo "Available targets:"
	@echo "  make build-unix - Build cgame module for Unix/Linux/MinGW64"
	@echo "  make clean      - Remove build directory"

build-unix:
	@rm -rf build
	@mkdir -p build
	@echo "Configuring build..."
	@cd build && cmake -G "Unix Makefiles" -DENABLE_TESTING=OFF ..
	@echo "Building cgame module..."
	@cmake --build build
	@echo "✓ Built: build/cgame.so (or .dll on Windows)"

clean:
	@echo "Cleaning build directory..."
	@rm -rf build
	@echo "✓ Clean complete"
