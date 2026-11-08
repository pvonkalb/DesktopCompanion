"""Google Calendar (read-only) -> device event list.

Auth: standard installed-app OAuth. The first run opens a browser to sign in;
the token is cached in %LOCALAPPDATA%\\DeskCompanion\\token.json (never in the repo).
Recurring events are expanded by Google (singleEvents=True).
"""

from __future__ import annotations

import datetime as dt
import logging
from pathlib import Path

from google.auth.exceptions import RefreshError
from google.auth.transport.requests import Request
from google.oauth2.credentials import Credentials
from google_auth_oauthlib.flow import InstalledAppFlow
from googleapiclient.discovery import build

import paths
from protocol import device_text

log = logging.getLogger("calendar")

SCOPES = ["https://www.googleapis.com/auth/calendar.readonly"]


class SignInRequired(Exception):
    """Interactive Google sign-in did not complete (abandoned, blocked or timed out)."""
MAX_CALENDARS = 4    # device palette size
MAX_EVENTS = 24      # device buffer size


def _credentials() -> Credentials:
    token_file = paths.data_dir() / "token.json"
    creds = None
    if token_file.exists():
        creds = Credentials.from_authorized_user_file(str(token_file), SCOPES)
    if creds and creds.valid:
        return creds
    if creds and creds.expired and creds.refresh_token:
        try:
            creds.refresh(Request())
        except RefreshError:
            log.warning("saved Google sign-in expired or was revoked - signing in again")
            creds = None
    if not creds or not creds.valid:
        secrets = paths.client_secrets_file()
        if secrets is None:
            raise FileNotFoundError(
                "credentials.json not found. Put the OAuth client file in "
                f"{paths.data_dir()} (see windows-companion/README.md).")
        flow = InstalledAppFlow.from_client_secrets_file(str(secrets), SCOPES)
        log.info("opening the browser for Google sign-in (waiting up to 5 minutes)")
        # Bounded wait: an abandoned or blocked sign-in must not hang the companion
        # (the local redirect server ignores Ctrl+C on Windows while it waits).
        try:
            creds = flow.run_local_server(port=0, prompt="consent", timeout_seconds=300,
                                          success_message="Desk Companion is signed in. You can close this tab.")
        except Exception as e:   # timed out, denied, blocked ("access_denied"), closed tab...
            raise SignInRequired(f"Google sign-in was not completed: {e}") from e
        if not creds or not getattr(creds, "valid", False):
            raise SignInRequired("Google sign-in was not completed")
    token_file.write_text(creds.to_json(), encoding="utf-8")
    return creds


def _epoch(when: dict) -> tuple[int, bool]:
    """Google start/end object -> (unix seconds, all_day). All-day dates map to local midnight."""
    if "dateTime" in when:
        return int(dt.datetime.fromisoformat(when["dateTime"]).timestamp()), False
    day = dt.date.fromisoformat(when["date"])
    return int(dt.datetime.combine(day, dt.time()).astimezone().timestamp()), True


def _declined(event: dict) -> bool:
    return any(a.get("self") and a.get("responseStatus") == "declined" for a in event.get("attendees", []))


class GoogleCalendarSource:
    def __init__(self, calendars: list[str] | None = None, days: int = 14):
        self._wanted = [c.lower() for c in (calendars or [])]
        self._days = days
        self._service = None

    def reset_auth(self) -> None:
        self._service = None

    def _api(self):
        if self._service is None:
            self._service = build("calendar", "v3", credentials=_credentials(), cache_discovery=False)
        return self._service

    def _chosen_calendars(self) -> list[dict]:
        items = self._api().calendarList().list(minAccessRole="reader").execute().get("items", [])
        if self._wanted:   # by id or display name, in the configured order
            picked = []
            for want in self._wanted:
                picked += [c for c in items
                           if want in (c["id"].lower(), c.get("summaryOverride", c.get("summary", "")).lower())
                           or (want == "primary" and c.get("primary"))]
        else:              # default: what is ticked in Google Calendar, primary first
            picked = [c for c in items if c.get("selected")]
            picked.sort(key=lambda c: not c.get("primary", False))
        return picked[:MAX_CALENDARS]

    def snapshot(self) -> dict:
        """Build the `cal` message payload (without t/s)."""
        start = dt.datetime.now().astimezone().replace(hour=0, minute=0, second=0, microsecond=0)
        end = start + dt.timedelta(days=self._days)
        calendars = self._chosen_calendars()
        events = []
        for index, cal in enumerate(calendars):
            result = self._api().events().list(
                calendarId=cal["id"], timeMin=start.isoformat(), timeMax=end.isoformat(),
                singleEvents=True, orderBy="startTime", maxResults=100,
            ).execute()
            for e in result.get("items", []):
                if e.get("status") == "cancelled" or _declined(e) or "start" not in e:
                    continue
                st, all_day = _epoch(e["start"])
                en, _ = _epoch(e.get("end", e["start"]))
                events.append({"ti": device_text(e.get("summary", "(No title)")), "st": st, "en": max(en, st),
                               "ad": 1 if all_day else 0, "c": index})
        events.sort(key=lambda ev: (ev["st"], ev["c"]))
        names = [device_text(c.get("summaryOverride", c.get("summary", "")), 24) for c in calendars]
        log.info("google: %d events from %s", len(events), ", ".join(names) or "no calendars")
        return {
            "tz": str(start.tzinfo),
            "cals": [{"i": i, "n": n} for i, n in enumerate(names)],
            "ev": events[:MAX_EVENTS],
        }


class DemoCalendarSource:
    """Sample week (relative to now) for testing the link without a Google account."""

    def snapshot(self) -> dict:
        now = dt.datetime.now().astimezone()
        midnight = now.replace(hour=0, minute=0, second=0, microsecond=0)

        def at(days: int, hour: int, minute: int = 0) -> int:
            return int((midnight + dt.timedelta(days=days, hours=hour, minutes=minute)).timestamp())

        def ev(title, start, minutes, cal, all_day=False):
            return {"ti": title, "st": start, "en": start + minutes * 60, "ad": int(all_day), "c": cal}

        t = int(now.timestamp())
        events = [
            ev("PC link test: coffee", t - 3 * 3600, 60, 0),
            ev("PC link test: dinner", t + 2 * 3600, 90, 0),
            ev("Farmers market", at(1, 9), 120, 1),
            ev("Pay rent", at(2, 0), 24 * 60, 0, all_day=True),
            ev("Yoga", at(3, 18), 60, 1),
        ]
        return {"tz": str(now.tzinfo), "cals": [{"i": 0, "n": "Demo"}, {"i": 1, "n": "Demo 2"}], "ev": events}
