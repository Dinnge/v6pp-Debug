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
- `monitor v6json memory/<addr-hex>/<len-hex>`
- `monitor v6json inode/<n>`
- `monitor v6json block/<n>`

插件会优先经由 `qXfer:v6pp-json:read:<kind>:<offset>,<length>` 分块拉取数据；
如果当前 GDB 适配器不支持 `maintenance packet` 透传，再自动回退到
`monitor v6json ...`。

## 使用方式

### 1. 启动插件开发宿主

1. 在 VS Code 中打开 [tools/vscode](/home/dmy/v6pp/unix-v6pp-tongji/tools/vscode)。
2. 按 `F5`，会启动一个新的 Extension Development Host，并自动打开仓库根目录。

`tools/vscode/.vscode/launch.json` 已经配置好了这一步。

### 2. 启动调试目标

在仓库根目录终端执行：

```bash
make deploy-full-debug
make qemug-boot-no-rebuild
```

这会启动 QEMU，并让生命周期调试桩监听 `127.0.0.1:1234`。

### 3. 在 VS Code 中附加 GDB

1. 确保已经安装 `ms-vscode.cpptools`。
2. 把 [launch.example.json](/home/dmy/v6pp/unix-v6pp-tongji/tools/vscode/launch.example.json) 的内容复制到工作区 `.vscode/launch.json`。
3. 运行调试配置 `V6++ Lifecycle Attach`。

这个示例配置会：

- 加载 `boot_debug.elf`
- 附加到 `127.0.0.1:1234`
- 补充加载 `kernel.exe` 符号

说明：
生命周期调试的最早期 `0x00000000` 初始停机点仍由极简 boot stub 提供，
它不适合在“刚连接时”就强制打开 `range-stepping` 或 `remote verbose-resume-packet`。
如果你已经继续到 `greatstart()` 之后、并且确实需要更积极的步过优化，
可以在 GDB 控制台中手动开启这两个选项。

### 4. 查看图形化视图

附加成功后，左侧活动栏会出现 `V6++` 图标。调试器每次停下时，插件会自动刷新：

- 寄存器
- 回溯
- 当前进程/进程表
- superblock 与 mount 状态
- 文件系统事务追踪

命令面板还提供：

- `V6++: Refresh Views`
- `V6++: Inspect Memory`
- `V6++: Inspect Inode`
- `V6++: Inspect Block`
- `V6++: Open Snapshot JSON`
- `V6++: Open Launch Example`

## 备注

- 插件优先通过 DAP `evaluate` 请求执行 `maintenance packet qXfer:v6pp-json:read:...`，
  因此要求当前调试适配器能把 `maintenance packet` 转交给 GDB。`cppdbg` 满足这个要求；
  如果不支持，会自动回退到 `monitor v6json ...`。
- 如果后续需要完全脱离 GDB 控制台，可以在这个插件基础上继续把 `qXfer:v6pp-json` 和
  原始 RSP 控制做成独立调试适配器。
