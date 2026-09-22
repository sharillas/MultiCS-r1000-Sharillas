///////////////////////////////////////////////////////////////////////////////
// NAGRA PROTECTION (caid 18xx/19xx/1a0x)
// v1.40: validacao estrutural - checksum das 4 quads + provider do perfil.
// (a validacao de ciclo moveu-se para o CYCLE ENGINE em setdcw.c)
// MultiCS r1000 - by Sharillas@2026
///////////////////////////////////////////////////////////////////////////////
#ifndef _NAGRA_H_
#define _NAGRA_H_

// codigos de retorno
#define NAGRA_OK      0
#define NAGRA_BADCHK  2
#define NAGRA_BADPROV 3

int nagra_check(ECM_DATA *ecm, uint8_t cw[16]);

#endif
