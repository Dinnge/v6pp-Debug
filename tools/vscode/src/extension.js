"use strict";

const vscode = require("vscode");

const REGISTER_ORDER = [
    "eax",
    "ecx",
    "edx",
    "ebx",
    "esp",
    "ebp",
    "esi",
    "edi",
    "eip",
    "eflags",
    "cs",
    "ss",
    "ds",
    "es",
    "fs",
    "gs"
];

const IFDIR = 0x4000;
const ROOTINO = 1;
const DIRSIZ = 28;
const BLOCK_SIZE = 512;
const DIRECTORY_ENTRY_SIZE = 4 + DIRSIZ;

function makeLeaf(label, description, tooltip) {
    const item = new vscode.TreeItem(label, vscode.TreeItemCollapsibleState.None);
    item.description = description;
    item.tooltip = tooltip || `${label}${description ? `: ${description}` : ""}`;
    return item;
}

function makeNode(label, children, description, tooltip) {
    const item = new vscode.TreeItem(
        label,
        children && children.length
            ? vscode.TreeItemCollapsibleState.Expanded
            : vscode.TreeItemCollapsibleState.None
    );
    item.description = description;
    item.tooltip = tooltip || label;
    item.children = children || [];
    return item;
}

function normalizeFsPath(rawPath) {
    const path = String(rawPath || "/").trim().replace(/\\/g, "/").replace(/\/+/g, "/");
    if (!path || path === "/") {
        return "/";
    }
    return path.startsWith("/") ? path : `/${path}`;
}

function joinFsPath(parentPath, name) {
    const base = normalizeFsPath(parentPath);
    if (!name) {
        return base;
    }
    return base === "/" ? `/${name}` : `${base}/${name}`;
}

class StaticTreeProvider {
    constructor(getItems) {
        this.getItems = getItems;
        this._emitter = new vscode.EventEmitter();
        this.onDidChangeTreeData = this._emitter.event;
    }

    refresh() {
        this._emitter.fire(undefined);
    }

    getTreeItem(element) {
        return element;
    }

    getChildren(element) {
        if (element && Array.isArray(element.children)) {
            return Promise.resolve(element.children);
        }
        return Promise.resolve(this.getItems());
    }
}

class FileSystemTreeProvider {
    constructor(ui) {
        this.ui = ui;
        this._emitter = new vscode.EventEmitter();
        this.onDidChangeTreeData = this._emitter.event;
    }

    refresh() {
        this._emitter.fire(undefined);
    }

    getTreeItem(element) {
        return element;
    }

    async getChildren(element) {
        if (!element) {
            return this.getRootItems();
        }

        if (element.isDirectory) {
            return this.loadDirectoryChildren(element);
        }

        return [];
    }

    async getRootItems() {
        const items = [];
        const root = new vscode.TreeItem("/", vscode.TreeItemCollapsibleState.Expanded);
        root.inodeNo = ROOTINO;
        root.isDirectory = true;
        root.fullPath = "/";
        root.iconPath = new vscode.ThemeIcon("folder");
        root.tooltip = "V6++ root directory";

        try {
            root.children = await this.loadDirectoryChildren(root);
        } catch (error) {
            root.children = [makeLeaf("Status", error instanceof Error ? error.message : String(error))];
        }
        items.push(root);

        if (this.ui.filesystem && this.ui.filesystem.data) {
            const data = this.ui.filesystem.data;
            if (data.superblock) {
                items.push(makeNode("Superblock", this.ui.objectChildren(data.superblock), "", "Superblock information"));
            }

            const mounts = Array.isArray(data.mounts) ? data.mounts : [];
            if (mounts.length) {
                items.push(
                    makeNode(
                        "Mounts",
                        mounts.map((mount, index) =>
                            makeNode(
                                `mount[${index}]`,
                                this.ui.objectChildren(mount),
                                mount.device ? String(mount.device) : (mount.dev !== undefined ? `dev=${mount.dev}` : "")
                            )
                        ),
                        `${mounts.length} items`,
                        "Mounted file systems"
                    )
                );
            }

            if (Array.isArray(data.trace)) {
                items.push(makeNode("Trace Preview", this.ui.traceChildren(data.trace), `${data.trace.length} lines`, "File system trace"));
            }
        } else {
            items.push(makeLeaf("Status", this.ui.stateMessage));
        }

        return items;
    }

