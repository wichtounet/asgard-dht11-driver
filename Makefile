user=pi
pi=192.168.20.161
password=raspberry
dir=/home/${user}/asgard/asgard-server/

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

remote_make:
	sshpass -p ${password} scp Makefile ${user}@${pi}:${dir}/
	sshpass -p ${password} scp src/*.cpp ${user}@${pi}:${dir}/
	sshpass -p ${password} ssh ${user}@${pi} "cd ${dir} && make"

remote_run:
	sshpass -p ${password} ssh ${user}@${pi} "cd ${dir} && make run"

clean: base_clean

include make-utils/cpp-utils-finalize.mk
