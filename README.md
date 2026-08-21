# YouTube Live Chat — Beat Saber Quest mod

A YouTube live chat overlay for **standalone** Beat Saber on Quest. No PC, no
companion app, no external server — just the headset's Wi-Fi and either your
own YouTube sign-in or a YouTube Data API key.

- Chat is visible **in the menu and during levels**, not just in the menu.
- **Sign in with your YouTube account** (OAuth device flow) and the mod finds
  your active broadcast by itself — no video ID to type. Or paste a plain API
  key and watch any public stream's chat.
- Pause the song and you get an **in-level control panel** (show/hide,
  reconnect, disconnect, clear) without leaving the level.

---

## 1. Three things worth knowing up front

**1. Quest Beat Saber mods are C++, not C#.** On PC, Beat Saber runs on Mono
and mods are BSIPA plugins written in C#. On Quest, the game is compiled to
IL2CPP, and mods hook into that IL2CPP layer directly — they're native `.so`
libraries written in C++, built with the Android NDK, using `beatsaber-hook`
for hooking and `custom-types` for creating new IL2CPP-visible classes.
There's no `.csproj` here because there can't be one. BSML is real on Quest
too (`bsq-ports/Quest-BSML`), and this mod uses its C++ "BSML-Lite" control
factories rather than XML markup.

**2. Reading chat works with just an API key; signing in does more.**
`liveChatMessages.list` and `videos.list` both accept a plain API key for
public data. But an API key is anonymous, which means it cannot answer "what
is this account streaming right now" and cannot read your own members-only
chat. Signing in with OAuth unlocks both, so the mod supports both modes and
you pick per your use case (§6).

**3. "New subscriber" events aren't in the public API.** YouTube's own live
chat UI shows a transient "X subscribed" toast, but that specific event is
**not** part of the documented `liveChatMessage` resource. What *is* public
and implemented here: `newSponsorEvent` (someone became a paid channel
**member**), `memberMilestoneChatEvent`, `membershipGiftingEvent`, and
`giftMembershipReceivedEvent`. The settings menu says "Memberships", not a
fabricated "Subscribers" toggle that could never actually fire.

---

## 2. Project structure

```
youtubequest/
├── qpm.json                  # QPM dependency manifest
├── mod.template.json         # qmod manifest template (mod.json is generated)
├── CMakeLists.txt            # NDK build config
├── build.ps1 / build.sh      # one-shot build scripts
├── createqmod.ps1 / .sh      # thin wrappers around scripts/package-qmod.sh
├── scripts/package-qmod.sh   # renders mod.json, checks it, zips the .qmod
├── shared/                   # headers
│   ├── Logging.hpp           # the one paper2 logger context
│   ├── ChatTypes.hpp         # ChatMessage, ConnectionState, event enums
│   ├── Config.hpp            # settings struct
│   ├── ModState.hpp          # process-wide singletons + callback fan-out
│   ├── UrlUtils.hpp          # video ID / URL parsing, percent-encoding
│   ├── Http.hpp              # System.Net.Http request coroutine
│   ├── YouTubeAuth.hpp       # OAuth 2.0 device flow
│   ├── YouTubeApiClient.hpp  # videos / liveBroadcasts / liveChatMessages
│   ├── ChatManager.hpp       # connect/poll/backoff/sign-in state machine
│   ├── UI/
│   │   ├── ChatOverlayController.hpp  # persistent FloatingScreen overlay
│   │   ├── MessageRow.hpp             # pooled per-message UI row
│   │   ├── AvatarCache.hpp            # profile-picture download/cache
│   │   ├── SettingsViewController.hpp # Mods > YouTube Live Chat screen
│   │   └── InLevelPanel.hpp           # pause-menu control panel
│   └── Hooks/Hooks.hpp
└── src/                      # implementation, mirrors shared/
    └── main.cpp              # modloader entry point (setup / late_load)
```