    async loadDirectoryChildren(dirNode) {
        const session = vscode.debug.activeDebugSession;
        if (!session) {
            return [makeLeaf("Status", this.ui.stateMessage)];
        }

        const dirPath = normalizeFsPath(dirNode.fullPath || "/");
        try {
            const listing = await this.ui.requestDirectory(session, dirPath);
            if (listing && listing.ok && Array.isArray(listing.entries)) {
                return this.buildItemsFromEntries(dirPath, listing.entries);
            }
        } catch (error) {
            this.ui.output.appendLine(`[filesystem] directory endpoint fallback for ${dirPath}: ${error instanceof Error ? error.message : String(error)}`);
        }

        return this.loadDirectoryChildrenLegacy(session, dirNode.inodeNo || ROOTINO, dirPath);
    }

    buildItemsFromEntries(parentPath, entries) {
        if (!entries.length) {
            return [makeLeaf("(empty)", "", "This directory is empty")];
        }

        const dirEntries = entries
            .filter(entry => entry.isDirectory)
            .sort((lhs, rhs) => String(lhs.name).localeCompare(String(rhs.name)));
        const fileEntries = entries
            .filter(entry => !entry.isDirectory)
            .sort((lhs, rhs) => String(lhs.name).localeCompare(String(rhs.name)));

        const children = [];
        for (const entry of dirEntries.concat(fileEntries)) {
            if (entry.name === "." || entry.name === "..") {
                continue;
            }

            const item = new vscode.TreeItem(
                entry.name,
                entry.isDirectory ? vscode.TreeItemCollapsibleState.Collapsed : vscode.TreeItemCollapsibleState.None
            );
            item.description = `inode ${entry.inode}${entry.size !== undefined ? `, ${entry.size} bytes` : ""}`;
            item.tooltip = `${entry.isDirectory ? "Directory" : "File"}: ${normalizeFsPath(entry.path || joinFsPath(parentPath, entry.name))}`;
            item.inodeNo = entry.inode;
            item.isDirectory = entry.isDirectory;
            item.fullPath = normalizeFsPath(entry.path || joinFsPath(parentPath, entry.name));
            item.iconPath = new vscode.ThemeIcon(entry.isDirectory ? "folder" : "file");
            item.contextValue = entry.isDirectory ? "v6ppDirectory" : "v6ppFile";
            if (!entry.isDirectory) {
                item.command = {
                    command: "v6ppDebugger.openFile",
                    title: "Open Filesystem File",
                    arguments: [item]
                };
            }
            children.push(item);
        }

        return children.length ? children : [makeLeaf("(empty)", "", "This directory is empty")];
    }

    async loadDirectoryChildrenLegacy(session, inodeNo, fullPath) {
        const children = [];

        try {
            const inodeData = await this.ui.requestJson(session, `inode/${inodeNo}`);
            if (!inodeData || !inodeData.data || !inodeData.data.ok) {
                return [makeLeaf("(error)", "", `Failed to read inode ${inodeNo}`)];
            }

            const inode = inodeData.data;
            if (!(inode.mode & IFDIR)) {
                return [makeLeaf("(not a directory)", "", `Mode: ${inode.mode}`)];
            }

            const entries = await this.parseDirectoryEntriesLegacy(session, inode, fullPath);
            return this.buildItemsFromEntries(fullPath, entries);
        } catch (error) {
            children.push(makeLeaf("(error)", "", `Failed to load directory: ${error instanceof Error ? error.message : String(error)}`));
            return children;
        }
    }

    async parseDirectoryEntriesLegacy(session, inode, fullPath) {
        const entries = [];
        const addr = Array.isArray(inode.addr) ? inode.addr : [];
        const size = inode.size || 0;
        let currentOffset = 0;

        for (let i = 0; i < addr.length && currentOffset < size; i++) {
            const blockNo = addr[i];
            if (!blockNo) {
                continue;
            }

            const blockEntries = await this.parseBlockForDirectoryEntries(session, blockNo, currentOffset, size, fullPath);
            entries.push(...blockEntries);
            currentOffset += BLOCK_SIZE;
        }

        return entries;
    }

