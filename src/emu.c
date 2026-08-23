///////////////////////////////////////////////////////////////////////////////
// EMULATOR (CONSTCW / BISS)
// Constant CW engine: carrega Softcam.cfg, casa SIDs nos ECMs e entrega CW
// local. Pagina web /emulator gere as chaves.
///////////////////////////////////////////////////////////////////////////////

struct emu_key_data *emu_keys = NULL;
int emu_keycount = 0;
int emu_enabled = 0;

struct emu_log_data emu_log[EMU_MAX_LOG];
int emu_logcount = 0;
uint32_t emu_lastmatch = 0;
static int emu_logidx = 0;

static pthread_mutex_t emu_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint8_t hexval(char c)
{
	if (c>='0'&&c<='9') return c-'0';
	if (c>='a'&&c<='f') return c-'a'+10;
	if (c>='A'&&c<='F') return c-'A'+10;
	return 0xff;
}

static int parsehex(const char *s, uint8_t *out, int maxlen)
{
	int n = 0;
	while (*s && n<maxlen) {
		if (*s==' '||*s=='\t') { s++; continue; }
		uint8_t hi = hexval(s[0]);
		uint8_t lo = hexval(s[1]);
		if (hi==0xff||lo==0xff) return -1;
		out[n++] = (hi<<4)|lo;
		s += 2;
	}
	return n;
}

static struct emu_key_data *emu_find(uint16_t caid, uint32_t provid, uint16_t sid)
{
	struct emu_key_data *k = emu_keys;
	while (k) {
		if (k->caid==caid && k->provid==provid && k->sid==sid) return k;
		k = k->next;
	}
	return NULL;
}

int emu_has_constcw(uint16_t caid, uint32_t provid, uint16_t sid)
{
	if (!emu_enabled) return 0;
	int r = 0;
	pthread_mutex_lock(&emu_mutex);
	if (emu_find(caid,provid,sid)) r = 1;
	pthread_mutex_unlock(&emu_mutex);
	return r;
}

int emu_get_constcw(struct ecm_request *ecm)
{
	if (!emu_enabled) return 0;
	int r = 0;
	pthread_mutex_lock(&emu_mutex);
	struct emu_key_data *k = emu_find(ecm->caid, ecm->provid, ecm->sid);
	if (k) {
		memcpy(ecm->cw, k->cw, 16);
		k->hits++;
		k->lasttime = GetTickCount();
		emu_lastmatch = GetTickCount();
		emu_log[emu_logidx].time = emu_lastmatch;
		emu_log[emu_logidx].caid = ecm->caid;
		emu_log[emu_logidx].provid = ecm->provid;
		emu_log[emu_logidx].sid = ecm->sid;
		memcpy(emu_log[emu_logidx].cw, k->cw, 16);
		emu_logidx = (emu_logidx+1) % EMU_MAX_LOG;
		if (emu_logcount < EMU_MAX_LOG) emu_logcount++;
		r = 1;
	}
	pthread_mutex_unlock(&emu_mutex);
	return r;
}

void emu_addkey(uint16_t caid, uint32_t provid, uint16_t sid, uint8_t cw[16], const char *name, int manual)
{
	pthread_mutex_lock(&emu_mutex);
	struct emu_key_data *k = emu_find(caid,provid,sid);
	if (k) {
		memcpy(k->cw, cw, 16);
		if (name && name[0]) strncpy(k->name, name, sizeof(k->name)-1);
		if (manual) k->manual = 1;
		pthread_mutex_unlock(&emu_mutex);
		return;
	}
	k = malloc(sizeof(struct emu_key_data));
	memset(k, 0, sizeof(struct emu_key_data));
	k->caid = caid;
	k->provid = provid;
	k->sid = sid;
	memcpy(k->cw, cw, 16);
	if (name && name[0]) strncpy(k->name, name, sizeof(k->name)-1);
	k->manual = manual;
	k->next = emu_keys;
	emu_keys = k;
	emu_keycount++;
	pthread_mutex_unlock(&emu_mutex);
}

