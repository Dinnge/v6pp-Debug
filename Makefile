# # Unix V6++ Tongji ¶¥²ã¹¹½¨½Å±¾
# #
# # ´´½¨ÓÚ 2024Äê4ÔÂ28ÈÕ ÉÏº£ÊÐ¼Î¶¨Çø
# # by 2051565 GTY

# .DEFAULT_GOAL := all
# FILE_SYS_TOOLS_BIN_DIR:=tools/unix-v6pp-filesystem-editor/bin
# TOOLS:=$(sort $(wildcard $(FILE_SYS_TOOLS_BIN_DIR)/*))

# ifeq ($(word 1, $(TOOLS)),)
# $(error "filescanner not found. please run 'bash init.sh' first")
# endif
# ifeq ($(word 2, $(TOOLS)),)
# $(error "fsedit not found. please run 'bash init.sh' first")
# endif


# .PHONY: help
# help:
# 	@echo "unix-v6pp makefile"
# 	@echo "---------------"
# 	@echo "commands available:"
# 	@echo "- make bochs"
# 	@echo "    build and launch unix-v6pp using Bochs"
# 	@echo "- make qemu"
# 	@echo "    build and launch unix-v6pp using QEMU"
# 	@echo "- make qemug"
# 	@echo "    build and launch unix-v6pp using QEMU (with GDB)"
# 	@echo "- make"
# 	@echo "    alias for \"make all\""
# 	@echo "- make qemug-serial"
# 	@echo "    build and launch unix-v6pp using QEMU (with Serial GDB)"
# 	@echo "- make build-boot-debug"
# 	@echo "    build bootloader with GDB debug support"
# 	@echo "- make qemug-boot"
# 	@echo "    launch unix-v6pp with Bootloader GDB debug"
# 	@echo "- make gdb-boot"
# 	@echo "    start GDB for Bootloader debugging"


# .PHONY: prepare
# prepare:
# 	mkdir -p target/objs/asm-dump


# .PHONY: build-programs
# build-programs: prepare
# 	mkdir -p target/objs/apps
# 	mkdir -p build/apps && cd build/apps \
# 	&& cmake -G"Ninja" ../../programs -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && \
# 	cmake --build . -- -j 4


# .PHONY: build-lib
# build-lib: prepare
# 	mkdir -p build/lib && cd build/lib \
# 	&& cmake -G"Ninja" ../../lib/src -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
# 	&& cmake --build . -- -j 4
# 	mkdir -p target/objs
# 	cp build/lib/libv6pptongji.a target/objs/libv6pptongji.a


# .PHONY: build-shell
# build-shell: prepare
# 	mkdir -p build/shell && cd build/shell \
# 	&& cmake -G"Ninja" ../../shell -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && \
# 	cmake --build . -- -j 2
# 	mkdir -p target/objs
# 	objdump -d target/objs/Shell.exe > target/asm-dump/Shell.exe.text.asm  # optional
# 	objdump -D target/objs/Shell.exe > target/asm-dump/Shell.exe.full.asm  # optional


# .PHONY: build-kernel
# build-kernel: prepare
# 	mkdir -p build/kernel && cd build/kernel \
# 	&& cmake -G"Ninja" ../../src -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && \
# 	cmake --build . -- -j 1

# .PHONY: build-boot
# build-boot: prepare
# 	mkdir -p target/objs/boot
# 	nasm -f bin src/boot/boot.asm -o target/objs/boot/boot.bin

# .PHONY: build-boot-debug
# build-boot-debug: prepare
# 	mkdir -p target/objs/boot
# 	nasm -f bin src/boot/boot.asm -o target/objs/boot/boot_debug.bin -DGDB_DEBUG
# 	nasm -f elf32 src/boot/boot.asm -o target/objs/boot/boot_debug.o -DGDB_DEBUG
# 	ld -m elf_i386 -Ttext 0x7c00 -e start -nostdlib -o target/objs/boot/boot_debug.elf target/objs/boot/boot_debug.o