    async parseBlockForDirectoryEntries(session, blockNo, blockOffset, totalSize, fullPath) {
        const entries = [];
        const blockData = await this.ui.requestJson(session, `block/${blockNo}`);
        if (!blockData || !blockData.data || !blockData.data.ok) {
            return entries;
        }

        const hexData = blockData.data.data;
        const bytes = this.hexToBinary(hexData);
        const bytesLeftInFile = totalSize - blockOffset;
        const bytesInBlock = Math.min(BLOCK_SIZE, bytesLeftInFile);

        for (let offset = 0; offset + DIRECTORY_ENTRY_SIZE <= bytesInBlock; offset += DIRECTORY_ENTRY_SIZE) {
            const inodeBytes = bytes.slice(offset, offset + 4);
            const inodeNo = (inodeBytes[0] | (inodeBytes[1] << 8) | (inodeBytes[2] << 16) | (inodeBytes[3] << 24)) >>> 0;
            if (!inodeNo) {
                continue;
            }

            const nameBytes = bytes.slice(offset + 4, offset + 4 + DIRSIZ);
            let name = "";
            for (let i = 0; i < DIRSIZ; i++) {
                if (nameBytes[i] === 0) {
                    break;
                }
                name += String.fromCharCode(nameBytes[i]);
            }
            if (!name) {
                continue;
            }

            let isDirectory = false;
            let fileSize;
            try {
                const inodeInfo = await this.ui.requestJson(session, `inode/${inodeNo}`);
                if (inodeInfo && inodeInfo.data && inodeInfo.data.ok) {
                    isDirectory = !!(inodeInfo.data.mode & IFDIR);
                    fileSize = inodeInfo.data.size;
                }
            } catch {
                // Keep fallback values.
            }

            entries.push({
                name,
                inode: inodeNo,
                isDirectory,
                size: fileSize,
                path: joinFsPath(fullPath, name)
            });
        }

        return entries;
    }

    hexToBinary(hexString) {
        const bytes = [];
        for (let i = 0; i < hexString.length; i += 2) {
            bytes.push(parseInt(hexString.slice(i, i + 2), 16));
        }
        return bytes;
    }
}

class V6ppDebuggerUi {
    constructor(context) {
        this.context = context;
        this.output = vscode.window.createOutputChannel("V6++ Debugger");
        this.snapshot = null;
        this.registers = null;
        this.backtrace = null;
        this.currentProcess = null;
        this.processes = null;
        this.filesystem = null;
        this.fsTrace = null;
        this.stateMessage = "No stopped debug session";
        this.activeSessionId = null;
        this._viewsOpened = false;

        this.registerProvider = new StaticTreeProvider(() => this.buildRegisterItems());
        this.backtraceProvider = new StaticTreeProvider(() => this.buildBacktraceItems());
        this.processProvider = new StaticTreeProvider(() => this.buildProcessItems());
        this.filesystemProvider = new FileSystemTreeProvider(this);
        this.fsTraceProvider = new StaticTreeProvider(() => this.buildFsTraceItems());

        context.subscriptions.push(
            this.output,
            vscode.window.registerTreeDataProvider("v6ppDebugger.registers", this.registerProvider),
            vscode.window.registerTreeDataProvider("v6ppDebugger.backtrace", this.backtraceProvider),
            vscode.window.registerTreeDataProvider("v6ppDebugger.processes", this.processProvider),
            vscode.window.registerTreeDataProvider("v6ppDebugger.filesystem", this.filesystemProvider),
            vscode.window.registerTreeDataProvider("v6ppDebugger.fsTrace", this.fsTraceProvider),
            vscode.commands.registerCommand("v6ppDebugger.openViews", () => this.openViews()),
            vscode.commands.registerCommand("v6ppDebugger.refresh", () => this.refresh()),
            vscode.commands.registerCommand("v6ppDebugger.showMemory", () =>
                this.promptJson("memory", "memory/<addr-hex>/<len-hex>", "memory/c0007a00/40")
            ),
            vscode.commands.registerCommand("v6ppDebugger.showInode", item =>
                item && item.inodeNo
                    ? this.showJsonForKind(`inode-${item.inodeNo}.json`, `inode/${item.inodeNo}`)
                    : this.promptJson("inode", "inode/<n>", "inode/1")
            ),
            vscode.commands.registerCommand("v6ppDebugger.showBlock", item =>
                item && item.blockNo
                    ? this.showJsonForKind(`block-${item.blockNo}.json`, `block/${item.blockNo}`)
                    : this.promptJson("block", "block/<n>", "block/1")
            ),
            vscode.commands.registerCommand("v6ppDebugger.openFile", item => this.openFile(item)),
            vscode.commands.registerCommand("v6ppDebugger.runMonitorCommand", () => this.runMonitorCommand()),
            vscode.commands.registerCommand("v6ppDebugger.showSnapshot", () => this.showSnapshot()),
            vscode.commands.registerCommand("v6ppDebugger.openLaunchExample", () => this.openLaunchExample()),
            vscode.debug.registerDebugAdapterTrackerFactory("*", {
                createDebugAdapterTracker: session => this.createTracker(session)
            }),
            vscode.debug.onDidTerminateDebugSession(session => {
                if (session.id === this.activeSessionId) {
                    this.clearState("Debug session terminated");
                }
            }),
            vscode.debug.onDidChangeActiveDebugSession(session => {
                if (!session) {
                    this.clearState("No active debug session");
                }
            })
        );
    }

