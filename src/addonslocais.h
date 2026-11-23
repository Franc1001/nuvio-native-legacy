// ADD-ONS ADDED BY HAND ON THIS DEVICE (Stremio-compatible manifest URLs).
//
// Without a Nuvio account there is no add-on list to sync, so the app had no
// add-ons at all. These live per profile in addons-locais-p<N>.txt
// ("name<TAB>manifest URL" per line) and are installed into the running list
// (addons_adicionar) every time a profile session opens. With an account they
// add to what the account brings until the next pull replaces the list.
#ifndef NV_ADDONSLOCAIS_H
#define NV_ADDONSLOCAIS_H

#define ADDONSLOCAIS_MAX 16

// Install the saved ones into the live list. Called after addons_abrir_sessao.
void addonslocais_carregar(void);
// Normalise ("stremio://", no scheme, trailing spaces), save and install.
// 1 added; 0 invalid address; -1 list full; -2 already there.
int  addonslocais_adicionar(const char *entrada);
int  addonslocais_n(void);
// Display name: host only (the path of an add-on URL often carries its config
// or an API key).
const char *addonslocais_nome(int i);
int  addonslocais_remover(int i);
// Pure helper, for tests: the manifest URL an entry stands for. 1 if valid.
int  addonslocais_normalizar(const char *entrada, char *dst, unsigned tam);
#endif
