import os
import json
import requests
import sys
from typing import Optional, List, Dict, Any
from mcp.server.fastmcp import FastMCP
from dotenv import load_dotenv

# Load environment variables
load_dotenv()

# Configuration setup
def load_config():
    if sys.platform == "win32":
        config_dir = os.path.join(os.environ.get("APPDATA", os.getcwd()), ".aura_cli_config")
    else:
        config_dir = os.path.join(os.path.expanduser("~"), ".aura_cli_config")

    config_file = os.path.join(config_dir, "config.json")
    
    config = {
        "api_url": os.environ.get("AURA_API_URL", "https://cloud.auraboard.online/api"),
        "token": os.environ.get("AURA_TOKEN", "")
    }
    
    if os.path.exists(config_file):
        try:
            with open(config_file, "r") as f:
                file_config = json.load(f)
                config.update(file_config)
        except Exception as e:
            print(f"Warning: Could not read config file: {e}", file=sys.stderr)
    return config

config = load_config()
mcp = FastMCP("Aura Secure Agent")

def get_headers():
    if not config["token"]:
        raise ValueError("No AURA_TOKEN found. Please login via 'aura_cli' or set AURA_TOKEN env var.")
    return {
        "Authorization": f"Bearer {config['token']}",
        "Content-Type": "application/json"
    }

def get_url(path: str):
    return f"{config['api_url'].rstrip('/')}/{path.lstrip('/')}"

# --- PROJECTS ---

@mcp.tool()
def list_projects() -> List[Dict[str, Any]]:
    """List all accessible projects with their names and secure UIDs."""
    resp = requests.get(get_url("projects"), headers=get_headers())
    resp.raise_for_status()
    return resp.json()

@mcp.tool()
def get_project_details(project_uid: str) -> Dict[str, Any]:
    """Get full details of a project using its secure UID."""
    resp = requests.get(get_url(f"projects/{project_uid}"), headers=get_headers())
    resp.raise_for_status()
    return resp.json()

# --- COLUMNS ---

@mcp.tool()
def list_columns(project_uid: str) -> List[Dict[str, Any]]:
    """List all columns in a project. Requires project_uid for security."""
    # Find numeric ID first for this project as backend columns use project_id (int) in some endpoints
    # But we use the new GET /columns/{project_id}
    # For simplicity, we assume the backend handles the mapping or we fetch project first
    proj = get_project_details(project_uid)
    project_id = proj['id']
    resp = requests.get(get_url(f"columns/{project_id}"), headers=get_headers())
    resp.raise_for_status()
    return resp.json()

# --- TASKS ---

@mcp.tool()
def list_tasks(project_uid: str, status_uid: Optional[str] = None) -> List[Dict[str, Any]]:
    """List tasks for a project. status_uid can be used for filtering."""
    proj = get_project_details(project_uid)
    project_id = proj['id']
    url = get_url(f"projects/{project_id}/tasks")
    
    resp = requests.get(url, headers=get_headers())
    resp.raise_for_status()
    tasks = resp.json()
    if status_uid:
        # In the new logic, tasks also have uid. We filter by column uid if needed
        # status in TaskResponse is currently the integer ID, but we can filter by the column uid
        cols = list_columns(project_uid)
        col_id = next((c['id'] for c in cols if c['uid'] == status_uid), None)
        if col_id:
            tasks = [t for t in tasks if t.get("status") == col_id]
    return tasks

@mcp.tool()
def create_task(
    title: str, 
    project_uid: str, 
    description: Optional[str] = "", 
    column_uid: Optional[str] = None, 
    priority: str = "medium"
) -> Dict[str, Any]:
    """Create a new task in a project using secure UIDs."""
    proj = get_project_details(project_uid)
    project_id = proj['id']
    
    data = {
        "title": title,
        "description": description,
        "priority": priority
    }
    
    if column_uid:
        cols = list_columns(project_uid)
        col_id = next((c['id'] for c in cols if c['uid'] == column_uid), None)
        if col_id:
            data["status"] = col_id
            
    resp = requests.post(get_url(f"projects/{project_id}/tasks"), headers=get_headers(), json=data)
    resp.raise_for_status()
    return resp.json()

@mcp.tool()
def update_column(
    project_uid: str,
    column_uid: str,
    title: Optional[str] = None,
    color: Optional[str] = None,
    position: Optional[int] = None
) -> Dict[str, Any]:
    """Update a column's properties securely."""
    proj = get_project_details(project_uid)
    project_id = proj['id']
    
    data = {}
    if title: data["title"] = title
    if color: data["color"] = color
    if position is not None: data["position"] = position
    
    resp = requests.put(get_url(f"projects/{project_id}/columns/{column_uid}"), headers=get_headers(), json=data)
    resp.raise_for_status()
    return resp.json()

if __name__ == "__main__":
    mcp.run()