int emu_delkey(uint16_t caid, uint32_t provid, uint16_t sid)
{
	int r = 0;
	pthread_mutex_lock(&emu_mutex);
	struct emu_key_data *k = emu_keys;
	struct emu_key_data *prev = NULL;
	while (k) {
		if (k->caid==caid && k->provid==provid && k->sid==sid) {
			if (prev) prev->next = k->next; else emu_keys = k->next;
			free(k);
			emu_keycount--;
			r = 1;
			break;
		}
		prev = k;
		k = k->next;
	}
	pthread_mutex_unlock(&emu_mutex);
	if (r) emu_save();
	return r;
}

// caminho efetivo do Softcam.cfg: o CONSTCW FILE da config, ou na falta
// desse, <pasta do multics.cfg>/Softcam.cfg (funciona em qualquer layout)
void emu_path(char *out, int outsz)
{
	if (cfg.constcw_file[0]) {
		snprintf(out, outsz, "%s", cfg.constcw_file);
		return;
	}
	const char *sl = strrchr(config_file, '/');
	if (sl) snprintf(out, outsz, "%.*s/Softcam.cfg", (int)(sl-config_file), config_file);
	else snprintf(out, outsz, "Softcam.cfg");
}

void emu_init()
{
	emu_keys = NULL;
	emu_keycount = 0;
	emu_logcount = 0;
	emu_logidx = 0;
	emu_lastmatch = 0;
	char p[512];
	emu_path(p, sizeof(p));
	if (cfg.constcw_file[0] || !access(p, F_OK)) emu_enabled = 1;
	else emu_enabled = 0;
}

static void emu_clean_name(char *s);

// formato Softcam.cfg: caid:provid:sid:CW32hex  (comentarios # ou ;)
// canal name: comentario "# nome" ou "; nome" na mesma linha
void emu_load()
{
	emu_init();
	if (!emu_enabled) return;
	char p[512];
	emu_path(p, sizeof(p));
	FILE *fp = fopen(p, "r");
	if (!fp) {
		mlogf(LOGINFO,DBG_CONFIG," emu: CONSTCW file %s not found\n", p);
		return;
	}
	char line[512];
	int manual_section = 0;
	char pending_name[96] = "";
	while (fgets(line, sizeof(line), fp)) {
		char *p = line;
		while (*p==' '||*p=='\t') p++;
		if (p[0]=='#') {
			if (!strncmp(p+1, " [MANUAL]", 9) || strstr(p, "Manual entries")) manual_section = 1;
			else {
				// nome do canal no comentario
				char *q = p+1;
				while (*q==' '||*q=='\t') q++;
				strncpy(pending_name, q, sizeof(pending_name)-1);
				pending_name[sizeof(pending_name)-1]=0;
				char *nl = strchr(pending_name,'\n'); if (nl) *nl=0;
				char *cr = strchr(pending_name,'\r'); if (cr) *cr=0;
				emu_clean_name(pending_name);
			}
			continue;
		}
		if (!p[0] || p[0]=='\n' || p[0]=='\r' || p[0]==';') continue;
		// remove comentario inline
		char *cmt = strchr(p, ';');
		if (!cmt) cmt = strchr(p, '#');
		char comment[96] = "";
		if (cmt) {
			*cmt = 0;
			char *q = cmt+1;
			while (*q==' '||*q=='\t') q++;
			strncpy(comment, q, sizeof(comment)-1);
			comment[sizeof(comment)-1]=0;
			char *nl = strchr(comment,'\n'); if (nl) *nl=0;
			char *cr = strchr(comment,'\r'); if (cr) *cr=0;
			emu_clean_name(comment);
		}
		unsigned int caid=0, provid=0, sid=0;
		char cwhex[64];
		if (sscanf(p, "%x:%x:%x:%63s", &caid, &provid, &sid, cwhex)!=4) continue;
		if (!caid || !cwhex[0]) continue;
		uint8_t cw[16];
		int n = parsehex(cwhex, cw, 16);
		if (n==8) {
			// half cw: duplica
			memcpy(cw+8, cw, 8);
		}
		else if (n!=16) continue;
		char name[96] = "";
		if (comment[0]) strncpy(name, comment, sizeof(name)-1);
		else if (pending_name[0]) strncpy(name, pending_name, sizeof(name)-1);
		emu_addkey((uint16_t)caid, provid, (uint16_t)sid, cw, name, manual_section);
		pending_name[0] = 0;
	}
	fclose(fp);
	mlogf(LOGINFO,DBG_CONFIG," emu: loaded %d constant CW keys from %s\n", emu_keycount, p);
}

