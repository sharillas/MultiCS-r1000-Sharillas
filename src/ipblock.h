///////////////////////////////////////////////////////////////////////////////
// IPBLOCK - Lista de IPs bloqueados (pagina Iptables)
// MultiCS r1000 - by Sharillas@2026
///////////////////////////////////////////////////////////////////////////////
#ifndef _IPBLOCK_H_
#define _IPBLOCK_H_

#define IPBLOCK_MAX 512

struct ipblock_data
{
	uint32_t ip;
	uint32_t time; // GetTickCount quando foi bloqueado
};

extern struct ipblock_data ipblock_list[IPBLOCK_MAX];
extern int ipblock_count;

void ipblock_load();
void ipblock_save();
int ipblock_check(uint32_t ip);
void ipblock_add(uint32_t ip);
int ipblock_del(uint32_t ip);

#endif
