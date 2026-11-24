// BACKUP SERVERS FOR A LIVE TV ACCOUNT (Xtream, M3U, Stalker).
//
// IPTV providers usually hand out several addresses ("DNS") for the same
// subscription: the username/password (or MAC) stay the same, only the host
// changes. When the main host is down, blocked by the ISP or behind a broken
// Cloudflare page, the same account works on another one. This module is the
// shared, I/O-free part: the list of backup addresses of ONE account, which of
// them is in use, the automatic-switch flag, the order in which they are tried
// and the rule for "this answer means the server is down, try the next one".
//
// Storage belongs to each backend: the lines below live in the account's own
// credential file (xtream-p<N>-s<id>.txt, m3u-..., stalker-...), so removing
// or forgetting the account removes its backups too, and they never reach the
// synced settings, logs or the screen in full.
//
//   reserva\t<url>          one line per backup, in the order added
//   reservaAtiva\t<k>        0 = main address, k = backup k
//   reservaAuto\t<0|1>       automatic switching (default on)
//
// Index convention everywhere: 0 is the main address, 1..n are backups.
#ifndef NV_IPTVRESERVA_H
#define NV_IPTVRESERVA_H

#define IPTVRES_MAX 8            // backups per account (the main one excluded)
#define IPTVRES_URL 1100         // fits an M3U playlist URL with its query

typedef struct {
  char url[IPTVRES_MAX][IPTVRES_URL];
  int n;           // how many backups
  int ativo;       // 0 main, 1..n backup in use
  int autoTroca;   // 1: switch by itself when the server in use fails
} IptvReservas;

void iptvres_zerar(IptvReservas *r);

// Parse one "key\tvalue" line of the backend file. 1 if the key was ours.
int iptvres_ler_linha(IptvReservas *r, const char *chave, const char *valor);
// After every line was read: clamp `ativo` to the list.
void iptvres_ler_fim(IptvReservas *r);
// Append the lines above to `dst` (NUL-terminated, at most `tam` bytes in
// total). 0 if they did not fit.
int iptvres_gravar(const IptvReservas *r, char *dst, unsigned tam);

// `url` must already be normalised by the backend. Rejects empty, duplicates
// (of the main one too), control characters and a full list. 1 on success.
int iptvres_adicionar(IptvReservas *r, const char *url, const char *principal);
// Remove backup `k` (1..n). The server in use keeps pointing at the same
// address; removing the one in use goes back to the main address.
int iptvres_remover(IptvReservas *r, int k);
int iptvres_definir_ativo(IptvReservas *r, int k);

// The address with index k (0 = `principal`). NULL past the end.
const char *iptvres_servidor(const IptvReservas *r, const char *principal, int k);
// How many addresses there are, the main one included (0 without a main one).
int iptvres_total(const IptvReservas *r, const char *principal);
// Order of attempts: the one in use first, then the others in list order.
// With automatic switching off only the one in use. Returns the count.
int iptvres_ordem(const IptvReservas *r, const char *principal, int idx[IPTVRES_MAX + 1]);

// Failover policy for one HTTP answer. `status` 0 = no answer at all; `html` 1
// when an API that answers JSON sent a web page (ISP block page, Cloudflare
// error, captive portal). Credential (401), screen limit (458) and rate limit
// (429) answers are about the ACCOUNT: another host would say the same, and
// hammering it could get the account banned, so they never switch.
int iptvres_falhou(int status, int html);

// "scheme://host[:port]" of an http(s) URL, without user info, path or query.
int iptvres_origem(const char *url, char *dst, unsigned tam);
// The same URL with its origin replaced by `origem` (path and query kept).
int iptvres_trocar_origem(const char *url, const char *origem, char *dst, unsigned tam);
// 1 when `url` is just "scheme://host[:port]" (an optional "/" at the end).
int iptvres_so_origem(const char *url);
// Which address of the list `url` belongs to (same origin), or -1.
int iptvres_indice_da_url(const IptvReservas *r, const char *principal, const char *url);
// Host only, for the screen: never the path or query (an M3U URL carries the
// password there). "-" when empty.
void iptvres_curto(const char *url, char *dst, unsigned tam);

#endif
