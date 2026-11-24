// Backup servers of the Live TV account selected in Settings, whatever its
// type (Xtream, M3U, Stalker). Thin dispatch over each backend's
// *_reservas_ler / *_reservas_mudar; the backend context must already point at
// the selected account (ajustes.c: ajIptvEspelhar).
#ifndef NV_IPTVCONTA_RESERVA_H
#define NV_IPTVCONTA_RESERVA_H
#include "iptvreserva.h"

// Copy of the list and the main address. Returns the account type
// (IPTV_XTREAM...) or 0 when no account is selected or it has no main address.
int iptvconta_reservas(IptvReservas *r, char *principal, unsigned n);
// 1 added; 0 invalid address; -1 the list is full; -2 already there (or it
// is the main address).
int iptvconta_reserva_adicionar(const char *entrada);
int iptvconta_reserva_remover(int k);      // k: 1..n
int iptvconta_reserva_usar(int k);         // k: 0 main, 1..n backup
int iptvconta_reserva_auto(int ligado);
#endif
