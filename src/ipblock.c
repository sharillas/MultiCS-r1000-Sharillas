///////////////////////////////////////////////////////////////////////////////
// IPBLOCK - Lista de IPs bloqueados
// Ficheiro: cfg.blockedip_file (BLOCKEDIP FILE: /var/etc/blocked_ips.cfg)
// Formato do ficheiro: um IP por linha (comentarios # ou ;)
///////////////////////////////////////////////////////////////////////////////

struct ipblock_data ipblock_list[IPBLOCK_MAX];
int ipblock_count = 0;

static pthread_mutex_t ipblock_mutex = PTHREAD_MUTEX_INITIALIZER;

void ipblock_load()
{
	ipblock_count = 0;
	memset(ipblock_list, 0, sizeof(ipblock_list));
	if (!cfg.blockedip_file[0]) return;
	FILE *fp = fopen(cfg.blockedip_file, "r");
	if (!fp) return;
	char line[256];
	while (fgets(line, sizeof(line), fp) && ipblock_count<IPBLOCK_MAX) {
		char *p = line;
		while (*p==' '||*p=='\t') p++;
		if (!p[0] || p[0]=='#' || p[0]==';' || p[0]=='\n' || p[0]=='\r') continue;
		// corta comentario
		char *cmt = strchr(p, '#');
		if (!cmt) cmt = strchr(p, ';');
		if (cmt) *cmt = 0;
		// corta \r\n
		char *nl = strchr(p, '\r');
		if (nl) *nl = 0;
		nl = strchr(p, '\n');
		if (nl) *nl = 0;
		uint32_t ip = inet_addr(p);
		if (ip==INADDR_NONE || ip==0) continue;
		int dup = 0;
		int i;
		for (i=0; i<ipblock_count; i++) {
			if (ipblock_list[i].ip==ip) { dup=1; break; }
		}
		if (!dup) {
			ipblock_list[ipblock_count].ip = ip;
			ipblock_list[ipblock_count].time = GetTickCount();
			ipblock_count++;
		}
	}
	fclose(fp);
	if (ipblock_count) mlogf(LOGINFO,DBG_CONFIG," ipblock: %d IPs bloqueados carregados de %s\n", ipblock_count, cfg.blockedip_file);
}

void ipblock_save()
{
	if (!cfg.blockedip_file[0]) return;
	pthread_mutex_lock(&ipblock_mutex);
	char tmp[512];
	snprintf(tmp, sizeof(tmp), "%s.tmp", cfg.blockedip_file);
	FILE *fp = fopen(tmp, "w");
	if (fp) {
		fprintf(fp, "# MultiCS r1000 - IPs bloqueados\n");
		fprintf(fp, "# Um IP por linha (gerido pela pagina Iptables)\n");
		int i;
		for (i=0; i<ipblock_count; i++) {
			fprintf(fp, "%s\n", (char*)ip2string(ipblock_list[i].ip));
		}
		fclose(fp);
		rename(tmp, cfg.blockedip_file);
	}
	pthread_mutex_unlock(&ipblock_mutex);
}

int ipblock_check(uint32_t ip)
{
	if (!ipblock_count) return 0;
	int i;
	for (i=0; i<ipblock_count; i++) {
		if (ipblock_list[i].ip==ip) return 1;
	}
	return 0;
}

void ipblock_add(uint32_t ip)
{
	if (!ip || ip==INADDR_NONE) return;
	pthread_mutex_lock(&ipblock_mutex);
	if (!ipblock_check(ip)) {
		if (ipblock_count>=IPBLOCK_MAX) {
			// remove o mais antigo
			int oldest = 0;
			int i;
			for (i=1; i<ipblock_count; i++)
				if (ipblock_list[i].time < ipblock_list[oldest].time) oldest = i;
			memmove(&ipblock_list[oldest], &ipblock_list[oldest+1], (ipblock_count-oldest-1)*sizeof(struct ipblock_data));
			ipblock_count--;
		}
		ipblock_list[ipblock_count].ip = ip;
		ipblock_list[ipblock_count].time = GetTickCount();
		ipblock_count++;
	}
	pthread_mutex_unlock(&ipblock_mutex);
	ipblock_save();
	mlogf(LOGINFO,DBG_CONFIG," ipblock: IP %s bloqueado\n", (char*)ip2string(ip));
}

int ipblock_del(uint32_t ip)
{
	int r = 0;
	pthread_mutex_lock(&ipblock_mutex);
	int i;
	for (i=0; i<ipblock_count; i++) {
		if (ipblock_list[i].ip==ip) {
			memmove(&ipblock_list[i], &ipblock_list[i+1], (ipblock_count-i-1)*sizeof(struct ipblock_data));
			ipblock_count--;
			r = 1;
			break;
		}
	}
	pthread_mutex_unlock(&ipblock_mutex);
	if (r) {
		ipblock_save();
		mlogf(LOGINFO,DBG_CONFIG," ipblock: IP %s desbloqueado\n", (char*)ip2string(ip));
	}
	return r;
}
