# 📘 Aura CLI Documentation for Gemini Agent

This document provides instructions on how to use the `aura_cli.py` tool to interact with the Aura platform.

## 📂 Configuration
- **File:** `aura_cli.py`
- **Config Path:** `.aura_cli_config/config.json`
- **API Base:** `https://cloud.auraboard.online/api`

## 🔐 Authentication
Use the following command to log in and establish a persistent session:
```bash
python aura_cli.py auth login
```
This generates a persistent **Agent Token** and saves it locally.

## 🛠 Command Reference

### Projects
- `python aura_cli.py projects list` - List all projects with IDs.
- `python aura_cli.py projects info <ID>` - Get detailed project description, tech stack, and goals.

### Tasks
- `python aura_cli.py tasks list [--project-id ID] [--status STATUS_ID]` - List tasks.
- `python aura_cli.py tasks create "<TITLE>" --description "<DESC>" --project-id <ID> [--priority low|medium|high]` - Create a new task.
- `python aura_cli.py tasks update <TASK_ID> [--status <COL_ID>] [--title "<NEW_TITLE>"]` - Update or move a task.

### Agent Tokens
- `python aura_cli.py tokens create "<NAME>" [--project-id <ID>]` - Generate a permanent API key for agents/bots.
- `python aura_cli.py tokens list` - List active tokens.

## 🤖 Agent Workflow
1. **Check Access:** Run `projects list`.
2. **Understand Project:** Run `projects info <ID>`.
3. **Analyze State:** Run `tasks list --project-id <ID>`.
4. **Execute:** Create or update tasks as requested.
