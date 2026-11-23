// Settings rows for the backup servers of a Live TV account (iptvconta_reserva.h):
// real navigation, keyboard results and the account file. No network.
#include "text.h"
#include "gfx.h"
#include "guia.h"
static void recorteTeste(float x,float y,float w,float h){(void)x;(void)y;(void)w;(void)h;}
static void semRecorteTeste(void){}
static int recargasTeste;
static void recarregarTeste(int limpar){(void)limpar;recargasTeste++;}
static GuiaCategoria categoriasTeste[2];
static int nCategoriasTeste, categoriaGravacoes;
static int categoriasLerTeste(GuiaCategoria *out,int max) {
  int n=out && max<nCategoriasTeste?max:nCategoriasTeste;
  if(out && n>0)memcpy(out,categoriasTeste,sizeof *out*(size_t)n);
  return n;
}
static int categoriaFalhar;
static int categoriasDefinirTeste(const GuiaCategoria *cs,int n) {
  if(categoriaFalhar)return 0;
  if(!n)return 1;
  for(int k=0;k<n;k++) {
    int achou=0;for(int i=0;i<nCategoriasTeste;i++)if(categoriasTeste[i].chave==cs[k].chave)achou=1;
    if(!achou)return 0;
  }
  for(int k=0;k<n;k++)for(int i=0;i<nCategoriasTeste;i++)if(categoriasTeste[i].chave==cs[k].chave)categoriasTeste[i].oculta=cs[k].oculta;
  categoriaGravacoes++;return 1;
}
#define guia_categorias categoriasLerTeste
#define guia_categorias_definir categoriasDefinirTeste
#define guia_recarregar recarregarTeste
#define gfx_recorte recorteTeste
#define gfx_sem_recorte semRecorteTeste
#include "../src/ajustes.c"
#undef guia_recarregar
#undef guia_categorias
#undef guia_categorias_definir
#undef gfx_recorte
#undef gfx_sem_recorte
#include <assert.h>
void teclado_teste_texto(const char *t);
void teclado_teste_foco(int f,int c);
static void key(SDL_Keycode k) {
  SDL_Event e;SDL_zero(e);e.type=SDL_KEYDOWN;e.key.keysym.sym=k;ajustes_evento(&e);
}
static int linha(int op) {
  for(int i=0;i<AJ_N_TELA;i++)if(TELA[i].tipo==IT_OPC && TELA[i].op==op)return i;
  assert(!"Settings row missing");return -1;
}
static void tocar(int op) {
  int i=linha(op);assert(visivel(i) && focavel(i));
  ajPonteiroItem(i,0);assert(focoOp==op);key(SDLK_RETURN);
}
static void salvarTeclado(const char *t) {
  assert(teclado_aberto());teclado_teste_texto(t);teclado_teste_foco(0,0);
  SDL_Event e;SDL_zero(e);e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_DOWN;
  for(int i=0;i<20;i++)teclado_evento(&e);
  e.key.keysym.sym=SDLK_RIGHT;
  for(int i=0;i<(teclado_mascarado()?3:2);i++)teclado_evento(&e);
  e.key.keysym.sym=SDLK_RETURN;teclado_evento(&e);assert(!teclado_aberto());
  ajustes_atualizar(0.016f,SDL_GetTicks());
}
static void campo(int op,const char *t) {
  if(teclado_aberto())assert(stCampo==op);else tocar(op);
  salvarTeclado(t);
}
static IptvConta selecionada(void){IptvConta c;assert(iptv_conta_selecionada(&c));return c;}
static void resumoContas(int quantas) {
  char texto[160],esperado[160];int antes=iptv_conta_indice();
  aj2MenuValor(AJS_TVAOVIVO,texto,sizeof texto);
  if(quantas)snprintf(esperado,sizeof esperado,"%s: %d",i18n("Contas de TV ao vivo"),quantas);
  else snprintf(esperado,sizeof esperado,"%s",i18n("Não configurado"));
  assert(!strcmp(texto,esperado) && iptv_conta_indice()==antes);
}
static int listaOp(int tipo){return AJ_IPTV_LISTA_XTREAM+tipo-IPTV_XTREAM;}
static void raiz(void) {
  for(int i=0;i<6 && (uxEditor || iptvForm.tipo || teclado_aberto());i++) {
    key(SDLK_ESCAPE);ajustes_atualizar(0.016f,SDL_GetTicks());
  }
  assert(!uxEditor && !iptvForm.tipo);
}
static void lista(int tipo){raiz();tocar(listaOp(tipo));assert(uxEditor && uxOp==listaOp(tipo));}
static void nova(int tipo) {
  int antes=iptv_contas(NULL,0),selecionadaAntes=iptv_conta_indice();lista(tipo);
  ajIptvToqueEscolher(nValores(uxOp)-1,0);
  assert(!uxEditor && iptvForm.nova && iptvForm.tipo==tipo && teclado_aberto());
  assert(stCampo==ajIptvPrimeiroCampo(tipo) && !teclado_texto()[0]);
  assert(!iptvForm.nome[0] && !iptvForm.servidor[0] && !iptvForm.url[0] && !iptvForm.senha[0]);
  assert(iptv_contas(NULL,0)==antes && iptv_conta_indice()==selecionadaAntes);
}
static void adicionar(int tipo,const char *url,const char *usuario) {
  int antes=iptv_contas(NULL,0);nova(tipo);
  campo(ajIptvPrimeiroCampo(tipo),url);
  if(tipo==IPTV_XTREAM){campo(AJ_XTREAM_USUARIO,usuario);campo(AJ_XTREAM_SENHA,"fixture");}
  if(tipo==IPTV_STALKER)campo(AJ_STALKER_MAC,"00:1a:79:00:00:01");
  assert(iptv_contas(NULL,0)==antes);tocar(AJ_IPTV_SALVAR);
  assert(iptv_contas(NULL,0)==antes+1 && !iptvForm.tipo && uxEditor && uxOp==listaOp(tipo));
}
static void conta(int tipo,int pos) {
  lista(tipo);ajIptvToqueEscolher(pos,0);
  assert(uxEditor && uxOp==AJ_IPTV_ACOES && !iptvForm.tipo);
}
static void editar(int tipo,int pos) {
  conta(tipo,pos);ajIptvToqueEscolher(0,0);
  assert(!uxEditor && iptvForm.tipo==tipo && !iptvForm.nova);
}
static void excluir(int tipo,int pos) {
  conta(tipo,pos);ajIptvToqueEscolher(1,0);
  assert(uxEditor && uxOp==AJ_IPTV_EXCLUIR && uxPendente==0);
  ajIptvToqueEscolher(1,0);assert(uxEditor && uxOp==listaOp(tipo));
}

