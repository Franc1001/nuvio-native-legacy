// M3U backup servers (src/m3u.c + src/iptvreserva.c): the playlist falls over
// to the backups, the one that answered becomes the server in use, channel
// URLs served from the account's own host can be tried on the other hosts.
#include "m3u.h"
#include "dados.h"
#include "rede.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>

static char pasta[512], ultima[1200], morto[2][64], pagina[64];
static int pedidos;
int perfis_ativo(void){return 1;}
uint64_t perfis_sessao_geracao(void){return 1;}
int perfis_sessao_atual(uint64_t s){return s==1;}
int perfis_sessao_travar(uint64_t s){return s==1;}
void perfis_sessao_destravar(void){}
char *dados_caminho(char *out,unsigned n,const char *nome){int k=snprintf(out,n,"%s/%s",pasta,nome);return k>=0&&(size_t)k<n?out:NULL;}
char *dados_ler(const char *nome){char cam[1024];assert(dados_caminho(cam,sizeof cam,nome));FILE *f=fopen(cam,"rb");if(!f)return NULL;
  fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);char *b=malloc((size_t)n+1);assert(b);assert(fread(b,1,(size_t)n,f)==(size_t)n);b[n]=0;fclose(f);return b;}
int dados_gravar(const char *nome,const char *body){char cam[1024];assert(dados_caminho(cam,sizeof cam,nome));FILE *f=fopen(cam,"wb");if(!f)return 0;size_t n=strlen(body);int ok=fwrite(body,1,n,f)==n;return fclose(f)==0&&ok;}
int dados_gravar_leve(const char *nome,const char *body){return dados_gravar(nome,body);}
int dados_apagar(const char *nome){char cam[1024];assert(dados_caminho(cam,sizeof cam,nome));return unlink(cam)==0;}
// Like an Xtream panel's get.php: the channels live on the host that was asked.
char *rede_baixar_bin_medido_controle(const char *url,int prazo,const char *const *cab,const RedeControle *ctl,long *bytes,RedeMedida *med){
  char host[256],*b;const char *h=strstr(url,"://")+3;size_t n=strcspn(h,"/?");(void)prazo;(void)cab;(void)ctl;
  pedidos++;snprintf(ultima,sizeof ultima,"%s",url);
  for(int i=0;i<2;i++)if(morto[i][0]&&strstr(url,morto[i])){med->status=0;*bytes=0;return NULL;}
  if(pagina[0]&&strstr(url,pagina)){med->status=200;b=strdup("<html>This site is blocked</html>");*bytes=(long)strlen(b);return b;}
  snprintf(host,sizeof host,"%.*s",(int)n,h);b=malloc(600);assert(b);
  snprintf(b,600,"#EXTM3U\n#EXTINF:-1 tvg-id=\"one\" group-title=\"News\",One\nhttp://%s/live/u/p/1.ts\n"
                 "#EXTINF:-1 tvg-id=\"two\" group-title=\"News\",Two\nhttps://cdn.elsewhere.invalid/two.m3u8\n",host);
  med->status=200;*bytes=(long)strlen(b);return b;
}
static int add(IptvReservas *r,const char *p,void *u){char n[1100];return m3u_normalizar_reserva(u,n,sizeof n)&&iptvres_adicionar(r,n,p);}
static int ativo(IptvReservas *r,const char *p,void *u){(void)p;return iptvres_definir_ativo(r,*(int*)u);}
static int autoT(IptvReservas *r,const char *p,void *u){(void)p;r->autoTroca=*(int*)u;return 1;}
int main(int argc,char **argv){
  M3uCanal c[8];IptvReservas r;char principal[1100],url[1100],id1[80],id2[80];int k,zero=0,um=1;
  assert(argc==2&&strlen(argv[1])<sizeof pasta);snprintf(pasta,sizeof pasta,"%s",argv[1]);assert(!mkdir(pasta,0700));
  assert(m3u_definir_lista("http://main.invalid:8080/get.php?username=u&password=p&type=m3u_plus"));
  assert(!m3u_reservas_mudar(add,"main.invalid:8080"));
  assert(m3u_reservas_mudar(add,"b1.invalid:8080"));
  assert(m3u_reservas_mudar(add,"https://full.invalid/other/list.m3u?k=1"));
  assert(!m3u_reservas_mudar(add,"bad host"));
  assert(m3u_reservas_ler(&r,principal,sizeof principal)&&r.n==2&&!strcmp(r.url[0],"http://b1.invalid:8080"));
  puts("ok  m3u reservas: host ou URL completa, sem repetir o principal");

  pedidos=0;assert(m3u_canais_sessao(c,8,1)==2&&pedidos==1&&strstr(ultima,"main.invalid"));
  snprintf(id1,sizeof id1,"%s",c[0].id);snprintf(id2,sizeof id2,"%s",c[1].id);
  // Main down: the bare-host backup keeps path and query of the main playlist.
  snprintf(morto[0],sizeof morto[0],"main.invalid");
  m3u_recarregar();pedidos=0;
  assert(m3u_canais_sessao(c,8,1)==2&&pedidos==2);
  assert(!strcmp(ultima,"http://b1.invalid:8080/get.php?username=u&password=p&type=m3u_plus"));
  assert(m3u_reservas_ler(&r,NULL,0)&&r.ativo==1);
  // The next refresh starts on b1: no time lost on the dead one.
  m3u_recarregar();pedidos=0;assert(m3u_canais_sessao(c,8,1)==2&&pedidos==1);
  puts("ok  m3u reservas: lista vem da reserva, que vira o servidor em uso");

  // A block page with HTTP 200 counts as down; a full-URL backup is used as typed.
  morto[0][0]=0;snprintf(pagina,sizeof pagina,"b1.invalid");snprintf(morto[1],sizeof morto[1],"main.invalid");
  m3u_recarregar();assert(m3u_canais_sessao(c,8,1)==2&&!strcmp(ultima,"https://full.invalid/other/list.m3u?k=1"));
  assert(m3u_reservas_ler(&r,NULL,0)&&r.ativo==2);
  pagina[0]=morto[1][0]=0;
  puts("ok  m3u reservas: pagina de bloqueio troca; reserva com URL completa vale como esta");

  // Channel on the account's own host: alternatives on the other hosts, in order.
  k=0;assert(m3u_reservas_mudar(ativo,&k));
  m3u_recarregar();assert(m3u_canais_sessao(c,8,1)==2);
  assert(m3u_url_reserva(id1,"http://main.invalid:8080/live/u/p/1.ts",1,url,sizeof url)&&!strcmp(url,"http://b1.invalid:8080/live/u/p/1.ts"));
  assert(m3u_url_reserva(id1,"nvproxy:http://main.invalid:8080/live/u/p/1.ts",2,url,sizeof url)&&!strcmp(url,"https://full.invalid/live/u/p/1.ts"));
  assert(!m3u_url_reserva(id1,"http://main.invalid:8080/live/u/p/1.ts",3,url,sizeof url));
  // Third-party CDN: nothing to swap.
  assert(!m3u_url_reserva(id2,"https://cdn.elsewhere.invalid/two.m3u8",1,url,sizeof url));
  // Played from b1: b1 becomes the server in use.
  m3u_canal_funcionou(id1,"http://b1.invalid:8080/live/u/p/1.ts");
  assert(m3u_reservas_ler(&r,NULL,0)&&r.ativo==1);
  m3u_canal_funcionou(id2,"https://cdn.elsewhere.invalid/two.m3u8");
  assert(m3u_reservas_ler(&r,NULL,0)&&r.ativo==1);
  puts("ok  m3u reservas: canal do proprio servidor ganha fontes nas reservas; CDN de terceiros nao");

  // Manual: only the chosen one.
  assert(m3u_reservas_mudar(autoT,&zero));
  snprintf(morto[0],sizeof morto[0],"b1.invalid");
  m3u_recarregar();pedidos=0;assert(m3u_canais_sessao(c,8,1)==2&&pedidos==1);  // cache kept, one try only
  assert(m3u_reservas_ler(&r,NULL,0)&&r.ativo==1);
  assert(!m3u_url_reserva(id1,"http://b1.invalid:8080/live/u/p/1.ts",1,url,sizeof url));
  assert(m3u_reservas_mudar(autoT,&um));morto[0][0]=0;
  puts("ok  m3u reservas: modo manual tenta so o escolhido e guarda a lista anterior");

  // New main playlist goes back to it; forgetting the account removes all.
  assert(m3u_definir_lista("http://newmain.invalid/get.php?x=1"));
  assert(m3u_reservas_ler(&r,NULL,0)&&r.ativo==0&&r.n==2);
  m3u_esquecer();assert(!m3u_reservas_ler(&r,NULL,0)&&r.n==0);
  puts("m3u reservas: PASS");
  return 0;
}
