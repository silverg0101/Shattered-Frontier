@AGENTS.md

# Claude Code notes

- Unreal Editor MCP: `.mcp.json` server `unreal-editor` at `http://127.0.0.1:8000/mcp`. It answers only while the editor is open; `GameClient/Config/DefaultEditorPerProjectUserSettings.ini` auto-starts it. Use it to inspect and for one-off checks. Any change that must survive becomes a committed script (`GameClient/ArtSource/**`, `tools/`) or a committed asset; never leave lasting work as editor-only clicks. The server exposes three meta-tools (`list_toolsets`, `describe_toolset`, `call_tool`); `call_tool` takes the short tool name (`find_actors`), not the dotted name `describe_toolset` prints.
- Only write under `/Game/Frontier/`. Megascans/Fab content is licensed, git-ignored and read-only.
- Close the editor before `/build-editor`, or use Live Coding while it is open.
- With the editor open, don't run headless commandlets that rewrite maps or assets it may hold. Run the script inside the editor instead: `python tools/Run-InEditor.py <script.py>`. This needs Project Settings > Plugins > Python > "Enable Remote Execution?" and works on localhost only. MCP's `execute_tool_script` is sandboxed and can't import `unreal`.
- Project commands: `/build-editor`, `/test <filter>`, `/netcheck <name>`, `/implement <spec>`.
