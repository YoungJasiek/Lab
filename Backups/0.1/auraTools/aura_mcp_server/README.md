# Aura MCP Server

Pełnoprawny serwer MCP (Model Context Protocol) dla ekosystemu Aura. Pozwala modelom LLM na bezpośrednie zarządzanie Twoimi projektami, zadaniami oraz korzystanie z wbudowanego asystenta AI Aury.

## Funkcje

- **Zarządzanie Projektami**: Listowanie projektów i pobieranie szczegółów.
- **Zarządzanie Zadaniami**: Pobieranie, tworzenie i aktualizacja statusów zadań.
- **Aura AI Assistant**: Dostęp do zaawansowanego asystenta AI z kontekstem projektu i możliwością wykonywania akcji (np. "stwórz zadanie na podstawie naszej rozmowy").

## Instalacja

1. Przejdź do katalogu:
   ```bash
   cd aura_mcp_server
   ```
2. Zainstaluj zależności:
   ```bash
   pip install -r requirements.txt
   ```

## Konfiguracja

Serwer automatycznie próbuje odczytać konfigurację z:
1. Pliku konfiguracyjnego Aura CLI (`.aura_cli_config/config.json`).
2. Zmiennych środowiskowych `AURA_API_URL` oraz `AURA_TOKEN`.
3. Pliku `.env` w katalogu serwera.

## Użycie z Claude Desktop

Dodaj poniższą konfigurację do swojego pliku `claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "aura": {
      "command": "python",
      "args": [
        "C:/Users/adam/Downloads/Project Aura/aura/aura_mcp_server/server.py"
      ],
      "env": {
        "AURA_TOKEN": "TWÓJ_TOKEN_AGENTA"
      }
    }
  }
}
```

## Narzędzia (Tools)

- `list_projects()` - Pobiera listę Twoich projektów.
- `get_project_details(project_id)` - Szczegółowe informacje o projekcie.
- `list_tasks(project_id, status_id)` - Lista zadań z filtrowaniem.
- `create_task(title, project_id, ...)` - Tworzenie nowego zadania.
- `update_task(task_id, ...)` - Aktualizacja zadania.
- `aura_ai_assistant(prompt, project_id)` - Rozmowa z AI Aury (obsługuje akcje w bazie danych!).