# .PHONY: build-full
# build-full: prepare build-lib build-programs build-shell build-kernel

# .PHONY: build-full-debug
# build-full-debug: prepare build-lib build-programs build-shell build-kernel

# .PHONY: deploy-full
# deploy-full: build-full
# 	mkdir -p target/img-workspace
# 	mkdir -p target/img-workspace/programs/bin
# 	mkdir -p target/img-workspace/programs/etc
# 	cp target/objs/kernel.bin target/img-workspace/
# 	cp target/objs/boot/boot.bin target/img-workspace/
# 	cp target/objs/apps/* target/img-workspace/programs/bin/
# 	cp target/objs/Shell.exe target/img-workspace/programs/
# 	cp tools/unix-v6pp-filesystem-editor/bin/* target/img-workspace/
# 	cp tools/unixv6pp_splash/v6pp_splash.bmp target/img-workspace/programs/etc/
# 	cd target/img-workspace && ./filescanner | ./fsedit c.img c
# 	cp target/img-workspace/c.img target/

# .PHONY: deploy-full-debug
# deploy-full-debug: build-full-debug
# 	mkdir -p target/img-workspace
# 	mkdir -p target/img-workspace/programs/bin
# 	mkdir -p target/img-workspace/programs/etc
# 	cp target/objs/kernel.bin target/img-workspace/
# 	cp target/objs/boot/boot_debug.bin target/img-workspace/boot.bin
# 	cp target/objs/apps/* target/img-workspace/programs/bin/
# 	cp target/objs/Shell.exe target/img-workspace/programs/
# 	cp tools/unix-v6pp-filesystem-editor/bin/* target/img-workspace/
# 	cp tools/unixv6pp_splash/v6pp_splash.bmp target/img-workspace/programs/etc/
# 	cd target/img-workspace && ./filescanner | ./fsedit c.img c
# 	cp target/img-workspace/c.img target/

# .PHONY: bochs
# bochs:
# 	@echo 'not supported. use \"make qemu\" instead.'

# QEMU := qemu-system-i386 
# QEMU += -m 64M 
# QEMU += -rtc base=localtime 
# QEMU += -d cpu_reset -D target/qemu.log 
# QEMU += -machine pc 
# QEMU += -cpu Icelake-Server 
# QEMU_GDB := -chardev socket,path=target/qemu-gdb.sock,server=on,wait=off,id=gdb0 
# QEMU_GDB += -gdb chardev:gdb0 -S 

# QEMU_DISK := -boot c -drive file=target/c.img,if=ide,index=0,media=disk,format=raw

# # ´®¿Ú GDB µ÷ÊÔÅäÖÃ
# QEMU_SERIAL_GDB := -serial tcp::1234,server,nowait

# QEMU_BUILTIN_GDB := -gdb tcp::2005 -S

# QEMU_BOOT_GDB := -serial tcp::1234,server,nowait

# .PHONY: qemu-no-rebuild
# qemu-no-rebuild:
# 	$(QEMU) $(QEMU_DISK)

# .PHONY: qemug-no-rebuild
# qemug-no-rebuild:
# 	$(QEMU) $(QEMU_DISK) $(QEMU_GDB)

# .PHONY: qemug-boot-no-rebuild
# qemug-boot-no-rebuild:
# 	$(QEMU) $(QEMU_DISK) $(QEMU_BOOT_STUB)

# .PHONY: qemu
# qemu: deploy-full qemu-no-rebuild

# .PHONY: qemug
# qemug: deploy-full qemug-no-rebuild

# .PHONY: qemug-boot
# qemug-boot: deploy-full-debug qemug-boot-no-rebuild

# .PHONY: clean
# clean:
# 	rm -rf ./target
# 	rm -rf ./build

