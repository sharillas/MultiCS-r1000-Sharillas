///////////////////////////////////////////////////////////////////////////////
// EMULATOR (CONSTCW / BISS)
// MultiCS r1000 - by Sharillas@2026
///////////////////////////////////////////////////////////////////////////////
#ifndef _EMU_H_
#define _EMU_H_

#define EMU_MAX_LOG 200

struct emu_key_data
{
	struct emu_key_data *next;
	uint16_t caid;
	uint32_t provid;
	uint16_t sid;
	uint8_t cw[16];
	char name[96];
	int manual;   // 1 = manual key (preserved)
	int hits;
	uint32_t lasttime;
};

struct emu_log_data
{
	uint32_t time;
	uint16_t caid;
	uint32_t provid;
	uint16_t sid;
	uint8_t cw[16];
};

extern struct emu_key_data *emu_keys;
extern int emu_keycount;
extern int emu_enabled;

extern struct emu_log_data emu_log[EMU_MAX_LOG];
extern int emu_logcount;
extern uint32_t emu_lastmatch;

void emu_init();
void emu_load();
void emu_save();
void emu_addkey(uint16_t caid, uint32_t provid, uint16_t sid, uint8_t cw[16], const char *name, int manual);
int emu_delkey(uint16_t caid, uint32_t provid, uint16_t sid);
int emu_has_constcw(uint16_t caid, uint32_t provid, uint16_t sid);
int emu_get_constcw(struct ecm_request *ecm);
int emu_parse_softcam(const char *data, int len);
int emu_log_get(int i, struct emu_log_data *out);

#endif
