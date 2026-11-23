#include "m3u.h"
#include "iptvid.h"
#include "iptvreserva.h"
#include "guiacorte.h"
#include "dados.h"
#include "perfis.h"
#include "perfisessao.h"
#include "rede.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <inttypes.h>
#include <time.h>
#include <sys/stat.h>

typedef struct { char id[80]; char *url, *cabecalhos; } M3uLigacao;
static void liberarLigacoes(M3uLigacao *v,int n) {
  for(int i=0;i<n;i++){free(v[i].url);free(v[i].cabecalhos);}free(v);
}
typedef struct FonteContexto {
  unsigned sid; struct FonteContexto *proximo;
  pthread_mutex_t trava_;
  pthread_mutex_t cargaTrava_;
  char lista_[1100];
  char epg_[1100];
  char epgDetectada_[1100];
  M3uLigacao *canais_;
  int nCanais_;
  int total_;
  int perfil_;
  int lido_;
  int renovarCache_;
  uint64_t sessaoLida_;
  uint64_t revisao_;
  time_t carregada_;
  volatile int cancelada_;
  IptvReservas reservas_;  // backup playlist addresses (iptvreserva.h)
} FonteContexto;
#define IPTV_CTX_INICIALIZAR(c) do { (c)->perfil_ = -1; iptvres_zerar(&(c)->reservas_); pthread_mutex_init(&(c)->trava_, NULL); pthread_mutex_init(&(c)->cargaTrava_, NULL); } while (0)
#define IPTV_CTX_EMERGENCIA { .sid = UINT_MAX, .perfil_ = -1, .reservas_ = { .autoTroca = 1 }, .trava_ = PTHREAD_MUTEX_INITIALIZER, .cargaTrava_ = PTHREAD_MUTEX_INITIALIZER }
// NV_TPK40: rebuild this unit with pthread storage (tools/tpk.sh).
#include "iptvcontexto.inc"
#define trava (contexto()->trava_ )
#define cargaTrava (contexto()->cargaTrava_ )
#define lista (contexto()->lista_ )
#define epg (contexto()->epg_ )
#define epgDetectada (contexto()->epgDetectada_ )
#define canais (contexto()->canais_ )
#define nCanais (contexto()->nCanais_ )
#define total (contexto()->total_ )
#define perfil (contexto()->perfil_ )
#define lido (contexto()->lido_ )
#define renovarCache (contexto()->renovarCache_ )
#define sessaoLida (contexto()->sessaoLida_ )
#define revisao (contexto()->revisao_ )
#define carregada (contexto()->carregada_ )
#define cancelada (contexto()->cancelada_ )
#define reservas (contexto()->reservas_ )

