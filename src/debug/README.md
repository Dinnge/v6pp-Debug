# V6++ è°è¯å¨æ¨¡å

## æ¦è¿°

è¿æ¯ä¸ä¸ªä¸º V6++ åæ ¸å¼åçè°è¯å¨æ¨¡å,æ¯æ GDB è¿ç¨åè®®ã

## ç®å½ç»æ

- `debug.h` - è°è¯å¨ä¸»å¤´æä»¶
- `gdb/` - GDB åè®®å®ç°
  - `debugger.cpp` - è°è¯å¨éææ¥å£
  - `gdb_protocol.h` - GDB åè®®å¤´æä»¶
  - `gdb_protocol.cpp` - GDB åè®®å®ç°
- `boot/` - å¯å¨è°è¯æ¯æï¼å·²æ¯æ Bootloader çå½å¨ææ­ç¹ï¼
- `fs/` - æä»¶ç³»ç»è°è¯æ¯æ(å¾å®ç°)
- `json/` - JSON è°è¯æ°æ®æ¯æ(å¾å®ç°)

## GDB è¿ç¨åè®®

### æ¯æçå½ä»¤

- `c` - ç»§ç»­æ§è¡
- `s` - åæ­¥æ§è¡
- `g` - è¯»åææå¯å­å¨
- `G` - åå¥ææå¯å­å¨
- `m` - è¯»ååå­
- `M` - åå¥åå­
- `Z` - è®¾ç½®æ­ç¹
- `z` - ç§»é¤æ­ç¹
- `q` - æ¥è¯¢å½ä»¤

## ä½¿ç¨æ¹æ³

### ç¼è¯

```bash
cd build
cmake ..
make
```

### è¿æ¥ GDB

```bash
gdb your-kernel-image
(gdb) target remote :1234
```

### ç¤ºä¾

```bash
# è®¾ç½®æ­ç¹
(gdb) break main

# ç»§ç»­æ§è¡
(gdb) continue

# åæ­¥æ§è¡
(gdb) step

# æ¥çå¯å­å¨
(gdb) info registers

# æ¥çåå­
(gdb) x/10x 0x1000
```

### Bootloader é¶æ®µè°è¯ï¼å®æ¨¡å¼/ä¿æ¤æ¨¡å¼ï¼

```bash
# 1) æå»º boot è°è¯éååç¬¦å·
make build-boot-debug

# 2) å¯å¨ QEMUï¼CPU ä»ç¬¬ä¸æ¡æä»¤åæåï¼
make qemug-boot

# 3) è¿è¡çå½å¨æè°è¯èæ¬ï¼å¦ä¸ä¸ªç»ç«¯ï¼
make gdb-boot-lifecycle
```

å³é®æ­ç¹æ ç­¾ï¼

- `boot_dbg_stage_real_mode_entry`
- `boot_dbg_stage_before_protected_mode`
- `boot_dbg_stage_after_lgdt_a20`
- `boot_dbg_stage_after_cr0_pe`
- `boot_dbg_stage_protected_mode_entry`
- `boot_dbg_stage_kernel_loaded`
- `boot_dbg_stage_jump_kernel`

## å¼åè¿åº¦

### ç¬¬ä¸é¶æ®µ: æ¡æ¶æ­å»º
- [x] ç®å½ç»æåå»º
- [x] åºç¡å¤´æä»¶å®ä¹
- [x] GDB åè®®æ¡æ¶
- [ ] ç½ç»éä¿¡å±
- [ ] å¯å­å¨è®¿é®æ¥å£
- [ ] åå­è®¿é®æ¥å£
- [ ] æ­ç¹ç®¡ç

### ç¬¬äºé¶æ®µ: åè½å®ç°
- [ ] å®æ´çå½ä»¤è§£æ
- [ ] ä¸ä¸æä¿å­/æ¢å¤
- [ ] æ­ç¹å½ä¸­å¤ç
- [ ] å¼å¸¸å¤çéæ

### ç¬¬ä¸é¶æ®µ: ä¼åå®å
- [ ] æ§è½ä¼å
- [ ] éè¯¯å¤ç
- [ ] ææ¡£å®å

## Boot Debug Roadmap

目标：把调试框架扩展到系统启动初期，实现从“第一条指令”开始，到实模式/保护模式切换、再到早期内核入口的全生命周期覆盖。

阶段 1：Boot Sector 最小 Stub

- 完成 `target remote` 握手，至少支持 `vMustReplyEmpty`、`?`、`g`。
- 在引导扇区内提供最小停止点，覆盖保护模式切换前后等关键阶段。
- 保持 boot sector 不超过 `512` 字节。

阶段 2：Early Kernel Handoff

- 在 `kernelBridge()` 尽早切入完整 GDB 框架。
- 让保护模式后半段和早期 C 入口复用现有 `src/debug/gdb/` 能力。
- 将 boot stub 与内核调试器衔接成一个连续会话。

阶段 3：功能验收

- 源代码级/指令级的软件断点设置与清除。
- 单步执行：步入、步过、继续执行。
- 查看与修改寄存器。
- 查看与修改内存。
- 查看调用栈回溯。
- 覆盖实模式、保护模式切换、早期内核入口等关键过程。

## Log Tips

先把调试 bootloader 部署进镜像：

```bash
make deploy-full-debug
```

启动 boot 调试 QEMU：

```bash
make qemug-boot-no-rebuild
```

另一个终端运行生命周期脚本：

```bash
make gdb-boot-lifecycle
```

如果要看 GDB 远程协议日志，可以在 GDB 中执行：

```gdb
set debug remote 1
set remotelogbase hex
target remote localhost:1234
```

常用日志文件：

- `/tmp/gdb_boot_remote.log`
- `/tmp/qemu_boot_dbg.log`
- `target/qemu.log`

常用查看命令：

```bash
sed -n '1,260p' /tmp/gdb_boot_remote.log
tail -f /tmp/gdb_boot_remote.log
tail -f /tmp/qemu_boot_dbg.log
tail -f target/qemu.log
```
