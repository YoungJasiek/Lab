import argparse
import json
import os
import requests
import sys
import getpass
import socket
from datetime import datetime

CONFIG_DIR = os.path.join(os.getcwd(), ".aura_cli_config")
CONFIG_FILE = os.path.join(CONFIG_DIR, "config.json")

def load_config():
    if not os.path.exists(CONFIG_FILE):
        return {}
    try:
        with open(CONFIG_FILE, "r") as f:
            return json.load(f)
    except:
        return {}

def save_config(config):
    if not os.path.exists(CONFIG_DIR):
        os.makedirs(CONFIG_DIR)
    with open(CONFIG_FILE, "w") as f:
        json.dump(config, f, indent=4)

def get_api_url(config):
    return config.get("api_url", "https://cloud.auraboard.online/api").rstrip('/')

def get_headers(config):
    token = config.get("token")
    if not token:
        print("Error: No token found. Run 'python aura_cli.py auth login' first.")
        sys.exit(1)
    return {
        "Authorization": f"Bearer {token}",
        "Content-Type": "application/json"
    }

def cmd_auth_login(args):
    config = load_config()
    base_url = get_api_url(config)
    
    print(f"Logging in to {base_url}...")
    username = input("Username: ").strip()
    password = getpass.getpass("Password: ")
    
    try:
        # 1. Get temporary JWT
        token_url = f"{base_url}/token"
        resp = requests.post(token_url, data={"username": username, "password": password})
        
        if resp.status_code != 200:
            print(f"Login failed: {resp.text}")
            return

        jwt_token = resp.json().get("access_token")
        temp_headers = {"Authorization": f"Bearer {jwt_token}", "Content-Type": "application/json"}

        # 2. Automatically create a PERSISTENT Agent Token for this CLI
        print("Creating persistent CLI session...")
        hostname = socket.gethostname()
        token_name = f"CLI Session ({hostname})"
        
        create_url = f"{base_url}/tokens"
        t_resp = requests.post(create_url, headers=temp_headers, json={"name": token_name})
        
        if t_resp.status_code == 200:
            persistent_token = t_resp.json().get("token")
            config['token'] = persistent_token
            save_config(config)
            print(f"\nSUCCESS! Logged in as {username}.")
            print("A persistent access key has been generated and saved.")
        else:
            # Fallback to JWT if token creation fails
            config['token'] = jwt_token
            save_config(config)
            print("\nLogged in with temporary session (JWT).")
            
    except Exception as e:
        print(f"Error during login: {e}")

def cmd_configure(args):
    config = load_config()
    current_url = config.get('api_url', 'https://cloud.auraboard.online/api')
    api_url = input(f"API URL [{current_url}]: ").strip()
    if api_url:
        config['api_url'] = api_url
    else:
        config['api_url'] = current_url
    save_config(config)
    print("Configuration saved.")

def cmd_projects_list(args):
    config = load_config()
    url = f"{get_api_url(config)}/projects"
    try:
        resp = requests.get(url, headers=get_headers(config))
        if resp.status_code == 401:
            print("Error: Session expired or invalid. Please 'auth login' again.")
            return
        resp.raise_for_status()
        projects = resp.json()
        print(f"{'UID':<40} {'NAME':<30} {'TASKS':<10}")
        print("-" * 80)
        for p in projects:
            uid = p.get('uid', p.get('id')) # Fallback to id if uid missing
            print(f"{str(uid):<40} {p['name']:<30} {p.get('total_tasks', 0):<10}")
    except Exception as e:
        print(f"Error: {e}")

def cmd_project_info(args):
    config = load_config()
    # Use the ID or UID from arguments
    url = f"{get_api_url(config)}/projects/{args.id}"
    try:
        resp = requests.get(url, headers=get_headers(config))
        resp.raise_for_status()
        p = resp.json()
        print(f"=== {p['name']} (UID: {p.get('uid', 'N/A')}) ===")
        print(f"\n[Description]\n{p.get('description', 'No description')}")
        print(f"\n[Tech Stack]\n{p.get('tech_stack', 'N/A')}")
        print(f"\n[Goals]\n{p.get('goals', 'N/A')}")
        print(f"\n[Stats]")
        print(f"Total Tasks: {p.get('total_tasks', 0)}")
        print(f"Completed:   {p.get('completed_tasks', 0)}")
    except Exception as e:
        print(f"Error: {e}")

def cmd_tokens_list(args):
    config = load_config()
    url = f"{get_api_url(config)}/tokens"
    try:
        resp = requests.get(url, headers=get_headers(config))
        resp.raise_for_status()
        tokens = resp.json()
        print(f"{'ID':<5} {'NAME':<20} {'TOKEN (prefix)':<20} {'PROJECT'}")
        print("-" * 60)
        for t in tokens:
            token_display = t['token'][:10] + "..."
            print(f"{t['id']:<5} {t['name']:<20} {token_display:<20} {t.get('project_id', 'Global')}")
    except Exception as e:
        print(f"Error: {e}")

def cmd_tokens_create(args):
    config = load_config()
    url = f"{get_api_url(config)}/tokens"
    data = {"name": args.name, "role": "editor"}
    if args.project_id:
        data["project_id"] = int(args.project_id)
        
    try:
        resp = requests.post(url, headers=get_headers(config), json=data)
        resp.raise_for_status()
        t = resp.json()
        print(f"\nSUCCESS! Token Created.")
        print(f"Name:  {t['name']}")
        print(f"Token: {t['token']}")
        print(f"\nIMPORTANT: Save this token now! It's your key for external agents/bots.")
    except Exception as e:
        print(f"Error: {e}")
        try:
             print("Details:", e.response.text)
        except: pass

