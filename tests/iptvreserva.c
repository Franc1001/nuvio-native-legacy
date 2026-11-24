// Backup servers of a Live TV account (src/iptvreserva.c): list, order of
// attempts, failover rule and URL origin swapping. No network, no disk.
#include "iptvreserva.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void ok(const char *m) { printf("ok  %s\n", m); }

int main(void) {
  IptvReservas r;
  char b[2048], s[600];
  int idx[IPTVRES_MAX + 1], n;
  const char *P = "http://main.tv:8080";

  iptvres_zerar(&r);
  assert(r.n == 0 && r.ativo == 0 && r.autoTroca == 1);
  assert(iptvres_total(&r, "") == 0 && iptvres_total(&r, P) == 1);
  n = iptvres_ordem(&r, P, idx); assert(n == 1 && idx[0] == 0);
  ok("empty list: only the main address");

  assert(iptvres_adicionar(&r, "http://b1.tv:8080", P));
  assert(iptvres_adicionar(&r, "https://b2.tv", P));
  assert(!iptvres_adicionar(&r, "http://MAIN.tv:8080/", P));   // same as main
  assert(!iptvres_adicionar(&r, "http://b1.tv:8080", P));      // duplicate
  assert(!iptvres_adicionar(&r, "b3.tv", P));                  // not normalised
  assert(!iptvres_adicionar(&r, "http://b3.tv\nx", P));
  assert(!iptvres_adicionar(&r, "", P));
  assert(r.n == 2);
  ok("add rejects duplicates, the main address and bad input");

  n = iptvres_ordem(&r, P, idx);
  assert(n == 3 && idx[0] == 0 && idx[1] == 1 && idx[2] == 2);
  assert(iptvres_definir_ativo(&r, 2));
  n = iptvres_ordem(&r, P, idx);
  assert(n == 3 && idx[0] == 2 && idx[1] == 0 && idx[2] == 1);
  r.autoTroca = 0;
  n = iptvres_ordem(&r, P, idx); assert(n == 1 && idx[0] == 2);
  r.autoTroca = 1;
  assert(!iptvres_definir_ativo(&r, 3) && !iptvres_definir_ativo(&r, -1));
  ok("order: server in use first; manual mode tries only it");

  assert(!strcmp(iptvres_servidor(&r, P, 0), P));
  assert(!strcmp(iptvres_servidor(&r, P, 2), "https://b2.tv"));
  assert(!iptvres_servidor(&r, P, 3));
  ok("index 0 is the main address");

  b[0] = 0; assert(iptvres_gravar(&r, b, sizeof b));
  assert(strstr(b, "reserva\thttp://b1.tv:8080\nreserva\thttps://b2.tv\nreservaAtiva\t2\nreservaAuto\t1\n"));
  { IptvReservas q; char linha[2048], *l, *fim; iptvres_zerar(&q);
    snprintf(linha, sizeof linha, "%s", b);
    for (l = linha; *l; l = fim) { char *t; fim = strchr(l, '\n'); if (fim) *fim++ = 0; else fim = l + strlen(l);
      t = strchr(l, '\t'); if (!t) continue; *t++ = 0; assert(iptvres_ler_linha(&q, l, t)); }
    iptvres_ler_fim(&q);
    assert(q.n == 2 && q.ativo == 2 && q.autoTroca == 1 && !strcmp(q.url[1], "https://b2.tv"));
    assert(!iptvres_ler_linha(&q, "servidor", "x"));
    q.ativo = 9; iptvres_ler_fim(&q); assert(q.ativo == 0); }
  { char pequeno[20] = ""; assert(!iptvres_gravar(&r, pequeno, sizeof pequeno)); }
  ok("save/load round trip; a stale index falls back to main");

  assert(iptvres_remover(&r, 1));          // removing b1 keeps b2 in use
  assert(r.n == 1 && r.ativo == 1 && !strcmp(r.url[0], "https://b2.tv"));
  assert(iptvres_remover(&r, 1) && r.n == 0 && r.ativo == 0);  // in use -> main
  assert(!iptvres_remover(&r, 1));
  for (int i = 0; i < IPTVRES_MAX; i++) { char u[64]; snprintf(u, sizeof u, "http://r%d.tv", i); assert(iptvres_adicionar(&r, u, P)); }
  assert(!iptvres_adicionar(&r, "http://one-too-many.tv", P));
  ok("remove keeps the server in use; list is capped");

  assert(iptvres_falhou(0, 0) && iptvres_falhou(502, 0) && iptvres_falhou(503, 0) && iptvres_falhou(521, 0));
  assert(iptvres_falhou(403, 0) && iptvres_falhou(404, 0) && iptvres_falhou(200, 1));
  assert(!iptvres_falhou(200, 0) && !iptvres_falhou(401, 0) && !iptvres_falhou(429, 0) && !iptvres_falhou(458, 0));
  ok("failover rule: down/blocked yes, account answers no");

  assert(iptvres_origem("http://User:Pw@Host.tv:8080/get.php?u=a", s, sizeof s) && !strcmp(s, "http://Host.tv:8080"));
  assert(iptvres_origem("HTTPS://x.tv", s, sizeof s) && !strcmp(s, "https://x.tv"));
  assert(!iptvres_origem("ftp://x", s, sizeof s) && !iptvres_origem("http://", s, sizeof s));
  assert(iptvres_trocar_origem("http://a.tv:80/live/u/p/1.ts?x=1", "https://b.tv:443/ignored", s, sizeof s) &&
         !strcmp(s, "https://b.tv:443/live/u/p/1.ts?x=1"));
  assert(iptvres_trocar_origem("http://a.tv", "http://b.tv", s, sizeof s) && !strcmp(s, "http://b.tv"));
  { char t[16]; assert(!iptvres_trocar_origem("http://a.tv/very/long/path", "http://b.tv", t, sizeof t) && !t[0]); }
  ok("origin parsing and swapping keep path and query");

  iptvres_zerar(&r); iptvres_adicionar(&r, "http://b1.tv:8080", P);
  assert(iptvres_indice_da_url(&r, P, "http://main.tv:8080/live/u/p/1.ts") == 0);
  assert(iptvres_indice_da_url(&r, P, "http://B1.tv:8080/live/u/p/1.m3u8") == 1);
  assert(iptvres_indice_da_url(&r, P, "http://other.tv/live") == -1);
  iptvres_curto("http://u:p@host.tv:8080/get.php?password=secret", s, sizeof s);
  assert(!strcmp(s, "host.tv:8080"));
  iptvres_curto("", s, sizeof s); assert(!strcmp(s, "-"));
  ok("which server a URL belongs to; the screen never shows path or query");

  printf("iptvreserva: all ok\n");
  return 0;
}
