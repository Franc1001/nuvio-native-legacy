# Live TV: backup servers

IPTV providers usually give several addresses ("DNS") for one subscription.
The username and password (or the MAC for a Stalker portal) stay the same;
only the host changes. When the main host is down, blocked by the internet
provider or stuck behind an error page, the same account works on another
address. Each Live TV account can now keep up to 8 of these backup addresses.

## Using it

Settings › Live TV › (Xtream / M3U / Stalker) › pick the account › **Edit**:

| Row | What it does |
|---|---|
| **Add backup server** | Opens the keyboard. Type the address the provider sent, e.g. `backup.server.tv:8080`. For an M3U account a bare host keeps the path and query of the main playlist (`/get.php?username=…`); a full playlist link is used as typed. |
| **Backup servers** | Picks one of the saved backups (only the host is shown, never the path or the password). |
| **Remove the chosen server** | Removes the backup picked above (press OK twice). |
| **Server in use** | Main or one of the backups. Changing it applies at once and reloads the guide. |
| **Switch servers automatically** | On by default. Off: only the server in use is ever tried. |

The rows appear once the account is saved (they are hidden while a new account
is being typed) and the list belongs to that account only.

## What switches, and what does not

The server in use is always asked first, then the other addresses of the
account in list order. The first one that answers becomes the server in use
and is remembered, so the next list, guide and channel start there directly.

A server counts as **down** when there is no answer at all (DNS, refused,
timeout, TLS), an HTTP 5xx, a 403/404/410/451, or an HTTP 200 that is a web
page instead of the expected JSON / playlist (ISP block page, Cloudflare error,
captive portal).

Answers about the **account** never switch: a refused password (401 or
`"auth": 0`), screens full (458) or rate limiting (429). Every address of the
provider would say the same, and hopping between them could get the account
blocked.

| Account type | Channel list / guide | Playback |
|---|---|---|
| Xtream | `player_api.php` fails over; the XMLTV guide uses the server in use | After every source of the server in use (formats, variants), the same channel on each backup. The one that plays becomes the server in use. |
| M3U | The playlist download fails over (an HTML answer counts as down) | Channels served from the account's own host get the same URL on each backup host. Channels on third-party CDNs are left alone. |
| Stalker | The handshake tries the portal in use, then the others, each with the full route scan | The portal that gave the token creates the links. |

## Where it is stored

In the account's own credential file (`xtream-p<N>-s<id>.txt`,
`m3u-p<N>-s<id>.txt`, `stalker-p<N>-s<id>.txt`), as `reserva`,
`reservaAtiva` and `reservaAuto` lines. Removing or forgetting the account
removes its backups. Nothing goes to `ajustes.txt`, the synced settings or the
log (the log only says "server 1 of 3 failed", never an address).

## Code

- `src/iptvreserva.[ch]`: list, order of attempts, failover rule, URL origin
  helpers. No I/O.
- `src/xtream.c`, `src/m3u.c`, `src/stalker.c`: storage and failover.
- `src/iptvconta_reserva.[ch]`: Settings dispatch for the selected account.
- `src/app.c`: backup sources for Xtream and M3U channels.
- `src/ajustes.c` and `ajustes_ux_*.inc`: the five rows; strings in all 30
  translation tables.

Tests: `tests/iptvreserva.sh`, `tests/xtream.sh`, `tests/m3u.sh`,
`tests/stalker_portal.sh`, `tests/ajustes_iptv_reserva.sh`.

## Builds without an account server

A build made without `NUVIO_SUPABASE_URL` / `NUVIO_SUPABASE_ANON_KEY` cannot
sign in to a Nuvio account, so it no longer shows a login screen that cannot
work: it opens straight into the home screen with local profiles, local add-ons
and local Live TV accounts (`login_sem_conta()` in `src/login.c`). Builds with
the account server configured behave exactly as before.