def cmd_tasks_list(args):
    config = load_config()
    base_url = get_api_url(config)
    
    if args.project_id:
        url = f"{base_url}/projects/{args.project_id}/tasks"
    else:
        url = f"{base_url}/users/me/tasks"

    try:
        resp = requests.get(url, headers=get_headers(config))
        resp.raise_for_status()
        tasks = resp.json()
        
        if args.status:
            tasks = [t for t in tasks if str(t.get('status')) == args.status]

        print(f"{'ID':<5} {'STATUS':<10} {'PRIORITY':<10} {'TITLE'}")
        print("-" * 70)
        for t in tasks:
            status = t.get('status', '?')
            priority = t.get('priority', 'normal')
            print(f"{t['id']:<5} {str(status):<10} {priority:<10} {t['title']}")
    except Exception as e:
        print(f"Error: {e}")

def cmd_tasks_create(args):
    config = load_config()
    base_url = get_api_url(config)
    
    if args.project_id:
        url = f"{base_url}/projects/{args.project_id}/tasks"
    else:
        url = f"{base_url}/tasks"
        
    data = {
        "title": args.title,
        "description": args.description,
        "priority": args.priority
    }
    if args.status:
        data["status"] = int(args.status)
        
    try:
        resp = requests.post(url, headers=get_headers(config), json=data)
        resp.raise_for_status()
        t = resp.json()
        print(f"Task created successfully! ID: {t['id']}")
    except Exception as e:
        print(f"Error creating task: {e}")

def cmd_tasks_update(args):
    config = load_config()
    url = f"{get_api_url(config)}/tasks/{args.id}"
    data = {}
    if args.status:
        data["status"] = int(args.status)
    if args.title:
        data["title"] = args.title
    
    try:
        resp = requests.put(url, headers=get_headers(config), json=data)
        resp.raise_for_status()
        print(f"Task {args.id} updated successfully.")
    except Exception as e:
        print(f"Error updating task: {e}")

def main():
    parser = argparse.ArgumentParser(description="Aura CLI - Task & Project Manager")
    subparsers = parser.add_subparsers(dest="command")

    subparsers.add_parser("configure", help="Configure API URL manually")

    auth_parser = subparsers.add_parser("auth", help="Authentication")
    auth_sub = auth_parser.add_subparsers(dest="subcommand")
    auth_sub.add_parser("login", help="Login and create persistent session")

    p_parser = subparsers.add_parser("projects", help="Manage projects")
    p_sub = p_parser.add_subparsers(dest="subcommand")
    p_sub.add_parser("list", help="List projects")
    p_info = p_sub.add_parser("info", help="Show project details")
    p_info.add_argument("id", help="Project ID")

    t_parser = subparsers.add_parser("tokens", help="Manage API Tokens (Agent Keys)")
    t_sub = t_parser.add_subparsers(dest="subcommand")
    t_sub.add_parser("list", help="List your agent tokens")
    t_create = t_sub.add_parser("create", help="Create a new agent token for bot/agent")
    t_create.add_argument("name", help="Token name")
    t_create.add_argument("--project-id", help="Restrict token to specific project ID")

    tk_parser = subparsers.add_parser("tasks", help="Manage tasks")
    tk_sub = tk_parser.add_subparsers(dest="subcommand")
    tk_list = tk_sub.add_parser("list", help="List tasks")
    tk_list.add_argument("--project-id", help="Filter by project")
    tk_list.add_argument("--status", help="Filter by status ID")
    tk_create = tk_sub.add_parser("create", help="Create task")
    tk_create.add_argument("title", help="Task title")
    tk_create.add_argument("--description", help="Task description")
    tk_create.add_argument("--project-id", help="Project ID")
    tk_create.add_argument("--priority", default="medium")
    tk_create.add_argument("--status", help="Column/Status ID")
    tk_update = tk_sub.add_parser("update", help="Update task")
    tk_update.add_argument("id", help="Task ID")
    tk_update.add_argument("--status", help="New status ID")
    tk_update.add_argument("--title", help="New title")

    args = parser.parse_args()

    if args.command == "configure":
        cmd_configure(args)
    elif args.command == "auth":
        if args.subcommand == "login":
            cmd_auth_login(args)
        else:
            auth_parser.print_help()
    elif args.command == "projects":
        if args.subcommand == "list":
            cmd_projects_list(args)
        elif args.subcommand == "info":
            cmd_project_info(args)
        else:
            p_parser.print_help()
    elif args.command == "tokens":
        if args.subcommand == "list":
            cmd_tokens_list(args)
        elif args.subcommand == "create":
            cmd_tokens_create(args)
        else:
            t_parser.print_help()
    elif args.command == "tasks":
        if args.subcommand == "list":
            cmd_tasks_list(args)
        elif args.subcommand == "create":
            cmd_tasks_create(args)
        elif args.subcommand == "update":
            cmd_tasks_update(args)
        else:
            tk_parser.print_help()
    else:
        parser.print_help()

if __name__ == "__main__":
    main()