Sources live in `src/` and headers in `shared/`, and CMake globs `src/` only.
An earlier layout had every `.cpp` duplicated between `shared/` and
`shared/UI`, with the glob picking up both copies — two subtly different
versions of each translation unit in one library.

---

## 3. Dependencies

Resolved via `qpm restore` (see `qpm.json`):

| Package | Why |
|---|---|
| `beatsaber-hook` | IL2CPP function hooking, `SafePtr`, `StringW`, utilities |
| `custom-types` | Defining new IL2CPP-visible C++ classes (MonoBehaviours, ViewControllers) and C++20 coroutines Unity can run |
| `bs-cordl` | Generated headers for Beat Saber's and Unity's own types |
| `bsml` | Quest-BSML: settings-menu registration, BSML-Lite control factories, `FloatingScreen` |
| `paper2_scotland2` | Logging shim for the Scotland2 modloader |
| `scotland2` | Modloader headers (`setup` / `late_load` entry points) |

**RapidJSON** (`#include "rapidjson/document.h"`) is vendored as plain
committed headers at `extern-vendor/rapidjson/include`, from the same upstream
commit beatsaber-hook 6.4.2 bundles. It is deliberately not a qpm dependency:
the registry's `rapidjson-macros` package restores to an empty directory
(confirmed by a CI step that listed the restored tree), and a git submodule
kept arriving empty in CI. Plain files in the repo have neither failure mode
and need no special clone flags.

Toolchain: QPM.CLI, CMake ≥ 3.22, Ninja, Android NDK **r27** (r27d tested).
r25c's bundled Clang 14 is too old for beatsaber-hook 6.x's C++20 usage and
fails with `'ranges' file not found` plus constexpr errors deep in
compilation — nowhere near an obviously NDK-shaped error.

### Committing the lockfile

`qpm.shared.json` is QPM's lockfile (same idea as `Cargo.lock`) — **it should
be committed**. Without it QPM does an unlocked resolve and prints a warning
in CI. Generate it once:

```
qpm restore
git add qpm.shared.json
git commit -m "Lock qpm dependencies"
```

### Game version and codegen

This mod targets **Beat Saber 1.40.8_7379**, via `bs-cordl` `^4008.0.0` and
`bsml` `^0.4.55` (the BSML release built against that same codegen).

**bs-cordl version numbers are not ordered by game version**, which is worth
knowing before you touch that range. 1.40.1, 1.40.2 and 1.40.3 were published
as `4010`, `4020`, `4030`; then 1.40.4 through 1.40.8 as `4004` through
`4008`. So `4008.0.0` (1.40.8) sorts *below* `4030.0.0` (1.40.3), and a range
of `*` quietly resolves to the older game — which is exactly what this repo
was doing.

Three things must move together when you retarget:

| File | Field | For 1.40.8 |
|---|---|---|
| `qpm.json` | `bs-cordl` range | `^4008.0.0` |
| `qpm.json` | `bsml` range | `^0.4.55` |
| `mod.template.json` | `packageVersion` | `1.40.8_7379` |

