.DEFAULT_GOAL := all
FILE_SYS_TOOLS_BIN_DIR := tools/unix-v6pp-filesystem-editor/bin
TOOLS := $(sort $(wildcard $(FILE_SYS_TOOLS_BIN_DIR)/*))

ifeq ($(word 1, $(TOOLS)),)
$(error "filescanner not found. please run 'bash init.sh' first")
endif
ifeq ($(word 2, $(TOOLS)),)
$(error "fsedit not found. please run 'bash init.sh' first")
endif

.PHONY: help
help:
	@echo "unix-v6pp makefile"
	@echo "---------------"
	@echo "commands available:"
	@echo "- make bochs"
	@echo "    build and launch unix-v6pp using Bochs"
	@echo "- make qemu"
	@echo "    build and launch unix-v6pp using QEMU"
	@echo "- make qemug"
	@echo "    build and launch unix-v6pp using QEMU (built-in GDB stub)"
	@echo "- make qemug-serial"
	@echo "    build and launch unix-v6pp using QEMU (kernel-stage serial GDB)"
	@echo "- make qemug-boot"
	@echo "    build and launch unix-v6pp using QEMU (boot-to-kernel lifecycle stub)"
	@echo "- make gdb-boot-lifecycle"
	@echo "    open GDB with the lifecycle script and connect to :1234"
	@echo "- make debug-lifecycle"
	@echo "    one command: build, launch QEMU, attach GDB, start at bootloader"

.PHONY: prepare
prepare:
	mkdir -p target/objs/asm-dump

.PHONY: build-programs
build-programs: prepare
	mkdir -p target/objs/apps
	mkdir -p build/apps && cd build/apps \
	&& cmake -G"Ninja" ../../programs -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && \
	cmake --build . -- -j 4

.PHONY: build-lib
build-lib: prepare
	mkdir -p build/lib && cd build/lib \
	&& cmake -G"Ninja" ../../lib/src -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
	&& cmake --build . -- -j 4
	mkdir -p target/objs
	cp build/lib/libv6pptongji.a target/objs/libv6pptongji.a

.PHONY: build-shell
build-shell: prepare
	mkdir -p build/shell && cd build/shell \
	&& cmake -G"Ninja" ../../shell -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && \
	cmake --build . -- -j 2
	mkdir -p target/objs
	objdump -d target/objs/Shell.exe > target/asm-dump/Shell.exe.text.asm
	objdump -D target/objs/Shell.exe > target/asm-dump/Shell.exe.full.asm

.PHONY: build-kernel
build-kernel: prepare
	mkdir -p build/kernel && cd build/kernel \
	&& cmake -G"Ninja" ../../src -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && \
	cmake --build . -- -j 1

.PHONY: build-kernel-debug
build-kernel-debug: prepare
	rm -rf build/kernel
	mkdir -p build/kernel && cd build/kernel \
	&& cmake -G"Ninja" ../../src -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DEARLY_BOOT_GDB=ON && \
	cmake --build . -- -j 1

.PHONY: build-kernel-serial-debug
build-kernel-serial-debug: prepare
	rm -rf build/kernel
	mkdir -p build/kernel && cd build/kernel \
	&& cmake -G"Ninja" ../../src -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DEARLY_BOOT_GDB=OFF -DKERNEL_GDB_AUTOSTART=ON && \
	cmake --build . -- -j 1

.PHONY: build-boot
build-boot: prepare
	mkdir -p target/objs/boot
	nasm -f bin src/boot/boot.asm -o target/objs/boot/boot.bin

.PHONY: build-boot-debug
build-boot-debug: prepare
	mkdir -p target/objs/boot
	nasm -f bin src/boot/boot.asm -o target/objs/boot/boot_debug.bin -DGDB_DEBUG -DUSE_VESA
	nasm -f elf32 src/boot/boot.asm -o target/objs/boot/boot_debug.o -DGDB_DEBUG -DUSE_VESA
	ld -m elf_i386 -Ttext 0x7c00 -e start -nostdlib -o target/objs/boot/boot_debug.elf target/objs/boot/boot_debug.o
	nasm -f elf32 src/boot/sector2.asm -o target/objs/boot/sector2_debug.o -DEARLY_BOOT_GDB

.PHONY: build-full
build-full: prepare build-lib build-programs build-shell build-kernel

.PHONY: build-full-debug
build-full-debug: prepare build-lib build-programs build-shell build-kernel-debug

.PHONY: build-full-serial-debug
build-full-serial-debug: prepare build-lib build-programs build-shell build-kernel-serial-debug

.PHONY: deploy-full
deploy-full: build-full
	mkdir -p target/img-workspace
	mkdir -p target/img-workspace/programs/bin
	mkdir -p target/img-workspace/programs/etc
	cp target/objs/kernel.bin target/img-workspace/
	cp target/objs/boot/boot.bin target/img-workspace/
	cp target/objs/apps/* target/img-workspace/programs/bin/
	cp target/objs/Shell.exe target/img-workspace/programs/
	cp tools/unix-v6pp-filesystem-editor/bin/* target/img-workspace/
	cp tools/unixv6pp_splash/v6pp_splash.bmp target/img-workspace/programs/etc/
	cd target/img-workspace && ./filescanner | ./fsedit c.img c
	cp target/img-workspace/c.img target/

.PHONY: deploy-full-debug
deploy-full-debug: build-full-debug build-boot-debug
	mkdir -p target/img-workspace
	mkdir -p target/img-workspace/programs/bin
	mkdir -p target/img-workspace/programs/etc
	cp target/objs/kernel.bin target/img-workspace/
	cp target/objs/boot/boot_debug.bin target/img-workspace/boot.bin
	cp target/objs/apps/* target/img-workspace/programs/bin/
	cp target/objs/Shell.exe target/img-workspace/programs/
	cp tools/unix-v6pp-filesystem-editor/bin/* target/img-workspace/
	cp tools/unixv6pp_splash/v6pp_splash.bmp target/img-workspace/programs/etc/
	cd target/img-workspace && ./filescanner | ./fsedit c.img c
	cp target/img-workspace/c.img target/

.PHONY: deploy-full-serial-debug
deploy-full-serial-debug: build-full-serial-debug build-boot
	mkdir -p target/img-workspace
	mkdir -p target/img-workspace/programs/bin
	mkdir -p target/img-workspace/programs/etc
	cp target/objs/kernel.bin target/img-workspace/
	cp target/objs/boot/boot.bin target/img-workspace/
	cp target/objs/apps/* target/img-workspace/programs/bin/
	cp target/objs/Shell.exe target/img-workspace/programs/
	cp tools/unix-v6pp-filesystem-editor/bin/* target/img-workspace/
	cp tools/unixv6pp_splash/v6pp_splash.bmp target/img-workspace/programs/etc/
	cd target/img-workspace && ./filescanner | ./fsedit c.img c
	cp target/img-workspace/c.img target/

.PHONY: bochs
bochs:
	@echo 'not supported. use "make qemu" instead.'

QEMU := qemu-system-i386
QEMU += -m 64M
QEMU += -rtc base=localtime
QEMU += -d cpu_reset -D target/qemu.log
QEMU += -machine pc
QEMU += -cpu Icelake-Server

QEMU_GDB := -chardev socket,path=target/qemu-gdb.sock,server=on,wait=off,id=gdb0
QEMU_GDB += -gdb chardev:gdb0 -S

QEMU_DISK := -boot c -drive file=target/c.img,if=ide,index=0,media=disk,format=raw
QEMU_SERIAL_GDB := -chardev socket,id=serial0,host=0.0.0.0,port=1234,server=on,wait=off -device isa-serial,chardev=serial0
QEMU_SERIAL_GDB_LOG := -chardev socket,id=serial0,host=0.0.0.0,port=1234,server=on,wait=off -device isa-serial,chardev=serial0 -chardev file,id=serial1,path=target/serial.log -device isa-serial,chardev=serial1
QEMU_BUILTIN_GDB := -gdb tcp::2005 -S
QEMU_BOOT_STUB := -serial tcp::1234,server,nowait
LIFECYCLE_PORT := 1234

.PHONY: qemu-no-rebuild
qemu-no-rebuild:
	$(QEMU) $(QEMU_DISK)

.PHONY: qemug-no-rebuild
qemug-no-rebuild:
	$(QEMU) $(QEMU_DISK) $(QEMU_GDB)

.PHONY: qemug-boot-no-rebuild
qemug-boot-no-rebuild:
	$(QEMU) $(QEMU_DISK) $(QEMU_BOOT_STUB)

.PHONY: qemu
qemu: deploy-full qemu-no-rebuild

.PHONY: qemug
qemug: deploy-full qemug-no-rebuild

.PHONY: qemug-boot
qemug-boot: deploy-full-debug qemug-boot-no-rebuild

.PHONY: qemug-serial-no-rebuild
qemug-serial-no-rebuild:
	$(QEMU) $(QEMU_DISK) $(QEMU_SERIAL_GDB)

.PHONY: qemug-serial
qemug-serial: deploy-full-serial-debug qemug-serial-no-rebuild

.PHONY: qemug-builtin-no-rebuild
qemug-builtin-no-rebuild:
	$(QEMU) $(QEMU_DISK) $(QEMU_BUILTIN_GDB)

.PHONY: qemug-builtin
qemug-builtin: deploy-full qemug-builtin-no-rebuild

.PHONY: qemug-serial-log-no-rebuild
qemug-serial-log-no-rebuild:
	mkdir -p target
	$(QEMU) $(QEMU_DISK) $(QEMU_SERIAL_GDB_LOG)

.PHONY: qemug-serial-log
qemug-serial-log: deploy-full-serial-debug qemug-serial-log-no-rebuild

.PHONY: gdb-boot
gdb-boot: build-boot-debug
	@echo "=========================================="
	@echo "Starting GDB for bootloader lifecycle debugging..."
	@echo "Connect with: target remote localhost:1234"
	@echo "=========================================="
	gdb -x tools/gdb/bootloader_lifecycle.gdb

.PHONY: gdb-boot-lifecycle
gdb-boot-lifecycle: build-full-debug build-boot-debug
	@echo "=========================================="
	@echo "Starting GDB with boot lifecycle script..."
	@echo "=========================================="
	gdb -x tools/gdb/bootloader_lifecycle.gdb

.PHONY: debug-boot
debug-boot: debug-lifecycle

.PHONY: stop-debug-lifecycle
stop-debug-lifecycle:
	@mkdir -p target
	@PID_FILE=target/debug-lifecycle.qemu.pid; \
	if [ -f $$PID_FILE ]; then \
		PID=$$(cat $$PID_FILE); \
		kill $$PID 2>/dev/null || true; \
		rm -f $$PID_FILE; \
	fi; \
	fuser -k $(LIFECYCLE_PORT)/tcp >/dev/null 2>&1 || true

.PHONY: debug-lifecycle
debug-lifecycle: deploy-full-debug stop-debug-lifecycle
	@mkdir -p target
	@$(MAKE) qemug-boot-no-rebuild > target/debug-lifecycle.qemu.log 2>&1 & \
	QEMU_PID=$$!; \
	echo $$QEMU_PID > target/debug-lifecycle.qemu.pid; \
	trap 'kill $$QEMU_PID 2>/dev/null || true' EXIT INT TERM; \
	sleep 2; \
	gdb -x tools/gdb/bootloader_lifecycle.gdb; \
	rm -f target/debug-lifecycle.qemu.pid

.PHONY: clean
clean:
	rm -rf ./target
	rm -rf ./build

.PHONY: all
all: deploy-full
	@echo -e "\033[32mbuild success (unix-v6pp-tongji).\033[0m"

.PHONY: full
full: deploy-full