# .PHONY: all
# all: deploy-full
# 	@echo -e "\033[32mbuild success (unix-v6pp-tongji).\033[0m"

# .PHONY: full
# full: deploy-full

# .PHONY: qemug-serial-no-rebuild
# qemug-serial-no-rebuild:
# 	$(QEMU) $(QEMU_DISK) $(QEMU_SERIAL_GDB)

# .PHONY: qemug-serial
# qemug-serial: deploy-full qemug-serial-no-rebuild

# .PHONY: qemug-builtin-no-rebuild
# qemug-builtin-no-rebuild:
# 	$(QEMU) $(QEMU_DISK) $(QEMU_BUILTIN_GDB)

# .PHONY: qemug-builtin
# qemug-builtin: deploy-full qemug-builtin-no-rebuild

# .PHONY: gdb-boot
# gdb-boot:
# 	@echo "=========================================="
# 	@echo "Starting GDB for Bootloader debugging..."
# 	@echo "Connect with: target remote localhost:1234"
# 	@echo "Set breakpoint: break *0x7c00"
# 	@echo "=========================================="
# 	gdb -ex "target remote localhost:1234" -ex "set architecture i8086"

# .PHONY: debug-boot
# debug-boot:
# 	@echo "Starting QEMU from disk in background..."
# 	@make qemug-boot > /tmp/qemu.log 2>&1 & \
# 	QEMU_PID=$$!; \
# 	echo "QEMU PID: $$QEMU_PID"; \
# 	echo "Waiting for QEMU to start..."; \
# 	sleep 2; \
# 	echo "Starting GDB..."; \
# 	gdb -ex "target remote localhost:1234" -ex "set architecture i8086"; \
# 	kill $$QEMU_PID 2>/dev/null || true
# Unix V6++ Tongji ¶¥²ã¹¹½¨½Å±¾
#
# ´´½¨ÓÚ 2024Äê4ÔÂ28ÈÕ ÉÏº£ÊÐ¼Î¶¨Çø
# by 2051565 GTY

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
	@echo "- make gdb-serial"
	@echo "    attach GDB to the kernel-stage serial debugger on localhost:1234"
	@echo "- make build-boot-debug"
	@echo "    build bootloader with GDB debug support"
	@echo "- make qemug-boot"
	@echo "    launch unix-v6pp with Bootloader GDB debug"
	@echo "- make gdb-boot-lifecycle"
	@echo "    launch GDB with boot lifecycle debug script"
	@echo "- make debug-boot-lifecycle"
	@echo "    start QEMU boot debug and attach scripted GDB session"


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
	&& cmake -G"Ninja" ../../src -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DEARLY_BOOT_GDB=OFF && \
	cmake --build . -- -j 1


.PHONY: build-kernel-debug
build-kernel-debug: prepare
	mkdir -p build/kernel && cd build/kernel \
	&& cmake -G"Ninja" ../../src -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DEARLY_BOOT_GDB=ON && \
	cmake --build . -- -j 1


.PHONY: build-kernel-serial-debug
build-kernel-serial-debug: prepare
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
	nasm -f bin src/boot/boot.asm -o target/objs/boot/boot_debug.bin -DGDB_DEBUG
	nasm -f elf32 src/boot/boot.asm -o target/objs/boot/boot_debug.o -DGDB_DEBUG
	ld -m elf_i386 -Ttext 0x7c00 -e start -nostdlib -o target/objs/boot/boot_debug.elf target/objs/boot/boot_debug.o


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

# ´®¿Ú GDB µ÷ÊÔÅäÖÃ£¨Ê¹ÓÃ×Ö·ûÉè±¸£©
QEMU_SERIAL_GDB := -chardev socket,id=serial0,host=0.0.0.0,port=1234,server=on,wait=off -device isa-serial,chardev=serial0