    isV6ppSession(session) {
        if (!session) {
            return false;
        }

        const name = String(session.name || "");
        const config = session.configuration || {};
        return name.includes("V6++") ||
            String(config.miDebuggerServerAddress || "").includes("127.0.0.1:1234") ||
            String(config.program || "").includes("boot_debug.elf");
    }

    createTracker(session) {
        return {
            onDidSendMessage: message => {
                if (!message || message.type !== "event" || !this.isV6ppSession(session)) {
                    return;
                }

                if (message.event === "stopped") {
                    void this.refresh(session);
                } else if (message.event === "continued") {
                    this.markRunning(session);
                } else if (message.event === "terminated" || message.event === "exited") {
                    if (session.id === this.activeSessionId) {
                        this.clearState("Debug session exited");
                    }
                }
            }
        };
    }

    async openViews() {
        await vscode.commands.executeCommand("workbench.view.debug");
        await vscode.commands.executeCommand("v6ppDebugger.filesystem.focus");
        this._viewsOpened = true;
    }

    markRunning(session) {
        this.activeSessionId = session.id;
        this.stateMessage = `Running: ${session.name}`;
        this.refreshViews();
    }

    clearState(message) {
        this.snapshot = null;
        this.registers = null;
        this.backtrace = null;
        this.currentProcess = null;
        this.processes = null;
        this.filesystem = null;
        this.fsTrace = null;
        this.activeSessionId = null;
        this.stateMessage = message;
        this.refreshViews();
    }

    refreshViews() {
        this.registerProvider.refresh();
        this.backtraceProvider.refresh();
        this.processProvider.refresh();
        this.filesystemProvider.refresh();
        this.fsTraceProvider.refresh();
    }

    async refresh(session = vscode.debug.activeDebugSession) {
        if (!session) {
            this.clearState("No active debug session");
            return;
        }
        if (!this.isV6ppSession(session)) {
            return;
        }

        this.activeSessionId = session.id;
        this.stateMessage = `Refreshing: ${session.name}`;
        this.refreshViews();

        const requests = [
            ["snapshot", () => this.requestDebuggerDocument(session, "snapshot")],
            ["registers", () => this.requestDebuggerDocument(session, "registers")],
            ["backtrace", () => this.requestDebuggerDocument(session, "backtrace")],
            ["currentProcess", () => this.requestDebuggerDocument(session, "current-process")],
            ["processes", () => this.requestDebuggerDocument(session, "processes")],
            ["filesystem", () => this.requestDebuggerDocument(session, "filesystem")],
            ["fsTrace", () => this.requestDebuggerDocument(session, "fs-trace")]
        ];

        const results = await Promise.allSettled(requests.map(([, load]) => load()));
        const failures = [];
        let successCount = 0;

        for (let index = 0; index < requests.length; index++) {
            const [field] = requests[index];
            const result = results[index];
            if (result.status === "fulfilled") {
                this[field] = result.value;
                successCount++;
                continue;
            }

            this[field] = null;
            const message = result.reason instanceof Error ? result.reason.message : String(result.reason);
            failures.push(`${field}: ${message}`);
            this.output.appendLine(`[refresh] ${field}: ${message}`);
        }

        if (successCount > 0) {
            this.stateMessage = failures.length
                ? `Stopped: ${session.name} (${failures.length} partial issue${failures.length === 1 ? "" : "s"})`
                : `Stopped: ${session.name}`;

            if (!this._viewsOpened) {
                setTimeout(() => {
                    void this.openViews();
                }, 150);
            }
        } else {
            this.stateMessage = failures.length
                ? `Refresh failed: ${failures[0]}`
                : `Refresh failed: ${session.name}`;
        }

        this.refreshViews();
    }

