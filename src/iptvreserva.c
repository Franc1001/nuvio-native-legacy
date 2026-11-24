#include "iptvreserva.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void iptvres_zerar(IptvReservas *r) {
  if (!r) return;
  r->n = 0; r->ativo = 0; r->autoTroca = 1;
  for (int i = 0; i < IPTVRES_MAX; i++) r->url[i][0] = 0;
}

static int textoLimpo(const char *s) {
  if (!s || !*s) return 0;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++)
    if (*p < 32 || *p == 127) return 0;
  return 1;
}

int iptvres_ler_linha(IptvReservas *r, const char *chave, const char *valor) {
  if (!r || !chave || !valor) return 0;
  if (!strcmp(chave, "reserva")) {
    if (r->n < IPTVRES_MAX && textoLimpo(valor) && strlen(valor) < IPTVRES_URL) {
      snprintf(r->url[r->n], IPTVRES_URL, "%s", valor);
      r->n++;
    }
    return 1;
  }
  if (!strcmp(chave, "reservaAtiva")) { r->ativo = atoi(valor); return 1; }
  if (!strcmp(chave, "reservaAuto")) { r->autoTroca = atoi(valor) != 0; return 1; }
  return 0;
}

void iptvres_ler_fim(IptvReservas *r) {
  if (!r) return;
  if (r->ativo < 0 || r->ativo > r->n) r->ativo = 0;
}

int iptvres_gravar(const IptvReservas *r, char *dst, unsigned tam) {
  size_t k;
  if (!r || !dst || !tam) return 0;
  k = strlen(dst);
  for (int i = 0; i < r->n; i++) {
    int w = snprintf(dst + k, tam - k, "reserva\t%s\n", r->url[i]);
    if (w < 0 || (size_t)w >= tam - k) return 0;
    k += (size_t)w;
  }
  int w = snprintf(dst + k, tam - k, "reservaAtiva\t%d\nreservaAuto\t%d\n", r->ativo, r->autoTroca ? 1 : 0);
  return w >= 0 && (size_t)w < tam - k;
}

// Two addresses are the same server when scheme, host and port match without
// regard to case, and the rest matches exactly (a path can be case sensitive).
// Everything after the authority ("/path?query"), "" when there is none.
static const char *restoDaUrl(const char *url) {
  const char *p = url + (strncasecmp(url, "https://", 8) ? 7 : 8);
  return p + strcspn(p, "/?#");
}
static int mesmoEndereco(const char *a, const char *b) {
  char oa[300], ob[300];
  const char *ra, *rb;
  if (!a || !b) return 0;
  if (!iptvres_origem(a, oa, sizeof oa) || !iptvres_origem(b, ob, sizeof ob)) return !strcmp(a, b);
  if (strcasecmp(oa, ob)) return 0;
  ra = restoDaUrl(a); rb = restoDaUrl(b);
  // A bare host stands for "this host, with the main path" (M3U): same server
  // as any address on that host. "http://h:1" and "http://h:1/" too.
  if (!*ra || !strcmp(ra, "/") || !*rb || !strcmp(rb, "/")) return 1;
  return !strcmp(ra, rb);
}

int iptvres_adicionar(IptvReservas *r, const char *url, const char *principal) {
  if (!r || !textoLimpo(url) || strlen(url) >= IPTVRES_URL || r->n >= IPTVRES_MAX) return 0;
  if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) return 0;
  if (principal && principal[0] && mesmoEndereco(url, principal)) return 0;
  for (int i = 0; i < r->n; i++) if (mesmoEndereco(url, r->url[i])) return 0;
  snprintf(r->url[r->n], IPTVRES_URL, "%s", url);
  r->n++;
  return 1;
}

int iptvres_remover(IptvReservas *r, int k) {
  if (!r || k < 1 || k > r->n) return 0;
  memmove(r->url[k - 1], r->url[k], (size_t)(r->n - k) * IPTVRES_URL);
  r->n--;
  r->url[r->n][0] = 0;
  if (r->ativo == k) r->ativo = 0;
  else if (r->ativo > k) r->ativo--;
  return 1;
}

int iptvres_definir_ativo(IptvReservas *r, int k) {
  if (!r || k < 0 || k > r->n) return 0;
  r->ativo = k;
  return 1;
}

