// Android: an IPTV account with hundreds of live categories shows all of them
// (GUIACORTE_MAX_CAT), not just the first 128 in panel order.
#include "perfis.h"
#define NV_PERFIL_SESSAO_FORTE
#include "../src/guia.c"
#include <assert.h>
static const char *pasta;
int ajustes_livetv_sugerir(void){return 0;}
const char *livecat_secao(const char *n,int *r){(void)n;*r=0;return NULL;}
int xtream_e_id(const char *id){(void)id;return 0;}
uint64_t perfis_sessao_geracao(void){return 1;}
int perfis_sessao_atual(uint64_t s){return s==1;}
int perfis_sessao_travar(uint64_t s){return s==1;}
void perfis_sessao_destravar(void){}
int perfis_ativo(void){return 1;}
int addons_n(void){return 1;}
const char *addons_base(int i){(void)i;return "https://addon.invalid";}
int addons_ativo(int i){(void)i;return 1;}
const char *addons_nome(int i){(void)i;return "Addon";}
const char *i18n(const char *s){return s;}
int ajustes_guia_ordem(void){return 0;}
int ajustes_guia_fonte(void){return 0;}
int iptv_contas(IptvConta *out,int n){(void)out;(void)n;return 0;}
char *dados_ler(const char *n){(void)n;return NULL;}
int dados_gravar(const char *n,const char *s){(void)n;(void)s;return 1;}
char *dados_caminho(char *out,unsigned n,const char *nome){snprintf(out,n,"%s/%s",pasta,nome);return out;}
void marco(const char *s){(void)s;}

static void carregar(int total,int ng) {
 stagingZerar();sTeto=30000;
 int f=sFonte(LTV_ADDON,"https://addon.invalid","Addon","https://addon.invalid");assert(f>=0);
 for(int i=0;i<total;i++) {
  char id[80],nome[80],grupo[80];snprintf(id,sizeof id,"xtream:%d",i);snprintf(nome,sizeof nome,"Channel %d",i);snprintf(grupo,sizeof grupo,"Category %04d",i/(total/ng));
  GCru c={.id=id,.nome=nome,.grupo=grupo,.logo="",.desc="",.epgId="",.tvgNome="",.url="",.ua="",.ref="",.tipo="channel"};
  sGrupoInfo(f,grupo,1);sCanalAdd(f,&c);
 }

 adotarStaging();
}
int main(int argc,char **argv) {
 setvbuf(stdout,NULL,_IONBF,0);assert(argc==2);pasta=argv[1];
 carregar(10000,400);
 printf("categories shown: %d of 400, channels: %d of 10000\n",nCats,nCanais);
 assert(nCats==400 && nCanais==10000 && nLinhas()==400);
 carregar(30000,1500);
 printf("categories shown: %d of 1500, channels: %d of 30000\n",nCats,nCanais);
 assert(nCats==1500 && nCanais==30000);
 puts("guia categorias android: PASS");
 return 0;
}
