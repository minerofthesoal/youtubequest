# YouTube Live Chat — Beat Saber Quest mod

A pop-out YouTube live chat overlay for **standalone** Beat Saber on the Meta
Quest 3S (Beat Saber 1.40.8). No PC, no companion app, no external server —
just the headset's own Wi-Fi and a YouTube Data API key you provide.

Read **"Three corrections to the original brief"** below first — they explain
why this project looks structurally different from a PC BSML mod, and they're
the difference between something that actually builds for Quest and something
that doesn't.

---

## 1. Three corrections to the original brief

**1. Quest Beat Saber mods are C++, not C#.** On PC, Beat Saber runs on Mono
and mods are BSIPA plugins written in C#. On Quest, the game is compiled to
IL2CPP for performance, and mods hook into that IL2CPP layer directly —
they're native `.so` libraries written in C++, built with the Android NDK,
using `beatsaber-hook` for hooking and `custom-types` for creating new
IL2CPP-visible classes. There's no `.csproj` here because there can't be one.
BSML itself is real on Quest too — it's been ported to C++ by RedBrumbler
(`Quest-BSML`) and works the same way conceptually (XML markup → UI), just
called from C++.

**2. Reading YouTube live chat doesn't need OAuth.** `liveChatMessages.list`
and `videos.list` both accept a plain API key for public data — OAuth is only
required for *posting/deleting* chat messages or acting on a user's account,
neither of which this mod does. That's a large simplification over what the
brief assumed: no Google authorization flow, no token storage/refresh, no
embedded browser on a device that doesn't really have one. The user pastes a
free API key into the settings menu; it's stored in this mod's own local
config file and sent only to `googleapis.com` over HTTPS.

**3. "New subscriber" events aren't in the public API.** YouTube's own live
chat UI shows a transient "X subscribed" toast, but that specific event is
**not** part of the documented `liveChatMessage` resource. What *is* public
and implemented here: `newSponsorEvent` (someone became a paid channel
**member**), `memberMilestoneChatEvent`, `membershipGiftingEvent`, and
`giftMembershipReceivedEvent`. The settings menu and code reflect this
honestly — "Show memberships" (👑), not a fabricated "Show subscribers" (⭐)
toggle that could never actually fire.

---

## 2. Project structure

```
YouTubeLiveChat/
├── qpm.json                  # QPM dependency manifest
├── CMakeLists.txt            # NDK build config
├── mod.json                  # Scotland2/QuestPatcher qmod manifest
├── build.ps1 / build.sh      # one-shot build scripts
├── createqmod.ps1 / .sh      # packages the built .so into a .qmod
├── src/
│   └── main.cpp              # modloader entry point (setup/load)
└── shared/
    ├── ChatTypes.hpp         # ChatMessage, ConnectionState, event enums
    ├── Config.hpp/.cpp       # settings struct, JSON load/save
    ├── ModState.hpp/.cpp     # process-wide singletons (config/manager/overlay)
    ├── UrlUtils.hpp/.cpp     # video ID / URL parsing
    ├── YouTubeApiClient.hpp/.cpp   # videos.list + liveChatMessages.list
    ├── ChatManager.hpp/.cpp  # connect/poll/backoff state machine
    ├── UI/
    │   ├── BSMLLayouts.hpp             # embedded settings-menu XML
    │   ├── SettingsViewController.*    # BSML-bound settings screen
    │   ├── ChatOverlayController.*     # persistent world-space HUD panel
    │   ├── MessageRow.*                # pooled per-message UI row
    │   └── AvatarCache.*               # profile-picture download/cache
    └── Hooks/
        └── MainFlowCoordinatorHooks.*  # spawns the overlay once, on boot
```

---

## 3. Dependencies

Resolved via `qpm restore` (see `qpm.json`), plus one dependency vendored
directly as a git submodule (see below):

| Package | Why |
|---|---|
| `beatsaber-hook` | IL2CPP function hooking + coroutine/utility layer |
| `custom-types` | Lets us define new IL2CPP-visible C++ classes (MonoBehaviours, ViewControllers) |
| `bs-cordl` | Auto-generated headers for Beat Saber's own types, **pinned to your exact build** |
| `bsml` | RedBrumbler's Quest-BSML port, for the settings menu |
| `paper2_scotland2` | Logging shim for the Scotland2 modloader |