static uint64_t hash(const char *s) {
  uint64_t h=UINT64_C(14695981039346656037);
  for(;*s;s++){h^=(unsigned char)*s;h*=UINT64_C(1099511628211);}return h;
}
static void nomeConfig(char *s,unsigned n,int p) { if(contexto()->sid)snprintf(s,n,"m3u-p%d-s%u.txt",p,contexto()->sid);else snprintf(s,n,"m3u-p%d.txt",p); }
static void nomeCache(char *s,unsigned n,int p,const char *url) {
  if(contexto()->sid)snprintf(s,n,"m3u-cache-p%d-s%u-%016" PRIx64 ".txt",p,contexto()->sid,hash(url));
  else snprintf(s,n,"m3u-cache-p%d-%016" PRIx64 ".txt",p,hash(url));
}
static void limparEpg(int p) {
  static const char *ext[]={"bin","ts","id"};char nome[96];
  for(int i=0;i<3;i++) {
    snprintf(nome,sizeof nome,"epg-conta-2-%08x-p%d.%s",contexto()->sid,p,ext[i]);dados_apagar(nome);
    if(!contexto()->sid){snprintf(nome,sizeof nome,"epg-extra-m3u-p%d.%s",p,ext[i]);dados_apagar(nome);}
  }
}
static void invalidar(void) {
  cancelada=1;
  liberarLigacoes(canais,nCanais); canais=NULL; nCanais=total=0; carregada=0; epgDetectada[0]=0; renovarCache=0; revisao++;
}
static void carregar(void) {
  int p=perfis_ativo(); uint64_t sessao=nv_perfil_sessao_ler();
  if(lido&&perfil==p&&sessaoLida==sessao)return;
  lido=1;perfil=p;sessaoLida=sessao;invalidar();lista[0]=epg[0]=0;iptvres_zerar(&reservas);
  char nome[64];nomeConfig(nome,sizeof nome,p);
  char *body=dados_ler(nome);if(!body)return;
  for(char *line=body;*line;){char *next=strchr(line,'\n');if(next)*next++=0;else next=line+strlen(line);
    if(!strncmp(line,"lista\t",6)&&strlen(line+6)<sizeof lista)snprintf(lista,sizeof lista,"%s",line+6);
    else if(!strncmp(line,"epg\t",4)&&strlen(line+4)<sizeof epg)snprintf(epg,sizeof epg,"%s",line+4);
    else { char *tab=strchr(line,'\t'); if(tab){*tab=0;iptvres_ler_linha(&reservas,line,tab+1);} }
    line=next;
  }free(body);
  iptvres_ler_fim(&reservas);
}
// The config file: list, guide and the backup lines (iptvreserva.h). Caller
// holds `trava` and has checked the profile session.
static int gravarConfig(const char *url,const char *grade) {
  size_t tam=2400+(size_t)IPTVRES_MAX*(IPTVRES_URL+16);
  char *body=malloc(tam),nome[64];int ok;
  if(!body)return 0;
  nomeConfig(nome,sizeof nome,perfil);
  snprintf(body,tam,"lista\t%s\nepg\t%s\n",url,grade);
  ok=iptvres_gravar(&reservas,body,(unsigned)tam)&&dados_gravar(nome,body);
  memset(body,0,tam);free(body);return ok;
}
int m3u_url_valida(const char *s) {
  if(!s)return 0;
  if(!*s)return 1;
  if(strlen(s)>=sizeof lista||strpbrk(s,"\r\n\t "))return 0;
  return !strncmp(s,"http://",7)||!strncmp(s,"https://",8);
}
static int definir(const char *url,const char *grade) {
  if((url && !m3u_url_valida(url)) || (grade && !m3u_url_valida(grade)))return 0;
  pthread_mutex_lock(&trava);carregar();
  // A new playlist address is a deliberate choice: use it.
  int ativoAntes=reservas.ativo;
  if(url && strcmp(url,lista))reservas.ativo=0;
  int atual=nv_perfil_sessao_travar(sessaoLida);
  int ok=atual && gravarConfig(url?url:lista,grade?grade:epg);
  if(!ok)reservas.ativo=ativoAntes;
  if(ok){
    limparEpg(perfil);
    if (url && lista[0] && strcmp(lista,url)) {
      char cache[96];nomeCache(cache,sizeof cache,perfil,lista);
      dados_apagar(cache);
    }
    if(url)snprintf(lista,sizeof lista,"%s",url);
    if(grade)snprintf(epg,sizeof epg,"%s",grade);
    invalidar();
  }
  if(atual)nv_perfil_sessao_destravar();
  pthread_mutex_unlock(&trava);return ok;
}
int m3u_definir_lista(const char *s){return s?definir(s,NULL):0;}
int m3u_definir_epg(const char *s){return s?definir(NULL,s):0;}
int m3u_conta_definir(const char *s,const char *e){return s && *s && e?definir(s,e):0;}
void m3u_esquecer(void) {
  pthread_mutex_lock(&trava);carregar();
  if (nv_perfil_sessao_travar(sessaoLida)) {
    char nome[96];limparEpg(perfil);
    nomeCache(nome,sizeof nome,perfil,lista);
    dados_apagar(nome);nomeConfig(nome,sizeof nome,perfil);dados_apagar(nome);
    lista[0]=epg[0]=0;invalidar();iptvres_zerar(&reservas);
    nv_perfil_sessao_destravar();
  }
  pthread_mutex_unlock(&trava);
}
void m3u_esquecer_todos(void) {
  pthread_mutex_lock(&trava);
  invalidar();lista[0]=epg[0]=0;lido=0;iptvres_zerar(&reservas);
  // Matches the account cleanup's supported local profile slots.
  for (int p=0;p<=32;p++) {
    limparEpg(p);
    char nome[96];nomeConfig(nome,sizeof nome,p);
    char *body=dados_ler(nome);
    if(body) {
      char *u=!strncmp(body,"lista\t",6)?body+6:NULL;
      if(u) { char *fim=strchr(u,'\n');if(fim)*fim=0;
        nomeCache(nome,sizeof nome,p,u);dados_apagar(nome);
      }
      free(body);
    }
    nomeConfig(nome,sizeof nome,p);dados_apagar(nome);
  }
  pthread_mutex_unlock(&trava);
}
int m3u_configurado(void){pthread_mutex_lock(&trava);carregar();int r=lista[0]!=0;pthread_mutex_unlock(&trava);return r;}
const char *m3u_lista(void){static char s[1100];pthread_mutex_lock(&trava);carregar();snprintf(s,sizeof s,"%s",lista);pthread_mutex_unlock(&trava);return s;}
const char *m3u_epg(void){static char s[1100];pthread_mutex_lock(&trava);carregar();snprintf(s,sizeof s,"%s",epg);pthread_mutex_unlock(&trava);return s;}
int m3u_url_epg(char *s,unsigned n){pthread_mutex_lock(&trava);carregar();const char *u=epg[0]?epg:epgDetectada;int k=snprintf(s,n,"%s",u);pthread_mutex_unlock(&trava);return k>0&&(unsigned)k<n;}
int m3u_e_id(const char *id){unsigned f;char original[80];if(!iptvid_ler("nvm3u:",id,&f,original,sizeof original)||strlen(original)!=22)return 0;for(const char *p=original+6;*p;p++)if(!isxdigit((unsigned char)*p))return 0;return 1;}

