# Telegram Pager — design concept (#22)

> **Status:** design only. No code in this document. It describes how a Telegram
> "pager" would forward the last ~50 Telegram messages to the Nocturne device in
> a readable form, with quick replies, served from `dashboard.example.com/pager`, and
> end-to-end encrypted between the server and the device.

## 1. Goal & scope

Turn the Heltec OLED into a glanceable, two-way Telegram pager:

- A server-side logger ingests messages from selected Telegram chats and keeps a
  rolling ring of the **last ~50** messages.
- The device pulls those messages, renders them in a compact reader, and lets the
  user fire a small set of **canned quick replies** back to the originating chat
  with the single button.
- Message bodies are **encrypted on the wire and at rest** so `dashboard.example.com`
  (a VPS the project already talks to for Alertmanager) never needs to be trusted
  with plaintext beyond the moment of ingest.

Non-goals: full chat history, media, free-text composition on-device (the OLED +
single button can't do a keyboard), group administration. Quick replies are a
fixed, configurable phrase set, not arbitrary text.

## 2. Where this sits relative to the existing system

The device today opens **one** TCP connection to the PC companion
(`server/monitor.py`, port **8888**) and receives a compact, newline-terminated
JSON telemetry frame ~2×/second; it sends short lines back (`HELO`, `screen:N`,
and now `cmd:claude` / `cmd:status`). The line cap on both ends is
`NOCT_TCP_LINE_MAX = 4096` (`lib/nocturne-core/src/nocturne/config.h`).

The pager is **a second, independent data source** living on `dashboard.example.com`,
not part of monitor.py. Two integration shapes are possible; the doc recommends
**(B)** but both are viable:

- **(A) Inline in the telemetry frame.** monitor.py polls `dashboard.example.com/pager`
  (like it already polls Alertmanager / the forest panel) and folds a small
  `pgr` block into the existing 8888 frame. Pros: device keeps one socket, reuses
  the proven parse path, works only when the PC is up. Cons: couples the pager to
  the PC being on; bloats the hot telemetry frame; the PC sees ciphertext only
  (good) but must relay it.
- **(B) Direct device ↔ forestserver session (recommended).** The device opens a
  second TCP (or TLS) connection straight to `dashboard.example.com:<pagerport>` (or
  hits the HTTP endpoint `dashboard.example.com/pager`). Pager works even when the PC
  is off; the hot telemetry path stays lean; encryption keys live only on the
  device and the pager server, never on the PC. Cons: a second connection + a
  second reader on the device.

Either way the **payload framing is identical** to today's protocol —
newline-terminated compact JSON, ≤4096 bytes per line — so the device's existing
line reader and `ArduinoJson` deserialize are reused wholesale.

## 3. Architecture

```
 Telegram ──▶  Ingest (bot or userbot)  ──▶  Store (encrypted ring, ~50 msgs)
                     on dashboard.example.com             │
                                                    ▼
                                           Pager endpoint  /pager
                                          (TCP line / HTTP long-poll)
                                                    │  ciphertext frames
                                                    ▼
                                         Device pager reader + UX
                                          (decrypt → render → quick-reply)
                                                    │  ciphertext reply
                                                    ▼
                                          Endpoint → Ingest → Telegram
```

### 3.1 Ingest (Telegram → store)

Two options, pick per how many chats and which message visibility is needed:

- **Bot API (simplest, recommended first).** A normal `@BotFather` bot added to
  the chats of interest; receive updates via long-poll `getUpdates` or a webhook.
  Limitation: a bot only sees messages in groups where it is a member and (by
  privacy mode) usually only messages addressed to it or commands — fine for a
  dedicated "pager" group/channel, not for mirroring a personal DM feed.
- **Userbot (MTProto, e.g. Telethon/Pyrogram).** Logs in as the user account, so
  it can see *all* the user's chats. More powerful and exactly matches "the last
  50 messages I received," but carries ToS/account-safety considerations and
  needs the user's API id/hash + a session string. Treat as opt-in/advanced.

The ingest worker normalizes every inbound message to a small record:

| field | meaning |
|-------|---------|
| `id`  | monotonic pager-local sequence id (NOT the TG message id) |
| `chat`| short chat label (resolved from chat id; truncated to ~16 chars) |
| `from`| sender display name (truncated) |
| `ts`  | unix seconds |
| `text`| message body, whitespace-collapsed, truncated to a device-friendly length (e.g. 160 chars; longer bodies get an ellipsis + are readable in full only on the source) |
| `tgchat`, `tgmsg` | the real TG chat id + message id, kept **server-side only** for routing replies (never sent to the device) |

Media/stickers/etc. collapse to a placeholder body (`[photo]`, `[voice]`, …).

### 3.2 Store (rolling encrypted ring)

- Fixed-capacity ring buffer of the last **N≈50** records (configurable). Oldest
  evicted on overflow. Persist to a single file (or SQLite) so a restart keeps
  history; persistence holds **ciphertext** bodies (see §5), so disk compromise
  on the VPS does not leak message text.
- The `tgchat`/`tgmsg` routing fields are the one piece kept in cleartext
  server-side (the server must route replies); everything the *device* sees is
  encrypted. A reply token (§4.3) indirects the routing so even the device→server
  reply does not have to carry raw TG ids.

### 3.3 Pager endpoint (`dashboard.example.com/pager`)

Serves the device. Mirror the existing wire conventions:

- **Transport:** prefer TCP-with-TLS or HTTPS so the network path is encrypted
  even before the app-layer crypto; app-layer AES-GCM (§5) is still mandatory so
  the VPS itself is not a plaintext trust point.
- **Pull model:** device sends a small request line with the highest `id` it has
  (`pgr:since:<id>`); server replies with newline-terminated JSON frames for any
  newer records, then idles / long-polls. This is the same "device sends a short
  line, server streams JSON lines back" shape as port 8888.
- **Frame budget:** keep each frame ≤4096 bytes. ~50 short messages do **not**
  fit in one frame, so the endpoint **paginates**: send a compact `head` summary
  (ids + chat + first ~24 chars, enough to render the list) plus full bodies for
  a small window (the few messages around the cursor), fetched on demand as the
  user scrolls (`pgr:open:<id>`). This keeps both the frame and the device's RAM
  small (the device cannot hold 50 full bodies comfortably).

## 4. Device-side reader + quick-reply UX

### 4.1 New scene set

Add a "Pager" scene group (same pattern as the `claude` / `forest` / `svc`
blocks already rendered by `SceneManager`), reached from the menu:

- **Inbox list view** — one line per message: `●` unread dot, chat label (left),
  age (`3m`, `1h`, right), and as much of the body as fits. Scroll the list with
  short/triple gestures; an unread counter shows in the header.
- **Message view** — open one message: full (truncated) body word-wrapped across
  the 128×64 panel, with `from` + `chat` + time in a header line, and a footer
  hint showing the quick-reply affordance.

### 4.2 Input mapping (single button, existing gestures)

The firmware already exposes `EV_SHORT` / `EV_DOUBLE` / `EV_LONG` / `EV_TRIPLE`
(see `src/main.cpp`). A natural mapping inside the pager scene:

| gesture | inbox list | message view | reply chooser |
|---------|-----------|--------------|---------------|
| `EV_SHORT`  | next message | scroll body / next | next canned reply |
| `EV_TRIPLE` | previous message | previous | previous canned reply |
| `EV_DOUBLE` | open selected message | open reply chooser | confirm/send reply |
| `EV_LONG`   | leave pager / mark-all-read | back to list | cancel reply |

(Exact bindings to be tuned on hardware; the point is the existing gesture
vocabulary is sufficient — no new input hardware.)

### 4.3 Quick replies

- A **fixed, configurable list** of short canned phrases provisioned to the
  device (e.g. "OK", "On my way", "Call me", "👍", "Later"). The list is part of
  the pager config, not typed on-device.
- Reply flow: in a message view, `EV_DOUBLE` opens the chooser; cycle phrases;
  `EV_DOUBLE` sends. The device emits one short line on the pager connection:
  `pgr:reply:<reply_token>:<phrase_idx>` where `<reply_token>` is an opaque,
  short-lived token the server handed out with the message (it maps server-side
  to the real `tgchat`/`tgmsg`, so the device never holds or transmits raw TG
  ids). The reply body is the indexed canned phrase; for confidentiality the
  phrase index (or the phrase text) is carried inside the AES-GCM envelope just
  like inbound bodies.
- Server validates the token (unexpired, belongs to this device), looks up the
  route, and sends the phrase to Telegram as a reply to the original message.
- Device shows a transient toast (`sent ✓` / `send failed`) using the existing
  toast mechanism; the server may also echo a synthetic confirmation record.

### 4.4 Notifications

When a new record arrives while the device is on another scene, reuse the
existing toast/banner path (the same one Alertmanager events use) to surface
"`<chat>: <first words>`" and bump the unread counter, so the pager is glanceable
without being parked on its scene.

## 5. Encryption approach

Threat model: the VPS (`dashboard.example.com`) and the network are **semi-trusted** —
we want message bodies and replies unreadable to anyone who compromises the VPS
disk or sniffs the link. The endpoints we trust are the **device** and the
**ingest worker** (which must see plaintext to talk to Telegram at all).

### 5.1 Primitive

- **AES-256-GCM** over each message **body** and each **reply body** (an AEAD:
  confidentiality + integrity in one). The ESP32-S3 has hardware AES; a small
  embedded GCM implementation (e.g. mbedTLS, already in the Arduino-ESP32 stack)
  handles the device side.
- Envelope per encrypted field: `{ "n": <nonce>, "c": <ciphertext>, ... }` with a
  **96-bit random nonce per message** (never reused under a given key) and the
  GCM tag appended to the ciphertext. Nonce + ciphertext + tag are base64'd for
  JSON transport (or hex if simpler on-device).
- **AAD (associated data):** bind each ciphertext to its non-secret context — at
  least the pager `id` and `key_id` — so a frame can't be lied about or replayed
  under a different id without failing the tag check.

What is encrypted vs. cleartext in a device frame:

- **Encrypted:** message body text, sender/chat labels if desired, quick-reply
  phrase/index.
- **Cleartext (metadata, kept minimal):** pager `id`, `ts`, `key_id`, unread
  flag, and the opaque `reply_token`. (If even chat/sender labels are sensitive,
  put them inside the envelope too; the list view then needs the body window.)

### 5.2 Key model & provisioning

Recommended: a **shared symmetric key** per device, known only to the device and
the pager server (matches "shared key on device + server" in the brief).

- **Provisioning (out-of-band, one-time):** generate a random 256-bit key on a
  trusted machine; load it into the device the same way `secrets.h` already
  carries Wi-Fi/IP secrets — i.e. **compiled in** or flashed to NVS, never sent
  over the wire. The matching key is placed in the pager server's secret store
  (env/file with tight perms, not in git). Each frame/envelope carries a `key_id`
  so keys can rotate.
- **Key rotation:** support ≥2 active `key_id`s server-side during a rollover;
  the device is reflashed/reprovisioned with the new key and starts sending the
  new `key_id`. Old records remain decryptable until evicted from the ring.
- **Per-message nonce uniqueness:** the server generates the nonce at encrypt
  time and stores it with the record; the device generates its own random nonce
  for replies. With random 96-bit nonces and ~50 messages the reuse risk is
  negligible; if a deterministic scheme is ever wanted, derive nonce = HKDF(key,
  id) so it is unique per id.

### 5.3 Stronger option (future)

If the VPS should be **zero-knowledge** even at ingest, the body could be
encrypted by the *sender's* side instead — but Telegram delivers plaintext to any
bot/userbot, so true zero-knowledge isn't achievable without the user
pre-encrypting in Telegram. Therefore the practical boundary is: plaintext exists
only transiently inside the ingest worker; everything stored and everything the
device sees is AES-GCM. Document this boundary explicitly for the user.

### 5.4 Authentication of the device

Reuse the shared key for a lightweight device→server auth: the device's request
lines (`pgr:since`, `pgr:reply`) include a short HMAC (or are themselves sent
inside an AEAD envelope) so the server only accepts replies from a holder of the
key, preventing a stranger who finds the endpoint from injecting Telegram
messages. Rate-limit replies server-side regardless.

## 6. Wire protocol sketch (framing only, not code)

Device → server (short lines, like `screen:N`):

- `pgr:hello:<key_id>` — open/identify on connect.
- `pgr:since:<id>` — request records newer than `<id>`.
- `pgr:open:<id>` — request the full (encrypted) body for one id (body-window
  fetch for the message view).
- `pgr:reply:<reply_token>:<env>` — send a quick reply; `<env>` is the AES-GCM
  envelope of the phrase/index.
- `pgr:read:<id>` — mark read (optional; clears the unread dot server-side).

Server → device (newline JSON, ≤4096 B/frame):

```
{"t":"pgr","key_id":1,"head":[{"id":91,"chat":"<env>","ts":171...,"u":1,"rt":"<token>"}, ...]}
{"t":"pgr","key_id":1,"body":{"id":91,"from":"<env>","text":"<env>"}}
{"t":"pgr","key_id":1,"ack":{"id":91,"reply":"sent"}}
```

`<env>` = the base64 AES-GCM envelope (`nonce|ciphertext|tag`). `rt` = opaque
reply token. The device renders `head` for the list and pulls `body` on open.

## 7. Open questions

1. **Ingest source:** Bot API (a dedicated pager group/channel, safe, limited) vs.
   userbot/MTProto (full personal feed, ToS + account-safety risk). Which does
   the owner want — and if userbot, where does the session string live?
2. **Integration shape:** inline `pgr` block in monitor.py's 8888 frame (one
   socket, PC-dependent) vs. a direct device↔forestserver pager connection
   (recommended; PC-independent; keys off the PC). Confirm (B).
3. **Reply set:** what canned phrases, how many, per-chat or global? Any emoji?
4. **Which chats** are mirrored, and how are chat labels chosen/abbreviated for a
   16-char field?
5. **Body length & truncation:** device-side max chars; do we ever need
   multi-frame bodies for long messages, or is a hard truncate acceptable?
6. **Key provisioning UX:** compile-time in `secrets.h` vs. NVS load vs. a
   one-time pairing step. Rotation cadence and how the device is re-keyed.
7. **What metadata may stay cleartext** on the VPS/wire (timestamps, chat labels,
   unread flags) vs. must be inside the envelope?
8. **Read/notification semantics:** does opening on-device mark read in Telegram,
   or only in the pager ring? Should the pager dedupe against the user's own
   Telegram clients?
9. **Endpoint hardening:** TLS termination, auth (HMAC vs. envelope-only),
   per-device rate limits, abuse limits on outbound replies.
10. **Lifecycle on dashboard.example.com:** process/supervisor (systemd?), persistence
    format (file vs. SQLite), and coexistence with the existing Alertmanager
    service already hosted there.
```