void emu_save()
{
	char p[512];
	emu_path(p, sizeof(p));
	if (!cfg.constcw_file[0] && emu_keycount==0) return;
	pthread_mutex_lock(&emu_mutex);
	char tmp[512];
	snprintf(tmp, sizeof(tmp), "%s.tmp", p);
	FILE *fp = fopen(tmp, "w");
	if (!fp) {
		pthread_mutex_unlock(&emu_mutex);
		mlogf(LOGERROR,DBG_CONFIG," emu: SEM PERMISSOES para gravar %s (executa como root ou chmod 666)\n", p);
		return;
	}
	if (fp) {
		fprintf(fp, "# MultiCS r1000 Emulator - Constant CW keys\n");
		fprintf(fp, "# Auto-updated by MultiCS Emulator\n");
		fprintf(fp, "\n");
		fprintf(fp, "# BISS keys\n");
		struct emu_key_data *k = emu_keys;
		while (k) {
			if (!k->manual) {
				char cwhex[40] = "";
				int i;
				char *p = cwhex;
				for (i=0; i<16; i++) { sprintf(p, "%02X", k->cw[i]); p+=2; }
				if (k->name[0])
					fprintf(fp, "%04x:%06x:%04x:%s ; %s\n", k->caid, k->provid, k->sid, cwhex, k->name);
				else
					fprintf(fp, "%04x:%06x:%04x:%s\n", k->caid, k->provid, k->sid, cwhex);
			}
			k = k->next;
		}
		fprintf(fp, "\n# [MANUAL] - entries not in remote source (preserved forever)\n");
		k = emu_keys;
		while (k) {
			if (k->manual) {
				char cwhex[40] = "";
				int i;
				char *p = cwhex;
				for (i=0; i<16; i++) { sprintf(p, "%02X", k->cw[i]); p+=2; }
				if (k->name[0])
					fprintf(fp, "%04x:%06x:%04x:%s ; %s\n", k->caid, k->provid, k->sid, cwhex, k->name);
				else
					fprintf(fp, "%04x:%06x:%04x:%s\n", k->caid, k->provid, k->sid, cwhex);
			}
			k = k->next;
		}
		fclose(fp);
		if (rename(tmp, p)) {
			unlink(tmp);
			mlogf(LOGERROR,DBG_CONFIG," emu: nao consegui gravar %s (rename falhou - permissoes?)\n", p);
		}
	}
	pthread_mutex_unlock(&emu_mutex);
}

int emu_log_get(int i, struct emu_log_data *out)
{
	if (i<0 || i>=emu_logcount) return 0;
	int idx = (emu_logidx-1-i+EMU_MAX_LOG*2) % EMU_MAX_LOG;
	memcpy(out, &emu_log[idx], sizeof(struct emu_log_data));
	return 1;
}