// The playlist address of server k (iptvreserva.h): 0 is `lista`. A backup
// typed as a bare host ("backup.tv:8080") keeps the path and query of the main
// playlist, which is how providers' alternative DNS work; a backup typed as a
// full URL is used as it is. Caller holds `trava`.
static int listaDoServidor(int k,char *out,unsigned n) {
  const char *b=iptvres_servidor(&reservas,lista,k);
  int w;
  if(!b||!n)return 0;
  if(k>0&&iptvres_so_origem(b))return iptvres_trocar_origem(lista,b,out,n);
  w=snprintf(out,n,"%s",b);
  return w>0&&(unsigned)w<n;
}
// An answer is a playlist only if it looks like one: a 200 with an ISP block
// page or a panel error must count as "server down" so the next one is tried.
static int pareceM3u(const char *b) {
  if(!b)return 0;
  if((unsigned char)b[0]==0xEF&&(unsigned char)b[1]==0xBB&&(unsigned char)b[2]==0xBF)b+=3;
  while(*b&&(unsigned char)*b<=' ')b++;
  return !strncmp(b,"#EXTM3U",7)||strstr(b,"#EXTINF")!=NULL;
}
// Fetch/parse on the guide/resolver worker, never in a frame or Settings getter.
static int atualizar(uint64_t sessao,int renovar,M3uCanal *saida,int max) {
  char url[1100],nome[96],descoberta[1100];uint64_t rev;int p;
  // BACKUP SERVERS: the addresses to try, the one in use first.
  char (*urls)[1100]=NULL;int idx[IPTVRES_MAX+1],nUrls=0,usado=0;
  pthread_mutex_lock(&cargaTrava);
  pthread_mutex_lock(&trava);carregar();
  if(!nv_perfil_sessao_atual(sessao)||!lista[0]){pthread_mutex_unlock(&trava);pthread_mutex_unlock(&cargaTrava);return 0;}
  renovar = renovar || renovarCache;
  if(!saida&&!renovar&&canais&&time(NULL)-carregada<3600){int n=nCanais;pthread_mutex_unlock(&trava);pthread_mutex_unlock(&cargaTrava);return n;}
  urls=malloc(sizeof *urls*(IPTVRES_MAX+1));
  if(urls){
    int ordem[IPTVRES_MAX+1],k=iptvres_ordem(&reservas,lista,ordem);
    for(int i=0;i<k;i++)if(listaDoServidor(ordem[i],urls[nUrls],sizeof urls[nUrls]))idx[nUrls++]=ordem[i];
  }
  if(!nUrls){free(urls);pthread_mutex_unlock(&trava);pthread_mutex_unlock(&cargaTrava);return 0;}
  // The cache belongs to the account, not to the server that answered.
  snprintf(url,sizeof url,"%s",urls[0]);p=perfil;rev=revisao;cancelada=0;
  nomeCache(nome,sizeof nome,p,lista);
  pthread_mutex_unlock(&trava);
  char *body=NULL;
  if(!renovar){
    char caminho[1024];struct stat st;
    if(dados_caminho(caminho,sizeof caminho,nome)&&!stat(caminho,&st)&&time(NULL)-st.st_mtime<3600)body=dados_ler(nome);
  }
  int daRede=0;
  for(int i=0;!body&&i<nUrls&&!cancelada;i++){
    RedeControle ctl={0};RedeMedida med={0};long bytes=0;
    ctl.max_bytes=16*1024*1024;ctl.cancelado=&cancelada;
    body=rede_baixar_bin_medido_controle(urls[i],45,NULL,&ctl,&bytes,&med);
    if(!body||med.status<200||med.status>=300||med.limitado||bytes<=0){free(body);body=NULL;}
    else if(memchr(body,0,(size_t)bytes)){free(body);body=NULL;}
    else {char *b=realloc(body,(size_t)bytes+1);if(b){body=b;body[bytes]=0;daRede=1;}else{free(body);body=NULL;}}
    // A playlist too big for the limit is the account's size, not a dead
    // server: another address would send the same.
    if(body&&nUrls>1&&!pareceM3u(body)){free(body);body=NULL;daRede=0;}
    if(body){usado=i;snprintf(url,sizeof url,"%s",urls[i]);}
    else if(i+1<nUrls&&!med.limitado&&!cancelada){
      // Index only: playlist URLs carry the password.
      printf("[m3u] servidor %d de %d falhou (HTTP %d): tentando o proximo\n",i+1,nUrls,med.status);fflush(stdout);
    }
    else break;
  }
  if(!body)body=dados_ler(nome);
  M3uCanal *meta=body?calloc(GUIACORTE_MAX_CANAL,sizeof *meta):NULL;
  char *cache=body&&daRede?strdup(body):NULL;
  int totalNovo=0,n=meta?m3u_processar(body,url,meta,GUIACORTE_MAX_CANAL,descoberta,sizeof descoberta,&totalNovo):0;
  free(body);
  // A malformed HTTP 200 must preserve the previous playlist, too.
  if(!n&&daRede&&meta){
    free(cache);cache=NULL;daRede=0;body=dados_ler(nome);
    if(body)n=m3u_processar(body,url,meta,GUIACORTE_MAX_CANAL,descoberta,sizeof descoberta,&totalNovo);
    free(body);
  }
  M3uLigacao *novo=n?calloc((size_t)n,sizeof *novo):NULL;
  int alocadas=0;
  if(novo)for(int i=0;i<n;i++){
    snprintf(novo[i].id,sizeof novo[i].id,"%s",meta[i].id);
    novo[i].url=strdup(meta[i].url);novo[i].cabecalhos=strdup(meta[i].cabecalhos);alocadas=i+1;
    if(!novo[i].url||!novo[i].cabecalhos){n=0;break;}
  }
  if(!novo)n=0;
  pthread_mutex_lock(&trava);
  if(n>0&&rev==revisao&&p==perfil&&nv_perfil_sessao_travar(sessao)){
    if(daRede&&idx[usado]!=reservas.ativo&&iptvres_servidor(&reservas,lista,idx[usado])){
      int antes=reservas.ativo;reservas.ativo=idx[usado];
      if(!gravarConfig(lista,epg))reservas.ativo=antes;
      else {printf("[m3u] servidor em uso agora: %s\n",idx[usado]?"reserva":"principal");fflush(stdout);}
    }
    liberarLigacoes(canais,nCanais);canais=novo;novo=NULL;nCanais=n;total=totalNovo;
    carregada=daRede?time(NULL):(renovar?0:time(NULL));renovarCache=0;
    snprintf(epgDetectada,sizeof epgDetectada,"%s",descoberta);
    if(saida){if(n>max)n=max;memcpy(saida,meta,sizeof *saida*(size_t)n);}
    if(cache)dados_gravar_leve(nome,cache);
    nv_perfil_sessao_destravar();
    printf("[m3u] %d channel(s) loaded, %d declared\n",nCanais,totalNovo);fflush(stdout);
  } else n=0;
  pthread_mutex_unlock(&trava);
  liberarLigacoes(novo,novo?alocadas:0);free(meta);free(cache);free(urls);pthread_mutex_unlock(&cargaTrava);return n;
}
int m3u_canais_sessao(M3uCanal *out,int max,uint64_t sessao) {
  return !out||max<1?0:atualizar(sessao,0,out,max);
}
int m3u_ultimo_total(void){pthread_mutex_lock(&trava);int n=total;pthread_mutex_unlock(&trava);return n;}
static int resolverSessao(const char *id,char *out,unsigned n,uint64_t sessao,int renovar) {
  if(n)out[0]=0;
  if(!m3u_e_id(id)||!nv_perfil_sessao_atual(sessao))return 0;
  int pronto=0;
  pthread_mutex_lock(&trava);carregar();
  if(!renovar && nv_perfil_sessao_atual(sessao)) {
    for(int i=0;i<nCanais;i++)if(!strcmp(id,canais[i].id)) {
      int k=snprintf(out,n,"%s",canais[i].url);pronto=k>0&&(unsigned)k<n;break;
    }
  }
  pthread_mutex_unlock(&trava);
  if(pronto)return 1;
  if(!atualizar(sessao,renovar,NULL,0))return 0;
  pthread_mutex_lock(&trava);int ok=0;
  if(nv_perfil_sessao_atual(sessao)) {
    for(int i=0;i<nCanais;i++)if(!strcmp(id,canais[i].id)){
      int k=snprintf(out,n,"%s",canais[i].url);ok=k>0&&(unsigned)k<n;break;
    }
  }
  pthread_mutex_unlock(&trava);return ok;
}
static int cabecalhos(const char *id,char *out,unsigned n) {
  if(n)out[0]=0;
  pthread_mutex_lock(&trava);carregar();int ok=0;
  for(int i=0;i<nCanais;i++)if(!strcmp(id,canais[i].id)){int k=snprintf(out,n,"%s",canais[i].cabecalhos);ok=k>=0&&(unsigned)k<n;break;}
  pthread_mutex_unlock(&trava);return ok;
}
void m3u_recarregar(void){pthread_mutex_lock(&trava);carregar();carregada=0;renovarCache=1;pthread_mutex_unlock(&trava);}

