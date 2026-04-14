.DEFAULT_GOAL := all
FILE_SYS_TOOLS_BIN_DIR:=tools/unix-v6pp-filesystem-editor/bin
TOOLS:=$(sort $(wildcard $(FILE_SYS_TOOLS_BIN_DIR)/*)))

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
	@echo "    build and launch unix-v6pp using QEMU (with GDB)"
	@echo "- make qemug-serial"
	@echo "    build and launch unix-v6pp using QEMU (with Serial GDB)"
	@echo "- make qemug-serial-log"
	@echo "    build and launch unix-v6pp using QEMU (with Serial GDB + output log)"
	@echo "- make build-boot-debug"
	@echo "    build bootloader with GDB debug support"
	@echo "- make qemug-boot"
	@echo "    launch unix-v6pp with Bootloader GDB debug"


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
	objdump -d target/objs/Shell.exe > target/asm-dump/Shell.exe.text.asm  # optional
	objdump -D target/objs/Shell.exe > target/asm-dump/Shell.exe.full.asm  # optional


.PHONY: build-kernel
build-kernel: prepare
	mkdir -p build/kernel && cd build/kernel \
	&& cmake -G"Ninja" ../../src -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && \
	cmake --build . -- -j 1


.PHONY: build-boot
build-boot: prepare
	mkdir -p target/objs/boot
	nasm -f bin src/boot/boot.asm -o target/objs/boot/boot.bin


.PHONY: build-boot-debug
build-boot-debug: prepare
	mkdir -p target/objs/boot
	nasm -f bin src/boot/boot.asm -o target/objs/boot/boot_debug.bin -DGDB_DEBUG


.PHONY: build-full
build-full: prepare build-lib build-programs build-shell build-kernel


.PHONY: build-full-debug
build-full-debug: prepare build-lib build-programs build-shell build-kernel


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
deploy-full-debug: build-full-debug
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


.PHONY: bochs
bochs:
	@echo 'not supported. use \"make qemu\" instead.'



QEMU := qemu-system-i386
QEMU += -m 64M
QEMU += -rtc base=localtime
QEMU += -d cpu_reset -D target/qemu.log
QEMU += -machine pc
QEMU += -cpu Icelake-Server
QEMU_GDB := -chardev socket,path=target/qemu-gdb.sock,server=on,wait=off,id=gdb0
QEMU_GDB += -gdb chardev:gdb0 -S

QEMU_DISK := -boot c -drive file=target/c.img,if=ide,index=0,media=disk,format=raw

# 串口 GDB 调试配置（使用字符设备）
QEMU_SERIAL_GDB := -chardev socket,id=serial0,host=0.0.0.0,port=1234,server=on,wait=off -device isa-serial,chardev=serial0

# 串口 GDB 调试配置（输出到文件）
QEMU_SERIAL_GDB_LOG := -chardev socket,id=serial0,host=0.0.0.0,port=1234,server=on,wait=off -device isa-serial,chardev=serial0 -chardev file,id=serial1,path=target/serial.log -device isa-serial,chardev=serial1

# QEMU 内置 GDB stub 配置（用于调试内核）
QEMU_BUILTIN_GDB := -gdb tcp::2005 -S

# Bootloader GDB 调试配置
QEMU_BOOT_GDB := -serial tcp::1234,server,nowait -gdb tcp::1235 -S


.PHONY: qemu-no-rebuild
qemu-no-rebuild:
	$(QEMU) $(QEMU_DISK)


.PHONY: qemug-no-rebuild
qemug-no-rebuild:
	$(QEMU) $(QEMU_DISK) $(QEMU_GDB)


.PHONY: qemug-boot-no-rebuild
qemug-boot-no-rebuild:
	$(QEMU) $(QEMU_DISK) $(QEMU_BOOT_GDB)


.PHONY: qemu
qemu: deploy-full qemu-no-rebuild


.PHONY: qemug
qemug: deploy-full qemug-no-rebuild


.PHONY: qemug-boot
qemug-boot: deploy-full-debug qemug-boot-no-rebuild


.PHONY: clean
clean:
	rm -rf ./target
	rm -rf ./build


.PHONY: all
all: deploy-full
	@echo -e "\033[32mbuild success (unix-v6pp-tongji).\033[0m"


.PHONY: full
full: deploy-full

.PHONY: qemug-serial-no-rebuild
qemug-serial-no-rebuild:
	$(QEMU) $(QEMU_DISK) $(QEMU_SERIAL_GDB)


.PHONY: qemug-serial
qemug-serial: deploy-full qemug-serial-no-rebuild

.PHONY: qemug-builtin-no-rebuild
qemug-builtin-no-rebuild:
	$(QEMU) $(QEMU_DISK) $(QEMU_BUILTIN_GDB)

.PHONY: qemug-builtin
qemug-builtin: deploy-full qemug-builtin-no-rebuild

# 串口 GDB 调试带日志输出
.PHONY: qemug-serial-log-no-rebuild
qemug-serial-log-no-rebuild:
	mkdir -p target
	$(QEMU) $(QEMU_DISK) $(QEMU_SERIAL_GDB_LOG)

.PHONY: qemug-serial-log
qemug-serial-log: deploy-full qemug-serial-log-no-rebuild


# Bootloader GDB 调试相关目标
.PHONY: gdb-boot
gdb-boot:
	@echo "=========================================="
	@echo "Starting GDB for Bootloader debugging..."
	@echo "Connect with: target remote localhost:1235"
	@echo "Set breakpoint: break *0x7c00"
	@echo "=========================================="
	gdb -ex "target remote localhost:1235" -ex "set architecture i8086"


.PHONY: debug-boot
debug-boot:
	@echo "Starting QEMU from disk in background..."
	@make qemug-boot > /tmp/qemu.log 2>&1 & \
	QEMU_PID=$$!; \
	echo "QEMU PID: $$QEMU_PID"; \
	echo "Waiting for QEMU to start..."; \
	sleep 2; \
	echo "Starting GDB..."; \
	gdb -ex "target remote localhost:1235" -ex "set architecture i8086"; \
	kill $$QEMU_PID 2>/dev/null || true