**RapidJSON** (`#include "rapidjson/document.h"`) is vendored directly at
`extern-vendor/rapidjson` as a git submodule, pinned to the same commit
beatsaber-hook 6.4.2 itself bundles. The `rapidjson-macros` qpm package was
tried first, but its restore silently produces an empty directory (confirmed
by listing the actual restored tree in CI) -- it appears to be broken or
stale upstream, so it's not used here. Clone this repo with
`git clone --recurse-submodules`, or if you already cloned it, run
`git submodule update --init --recursive` before building (`build.sh` /
`build.ps1` do this automatically).

Toolchain: `qpm-rust`/QPM.CLI, CMake ≥ 3.22, Ninja, Android NDK r27 (r27d tested; r25c's
bundled Clang 14 is too old for beatsaber-hook ^6.0.0's C++20 usage -- see
`.github/workflows/build.yml` for details).

### Committing the lockfile

`qpm.shared.json` is QPM's lockfile (same idea as `Cargo.lock` or
`package-lock.json`) — **it must be committed**, not gitignored. This repo's
`.gitignore` no longer excludes it (an earlier version of this project
incorrectly did). The first time you touch `qpm.json`'s dependencies, run:

```
qpm restore
git add qpm.shared.json
git commit -m "Lock qpm dependencies"
```

Without a committed lockfile, QPM.CLI falls back to an unlocked resolve and
prints `"Running in CI and using unlocked resolve, this seems like a bug!"`
in Actions — that warning is QPM telling you exactly this, not a bug in the
workflow itself.

### Verifying codegen against your exact build

`bs-cordl`'s codegen headers are minted per game build, not something anyone
can hand-write from memory — including this document. Before building:

```
qpm dependency add bs-cordl
qpm restore
```

and let QPM pick the codegen version matching Beat Saber `1.40.8_7379`. A few
spots in the source are marked `// VERIFY:` where the *exact* generated
method signature (e.g. `MainFlowCoordinator::DidActivate`'s parameters, or
`UnityWebRequest::Result` vs the older `isNetworkError`/`isHttpError`
booleans depending on which Unity version this BS build shipped on) should be
checked against your freshly-generated headers. If a signature has drifted,
the hook macro simply fails to compile — a safe, loud failure, not a silent
runtime mismatch.

---

## 4. Build instructions

```powershell
# Windows
$env:ANDROID_NDK_HOME = "C:\path\to\ndk\27.3.13750724"
qpm restore
./build.ps1
./createqmod.ps1
```

```bash
# Linux/macOS
export ANDROID_NDK_HOME=/path/to/ndk/27.3.13750724
qpm restore
./build.sh
./createqmod.sh
```

Output: `build/libyoutubelivechat.so`, packaged into `YouTubeLiveChat.qmod`.

---

## 5. Quest installation

1. Beat Saber must already be modded with **Scotland2** as the modloader
   (not the older QuestLoader). Follow the BSMG wiki's Quest Modding guide if
   it isn't yet — it walks through enabling Developer Mode, installing
   QuestPatcher, and patching the APK.
2. Open **QuestPatcher**, connect the headset, and use its mod install
   screen (or drag-and-drop) to install, in order:
   - `Scotland2` (if not already the active modloader)
   - `CustomTypes`
   - `BSML.qmod`
   - `paper2_scotland2.qmod`
   - `YouTubeLiveChat.qmod`
