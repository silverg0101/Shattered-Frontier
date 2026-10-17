# First-time developer setup

Load this guide only when onboarding a developer, setting up a machine, or explicitly repairing the development environment. Do not load it for ordinary conversations or feature work. Agents should use only the sections needed for the requested setup.

## Project status

The runnable project is currently a Node.js browser harness. The next engine milestone targets Unreal Engine 5.8 and Visual Studio 2026; an Unreal project and packaged dedicated server have not yet been added. See [alpha milestones](alpha-plan.md).

## 1. Required software

| Software | Required for | Installation notes |
| --- | --- | --- |
| Windows with PowerShell | Current Windows setup workflow | Use a Windows version supported by the selected Unreal release. |
| [Git for Windows](https://git-scm.com/downloads/win) | Repository checkout and source builds | Include Git on PATH. Git Bash is included. |
| OpenSSH client | SSH access and deploy keys | Confirm `ssh` and `ssh-keygen` are available; Windows provides OpenSSH Client as an optional feature. |
| [Node.js](https://nodejs.org/en/download) 24 or later | Browser harness | Include npm. SQLite support is built into Node; no separate database install is needed. |
| A current browser | Browser harness | Use a second profile/private window to test another player. |
| [Unreal Engine](https://www.unrealengine.com/download) 5.8 | Engine milestone | Install through Epic Games Launcher or use a matching source build. Record the actual install directory. |
| [Visual Studio](https://visualstudio.microsoft.com/downloads/) 2026, version 18.0 or later | Unreal C++ development | Install Desktop development with C++ and Game development with C++; include the MSVC compiler and Windows SDK. |

For UE 5.8, Epic recommends VS 2026, MSVC 14.50, and Windows SDK 10.0.26100 or newer. Include .NET 10 for VS 2026. Epic's broader workload checklist also includes .NET desktop development and .NET Multi-platform App UI development. Recheck [Epic's version-specific setup guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine) when changing engine versions.

In Visual Studio Installer, also select Visual Studio Tools for Unreal Engine, Blueprint debugger tools, and Unreal Engine Test Adapter. See [Microsoft's installation steps](https://learn.microsoft.com/en-us/visualstudio/gamedev/unreal/get-started/vs-tools-unreal-install).

## 2. Repository access and deploy keys

Use your normal GitHub developer authentication for interactive work. A deploy key grants automation access to one repository; it is optional for local development. Repository administration access is needed to register it. Keep write access disabled unless the intended automation must push.

### Have a session agent generate the key

Ask: "Generate a dedicated Ed25519 deploy key for this repository under my user .ssh directory. Check for existing keys first, never overwrite them, keep the private key local, and return only the public key and fingerprint. Use a unique filename if one already exists. This is for unattended read-only repository access."

The agent can generate the pair on the machine that will use it. Registration on GitHub and configuring repository access are separate steps; supply the repository URL when requesting those actions. Unattended keys can have an empty passphrase; interactive keys can use a passphrase with an SSH agent. Never send a private key through chat or commit it.

### Generate it manually in PowerShell

```powershell
$deployKeyPath = Join-Path $env:USERPROFILE '.ssh\shattered-frontier-deploy'
New-Item -ItemType Directory -Force (Split-Path $deployKeyPath) | Out-Null
if ((Test-Path -LiteralPath $deployKeyPath) -or (Test-Path -LiteralPath "$deployKeyPath.pub")) {
    throw 'Key already exists. Reuse it intentionally or choose another filename.'
}
ssh-keygen -t ed25519 -C 'shattered-frontier-github-deploy' -f $deployKeyPath
# Answer the passphrase prompts for your intended use.
Get-Content -LiteralPath "$deployKeyPath.pub"
ssh-keygen -lf "$deployKeyPath.pub"
```

On GitHub, open the target repository's Settings → Deploy keys → Add deploy key. Name the machine/use, paste the entire `.pub` line, leave Allow write access unchecked for read-only access, and save. Each repository needs its own deploy key. See [GitHub's deploy-key guide](https://docs.github.com/en/authentication/connecting-to-github-with-ssh/managing-deploy-keys).

Merge this entry into your user `.ssh/config` file without replacing existing entries. Adjust IdentityFile if you chose another filename:

```sshconfig
Host github-shattered-frontier
    HostName github.com
    User git
    IdentityFile ~/.ssh/shattered-frontier-deploy
    IdentitiesOnly yes
```

Replace OWNER/REPO below with the actual repository. On first connection, compare the host fingerprint with [GitHub's published fingerprints](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/githubs-ssh-key-fingerprints) before accepting it.

```powershell
git ls-remote git@github-shattered-frontier:OWNER/REPO.git
git clone git@github-shattered-frontier:OWNER/REPO.git
```

Run the clone command only if you do not already have a checkout. For an existing checkout, inspect `git remote -v` and deliberately update the appropriate remote if this key should be used. Store the private key outside the checkout, accessible only to the intended user/service. Revoke its GitHub entry when retiring the machine.

## 3. Run the browser harness

From your checkout directory:

```powershell
node --version
npm --version
git --version
npm start
```

No `npm install` step is needed with the current dependency-free package. Open [the local game](http://localhost:3000), create a character, and confirm movement. Stop with Ctrl+C. World/session data lives in `data/world.sqlite`; preserve existing data when setting up an existing machine. See [README](../README.md) for controls and backup notes.

Setup does not require running the full test suite. For later changes, run only precise tests identified in [the dependency map](dependency-map.md). The full suite requires an explicit user request.

## 4. Prepare Unreal development

Finish the engine and Visual Studio installations, then record the engine version/path and installed compiler/SDK versions in the setup handoff. Once this repository contains a `.uproject`, open that project's file with the matching engine and build its editor target using Visual Studio. Until then, there is no Unreal project here to open or build.

A packaged dedicated-server build requires an Unreal source build in [Epic's documented workflow](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-dedicated-servers-in-unreal-engine). Link your GitHub account to Epic and obtain access to Epic's engine repository before that stage. This is separate from the game's deploy key; a game-repository key does not grant engine-source access.

## 5. Optional: session-agent control of Unreal

Install the skills from [Epic's Unreal skills repository](https://github.com/EpicGames/unreal-engine-skills-for-claude-code-plugin) on each agent workstation. A session agent can use its skill-installer workflow to install `skills/unreal-mcp`, `skills/unreal-skill`, and `skills/create-toolset`. These skills were installed on the initial workstation; they are not bundled with this project.

For live editor control, use the installed `unreal-mcp` setup reference only during this setup. Verify that your engine build contains ModelContextProtocol and AllToolsets, enable both in the intended project, restart the editor if needed, and run `ModelContextProtocol.StartServer`. The default endpoint is `http://127.0.0.1:8000/mcp`.

For Codex, run `ModelContextProtocol.GenerateClientConfig Codex` in the editor console if supported by your build. Inspect the generated config location; source builds may write at the engine workspace root. Merge configuration deliberately if a Codex config already exists. Claude Code's `.mcp.json` and hooks are not a substitute for Codex configuration. Verify the connection with a read-only operation such as listing level actors; installing skills alone does not establish it.

## Setup handoff

Record only software versions, checkout/engine paths, public-key fingerprint, repository access result, and which startup checks succeeded. List remaining blockers without claiming unfinished engine, server, or MCP setup is complete. Never record private keys or tokens. Update this guide when required versions or setup steps change.
