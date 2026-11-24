// Live TV with NO titles: a build without account and add-ons has an empty
// catalog, and only IPTV channels. cat_acrescentar refuses an empty catalog
// (other callers rely on that), so pressing a channel did nothing; the channel
// path (cat_acrescentar_canal) must still get an index.
#include "../src/catalogo.h"
#include "../src/progresso.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int         ajustes_idioma_ingles(void) { return 0; }
int ajustes_idioma(void) { return 0; }
const char *i18n(const char *s)         { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
const char *dados_dir(void)             { return ""; }
const char *sessao_usuario(void)        { return ""; }
int         perfis_ativo(void)          { return 1; }
const char *desc_genero_pt(const char *g) { return g; }
int prog_ler(ProgRegistro *saida, int max) { (void)saida; (void)max; return 0; }
int prog_gravar_local(const char *imdb, int t, int e, double p, double d, int tocou) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; (void)tocou; return 0;
}

int main(void) {
  CatItem c, d;
  memset(&c, 0, sizeof c); memset(&d, 0, sizeof d);
  snprintf(c.imdb, sizeof c.imdb, "xtream:1:123");
  snprintf(c.tipo, sizeof c.tipo, "channel");
  snprintf(c.titulo, sizeof c.titulo, "Channel One");
  snprintf(d.imdb, sizeof d.imdb, "xtream:1:456");
  snprintf(d.tipo, sizeof d.tipo, "channel");
  assert(cat_n() == 0);
  assert(cat_acrescentar(&c) < 0);            // other callers: unchanged
  int i = cat_acrescentar_canal(&c);
  assert(i == 0 && cat_n() == 1);
  assert(!strcmp(cat_item(i)->imdb, "xtream:1:123") && !strcmp(cat_item(i)->titulo, "Channel One"));
  assert(cat_indice_por_imdb("xtream:1:123") == 0);
  assert(cat_acrescentar_canal(&d) == 1 && cat_n() == 2);   // the next channel too
  puts("catcanalvazio: ok");
  return 0;
}
