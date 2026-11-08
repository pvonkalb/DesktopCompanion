"""Where the companion keeps private files (outside the repository)."""

from __future__ import annotations

import os
from pathlib import Path

APP_DIR_NAME = "DeskCompanion"
HERE = Path(__file__).resolve().parent


def data_dir() -> Path:
    """%LOCALAPPDATA%\\DeskCompanion - token, log, optional credentials/config."""
    base = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData" / "Local"))
    d = base / APP_DIR_NAME
    d.mkdir(parents=True, exist_ok=True)
    return d


def client_secrets_file() -> Path | None:
    """The Google OAuth client file: AppData first, then next to the code (gitignored)."""
    for candidate in (data_dir() / "credentials.json", HERE / "credentials.json"):
        if candidate.exists():
            return candidate
    return None


def config_file() -> Path | None:
    for candidate in (data_dir() / "config.json", HERE / "config.json"):
        if candidate.exists():
            return candidate
    return None
