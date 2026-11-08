"""Now-playing info, transport controls and master volume.

Anti-cheat safe by design: uses Windows' own media-session API (the same one the
volume flyout uses) and Core Audio - no simulated key presses, no hooks, no drivers.
Runs its own asyncio loop on a worker thread because the WinRT calls are async.
"""

from __future__ import annotations

import asyncio
import logging
import threading
import time
from collections.abc import Callable

from protocol import device_text

log = logging.getLogger("media")

POLL_S = 1.0
RESEND_S = 15.0        # re-send unchanged state now and then (device may have rebooted)
VOLUME_STEP = 0.04     # 4 % per tap

# Windows.Media.Control playback status enum
_PLAYING, _PAUSED = 4, 5


def _app_name(app_id: str) -> str:
    """'Spotify.exe' -> 'Spotify', 'Microsoft.ZuneMusic_8wek...!Microsoft.ZuneMusic' -> 'ZuneMusic'."""
    name = app_id.split("!")[-1]
    if name.lower().endswith(".exe"):
        name = name[:-4]
    name = name.split(".")[-1] if "." in name else name
    return {"chrome": "Chrome", "msedge": "Edge", "firefox": "Firefox", "brave": "Brave",
            "spotify": "Spotify", "zunemusic": "Media Player"}.get(name.lower(), name)


class MediaController:
    def __init__(self, on_state: Callable[[dict], None]):
        self._on_state = on_state
        self._loop: asyncio.AbstractEventLoop | None = None
        self._ready = threading.Event()
        self._last: dict | None = None
        self._last_sent = 0.0
        self._endpoint = None
        threading.Thread(target=self._thread, name="media", daemon=True).start()

    # ---- public (any thread)
    def command(self, action: str) -> bool:
        """playpause | next | prev | vol_up | vol_down | mute. Returns False if unknown."""
        if action not in ("playpause", "next", "prev", "vol_up", "vol_down", "mute"):
            return False
        if self._ready.wait(2) and self._loop:
            asyncio.run_coroutine_threadsafe(self._command(action), self._loop)
        return True

    def resend(self) -> None:
        self._last_sent = 0.0

    # ---- worker thread
    def _thread(self) -> None:
        try:
            import comtypes
            comtypes.CoInitialize()   # Core Audio is COM; initialise it on this thread
        except Exception as e:
            log.warning("COM init failed (volume unavailable): %s", e)
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)
        self._ready.set()
        self._loop.run_until_complete(self._poll_forever())

    def _volume(self):
        if self._endpoint is None:
            from pycaw.pycaw import AudioUtilities
            self._endpoint = AudioUtilities.GetSpeakers().EndpointVolume
        return self._endpoint

    async def _session(self):
        from winrt.windows.media.control import GlobalSystemMediaTransportControlsSessionManager as Manager
        manager = await Manager.request_async()
        return manager.get_current_session()

    async def _read(self) -> dict:
        state = {"app": "", "ti": "", "ar": "", "st": 0, "vol": -1, "mu": 0}
        try:
            session = await self._session()
            if session:
                props = await session.try_get_media_properties_async()
                status = session.get_playback_info().playback_status
                state.update(app=device_text(_app_name(session.source_app_user_model_id or ""), 23),
                             ti=device_text(props.title or "", 63), ar=device_text(props.artist or "", 47),
                             st=1 if status == _PLAYING else 2 if status == _PAUSED else 0)
        except Exception as e:
            log.debug("media session read failed: %s", e)
        try:
            ep = self._volume()
            state["vol"] = round(ep.GetMasterVolumeLevelScalar() * 100)
            state["mu"] = 1 if ep.GetMute() else 0
        except Exception as e:
            self._endpoint = None   # output device changed (e.g. headset unplugged): re-acquire next time
            log.debug("volume read failed: %s", e)
        return state

    async def _poll_forever(self) -> None:
        while True:
            state = await self._read()
            now = time.monotonic()
            if state != self._last or now - self._last_sent > RESEND_S:
                self._last, self._last_sent = state, now
                try:
                    self._on_state(state)
                except Exception:
                    log.exception("media state handler failed")
            await asyncio.sleep(POLL_S)

    async def _command(self, action: str) -> None:
        try:
            if action in ("vol_up", "vol_down", "mute"):
                ep = self._volume()
                if action == "mute":
                    ep.SetMute(0 if ep.GetMute() else 1, None)
                else:
                    level = ep.GetMasterVolumeLevelScalar() + (VOLUME_STEP if action == "vol_up" else -VOLUME_STEP)
                    ep.SetMasterVolumeLevelScalar(min(1.0, max(0.0, level)), None)
                    if action == "vol_up" and ep.GetMute():
                        ep.SetMute(0, None)
            else:
                session = await self._session()
                if session:
                    await {"playpause": session.try_toggle_play_pause_async,
                           "next": session.try_skip_next_async,
                           "prev": session.try_skip_previous_async}[action]()
            log.info("media command: %s", action)
        except Exception as e:
            self._endpoint = None
            log.warning("media command %s failed: %s", action, e)
        await asyncio.sleep(0.15)   # let Windows apply it, then report the new state at once
        self._last_sent = 0.0
