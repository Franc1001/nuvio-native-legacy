#include "addonslocais.h"
#include "addons.h"
#include "dados.h"
#include "perfis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define URL_MAX 2048
typedef struct { char nome[96]; char url[URL_MAX]; } Local;
static Local lista[ADDONSLOCAIS_MAX];
static int n, perfilLido = -1;

static void arquivo(char *d, unsigned tam) { snprintf(d, tam, "addons-locais-p%d.txt", perfis_ativo()); }

int addonslocais_normalizar(const char *e, char *dst, unsigned tam) {
  char t[URL_MAX];
  size_t k;
  if (!dst || !tam) return 0;
  dst[0] = 0;
  if (!e) return 0;
  while (*e == ' ' || *e == '\t') e++;
  snprintf(t, sizeof t, "%s", e);
  k = strlen(t);
  while (k && (t[k - 1] == ' ' || t[k - 1] == '\n' || t[k - 1] == '\r' || t[k - 1] == '\t')) t[--k] = 0;
  if (!k || strpbrk(t, " \t\r\n")) return 0;
  const char *p = t, *esquema = "https://";
  if (!strncasecmp(p, "stremio://", 10)) p += 10;
  else if (!strncasecmp(p, "https://", 8)) p += 8;
  else if (!strncasecmp(p, "http://", 7)) { p += 7; esquema = "http://"; }
  if (!*p || *p == '/' || *p == '.') return 0;
  { const char *barra = strchr(p, '/'), *ponto = strchr(p, '.');
    // A host needs a dot (or a port, for a LAN add-on) before the path.
    if (!ponto || (barra && ponto > barra)) { const char *dp = strchr(p, ':'); if (!dp || (barra && dp > barra)) return 0; } }
  int w = snprintf(dst, tam, "%s%s", esquema, p);
  if (w <= 0 || (unsigned)w >= tam) { dst[0] = 0; return 0; }
  // A bare add-on address gets /manifest.json, the way Stremio installs it.
  if (!strstr(dst, "manifest.json")) {
    size_t m = strlen(dst);
    while (m && dst[m - 1] == '/') dst[--m] = 0;
    w = snprintf(dst + m, tam - m, "/manifest.json");
    if (w <= 0 || (unsigned)w >= tam - m) { dst[0] = 0; return 0; }
  }
  return 1;
}

static void hostDe(const char *url, char *d, unsigned tam) {
  const char *p = strstr(url, "://");
  p = p ? p + 3 : url;
  size_t k = strcspn(p, "/?#");
  if (k >= tam) k = tam - 1;
  memcpy(d, p, k); d[k] = 0;
}

static void ler(void) {
  char nome[64], *b, *l, *fim;
  if (perfilLido == perfis_ativo()) return;
  perfilLido = perfis_ativo(); n = 0;
  arquivo(nome, sizeof nome);
  b = dados_ler(nome);
  if (!b) return;
  for (l = b; *l && n < ADDONSLOCAIS_MAX; l = fim) {
    char *tab;
    fim = strchr(l, '\n');
    if (fim) *fim++ = 0; else fim = l + strlen(l);
    tab = strchr(l, '\t');
    if (!tab || l[0] == '#') continue;
    *tab++ = 0;
    if (!tab[0] || strlen(tab) >= URL_MAX) continue;
    snprintf(lista[n].nome, sizeof lista[n].nome, "%s", l);
    snprintf(lista[n].url, sizeof lista[n].url, "%s", tab);
    n++;
  }
  free(b);
}

static int gravar(void) {
  char nome[64];
  size_t tam = 256 + (size_t)n * (URL_MAX + 128), k = 0;
  char *t = malloc(tam);
  int ok;
  if (!t) return 0;
  k += (size_t)snprintf(t, tam, "# Add-ons added on this device (name<TAB>manifest URL).\n");
  for (int i = 0; i < n; i++) k += (size_t)snprintf(t + k, tam - k, "%s\t%s\n", lista[i].nome, lista[i].url);
  arquivo(nome, sizeof nome);
  ok = n ? dados_gravar(nome, t) : (dados_apagar(nome), 1);
  free(t);
  return ok;
}

void addonslocais_carregar(void) {
  int instalados = 0;
  perfilLido = -1;
  ler();
  for (int i = 0; i < n; i++) instalados += addons_adicionar(lista[i].nome, lista[i].url) > 0;
  if (n) { printf("[addons] %d de %d addon(s) desta TV instalado(s)\n", instalados, n); fflush(stdout); }
}

int addonslocais_adicionar(const char *entrada) {
  char url[URL_MAX];
  if (!addonslocais_normalizar(entrada, url, sizeof url)) return 0;
  ler();
  for (int i = 0; i < n; i++) if (!strcasecmp(lista[i].url, url)) return -2;
  if (n >= ADDONSLOCAIS_MAX) return -1;
  hostDe(url, lista[n].nome, sizeof lista[n].nome);
  snprintf(lista[n].url, sizeof lista[n].url, "%s", url);
  n++;
  if (!gravar()) { n--; return 0; }
  addons_adicionar(lista[n - 1].nome, url);
  return 1;
}

int addonslocais_n(void) { ler(); return n; }
const char *addonslocais_nome(int i) { ler(); return i >= 0 && i < n ? lista[i].nome : ""; }

int addonslocais_remover(int i) {
  Local tirado;
  ler();
  if (i < 0 || i >= n) return 0;
  tirado = lista[i];
  memmove(&lista[i], &lista[i + 1], sizeof *lista * (size_t)(n - i - 1));
  n--;
  if (!gravar()) {
    memmove(&lista[i + 1], &lista[i], sizeof *lista * (size_t)(n - i));
    lista[i] = tirado; n++;
    return 0;
  }
  return 1;
}
