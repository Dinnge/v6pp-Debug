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

    this.registerProvider = new StaticTreeProvider(() => this.buildRegisterItems());
    this.backtraceProvider = new StaticTreeProvider(() => this.buildBacktraceItems());
    this.processProvider = new StaticTreeProvider(() => this.buildProcessItems());
    this.filesystemProvider = new StaticTreeProvider(() => this.buildFilesystemItems());
    this.fsTraceProvider = new StaticTreeProvider(() => this.buildFsTraceItems());

    context.subscriptions.push(
      this.output,
      vscode.window.registerTreeDataProvider("v6ppDebugger.registers", this.registerProvider),
      vscode.window.registerTreeDataProvider("v6ppDebugger.backtrace", this.backtraceProvider),
      vscode.window.registerTreeDataProvider("v6ppDebugger.processes", this.processProvider),
      vscode.window.registerTreeDataProvider("v6ppDebugger.filesystem", this.filesystemProvider),
      vscode.window.registerTreeDataProvider("v6ppDebugger.fsTrace", this.fsTraceProvider),
      vscode.commands.registerCommand("v6ppDebugger.refresh", () => this.refresh()),
      vscode.commands.registerCommand("v6ppDebugger.showMemory", () =>
        this.promptJson("memory", "memory/<addr-hex>/<len-hex>", "memory/c0007a00/40")
      ),
      vscode.commands.registerCommand("v6ppDebugger.showInode", () =>
        this.promptJson("inode", "inode/<n>", "inode/1")
      ),
      vscode.commands.registerCommand("v6ppDebugger.showBlock", () =>
        this.promptJson("block", "block/<n>", "block/1")
      ),
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
      vscode.debug.onDidChangeActiveDebugSession(() => {
        if (!vscode.debug.activeDebugSession) {
          this.clearState("No active debug session");
        }
      })
    );
  }

  createTracker(session) {
    return {
      onDidSendMessage: message => {
        if (!message || message.type !== "event") {
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

    this.activeSessionId = session.id;
    this.stateMessage = `Refreshing: ${session.name}`;
    this.refreshViews();

    try {
      const [snapshot, registers, backtrace, currentProcess, processes, filesystem, fsTrace] =
        await Promise.all([
          this.requestJson(session, "snapshot"),
          this.requestJson(session, "registers"),
          this.requestJson(session, "backtrace"),
          this.requestJson(session, "current-process"),
          this.requestJson(session, "processes"),
          this.requestJson(session, "filesystem"),
          this.requestJson(session, "fs-trace")
        ]);

      this.snapshot = snapshot;
      this.registers = registers;
      this.backtrace = backtrace;
      this.currentProcess = currentProcess;
      this.processes = processes;
      this.filesystem = filesystem;
      this.fsTrace = fsTrace;
      this.stateMessage = `Stopped: ${session.name}`;
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      this.stateMessage = `Refresh failed: ${message}`;
      this.output.appendLine(`[refresh] ${message}`);
    }

    this.refreshViews();
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
    if (payload.startsWith('"') && payload.endsWith('"') && payload.length >= 2) {
      payload = payload.slice(1, -1);
    }
    return payload;
  }

  buildRegisterItems() {
    const data = this.registers && this.registers.data;
    if (!data) {
      return [makeLeaf("Status", this.stateMessage)];
    }

    return REGISTER_ORDER.filter(name => Object.prototype.hasOwnProperty.call(data, name)).map(name =>
      makeLeaf(name.toUpperCase(), String(data[name]))
    );
  }

  buildBacktraceItems() {
    const frames = this.backtrace && this.backtrace.data && this.backtrace.data.frames;
    if (!Array.isArray(frames) || !frames.length) {
      return [makeLeaf("Status", this.stateMessage)];
    }

    return frames.map(frame =>
      makeNode(
        `#${frame.index}`,
        [
          makeLeaf("pc", String(frame.pc)),
          makeLeaf("framePointer", String(frame.framePointer))
        ],
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
      items.push(
        makeNode("Current Process", this.objectChildren(current, ["flags", "memory"]), `pid=${current.pid}`)
      );
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

  buildFilesystemItems() {
    const data = this.filesystem && this.filesystem.data;
    if (!data) {
      return [makeLeaf("Status", this.stateMessage)];
    }
    if (data.status === "not-ready") {
      return [makeLeaf("Filesystem", "not-ready")];
    }

    const mounts = Array.isArray(data.mounts) ? data.mounts : [];
    return [
      makeNode("Superblock", this.objectChildren(data.superblock || {}), "root"),
      makeNode(
        "Mounts",
        mounts.map((mount, index) =>
          makeNode(`mount[${index}]`, this.objectChildren(mount), mount.device ? String(mount.device) : "")
        ),
        `${mounts.length} items`
      ),
      makeNode("Trace Preview", this.traceChildren(data.trace), Array.isArray(data.trace) ? `${data.trace.length} lines` : "")
    ];
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
      const document = await this.requestJson(session, input.trim());
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
