default: release

.PHONY: default release debug all clean

include make-utils/flags-pi.mk
include make-utils/cpp-utils.mk

CXX_FLAGS += -pedantic
LD_FLAGS  += -lwiringPi -pthread

$(eval $(call auto_folder_compile,src))
$(eval $(call auto_add_executable,dht11_driver))

release: release_dht11_driver
release_debug: release_debug_dht11_driver
debug: debug_dht11_driver

all: release release_debug debug

run: release
	sudo ./release/bin/dht11_driver

clean: base_clean

include make-utils/cpp-utils-finalize.mk