// limpa o nome do canal: remove ';' e espacos no inicio e no fim
// (no SoftCam.Key o ';' fica colado ao nome e aparecia na GUI)
static void emu_clean_name(char *s)
{
	char *q = s;
	while (*q==' '||*q=='\t'||*q==';') q++;
	if (q!=s) memmove(s, q, strlen(q)+1);
	int l = (int)strlen(s);
	while (l>0 && (s[l-1]==' '||s[l-1]=='\t'||s[l-1]==';')) s[--l]=0;
}

// parse SoftCam.Key (formato CCcam "F xxxx TYPE KEY")
int emu_parse_softcam(const char *data, int len)
{
	int added = 0;
	char *buf = malloc(len+1);
	memcpy(buf, data, len);
	buf[len] = 0;
	char *line = buf;
	while (line && *line) {
		char *nl = strchr(line, '\n');
		if (nl) *nl = 0;
		char *cr = strchr(line, '\r');
		if (cr) *cr = 0;
		char *p = line;
		while (*p==' '||*p=='\t') p++;
		if ( (p[0]=='F'||p[0]=='f') && (p[1]==' '||p[1]=='\t') ) {
			// F <field1> <type> <key>  |  F <field1> <key>
			char field1[32], type[8] = "", key[64], rest[128] = "";
			int n = sscanf(p+1, "%31s %7s %63s %127s", field1, type, key, rest);
			if (n>=3) {
				// se type nao for 00/01 entao key=type, type=""
				if (strcmp(type,"00")&&strcmp(type,"01")&&strcmp(type,"0")&&strcmp(type,"1")) {
					strcpy(key, type);
					type[0] = 0;
					// rest fica o comentario
				}
				unsigned int sid = 0;
				int flen = strlen(field1);
				if (flen>=8) {
					// field1: XXXX1FFF (CCcam) -> sid = primeiros 4 hex
					if (!strcasecmp(field1+flen-4, "1FFF") && flen>=8) {
						char sidhex[8];
						memcpy(sidhex, field1+flen-8, 4);
						sidhex[4] = 0;
						sscanf(sidhex, "%x", &sid);
					}
					else {
						// ultimos 4 chars
						char sidhex[8];
						strncpy(sidhex, field1+flen-4, 4);
						sidhex[4] = 0;
						sscanf(sidhex, "%x", &sid);
					}
				}
				else if (flen>0) {
					// campo curto: usar todo como SID (ex: F 0003 01 KEY)
					sscanf(field1, "%x", &sid);
				}
				uint8_t cw[16];
				int nbytes = parsehex(key, cw, 16);
				if (nbytes==8) memcpy(cw+8, cw, 8);
				else if (nbytes!=16) nbytes = 0;
				if (sid && nbytes) {
					if (rest[0]) emu_clean_name(rest);
					emu_addkey(0x2600, 0, (uint16_t)sid, cw, rest[0]?rest:"", 0);
					added++;
				}
			}
		}
		else if ( (p[0]=='T'||p[0]=='t') && (p[1]==' '||p[1]=='\t') ) {
			// Tandberg: T <sid> <type> <key>  (caid 1010)
			char field1[32], type[8] = "", key[64], rest[128] = "";
			int n = sscanf(p+1, "%31s %7s %63s %127s", field1, type, key, rest);
			if (n>=3) {
				if (strcmp(type,"00")&&strcmp(type,"01")&&strcmp(type,"0")&&strcmp(type,"1")) {
					strcpy(key, type);
					type[0] = 0;
				}
				unsigned int sid = 0;
				sscanf(field1, "%x", &sid);
				uint8_t cw[16];
				int nbytes = parsehex(key, cw, 16);
				if (nbytes==8) memcpy(cw+8, cw, 8);
				else if (nbytes!=16) nbytes = 0;
				if (sid && nbytes) {
					if (rest[0]) emu_clean_name(rest);
					emu_addkey(0x1010, 0, (uint16_t)sid, cw, rest[0]?rest:"", 0);
					added++;
				}
			}
		}
		line = nl ? nl+1 : NULL;
	}
	free(buf);
	if (added) emu_save();
	return added;
}
