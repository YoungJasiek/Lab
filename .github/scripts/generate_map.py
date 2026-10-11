from pathlib import Path
from typing import TypedDict, Optional, Any, cast
from datetime import datetime, timezone
import json
import re
import yaml


class DocumentMetadata(TypedDict, total=False):
    docid: str
    name: str
    path: str
    category: str
    subcategory: str
    related_docs: list[str]
    tags: list[str]
    reserved: bool


class CategoryConfig(TypedDict, total=False):
    id: str
    name: str
    type: str
    parent: str
    scope: list[str]
    subcategories: list["CategoryConfig"]


class MapStats(TypedDict):
    total_documents: int
    total_categories: int


class MapStructure(TypedDict):
    version: str
    last_updated: str
    stats: MapStats
    categories: list[CategoryConfig]
    documents: list[DocumentMetadata]


root_dir: Path = (Path(__file__).parent / ".." / "..").resolve()
docs_dir: Path = root_dir / "docs"


def create_map_structure() -> MapStructure:
    return {
        "version": "1.0.0",
        "last_updated": "",
        "stats": {
            "total_documents": 0,
            "total_categories": 0
        },
        "categories": [],
        "documents": []
    }


def slugify(text: str) -> str:
    text = text.lower().strip()
    text = re.sub(r'[^\w\s-]', '', text)
    return re.sub(r'[\s_-]+', '-', text)


def get_project_version() -> str:
    package_json_path = root_dir / "package.json"
    if package_json_path.is_file():
        try:
            with open(package_json_path, encoding="utf-8") as f:
                data: dict[str, Any] = json.load(f)
                return str(data.get("version", "1.0.0"))
        except Exception:
            pass
    return "1.0.0"


def load_metadata(path: Path) -> DocumentMetadata:
    content = path.read_text(encoding="utf-8")
    if not content.startswith("---"):
        return {}

    parts = content.split("---", 2)
    if len(parts) < 3:
        return {}

    loaded = yaml.safe_load(parts[1])
    if isinstance(loaded, dict):
        return cast(DocumentMetadata, loaded)

    return {}


def open_yaml(path: Path) -> dict[str, Any]:
    with open(path, encoding="utf-8") as f:
        loaded = yaml.safe_load(f)
        return loaded if isinstance(loaded, dict) else {}


def get_category_config(directory: Path, is_sub: bool = False) -> Optional[CategoryConfig]:
    files = (
        ["subcategory.yaml", "subcategory.yml"]
        if is_sub
        else ["category.yaml", "category.yml"]
    )

    for filename in files:
        path = directory / filename
        if path.is_file():
            data = open_yaml(path)
            if data:
                return cast(CategoryConfig, data)

    return None


def find_category_node(
    nodes: list[CategoryConfig],
    target_id: str
) -> Optional[CategoryConfig]:
    for node in nodes:
        if node.get("id") == target_id:
            return node
        if "subcategories" in node:
            found = find_category_node(node["subcategories"], target_id)
            if found:
                return found
    return None


def insert_category_node(
    root_categories: list[CategoryConfig],
    cat_data: CategoryConfig
) -> None:
    cat_type = cat_data.get("type", "category")
    cat_id = cat_data.get("id")

    if cat_type == "subcategory":
        parent_id = cat_data.get("parent")
        parent_node = (
            find_category_node(root_categories, parent_id)
            if parent_id
            else None
        )

        if parent_node:
            parent_node.setdefault("subcategories", []).append(cat_data)
        else:
            root_categories.append(cat_data)
    else:
        existing = (
            find_category_node(root_categories, cat_id)
            if cat_id
            else None
        )
        if existing:
            existing.update(cat_data)
        else:
            root_categories.append(cat_data)


def process_docs(
    current_dir: Path,
    map_structure: MapStructure,
    current_cat_id: Optional[str] = None,
    current_subcat_id: Optional[str] = None
) -> None:
    for item in sorted(current_dir.iterdir()):
        # Skip backup and external reference folders
        if item.name in ["th_docs", "backup_legacy_docs"]:
            continue

        if item.is_dir():
            cat_data = get_category_config(item)
            subcat_data = get_category_config(item, True)

            if cat_data:
                insert_category_node(map_structure["categories"], cat_data)
                process_docs(
                    item,
                    map_structure,
                    cat_data.get("id"),
                    current_subcat_id
                )
            elif subcat_data:
                subcat_data["type"] = "subcategory"
                if "id" not in subcat_data:
                    name = subcat_data.get("name", item.name)
                    subcat_data["id"] = f"subcat-{slugify(name)}"

                if current_cat_id and "parent" not in subcat_data:
                    subcat_data["parent"] = current_cat_id

                insert_category_node(map_structure["categories"], subcat_data)
                process_docs(
                    item,
                    map_structure,
                    current_cat_id,
                    subcat_data.get("id")
                )
            else:
                process_docs(
                    item,
                    map_structure,
                    current_cat_id,
                    current_subcat_id
                )
        elif item.is_file() and item.suffix == ".md":
            # Skip root README or other non-indexed markdown if needed
            if item.name == "README.md" and item.parent == docs_dir:
                continue

            doc_meta = load_metadata(item)
            if doc_meta:
                doc_meta["path"] = str(
                    item.relative_to(docs_dir)
                ).replace("\\", "/")

                if current_cat_id:
                    doc_meta.setdefault("category", current_cat_id)
                if current_subcat_id:
                    doc_meta.setdefault("subcategory", current_subcat_id)

                if not doc_meta.get("reserved"):
                    doc_meta.pop("reserved", None)

                map_structure["documents"].append(doc_meta)


def generate_map(
    output_file: Optional[Path] = None
) -> MapStructure:
    map_structure = create_map_structure()
    process_docs(docs_dir, map_structure)

    map_structure["documents"].sort(
        key=lambda d: str(d.get("docid", ""))
    )
    map_structure["version"] = get_project_version()
    map_structure["last_updated"] = datetime.now(timezone.utc).isoformat()
    map_structure["stats"] = {
        "total_documents": len(map_structure["documents"]),
        "total_categories": len(map_structure["categories"])
    }

    target = output_file if output_file else root_dir / "map.yaml"
    with open(target, "w", encoding="utf-8") as f:
        yaml.dump(
            map_structure,
            f,
            allow_unicode=True,
            sort_keys=False
        )

    return map_structure


if __name__ == "__main__":
    output = root_dir / "map.yaml"
    generate_map(output)
    print(f"Map generated and saved to: {output}")
