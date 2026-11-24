// Add-ons added by hand (src/addonslocais.c): normalising, persistence, limits.
#include "addonslocais.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char disco[65536]; static int tem, adicionados;
char *dados_ler(const char *n){(void)n;return tem?strdup(disco):NULL;}
int dados_gravar(const char *n,const char *c){(void)n;snprintf(disco,sizeof disco,"%s",c);tem=1;return 1;}
int dados_apagar(const char *n){(void)n;tem=0;disco[0]=0;return 1;}
int perfis_ativo(void){return 1;}
int addons_adicionar(const char *nome,const char *url){(void)nome;(void)url;adicionados++;return 1;}
int main(void){
  char u[2048];
  assert(addonslocais_normalizar("stremio://v3-cinemeta.strem.io/manifest.json",u,sizeof u)&&!strcmp(u,"https://v3-cinemeta.strem.io/manifest.json"));
  assert(addonslocais_normalizar("  torrentio.strem.fun  ",u,sizeof u)&&!strcmp(u,"https://torrentio.strem.fun/manifest.json"));
  assert(addonslocais_normalizar("http://192.168.1.5:7000/",u,sizeof u)&&!strcmp(u,"http://192.168.1.5:7000/manifest.json"));
  assert(addonslocais_normalizar("https://x.io/abc%7Cdef=/manifest.json?v=2",u,sizeof u)&&!strcmp(u,"https://x.io/abc%7Cdef=/manifest.json?v=2"));
  assert(!addonslocais_normalizar("",u,sizeof u)&&!addonslocais_normalizar("hello",u,sizeof u)&&!addonslocais_normalizar("a b.com",u,sizeof u));
  puts("ok  normaliza stremio://, sem esquema, LAN e URL com config");
  assert(addonslocais_adicionar("v3-cinemeta.strem.io")==1);
  assert(addonslocais_adicionar("https://v3-cinemeta.strem.io/manifest.json")==-2);
  assert(addonslocais_adicionar("nope")==0);
  assert(addonslocais_n()==1&&!strcmp(addonslocais_nome(0),"v3-cinemeta.strem.io")&&adicionados==1);
  assert(strstr(disco,"v3-cinemeta.strem.io\thttps://v3-cinemeta.strem.io/manifest.json\n"));
  for(int i=0;i<15;i++){char e[64];snprintf(e,sizeof e,"a%d.example.com",i);assert(addonslocais_adicionar(e)==1);}
  assert(addonslocais_adicionar("one-more.example.com")==-1);
  puts("ok  grava, recusa repetido e invalido, limite de 16");
  adicionados=0;addonslocais_carregar();assert(adicionados==16);
  assert(addonslocais_remover(0)&&addonslocais_n()==15&&!strstr(disco,"cinemeta"));
  for(int i=addonslocais_n()-1;i>=0;i--)assert(addonslocais_remover(i));
  assert(addonslocais_n()==0&&!tem);
  puts("ok  carrega tudo de novo na sessao; remover apaga do arquivo");
  puts("addonslocais: PASS");return 0;
}