void m3u_fonte_selecionar(unsigned id){atomic_store(&contaEditada,id);}
int m3u_fonte_configurado(unsigned id){unsigned a=usarFonte(id);int r=m3u_configurado();usarFonte(a);return r;}
int m3u_fonte_canais(unsigned id,M3uCanal *out,int max,uint64_t sessao){
  unsigned a=usarFonte(id);int n=m3u_canais_sessao(out,max,sessao);
  for(int i=0;i<n;i++){char original[80];snprintf(original,sizeof original,"%s",out[i].id);
    if(!iptvid_montar("nvm3u:",id,original,out[i].id,sizeof out[i].id)){n=i;break;}}
  usarFonte(a);return n;
}
int m3u_fonte_urls(unsigned id,char *url,unsigned nUrl,char *grade,unsigned nGrade) {
  if (!url || !nUrl || !grade || !nGrade) return 0;
  unsigned a=usarFonte(id);
  pthread_mutex_lock(&trava);carregar();
  int l=snprintf(url,nUrl,"%s",lista),e=snprintf(grade,nGrade,"%s",epg);
  int ok=l>0&&(unsigned)l<nUrl&&e>=0&&(unsigned)e<nGrade;
  if(!ok)url[0]=grade[0]=0;
  pthread_mutex_unlock(&trava);usarFonte(a);return ok;
}
// The guide downloads account playlists itself (guia.c m3uCarregar): it asks
// for the playlist of each server in the order of attempts and reports which
// one answered, so failover and "server in use" work there too.
int m3u_fonte_lista_em(unsigned id,int pos,char *url,unsigned n) {
  if(!url||!n||pos<0)return 0;
  url[0]=0;
  unsigned a=usarFonte(id);
  pthread_mutex_lock(&trava);carregar();
  int ordem[IPTVRES_MAX+1],k=lista[0]?iptvres_ordem(&reservas,lista,ordem):0;
  int ok=pos<k&&listaDoServidor(ordem[pos],url,n);
  if(!ok)url[0]=0;
  pthread_mutex_unlock(&trava);usarFonte(a);return ok;
}
void m3u_fonte_lista_usou(unsigned id,int pos) {
  unsigned a=usarFonte(id);
  pthread_mutex_lock(&trava);carregar();
  int ordem[IPTVRES_MAX+1],k=lista[0]?iptvres_ordem(&reservas,lista,ordem):0;
  if(pos>0&&pos<k&&reservas.ativo!=ordem[pos]&&nv_perfil_sessao_travar(sessaoLida)) {
    int antes=reservas.ativo;reservas.ativo=ordem[pos];
    if(!gravarConfig(lista,epg))reservas.ativo=antes;
    else {printf("[m3u] servidor em uso agora: %s\n",ordem[pos]?"reserva":"principal");fflush(stdout);}
    nv_perfil_sessao_destravar();
  }
  pthread_mutex_unlock(&trava);usarFonte(a);
}
int m3u_fonte_epg(unsigned id,char *out,unsigned n){unsigned a=usarFonte(id);int r=m3u_url_epg(out,n);usarFonte(a);return r;}
int m3u_fonte_total(unsigned id){unsigned a=usarFonte(id);int r=m3u_ultimo_total();usarFonte(a);return r;}
void m3u_fonte_recarregar(unsigned id){unsigned a=usarFonte(id);m3u_recarregar();usarFonte(a);}
void m3u_fonte_esquecer(unsigned id){unsigned a=usarFonte(id);m3u_esquecer();usarFonte(a);}
int m3u_resolver_sessao(const char *id,char *out,unsigned n,uint64_t sessao,int renovar){
  unsigned f;char original[80];if(!m3u_e_id(id)||!iptvid_ler("nvm3u:",id,&f,original,sizeof original))return 0;
  unsigned a=usarFonte(f);int r=resolverSessao(original,out,n,sessao,renovar);usarFonte(a);return r;
}
int m3u_cabecalhos(const char *id,char *out,unsigned n){
  unsigned f;char original[80];if(n)out[0]=0;
  if(!m3u_e_id(id)||!iptvid_ler("nvm3u:",id,&f,original,sizeof original))return 0;
  unsigned a=usarFonte(f);int r=cabecalhos(original,out,n);usarFonte(a);return r;
}