# ´®¿Ú GDB µ÷ÊÔÅäÖÃ£¨Êä³öµ½ÎÄ¼þ£©
QEMU_SERIAL_GDB_LOG := -chardev socket,id=serial0,host=0.0.0.0,port=1234,server=on,wait=off -device isa-serial,chardev=serial0 -chardev file,id=serial1,path=target/serial.log -device isa-serial,chardev=serial1

# QEMU ÄÚÖÃ GDB stub ÅäÖÃ£¨ÓÃÓÚµ÷ÊÔÄÚºË£©
QEMU_BUILTIN_GDB := -gdb tcp::2005 -S

# Bootloader GDB µ÷ÊÔÅäÖÃ
QEMU_BOOT_STUB := -serial tcp::1234,server,nowait


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
qemug-boot:
	@$(MAKE) clean
	@$(MAKE) deploy-full-debug
	@$(MAKE) qemug-boot-no-rebuild


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
qemug-serial: deploy-full-serial-debug qemug-serial-no-rebuild

.PHONY: qemug-builtin-no-rebuild
qemug-builtin-no-rebuild:
	$(QEMU) $(QEMU_DISK) $(QEMU_BUILTIN_GDB)

.PHONY: qemug-builtin
qemug-builtin: deploy-full qemug-builtin-no-rebuild

# ´®¿Ú GDB µ÷ÊÔ´øÈÕÖ¾Êä³ö
.PHONY: qemug-serial-log-no-rebuild
qemug-serial-log-no-rebuild:
	mkdir -p target
	$(QEMU) $(QEMU_DISK) $(QEMU_SERIAL_GDB_LOG)

.PHONY: qemug-serial-log
qemug-serial-log: deploy-full-serial-debug qemug-serial-log-no-rebuild


.PHONY: gdb-serial
gdb-serial: build-kernel-serial-debug
	@echo "=========================================="
	@echo "Starting GDB for kernel-stage serial debugging..."
	@echo "Symbols: build/kernel/kernel.exe"
	@echo "Connect with: target remote localhost:1234"
	@echo "=========================================="
	gdb build/kernel/kernel.exe -ex "target remote localhost:1234"


# Bootloader GDB µ÷ÊÔÏà¹ØÄ¿±ê
.PHONY: gdb-boot
gdb-boot:
	@echo "=========================================="
	@echo "Starting GDB for Bootloader debugging..."
	@echo "Connect with: target remote localhost:1234"
	@echo "Set breakpoint: break *0x7c00"
	@echo "=========================================="
	gdb -ex "target remote localhost:1234" -ex "set architecture i8086"

.PHONY: gdb-boot-lifecycle
gdb-boot-lifecycle: build-boot-debug
	@echo "=========================================="
	@echo "Starting GDB with boot lifecycle script..."
	@echo "Script: tools/gdb/bootloader_lifecycle.gdb"
	@echo "=========================================="
	gdb -x tools/gdb/bootloader_lifecycle.gdb


.PHONY: debug-boot
debug-boot:
	@echo "Starting QEMU from disk in background..."
	@make qemug-boot > /tmp/qemu.log 2>&1 & \
	QEMU_PID=$$!; \
	echo "QEMU PID: $$QEMU_PID"; \
	echo "Waiting for QEMU to start..."; \
	sleep 2; \
	echo "Starting GDB..."; \
	gdb -ex "target remote localhost:1234" -ex "set architecture i8086"; \
	kill $$QEMU_PID 2>/dev/null || true

.PHONY: debug-boot-lifecycle
debug-boot-lifecycle:
	@echo "Starting QEMU from disk in background..."
	@make qemug-boot > /tmp/qemu.log 2>&1 & \
	QEMU_PID=$$!; \
	echo "QEMU PID: $$QEMU_PID"; \
	echo "Waiting for QEMU to start..."; \
	sleep 2; \
	echo "Starting scripted GDB session..."; \
	make gdb-boot-lifecycle; \
	kill $$QEMU_PID 2>/dev/null || true