BSML pins its own `bs-cordl` range, so a mismatched pair fails to resolve
rather than building something subtly wrong. The mapping for other builds is
in the [bs-cordl tags](https://github.com/QuestPackageManager/bs-cordl/tags) —
each tag's commit message names its game version — and the matching BSML is
the release whose `qpm.json` pins that same cordl range.

QuestPatcher compares `packageVersion` against the installed game exactly. If
it still warns on install, the version string it shows you is the one to put
in `mod.template.json`.

The `.qmod` itself contains only `mod.json` and `libyoutubelivechat.so`; the
four dependencies are fetched by QuestPatcher from the `downloadIfMissing`
URLs in the generated manifest.

---

## 4. Build

```powershell
# Windows
$env:ANDROID_NDK_HOME = "C:\path\to\ndk\27.3.13750724"
./build.ps1
./createqmod.ps1
```

```bash
# Linux/macOS
export ANDROID_NDK_HOME=/path/to/ndk/27.3.13750724
./build.sh
./createqmod.sh
```

Output: `build/libyoutubelivechat.so`, packaged into `YouTubeLiveChat.qmod`.

Packaging goes through `scripts/package-qmod.sh`, which CI and both local
`createqmod` scripts call, so the local and CI archives can't drift apart. It
renders the manifest, checks that the files mod.json declares are exactly what
the archive will contain (a manifest naming a missing file fails at install
time on the headset, which is a slow place to find out), and then zips.

`mod.json` is **generated** from `mod.template.json` by `qpm qmod manifest`,
which fills in the dependency list — ids, version ranges and
`downloadIfMissing` URLs — from what QPM actually resolved. It is gitignored
on purpose: a hand-maintained copy goes stale silently when an upstream repo
moves, which is exactly what had happened here (both `custom-types` and
`bsml` changed owner, so the committed URLs 404'd and installs failed).

Both build scripts and CI check that `setup` and `late_load` are present in
the `.so`'s dynamic symbol table. The build uses `-fvisibility=hidden`, so a
missing visibility attribute produces a library that loads without complaint
and then does nothing at all.

---

## 5. Quest installation

1. Beat Saber must already be modded with **Scotland2** (not the older
   QuestLoader). Follow the BSMG wiki's Quest Modding guide if it isn't.
2. In **QuestPatcher**, install `YouTubeLiveChat.qmod`. Its dependencies
   (CustomTypes, BSML, paper2_scotland2) are listed in the manifest with
   download URLs, so QuestPatcher fetches any that are missing.
3. Launch Beat Saber. **Mods → YouTube Live Chat** appears in the settings
   menu, and the chat panel appears in the world.

---

## 6. YouTube setup

### Option A — Sign in with YouTube (recommended if it's your stream)

This uses OAuth 2.0 for **TVs and Limited-Input Devices**, the flow Google
documents for hardware with no usable browser or keyboard. Nothing is typed
in the headset except the client id/secret, once.

1. [Google Cloud Console](https://console.cloud.google.com/) → create or pick
   a project.
2. **APIs & Services → Library** → enable **YouTube Data API v3**.
3. **APIs & Services → OAuth consent screen** → configure it (External is
   fine; add your own Google account as a test user).
4. **Credentials → Create Credentials → OAuth client ID → application type
   "TVs and Limited Input devices."** Copy the client ID and client secret.
5. In the mod's settings: set **Sign-in method** to "Sign in with YouTube",
   paste the client ID and secret, then press **Sign in with YouTube**.
6. The headset shows a short code and a URL. Open it on your phone or PC,
   enter the code, approve. The mod connects by itself.
7. Leave **"Use my own live stream"** on and the mod finds your active
   broadcast via `liveBroadcasts.list(mine=true)` — no video ID needed. Turn
   it off to watch a specific video ID instead.

Only a refresh token is stored, in the mod's own config file on the headset.
The scope requested is `youtube.readonly`: the mod reads chat and cannot
post, delete, or change anything on your account.

### Option B — API key (any public stream)

1. Same project, **APIs & Services → Credentials → Create Credentials → API
   key**. Restrict it to the YouTube Data API v3.
2. In the mod's settings, leave **Sign-in method** on "API key", paste the
   key, and paste a video ID or URL.

Quota: the default is 10,000 units/day and `liveChatMessages.list` costs a
handful of units per call. At the default 2-second floor that's enough for
several hours of continuous polling on one key.

---

## 7. Configuration reference

Everything is in **Mods → YouTube Live Chat**, in three columns:

- **Connection** — sign-in method, API key, OAuth client id/secret, sign
  in/out, "use my own live stream", video ID/URL, connect/disconnect,
  auto-reconnect, debug logging.
- **Panel** — show overlay, follow-my-head, position preset, distance,
  horizontal/vertical offset, width, height, scale, opacity.
- **Messages** — max visible, hide-after seconds, and per-category toggles
  (regular / Super Chats / Super Stickers / memberships), usernames, profile
  pictures, highlight toggles, notification sound.

Settings persist to
`/sdcard/ModData/com.beatgames.beatsaber/Configs/YouTubeLiveChat.json`.

**Panel placement.** Three ways to put the panel where you want it:

- **Point with the red saber.** Press *Move with red saber* on the pause panel
  (or the *Position with red saber* toggle in settings), point where you want
  the chat window, and press *Drop it here*. The panel rides the saber tip and
  turns to face you while you aim. In the menu, where there are no sabers, it
  follows your left controller instead. Dropping it switches the panel to
  fixed placement and saves the spot.
- **Follow my head.** The panel sits at the chosen preset relative to your head
  and eases into place — a rigid 1:1 head-lock reads as "swimmy" in VR and is
  distracting mid-song. Presets are directions, not just sideways offsets, so
  *middle left/right* really do sit beside you rather than out in front.
  *Distance* is metres from your head; the default is 1.6.
- **Grab handle.** With *follow my head* off, the panel gets a handle you can
  drag. The position is saved a couple of seconds after you let go.

**During a level.** The overlay keeps rendering, because it is a
`DontDestroyOnLoad` BSML `FloatingScreen` rather than a menu view controller.
Pause the song and a small panel appears below the pause menu with show/hide,
reconnect, clear and disconnect — the main settings screen belongs to the
main-menu flow coordinator and can't be opened mid-level.

You can paste either a bare video ID or a full URL (`watch?v=`, `youtu.be/`,
or `/live/` forms) into the video field — it's parsed automatically.

---

## 8. Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| "Not configured" | Missing API key, or missing OAuth client id/secret, or no video ID in API-key mode |
| "Sign-in required" | OAuth mode with no stored token, or the token was revoked. Press "Sign in with YouTube" again |
| "Video not found" | Wrong ID, or the video is private/deleted |
| "Stream offline" | Video exists but isn't live, or you aren't currently streaming — the mod rechecks every 20s |
| "API / auth error" | Bad/disabled API key, or YouTube Data API v3 isn't enabled on that Cloud project. Does **not** auto-retry: fix it, then press Connect |
| "Rate limited" | Daily quota exhausted. Stops rather than hammering a dead key for the rest of the day |
| "Network error" | Wi-Fi hiccup, DNS, etc. Retries with exponential backoff (3s → 60s cap) if auto-reconnect is on |
| Sign-in says the client was rejected | The OAuth client must be of type "TVs and Limited Input devices"; a Web or Desktop client is refused by the device endpoint |
| Overlay doesn't appear at all | Check the Scotland2 log for `youtube-live-chat`. If you see "No late_load function on mod", the `.so` was built without exported entry points |
| Avatars never load | Confirm "Profile pictures" is on; a failed avatar leaves a blank square rather than blocking the message text |

---

## 9. How the live chat update system works

There is no push/streaming endpoint for YouTube live chat in the public API —
it's poll-based by design on Google's end. The flow:

1. Find the live chat id. In OAuth mode with "use my own live stream",
   `liveBroadcasts.list?broadcastStatus=active&mine=true` gives
   `snippet.liveChatId` directly. Otherwise
   `videos.list?part=snippet,liveStreamingDetails&id=<id>` resolves a video ID
   to `liveStreamingDetails.activeLiveChatId`; if that field is absent the
   video isn't live (handled as `StreamOffline`, rechecked every 20s).
2. `liveChatMessages.list?liveChatId=<id>&pageToken=<token>` returns a page of
   new messages plus a `nextPageToken` and a server-suggested
   `pollingIntervalMillis`. We wait `max(pollingIntervalMillis,
   minPollIntervalSeconds)` — respecting the server's hint, with a
   configurable floor (default 2s) so a low suggestion can't turn into
   quota-burning spam.
3. The **first** page is the existing chat backlog, and it is dropped rather
   than displayed: dumping a few hundred stale messages into the overlay the
   moment you connect is not useful.
4. Each message's `snippet.type` maps to one of the categories in §1.3;
   unsupported types (polls, `chatEndedEvent`, sponsor-only-mode toggles, the
   legacy `fanFundingEvent`) are parsed and dropped rather than guessed at.
5. Everything is a chain of C++20 coroutines driven by
   `MonoBehaviour::StartCoroutine` on the overlay's `DontDestroyOnLoad`
   component, so a poll in flight is not cancelled when a level loads.

### Why System.Net.Http instead of UnityWebRequest

Beat Saber's IL2CPP build has `UnityWebRequest::Post`, `UploadHandlerRaw` and
`SetRequestHeader` stripped — they're absent from the generated headers
because nothing in the game references them. That leaves UnityWebRequest able
to issue only header-less GETs, which cannot express an OAuth form POST or an
`Authorization: Bearer` header. `System.Net.Http.HttpClient` is present and
complete, so API calls go through it: the request runs on a .NET thread-pool
thread while a coroutine parks on the `Task`, one frame at a time, so
callbacks still arrive on the main thread. Avatar images are plain public GETs
and still use `UnityWebRequestTexture`.

---

## 10. How this avoids impacting Beat Saber's performance

- **No per-message allocation churn.** The overlay pre-builds a fixed pool of
  `MessageRow`s once. New messages reuse a pooled row (text/texture mutated in
  place); nothing is `Instantiate`d or `Destroy`ed per chat message.
- **No per-frame network work.** Requests fire on a timer, never in `Update()`.
- **Expiry is O(1) amortized.** Messages expire in FIFO order, so `Update()`
  only checks the *oldest* active row — if that one hasn't expired, nothing
  behind it has either.
- **Layout only recomputes on actual change** (a message added or expired).
- **Avatars are cached by URL**, so a chatter who posts 20 times triggers one
  download.
- **Gameplay code is untouched.** The hooks installed are
  `MainFlowCoordinator::DidActivate` (fires once at boot, to spawn the
  overlay) and three `PauseMenuManager` methods (to show/hide the in-level
  panel). Nothing touches note spawning, scoring, sabers, or timing.

### A note on GC and C++ containers

IL2CPP's garbage collector does not scan plain C++ memory. A C# object whose
only reference lives in a `std::unordered_map`, or in a coroutine frame across
a `co_yield`, can be collected out from under you. Long-lived C# references
here are held through `SafePtr` / `SafePtrUnity`, which register them as GC
roots — the cached avatar textures, the in-flight `HttpClient` and `Task`s,
and the UI objects the controllers hold on to.

---

## 11. Continuous integration

`.github/workflows/build.yml` builds the `.qmod` on every push to `main` or a
`claude/**` branch, on PRs to `main`, and on manual dispatch, using
`nttld/setup-ndk` + `seanmiddleditch/gha-setup-ninja` + QPM.CLI — the same
toolchain as the local build scripts, so CI and local builds can't silently
drift. Every run uploads the built `.qmod`, the generated `mod.json` and the
`.so` as artifacts, and prints the archive listing.

The build additionally fails if `setup` or `late_load` is missing from the
`.so`'s dynamic symbol table (see §4).

Push a tag like `v0.2.0` and it also cuts a GitHub Release with the `.qmod`
attached.

---

## Known limitations

- Super Sticker artwork isn't fetchable through the public API (only metadata
  like amount and tier), so those render as a labeled Super Chat-style row.
- The mod is built against `bs-cordl` for Beat Saber 1.40.3_4614 (§3). On a
  newer game build QuestPatcher may warn about the version; the hooks resolve
  by name, so they generally still bind.
- OAuth needs your own Google Cloud OAuth client. There is no shared client id
  baked into the mod on purpose: a client secret shipped inside a public
  `.qmod` is not a secret, and everyone sharing one would share its quota.
- The "notification sound" is a procedurally generated beep, so no audio asset
  ships in the `.qmod`.