static void ok2(const char *m){printf("PASS: %s\n",m);}
static void armarRemover(void){ tocar(AJ_IPTV_RES_REM); assert(sairArmado==AJ_IPTV_RES_REM); tocar(AJ_IPTV_RES_REM); }
int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);
  assert(getenv("NUVIO_DADOS"));assert(SDL_Init(SDL_INIT_TIMER)==0);
  dados_iniciar(getenv("NUVIO_DADOS"));ajustes_dir(getenv("NUVIO_DADOS"));
  ajustes_iniciar();layout_modo_definir(1);layout_tela_definir(1080,1920);gfx_opacidade_grupo=0;
  assert(dados_gravar("aviso-legal.txt","1\n"));

  // New account form: no backup rows until the account exists.
  nova(IPTV_XTREAM);
  for(int op=AJ_IPTV_RES_USO;op<=AJ_IPTV_RES_REM;op++)assert(!visivel(linha(op)));
  campo(AJ_XTREAM_SERVIDOR,"main.invalid:8080");campo(AJ_XTREAM_USUARIO,"user");campo(AJ_XTREAM_SENHA,"fixture");
  tocar(AJ_IPTV_SALVAR);assert(iptv_contas(NULL,0)==1);
  ok2("backup rows are hidden while a new account is being typed");

  // Existing account: only Add until there is a backup.
  editar(IPTV_XTREAM,0);
  assert(visivel(linha(AJ_IPTV_RES_ADD)) && !visivel(linha(AJ_IPTV_RES_USO)) && !visivel(linha(AJ_IPTV_RES_LISTA)));
  tocar(AJ_IPTV_RES_ADD);assert(teclado_aberto() && stCampo==AJ_IPTV_RES_ADD);salvarTeclado("backup1.invalid:8080");
  assert(resResultado==1 && !strcmp(textoLeitura(AJ_IPTV_RES_ADD),i18n("adicionado")));
  tocar(AJ_IPTV_RES_ADD);salvarTeclado("backup1.invalid:8080");assert(resResultado==4);
  tocar(AJ_IPTV_RES_ADD);salvarTeclado("main.invalid:8080");assert(resResultado==4);
  tocar(AJ_IPTV_RES_ADD);salvarTeclado("backup2.invalid");assert(resResultado==1);
  { IptvReservas r;char p[300];assert(iptvconta_reservas(&r,p,sizeof p)==IPTV_XTREAM && r.n==2 && !strcmp(p,"http://main.invalid:8080")); }
  assert(visivel(linha(AJ_IPTV_RES_USO)) && visivel(linha(AJ_IPTV_RES_AUTO)) && visivel(linha(AJ_IPTV_RES_LISTA)) && visivel(linha(AJ_IPTV_RES_REM)));
  ok2("Add opens the keyboard, normalises, refuses duplicates and the main address");

  // Server in use: main first; picking a backup saves it and reloads the guide.
  assert(nValores(AJ_IPTV_RES_USO)==3);
  { char esperado[200];snprintf(esperado,sizeof esperado,"%s \xc2\xb7 main.invalid:8080",i18n("Principal"));
    assert(!strcmp(textoValor(AJ_IPTV_RES_USO),esperado)); }
  int antes=recargasTeste;
  assert(definirValorDireto(AJ_IPTV_RES_USO,2));
  { char esperado[200];snprintf(esperado,sizeof esperado,"%s 2 \xc2\xb7 backup2.invalid",i18n("Reserva"));
    assert(!strcmp(textoValor(AJ_IPTV_RES_USO),esperado)); }
  assert(recargasTeste==antes+1);
  { IptvReservas r;iptvconta_reservas(&r,NULL,0);assert(r.ativo==2); }
  ok2("server in use is chosen by hand and shown as host only");

  // Automatic switching: on by default, off and back.
  assert(!strcmp(textoValor(AJ_IPTV_RES_AUTO),i18n("Ligado")));
  assert(definirValorDireto(AJ_IPTV_RES_AUTO,1));
  { IptvReservas r;iptvconta_reservas(&r,NULL,0);assert(!r.autoTroca); }
  assert(!strcmp(textoValor(AJ_IPTV_RES_AUTO),i18n("Desligado")));
  assert(definirValorDireto(AJ_IPTV_RES_AUTO,0));
  ok2("automatic switching toggles and is stored with the account");

  // Remove: needs a chosen backup and two OKs; removing the one in use goes back to main.
  assert(valor[AJ_IPTV_RES_LISTA]==2);   // the last one added is the chosen one
  assert(definirValorDireto(AJ_IPTV_RES_LISTA,0));assert(inativa(AJ_IPTV_RES_REM));
  assert(definirValorDireto(AJ_IPTV_RES_LISTA,2));assert(!inativa(AJ_IPTV_RES_REM));
  armarRemover();
  { IptvReservas r;iptvconta_reservas(&r,NULL,0);assert(r.n==1 && r.ativo==0 && !strcmp(r.url[0],"http://backup1.invalid:8080")); }
  assert(valor[AJ_IPTV_RES_LISTA]==0);
  ok2("remove asks twice and the server in use falls back to main");

  // The other account types.
  raiz();
  adicionar(IPTV_M3U,"http://m3u.invalid/get.php?username=u&password=secret",NULL);
  editar(IPTV_M3U,0);
  tocar(AJ_IPTV_RES_ADD);salvarTeclado("https://other.invalid/list.m3u");assert(resResultado==1);
  tocar(AJ_IPTV_RES_ADD);salvarTeclado("m3u2.invalid:8080");assert(resResultado==1);
  assert(definirValorDireto(AJ_IPTV_RES_LISTA,1));
  { const char *t=textoValor(AJ_IPTV_RES_LISTA);assert(strstr(t,"other.invalid") && !strstr(t,"list.m3u")); }
  { const char *t=textoValor(AJ_IPTV_RES_USO);assert(strstr(t,"m3u.invalid") && !strstr(t,"secret")); }
  raiz();
  adicionar(IPTV_STALKER,"portal.invalid:8080/c/",NULL);
  editar(IPTV_STALKER,0);
  tocar(AJ_IPTV_RES_ADD);salvarTeclado("backup-portal.invalid:8080/c/");assert(resResultado==1);
  { IptvReservas r;assert(iptvconta_reservas(&r,NULL,0)==IPTV_STALKER && r.n==1 && !strcmp(r.url[0],"http://backup-portal.invalid:8080")); }
  ok2("M3U (host or full link, never the query on screen) and Stalker portals");

  // Each account keeps its own list.
  raiz();editar(IPTV_XTREAM,0);
  { IptvReservas r;assert(iptvconta_reservas(&r,NULL,0)==IPTV_XTREAM && r.n==1); }
  raiz();
  puts("ajustes iptv reservas: PASS");
  return 0;
}
