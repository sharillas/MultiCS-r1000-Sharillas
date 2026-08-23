// BUILD LITE: lista de canais activos (CCcam.lite)
// formato: caid:provid:sid  (hex)  - comentarios com # ou ;
// wildcards: caid FFFE (ou 0) = qualquer caid para esse sid
//            provid 000000 = qualquer provid para esse caid/sid
// FTA (caid FFFF ou 0x0000) nao precisa de entrada (e ignorada pela lista)
// Se o ficheiro nao existir ou estiver vazio, o filtro deixa passar tudo.

struct lite_entry {
	uint16_t caid;
	uint32_t provid;
	uint16_t sid;
	struct lite_entry *next;
};

static struct lite_entry *lite_list = NULL;
static int lite_loaded = 0;      // ficheiro carregado (0 = deixa passar tudo)
static int lite_countentries = 0;

void lite_load()
{
	struct lite_entry *e = lite_list;
	while (e) { struct lite_entry *n = e->next; free(e); e = n; }
	lite_list = NULL;
	lite_countentries = 0;
	lite_loaded = 0;

	if (!cfg.lite_file[0]) return;

	FILE *fp = fopen(cfg.lite_file, "r");
	if (!fp) {
		mlogf(LOGINFO,getdbgflag(DBG_CONFIG,0,0)," lite: file %s not found (filter disabled)\n", cfg.lite_file);
		return;
	}

	char line[256];
	int nbline = 0;
	while (fgets(line, sizeof(line), fp)) {
		nbline++;
		strncpy( g_parse_filename, cfg.lite_file, sizeof(g_parse_filename)-1 );
		g_parse_filename[sizeof(g_parse_filename)-1] = 0;
		g_parse_nbline = nbline;
		char *p = line;
		while (*p==' '||*p=='\t') p++;
		if (!p[0] || p[0]=='#' || p[0]==';' || p[0]=='\n' || p[0]=='\r') continue;
		char *cmt = strchr(p, '#'); if (cmt) *cmt = 0;
		cmt = strchr(p, ';'); if (cmt) *cmt = 0;
		unsigned int caid=0, provid=0, sid=0;
		if (sscanf(p, "%x:%x:%x", &caid, &provid, &sid)!=3) {
			mlogf(LOGERROR,getdbgflag(DBG_CONFIG,0,0)," config(%d,%d): invalid lite line '%s'\n", nbline, 0, p);
			continue;
		}
		if (!sid) continue;
		if (caid==0xFFFF || caid==0) continue; // FTA nao entra na lista
		struct lite_entry *ne = (struct lite_entry *)calloc(1, sizeof(struct lite_entry));
		ne->caid = (uint16_t)caid;
		ne->provid = (uint32_t)provid;
		ne->sid = (uint16_t)sid;
		ne->next = lite_list;
		lite_list = ne;
		lite_countentries++;
	}
	fclose(fp);

	if (lite_countentries) {
		lite_loaded = 1;
		mlogf(LOGINFO,getdbgflag(DBG_CONFIG,0,0)," lite: loaded %d channels from %s\n", lite_countentries, cfg.lite_file);
	} else {
		mlogf(LOGINFO,getdbgflag(DBG_CONFIG,0,0)," lite: no channels in %s (filter disabled)\n", cfg.lite_file);
	}
}

int lite_count()
{
	return lite_countentries;
}

// retorna 1 = canal permitido / 0 = ignorado (fora da lista)
int lite_check( uint16_t caid, uint32_t provid, uint16_t sid )
{
	if (!lite_loaded) return 1;               // sem lista = sem filtro
	if (caid==0xFFFF || caid==0) return 1;    // FTA passa
	struct lite_entry *e = lite_list;
	while (e) {
		if (e->sid!=sid) { e = e->next; continue; }
		if (e->caid==0xFFFE || e->caid==0) return 1; // qualquer caid neste sid
		if (e->caid==caid) {
			if (e->provid==0 || e->provid==provid) return 1;
		}
		e = e->next;
	}
	return 0;
}