const char *iptvres_servidor(const IptvReservas *r, const char *principal, int k) {
  if (k == 0) return principal && principal[0] ? principal : NULL;
  if (!r || k < 0 || k > r->n) return NULL;
  return r->url[k - 1];
}

int iptvres_total(const IptvReservas *r, const char *principal) {
  if (!principal || !principal[0]) return 0;
  return 1 + (r ? r->n : 0);
}

int iptvres_ordem(const IptvReservas *r, const char *principal, int idx[IPTVRES_MAX + 1]) {
  int total = iptvres_total(r, principal), n = 0, ativo;
  if (!total || !idx) return 0;
  ativo = r && r->ativo >= 0 && r->ativo < total ? r->ativo : 0;
  idx[n++] = ativo;
  if (r && !r->autoTroca) return n;
  for (int k = 0; k < total; k++) if (k != ativo) idx[n++] = k;
  return n;
}

int iptvres_falhou(int status, int html) {
  if (status == 0) return 1;                 // DNS, refused, timeout, TLS
  if (status == 401 || status == 429 || status == 458) return 0;
  if (status >= 500) return 1;               // panel down, bad gateway
  if (status == 403 || status == 404 || status == 410 || status == 451) return 1;  // blocked / wrong host
  if (status >= 200 && status < 300 && html) return 1;  // block or error page
  return 0;
}

int iptvres_origem(const char *url, char *dst, unsigned tam) {
  const char *p, *ini, *fim, *arroba;
  size_t esquema;
  if (!dst || !tam) return 0;
  dst[0] = 0;
  if (!url) return 0;
  if (!strncasecmp(url, "http://", 7)) esquema = 7;
  else if (!strncasecmp(url, "https://", 8)) esquema = 8;
  else return 0;
  ini = url + esquema;
  fim = ini + strcspn(ini, "/?#");
  // Drop "user:pass@" from the authority: it is never part of the origin.
  arroba = NULL;
  for (p = ini; p < fim; p++) if (*p == '@') arroba = p;
  if (arroba) ini = arroba + 1;
  if (fim == ini) return 0;
  if ((size_t)(fim - ini) + esquema + 1 > tam) return 0;
  memcpy(dst, url, esquema);
  for (size_t i = 0; i < esquema; i++) if (dst[i] >= 'A' && dst[i] <= 'Z') dst[i] = (char)(dst[i] - 'A' + 'a');
  memcpy(dst + esquema, ini, (size_t)(fim - ini));
  dst[esquema + (size_t)(fim - ini)] = 0;
  return 1;
}

int iptvres_trocar_origem(const char *url, const char *origem, char *dst, unsigned tam) {
  char o[300], nova[300];
  const char *resto;
  int w;
  if (!dst || !tam) return 0;
  dst[0] = 0;
  if (!iptvres_origem(url, o, sizeof o) || !iptvres_origem(origem, nova, sizeof nova)) return 0;
  // The rest of `url` starts right after its authority (user info included).
  resto = restoDaUrl(url);
  w = snprintf(dst, tam, "%s%s", nova, resto);
  if (w < 0 || (unsigned)w >= tam) { dst[0] = 0; return 0; }
  return 1;
}

int iptvres_so_origem(const char *url) {
  char o[300];
  const char *r;
  if (!iptvres_origem(url, o, sizeof o)) return 0;
  r = restoDaUrl(url);
  return !*r || !strcmp(r, "/");
}

int iptvres_indice_da_url(const IptvReservas *r, const char *principal, const char *url) {
  char ou[300], os[300];
  int total = iptvres_total(r, principal);
  if (!iptvres_origem(url, ou, sizeof ou)) return -1;
  for (int k = 0; k < total; k++) {
    const char *s = iptvres_servidor(r, principal, k);
    if (s && iptvres_origem(s, os, sizeof os) && !strcasecmp(os, ou)) return k;
  }
  return -1;
}

void iptvres_curto(const char *url, char *dst, unsigned tam) {
  char o[300];
  const char *p;
  if (!dst || !tam) return;
  if (!url || !url[0]) { snprintf(dst, tam, "-"); return; }
  if (!iptvres_origem(url, o, sizeof o)) { snprintf(dst, tam, "-"); return; }
  p = o + (strncmp(o, "https://", 8) ? 7 : 8);
  snprintf(dst, tam, "%s", p);
}
