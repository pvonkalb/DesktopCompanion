# Windows companion

Sends your Google Calendar to the Desk Companion over USB. Later it will also carry PC
stats and media controls.

## Install

```
cd windows-companion
python -m venv .venv
.venv\Scripts\pip install -r requirements.txt
```

## Try it without Google

```
.venv\Scripts\python main.py --demo -v
```
The device's Up Next card should show "PC link test" events within a few seconds.

## One-time Google setup (about 10 minutes)

The companion reads your calendar with Google's read-only permission. Google requires
every app to have its own "OAuth client", which you create for free in your account:

1. Open <https://console.cloud.google.com/>, then create a project named **Desk Companion**.
2. **APIs & Services → Library →** search **Google Calendar API → Enable**.
3. **Google Auth Platform → Get started** (OAuth consent screen):
   - App name **Desk Companion**, support email = your Gmail.
   - Audience **External**.
   - Contact email = your Gmail. Finish.
4. **Audience → Test users → Add users →** your Gmail.
5. **Clients → Create client →** type **Desktop app**, name **Desk Companion PC**.
   **Download JSON**.
6. Save that file as
   `%LOCALAPPDATA%\DeskCompanion\credentials.json`
   (paste that path into Explorer's address bar; the folder exists after the first run).
7. Run `.venv\Scripts\python main.py`. A browser opens. Sign in, then allow
   "See and download any calendar you can access". Google warns that the app is
   unverified because it's your own private app: click **Continue**.

**Stay signed in:** while the app is in *Testing*, Google expires the sign-in every
7 days. To avoid that, go to **Audience → Publish app**. Your app stays private; only you
can use it, and the "unverified" notice stays.

## Which calendars

By default the companion sends every calendar that's **ticked in Google Calendar**
(yours first, then shared ones like a partner's), up to 4. To choose explicitly, create
`%LOCALAPPDATA%\DeskCompanion\config.json`:

```json
{ "calendars": ["primary", "Family"], "days": 14, "sync_minutes": 5 }
```
Entries match a calendar's ID or its name.

## Where things live

| File | Location | In git? |
|---|---|---|
| OAuth client (`credentials.json`) | `%LOCALAPPDATA%\DeskCompanion\` | Never |
| Sign-in token (`token.json`) | `%LOCALAPPDATA%\DeskCompanion\` | Never |
| Log (incl. device debug output) | `%LOCALAPPDATA%\DeskCompanion\companion.log` | No |

To revoke access: <https://myaccount.google.com/permissions>, then remove **Desk Companion**.

## Notes

- The port is opened without toggling DTR/RTS. On this board those lines reset the ESP32.
- Only one program can use the COM port. Close any serial monitor while the companion runs,
  and stop the companion before flashing firmware.
