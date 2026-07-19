.PHONY: build-unix clean help

help:
	@echo "Available targets:"
	@echo "  make build-unix - Build cgame module for Unix/Linux/MinGW64"
	@echo "  make clean      - Remove build directory"

build-unix:
	@rm -rf build
	@mkdir -p build
	@echo "Configuring build..."
	@cd build && cmake -G "Unix Makefiles" -DBINARY_NAME=cgamex86_64 -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTING=OFF ..
	@echo "Building cgame module..."
	@cmake --build build
	@echo "✓ Built: build/cgamex86_64.dll"

clean:
	@echo "Cleaning build directory..."
	@rm -rf build
	@echo "✓ Clean complete"
