# V6++ VS Code Front-End

`tools/vscode` 现在是一个可直接运行的 VS Code 插件目录。它不替换 GDB，而是把
现有生命周期调试桩暴露出来的 `qXfer:v6pp-json` / `monitor v6json ...` / `qfs:*`
能力整理成图形视图：

- `Registers`
- `Backtrace`
- `Processes`
- `Filesystem`
- `FS Trace`

调试控制本身仍由 VS Code 的 C/C++ 调试器负责，插件只负责在停机点自动刷新
V6++ 特有的内核与文件系统状态。

## 已使用的数据接口

调试桩后端已经支持以下 JSON 文档：

- `monitor v6json snapshot`
- `monitor v6json registers`
- `monitor v6json backtrace`
- `monitor v6json current-process`
- `monitor v6json processes`
- `monitor v6json filesystem`
- `monitor v6json fs-trace`
- `monitor v6json directory[/path]`
- `monitor v6json file/<path>`
- `monitor v6json memory/<addr-hex>/<len-hex>`
- `monitor v6json inode/<n>`
- `monitor v6json block/<n>`

插件会优先经由 `qXfer:v6pp-json:read:<kind>:<offset>,<length>` 分块拉取数据；
如果当前 GDB 适配器不支持 `maintenance packet` 透传，再自动回退到
`monitor v6json ...`。

## 使用方式

### 1. 在当前窗口安装前端扩展

推荐直接在仓库根目录窗口里安装本地扩展，而不是再开一个 Extension Development Host：

```bash
bash tools/vscode/install_local_extension.sh
```

然后在 VS Code 里执行一次 `Developer: Reload Window`。

说明：

- 这个脚本会把 [tools/vscode](/home/dmy/v6pp/unix-v6pp-tongji/tools/vscode) 软链接到当前远端 VS Code Server 的扩展目录。
- 之后你继续在仓库里改扩展代码，只需要 `Reload Window`，不需要重新装。

### 2. 启动调试目标

在仓库根目录终端执行：

```bash
make deploy-full-debug
make qemug-boot-no-rebuild
```

这会启动 QEMU，并让生命周期调试桩监听 `127.0.0.1:1234`。

### 3. 在 VS Code 中附加 GDB

1. 确保已经安装 `ms-vscode.cpptools`。
2. 直接在仓库根目录按 `F5`，选择 `V6++ Lifecycle Attach`。

根目录工作区已经自带：

- 加载 `boot_debug.elf`
- 附加到 `127.0.0.1:1234`
- 补充加载 `kernel.exe` 符号
- 在启动前检查 `make deploy-full-debug` 和 `make qemug-boot-no-rebuild` 是否已经准备好

说明：
生命周期调试的最早期 `0x00000000` 初始停机点仍由极简 boot stub 提供，
它不适合在“刚连接时”就强制打开 `range-stepping` 或 `remote verbose-resume-packet`。
如果你已经继续到 `greatstart()` 之后、并且确实需要更积极的步过优化，
可以在 GDB 控制台中手动开启这两个选项。

### 4. 查看图形化视图

附加成功后，调试侧边栏会直接出现这些视图；不需要再切到新窗口。调试器每次停下时，插件会自动刷新：

- 寄存器
- 回溯
- 当前进程/进程表
- superblock 与 mount 状态
- 文件系统事务追踪
- 文件系统目录树
- 文件内容预览（点击文件节点即可打开）

命令面板还提供：

- `V6++: Focus Debug Views`
- `V6++: Refresh Views`
- `V6++: Run Monitor Command`
- `V6++: Inspect Memory`
- `V6++: Inspect Inode`
- `V6++: Inspect Block`
- `V6++: Open Filesystem File`
- `V6++: Open Snapshot JSON`
- `V6++: Open Launch Example`

调试控制台里也可以直接输入：

- `monitor v6json registers`
- `monitor v6json filesystem`
- `monitor v6json directory/bin`
- `monitor v6json file/etc/profile`
- `monitor ls /`
- `-exec info registers`
- `-exec bt`
- `-exec x/32xb 0xc0000000`
- `-exec set $eax = 0x1234`
- `-exec set {unsigned int}0xc0007a00 = 0xdeadbeef`

### 5. 插件开发模式（可选）

如果你后续要继续开发这个 VS Code 扩展本身，仍然可以打开 [tools/vscode](/home/dmy/v6pp/unix-v6pp-tongji/tools/vscode) 并使用它自己的 `F5` 配置来启动 Extension Development Host。

## 备注

- 插件优先通过 DAP `evaluate` 请求执行 `maintenance packet qXfer:v6pp-json:read:...`，
  因此要求当前调试适配器能把 `maintenance packet` 转交给 GDB。`cppdbg` 满足这个要求；
  如果不支持，会自动回退到 `monitor v6json ...`。
- 如果后续需要完全脱离 GDB 控制台，可以在这个插件基础上继续把 `qXfer:v6pp-json` 和
  原始 RSP 控制做成独立调试适配器。