    async requestDirectory(session, dirPath) {
        const normalized = normalizeFsPath(dirPath);
        const kind = normalized === "/" ? "directory" : `directory${normalized}`;
        const document = await this.requestDebuggerDocument(session, kind);
        return document && document.data;
    }

    async requestDebuggerDocument(session, kind) {
        try {
            return await this.requestJson(session, kind);
        } catch (error) {
            if (kind === "registers") {
                this.output.appendLine(`[gdb fallback] ${kind}: ${error instanceof Error ? error.message : String(error)}`);
                return this.requestRegistersViaGdb(session);
            }
            if (kind === "backtrace") {
                this.output.appendLine(`[gdb fallback] ${kind}: ${error instanceof Error ? error.message : String(error)}`);
                return this.requestBacktraceViaGdb(session);
            }
            throw error;
        }
    }

    async requestJson(session, kind) {
        try {
            return await this.requestJsonViaQxfer(session, kind);
        } catch (error) {
            this.output.appendLine(`[qxfer fallback] ${kind}: ${error instanceof Error ? error.message : String(error)}`);
            const response = await session.customRequest("evaluate", {
                expression: `monitor v6json ${kind}`,
                context: "repl"
            });

            const text = this.extractText(response);
            const jsonText = this.extractJsonText(text);
            return JSON.parse(jsonText);
        }
    }

    async requestJsonViaQxfer(session, kind) {
        const chunkSize = 0x400;
        let offset = 0;
        let assembled = "";

        while (true) {
            const response = await session.customRequest("evaluate", {
                expression: `maintenance packet qXfer:v6pp-json:read:${kind}:${offset.toString(16)},${chunkSize.toString(16)}`,
                context: "repl"
            });

            const text = this.extractText(response);
            const packet = this.extractQxferPacket(text);
            if (!packet) {
                throw new Error(`No qXfer payload found in: ${text}`);
            }

            const status = packet[0];
            const payload = packet.slice(1);
            assembled += payload;

            if (status === "l") {
                break;
            }
            if (status !== "m") {
                throw new Error(`Unexpected qXfer status '${status}'`);
            }

            offset += payload.length;
        }

        return JSON.parse(assembled);
    }

    extractText(response) {
        if (typeof response === "string") {
            return response;
        }
        if (response && typeof response.result === "string") {
            return response.result;
        }
        if (response && response.body && typeof response.body.result === "string") {
            return response.body.result;
        }
        return JSON.stringify(response);
    }

    extractJsonText(text) {
        const start = text.indexOf("{");
        const end = text.lastIndexOf("}");
        if (start < 0 || end < start) {
            throw new Error(`No JSON document found in: ${text}`);
        }
        return text.slice(start, end + 1);
    }

    extractQxferPacket(text) {
        const lines = String(text)
            .split(/\r?\n/)
            .map(line => line.trim())
            .filter(Boolean);
        const receivedLine = [...lines].reverse().find(line => line.startsWith("received:"));
        if (!receivedLine) {
            return null;
        }

        let payload = receivedLine.slice("received:".length).trim();
        if (payload.startsWith("\"") && payload.endsWith("\"") && payload.length >= 2) {
            payload = payload.slice(1, -1);
        }
        return payload;
    }

    async requestRegistersViaGdb(session) {
        const response = await session.customRequest("evaluate", {
            expression: "-exec info registers",
            context: "repl"
        });
        const text = this.extractText(response);
        const data = {};

        for (const line of String(text).split(/\r?\n/)) {
            const match = line.trim().match(/^([a-z][a-z0-9]+)\s+(0x[0-9a-fA-F]+)/);
            if (!match) {
                continue;
            }
            data[match[1].toLowerCase()] = match[2].toLowerCase();
        }

        if (!Object.keys(data).length) {
            throw new Error(`Unable to parse GDB registers from: ${text}`);
        }

        return {
            schemaVersion: 1,
            kind: "registers",
            data
        };
    }