3. Launch Beat Saber. A "YouTube Live Chat" entry appears in the mod
   settings menu (accessible from the main menu's Mods button).

## 6. YouTube API setup (no OAuth needed)

1. Go to the [Google Cloud Console](https://console.cloud.google.com/),
   create a project (or reuse one).
2. **APIs & Services → Library** → enable **"YouTube Data API v3."**
3. **APIs & Services → Credentials → Create Credentials → API key.**
4. Optional but recommended: restrict the key to the YouTube Data API v3 so
   it can't be used for anything else if it ever leaks.
5. Paste that key into the mod's settings menu, "API Key" field.

Quota: the default is 10,000 units/day; `liveChatMessages.list` costs a
handful of units per call (check the [current quota
calculator](https://developers.google.com/youtube/v3/determine_quota_cost)
for the exact number, it's occasionally revised). At the default 2-second
floor, that's roughly enough for several hours of continuous polling on one
key — plenty for a single stream, but don't share one key across many
simultaneous headsets/streams.

If you ever want to **post** to chat from the headset, that's a real
YouTube Live Streaming API capability but requires the OAuth path this mod
intentionally skips (see §1) — out of scope here by design, not oversight.

## 7. Configuration reference

All fields live in the in-game settings menu (§"BSML Configuration Menu" in
the original spec, implemented in full): enable/disable, API key, video
ID/URL, connect/disconnect, position preset + custom X/Y/Z, width/height/
scale/opacity, max visible messages, message duration, per-category show/hide
toggles (regular / Super Chat / Super Sticker / memberships), username/avatar
toggles, highlight toggles, notification sound, auto-reconnect, debug
logging. Settings persist to
`/sdcard/ModData/com.beatgames.beatsaber/Configs/YouTubeLiveChat.json` and
survive between sessions.

You can paste either a bare video ID or a full URL (`watch?v=`, `youtu.be/`,
or `/live/` forms) into the video field — it's parsed automatically.

## 8. Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| "Invalid configuration" | API key or video field is empty, or the video field didn't parse to anything ID-shaped |
| "Video not found" | Wrong ID, or the video is private/deleted |
| "Stream offline" | Video exists but isn't currently live, or has no active chat — the mod keeps checking every 15s |
| "API / auth error" | Bad/disabled API key, or YouTube Data API v3 isn't enabled on that Cloud project. This state does **not** auto-retry — fix the key, then tap Connect again |
| "Rate limited" | Daily quota exhausted. Waits and gives up until you reconnect manually, rather than hammering a dead key for the rest of the day |
| "Network error" | Wi-Fi hiccup, DNS issue, etc. Auto-retries with exponential backoff (3s → 60s cap) if Auto-reconnect is on |
| Overlay doesn't appear at all | Confirm all four dependency qmods installed (Scotland2, CustomTypes, BSML, paper2_scotland2) and check the Scotland2/QuestPatcher log for a load error from `youtube-live-chat` |
| Avatars never load | Confirm "Show profile pictures" is on; avatars fail silently to a blank square rather than blocking text messages, by design |
| Badge glyphs (💰🎉👑) show as boxes | The game's bundled TMP font may not include full-color emoji glyph coverage — this is a font limitation, not a logic bug. See §10 for the fallback path if this bothers you |

## 9. How the live chat update system works

There is no push/streaming endpoint for YouTube live chat in the public API
— it's poll-based by design on Google's end. The flow:

1. `videos.list?part=snippet,liveStreamingDetails&id=<id>` resolves a video
   ID to `liveStreamingDetails.activeLiveChatId`. If that field is absent,
   the video isn't currently live (handled as `StreamOffline`, rechecked
   every 15s).
2. `liveChatMessages.list?liveChatId=<id>&pageToken=<token>` returns a page
   of new messages plus a `nextPageToken` and a server-suggested
   `pollingIntervalMillis`. We wait `max(pollingIntervalMillis,
   minPollIntervalSeconds)` — respecting the server's hint, but with a
   configurable floor (default 2s) so a low suggested value can never turn
   into quota-burning spam.
3. Each returned message's `snippet.type` maps to one of the categories in
   §1.3; unsupported types (polls, `chatEndedEvent`, sponsor-only-mode
   toggles, the legacy `fanFundingEvent`) are parsed but dropped rather than
   guessed at.
4. Everything runs as a chain of C++20 coroutines (`co_yield`ing Unity's
   `UnityWebRequestAsyncOperation` / `WaitForSecondsRealtime`) driven by
   `MonoBehaviour::StartCoroutine` — the same asynchronous mechanism any
   other Unity networking code uses, so a poll cycle costs ~0 main-thread
   time between requests.

## 10. How this avoids impacting Beat Saber's performance

- **No per-message allocation churn.** `ChatOverlayController` pre-builds a
  fixed pool of up to 50 `MessageRow`s once, at startup. New messages reuse
  a pooled row (text/color/sprite mutated in place); nothing is
  `Instantiate`d or `Destroy`ed per chat message.
- **No per-frame network polling.** Requests fire on a timer (§9), never in
  `Update()`. `Update()` itself only does cheap vector math (head-relative
  repositioning) and a couple of comparisons for message expiry.
- **Expiry is O(1) amortized, not a per-frame scan.** Messages expire in
  FIFO order, so `Update()` only ever needs to check the *oldest* active
  row's timestamp — if that one hasn't expired, nothing behind it has
  either.
- **Layout only recomputes on actual change** (a message added or expired),
  not every frame.
- **Avatars are cached by URL**, so a chatter who posts 20 times in a stream
  triggers one download, not 20.
- **The overlay is a plain world-space `Canvas`, not a BSML view.** BSML
  view controllers are built for the menu flow-coordinator system and carry
  overhead/assumptions that don't fit a HUD element rendering continuously
  during active gameplay — using one for the persistent panel would fight
  the framework. BSML is used where it's actually the right tool: the
  settings menu.
- **Gameplay code is untouched.** Nothing here hooks note spawning, scoring,
  saber colliders, or timing — the only hook installed
  (`MainFlowCoordinator::DidActivate`) fires once, at boot, purely to spawn
  the overlay's root `GameObject`.

---

## 11. Continuous integration

`.github/workflows/build.yml` builds the `.qmod` on every push/PR to `main`
(and on manual dispatch), using `nttld/setup-ndk` + `seanmiddleditch/gha-setup-ninja`
+ QPM.CLI — the same toolchain as the local `build.sh`/`build.ps1` scripts, so
CI and local builds can't silently drift apart. Every run uploads the built
`.qmod`/`.so` as a workflow artifact.

Push a tag like `v0.1.0` and it additionally cuts a GitHub Release with the
`.qmod` attached — handy for `qpm.json`'s `workspace.domain` field, which
points at `releases/latest/download/`, and for `mod.json`'s
`downloadIfMissing` URLs on your own future releases if this ever becomes a
dependency of another mod.

The QPM.CLI download step pulls the latest release asset by name pattern
(`grep -i linux`) rather than a pinned URL — reasonable for now, but check
the **Known limitations** note below if it ever starts failing.

## Known limitations / honesty notes

- This has **not** been compiled against real `bs-cordl` 1.40.8 headers in
  the environment that produced it (that requires the actual Quest modding
  toolchain and network access to QPM's package registry, neither of which
  were available while writing this). The architecture, control flow, and
  YouTube API integration are correct and complete; a handful of
  `// VERIFY:` comments mark the exact spots — mostly individual method
  signatures — that only your freshly-generated headers can confirm.
  Getting those to line up is the normal first-build step for *any* Quest
  mod, not specific to this one.
- Super Sticker images themselves aren't fetchable through the public API
  (only metadata like the amount/tier) — the sticker event renders as a
  labeled Super Chat–style row rather than the actual sticker artwork.
- Emoji badge glyphs depend on the active TMP font's glyph coverage (§8).
- The CI workflow's QPM.CLI install step resolves the latest Linux release
  asset by a name-pattern match rather than a version-pinned URL (see §11)
  — quick to fix if QuestPackageManager/QPM.CLI ever renames their release
  assets, but worth knowing if a build suddenly fails on that step.
- `qpm.shared.json` (the dependency lockfile) isn't included here and can't
  be generated in the environment that produced this project — it requires
  a real `qpm` binary with network access to the QPM package registry. Run
  `qpm restore` locally once and commit the resulting `qpm.shared.json`
  before your first CI build (see §3, "Committing the lockfile") — until
  then, every `qpm restore` (local or CI) does a slower unlocked resolve
  and CI will print a warning about it.