// --- backup servers --------------------------------------------------------
// Channel URL on another address of its account (same path and query, other
// host). Only for channels served from one of the account's own addresses: a
// playlist that points at third-party hosts has nothing to swap. `pos` 1 is
// the first alternative in the order of attempts.
static int urlReserva(const char *url,int pos,char *out,unsigned n) {
  char co[300],os[300],vistos[IPTVRES_MAX+1][300];
  int ordem[IPTVRES_MAX+1],k,achou=0,nv=0,alt=0,ok=0;
  if(n)out[0]=0;
  if(pos<1||!iptvres_origem(url,co,sizeof co))return 0;
  pthread_mutex_lock(&trava);carregar();
  k=iptvres_ordem(&reservas,lista,ordem);
  for(int i=0;i<k;i++){const char *srv=iptvres_servidor(&reservas,lista,ordem[i]);
    if(srv&&iptvres_origem(srv,os,sizeof os)&&!strcasecmp(os,co))achou=1;}
  for(int i=0;achou&&i<k&&!ok;i++){
    const char *srv=iptvres_servidor(&reservas,lista,ordem[i]);int repetido=0;
    if(!srv||!iptvres_origem(srv,os,sizeof os)||!strcasecmp(os,co))continue;
    for(int j=0;j<nv;j++)if(!strcasecmp(vistos[j],os))repetido=1;
    if(repetido)continue;
    snprintf(vistos[nv++],sizeof vistos[0],"%s",os);
    if(++alt==pos)ok=iptvres_trocar_origem(url,os,out,n);
  }
  pthread_mutex_unlock(&trava);return ok;
}
static void canalFuncionou(const char *url) {
  pthread_mutex_lock(&trava);carregar();
  int k=iptvres_indice_da_url(&reservas,lista,url);
  if(k>=0&&k!=reservas.ativo&&reservas.autoTroca&&nv_perfil_sessao_travar(sessaoLida)){
    int antes=reservas.ativo;reservas.ativo=k;
    if(!gravarConfig(lista,epg))reservas.ativo=antes;
    else {printf("[m3u] canal tocou pelo servidor %s: passa a ser o servidor em uso\n",k?"reserva":"principal");fflush(stdout);}
    nv_perfil_sessao_destravar();
  }
  pthread_mutex_unlock(&trava);
}
int m3u_url_reserva(const char *id,const char *url,int pos,char *out,unsigned n){
  unsigned f;if(n&&out)out[0]=0;
  if(!url||!out||!m3u_e_id(id)||!iptvid_ler("nvm3u:",id,&f,NULL,0))return 0;
  if(!strncmp(url,"nvproxy:",8))url+=8;
  unsigned a=usarFonte(f);int r=urlReserva(url,pos,out,n);usarFonte(a);return r;
}
void m3u_canal_funcionou(const char *id,const char *url){
  unsigned f;if(!url||!m3u_e_id(id)||!iptvid_ler("nvm3u:",id,&f,NULL,0))return;
  if(!strncmp(url,"nvproxy:",8))url+=8;
  unsigned a=usarFonte(f);canalFuncionou(url);usarFonte(a);
}
// Settings (selected account).
int m3u_reservas_ler(IptvReservas *out,char *principal,unsigned n) {
  pthread_mutex_lock(&trava);carregar();
  if(out)*out=reservas;
  if(principal&&n)snprintf(principal,n,"%s",lista);
  int ok=lista[0]!=0;pthread_mutex_unlock(&trava);return ok;
}
int m3u_reservas_mudar(int (*f)(IptvReservas *r,const char *principal,void *u),void *u) {
  IptvReservas *antes=f?malloc(sizeof *antes):NULL;int ok=0;
  if(!antes)return 0;
  pthread_mutex_lock(&trava);carregar();*antes=reservas;
  if(lista[0]&&f(&reservas,lista,u)) {
    int atual=nv_perfil_sessao_travar(sessaoLida);
    ok=atual&&gravarConfig(lista,epg);
    if(atual)nv_perfil_sessao_destravar();
    // The next refresh goes to the network: the list may now come from elsewhere.
    if(ok){carregada=0;renovarCache=1;}
  }
  if(!ok)reservas=*antes;
  pthread_mutex_unlock(&trava);free(antes);return ok;
}
// "backup.tv:8080" or a full playlist URL. No scheme means http.
int m3u_normalizar_reserva(const char *entrada,char *dst,unsigned n) {
  const char *p=entrada;int w;
  if(!dst||!n)return 0;
  dst[0]=0;
  if(!p)return 0;
  while(*p==' ')p++;
  if(!*p||strpbrk(p,"\r\n\t "))return 0;
  if(!strncmp(p,"http://",7)||!strncmp(p,"https://",8))w=snprintf(dst,n,"%s",p);
  else w=snprintf(dst,n,"http://%s",p);
  if(w<=0||(unsigned)w>=n||(unsigned)w>=sizeof lista||!m3u_url_valida(dst)){dst[0]=0;return 0;}
  { char o[300]; if(!iptvres_origem(dst,o,sizeof o)){dst[0]=0;return 0;} }
  return 1;
}

void m3u_fontes_invalidar(void) {
  pthread_mutex_lock(&fontesTrava);
  for(FonteContexto *c=fontesContextos;c;c=c->proximo){ pthread_mutex_lock(&c->trava_);c->cancelada_=1;c->revisao_++;c->lido_=0;
    liberarLigacoes(c->canais_,c->nCanais_);c->canais_=NULL;c->nCanais_=c->total_=0;c->carregada_=0;
    c->lista_[0]=c->epg_[0]=c->epgDetectada_[0]=0;iptvres_zerar(&c->reservas_);
    pthread_mutex_unlock(&c->trava_); }
  pthread_mutex_unlock(&fontesTrava);
}