    async requestBacktraceViaGdb(session) {
        const response = await session.customRequest("evaluate", {
            expression: "-exec bt",
            context: "repl"
        });
        const text = this.extractText(response);
        const frames = [];

        for (const line of String(text).split(/\r?\n/)) {
            const match = line.trim().match(/^#(\d+)\s+(0x[0-9a-fA-F]+)/);
            if (!match) {
                continue;
            }
            frames.push({
                index: Number.parseInt(match[1], 10),
                pc: match[2].toLowerCase(),
                framePointer: "n/a"
            });
        }

        if (!frames.length) {
            throw new Error(`Unable to parse GDB backtrace from: ${text}`);
        }

        return {
            schemaVersion: 1,
            kind: "backtrace",
            data: { frames }
        };
    }

    buildRegisterItems() {
        const data = this.registers && this.registers.data;
        if (!data) {
            return [makeLeaf("Status", this.stateMessage)];
        }

        return REGISTER_ORDER
            .filter(name => Object.prototype.hasOwnProperty.call(data, name))
            .map(name => makeLeaf(name.toUpperCase(), String(data[name])));
    }

    buildBacktraceItems() {
        const frames = this.backtrace && this.backtrace.data && this.backtrace.data.frames;
        if (!Array.isArray(frames) || !frames.length) {
            return [makeLeaf("Status", this.stateMessage)];
        }

        return frames.map(frame =>
            makeNode(
                `#${frame.index}`,
                frame.framePointer !== undefined
                    ? [
                        makeLeaf("pc", String(frame.pc)),
                        makeLeaf("framePointer", String(frame.framePointer))
                    ]
                    : [makeLeaf("pc", String(frame.pc))],
                String(frame.pc)
            )
        );
    }

    buildProcessItems() {
        const items = [];
        const current = this.currentProcess && this.currentProcess.data;
        const processes = this.processes && this.processes.data && this.processes.data.items;

        if (!current && (!Array.isArray(processes) || !processes.length)) {
            return [makeLeaf("Status", this.stateMessage)];
        }

        if (current && current.status !== "not-ready") {
            items.push(makeNode("Current Process", this.objectChildren(current, ["flags", "memory"]), `pid=${current.pid}`));
        }

        if (Array.isArray(processes)) {
            items.push(
                makeNode(
                    "All Processes",
                    processes.map(proc =>
                        makeNode(
                            `pid=${proc.pid}`,
                            this.objectChildren(proc, ["flags"]),
                            `${proc.state}${proc.current ? " current" : ""}`
                        )
                    ),
                    `${processes.length} items`
                )
            );
        }

        return items;
    }

    buildFsTraceItems() {
        const trace = this.fsTrace && this.fsTrace.data && this.fsTrace.data.trace;
        if (!Array.isArray(trace) || !trace.length) {
            return [makeLeaf("Status", this.stateMessage)];
        }
        return this.traceChildren(trace);
    }

    traceChildren(lines) {
        if (!Array.isArray(lines) || !lines.length) {
            return [makeLeaf("trace", "(empty)")];
        }
        return lines.map((line, index) => makeLeaf(`[${index}]`, String(line).trim()));
    }

    objectChildren(value, nestedKeys = []) {
        if (!value || typeof value !== "object") {
            return [];
        }

        return Object.entries(value).map(([key, entryValue]) => {
            if (Array.isArray(entryValue)) {
                return makeNode(
                    key,
                    entryValue.map((item, index) => {
                        if (item && typeof item === "object") {
                            return makeNode(`[${index}]`, this.objectChildren(item));
                        }
                        return makeLeaf(`[${index}]`, String(item));
                    }),
                    `${entryValue.length} items`
                );
            }

            if (entryValue && typeof entryValue === "object") {
                return makeNode(key, this.objectChildren(entryValue), nestedKeys.includes(key) ? "nested" : "");
            }

            return makeLeaf(key, String(entryValue));
        });
    }

    async showJsonForKind(fileName, kind) {
        const session = vscode.debug.activeDebugSession;
        if (!session) {
            vscode.window.showErrorMessage("No active debug session.");
            return;
        }

        try {
            const document = await this.requestDebuggerDocument(session, kind);
            await this.showJsonDocument(fileName, document);
        } catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            vscode.window.showErrorMessage(`V6++ request failed: ${message}`);
        }
    }

