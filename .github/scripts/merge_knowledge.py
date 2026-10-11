from pathlib import Path
import yaml


ROOT_DIR: Path = (Path(__file__).parent / ".." / "..").resolve()
DOCS_DIR: Path = ROOT_DIR / "docs"
MAP_FILE: Path = ROOT_DIR / "map.yaml"
OUTPUT_FILE: Path = ROOT_DIR / "knowledge_base_bundle.txt"


def load_map() -> dict:
    with open(MAP_FILE, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def merge_files():
    if not MAP_FILE.exists():
        print("map.yaml not found")
        return

    data = load_map()
    documents = data.get("documents", [])

    with open(OUTPUT_FILE, "w", encoding="utf-8") as outfile:
        # Header / Map
        outfile.write("--- START FILE: [MAP] ---\n\n")
        outfile.write(MAP_FILE.read_text(encoding="utf-8"))
        outfile.write("\n\n--- END FILE: [MAP] ---\n")

        # All docs
        for doc in documents:
            doc_id = doc.get("docid", "UNKNOWN")
            path = doc.get("path", "")
            category = doc.get("category", "")
            subcategory = doc.get("subcategory", "")
            full_path = DOCS_DIR / path

            outfile.write(f"\n--- START FILE: [{doc_id}] ---\n")
            outfile.write(f"Category: {category}\n")
            if subcategory:
                outfile.write(f"Subcategory: {subcategory}\n")
            outfile.write(f"Path: {path}\n\n")

            if full_path.exists():
                outfile.write(full_path.read_text(encoding="utf-8"))
            else:
                outfile.write(f"ERROR: File {path} not found")

            outfile.write(f"\n--- END FILE: [{doc_id}] ---\n")

    print(f"Merged into: {OUTPUT_FILE}")


if __name__ == "__main__":
    merge_files()
