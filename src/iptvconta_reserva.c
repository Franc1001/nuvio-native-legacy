#include "iptvconta_reserva.h"
#include "iptvfontes.h"
#include "xtream.h"
#include "m3u.h"
#include "stalker.h"
#include <string.h>
#include <stdlib.h>

typedef int (*Mudar)(int (*f)(IptvReservas *, const char *, void *), void *);

static int tipoSelecionado(void) {
  IptvConta c;
  return iptv_conta_selecionada(&c) ? c.tipo : 0;
}
static Mudar mudarDe(int tipo) {
  return tipo == IPTV_XTREAM ? xtream_reservas_mudar : tipo == IPTV_M3U ? m3u_reservas_mudar :
         tipo == IPTV_STALKER ? stalker_reservas_mudar : NULL;
}

int iptvconta_reservas(IptvReservas *r, char *principal, unsigned n) {
  int tipo = tipoSelecionado(), ok;
  if (r) iptvres_zerar(r);
  if (principal && n) principal[0] = 0;
  if (tipo == IPTV_XTREAM) ok = xtream_reservas_ler(r, principal, n);
  else if (tipo == IPTV_M3U) ok = m3u_reservas_ler(r, principal, n);
  else if (tipo == IPTV_STALKER) ok = stalker_reservas_ler(r, principal, n);
  else ok = 0;
  return ok ? tipo : 0;
}

typedef struct { const char *entrada; int tipo, cheio, repetido; } Adicao;
static int fAdicionar(IptvReservas *r, const char *principal, void *u) {
  Adicao *a = u;
  char n[IPTVRES_URL];
  int ok = a->tipo == IPTV_XTREAM ? xtream_normalizar_servidor(a->entrada, n, sizeof n) :
           a->tipo == IPTV_M3U ? m3u_normalizar_reserva(a->entrada, n, sizeof n) :
           stalker_normalizar_portal(a->entrada, n, sizeof n);
  if (!ok) return 0;
  if (r->n >= IPTVRES_MAX) { a->cheio = 1; return 0; }
  if (!iptvres_adicionar(r, n, principal)) { a->repetido = 1; return 0; }
  return 1;
}
int iptvconta_reserva_adicionar(const char *entrada) {
  int tipo = tipoSelecionado();
  Mudar m = mudarDe(tipo);
  Adicao a = { entrada, tipo, 0, 0 };
  if (!m || !entrada) return 0;
  if (m(fAdicionar, &a)) return 1;
  return a.cheio ? -1 : a.repetido ? -2 : 0;
}

static int fRemover(IptvReservas *r, const char *principal, void *u) {
  (void)principal; return iptvres_remover(r, *(int *)u);
}
int iptvconta_reserva_remover(int k) {
  Mudar m = mudarDe(tipoSelecionado());
  return m ? m(fRemover, &k) : 0;
}

static int fUsar(IptvReservas *r, const char *principal, void *u) {
  (void)principal; return iptvres_definir_ativo(r, *(int *)u);
}
int iptvconta_reserva_usar(int k) {
  Mudar m = mudarDe(tipoSelecionado());
  return m ? m(fUsar, &k) : 0;
}

static int fAuto(IptvReservas *r, const char *principal, void *u) {
  (void)principal; r->autoTroca = *(int *)u ? 1 : 0; return 1;
}
int iptvconta_reserva_auto(int ligado) {
  Mudar m = mudarDe(tipoSelecionado());
  return m ? m(fAuto, &ligado) : 0;
}