    async promptJson(kindLabel, prompt, example) {
        const session = vscode.debug.activeDebugSession;
        if (!session) {
            vscode.window.showErrorMessage("No active debug session.");
            return;
        }

        const input = await vscode.window.showInputBox({
            prompt: `Enter ${prompt}`,
            value: example
        });
        if (!input) {
            return;
        }

        try {
            const document = await this.requestDebuggerDocument(session, input.trim());
            await this.showJsonDocument(`${kindLabel}.json`, document);
        } catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            vscode.window.showErrorMessage(`V6++ ${kindLabel} request failed: ${message}`);
        }
    }

    async showSnapshot() {
        if (!this.snapshot) {
            await this.refresh();
        }
        if (!this.snapshot) {
            return;
        }
        await this.showJsonDocument("snapshot.json", this.snapshot);
    }

    async openFile(item) {
        const session = vscode.debug.activeDebugSession;
        if (!session) {
            vscode.window.showErrorMessage("No active debug session.");
            return;
        }
        if (!item || item.isDirectory) {
            vscode.window.showErrorMessage("Please select a filesystem file.");
            return;
        }

        const fullPath = normalizeFsPath(item.fullPath || item.path || item.label);
        try {
            const document = await this.requestDebuggerDocument(session, `file${fullPath}`);
            const data = document && document.data;
            if (!data || !data.ok) {
                const message = data && data.message ? data.message : "file request failed";
                throw new Error(message);
            }

            const preview = this.renderFilePreview(data);
            const doc = await vscode.workspace.openTextDocument({
                language: "text",
                content: preview
            });
            await vscode.window.showTextDocument(doc, { preview: false });
            this.output.appendLine(`[open-file] ${fullPath}`);
        } catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            vscode.window.showErrorMessage(`V6++ file preview failed: ${message}`);
        }
    }

    renderFilePreview(data) {
        const lines = [
            `Path: ${data.path}`,
            `Inode: ${data.inode}`,
            `Size: ${data.size} bytes`,
            `Preview: ${data.previewLength} bytes${data.truncated ? " (truncated)" : ""}`,
            ""
        ];

        const asciiPreview = String(data.asciiPreview || "");
        if (asciiPreview) {
            lines.push("ASCII Preview");
            lines.push(asciiPreview);
            lines.push("");
        }

        const hexPreview = this.formatHexDump(String(data.data || ""));
        if (hexPreview) {
            lines.push("Hex Preview");
            lines.push(hexPreview);
        }

        return lines.join("\n");
    }

    formatHexDump(hexData) {
        if (!hexData) {
            return "";
        }

        const bytes = [];
        for (let index = 0; index + 1 < hexData.length; index += 2) {
            bytes.push(Number.parseInt(hexData.slice(index, index + 2), 16));
        }

        const lines = [];
        for (let offset = 0; offset < bytes.length; offset += 16) {
            const chunk = bytes.slice(offset, offset + 16);
            const hexChunk = chunk.map(byte => byte.toString(16).padStart(2, "0")).join(" ");
            const asciiChunk = chunk
                .map(byte => (byte >= 32 && byte <= 126 ? String.fromCharCode(byte) : "."))
                .join("");
            lines.push(`${offset.toString(16).padStart(4, "0")}: ${hexChunk.padEnd(47, " ")}  ${asciiChunk}`);
        }
        return lines.join("\n");
    }

    async runMonitorCommand() {
        const session = vscode.debug.activeDebugSession;
        if (!session) {
            vscode.window.showErrorMessage("No active debug session.");
            return;
        }

        const input = await vscode.window.showInputBox({
            prompt: "Enter a GDB monitor command",
            value: "v6json filesystem"
        });
        if (!input) {
            return;
        }

        try {
            const response = await session.customRequest("evaluate", {
                expression: `monitor ${input.trim()}`,
                context: "repl"
            });
            const text = this.extractText(response);
            const doc = await vscode.workspace.openTextDocument({
                language: "text",
                content: text
            });
            await vscode.window.showTextDocument(doc, { preview: false });
            this.output.appendLine(`[monitor] ${input.trim()}`);
        } catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            vscode.window.showErrorMessage(`Monitor command failed: ${message}`);
        }
    }

    async showJsonDocument(fileName, document) {
        const text = JSON.stringify(document, null, 2);
        const doc = await vscode.workspace.openTextDocument({
            language: "json",
            content: text
        });
        await vscode.window.showTextDocument(doc, { preview: false });
        this.output.appendLine(`[open] ${fileName}`);
    }

    async openLaunchExample() {
        const examplePath = vscode.Uri.joinPath(this.context.extensionUri, "launch.example.json");
        const doc = await vscode.workspace.openTextDocument(examplePath);
        await vscode.window.showTextDocument(doc, { preview: false });
    }
}

function activate(context) {
    new V6ppDebuggerUi(context);
}

function deactivate() {}

module.exports = {
    activate,
    deactivate
};
