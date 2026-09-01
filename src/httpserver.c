#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

// protection.c (unidade do main.c) - prototipos usados na GUI
void prot_event_add(const char *fmt, ...);
char *prot_event_get(int n, unsigned int *age_ms);
unsigned int prot_uptime_ticks(void);
int dcw_filter_learned_count(void);

#ifdef WIN32

#include <windows.h>
#include <sys/types.h>
#include <sys/_default_fcntl.h>
#include <sys/poll.h>
#include <cygwin/types.h>
#include <cygwin/socket.h>
#include <sys/errno.h>
#include <cygwin/in.h>
#include <sched.h>
#include <netdb.h>
#include <netinet/tcp.h>

#else

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <poll.h>
#include <sys/prctl.h>
#include <poll.h>

#endif

#include "debug.h"
#include "convert.h"
#include "tools.h"
#include "threads.h"
#include "ecmdata.h"

#ifdef CCCAM
#include "msg-cccam.h"
#endif

#include "config.h"
#include "sockets.h"

#include "httpserver.h"
#include "httpbuffer.c"
#include "dyn_buffer.c"

#include "main.h"
#include "emu.h"
#include "ipblock.h"

const unsigned char *boyermoore_horspool_memmem(const unsigned char* haystack, ssize_t hlen, const unsigned char* needle, ssize_t nlen);

#include "images.c"
#include "country.c"
#include "httpstyle.c"



#define HTTP_GET  0
#define HTTP_POST 1
#define MAXHEADERS 20

struct cs_client_data *getnewcamdclientbyid(uint32_t id);
struct cccam_server_data *getcccamserverbyid(uint32_t id);
struct cc_client_data *getcccamclientbyid(uint32_t id);
struct cc_client_data *getcccamclientbyname(struct cccam_server_data *cccam, char *name);
#ifdef MGCAMD_SRV
struct mgcamdserver_data *getmgcamdserverbyid(uint32_t id);
struct mg_client_data *getmgcamdclientbyid(uint32_t id);
struct mg_client_data *getmgcamdclientbyname(struct mgcamdserver_data *mgcamd, char *name);
void mg_disconnect_cli(struct mg_client_data *cli);
#endif

#ifdef CS378X_SRV
struct camd35_client_data *getcs378xclientbyid(uint32_t id);
#endif

#ifdef CAMD35_SRV
struct camd35_client_data *getcamd35clientbyid(uint32_t id);
#endif


#define LIST_ACTIVE       0
#define LIST_CONNECTED    1
#define LIST_DISCONNECTED 2
#define LIST_ALL          3


#define ACTION_PAGE     0
#define ACTION_DIV      1
#define ACTION_ROW      2
#define ACTION_XML      3
#define ACTION_DISABLE  4
#define ACTION_ENABLE   5
#define ACTION_STATUS   6
#define ACTION_DEBUG    7
#define ACTION_UPDATE   8
#define ACTION_DBGINFO  9
#define ACTION_JSON     31



char HTTP_UPDATE_DIV[] = "\nvar autorefresh=%d;\nvar tautorefresh;\nfunction setautorefresh(t)\n{\n	clearTimeout(tautorefresh);\n	autorefresh = t;\n	if (t>0) tautorefresh = setTimeout('updateDiv()',autorefresh);\n}\nfunction updateDiv()\n{\n	var httpRequest;\n	try {\n		httpRequest = new XMLHttpRequest();  // Mozilla, Safari, etc\n	}\n	catch(trymicrosoft) {\n		try {\n			httpRequest = new ActiveXObject('Msxml2.XMLHTTP');\n		}\n		catch(oldermicrosoft) {\n			try {\n				httpRequest = new ActiveXObject('Microsoft.XMLHTTP');\n			}\n			catch(failed) {\n				httpRequest = false;\n			}\n		}\n	}\n	if (!httpRequest) {\n		alert('Your browser does not support Ajax.');\n		return false;\n	}\n	// Action http_request\n	httpRequest.onreadystatechange = function()\n	{\n		if (httpRequest.readyState == 4) {\n			if(httpRequest.status == 200) {\n				requestError=0;\n				document.getElementById('mainDiv').innerHTML = httpRequest.responseText;\n				if (window.bindSortable) bindSortable();\n			}\n			tautorefresh = setTimeout('updateDiv()',autorefresh);\n		}\n	}\n	httpRequest.open('GET', '%s',true);\n	httpRequest.send(null);\n}\n";
char HTTP_UPDATE_ROW[] = "\nvar idx = 0;\nvar tupdateRow;\n\nfunction setupdateRow(id)\n{\n	clearTimeout(tupdateRow);\n	idx = id;\n	if (id>0) tupdateRow = setTimeout('updateRow()',1000);\n}\n\nvar lastidx = 0;\nvar requestError = 0;\nfunction updateRow()\n{\n	if (lastidx!=idx) {\n		requestError = 0;\n		lastidx = idx;\n	}\n	if ( !requestError && (idx>0) ) {\n		var httpRequest;\n		try {\n			httpRequest = new XMLHttpRequest();  // Mozilla, Safari, etc\n		}\n		catch(trymicrosoft) {\n			try {\n				httpRequest = new ActiveXObject('Msxml2.XMLHTTP');\n			}\n			catch(oldermicrosoft) {\n				try {\n					httpRequest = new ActiveXObject('Microsoft.XMLHTTP');\n				}\n				catch(failed) {\n					httpRequest = false;\n				}\n			}\n		}\n		if (!httpRequest) {\n			alert('Your browser does not support Ajax.');\n			return false;\n		}\n		var savedidx = idx;\n		// Action http_request\n		httpRequest.onreadystatechange = function()\n		{\n			if (httpRequest.readyState == 4) {\n				if (httpRequest.status == 200) {\n					requestError=0;\n					xmlupdateRow( httpRequest.responseXML, 'Row'+savedidx );\n				}\n				else {\n					requestError++;\n				}\n				tupdateRow = setTimeout('updateRow()',1000);\n			}\n		}\n		httpRequest.open('GET', %s, true);\n		httpRequest.send(null);\n		requestError++;\n	}\n}\n";


int getcountryimage( char *code )
{
	int i;
	for(i=0; i<MAX_COUNTRY_IMAGES; i++) {
		if ( !strcmp(country_images[i].code, code) ) return i;
	}
	return -1;
}

void getcountryhtml( char *code, char *html )
{
	int i;
	for(i=0; i<MAX_COUNTRY_IMAGES; i++) {
		if ( !strcmp(country_images[i].code, code) ) {
			sprintf(html,"<img src='/flag%s.gif'>", code);
			return;
		}
	}
	sprintf(html, "[%s]", code);
}


char *getcountrycodebyip(uint32_t ip)
{
	struct ip2country_data *data= cfg.ip2country;
	ip = (ip>>24&0xFF)|(ip>>8&0xFF00)|(ip<<8&0xFF0000)|(ip<<24&0xFF000000);  // from little endian -> big endian
	while (data) {
		if ( (ip>=data->ipstart)&&(ip<=data->ipend) ) return data->code;
		data = data->next;
	}
	return NULL;
}

int isblockedip(uint32_t ip)
{
	if (ipblock_check(ip)) return 1; // lista de IPs bloqueados (Iptables)
	if (!cfg.blockcountry[0][0]) return 0; // accept
	char *p = getcountrycodebyip(ip);
	if (p) {
		int i;
		for(i=0; i<512; i++) {
			if (!cfg.blockcountry[i][0]) break;
			if ( !strcmp(cfg.blockcountry[i], p) ) return 1; // block
		}
	}
	return 0; // accept
}

char *getcountryname(char *code)
{
	int i;
	for(i=0; i<MAX_COUNTRY_IMAGES; i++) {
		if ( !strcmp(country_images[i].code, code) ) return country_images[i].name;
	}
	return NULL;
}

struct cachepeer_data *getpeerbyid(int id);
struct server_data *getsrvbyid(uint32_t id);
void cc_disconnect_cli(struct cc_client_data *cli);
char *src2string(int srctype, int srcid, char *ret);

typedef struct
{
	char name[256];
	char value[512];
} http_get;

typedef struct 
{
	int sock;
	uint32_t ip;
	struct dyn_buffer dbf;
	int type;//= (HTTP_GET/HTTP_POST)
	char path[512];
	char file[512];
	int http_version;//(0:1.0,1:1.1)
	char Host[100];//(localhost:9999)
	int Connection;//(1:keep-alive, 0:close);
	http_get getlist[MAXHEADERS];
	int getcount;
	http_get postlist[MAXHEADERS];
	int postcount;
	http_get headers[MAXHEADERS];
	int hdrcount;

} http_request;

void buf2str( char *dest, char *start, char *end)
{
  while (*start==' ') start++;
  while (start<=end)
  {
	*dest=*start;
	start++;
	dest++;
  }
  *dest='\0';
}

///////////////////////////////////////////////////////////////////////////////
char *isset_get(http_request *req, char *name)
{
  int i;
  char *n,*v;
  for(i=0; i<req->getcount; i++) {
    n = req->getlist[i].name;
    v = req->getlist[i].value;
    if (!strcmp(name, n)) {
		//printf("[$_GET] Name: '%s'    Value :'%s'\n", n,v);
		return v;
	}
  }
  return NULL;
}


///////////////////////////////////////////////////////////////////////////////
char *isset_header(http_request *req, char *name)
{
  int i;
  char *n,*v;
  //printf("Searching '%s'\n", name);
  for(i=0; i<req->hdrcount; i++) {
	//printf("[HEADER] Name: '%s'    Value :'%s'\n", req->headers[i].name, req->headers[i].value);
    n = req->headers[i].name;
    v = req->headers[i].value;
    if (!strcmp(name, n)) return v;
  }
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
void explode_get(http_request *req, char *get) // Get Variables
{
  char *end,*a;
  int i;
  i=0;
  //mlogf(LOGDEBUG,getdbgflag(DBG_HTTP,0,0),"explode_get()\n");
  while ( (end=strchr(get, '&')) ) 
  {
	*end = '\0';
        if (i>8) break; //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	if ( (a=strchr(get, '=')) ) {
	  *a='\0';
	  strncpy(req->getlist[i].name,get,255); 
	  strncpy(req->getlist[i].value,a+1,255);
	  //printf("$_GET['%s'] = '%s'\n",req->getlist[i].name,req->getlist[i].value);
	  i++;
	}
	get=end+1;
  }
  if ( (a=strchr(get, '=')) ) {
    *a='\0';
    strncpy(req->getlist[i].name,get,255);
    strncpy(req->getlist[i].value,a+1,255);
    //printf("$_GET['%s'] = '%s'\n",req->getlist[i].name,req->getlist[i].value);
    i++;
  }
  req->getcount=i;
}

///////////////////////////////////////////////////////////////////////////////
void explode_post(http_request *req, char *post)
{
  char *end,*a;
  int i;
  i=0;
  while ( (end=strchr(post, '&')) && i<MAXHEADERS ) 
  {
	*end = '\0';
	if ( (a=strchr(post, '=')) ) {
	  *a='\0';
	  strncpy(req->postlist[i].name,post,255); 
	  strncpy(req->postlist[i].value,a+1,255);
	  //printf("$_POST['%s'] = '%s'\n",req->postlist[i].name,req->postlist[i].value);
	  i++;
	}
	post=end+1;
  }
  if ( (a=strchr(post, '=')) && i<MAXHEADERS ) {
    *a='\0';
    strncpy(req->postlist[i].name,post,255);
    strncpy(req->postlist[i].value,a+1,255);
    i++;
  }
  req->postcount=i;
}

///////////////////////////////////////////////////////////////////////////////


int extractreq(http_request *req, char *buffer, int len )
{
	char *path_start, *path_end;
	char *rnrn, *slash;

	//printf("buffer size %d\n",len);
	//#Check Header
	if (!(rnrn=strstr( buffer, "\r\n\r\n"))) return -1;
	int reqsize = (rnrn-buffer)+4;
	//#Get Path
	path_start = buffer+4;
	while (*path_start==' ') path_start++;
	path_end=path_start;
	while (*path_end!=' ') path_end++;
	buf2str( req->path, path_start, path_end-1);
	//mlogf(LOGDEBUG,0, " HTTP PATH = '%s'\n", req->path);
	//#extract filename, and path
	slash = path_start = (char*)&req->path;
	while (*path_start) {
		if (*path_start=='/') slash=path_start;
		else if (*path_start=='?') {
			explode_get(req,path_start+1);
			*path_start='\0';
			break;
		}
		path_start++;
	}
	slash++;
	strncpy( req->file, slash, 100);
	//#Extract headers
	path_start = buffer+4;
	while ( (*path_start!='\r')&&(*path_start!='\n') ) path_start++;
	if (*path_start=='\r') path_start++;
	if (*path_start=='\n') path_start++;
	while ( path_start<rnrn && req->hdrcount<MAXHEADERS ) {
		// start = path_start
		//get end of line
		path_end = path_start;
		slash = NULL;
		while ( (*path_end!='\r')&&(*path_end!='\n')&&(*path_end!=0) ) {
			if (*path_end==':') if (!slash) slash = path_end;
			path_end++;
		}
		if (path_end==path_start) break; // end
		char tmp = *path_end;
		*path_end = 0;
		if (slash) {
			// Extract header name: value
			buf2str( req->headers[req->hdrcount].name , path_start, slash-1);
			buf2str( req->headers[req->hdrcount].value, slash+1, path_end-1);
			//printf(">> %s\n", path_start);
			//printf("[HEADER] Name: '%s'    Value :'%s'\n", req->headers[req->hdrcount].name, req->headers[req->hdrcount].value);
			//if ( !strcmp(req->headers[req->hdrcount].name,"Authorization") ) 
			req->hdrcount++;
		}
		*path_end = tmp;
		path_start = path_end;
		while ( (*path_start=='\r')||(*path_start=='\n') ) path_start++;
	}

	if ( !memcmp(buffer,"GET ",4) ) {
		//printf("requesttype = GET\n");
		req->type = HTTP_GET;
	}
	else if ( !memcmp(buffer,"HEAD ",5) ) {
		// HEAD tratado como GET
		req->type = HTTP_GET;
	}
	else if ( !memcmp(buffer,"POST",4) ) {
		//printf("requesttype = POST\n");
		req->type = HTTP_POST;
		int i;
		for(i=0; i<req->hdrcount; i++) {
			if ( !strcmp(req->headers[i].name,"Content-Length") ) {
				reqsize += atoi(req->headers[i].value);
				//printf("req size %d\n", reqsize);
				return reqsize;
			}
		}
	}
	return 0;
}




int parse_http_request(int sock, http_request *req )
{
	unsigned char buffer[2048]; // HTTP Header cant be greater than 1k
	int size;
	int totalsize = 0;
	memset(buffer,0,sizeof(buffer));
	memset(req,0, sizeof(http_request));
	size = recv( sock, buffer, sizeof(buffer), MSG_NOSIGNAL);
	if (size<10) return 0;
	totalsize += size;
	//printf("** Receiving %d bytes\n%s\n",size,buffer );
	if ( !memcmp(buffer,"GET ",4) || !memcmp(buffer,"HEAD ",5) || !memcmp(buffer,"POST",4) ) {
		buffer[size] = '\0';

		// Get Header
		while ( !strstr((char*)buffer, "\r\n\r\n") ) {
			struct pollfd pfd;
			pfd.fd = sock;
			pfd.events = POLLIN | POLLPRI;
			int retval = poll(&pfd, 1, 5000);
			if ( retval>0 )	{
				if ( pfd.revents & (POLLHUP|POLLNVAL) ) return 0; // Disconnect
				else if ( pfd.revents & (POLLIN|POLLPRI) ) {
					int len = recv(sock, (buffer+size), sizeof(buffer)-size, MSG_NOSIGNAL);
					//printf("** Receiving %d bytes\n",len );
					if (len<=0) return 0;
					size+=len;
					buffer[size]=0;
					totalsize += len;
				}
			}
			else if (retval==0) break;
			else return 0;
		}
		// Received Header
		//mlogf(LOGDEBUG,getdbgflag(DBG_HTTP,0,0)," Received Header >>>\n%s\n<<<\n", buffer);
		int ret = extractreq(req,(char*)buffer,size);
		if (ret==-1) return 0;
		//Get Data
		if (req->type==HTTP_POST) {
			char *rnrn = strstr((char*)buffer,"\r\n\r\n");
			int headerlen = rnrn ? (int)(rnrn-(char*)buffer)+4 : size;
			int firstchunk = 1;
			while (ret>totalsize) {
				//printf("Waiting....\n");
				struct pollfd pfd;
				pfd.fd = sock;
				pfd.events = POLLIN | POLLPRI;
				int retval = poll(&pfd, 1, 5000);
				if ( retval>0 )	{
					if ( pfd.revents & (POLLHUP|POLLNVAL) ) return 0; // Disconnect
					else if ( pfd.revents & (POLLIN|POLLPRI) ) {
						if (size>=sizeof(buffer)) {
							if (firstchunk) {
								dynbuf_write( &req->dbf, buffer+headerlen, size-headerlen);
								firstchunk = 0;
							} else dynbuf_write( &req->dbf, buffer, size);
							size = 0;
						}
						int len = recv(sock, (buffer+size), sizeof(buffer)-size, MSG_NOSIGNAL);
						//printf("** Receiving %d bytes\n",len );
						if (len<=0) return 0;
						size+=len;
						totalsize += len;
					}
				}
				else return 0;
			}
			if (size) {
				if (firstchunk) dynbuf_write( &req->dbf, buffer+headerlen, size-headerlen);
				else dynbuf_write( &req->dbf, buffer, size);
			}
			dynbuf_write( &req->dbf, (unsigned char*)"", 1 );
		}
	}
	else return 0;
	return 1;
}



/// XXX: not thread safe
char channelname[256];
char *getchname(uint16_t caid, uint32_t prov, uint16_t sid )
{
	struct chninfo_data *chn= cfg.chninfo;
	while (chn) {
		if ( (chn->caid==caid)&&(chn->prov==prov)&&(chn->sid==sid) ) return chn->name;
		chn = chn->next;
	}
	sprintf(channelname, "%04X:%06X:%04X", caid, prov, sid );
	return channelname;
}

struct chninfo_data *getchninfo(uint16_t caid, uint32_t prov, uint16_t sid )
{
	struct chninfo_data *chn= cfg.chninfo;
	while (chn) {
		if ( (chn->caid==caid)&&(chn->prov==prov)&&(chn->sid==sid) ) return chn;
		chn = chn->next;
	}
	return NULL;;
}

int total_profiles()
{
	int count=0;
	struct cardserver_data *cs = cfg.cardserver;
	while(cs) {
		// perfis internos nao aparecem na lista (DEFAULT e o emulador BISS)
		if ( strcmp(cs->name,"DEFAULT") && strcmp(cs->name,"BISS Emu") ) count++;
		cs = cs->next;
	}
	return count;
}	



int total_servers()
{
	int nb=0;
	struct server_data *srv=cfg.server;
	while (srv) {
		nb++;
		srv=srv->next;
	}
	return nb;
}

int connected_servers()
{
	int nb=0;
	struct server_data *srv=cfg.server;
	while (srv) {
		if ( !IS_DISABLED(srv->flags)&&(srv->handle>0) ) nb++;
		srv=srv->next;
	}
	return nb;
}

int total_cs_clients(uint8_t type)
{
	int nb=0;
	struct cardserver_data *cs=cfg.cardserver;
	while (cs) {
		struct cs_client_data *cli=cs->newcamd.client;
		while (cli) {
			if (cli->type==type) nb++;
			cli=cli->next;
		}
		cs=cs->next;
	}
	return nb;
}

int connected_cs_clients(uint8_t type)
{
	int nb=0;
	struct cardserver_data *cs=cfg.cardserver;
	while (cs) {
		struct cs_client_data *cli=cs->newcamd.client;
		while (cli) {
			if (cli->type==type && cli->connection.status>0) nb++;
			cli=cli->next;
		}
		cs=cs->next;
	}
	return nb;
}

#ifdef CCCAM_SRV
int total_cc_clients()
{
	int nb=0;
	struct cccam_server_data *ccsrv=cfg.cccam.server;
	while (ccsrv) {
		struct cc_client_data *cli=ccsrv->client;
		while (cli) { nb++; cli=cli->next; }
		ccsrv=ccsrv->next;
	}
	return nb;
}
int connected_cc_clients()
{
	int nb=0;
	struct cccam_server_data *ccsrv=cfg.cccam.server;
	while (ccsrv) {
		struct cc_client_data *cli=ccsrv->client;
		while (cli) { if (cli->connection.status>0) nb++; cli=cli->next; }
		ccsrv=ccsrv->next;
	}
	return nb;
}
#endif

#ifdef MGCAMD_SRV
int total_mg_clients()
{
	int nb=0;
	struct mgcamdserver_data *mgsrv=cfg.mgcamd.server;
	while (mgsrv) {
		struct mg_client_data *cli=mgsrv->client;
		while (cli) { nb++; cli=cli->next; }
		mgsrv=mgsrv->next;
	}
	return nb;
}
int connected_mg_clients()
{
	int nb=0;
	struct mgcamdserver_data *mgsrv=cfg.mgcamd.server;
	while (mgsrv) {
		struct mg_client_data *cli=mgsrv->client;
		while (cli) { if (cli->connection.status>0) nb++; cli=cli->next; }
		mgsrv=mgsrv->next;
	}
	return nb;
}
#endif

#ifdef CAMD35_SRV
int total_c35_clients()
{
	int nb=0;
	struct camd35_server_data *c35srv=cfg.camd35.server;
	while (c35srv) {
		struct camd35_client_data *cli=c35srv->client;
		while (cli) { nb++; cli=cli->next; }
		c35srv=c35srv->next;
	}
	return nb;
}
#endif

#ifdef CS378X_SRV
int total_cs378x_nb()
{
	int nb=0;
	struct camd35_server_data *csxsrv=cfg.cs378x.server;
	while (csxsrv) {
		struct camd35_client_data *cli=csxsrv->client;
		while (cli) { nb++; cli=cli->next; }
		csxsrv=csxsrv->next;
	}
	return nb;
}
#endif

#ifdef CACHEEX
int total_cacheex_servers()
{
	int nb=0;
	struct server_data *srv=cfg.cacheexserver;
	while (srv) {
		if (!(srv->flags&FLAG_DELETE)) if (srv->cacheex_mode) nb++;
		srv=srv->next;
	}
	struct cccam_server_data *cccam=cfg.cccam.server;
	while (cccam) {
		struct cc_client_data *cli=cccam->cacheexclient;
		while (cli) {
			if (cli->cacheex_mode) nb++;
			cli=cli->next;
		}
		cccam=cccam->next;
	}
#ifdef CAMD35_SRV
	struct camd35_server_data *c35=cfg.camd35.server;
	while (c35) {
		struct camd35_client_data *cli=c35->cacheexclient;
		while (cli) {
			if (cli->cacheex_mode) nb++;
			cli=cli->next;
		}
		c35=c35->next;
	}
#endif
#ifdef CS378X_SRV
	struct camd35_server_data *c37=cfg.cs378x.server;
	while (c37) {
		struct camd35_client_data *cli=c37->cacheexclient;
		while (cli) {
			if (cli->cacheex_mode) nb++;
			cli=cli->next;
		}
		c37=c37->next;
	}
#endif
	return nb;
}
#endif

int connected_all_clients()
{
	int nb=0;
	struct cardserver_data *cs=cfg.cardserver;
	while (cs) {
		struct cs_client_data *cli=cs->newcamd.client;
		while (cli) {
			if (cli->connection.status>0) nb++;
			cli=cli->next;
		}
		cs=cs->next;
	}
#ifdef CCCAM_SRV
	nb += connected_cc_clients();
#endif
#ifdef MGCAMD_SRV
	nb += connected_mg_clients();
#endif
	return nb;
}

struct server_data *getserverbyid(uint32_t id)
{
	struct server_data *srv=cfg.server;
	while (srv) {
		if (srv->id==id) return srv;
		srv=srv->next;
	}
	return NULL;
}

int get_editor_file_index(const char *needle)
{
	struct filename_data *fs = cfg.files;
	int i = 0;
	while (fs) {
		if (!fs->noeditor) {
			if (strstr(fs->name, needle)) return i;
			i++;
		}
		fs = fs->next;
	}
	return -1;
}

/*
int totalcachepeers()
{
	struct cachepeer_data *peer;
	int count=0;

	peer = cfg.cache.peer;
	while (peer) {
		count++;
		peer = peer->next;
	}
	return count;
}
*/

void cache_peers( struct cacheserver_data *cache, int *total, int *active )
{
	*total = 0;
	*active = 0;
	struct cachepeer_data *peer = cache->peer;
	while (peer) {
		(*total)++;
		if ( peer->ping>0 ) (*active)++;
		peer=peer->next;
	}
}

void total_cache_peers( int *total, int *active )
{
	*total = 0;
	*active = 0;
	struct cacheserver_data *cache = cfg.cache.server;
	while (cache) {
		struct cachepeer_data *peer = cache->peer;
		while (peer) {
			(*total)++;
			if ( peer->ping>0 ) (*active)++;
			peer=peer->next;
		}
		cache = cache->next;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//color: #000000; background-color: #FFFFFF;
char http_replyok[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nCache-Control: no-cache, no-store, must-revalidate\r\nConnection: close\r\n\r\n";

char http_html[] = "<HTML>\n";
char http_html_[] = "</HTML>\n";

char http_head[] = "<HEAD>\n";
char http_head_[] = "</HEAD>\n";

char http_body[] = "<BODY>\n";
char http_body_[] = "</BODY>\n";

char html_title[] = "<title>%s - %s</title>\n";

char http_link[] = "<meta http-equiv=\"Content-type\" content=\"text/html; charset=utf-8\"/>\n";

char http_style[] = "<link rel=\"stylesheet\" href=\"style.css?v=1109\" type=\"text/css\" />\n";

char http_javascript[] = "<script src=\"/customjs.js\"></script>\n";

#define PAGE_HOME      1
#define PAGE_SERVERS   2
#define PAGE_CACHE     3
#define PAGE_PROFILES  4
#define PAGE_NEWCAMD   5
#define PAGE_CCCAM     6
#define PAGE_FREECCCAM 7
#define PAGE_MGCAMD    8
#define PAGE_EDITOR    9
#define PAGE_RESTART   10
#define PAGE_CACHEEX   11

#define PAGE_CAMD35    12
#define PAGE_CS378X    13
#define PAGE_DEBUG     14
#define PAGE_EMULATOR  15
#define PAGE_IPTABLES  16
#define PAGE_CONFIGURATIONS 17
#define PAGE_PACKAGES  18


char *yesno( int a )
{
	static char yes[] ="YES";
	static char no[] ="NO";
	if (a) return yes; else return no;
}

char *onoff( int a )
{
	static char yes[] ="ON";
	static char no[] ="OFF";
	if (a) return yes; else return no;
}

int unreadsms()
{
	int nb = 0;
	struct cacheserver_data *cache = cfg.cache.server;
	while (cache) {
		struct cachepeer_data *peer = cache->peer;
		while (peer) {
			if ( (peer->sms)&&(peer->sms->status==0) ) nb++;
			peer = peer->next;
		}
		cache = cache->next;
	}
	return nb;
}		

void tcp_write_menu(struct tcp_buffer_data *tcpbuf, int sock, int selected)
{
	char *cNormal = "<li><a class='mbtn' href='%s'>%s</a></li>";
	char *cSelected = "<li><a class='mbtn on' href='%s'>%s</a></li>";
	char *cDisabled = "<li><a class='mbtn off' href='%s'>%s</a></li>";
	char *class;
	char buf[512];
	char label[128];

	tcp_writestr(tcpbuf, sock, "<div class=menu><ul>" );
	// Dashboard
	if (selected==PAGE_HOME) class = cSelected; else class = cNormal;
	sprintf( buf, class, "/dashboard", "Dashboard"); tcp_writestr(tcpbuf, sock, buf);

	// Servers
	if ( !cfg.http.show.noservers ) {
		if (cfg.server!=NULL) {
			if (selected==PAGE_SERVERS) class = cSelected; else class = cNormal;
		} else class = cDisabled;
		sprintf( label, "Servers [<span class='badge-count'> %d </span>]", total_servers());
		sprintf( buf, class, "/servers", label); tcp_writestr(tcpbuf, sock, buf);
	}
	// Cache
	if ( !cfg.http.show.nocache ) {
		if (cfg.cache.server) {
			if (selected==PAGE_CACHE) class = cSelected; else class = cNormal;
		} else class = cDisabled;
		sprintf( label, "Cache [<span class='badge-count'> %d </span>]", cfg.cache.totalservers);
		sprintf( buf, class, "/cache", label); tcp_writestr(tcpbuf, sock, buf);
	}
#ifdef CACHEEX
	// CacheEX
	if ( !cfg.http.show.nocacheex ) {
		if (selected==PAGE_CACHEEX) class = cSelected; else class = cNormal;
		sprintf( label, "CacheEX [<span class='badge-count'> %d </span>]", total_cacheex_servers());
		sprintf( buf, class, "/cacheex", label); tcp_writestr(tcpbuf, sock, buf);
	}
#endif
	// Newcamd
	if ( !cfg.http.show.nonewcamd ) {
		if (cfg.cardserver!=NULL) {
			if (selected==PAGE_NEWCAMD) class = cSelected; else class = cNormal;
		} else class = cDisabled;
		sprintf( label, "Newcamd [<span class='badge-count'> %d </span>]", total_cs_clients(TYPE_NEWCAMD));
		sprintf( buf, class, "/newcamd", label); tcp_writestr(tcpbuf, sock, buf);
	}
#ifdef MGCAMD_SRV
	if ( !cfg.http.show.noservers && (cfg.mgcamd.server!=NULL) ) {
		if (selected==PAGE_MGCAMD) class = cSelected; else class = cNormal;
		sprintf( label, "Mgcamd [<span class='badge-count'> %d </span>]", total_mg_clients());
		sprintf( buf, class, "/mgcamd", label); tcp_writestr(tcpbuf, sock, buf);
	}
#endif

#ifdef CCCAM_SRV
	// CCcam
	if ( !cfg.http.show.nocccam && (cfg.cccam.server!=NULL) ) {
		if (selected==PAGE_CCCAM) class = cSelected; else class = cNormal;
		sprintf( label, "CCcam [<span class='badge-count'> %d </span>]", total_cc_clients());
		sprintf( buf, class, "/cccam", label); tcp_writestr(tcpbuf, sock, buf);
	}
#endif

#ifdef CS378X_SRV
	// cs378x (incluido na pagina Cs357x/Camd35)
#endif

#ifdef CAMD35_SRV
	// camd35 (UDP) + cs378x (TCP) - mesma familia de protocolo
	if (cfg.camd35.server!=NULL) {
		if (selected==PAGE_CAMD35) class = cSelected; else class = cNormal;
#ifdef CS378X_SRV
		sprintf( label, "Cs358x/Camd35 [<span class='badge-count'> %d </span>]", total_c35_clients()+total_cs378x_nb());
#else
		sprintf( label, "Cs358x/Camd35 [<span class='badge-count'> %d </span>]", total_c35_clients());
#endif
		sprintf( buf, class, "/camd35", label); tcp_writestr(tcpbuf, sock, buf);
	}
#endif

	// Profiles
	if (!cfg.http.show.noprofiles) {
		if (cfg.cardserver!=NULL) {
			if (selected==PAGE_PROFILES) class = cSelected; else class = cNormal;
		} else class = cDisabled;
		sprintf( label, "Profiles [<span class='badge-count'> %d </span>]", cfg.totalprofiles);
		sprintf( buf, class, "/profiles", label); tcp_writestr(tcpbuf, sock, buf);
	}
	// Packages (dashboard por satelite/pacote)
	{
		if (selected==PAGE_PACKAGES) class = cSelected; else class = cNormal;
		sprintf( buf, class, "/packages", "Packages"); tcp_writestr(tcpbuf, sock, buf);
	}
	// Softcam
	{
		if (selected==PAGE_EMULATOR) class = cSelected; else class = cNormal;
		sprintf( label, "Softcam [<span class='badge-count'> %d </span>]", emu_keycount);
		sprintf( buf, class, "/emulator", label); tcp_writestr(tcpbuf, sock, buf);
	}
	// Configurations (Iptables + Edit Config)
	{
		if (selected==PAGE_CONFIGURATIONS) class = cSelected; else class = cNormal;
		sprintf( buf, class, "/configurations", "Configs"); tcp_writestr(tcpbuf, sock, buf);
	}
	// grupo lateral direito: Restart | tema | Logout (template do logout-btn)
	tcp_writestr(tcpbuf, sock, "<li class='menu-right'>");
	if (!cfg.http.show.norestart)
		tcp_writestr(tcpbuf, sock, "<a class='logout-btn' href='/restart'>Restart</a>");
	tcp_writestr(tcpbuf, sock, "<button id='themeToggle' class='logout-btn' onclick='toggleTheme()'>Dark</button>");
	tcp_writestr(tcpbuf, sock, "<a class='logout-btn' href='/login?action=logout'>Logout</a>");
	tcp_writestr(tcpbuf, sock, "</li>");
	tcp_writestr(tcpbuf, sock, "</ul>");
	tcp_writestr(tcpbuf, sock, "<div class='brand-line'><span class='brand'>MultiCS r"REVISION_STR"</span> <span class='brand-by'>by Sharillas</span></div></div>\n");

	//
	if ( (selected!=PAGE_RESTART)&&(selected!=PAGE_EDITOR)&&(selected!=PAGE_CONFIGURATIONS) ) {
		tcp_writestr(tcpbuf, sock, "<div class='toolbar'><span class='toolbar-label'>Autorefresh</span><input id='ar_slider' type=range min=0 max=100 value=");
		sprintf( buf, "%d", cfg.http.autorefresh);
		tcp_writestr(tcpbuf, sock, buf);
		tcp_writestr(tcpbuf, sock, " oninput='var v=this.value;document.getElementById(\"ar_val\").innerHTML=v==0?\"OFF\":v+\"s\";setautorefresh(v*1000);'> <span id=ar_val class='toolbar-badge'>");
		if (cfg.http.autorefresh>0) { sprintf( buf, "%ds", cfg.http.autorefresh); tcp_writestr(tcpbuf, sock, buf); }
		else tcp_writestr(tcpbuf, sock, "OFF");
		tcp_writestr(tcpbuf, sock, "</span></div>\n");
	}
}




void tcp_writeecmdata(struct tcp_buffer_data *tcpbuf, int sock, int ecmok, int ecmnb)
{
	char http_buf[2048];
	if (ecmnb) {
		int n;
		if (ecmnb>9999999) n = (ecmok*10)/(ecmnb/10); else n = (ecmok*100)/ecmnb;
		sprintf( http_buf, "<td>%d<span style=\"float: right;\">%d%%</span></td>", ecmok, n );
	}
	else
		sprintf( http_buf, "<td><span style=\"float: right;\">0%%</span></td>" );
	tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
}

void tcp_writeecmdata2(struct tcp_buffer_data *tcpbuf, int sock, int ecmok, int ecmnb)
{
	char http_buf[2048];
	if (ecmnb) {
		int n;
		if (ecmnb>9999999) n = (ecmok*10)/(ecmnb/10); else n = (ecmok*100)/ecmnb;
		sprintf( http_buf, "<td>%d / %d<span style=\"float: right;\">%d%%</span></td>", ecmok, ecmnb, n );
	}
	else
		sprintf( http_buf, "<td><span style=\"float: right;\">0%%</span></td>" );
	tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
}

void getstatcell(int ecmok, int ecmnb, char *dest)
{
	if (ecmnb) {
		int n;
		if (ecmnb>9999999) n = (ecmok*10)/(ecmnb/10); else n = (ecmok*100)/ecmnb;
		sprintf( dest, "%d<span style=\"float: right;\">%d%%</span>", ecmok, n );
	}
	else sprintf( dest, "<span style=\"float: right;\">0%%</span>" );
}

void getstatcell2(int ecmok, int ecmnb, char *dest)
{
	if (ecmnb) {
		int n;
		if (ecmnb>9999999) n = (ecmok*10)/(ecmnb/10); else n = (ecmok*100)/ecmnb;
		sprintf( dest, "%d / %d<span style=\"float: right;\">%d%%</span>", ecmok, ecmnb, n );
	}
	else sprintf( dest, "<span style=\"float: right;\">0%%</span>" );
}

#include <sys/stat.h>
#include <sys/statvfs.h>

void http_send_file(int sock, http_request *req, char *type, char *fname)
{
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);

	struct stat fstat;
	int fstatres=stat( fname, &fstat );	
	if ( fstatres<0 ) {
		mlogf(LOGERROR,DBG_HTTP," http: file %s not found\n",fname);
		// Not found
	}
	else {
		mlogf(LOGTRACE,DBG_HTTP," http: file %s size: %lld bytes\n",fname,(long long) fstat.st_size);
		FILE *fd = fopen( fname, "r");
		if (fd==NULL) {
			mlogf(LOGERROR,DBG_HTTP," http: could not open file %s for reading\n",fname);
			// ERROR
		}
		else {
			char buf[1024];
			sprintf( buf, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", type, (int)fstat.st_size);
			mlogf(LOGTRACE,DBG_HTTP, " http: send headers: %s\n", buf);
			tcp_write(&tcpbuf, sock, buf, strlen(buf) );
			buf[0]=0;
			while (!feof(fd)) {
				int result = fread (buf,1, sizeof(buf),fd);
				if (result>0) {
					buf[result]=0;
					mlogf(LOGTRACE,DBG_HTTP," http: send %d bytes from file %s: %s \n",result,fname,buf);
					tcp_write(&tcpbuf, sock, (char*)buf, result );
				}
			}
			fclose(fd);
		}
	}
	tcp_flush(&tcpbuf, sock);
}


void http_send_answer(int sock, http_request *req, char *type, char *buf, int size)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	sprintf( http_buf, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\nCache-Control: no-cache, no-store, must-revalidate\r\nConnection: close\r\n\r\n", type, size);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, (char*)buf, size );
	tcp_flush(&tcpbuf, sock);
}

void http_send_image(int sock, http_request *req, unsigned char *buf, int size, char *type)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	sprintf( http_buf, "HTTP/1.1 200 OK\r\nAccept-Ranges: bytes\r\nCache-Control: private, max-age=86400\r\nContent-Length: %d\r\nConnection: close\r\nContent-Type: image/%s\r\n\r\n", size, type);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, (char*)buf, size );
	tcp_flush(&tcpbuf, sock);
}

void http_send_xml(int sock, http_request *req, char *buf, int size)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	sprintf( http_buf, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nAccept-Ranges: bytes\r\nConnection: close\r\nContent-Type: application/xml\r\n\r\n", size);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, (char*)buf, size );
	tcp_flush(&tcpbuf, sock);
}


void http_send_ok(int sock)
{
	char http_buf[100];
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	sprintf( http_buf, "HTTP/1.1 200 OK\r\n\r\n");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_flush(&tcpbuf, sock);
}


void http_send_text(int sock, char *buf)
{
	int size = strlen(buf);
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	char http_buf[100];
	sprintf( http_buf, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",size);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, (char*)buf, size );
	tcp_flush(&tcpbuf, sock);
}




void http_send_ecmstatus(struct tcp_buffer_data *tcpbuf, int sock, ECM_DATA *ecm)
{
	char http_buf[2048];
	tcp_writestr(tcpbuf, sock, "<table class=\"infotable\"><tbody>\n" );
	snprintf( http_buf, sizeof(http_buf),"<tr><th>Current Ecm Request</th></tr>\n");
	tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
	// Status Msg
	if (ecm->statusmsg) {
		if (ecm->nokbiss) sprintf( http_buf,"<tr><td class=nok-yellow>%s</td></tr>", ecm->statusmsg);
		else sprintf( http_buf,"<tr><td>%s</td></tr>", ecm->statusmsg);
		tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	// Channel name
	snprintf( http_buf, sizeof(http_buf),"<tr><td>Channel  %s</td></tr>\n", getchname(ecm->caid, ecm->provid, ecm->sid) );
	tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
	// ECM
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ECM(%d): ", ecm->ecmlen); tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
	array2hex( ecm->ecm, http_buf, ecm->ecmlen );	tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"</td></tr>\n");
	// Last DCW
#ifdef CHECK_NEXTDCW
	if ( ecm->lastdecode.ecm && (ecm->lastdecode.counter>0) ) {
		snprintf( http_buf, sizeof(http_buf),"<tr><td>Previous CW: "); tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
		array2hex( ecm->lastdecode.dcw, http_buf, 16 ); tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_writestr(tcpbuf, sock, "</td></tr>\n");
		if ((ecm->lastdecode.cwcycle&0xFE)=='0') sprintf( http_buf,"<tr><td>Next Cycle = CW%c</td></tr>\n", ecm->lastdecode.cwcycle);
		tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
		if (ecm->lastdecode.error) {
			snprintf( http_buf, sizeof(http_buf),"<tr><td>Errors = %d</td></tr>\n", ecm->lastdecode.error);
			tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		snprintf( http_buf, sizeof(http_buf),"<tr><td>Total Cycles = %d</td></tr>\n<tr><td>ECM Interval = %ds</td></tr>\n", ecm->lastdecode.counter, ecm->lastdecode.dcwchangetime/1000);
		tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
	}
#endif
	// Servers
	if (ecm->server[0].srvid) {
		sprintf( http_buf, "<tr><td><table class='infotable'><tbody><tr><th width='30px'>ID</th><th width='250px'>Server</th><th width='50px'>Status</th><th width='70px'>Start time</th><th width='70px'>End time</th><th width='90px'>Elapsed time</th><th>CW</th></tr></tbody>");
		tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
		int i;
		for(i=0; i<20; i++) {
			if (!ecm->server[i].srvid) break;
			char* str_srvstatus[] = { "WAIT", "OK", "NOK", "BUSY" };
			struct server_data *srv = getsrvbyid(ecm->server[i].srvid);
			if (srv) {
				snprintf( http_buf, sizeof(http_buf),"<tr><td>%d</td><td>%s:%d</td><td>%s</td><td>%dms</td>", i+1, srv->host->name, srv->port, str_srvstatus[ecm->server[i].flag], ecm->server[i].sendtime - ecm->recvtime );
				tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
				// Recv Time
				if (ecm->server[i].statustime>ecm->server[i].sendtime)
					sprintf( http_buf,"<td>%dms</td><td>%dms</td>", ecm->server[i].statustime - ecm->recvtime, ecm->server[i].statustime-ecm->server[i].sendtime );
				else
					sprintf( http_buf,"<td>--</td><td>--</td>");
				tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
				// DCW
				if (ecm->server[i].flag==ECM_SRV_REPLY_GOOD) {
					sprintf( http_buf,"<td>"); tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
					array2hex( ecm->server[i].dcw, http_buf, 16 );	tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
					sprintf( http_buf,"</td>"); tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				else {
					sprintf( http_buf,"<td>--</td>");
					tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				sprintf( http_buf,"</tr>");
				tcp_write(tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
		tcp_writestr(tcpbuf, sock, "</tbody></table></td></tr>\n" );
	}
	// End of table
	tcp_writestr(tcpbuf, sock, "</tbody></table><br>\n");
}



void flagdebugvalue( char *str )
{
	uint32_t i,j,k;
	i = (flagdebug>>24);
	j = (flagdebug>>16)&0xff;
	k = flagdebug&0xffff;
	strcpy( str, "UNKNOWN");
	switch (i) {
		case DBG_ALL:
			strcpy( str, "ALL");
			break;
		case DBG_NEWCAMD:
			if (!j) strcpy( str, "PROFILES");
			else {
				struct cardserver_data *cs = getcsbyid(j);
				if (cs) {
					if (!k) sprintf( str, "[%s]", cs->name);
					else {
						struct cs_client_data *cli = getnewcamdclientbyid(k);
						if (cli) sprintf( str, "[%s] Newcamd Client '%s'", cs->name, cli->user);
						else sprintf( str, "[%s] Unknown Newcamd Client ID=%d", cs->name, k);
					}
				} else sprintf( str, "Unknown Profile ID=%d", j);
			}
			break;
		case DBG_CCCAM:
			if (!j) strcpy( str, "CCCAM");
			else {
				struct cccam_server_data *cc = getcccamserverbyid(j);
				if (cc) {
					if (!k) sprintf( str, "CCcam%d [%d]", cc->id, cc->port);
					else {
						struct cc_client_data *cli = getcccamclientbyid(k);
						if (cli) sprintf( str, "CCcam%d - Client '%s'", cc->id, cli->user);
						else sprintf( str, "CCcam%d - Unknown Client ID=%d", cc->id, k);
					}
				} else sprintf( str, "Unknown CCcam Server ID=%d", j);
			}
			break;
		case DBG_SERVER:
			if (!k) strcpy( str, "SERVERS");
			else {
				struct server_data *srv = getsrvbyid(k);
				if (srv) 
					sprintf( str, "Server (%s:%d)", srv->host->name, srv->port);
				else sprintf( str, "Unknown Server ID=%d", k);
			}
			break;
		case DBG_MGCAMD:
			if (!k) strcpy( str, "MGCAMD");
			else {
				struct mg_client_data *cli = getmgcamdclientbyid(k);
				if (cli)
					sprintf( str, "Mgcamd Client (%s)", cli->user);
				else sprintf( str, "Unknown Mgcamd Client ID=%d", k);
			}
			break;
		case DBG_CS378X:
			if (!k) strcpy( str, "CS378X");
			else {
				struct camd35_client_data *cli = getcs378xclientbyid(k);
				if (cli)
					sprintf( str, "Cs378x Client (%s)", cli->user);
				else sprintf( str, "Unknown Cs378x Client ID=%d", k);
			}
			break;
		case DBG_CAMD35:
			if (!k) strcpy( str, "CAMD35");
			else {
				struct camd35_client_data *cli = getcamd35clientbyid(k);
				if (cli)
					sprintf( str, "Camd35 Client (%s)", cli->user);
				else sprintf( str, "Unknown Camd35 Client ID=%d", k);
			}
			break;
		case DBG_ERROR:
			strcpy( str, "ERROR");
			break;

	}
}


void http_send_index(int sock, http_request *req)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;

	// Action
	char *str_action = isset_get( req, "action");
	int get_action = 0;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = 1;
		else if (!strcmp(str_action,"row")) get_action = 2;
		else str_action = NULL;
	}
	if (!str_action) str_action = "page";
	//
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	if (get_action==0) {
		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, html_title, cfg.http.title, "Dashboard"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		// JS
        tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
        tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
		// ACTIONS REQUEST
		tcp_writestr(&tcpbuf, sock, "\nfunction imgrequest( url, el )\n{\n	var httpRequest;\n	try { httpRequest = new XMLHttpRequest(); }\n	catch (trymicrosoft) { try { httpRequest = new ActiveXObject('Msxml2.XMLHTTP'); } catch (oldermicrosoft) { try { httpRequest = new ActiveXObject('Microsoft.XMLHTTP'); } catch(failed) { httpRequest = false; } } }\n	if (!httpRequest) { alert('Your browser does not support Ajax.'); return false; }\n	if ( typeof(el)!='undefined' ) {\n		el.onclick = null;\n		el.style.opacity = '0.7';\n		httpRequest.onreadystatechange = function()\n		{\n			if (httpRequest.readyState == 4) if (httpRequest.status == 200) el.style.opacity = '0.3';\n		}\n	}\n	httpRequest.open('GET', url, true);\n	httpRequest.send(null);\n}\n");
		// UPD DIV
		char url[256];
		sprintf( url, "/?action=div");
		sprintf( http_buf, HTTP_UPDATE_DIV, cfg.http.autorefresh*1000, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	 setautorefresh(autorefresh);\n}");
		tcp_writestr(&tcpbuf, sock, "\n</script>\n");
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
		tcp_write_menu(&tcpbuf, sock,PAGE_HOME);
		// DIV
		tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");
	}

	unsigned int d= GetTickCount()/1000;

	// === stat sections ===
	tcp_writestr(&tcpbuf, sock, "<div style='display:flex;gap:15px;flex-wrap:wrap;margin:10px 0'>");

	// System
	tcp_writestr(&tcpbuf, sock, "<div class=stat-section style='flex:1;min-width:280px;'>");
	tcp_writestr(&tcpbuf, sock, "<h3 class=stitle >System</h3><div class=stat-value>");
	sprintf( http_buf,"<span class=stat-label>Uptime:</span> %02dd %02d:%02d:%02d<br>", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<span class=stat-label>CPU Cores:</span> %ld<br>", sysconf(_SC_NPROCESSORS_ONLN));
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	FILE *fp = fopen ("/proc/loadavg", "r");
	if (fp) {
		float avg1,avg2,avg3;
		char procs[20];
		fscanf(fp, "%f %f %f %s", &avg1,&avg2,&avg3,procs);
		fclose(fp);
		sprintf( http_buf,"<span class=stat-label>Load Average:</span> %01.2f : %01.2f : %01.2f<br><span class=stat-label>Processes:</span> %s<br>", avg1,avg2,avg3,procs);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	int memtotal=0, memfree=0, memavail=0, memcached=0;
	fp = fopen ("/proc/meminfo", "r");
	if (fp) {
		char line[256];
		while (fgets(line,sizeof(line),fp)) {
			if (!strncmp(line,"MemTotal:",9)) memtotal=atoi(line+9);
			else if (!strncmp(line,"MemFree:",8)) memfree=atoi(line+8);
			else if (!strncmp(line,"MemAvailable:",13)) memavail=atoi(line+13);
			else if (!strncmp(line,"Cached:",7)) memcached=atoi(line+7);
		}
		fclose(fp);
	}
	if (memtotal) {
		sprintf( http_buf,"<span class=stat-label>Total RAM:</span> %d MB<br>", memtotal/1024);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	if (memavail) {
		sprintf( http_buf,"<span class=stat-label>Free RAM:</span> %d MB", memavail/1024);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		if (memcached) {
			sprintf( http_buf," (%d MB cached)", memcached/1024);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		tcp_writestr(&tcpbuf, sock, "<br>");
	} else if (memfree) {
		sprintf( http_buf,"<span class=stat-label>Free RAM:</span> %d MB", memfree/1024);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		if (memcached) {
			sprintf( http_buf," (%d MB cached)", memcached/1024);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		tcp_writestr(&tcpbuf, sock, "<br>");
	}
	// Process CPU (deltas de /proc/self/stat)
	{
		static unsigned long long last_proc_ticks = 0;
		static unsigned long long last_proc_time = 0;
		fp = fopen ("/proc/self/stat","r");
		if (fp) {
			unsigned long long utime, stime;
			if ( fscanf(fp,"%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu",&utime,&stime)==2 ) {
				unsigned long long now = GetTickCount();
				if (last_proc_time && now>last_proc_time) {
					double cpu = (double)(utime+stime-last_proc_ticks)*1000.0/(now-last_proc_time);
					sprintf( http_buf,"<span class=stat-label>Process CPU:</span> %.1f%%<br>", cpu);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				last_proc_ticks = utime+stime;
				last_proc_time = now;
			}
			fclose(fp);
		}
	}
	struct statvfs sv;
	if (statvfs("/",&sv)==0) {
		sprintf( http_buf,"<span class=stat-label>Disk (/):</span> %llu MB free<br>", (unsigned long long)(sv.f_bfree*sv.f_frsize/1024/1024));
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	fp = fopen ("/proc/self/statm","r");
	if (fp) {
		long size,resident;
		if (fscanf(fp,"%ld %ld",&size,&resident)==2) {
			sprintf( http_buf,"<span class=stat-label>Process Memory:</span> %ld MB<br>", resident*sysconf(_SC_PAGESIZE)/1024/1024);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		fclose(fp);
	}
	tcp_writestr(&tcpbuf, sock, "</div></div>");

	// Servers
	tcp_writestr(&tcpbuf, sock, "<div class=stat-section style='flex:1;min-width:280px;'>");
	tcp_writestr(&tcpbuf, sock, "<h3 class=stitle >Servers</h3><div class=stat-value>");
	sprintf( http_buf, "<span class=stat-label>Total Profiles:</span> %d<br>", cfg.totalprofiles );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<span class=stat-label>Total Servers:</span> %d<br>", cfg.totalservers );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<span class=stat-label>Total Cache Servers:</span> %d<br>", cfg.cache.totalservers );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<span class=stat-label>Total CCcam Servers:</span> %d<br>", cfg.cccam.totalservers );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<span class=stat-label>Total Mgcamd Servers:</span> %d<br>", cfg.mgcamd.totalservers );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<span class=stat-label>Total Camd35 Servers:</span> %d<br>", cfg.camd35.totalservers );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<span class=stat-label>Total cs378x Servers:</span> %d<br>", cfg.cs378x.totalservers );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_writestr(&tcpbuf, sock, "</div></div>");

	// Clients
	tcp_writestr(&tcpbuf, sock, "<div class=stat-section style='flex:1;min-width:280px;'>");
	tcp_writestr(&tcpbuf, sock, "<h3 class=stitle >Clients</h3><div class=stat-value>");
	sprintf( http_buf, "<span class=stat-label>Connected Clients:</span> %d<br>", connected_all_clients() );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<span class=stat-label>ECM Total:</span> %d<br>", totalecm );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	int activeecm=0;
	ECM_DATA *ecmwalk=ecmdata;
	while (ecmwalk) { if (ecmwalk->dcwstatus==STAT_DCW_WAIT) activeecm++; ecmwalk=ecmwalk->next; if (ecmwalk==ecmdata) break; }
	sprintf( http_buf, "<span class=stat-label>Active ECMs:</span> %d<br>", activeecm );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#ifdef CACHEEX
	sprintf( http_buf, "<span class=stat-label>CacheEX Servers:</span> %d<br>", total_cacheex_servers() );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
	sprintf( http_buf,"<span class=stat-label>NodeID =</span> %02x%02x%02x%02x%02x%02x%02x%02x", cfg.nodeid[0], cfg.nodeid[1], cfg.nodeid[2], cfg.nodeid[3], cfg.nodeid[4], cfg.nodeid[5], cfg.nodeid[6], cfg.nodeid[7]);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_writestr(&tcpbuf, sock, "</div></div></div>");

	// Current Ecm Request
	ECM_DATA *ecmreq = NULL;
	ecmwalk = ecmdata;
	while (ecmwalk) {
		if (ecmwalk->recvtime) { ecmreq = ecmwalk; break; }
		ecmwalk = ecmwalk->next;
		if (ecmwalk==ecmdata) break;
	}
	tcp_writestr(&tcpbuf, sock, "<div class=stat-section style='margin:10px 0'>");
	tcp_writestr(&tcpbuf, sock, "<h3 class=stitle >Current Ecm Request</h3>");
	if (ecmreq) {
		tcp_writestr(&tcpbuf, sock, "<table class='infotable'><tbody>");
		sprintf( http_buf, "<tr><td>Channel  %04X:%06X:%04X</td></tr>", ecmreq->caid, ecmreq->provid, ecmreq->sid);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf, "<tr><td>Next Cycle = CW%d</td></tr>", ecmreq->cw1cycle==0x80?0:1);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf, "<tr><td>Total Cycles = %d</td></tr>", totalecm);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf, "<tr><td>ECM Interval = %ds</td></tr>", TIME_ECMALIVE/1000);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_writestr(&tcpbuf, sock, "<tr><td><table class='infotable cwtable'><tbody><tr><th width='30px'>ID</th><th width='250px'>Server</th><th width='50px'>Status</th><th width='70px'>Start time</th><th width='70px'>End time</th><th width='90px'>Elapsed time</th><th class='cwcol'>CW</th></tr>");
		int si;
		for (si=0; si<ecmreq->server_totalsent; si++) {
			struct server_data *srv = getserverbyid(ecmreq->server[si].srvid);
			if (!srv) continue;
			char *st = ecmreq->server[si].flag==1 ? "ok" : (ecmreq->server[si].flag==2 ? "fail" : "wait");
			char cwhex[64] = "-";
			if (ecmreq->server[si].flag==1) {
				int ci; char *p=cwhex;
				for (ci=0; ci<16; ci++) { sprintf(p,"%02X ", ecmreq->server[si].dcw[ci]); p+=3; }
			}
			sprintf( http_buf, "<tr><td>%u</td><td>%s:%d</td><td>%s</td><td>%dms</td><td>%dms</td><td>%dms</td><td class='cwcol'>%s</td></tr>",
				ecmreq->server[si].srvid, srv->host->name, srv->port, st,
				ecmreq->server[si].sendtime, ecmreq->server[si].statustime,
				ecmreq->server[si].statustime-ecmreq->server[si].sendtime, cwhex);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		tcp_writestr(&tcpbuf, sock, "</tbody></table></td></tr></tbody></table>");
	} else {
		tcp_writestr(&tcpbuf, sock, "<p>No ecm request yet.</p>");
	}
	tcp_writestr(&tcpbuf, sock, "</div>");

	// Cache Stats
	int cache_total=0, cache_active=0;
	cache_peers( cfg.cache.server, &cache_total, &cache_active );
	sprintf( http_buf, "<div class=stat-section style='margin:10px 0'><h3 class=stitle >Cache Stats</h3><table class=maintable><tr><th>Metric</th><th>Value</th></tr><tr><td>Cache Servers</td><td>%d</td></tr><tr><td>Active Peers</td><td>%d</td></tr><tr><td>Unread SMS</td><td>%d</td></tr></table></div>", cfg.cache.totalservers, cache_active, unreadsms());
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	// Protecoes & Eventos recentes
	{
		uint32_t up = prot_uptime_ticks()/1000;
		int gecm=0, gok=0, gnok=0;
		struct cardserver_data *pcs = cfg.cardserver;
		while (pcs) {
			gecm += pcs->ecmaccepted + pcs->ecmdenied;
			gok += pcs->ecmok;
			gnok += pcs->ecmdenied;
			pcs = pcs->next;
		}
		sprintf( http_buf, "<div class=stat-section style='margin:10px 0'><h3 class=stitle >Protecoes &amp; Eventos</h3>"
			"<table class=maintable><tr><th>Metric</th><th>Value</th></tr>"
			"<tr><td>Uptime do processo</td><td>%02dd %02d:%02d:%02d</td></tr>"
			"<tr><td>ECMs totais</td><td>%d (OK: %d | NOK: %d)</td></tr>"
			"<tr><td>Regras CWPK aprendidas</td><td>%d</td></tr></table>"
			"<div style='margin-top:8px;max-height:220px;overflow-y:auto;font-size:12px;'>",
			up/(3600*24), (up/3600)%24, (up/60)%60, up%60,
			gecm, gok, gnok, dcw_filter_learned_count());
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		int evn = 0;
		uint32_t age = 0;
		char *ev = prot_event_get(0, &age);
		if (!ev) tcp_writestr(&tcpbuf, sock, "<span style='color:#8899aa'>Sem eventos ainda (filtros em AUTO - ativam sozinhos quando necessario).</span>");
		while (ev && evn<8) {
			sprintf( http_buf, "<div style='padding:2px 0;border-bottom:1px solid #333'>%s <span style='color:#8899aa;font-size:11px'>(%us atras)</span></div>", ev, age/1000);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			evn++;
			ev = prot_event_get(evn, &age);
		}
		tcp_writestr(&tcpbuf, sock, "</div></div>");
	}

	// Debug Log (no fim, antes do footer)
	char dbgbuf[MAX_DBGLINE_LEN];
	tcp_writestr(&tcpbuf, sock, "<div class=stat-section style='margin:10px 0'><h3 class=stitle >Debug Log</h3><select id='dbgfilter' onchange=\"setDebugFilter(this.value);\" style='width:250px;'>");
	int sel;
	if ( (flagdebug&0xffffff)!=0 ) {
		char str[255];
		flagdebugvalue( str );
		sprintf( dbgbuf, "<option>%s</option>",str);
		tcp_writestr(&tcpbuf, sock, dbgbuf);
		sel = 0x10;
	} else sel = (flagdebug>>24);
	if (sel==DBG_ALL) tcp_writestr(&tcpbuf, sock, "<option value='ALL' selected>ALL</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='ALL'>ALL</option>");
	if (sel==DBG_SERVER) tcp_writestr(&tcpbuf, sock, "<option value='SERVER' selected>SERVERS</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='SERVER'>SERVERS</option>");
	if (sel==DBG_CACHE) tcp_writestr(&tcpbuf, sock, "<option value='CACHE' selected>CACHE</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='CACHE'>CACHE</option>");
	if (sel==DBG_NEWCAMD) tcp_writestr(&tcpbuf, sock, "<option value='NEWCAMD' selected>PROFILES</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='NEWCAMD'>PROFILES</option>");
	if (sel==DBG_MGCAMD) tcp_writestr(&tcpbuf, sock, "<option value='MGCAMD' selected>MGCAMD</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='MGCAMD'>MGCAMD</option>");
	if (sel==DBG_CCCAM) tcp_writestr(&tcpbuf, sock, "<option value='CCCAM' selected>CCCAM</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='CCCAM'>CCCAM</option>");
#ifdef CS378X_SRV
	if (sel==DBG_CS378X) tcp_writestr(&tcpbuf, sock, "<option value='CS378X' selected>CS378X</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='CS378X'>CS378X</option>");
#endif
#ifdef CACHEEX
	if (sel==DBG_CACHEEX) tcp_writestr(&tcpbuf, sock, "<option value='CACHEEX' selected>CACHEEX</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='CACHEEX'>CACHEEX</option>");
#endif
#ifndef PUBLIC
	if (sel==DBG_ERROR) tcp_writestr(&tcpbuf, sock, "<option value='ERROR' selected>ERROR</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='ERROR'>ERROR</option>");
#endif
	tcp_writestr(&tcpbuf, sock, "</select><div id='dbglog'><pre style=\"font-size:13px;\">");
	int current = idbgline;
	int i = current - 25;
	if (i<0) i += MAX_DBGLINES;
	do {
		sprintf( dbgbuf, "%s", dbgline[i] ); tcp_write(&tcpbuf, sock, dbgbuf, strlen(dbgbuf) );
		i++;
		if (i>=MAX_DBGLINES) i=0;
	} while (i!=current);
	tcp_writestr(&tcpbuf, sock, "</pre></div></div>");

	if (get_action==0) tcp_writestr(&tcpbuf, sock, "</body></html>");
	tcp_flush(&tcpbuf, sock);
}


static uint32_t viewdbgflag = 0;

void http_send_debug(int sock, http_request *req)
{
	char http_buf[MAX_DBGLINE_LEN];
	struct tcp_buffer_data tcpbuf;

	// Action
	char *str_action = isset_get( req, "action");
	int get_action = 0;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = 1;
		else if (!strcmp(str_action,"row")) get_action = 2;
		else if (!strcmp(str_action,"log")) get_action = 8;
		else if (!strcmp(str_action,"download")) get_action = 9;
		else if (!strcmp(str_action,"debug")) {
			get_action = 3;
			char *str_value = isset_get( req, "value");
			if (str_value) {
				if (!strcmp(str_value,"ALL")) flagdebug = getdbgflag( DBG_ALL, 0, 0);
				//else if (!strcmp(str_value,"CONFIG")) flagdebug = getdbgflag( DBG_CONFIG, 0, 0);
				else if (!strcmp(str_value,"SERVER")) flagdebug = getdbgflag( DBG_SERVER, 0, 0);
				else if (!strcmp(str_value,"CACHE")) flagdebug = getdbgflag( DBG_CACHE, 0, 0);
				else if (!strcmp(str_value,"NEWCAMD")) flagdebug = getdbgflag( DBG_NEWCAMD, 0, 0);
				else if (!strcmp(str_value,"MGCAMD")) flagdebug = getdbgflag( DBG_MGCAMD, 0, 0);
				else if (!strcmp(str_value,"CCCAM")) flagdebug = getdbgflag( DBG_CCCAM, 0, 0);
#ifdef CS378X_SRV
				else if (!strcmp(str_value,"CS378X")) flagdebug = getdbgflag( DBG_CS378X, 0, 0);
#endif
#ifdef CACHEEX
				else if (!strcmp(str_value,"CACHEEX")) flagdebug = getdbgflag( DBG_CACHEEX, 0, 0);
#endif
				//else if (!strcmp(str_value,"HTTP")) flagdebug = getdbgflag( DBG_HTTP, 0, 0);
				else if (!strcmp(str_value,"ERROR")) flagdebug = getdbgflag( DBG_ERROR, 0, 0);
				//else return;
				viewdbgflag = flagdebug;
				http_send_ok(sock);
			}
			return;
		}
		else str_action = NULL;
	}
	if (!str_action) str_action = "page";

	// Apenas o log filtrado (AJAX)
	if (get_action==8) {
		tcp_init(&tcpbuf);
		tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
		tcp_writestr(&tcpbuf, sock, "<pre style=\"font-size:13px;\">");
		uint32_t fcat = viewdbgflag>>24;
		int current = idbgline;
		int i = current - 50;
		if (i<0) i += MAX_DBGLINES;
		int shown = 0;
		do {
			if ( !fcat || ((dbgflag[i]>>24)==fcat) ) {
				tcp_write(&tcpbuf, sock, dbgline[i], strlen(dbgline[i]) );
				shown++;
			}
			i++;
			if (i>=MAX_DBGLINES) i=0;
		} while (i!=current);
		if (!shown) tcp_writestr(&tcpbuf, sock, "(sem entradas para este filtro)\n");
		tcp_writestr(&tcpbuf, sock, "</pre>");
		tcp_flush(&tcpbuf, sock);
		return;
	}

	// Download do log completo (ring buffer) como ficheiro txt
	if (get_action==9) {
		tcp_init(&tcpbuf);
		sprintf( http_buf, "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Disposition: attachment; filename=\"multics-debug.txt\"\r\nCache-Control: no-cache, no-store, must-revalidate\r\nConnection: close\r\n\r\n");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_writestr(&tcpbuf, sock, "========================================\n");
		tcp_writestr(&tcpbuf, sock, " MultiCS r1000 debug log (ultimas entradas)\n");
		sprintf( http_buf, " %s\n", cfg.http.title);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_writestr(&tcpbuf, sock, "========================================\n\n");
		int current = idbgline;
		int i = current - (MAX_DBGLINES-1);
		if (i<0) i += MAX_DBGLINES;
		do {
			if (dbgline[i][0]) tcp_write(&tcpbuf, sock, dbgline[i], strlen(dbgline[i]) );
			i++;
			if (i>=MAX_DBGLINES) i=0;
		} while (i!=current);
		tcp_flush(&tcpbuf, sock);
		return;
	}

	//
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	if (get_action==0) {
		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, html_title, cfg.http.title, "Debug"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		// JS
		tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
        tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
		// ACTIONS REQUEST
		tcp_writestr(&tcpbuf, sock, "\nfunction imgrequest( url, el )\n{\n	var httpRequest;\n	try { httpRequest = new XMLHttpRequest(); }\n	catch (trymicrosoft) { try { httpRequest = new ActiveXObject('Msxml2.XMLHTTP'); } catch (oldermicrosoft) { try { httpRequest = new ActiveXObject('Microsoft.XMLHTTP'); } catch(failed) { httpRequest = false; } } }\n	if (!httpRequest) { alert('Your browser does not support Ajax.'); return false; }\n	if ( typeof(el)!='undefined' ) {\n		el.onclick = null;\n		el.style.opacity = '0.7';\n		httpRequest.onreadystatechange = function()\n		{\n			if (httpRequest.readyState == 4) if (httpRequest.status == 200) el.style.opacity = '0.3';\n		}\n	}\n	httpRequest.open('GET', url, true);\n	httpRequest.send(null);\n}\n");
		// UPD DIV
		char url[256];
		sprintf( url, "/debug?action=div");
		sprintf( http_buf, HTTP_UPDATE_DIV, cfg.http.autorefresh*1000, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	 setautorefresh(autorefresh);\n}");
		tcp_writestr(&tcpbuf, sock, "\n</script>\n");
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
		tcp_write_menu(&tcpbuf, sock,PAGE_DEBUG);
		// DIV
		tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");
	}

	if (!cfg.http.show.nodebug) {
		tcp_writestr(&tcpbuf, sock, "<br>\n<fieldset><legend> Debug: <select id='dbgfilter' onchange=\"setDebugFilter(this.value);\" style='width:250px;'>");
		int sel;
		if ( (flagdebug&0xffffff)!=0 ) {
			char str[255];
			flagdebugvalue( str );
			sprintf( http_buf, "<option>%s</option>",str);
			tcp_writestr(&tcpbuf, sock, http_buf);
			sel = 0x10;
		} else sel = (flagdebug>>24);
		if (sel==DBG_ALL) tcp_writestr(&tcpbuf, sock, "<option value='ALL' selected>ALL</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='ALL'>ALL</option>");
		if (sel==DBG_SERVER) tcp_writestr(&tcpbuf, sock, "<option value='SERVER' selected>SERVERS</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='SERVER'>SERVERS</option>");
		if (sel==DBG_CACHE) tcp_writestr(&tcpbuf, sock, "<option value='CACHE' selected>CACHE</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='CACHE'>CACHE</option>"); 
		if (sel==DBG_NEWCAMD) tcp_writestr(&tcpbuf, sock, "<option value='NEWCAMD' selected>PROFILES</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='NEWCAMD'>PROFILES</option>");
		if (sel==DBG_MGCAMD) tcp_writestr(&tcpbuf, sock, "<option value='MGCAMD' selected>MGCAMD</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='MGCAMD'>MGCAMD</option>"); 
		if (sel==DBG_CCCAM) tcp_writestr(&tcpbuf, sock, "<option value='CCCAM' selected>CCCAM</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='CCCAM'>CCCAM</option>");
#ifdef CS378X_SRV
		if (sel==DBG_CS378X) tcp_writestr(&tcpbuf, sock, "<option value='CS378X' selected>CS378X</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='CS378X'>CS378X</option>"); 
#endif
#ifdef CACHEEX
		if (sel==DBG_CACHEEX) tcp_writestr(&tcpbuf, sock, "<option value='CACHEEX' selected>CACHEEX</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='CACHEEX'>CACHEEX</option>"); 
#endif
#ifndef PUBLIC
		if (sel==DBG_ERROR) tcp_writestr(&tcpbuf, sock, "<option value='ERROR' selected>ERROR</option>"); else tcp_writestr(&tcpbuf, sock, "<option value='ERROR'>ERROR</option>");
#endif
		tcp_writestr(&tcpbuf, sock, "</select></legend>\n");
		tcp_writestr(&tcpbuf, sock, "<div id='dbglog'>");
		sprintf( http_buf, "<pre style=\"font-size:13px;\">"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		int current = idbgline;
		int i = current - 50;
		if (i<0) i += MAX_DBGLINES;
		do {
			sprintf( http_buf, "%s", dbgline[i] ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			i++;
			if (i>=MAX_DBGLINES) i=0;
		} while (i!=current);
		sprintf( http_buf, "</pre></div><br><a class='sbutton' href='/debug?action=download'>Download Log (txt)</a>&nbsp;<span style='font-size:11px;'>guarda as ultimas %d entradas do log num ficheiro multics-debug.txt</span></fieldset>", MAX_DBGLINES); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}

	// Paths (binario + ficheiros de config) - diagnostico
	{
		tcp_writestr(&tcpbuf, sock, "<br><fieldset><legend>Paths</legend><pre style=\"font-size:12px;\">");
		char selfexe[512];
		int n = readlink("/proc/self/exe", selfexe, sizeof(selfexe)-1);
		if (n>0) selfexe[n] = 0; else strcpy(selfexe, "?");
		sprintf( http_buf, "Binary: %s\n", selfexe);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		struct filename_data *pfs = cfg.files;
		int pfi = 0;
		while (pfs) {
			sprintf( http_buf, "  [%d] %s%s\n", pfi, pfs->name, pfs->nowatch?" (no-watch)":"");
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			pfi++;
			pfs = pfs->next;
		}
		sprintf( http_buf, "CONSTCW: %s\nSTYLESHEET: %s\nBLOCKEDIP: %s\nLITE FILE: %s\n", cfg.constcw_file[0]?cfg.constcw_file:"(none)", cfg.stylesheet_file[0]?cfg.stylesheet_file:"(none)", cfg.blockedip_file[0]?cfg.blockedip_file:"(none)", cfg.lite_file[0]?cfg.lite_file:"(none)");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_writestr(&tcpbuf, sock, "</pre></fieldset>");
	}

	if (get_action==0) {
		tcp_writestr(&tcpbuf, sock, "</div></body></html>");
	}
	tcp_flush(&tcpbuf, sock);
}

void http_send_redirect(int sock, char *location)
{
	char http_buf[512];
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	sprintf( http_buf, "HTTP/1.1 302 Found\r\nLocation: %s\r\nCache-Control: no-cache, no-store, must-revalidate\r\nConnection: close\r\n\r\n", location);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_flush(&tcpbuf, sock);
}

// ---------------- HTTP SESSIONS (login) ----------------
#define HTTP_MAX_SESSIONS 64
struct http_session_data {
	char token[33];
	uint32_t expire;
};
static struct http_session_data http_sessions[HTTP_MAX_SESSIONS];
static int http_session_count = 0;
static pthread_mutex_t http_session_mutex = PTHREAD_MUTEX_INITIALIZER;

// SESSOES PERSISTENTES: sobrevivem a restarts (o browser nao volta ao login)
#define HTTP_SESSION_FILE "/var/tmp/multics.sessions"
static int http_sessions_loaded = 0;

static void http_session_load(void)
{
	if (http_sessions_loaded) return;
	http_sessions_loaded = 1;
	FILE *fp = fopen(HTTP_SESSION_FILE, "r");
	if (!fp) return;
	time_t now = time(NULL);
	uint32_t ticks = (uint32_t)GetTickCount();
	char tok[40];
	long long exp;
	while ( fscanf(fp, "%39s %lld", tok, &exp)==2 ) {
		long long remain = exp - (long long)now;
		if ( (remain<=0) || (remain>86400) ) continue;
		int i;
		for (i=0; i<HTTP_MAX_SESSIONS; i++) {
			if (!http_sessions[i].token[0]) {
				strncpy(http_sessions[i].token, tok, sizeof(http_sessions[i].token)-1);
				http_sessions[i].token[sizeof(http_sessions[i].token)-1] = 0;
				http_sessions[i].expire = ticks + (uint32_t)(remain*1000);
				http_session_count++;
				break;
			}
		}
	}
	fclose(fp);
}

static void http_session_save(void)
{
	FILE *fp = fopen(HTTP_SESSION_FILE ".tmp", "w");
	if (!fp) return;
	time_t now = time(NULL);
	uint32_t ticks = (uint32_t)GetTickCount();
	int i;
	for (i=0; i<HTTP_MAX_SESSIONS; i++) {
		if (http_sessions[i].token[0] && (http_sessions[i].expire>ticks)) {
			long long exp = (long long)now + (long long)(http_sessions[i].expire-ticks)/1000;
			fprintf(fp, "%s %lld\n", http_sessions[i].token, exp);
		}
	}
	fclose(fp);
	rename(HTTP_SESSION_FILE ".tmp", HTTP_SESSION_FILE);
}

static char *http_session_new()
{
	static char token[33];
	uint32_t t = (uint32_t)GetTickCount();
	int i;
	for (i=0; i<32; i++) {
		t = t*1103515245 + 12345 + rand();
		token[i] = "0123456789abcdef"[(t>>16)&0xf];
	}
	token[32] = 0;
	pthread_mutex_lock(&http_session_mutex);
	if (http_session_count>=HTTP_MAX_SESSIONS) {
		// apaga a sessao mais antiga
		int oldest = 0;
		for (i=1; i<HTTP_MAX_SESSIONS; i++)
			if (http_sessions[i].expire < http_sessions[oldest].expire) oldest = i;
		memset(&http_sessions[oldest], 0, sizeof(struct http_session_data));
		http_session_count--;
	}
	for (i=0; i<HTTP_MAX_SESSIONS; i++) {
		if (!http_sessions[i].token[0]) {
			strcpy(http_sessions[i].token, token);
			http_sessions[i].expire = (uint32_t)GetTickCount() + 86400000; // 24h
			http_session_count++;
			break;
		}
	}
	http_session_save();
	pthread_mutex_unlock(&http_session_mutex);
	return token;
}

static int http_session_check(const char *token)
{
	if (!token || !token[0]) return 0;
	int ok = 0;
	uint32_t ticks = (uint32_t)GetTickCount();
	pthread_mutex_lock(&http_session_mutex);
	http_session_load();
	int i;
	for (i=0; i<HTTP_MAX_SESSIONS; i++) {
		if (http_sessions[i].token[0] && !strcmp(http_sessions[i].token, token)) {
			if (http_sessions[i].expire > ticks) ok = 1;
			else { memset(&http_sessions[i], 0, sizeof(struct http_session_data)); http_session_count--; http_session_save(); }
			break;
		}
	}
	pthread_mutex_unlock(&http_session_mutex);
	return ok;
}

static void http_session_del(const char *token)
{
	if (!token || !token[0]) return;
	pthread_mutex_lock(&http_session_mutex);
	int i;
	for (i=0; i<HTTP_MAX_SESSIONS; i++) {
		if (http_sessions[i].token[0] && !strcmp(http_sessions[i].token, token)) {
			memset(&http_sessions[i], 0, sizeof(struct http_session_data));
			http_session_count--;
			http_session_save();
			break;
		}
	}
	pthread_mutex_unlock(&http_session_mutex);
}

// termina TODAS as sessoes (usado no botao da GUI e quando a password muda)
void http_session_clearall(void)
{
	pthread_mutex_lock(&http_session_mutex);
	memset(http_sessions, 0, sizeof(http_sessions));
	http_session_count = 0;
	http_session_save();
	pthread_mutex_unlock(&http_session_mutex);
}

static char *http_get_cookie(http_request *req, const char *name)
{
	int i;
	for (i=0; i<req->hdrcount; i++) {
		if (!strncmp(req->headers[i].name, "Cookie", 255)) {
			char *p = strstr(req->headers[i].value, name);
			if (p) {
				p += strlen(name);
				while (*p=='='||*p==' ') p++;
				static char val[64];
				int n = 0;
				while (*p && *p!=';' && *p!=' ' && n<63) val[n++] = *p++;
				val[n] = 0;
				return val;
			}
		}
	}
	return NULL;
}

// ---------------- LOGIN GUARD (brute-force) ----------------
// N falhas de login do mesmo IP -> bloqueio temporario (defaults: 5 falhas / 30s)
// + throttle global: rajada de IPs distintos a falhar -> abrandar todos
#define LOGIN_GUARD_MAX 64
struct login_guard_data {
	uint32_t ip;
	int fails;
	time_t lockuntil;
};
static struct login_guard_data login_guard[LOGIN_GUARD_MAX];
static pthread_mutex_t login_guard_mutex = PTHREAD_MUTEX_INITIALIZER;
static time_t login_guard_lastburst = 0;
static int login_guard_burstips = 0;

// 1 = IP bloqueado (e a tabela e varrida de entradas expiradas)
static int login_guard_check(uint32_t ip)
{
	int locked = 0;
	time_t now = time(NULL);
	pthread_mutex_lock(&login_guard_mutex);
	int i;
	for (i=0; i<LOGIN_GUARD_MAX; i++) {
		if (!login_guard[i].ip) continue;
		if ( (login_guard[i].lockuntil>0) && (now >= login_guard[i].lockuntil) ) {
			memset(&login_guard[i], 0, sizeof(struct login_guard_data)); // expirou
			continue;
		}
		if ( (login_guard[i].ip==ip) && (login_guard[i].lockuntil>0) && (now < login_guard[i].lockuntil) ) locked = 1;
	}
	pthread_mutex_unlock(&login_guard_mutex);
	return locked;
}

static void login_guard_fail(uint32_t ip)
{
	time_t now = time(NULL);
	pthread_mutex_lock(&login_guard_mutex);
	// throttle global: 10 IPs distintos a falhar em 10s -> abrandar todos
	if ( (now - login_guard_lastburst) > 10 ) {
		login_guard_lastburst = now;
		login_guard_burstips = 0;
	}
	int known = 0;
	int i, freei = -1;
	for (i=0; i<LOGIN_GUARD_MAX; i++) {
		if (!login_guard[i].ip) { if (freei<0) freei = i; continue; }
		if (login_guard[i].ip==ip) {
			known = 1;
			login_guard[i].fails++;
			if (login_guard[i].fails >= cfg.http.loginfails) {
				login_guard[i].fails = 0;
				login_guard[i].lockuntil = now + cfg.http.locktime;
				mlogf(LOGINFO,DBG_HTTP," http: login guard - ip %s bloqueado %ds\n", (char*)ip2string(ip), cfg.http.locktime);
			}
			break;
		}
	}
	if (!known && (freei>=0)) {
		login_guard[freei].ip = ip;
		login_guard[freei].fails = 1;
		login_guard[freei].lockuntil = 0;
		login_guard_burstips++;
	}
	pthread_mutex_unlock(&login_guard_mutex);
	if ( (now - login_guard_lastburst) <= 10 ) {
		if (login_guard_burstips >= 10) { usleep(1500000); } // rajada: abranda 1.5s
	}
}

static void login_guard_clear(uint32_t ip)
{
	pthread_mutex_lock(&login_guard_mutex);
	int i;
	for (i=0; i<LOGIN_GUARD_MAX; i++) {
		if (login_guard[i].ip==ip) {
			memset(&login_guard[i], 0, sizeof(struct login_guard_data));
			break;
		}
	}
	pthread_mutex_unlock(&login_guard_mutex);
}

void http_send_login(int sock, http_request *req, int error){
	char http_buf[4096];
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, "<TITLE>Login - %s</TITLE>\n", cfg.http.title);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_writestr(&tcpbuf, sock, "<style>");
	tcp_write(&tcpbuf, sock, style_css, strlen(style_css) );
	tcp_writestr(&tcpbuf, sock, "</style>");
	tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_writestr(&tcpbuf, sock, "<body><div class='login-page'><div class='login-box'>");
	tcp_writestr(&tcpbuf, sock, "<span class='brand'>MultiCS r1000</span>");
	tcp_writestr(&tcpbuf, sock, "<span class='brand-by'>by Sharillas</span>");
	tcp_writestr(&tcpbuf, sock, "<form method='POST' action='/login'>");
	tcp_writestr(&tcpbuf, sock, "<input type='text' name='user' placeholder='User' autocomplete='username'>");
	tcp_writestr(&tcpbuf, sock, "<input type='password' name='pass' placeholder='Password' autocomplete='current-password'>");
	tcp_writestr(&tcpbuf, sock, "<input type='submit' value='Login'>");
	tcp_writestr(&tcpbuf, sock, "</form>");
	if (error==1) tcp_writestr(&tcpbuf, sock, "<div class='login-error'>Invalid user or password</div>");
	else if (error==2) tcp_writestr(&tcpbuf, sock, "<div class='login-error'>Too many failed attempts. Try again later.</div>");
	tcp_writestr(&tcpbuf, sock, "</div></div></body></html>");
	tcp_flush(&tcpbuf, sock);
}

void http_login_submit(int sock, http_request *req)
{
	if (req->dbf.data) explode_post(req, req->dbf.data);
	char user[256] = "", pass[256] = "";
	int i;
	for (i=0; i<req->postcount; i++) {
		if (!strcmp(req->postlist[i].name, "user")) strcpy(user, req->postlist[i].value);
		else if (!strcmp(req->postlist[i].name, "pass")) strcpy(pass, req->postlist[i].value);
	}
	// login guard: IP bloqueado? nem verifica a password
	if ( login_guard_check(req->ip) ) {
		mlogf(LOGINFO,DBG_HTTP," http: login guard - ip %s bloqueado, pedido rejeitado\n", (char*)ip2string(req->ip));
		http_send_login(sock, req, 2);
		return;
	}
	if (user[0] && !strcmp(user, cfg.http.user) && !strcmp(pass, cfg.http.pass)) {
		login_guard_clear(req->ip);
		char *token = http_session_new();
		char http_buf[512];
		struct tcp_buffer_data tcpbuf;
		tcp_init(&tcpbuf);
		sprintf( http_buf, "HTTP/1.1 302 Found\r\nSet-Cookie: multics_session=%s; Path=/; HttpOnly; SameSite=Lax; Max-Age=86400\r\nCache-Control: no-cache, no-store, must-revalidate\r\nLocation: /dashboard\r\nConnection: close\r\n\r\n", token);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_flush(&tcpbuf, sock);
		mlogf(LOGINFO,DBG_HTTP," http: login successful for user '%s'\n", user);
	}
	else {
		login_guard_fail(req->ip);
		mlogf(LOGINFO,DBG_HTTP," http: login failed for user '%s' from ip %s\n", user, (char*)ip2string(req->ip));
		http_send_login(sock, req, 1);
	}
}

void http_logout(int sock, http_request *req)
{
	char *token = http_get_cookie(req, "multics_session");
	if (token) http_session_del(token);
	char http_buf[512];
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	sprintf( http_buf, "HTTP/1.1 302 Found\r\nSet-Cookie: multics_session=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0\r\nLocation: /login\r\nConnection: close\r\n\r\n");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_flush(&tcpbuf, sock);
}

static int emu_parsehex16(const char *s, uint8_t out[16])
{
	int n=0;
	while (*s && n<16) {
		if (*s==' '||*s=='\t') { s++; continue; }
		uint8_t hi, lo;
		char c = *s;
		if (c>='0'&&c<='9') hi = c-'0';
		else if (c>='a'&&c<='f') hi = c-'a'+10;
		else if (c>='A'&&c<='F') hi = c-'A'+10;
		else return -1;
		c = *(s+1);
		if (c>='0'&&c<='9') lo = c-'0';
		else if (c>='a'&&c<='f') lo = c-'a'+10;
		else if (c>='A'&&c<='F') lo = c-'A'+10;
		else return -1;
		out[n++] = (hi<<4)|lo;
		s += 2;
	}
	return n;
}

static void emu_readmeta(char *out, int outlen)
{
	out[0] = 0;
	if (!cfg.constcw_file[0]) return;
	char meta[512];
	strncpy(meta, cfg.constcw_file, sizeof(meta)-1);
	meta[sizeof(meta)-1]=0;
	char *slash = strrchr(meta, '/');
	if (!slash) return;
	strcpy(slash+1, "biss_updater.meta");
	FILE *fp = fopen(meta, "r");
	if (!fp) {
		snprintf(out, outlen, "No updater data yet");
		return;
	}
	char buf[1024] = "";
	int len = fread(buf, 1, sizeof(buf)-1, fp);
	fclose(fp);
	buf[len] = 0;
	long long last_check = 0, last_update = 0, added = 0, updated = 0, total = 0;
	sscanf(buf, "{\"last_check\": %lld", &last_check);
	sscanf(buf, "{\"last_update\": %lld", &last_update);
	sscanf(buf, "{\"added\": %lld", &added);
	sscanf(buf, "{\"updated\": %lld", &updated);
	sscanf(buf, "{\"total\": %lld", &total);
	uint32_t ticks = GetTickCount()/1000;
	if (last_update) {
		uint32_t ago = ticks - (uint32_t)last_update;
		snprintf(out, outlen, "Last update: <b>%dh %dm ago</b><br>New keys: <b>+%lld</b><br>Updated: <b>%lld</b><br>Total after update: <b>%lld</b>", ago/3600, (ago/60)%60, added, updated, total);
	}
	else if (last_check) {
		uint32_t ago = ticks - (uint32_t)last_check;
		snprintf(out, outlen, "Last check: <b>%dh %dm ago</b><br>No key changes yet.<br>Total keys: <b>%lld</b>", ago/3600, (ago/60)%60, total);
	}
	else snprintf(out, outlen, "No updater data yet");
}

static int find_tool(const char *name, char *out, int outsz);
static void resolve_cfg_path(const char *name, char *out, int outsz);

void http_send_emulator(int sock, http_request *req)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;

	// ===== ACTIONS =====
	char *str_action = isset_get( req, "action");
	if (str_action && !strcmp(str_action,"delete")) {
		char *caid = isset_get( req, "caid");
		char *provid = isset_get( req, "provid");
		char *sid = isset_get( req, "sid");
		if (caid && provid && sid)
			emu_delkey( (uint16_t)strtol(caid,NULL,16), (uint32_t)strtol(provid,NULL,16), (uint16_t)strtol(sid,NULL,16) );
		http_send_redirect(sock, "/emulator");
		return;
	}
	if (str_action && !strcmp(str_action,"add")) {
		char *sid = isset_get( req, "sid");
		char *cw = isset_get( req, "cw");
		if (sid && cw && sid[0] && cw[0]) {
			uint16_t caid = 0x2600;
			uint32_t provid = 0;
			char *pcaid = isset_get( req, "caid");
			char *pprov = isset_get( req, "provid");
			if (pcaid && pcaid[0]) caid = (uint16_t)strtol(pcaid,NULL,16);
			if (pprov && pprov[0]) provid = (uint32_t)strtol(pprov,NULL,16);
			uint8_t keycw[16];
			int n = emu_parsehex16(cw, keycw);
			if (n==8) memcpy(keycw+8, keycw, 8);
			if ((n==8)||(n==16)) {
				emu_addkey( caid, provid, (uint16_t)strtol(sid,NULL,16), keycw, "", 1 );
				mlogf(LOGINFO,DBG_HTTP," emu: key added %04x:%06x:%04x via web\n", caid, provid, (uint16_t)strtol(sid,NULL,16));
			}
		}
		http_send_redirect(sock, "/emulator");
		return;
	}
	if (str_action && !strcmp(str_action,"updatekey")) {
		static uint32_t lastupdatekey = 0;
		uint32_t now = GetTickCount();
		if (lastupdatekey && ((now-lastupdatekey)<300000)) {
			http_send_text(sock, "<span class='miss'>Aguarda 5 minutos entre atualizacoes</span>");
			return;
		}
		lastupdatekey = now;
		char tool[512];
		if (find_tool("tools_update_softcam.py", tool, sizeof(tool))) {
			sprintf( http_buf, "python3 %s --port %d >/var/tmp/softcam_update.log 2>&1 &", tool, cfg.http.port);
			system(http_buf);
			mlogf(LOGINFO, DBG_HTTP, " http: softcam update iniciado (porta %d)\n", cfg.http.port);
			http_send_text(sock, "<span class='success'>Update SoftCam.Key iniciado. Resultado no Debug Log.</span>");
		}
		else http_send_text(sock, "<span class='miss'>Ferramenta nao encontrada (tools_update_softcam.py)</span>");
		return;
	}
	if (str_action && !strcmp(str_action,"applykeys")) {
		emu_load();
		sprintf( http_buf, "<span class='success'>Reload Keys OK (%d chaves carregadas)</span>", emu_keycount);
		http_send_text(sock, http_buf);
		return;
	}

	// ===== POST multipart (SoftCam.Key upload) =====
	if (req->type==HTTP_POST) {
		char *content = isset_header(req, "Content-Type");
		if (content && !memcmp(content,"multipart/form-data",19)) {
			// boundary
			while (*content!=';') { if (*content==0) break; content++; }
			if (*content==';') {
				content++;
				while (*content==' ') content++;
				if (!memcmp(content,"boundary",8)) {
					while (*content!='=') { if (*content==0) break; content++; }
					if (*content=='=') {
						content++;
						while (*content==' '||*content=='\t') content++;
						char boundary[255];
						char endboundary[255];
						sprintf( boundary, "--%s", content);
						sprintf( endboundary, "\r\n--%s", content);
						char *p = req->dbf.data;
						p = (char*) boyermoore_horspool_memmem( (uint8_t*)p, req->dbf.datasize, (uint8_t*)boundary, strlen(boundary) );
						if (p) {
							p += strlen(boundary);
							if ( *p=='\r' && *(p+1)=='\n' ) {
								p += 2;
								// headers
								char *h = p;
								while ( !(h[0]=='\r'&&h[1]=='\n'&&h[2]=='\r'&&h[3]=='\n') ) {
									if (h[0]==0) break;
									h++;
								}
								char *pdata = h+4;
								char *end = (char*) boyermoore_horspool_memmem( (uint8_t*)pdata, req->dbf.datasize-(pdata-(char*)req->dbf.data), (uint8_t*)endboundary, strlen(endboundary) );
								if (end && end>pdata) {
									int added = emu_parse_softcam( pdata, end-pdata );
									mlogf(LOGINFO,DBG_HTTP," emu: SoftCam.Key upload: %d keys added\n", added);
									if (added>0) sprintf( http_buf, "<span class='success'>Guardado com sucesso: %d chaves novas</span>", added);
									else sprintf( http_buf, "<span class='miss'>Nao encontrei chaves novas nesse ficheiro (ja existiam ou formato errado)</span>");
									http_send_text(sock, http_buf);
									return;
								}
								http_send_text(sock, "<span class='failed'>Nao consegui processar o upload</span>");
								return;
							}
						}
					}
				}
			}
			http_send_redirect(sock, "/emulator");
			return;
		}
	}

	// ===== PAGE =====
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, html_title, cfg.http.title, "Softcam"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
	tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
	tcp_writestr(&tcpbuf, sock, "\nfunction filterKeys(){var q=document.getElementById('keysearch').value.toLowerCase();var t=document.getElementById('keystable');var r=t.querySelectorAll('tbody tr');for(var i=0;i<r.length;i++){r[i].style.display=r[i].textContent.toLowerCase().indexOf(q)>-1?'':'none';}}");
	tcp_writestr(&tcpbuf, sock, "\nfunction uploadSoftcam(e)\n{\n	if(e&&e.preventDefault)e.preventDefault();\n	var f=document.getElementById('softcamform');\n	if(!f)return true;\n	var s=document.getElementById('softcamstatus');\n	if(s)s.innerHTML='<span class=busy>A processar...</span>';\n	var x=new XMLHttpRequest();\n	x.open('POST','/emulator',true);\n	x.onreadystatechange=function()\n	{\n		if(x.readyState==4){\n			if(x.status==200&&s)s.innerHTML=x.responseText;\n			else if(s)s.innerHTML='<span class=failed>Erro HTTP '+x.status+'</span>';\n		}\n	};\n	x.send(new FormData(f));\n	return false;\n}");
	tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	 setautorefresh(autorefresh);\n}");
	tcp_writestr(&tcpbuf, sock, "\n</script>\n");
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
	tcp_write_menu(&tcpbuf, sock, PAGE_EMULATOR);
	tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");

	// stat sections
	tcp_writestr(&tcpbuf, sock, "<div style='display:flex;gap:15px;flex-wrap:wrap;margin:10px 0'>");
	// Emulator Settings
	tcp_writestr(&tcpbuf, sock, "<div class=stat-section style='flex:1;min-width:280px;'>");
	tcp_writestr(&tcpbuf, sock, "<h3 class=stitle >Softcam Settings</h3><div class=stat-value>");
	char emup[512];
	emu_path(emup, sizeof(emup));
	sprintf( http_buf, "Softcam.cfg: <b>%s</b><br>", emup);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (!cfg.constcw_file[0]) {
		tcp_writestr(&tcpbuf, sock, "<span style='font-size:11px;color:#f0ad4e'>CONSTCW FILE nao definido no multics.cfg - a usar o caminho acima (junto do multics.cfg). Adiciona CONSTCW FILE para um caminho proprio.</span><br>");
	}
	sprintf( http_buf, "Keys loaded: <b>%d</b><br>", emu_keycount);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_writestr(&tcpbuf, sock, "</div></div>");
	// Activity Stats
	tcp_writestr(&tcpbuf, sock, "<div class=stat-section style='flex:1;min-width:280px;'>");
	tcp_writestr(&tcpbuf, sock, "<h3 class=stitle >Activity Stats</h3><div class=stat-value>");
	sprintf( http_buf, "Decrypted CWs: <b>%d</b><br>", emu_logcount);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (emu_lastmatch) {
		sprintf( http_buf, "Last match: <b>%us ago</b>", (GetTickCount()-emu_lastmatch)/1000);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	tcp_writestr(&tcpbuf, sock, "</div></div>");
	// Updater Stats
	tcp_writestr(&tcpbuf, sock, "<div class=stat-section style='flex:1;min-width:280px;'>");
	tcp_writestr(&tcpbuf, sock, "<h3 class=stitle >Updater Stats</h3><div class=stat-value>");
	char metaout[512];
	emu_readmeta(metaout, sizeof(metaout));
	tcp_write(&tcpbuf, sock, metaout, strlen(metaout) );
	tcp_writestr(&tcpbuf, sock, "</div></div></div>");

	// Upload + add forms
	tcp_writestr(&tcpbuf, sock, "<div class=stat-section>");
	tcp_writestr(&tcpbuf, sock, "<h3 class=stitle >SoftCam.Key Upload</h3><div class=stat-value><form id='softcamform' method='POST' enctype='multipart/form-data' action='/emulator' onsubmit='return uploadSoftcam(event)'><input type='file' name='softcamkey' accept='.key'>&nbsp;<input type='submit' value='Convert &amp; Load'>&nbsp;<span id='softcamstatus'></span></form><br><input type='button' class='sbutton' value='Update SoftCam.Key' title='Descarrega o SoftCam.Key mais recente e aplica; chaves manuais sao preservadas' onclick=\"btnrequest('/emulator?action=updatekey','keystatus')\">&nbsp;<input type='button' class='sbutton' value='Reload Keys' title='Rele o Softcam.cfg do disco' onclick=\"btnrequest('/emulator?action=applykeys','keystatus')\">&nbsp;<span id='keystatus'></span>&nbsp;<span style='font-size:11px;'>update: download + parse automatico do SoftCam.Key remoto (resultado no Debug Log)</span></div>");
	tcp_writestr(&tcpbuf, sock, "</div>");
	tcp_writestr(&tcpbuf, sock, "<div class=stat-section style='margin:10px 0'>");
	tcp_writestr(&tcpbuf, sock, "<h3 class=stitle >Add BISS Key (CAID 2600)</h3><div class=stat-value><form method='GET' action='/emulator'><input type='hidden' name='action' value='add'><input type='hidden' name='addkey_type' value='biss'>SID: <input type='text' name='sid' placeholder='17ED' style='width:60px;margin-right:8px'>CW (16 or 32 hex): <input type='text' name='cw' placeholder='1A2B3C81...' style='width:280px;margin-right:8px'><input type='submit' value='Add BISS Key'></form></div>");
	tcp_writestr(&tcpbuf, sock, "</div>");
	tcp_writestr(&tcpbuf, sock, "<div class=stat-section style='margin:10px 0'>");
	tcp_writestr(&tcpbuf, sock, "<h3 class=stitle >Add Generic CW Key</h3><div class=stat-value><form method='GET' action='/emulator'><input type='hidden' name='action' value='add'><input type='hidden' name='addkey_type' value='generic'>CAID: <input type='text' name='caid' placeholder='2600' style='width:60px;margin-right:8px'>Provider: <input type='text' name='provid' placeholder='000000' style='width:80px;margin-right:8px'>SID: <input type='text' name='sid' placeholder='17ED' style='width:60px;margin-right:8px'>CW (16 or 32 hex): <input type='text' name='cw' placeholder='1A2B3C81...' style='width:280px;margin-right:8px'><input type='submit' value='Add Key'></form></div>");
	tcp_writestr(&tcpbuf, sock, "</div>");

	// Loaded Keys
	sprintf( http_buf, "<div class=stat-section style='margin:10px 0'><h3 class=stitle >Loaded Keys (%d)</h3>", emu_keycount);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_writestr(&tcpbuf, sock, "<div class=stat-value><input type='text' id='keysearch' onkeyup='filterKeys()' placeholder='Search CAID, SID or channel...' style='width:280px;margin-bottom:8px'><div style='max-height:400px;overflow-y:auto'><table class=maintable id='keystable'><tr><th>CAID</th><th>Provider</th><th>SID</th><th>Channel Name</th><th>CW (32 hex)</th><th>Del</th></tr>");
	struct emu_key_data *k = emu_keys;
	char cwhex[40];
	int i;
	char *p;
	while (k) {
		p = cwhex;
		for (i=0; i<16; i++) { sprintf(p,"%02X", k->cw[i]); p+=2; }
		const char *chn = (k->name[0]) ? k->name : getchname(k->caid, k->provid, k->sid);
		sprintf( http_buf, "<tr><td>%04x</td><td>%06x</td><td>%04x</td><td>%s</td><td class='cwcell'>%s</td><td><a href='/emulator?action=delete&caid=%04x&provid=%06x&sid=%04x' onclick=\"return confirm('Delete this key?')\" class='btn-del'>Delete</a></td></tr>",
			k->caid, k->provid, k->sid, chn, cwhex, k->caid, k->provid, k->sid);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		k = k->next;
	}
	if (!emu_keycount)
		tcp_writestr(&tcpbuf, sock, "<tr><td colspan=6 style='text-align:center;color:#888'>No keys loaded. Upload a SoftCam.Key file or add entries to Softcam.cfg.</td></tr>");
	tcp_writestr(&tcpbuf, sock, "</table></div></div></div>");

	// Decrypted CW Log
	tcp_writestr(&tcpbuf, sock, "<div class=stat-section style='margin:10px 0'><h3 class=stitle >Decrypted CW Log</h3><div style='max-height:300px;overflow-y:auto'><table class=maintable><tr><th>Time</th><th>CAID</th><th>Provider</th><th>SID</th><th>CW (32 hex)</th></tr>");
	int li;
	int shown = 0;
	struct emu_log_data logentry;
	for (li=0; li<emu_logcount && shown<50; li++, shown++) {
		if (!emu_log_get(li, &logentry)) break;
		p = cwhex;
		for (i=0; i<16; i++) { sprintf(p,"%02X", logentry.cw[i]); p+=2; }
		sprintf( http_buf, "<tr><td>%us ago</td><td>%04x</td><td>%06x</td><td>%04x</td><td class='cwcell'>%s</td></tr>",
			(GetTickCount()-logentry.time)/1000, logentry.caid, logentry.provid, logentry.sid, cwhex);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	if (!emu_logcount)
		tcp_writestr(&tcpbuf, sock, "<tr><td colspan=5 style='text-align:center;color:#888'>No decrypted CWs yet</td></tr>");
	tcp_writestr(&tcpbuf, sock, "</table></div></div>");

	tcp_writestr(&tcpbuf, sock, "</div></body></html>");
	tcp_flush(&tcpbuf, sock);
}

void http_send_iptables(int sock, http_request *req)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;

	// ===== ACTIONS =====
	char *str_action = isset_get( req, "action");
	if (str_action) {
		if (!strcmp(str_action,"block")) {
			char *ip = isset_get( req, "ip");
			if (ip && ip[0]) {
				uint32_t ipv4 = inet_addr(ip);
				if (ipv4!=INADDR_NONE) ipblock_add(ipv4);
			}
			http_send_redirect(sock, "/iptables");
			return;
		}
		else if (!strcmp(str_action,"unblock")) {
			char *ip = isset_get( req, "ip");
			if (ip && ip[0]) {
				uint32_t ipv4 = inet_addr(ip);
				if (ipv4!=INADDR_NONE) ipblock_del(ipv4);
			}
			http_send_redirect(sock, "/iptables");
			return;
		}
	}

	// ===== PAGE =====
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, html_title, cfg.http.title, "Iptables"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
	tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
	tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	 setautorefresh(autorefresh);\n}");
	tcp_writestr(&tcpbuf, sock, "\n</script>\n");
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
	tcp_write_menu(&tcpbuf, sock, PAGE_IPTABLES);
	tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");

	// Form bloquear IP
	tcp_writestr(&tcpbuf, sock, "<div class=stat-section style='margin:10px 0'>");
	tcp_writestr(&tcpbuf, sock, "<h3 class=stitle >Block IP</h3><div class=stat-value><form method='GET' action='/iptables'><input type='hidden' name='action' value='block'>IP Address: <input type='text' name='ip' placeholder='192.168.1.100' style='width:200px;margin-right:8px'><input type='submit' value='Block IP'></form></div>");
	tcp_writestr(&tcpbuf, sock, "</div>");

	// Tabela de IPs bloqueados
	sprintf( http_buf, "<div class=stat-section style='margin:10px 0'><h3 class=stitle >Blocked IPs (%d)</h3>", ipblock_count);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (cfg.blockedip_file[0]) {
		sprintf( http_buf, "<div class=stat-value>File: <b>%s</b><br></div>", cfg.blockedip_file);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	tcp_writestr(&tcpbuf, sock, "<table class=maintable><tr><th>IP</th><th>Country</th><th>Blocked since</th><th>Actions</th></tr>");
	int i;
	for (i=0; i<ipblock_count; i++) {
		char *p = getcountrycodebyip(ipblock_list[i].ip);
		char country[64] = "-";
		if (p) {
			char *n = getcountryname(p);
			snprintf(country, sizeof(country), "<img src='/flag_%s.gif' title='%s'> %s", p, n?n:p, p);
		}
		uint32_t blk = (GetTickCount()-ipblock_list[i].time)/1000;
		sprintf( http_buf, "<tr><td>%s</td><td>%s</td><td>%02ud %02d:%02d:%02d</td><td><a href='/iptables?action=unblock&ip=%s' class='btn-del'>Unblock</a></td></tr>",
			(char*)ip2string(ipblock_list[i].ip), country,
			blk/(3600*24), (blk/3600)%24, (blk/60)%60, blk%60,
			(char*)ip2string(ipblock_list[i].ip));
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	if (!ipblock_count)
		tcp_writestr(&tcpbuf, sock, "<tr><td colspan=4 style='text-align:center;color:#888'>No blocked IPs</td></tr>");
	tcp_writestr(&tcpbuf, sock, "</table></div>");

	tcp_writestr(&tcpbuf, sock, "</div></body></html>");
	tcp_flush(&tcpbuf, sock);
}

void http_send_restart(int sock, http_request *req)
{	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, html_title, cfg.http.title, "Restarting"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	// CSS e tema inline (a pagina sobrevive ao restart sem pedidos externos)
	tcp_writestr(&tcpbuf, sock, "<style>");
	tcp_write(&tcpbuf, sock, style_css, strlen(style_css) );
	tcp_writestr(&tcpbuf, sock, "</style>");
	tcp_writestr(&tcpbuf, sock, "<script>var t='light';try{t=localStorage.getItem('theme')||'light';}catch(e){}if(t==='light'){document.documentElement.classList.add('light-mode');}else{document.documentElement.classList.remove('light-mode');}</script>");
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
	tcp_write_menu(&tcpbuf, sock,PAGE_RESTART);
	tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");
	sprintf( http_buf, "<div class='stat-section'><h3 class='stitle'>Restart</h3><div class='stat-value'><script type=\"text/JavaScript\"><!--\nsetTimeout(\"location.href = '/dashboard';\",8000);\n--></script>\n<h3>Restarting %s<br>Please Wait...</h3></div></div>", cfg.http.title);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_writestr(&tcpbuf, sock, "</div></body></html>");
	tcp_flush(&tcpbuf, sock);
	flag_debugfile = 1;
	mlogf(LOGINFO, 0 , " Restart: from http server\n");
	prg.restart = 1;
}


///////////////////////////////////////////////////////////////////////////////
// SERVERS
///////////////////////////////////////////////////////////////////////////////

char *srvtypename(struct server_data *srv)
{
	static char newcamd[] = "Newcamd";
	static char cccam[] = "CCcam";
	static char radegast[] = "Radegast";
#ifdef CACHEEX
	static char cacheex[] = "CacheEX";
	if (srv->cacheex_mode && (srv->type==TYPE_CCCAM)) return cacheex;
#endif
	if (srv->type==TYPE_NEWCAMD) return newcamd;
	if (srv->type==TYPE_CCCAM) return cccam;
	if (srv->type==TYPE_RADEGAST) return radegast;
	return NULL;
}
	

int srv_cardcount(struct server_data *srv, int uphops)
{
	int count=0;
	struct cs_card_data *card = srv->card;
	while (card) {
		if ( (uphops==-1) 
#ifdef CCCAM_CLI
			|| (card->uphops==uphops)
#endif
		) count++;
		card = card->next;
	}
	return count;
}


char *xmlescape( char *str )
{
// "   &quot;
// '   &apos;
// <   &lt;
// >   &gt;
// &   &amp;
	char exml[5000];
	char *src = str;
	char *dest = exml;
	while (*src) {
		switch (*src) {
			case '&':
				memcpy(dest,"&amp;", 5);
				dest +=5;
				break;				
			case '<':
				memcpy(dest,"&lt;", 4);
				dest +=4;
				break;				
			case '>':
				memcpy(dest,"&gt;", 4);
				dest +=4;
				break;				
			case '"':
				memcpy(dest,"&quot;", 6);
				dest +=6;
				break;				
			case '\'':
				memcpy(dest,"&apos;", 6);
				dest +=6;
				break;				
			default:
				*dest = *src;
				dest++;
		}
		src++;
	}
	*dest = 0;
	strcpy( str, exml);
	return str;
}

char *providerID( unsigned short caid, unsigned int provid )
{
	unsigned int caprovid = (caid<<16) | provid;
	struct providers_data *prov = cfg.providers;
	while (prov) {
		if (prov->caprovid==caprovid) return prov->name;
		prov = prov->next;
	}
	return NULL;
}

// nome do pacote/operadora por CAID (posicao + pais) - coluna Cards
char *caid_pkg_name( unsigned short caid );
// ha perfil no nosso projeto para este CAID? (para marcar cards do reader)
static int card_has_profile(uint16_t caid)
{
	struct cardserver_data *cs = cfg.cardserver;
	while (cs) {
		if (cs->card.caid==caid) return 1;
		cs = cs->next;
	}
	return 0;
}

// renderiza os providers de um card AGRUPADOS pelos perfis do projeto
// (uma linha por pacote: "CAID: idents... [Perfil]") - providers fora
// dos perfis ficam de fora (filtro rigoroso)
static void card_groups_html(struct cs_card_data *card, char *out, int outsz)
{
	int any = 0;
	struct cardserver_data *cs = cfg.cardserver;
	while (cs) {
		if (cs->card.caid != card->caid) { cs = cs->next; continue; }
		char provs[512] = "";
		int first = 1, n = 0, i;
		for (i=0; i<card->nbprov; i++) {
			int in = 0, k;
			for (k=0; k<cs->card.nbprov; k++)
				if (cs->card.prov[k].id==card->prov[i]) { in = 1; break; }
			if (!in) continue;
			char *pn = providerID(card->caid, card->prov[i]);
			char t2[160];
			if (pn) snprintf(t2, sizeof(t2), "%s%06x <font color=#CC3300>%s</font>", first?"":" , ", card->prov[i], pn);
			else snprintf(t2, sizeof(t2), "%s%06x", first?"":" , ", card->prov[i]);
			if ( (strlen(provs)+strlen(t2)) < (sizeof(provs)-4) ) { strcat(provs, t2); first = 0; n++; }
		}
		if (n) {
			char line[900];
			snprintf(line, sizeof(line), "<br><b>%04x:</b> %s <font style=\"font-size:8px;color:#8899aa\">[%s]</font>", card->caid, provs, cs->name);
			if ( (strlen(out)+strlen(line)) < (outsz-16) ) strcat(out, line);
			any = 1;
		}
		cs = cs->next;
	}
	if (!any) {
		char line[300];
		char *pn = providerID(card->caid, card->prov[0]);
		char *pkg = caid_pkg_name(card->caid);
		if (pn) snprintf(line, sizeof(line), "<br><b>%04x:</b> %06x <font color=#CC3300>%s</font> <font style=\"font-size:8px;color:#8899aa\">(sem perfil)</font>", card->caid, card->prov[0], pn);
		else if (pkg) snprintf(line, sizeof(line), "<br><b>%04x:</b> %06x <font style=\"font-size:8px;color:#8899aa\">%s (sem perfil)</font>", card->caid, card->prov[0], pkg);
		else snprintf(line, sizeof(line), "<br><b>%04x:</b> %06x <font style=\"font-size:8px;color:#8899aa\">(sem perfil)</font>", card->caid, card->prov[0]);
		if ( (strlen(out)+strlen(line)) < (outsz-16) ) strcat(out, line);
	}
}

static struct { unsigned short caid; char *name; } caid_pkg_table[] = {
	{ 0x1814, "MEO ID (30W Portugal)" },
	{ 0x1813, "Canal+ Polónia nc+ (13E)" },
	{ 0x1802, "NOS ID (30W Portugal)" },
	{ 0x1880, "Digi TV (0.8W Hungria)" },
	{ 0x1810, "Movistar+ (19.2E Espanha)" },
	{ 0x1830, "HD+ HD01 (19.2E Alemanha)" },
	{ 0x1843, "HD+ HD02 (19.2E Alemanha)" },
	{ 0x1860, "HD+ HD03 (19.2E Alemanha)" },
	{ 0x186A, "HD+ HD04 (19.2E Alemanha)" },
	{ 0x188A, "HD+ HD05 (19.2E Alemanha)" },
	{ 0x098C, "Sky DE (19.2E Alemanha)" },
	{ 0x098D, "Sky DE (19.2E Alemanha)" },
	{ 0x1818, "TNT SAT (19.2E Franca)" },
	{ 0x1817, "Canal Digitaal NL (19.2E)" },
	{ 0x181D, "TV Vlaanderen (19.2E Belgica)" },
	{ 0x0D95, "ORF Digital (19.2E Austria)" },
	{ 0x0D96, "Skylink (23.5E)" },
	{ 0x0648, "ORF Digital (19.2E Austria)" },
	{ 0x0650, "ORF Digital (19.2E Austria)" },
	{ 0x1702, "BetaDigital (19.2E Alemanha)" },
	{ 0x1722, "BetaDigital (19.2E Alemanha)" },
	{ 0x1811, "Canal+ France (19.2E Franca)" },
	{ 0x0500, "Viaccess/TNTSAT (19.2E Franca)" },
	{ 0x0963, "Sky UK (28.2E Reino Unido)" },
	{ 0x0960, "Sky UK (28.2E Reino Unido)" },
	{ 0x0961, "Sky UK (28.2E Reino Unido)" },
	{ 0x0B00, "M7 Group (13E)" },
	{ 0x0B01, "NC+ Conax (13E Polonia)" },
	{ 0x1870, "Polsat Box (13E Polonia)" },
	{ 0x0B02, "Focus Sat (0.8W Romenia)" },
	{ 0x1884, "Canal+ Polónia (13E Polonia)" },
	{ 0x0100, "SECA/Mediaguard (13E)" },
	{ 0x1803, "Polsat Box (13E Polonia)" },
	{ 0x1861, "Polsat Box (13E Polonia)" },
	{ 0x186C, "Polsat Box (13E Polonia)" },
	{ 0x0604, "Nova (13E Grecia)" },
	{ 0x0699, "Nova (13E Grecia)" },
	{ 0x4A70, "KABELIO (13E Suica)" },
	{ 0x4AFC, "SSR/SRG (13E Suica)" },
	{ 0x183D, "TivuSat/RAI (13E Italia)" },
	{ 0x183E, "TivuSat/RAI (13E Italia)" },
	{ 0x0919, "Sky Italia (13E Italia)" },
	{ 0x093B, "Sky Italia (13E Italia)" },
	{ 0x09CD, "Sky Italia (13E Italia)" },
	{ 0x09BD, "Vivacom (13E Bulgaria)" },
	{ 0x1880, "Digi TV (0.8W Hungria)" },
	{ 0x0624, "Skylink (23.5E) / Austriasat-Canal+ AT (19.2E)" },
	{ 0x090F, "Viasat (4.8E)" },
	{ 0x093E, "Viasat (4.8E)" },
	{ 0x1887, "HD+ Astra (19.2E Alemanha)" },
	{ 0x1819, "NAGRA (Europa)" },
	{ 0x4AEE, "Bulsatcom (1.9E Bulgaria)" },
	{ 0x0D00, "Cryptoworks/Turksat (42E)" },
	{ 0, NULL }
};

char *caid_pkg_name( unsigned short caid )
{
	int i;
	for (i=0; caid_pkg_table[i].caid; i++) {
		if (caid_pkg_table[i].caid==caid) return caid_pkg_table[i].name;
	}
	return NULL;
}

void getservercells(struct server_data *srv, char cell[8][8192] )
{
	char temp[2048];
	unsigned int ticks = GetTickCount();
	uint32_t d;
	int i;
	memset(cell, 0, 8*2048);
	// CELL0
	uint32_t uptime;
	if (srv->connection.status>0) uptime = (ticks-srv->connection.time) + srv->connection.uptime; else uptime = srv->connection.uptime;
	d = uptime / (ticks/100);
	uptime /= 1000;
	sprintf( cell[0],"<span title='%02dd %02d:%02d:%02d'>%d%%</span>",uptime/(3600*24),(uptime/3600)%24,(uptime/60)%60,uptime%60 ,d);

	// CELL1
	sprintf( cell[1],"<a href=\"/server?id=%d\">%s:%d</a><br>", srv->id,srv->host->name,srv->port);
	if (!srv->host->ip && srv->host->clip)
		sprintf( temp,"0.0.0.0 (%s)",(char*)ip2string(srv->host->ip) );
	else {
		char *p = getcountrycodebyip(srv->host->ip);
		if (p) sprintf( temp,"<img src='/flag_%s.gif' title='%s'> %s", p, getcountryname(p), (char*)ip2string(srv->host->ip) ); else sprintf( temp,"%s",(char*)ip2string(srv->host->ip) );
	}
	strcat( cell[1], temp );
	// CELL2
	if (srv->type==TYPE_NEWCAMD) {
		if (srv->progname) {
			if (srv->version) sprintf( cell[2],"%s %s", srv->progname, srv->version);
			else strcpy( cell[2], srv->progname);
		}
		else sprintf( cell[2],"Newcamd v6.06");
	}
#ifdef CCCAM_CLI
	else if (srv->type==TYPE_CCCAM) {
		if (srv->handle>0)
			sprintf( cell[2],"%s %s<br>%02x%02x%02x%02x%02x%02x%02x%02x", srv->progname, srv->version, srv->nodeid[0],srv->nodeid[1],srv->nodeid[2],srv->nodeid[3],srv->nodeid[4],srv->nodeid[5],srv->nodeid[6],srv->nodeid[7]);
		else sprintf( cell[2],"CCcam");
#ifdef CACHEEX
		if (srv->cacheex_mode) strcat( cell[2], "<br>CacheEX");
#endif
		//if (srv->progname) sprintf( cell[2],"<td>CCcam(%s) %s", srv->progname, srv->version); else sprintf( cell[2],"<td>CCcam %s", srv->version);
	}
#endif
#ifdef RADEGAST_CLI
	else if (srv->type==TYPE_RADEGAST) sprintf( cell[2],"Cs357x UDP v0.3.x");
#endif
#ifdef CAMD35_CLI
	else if (srv->type==TYPE_CAMD35) sprintf( cell[2],"Camd35 v0.3.x");
#endif
#ifdef CS378X_CLI
	else if (srv->type==TYPE_CS378X) sprintf( cell[2],"Cs738x TCP v0.3.x");
#endif
	else if (srv->type==TYPE_CCAM3) {
		if (srv->handle>0 && srv->version[0]) sprintf( cell[2],"CCcam3 v%s", srv->version);
		else sprintf( cell[2],"CCcam3");
	}
	else sprintf( cell[2],"Unknown");

	// CELL3
	if (srv->connection.status>0) {
		d = (ticks-srv->connection.time)/1000;
		sprintf( cell[3],"%02dd %02d:%02d:%02d", d/(3600*24),(d/3600)%24,(d/60)%60,d%60);
		if (srv->busy) sprintf( cell[7],"busy"); else sprintf( cell[7],"online");
	}
	else {
		sprintf( cell[7],"offline");
		if (srv->flags&FLAG_DELETE) sprintf( cell[3],"Removed");
		else if (srv->flags&FLAG_EXPIRED) sprintf( cell[3],"Expired");
		else if (srv->flags&FLAG_DISABLE) sprintf( cell[3],"Disabled");
		else sprintf( cell[3],"offline");
	}

#ifdef CCCAM_CLI
#ifdef CACHEEX
	if (srv->cacheex_mode) {
		sprintf( cell[4],"%d",srv->ecmnb);
		strcpy( cell[5], " "); // default
	}
	else
#endif
#endif
	{
		// CELL4
		if (srv->ecmnb)
			sprintf( cell[4],"%d / %d<span style=\"float: right;\">%d%%</span><br>Hits = %d",srv->ecmok ,srv->ecmnb, srv->ecmnb?((srv->ecmok*100)/srv->ecmnb):0, srv->hits);
		else
			sprintf( cell[4],"<span style=\"float: right;\">0%%</span>");
		// Health score (quando HEALTH ativo nalgum perfil deste server)
		{
			int henabled = 0;
			int hscore = srv_healthscore_gui(srv, &henabled);
			if (henabled) {
				if (hscore) sprintf( temp,"<br>Health = %d/1000", hscore);
				else sprintf( temp,"<br>Health = --");
				strcat( cell[4], temp );
			}
		}
		// CELL5
		if (srv->ecmok)
			sprintf( cell[5],"%d ms", srv->ecmok?((srv->ecmoktime/srv->ecmok)):0 ); //, srv->hits );
		else
			sprintf( cell[5],"-- ms");
	}

	// CELL6
	strcpy( cell[6], " "); // default
	if (srv->connection.status>0) {
		if (srv->type==TYPE_CCCAM)
			sprintf( temp,"<b>Total Cards = %d</b> ( Hop1 = %d, Hop2 = %d )<br>", srv_cardcount(srv,-1), srv_cardcount(srv,1), srv_cardcount(srv,2) );
		else
			sprintf( temp,"<b>Total Cards = %d</b><br>", srv_cardcount(srv,-1) );
		strcpy( cell[6], temp );
		// RESUMO COMPACTO por CAID (v1.26): a linha nunca estoura, mesmo com
		// dezenas de cards. So os CAIDs dos nossos perfis (ate 12) + contagem
		// dos restantes. Detalhe completo fica na pagina /server?id=.
		int caids[64]; int counts[64]; int ncaid = 0;
		struct cs_card_data *card = srv->card;
		while (card) {
			int k, found = -1;
			for (k=0; k<ncaid; k++) if (caids[k]==card->caid) { found = k; break; }
			if (found<0) {
				if (ncaid<64) { caids[ncaid] = card->caid; counts[ncaid] = 1; ncaid++; }
			}
			else counts[found]++;
			card = card->next;
		}
		int k;
		int shown = 0, others = 0;
		char tmp2[96];
		for (k=0; k<ncaid; k++) {
			if ( caid_in_profiles(caids[k]) && (shown<12) ) {
				sprintf( tmp2, "<span style='white-space:nowrap'>%04X:<b>%d</b></span> ", caids[k], counts[k]);
				if ( (strlen(cell[6])+strlen(tmp2)) < (sizeof(cell[6])-96) ) { strcat( cell[6], tmp2 ); shown++; }
			}
			else others += counts[k];
		}
		if (others) {
			sprintf( tmp2, "<span style='color:#8899aa;font-size:10px'>(+%d fora dos perfis)</span>", others);
			if ( (strlen(cell[6])+strlen(tmp2)) < (sizeof(cell[6])-96) ) strcat( cell[6], tmp2 );
		}
		strcat( cell[6], "<br>" );
	}
	else {
		if (srv->statmsg) {
			if (srv->connection.lastseen) {
				d = (ticks-srv->connection.lastseen)/1000;
				sprintf( temp,"%s<br>Last Seen %02dd %02d:%02d:%02d", srv->statmsg, d/(3600*24),(d/3600)%24,(d/60)%60,d%60);
			}
			else sprintf( temp,"%s",srv->statmsg);
			strcpy( cell[6], temp );
		}
	}

	// botoes em linha propria (guard de tamanho em todos os strcat)
	#define CELL6ADD(s) if ( (strlen(cell[6])+strlen(s)) < (sizeof(cell[6])-16) ) strcat( cell[6], s )
	CELL6ADD("<br><span style='display:inline-flex;gap:2px;white-space:nowrap;margin-top:4px;'>");
	if ( !(srv->flags&(FLAG_DELETE|FLAG_EXPIRED)) ) {
		if (srv->flags&FLAG_DISABLE) {
			sprintf( temp," <span class='icobtn on' title='Enable' onclick=\"imgrequest('/server?id=%d&action=enable',this);setTimeout('updateDiv()',600)\">ON</span>",srv->id);
			CELL6ADD(temp);
		}
		else {
			sprintf( temp," <span class='icobtn off' title='Disable' onclick=\"imgrequest('/server?id=%d&action=disable',this);setTimeout('updateDiv()',600)\">OFF</span>",srv->id);
			CELL6ADD(temp);
		}
	}
	sprintf( temp," <span class='icobtn dbg' title='Debug' onclick=\"toggleDbgRow(%d,'/server?id=%d&action=dbginfo')\">DBG</span>",srv->id,srv->id);
	CELL6ADD(temp);
	sprintf( temp," <span class='icobtn inf' title='Info completa do server (todos os cards e detalhes)' onclick=\"location.href='/server?id=%d'\">INF</span>",srv->id);
	CELL6ADD(temp);
	CELL6ADD("</span>");
	#undef CELL6ADD
}

void alltotal_servers( int *all, int *cccam, int *newcamd, int *radegast )
{
	*all = 0;
	*cccam = 0;
	*newcamd = 0;
	*radegast = 0;

	struct server_data *srv=cfg.server;
	while (srv) {
		(*all)++;
		if (srv->type==TYPE_CCCAM) (*cccam)++;
		else if (srv->type==TYPE_NEWCAMD) (*newcamd)++;
		else if (srv->type==TYPE_RADEGAST) (*radegast)++;
		srv=srv->next;
	}
}

void allconnected_servers( int *all, int *cccam, int *newcamd, int *radegast )
{
	*all = 0;
	*cccam = 0;
	*newcamd = 0;
	*radegast = 0;

	struct server_data *srv=cfg.server;
	while (srv) {
		if ( !IS_DISABLED(srv->flags)&&(srv->handle>0) ) {
			(*all)++;
			if (srv->type==TYPE_CCCAM) (*cccam)++;
			else if (srv->type==TYPE_NEWCAMD) (*newcamd)++;
			else if (srv->type==TYPE_RADEGAST) (*radegast)++;
		}
		srv=srv->next;
	}
}

void http_send_servers(int sock, http_request *req)
{
	char http_buf[5000];
	struct tcp_buffer_data tcpbuf;

	char cell[8][8192];
	struct server_data *srv;
	int i;

	// Action
	char *str_action = isset_get( req, "action");
	int get_action = 0;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = 1;
		else if (!strcmp(str_action,"row")) get_action = 2;
		else str_action = NULL;
	}
	if (!str_action) str_action = "page";
	//
	char *str_type = isset_get( req, "type");
	int get_type = 0;
	if (str_type) {
		if (!strcmp(str_type,"cccam"))  get_type = 1;
		else if (!strcmp(str_type,"newcamd")) get_type = 2;
		else if (!strcmp(str_type,"radegast")) get_type = 3;
		else str_type = NULL;
	}
	if (!str_type) str_type = "all";
	//
	char *str_list = isset_get( req, "list");
	int get_list = LIST_ALL;
	if (str_list) {
		if (!strcmp(str_list,"connected")) get_list = LIST_CONNECTED;
		else if (!strcmp(str_list,"disconnected")) get_list = LIST_DISCONNECTED;
		else str_list = NULL;
	}
	if (!str_list) str_list = "all";

	//
	char *id = isset_get( req, "id");
	// Get Server ID
	if (id)	{
		i = atoi(id);
		//look for server
		srv = cfg.server;
		while (srv) {
			if (!(srv->flags&FLAG_DELETE)) {
				if (srv->id==(uint32_t)i) break;
			}
			srv = srv->next;
		}
		if (!srv) return;
		char *action = isset_get( req, "action");
		if (action) {
			if (!strcmp(action,"disable")) {
				srv->flags |= FLAG_DISABLE;
				if (srv->connection.status>0) disconnect_srv(srv);
			}
			else if (!strcmp(action,"enable")) {
				srv->flags &= ~FLAG_DISABLE;
				srv->host->checkiptime = 0;
			}
		}			
		// Send XML CELLS
		getservercells(srv,cell);
		// FIX v1.26: coluna Cards pode ser grande -> cap antes de escapar
		// (o row XML e cosmetico; o div refresca completo a cada ciclo)
		if (strlen(cell[6])>3000) cell[6][3000] = 0;
		for(i=0; i<8; i++) xmlescape( cell[i] );
		char buf[5000] = "";
		snprintf( buf, sizeof(buf), "<server>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2>%s</c2>\n<c3_c>%s</c3_c>\n<c3>%s</c3>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6>\n</server>\n",cell[0],cell[1],cell[2],cell[7],cell[3],cell[4],cell[5],cell[6] );
		http_send_xml( sock, req, buf, strlen(buf));
		return;
	}

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	if (get_action==0) {
		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, html_title, cfg.http.title, "Servers"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		// JS
		tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
        tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
		// ACTIONS REQUEST
		tcp_writestr(&tcpbuf, sock, "\nfunction imgrequest( url, el )\n{\n	var httpRequest;\n	try { httpRequest = new XMLHttpRequest(); }\n	catch (trymicrosoft) { try { httpRequest = new ActiveXObject('Msxml2.XMLHTTP'); } catch (oldermicrosoft) { try { httpRequest = new ActiveXObject('Microsoft.XMLHTTP'); } catch(failed) { httpRequest = false; } } }\n	if (!httpRequest) { alert('Your browser does not support Ajax.'); return false; }\n	if ( typeof(el)!='undefined' ) {\n		el.onclick = null;\n		el.style.opacity = '0.7';\n		httpRequest.onreadystatechange = function()\n		{\n			if (httpRequest.readyState == 4) if (httpRequest.status == 200) el.style.opacity = '0.3';\n		}\n	}\n	httpRequest.open('GET', url, true);\n	httpRequest.send(null);\n}\n");
		// UPD ROW
		tcp_writestr(&tcpbuf, sock, "\nfunction xmlupdateRow( xmlDoc, id )\n{\n	var row = document.getElementById(id);\n	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n	row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n	row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n	row.cells.item(3).className = xmlDoc.getElementsByTagName('c3_c')[0].childNodes[0].nodeValue;\n	row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n	row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n	row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n	row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n}\n" );
		char url[256];
		sprintf( url, "'/servers?id='+idx");
		sprintf( http_buf, HTTP_UPDATE_ROW, url);
/////"\nvar idx = 0;\nvar lastidx = 0;\nvar requestError = 0;\nfunction updateRow()\n{\n	if (lastidx!=idx) {\n		requestError = 0;\n		lastidx = idx;\n	}\n	if ( !requestError && (idx>0) ) {\n		var httpRequest;\n		try {\n			httpRequest = new XMLHttpRequest();  // Mozilla, Safari, etc\n		}\n		catch(trymicrosoft) {\n			try {\n				httpRequest = new ActiveXObject('Msxml2.XMLHTTP');\n			}\n			catch(oldermicrosoft) {\n				try {\n					httpRequest = new ActiveXObject('Microsoft.XMLHTTP');\n				}\n				catch(failed) {\n					httpRequest = false;\n				}\n			}\n		}\n		if (!httpRequest) {\n			alert('Your browser does not support Ajax.');\n			return false;\n		}\n		var savedidx = idx;\n		// Action http_request\n		httpRequest.onreadystatechange = function()\n		{\n			if (httpRequest.readyState == 4) {\n				if (httpRequest.status == 200) {\n					requestError=0;\n					xmlupdateRow( httpRequest.responseXML, 'Row'+savedidx );\n				}\n				else {\n					requestError++;\n				}\n				t = setTimeout('updateRow()',1000);\n			}\n		}\n		httpRequest.open('GET', %s, true);\n		httpRequest.send(null);\n		requestError++;\n	} else t = setTimeout('updateRow()',1000);\n}\n"
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// UPD DIV
		sprintf( url, "/servers?action=div&type=%s&list=%s", str_type, str_list);
		sprintf( http_buf, HTTP_UPDATE_DIV, cfg.http.autorefresh*1000, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	setautorefresh(autorefresh);\n}");
		tcp_writestr(&tcpbuf, sock, "\n</script>\n");
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
		tcp_write_menu(&tcpbuf, sock,PAGE_SERVERS);
		// DIV
		tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");
	}
	//
	int iall, icccam, inewcamd, iradegast; // Total
	alltotal_servers( &iall, &icccam, &inewcamd, &iradegast );
	int jall, jcccam, jnewcamd, jradegast; // Connected
	allconnected_servers( &jall, &jcccam, &jnewcamd, &jradegast );
	//
	int connected = jall;
	int total = iall;
	if (get_type==1) { connected=jcccam; total=icccam; }
	else if (get_type==2) { connected=jnewcamd; total=inewcamd; }
	else if (get_type==3) { connected=jradegast; total=iradegast; }
	//
	tcp_writestr(&tcpbuf, sock, "<select style=\"width:200px;\" onchange=\"parent.location.href='/servers?type='+this.value\">");
	sprintf( http_buf, "<option value=all>All Servers (%d/%d)</option>",jall, iall );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (inewcamd) {
		if (get_type==2) sprintf( http_buf, "<option value=newcamd selected>Newcamd Servers (%d/%d)</option>",jnewcamd,inewcamd );
		else sprintf( http_buf, "<option value=newcamd>Newcamd Servers (%d/%d)</option>",jnewcamd,inewcamd );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	if (icccam) {
		if (get_type==1) sprintf( http_buf, "<option value=cccam selected>CCcam Servers (%d/%d)</option>",jcccam,icccam );
		else sprintf( http_buf, "<option value=cccam>CCcam Servers (%d/%d)</option>",jcccam,icccam );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	if (iradegast) {
		if (get_type==3) sprintf( http_buf, "<option value=radegast>Radegast Servers (%d/%d)</option>",jradegast,iradegast );
		else sprintf( http_buf, "<option value=radegast selected>Radegast Servers (%d/%d)</option>",jradegast,iradegast );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	tcp_writestr(&tcpbuf, sock, "</select>");
	//
	char *class1 = "button"; char *class2 = "sbutton";
	char *class;
	if (get_list==LIST_ALL) class = class2; else class = class1;
	sprintf( http_buf," <input type=button class=%s onclick=\"parent.location='/servers?type=%s&amp;list=all'\" value='All (%d)'>",class,str_type,total);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (get_list==LIST_CONNECTED) class = class2; else class = class1;
	sprintf( http_buf," <input type=button class=%s onclick=\"parent.location='/servers?type=%s&amp;list=connected'\" value='Connected (%d)'>",class,str_type,connected);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (get_list==LIST_DISCONNECTED) class = class2; else class = class1;
	sprintf( http_buf," <input type=button class=%s onclick=\"parent.location='/servers?type=%s&amp;list=disconnected'\" value='Disconnected (%d)'>",class,str_type,total-connected);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// Table
	sprintf( http_buf, "<br><table class=maintable width=100%%>\n<tr><th width=20px>Uptime</th><th width=200px>Host</th><th width=100px>Server</th><th width=100px>Connected</th><th width=150px>Ecm OK</th><th width=50px>EcmTime</th><th>Cards</th></tr>\n");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	srv = cfg.server;
	int alt = 0;

	if (get_type==0) {
		while (srv) {
			if (!(srv->flags&FLAG_DELETE))
			if ( ((get_list&LIST_CONNECTED)&&(srv->handle>0))||((get_list&LIST_DISCONNECTED)&&(srv->handle<=0)) ) {
				if (alt==1) alt=2; else alt=1;
				getservercells(srv,cell);
				snprintf( http_buf, sizeof(http_buf),"<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'><td align=\"center\">%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%s</td><td align=\"center\">%s</td><td>%s</td></tr>\n",srv->id,alt,srv->id,cell[0],cell[1],cell[2],cell[7],cell[3],cell[4],cell[5],cell[6]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			srv = srv->next;
		}
	}
	else if (get_type==1) {
		while (srv) {
			if (!(srv->flags&FLAG_DELETE))
			if (srv->type==TYPE_CCCAM)
			if ( ((get_list&LIST_CONNECTED)&&(srv->handle>0))||((get_list&LIST_DISCONNECTED)&&(srv->handle<=0)) ) {
				if (alt==1) alt=2; else alt=1;
				getservercells(srv,cell);
				snprintf( http_buf, sizeof(http_buf),"<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'><td align=\"center\">%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%s</td><td align=\"center\">%s</td><td>%s</td></tr>\n",srv->id,alt,srv->id,cell[0],cell[1],cell[2],cell[7],cell[3],cell[4],cell[5],cell[6]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			srv = srv->next;
		}
	}
	else if (get_type==2) {
		while (srv) {
			if (!(srv->flags&FLAG_DELETE))
			if (srv->type==TYPE_NEWCAMD)
			if ( ((get_list&LIST_CONNECTED)&&(srv->handle>0))||((get_list&LIST_DISCONNECTED)&&(srv->handle<=0)) ) {
				if (alt==1) alt=2; else alt=1;
				getservercells(srv,cell);
				snprintf( http_buf, sizeof(http_buf),"<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'><td align=\"center\">%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%s</td><td align=\"center\">%s</td><td>%s</td></tr>\n",srv->id,alt,srv->id,cell[0],cell[1],cell[2],cell[7],cell[3],cell[4],cell[5],cell[6]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			srv = srv->next;
		}
	}
	else if (get_type==3) {
		while (srv) {
			if (!(srv->flags&FLAG_DELETE))
			if (srv->type==TYPE_RADEGAST)
			if ( ((get_list&LIST_CONNECTED)&&(srv->handle>0))||((get_list&LIST_DISCONNECTED)&&(srv->handle<=0)) ) {
				if (alt==1) alt=2; else alt=1;
				getservercells(srv,cell);
				snprintf( http_buf, sizeof(http_buf),"<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'><td align=\"center\">%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%s</td><td align=\"center\">%s</td><td>%s</td></tr>\n",srv->id,alt,srv->id,cell[0],cell[1],cell[2],cell[7],cell[3],cell[4],cell[5],cell[6]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			srv = srv->next;
		}
	}
	tcp_writestr(&tcpbuf, sock, "</table>");

	// ===== ECM DEDUP (1 pedido unico por ECM em voo) =====
	// renderizado no page E no div (autorefresh AJAX) para nao desaparecer
	if ( (get_action==0)||(get_action==ACTION_DIV) ) {
		tcp_writestr(&tcpbuf, sock, "<div class=stat-section style='margin:10px 0'><h3 class=stitle >ECM Dedup (todos os readers)</h3><div class=stat-value>");
		if ((g_ecm_unique+g_ecm_dedup)>0) {
			sprintf( http_buf, "Pedidos unicos ao reader: <b>%d</b> | Repetidos evitados (dedup): <b>%d</b> (%d%%%%)<br>", g_ecm_unique, g_ecm_dedup, (g_ecm_unique+g_ecm_dedup)?((g_ecm_dedup*100)/(g_ecm_unique+g_ecm_dedup)):0);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		else tcp_writestr(&tcpbuf, sock, "Sem atividade de ECM ainda.<br>");
		// top de canais com mais dedup
		uint16_t dcaid[8]; uint16_t dsid[8]; uint32_t duni[8]; uint32_t dded[8];
		int dn = dedup_ch_top(8, dcaid, dsid, duni, dded);
		if (dn>0) {
			tcp_writestr(&tcpbuf, sock, "<table class=maintable style='margin-top:6px'><tr><th>Canal</th><th>CAID</th><th>SID</th><th>Unicos</th><th>Evitados</th></tr>");
			int di;
			for (di=0; di<dn; di++) {
				char *chname = NULL;
				struct chninfo_data *chn = cfg.chninfo;
				while (chn) {
					if ( (chn->caid==dcaid[di]) && (chn->sid==dsid[di]) ) { chname = chn->name; break; }
					chn = chn->next;
				}
				sprintf( http_buf, "<tr><td>%s</td><td>%04X</td><td>%04X</td><td>%u</td><td><b>%u</b></td></tr>", chname?chname:"-", dcaid[di], dsid[di], duni[di], dded[di]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			tcp_writestr(&tcpbuf, sock, "</table>");
		}
		tcp_writestr(&tcpbuf, sock, "</div></div>");
	}

	if (get_action==0) {
		tcp_writestr(&tcpbuf, sock, "</div></body></html>");
	}
	tcp_flush(&tcpbuf, sock);
}



void http_send_server(int sock, http_request *req)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;
	char *provname;
	char *pkgname;

	//
	int get_id;
	char *str_id = isset_get( req, "id");
	if (str_id)	get_id = atoi(str_id); else return;

	//look for server
	struct server_data *srv = getsrvbyid( get_id );
	if (!srv) {
		tcp_init(&tcpbuf);
		tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
		sprintf( http_buf, "<br>Server not found (id=%d)<br>", get_id);
		tcp_flush(&tcpbuf, sock);
		return;
	}
	//
	// Action
	char *str_action = isset_get( req, "action");
	int get_action = 0;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = 1;
		else if (!strcmp(str_action,"row")) get_action = 2;
		else if (!strcmp(str_action,"disable")) get_action = 3;
		else if (!strcmp(str_action,"enable")) get_action = 4;
		else if (!strcmp(str_action,"status")) get_action = 5;
		else if (!strcmp(str_action,"info")) get_action = 6; // XML info
		else if (!strcmp(str_action,"debug")) get_action = 7;
		else if (!strcmp(str_action,"dbginfo")) get_action = 8;
		else str_action = NULL;
	}
	if (!str_action) str_action = "page";
	//
	if (get_action==8) {
		char dbg[1024];
		sprintf( dbg, "<div class='dbginfo'><b>%s:%d</b> | Type: %s | Status: %s | Busy: %s<br>ECM: %d pedidos, %d OK (%d%%) | Hits: %d | %s</div>",
			srv->host? (char*)srv->host->name : "?", srv->port,
			(srv->type==TYPE_NEWCAMD)?"Newcamd":(srv->type==TYPE_CCCAM)?"CCcam":(srv->type==TYPE_CAMD35)?"Camd35":(srv->type==TYPE_CS378X)?"cs378x":"Radegast",
			srv->connection.status>0?"CONNECTED":(srv->connection.status<0?"CONNECTING...":"OFFLINE"),
			srv->busy?"yes":"no",
			srv->ecmnb, srv->ecmok, srv->ecmnb?(srv->ecmok*100)/srv->ecmnb:0, srv->hits,
			srv->statmsg?srv->statmsg:"");
		http_send_text(sock, dbg);
		return;
	}
	if (get_action==3) {
		srv->flags |= FLAG_DISABLE;
		if (srv->connection.status>0) disconnect_srv(srv);
		http_send_ok(sock);
		return;
	}
	else if (get_action==4) {
		srv->flags &= ~FLAG_DISABLE;
		srv->host->checkiptime = 0;
		http_send_ok(sock);
		return;
	}
	else if (get_action==5) {
		if (srv->handle>0) http_send_text(sock,"connected"); else http_send_text(sock,"disconnected");
		tcp_flush(&tcpbuf, sock);
		return;
	}
	else if (get_action==7) {
		flagdebug = getdbgflag( DBG_SERVER, 0, srv->id);
		http_send_ok(sock);
		return;
	}
	//

	// Send Server infoPage
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, html_title, cfg.http.title, "Server"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
	tcp_write_menu(&tcpbuf, sock,0);

	tcp_writestr(&tcpbuf, sock, "<table width=100%><tr><td style=\"vertical-align:top; width:40%\">");
	//
	tcp_writestr(&tcpbuf, sock, "<table class=infotable><tbody>\n<tr><th colspan=2>Server Informations</th></tr>\n" );
	// Host:Port
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Host</td><td class=right>%s : %d</td></tr>\n", srv->host->name, srv->port);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// Server Type
	tcp_writestr(&tcpbuf, sock, "<tr><td class=left>Type</td><td class=right>");
	if (srv->type==TYPE_CCCAM) tcp_writestr(&tcpbuf, sock, "CCcam</td></tr>\n");
	else if (srv->type==TYPE_NEWCAMD) tcp_writestr(&tcpbuf, sock, "Newcamd</td></tr>\n");
	else if (srv->type==TYPE_RADEGAST) tcp_writestr(&tcpbuf, sock, "Radegast</td></tr>\n");
	// USER
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>User</td><td class=right>%s</td></tr>\n",srv->user );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// Connection Time
	if (srv->connection.status>0) {
		tcp_writestr(&tcpbuf, sock, "<tr><td class=left>Status</td><td class=right>Connected</td></tr>\n");
		uint32_t d = (GetTickCount()-srv->connection.time)/1000;
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Connection time</td><td class=right>%02dd %02d:%02d:%02d</td></tr>\n", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// IP
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>IP Address</td><td class=right>%s</td></tr>\n",(char*)ip2string(srv->host->ip) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		if (srv->type==TYPE_CCCAM) {
			// Version
			snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Version</td><td class=right>%s</td></tr>\n", srv->version);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			// Nodeid
			snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>NodeID</td><td class=right>%02x%02x%02x%02x%02x%02x%02x%02x</td></tr>\n", srv->nodeid[0],srv->nodeid[1],srv->nodeid[2],srv->nodeid[3],srv->nodeid[4],srv->nodeid[5],srv->nodeid[6],srv->nodeid[7]);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
	}
	else {
		tcp_writestr(&tcpbuf, sock, "<tr><td class=left>Status</td><td class=right>Disconnected</td></tr>\n");
		if (srv->connection.lastseen) {
			uint32_t d = (GetTickCount()-srv->connection.lastseen)/1000;
			snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Last Seen</td><td class=right>%02dd %02d:%02d:%02d</td></tr>\n", d/(3600*24),(d/3600)%24,(d/60)%60,d%60);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
	}
	// UPTIME
	if ( srv->connection.uptime || (srv->connection.status>0) ) {
		uint32_t uptime;
		if (srv->connection.status>0) uptime = (GetTickCount()-srv->connection.time)+srv->connection.uptime; else uptime = srv->connection.uptime;
		uptime /= 1000;
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Uptime</td><td class=right>%02dd %02d:%02d:%02d</td></tr>",uptime/(3600*24),(uptime/3600)%24,(uptime/60)%60,uptime%60);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	// Priority
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Priority</td><td class=right>%d</td></tr>", srv->priority);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// EOT
	tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );
	if (srv->ecmnb) {
		// Ecm Stat
		tcp_writestr(&tcpbuf, sock, "<table class=\"infotable\"><tbody>\n" );
		tcp_writestr(&tcpbuf, sock, "<tr><th colspan=2>ECM Statistics</th></tr>\n" );
		sprintf( http_buf, "<tr><td class=left>Total ECM requests</td><td class=right>%d</td></tr>\n", srv->ecmnb);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf, "<tr><td class=left>Good ECM answer</td><td class=right>%d</td></tr>\n", srv->ecmok);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//Ecm Time
		if (srv->ecmok) {
			snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Average Time</td><td class=right>%d ms</td></tr>\n", srv->ecmok?((srv->ecmoktime/srv->ecmok)):0 );
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		// EOT
		tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );
	}

	tcp_writestr(&tcpbuf, sock, "</td><td style=\"vertical-align:top;\">");

	if (srv->cstat[0].csid) { //Print used profiles
		sprintf( http_buf, "<br>Used Profiles<br><table class=option><tr><th width=200px>Profile name</th><th width=90px>Total ECM</th><th width=90px>Ecm OK</th><th width=90px>Ecm Time</th></tr>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		int alt=0;
		int i;
		for ( i=0; i<MAX_CSPORTS; i++ ) {
			if (!srv->cstat[i].csid) break;
			struct cardserver_data *cs = getcsbyid(srv->cstat[i].csid);
			if (!cs) continue;
			if (alt==1) alt=2; else alt=1;
			//Profile name
			snprintf( http_buf, sizeof(http_buf),"<tr><td class=alt%d><a href=\"/profile?id=%d\">%s</a></td>",alt, cs->id, cs->name); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			//TotalECM
			sprintf( http_buf, "<td class=alt%d align=center>%d</td>",alt, srv->cstat[i].ecmnb ); //,cs->ecmdenied);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			//ECM OK
			tcp_writeecmdata(&tcpbuf, sock, srv->cstat[i].ecmok, srv->cstat[i].ecmnb );
			//ECM TIME
			int temp;
			if (srv->cstat[i].ecmok) temp =  srv->cstat[i].ecmoktime/srv->cstat[i].ecmok; else temp=0;
			if (temp)
				sprintf( http_buf, "<td class=alt%d align=center>%dms</td>",alt, temp);
			else
				sprintf( http_buf, "<td class=alt%d align=center>-- ms</td>",alt);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			//Close Row
			sprintf( http_buf,"</tr>");
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		sprintf( http_buf,"</table>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	// EOT
	tcp_writestr(&tcpbuf, sock, "</td></tr></table>");


		if (srv->handle>0) {
			// Print CardList
			if (srv->type==TYPE_CCCAM) {
				sprintf( http_buf, "<br>Total Cards = %d ( Hop1 = %d, Hop2 = %d )<br><table class=maintable width=100%%><tr><th width=120px>NodeID_CardID</th><th width=150px>EcmOK</th><th width=70px>EcmTime</th><th>Caid/Providers</th></tr>",srv_cardcount(srv,-1), srv_cardcount(srv,1), srv_cardcount(srv,2));
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				struct cs_card_data *card = srv->card;
				int alt=0;
				while(card) {
					if (alt==1) alt=2; else alt=1;
#ifdef CCCAM_CLI
					snprintf( http_buf, sizeof(http_buf),"<tr><td class=alt%d>%02x%02x%02x%02x%02x%02x%02x%02x_%x</td>",alt, card->nodeid[0], card->nodeid[1], card->nodeid[2], card->nodeid[3], card->nodeid[4], card->nodeid[5], card->nodeid[6], card->nodeid[7], card->shareid);
#else
					snprintf( http_buf, sizeof(http_buf),"<tr><td class=alt%d>%x</td>",alt, card->id);
#endif
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

#ifdef CCCAM_CLI
					sprintf( http_buf,"<td class=alt%d>%d / %d<span style=\"float:right\">",alt,card->ecmok,card->ecmnb);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

					if (card->ecmnb)					
						sprintf( http_buf,"%d%%</span></td>", card->ecmok*100/card->ecmnb);
					else
						sprintf( http_buf,"0%%</span></td>");
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

					if (card->ecmok)
						sprintf( http_buf,"<td class=alt%d align=center>%d ms</td>",alt, card->ecmoktime/card->ecmok );
					else
						sprintf( http_buf,"<td class=alt%d align=center>-- ms</td>",alt);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

					{
						char cardbuf[2048] = "";
						card_groups_html( card, cardbuf, sizeof(cardbuf) );
						sprintf( http_buf,"<td class=alt%d>[%d]%s",alt,card->uphops,cardbuf);
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
#else
					{
						char cardbuf[2048] = "";
						card_groups_html( card, cardbuf, sizeof(cardbuf) );
						sprintf( http_buf,"<td class=alt%d>%s",alt,cardbuf);
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
#endif
					sprintf( http_buf,"</td></tr>");
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

					card = card->next;
				}
				sprintf( http_buf,"</table>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			else {
				sprintf( http_buf, "<br>Cards:<br><table class=maintable width=100%%><tr><th>Caid/Providers</th></tr>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

				struct cs_card_data *card = srv->card;
				int alt=0;
				while(card) {
					if (alt==1) alt=2; else alt=1;
					{
						char cardbuf[2048] = "";
						card_groups_html( card, cardbuf, sizeof(cardbuf) );
						sprintf( http_buf,"<td class=alt%d>%s",alt,cardbuf);
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
					sprintf( http_buf,"</td></tr>");
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

					card = card->next;
				}
				sprintf( http_buf,"</table>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}

	tcp_flush(&tcpbuf, sock);
}


///////////////////////////////////////////////////////////////////////////////
// [CACHE]
///////////////////////////////////////////////////////////////////////////////

inline int isactivepeer(struct cachepeer_data *peer)
{
	if ( peer->ping>0 ) return 1;
	return 0;
}

void getcachecells(struct cachepeer_data *peer, char cell[12][2048] )
{
	char temp[2048];

	memset(cell, 0, 12*2048);
	// CELL0#Host/port
#ifdef NEWCACHE
	if ( (peer->protocol)&&(peer->ping>0) ) {
		if (peer->sms) {
			if (peer->sms->status==0) sprintf( cell[0],"<span style='float:right'><span class='smsdot new' title='New SMS'></span></span><a href='/cachepeer?id=%d'>%s:%d</a>", peer->id, peer->host->name,peer->port);
			else  sprintf( cell[0],"<span style='float:right'><span class='smsdot' title='SMS read'></span></span><a href='/cachepeer?id=%d'>%s:%d</a>", peer->id, peer->host->name,peer->port);
		}
		else sprintf( cell[0],"<a href='/cachepeer?id=%d'>%s:%d</a>", peer->id, peer->host->name,peer->port);
	}
	else
#endif
	sprintf( cell[0],"%s:%d", peer->host->name,peer->port);
	// CELL1#IP
	char *p = getcountrycodebyip(peer->host->ip);
	if (p) sprintf( cell[1],"<img src='/flag_%s.gif' title='%s'> %s", p, getcountryname(p), (char*)ip2string(peer->host->ip) ); else sprintf( cell[1],"%s",(char*)ip2string(peer->host->ip) );
	// CELL2#Program (assinatura)
	sprintf( cell[2],"Csp-Cache | Mcs1000");
	// CELL3 # Ping
	if (IS_DISABLED(peer->flags)) {
		sprintf( cell[3],"offline");
		sprintf( cell[4],"Dis.");
	}
	else {
		if ( peer->ping>0 ) {
			sprintf( cell[3],"online");
			sprintf( cell[4],"%d", peer->ping);
		}
		else {
			sprintf( cell[3],"offline");
			sprintf( cell[4],"?");
		}
		if (peer->csporthit[0].csid) {
			strcat( cell[4], "<table class=\"connect_data\">" );
#ifndef PUBLIC
			if (peer->ismultics) sprintf( temp,"<tr><td>Protocol</td><td>*%d</td></tr>", peer->protocol);
			else sprintf( temp,"<tr><td>Protocol</td><td>%d</td></tr>", peer->protocol);
			strcat( cell[4], temp );
#endif
			strcat( cell[4], "<tr><td width=150px>Profile</td><td>Hits</td></tr>" );
			int i;
			for(i=0; i<10; i++) {
				if (!peer->csporthit[i].csid) break;
				struct cardserver_data *cs = getcsbyid(peer->csporthit[i].csid);
				if (!cs) continue;
				sprintf( temp,"<tr><td>%s</td><td>%d</td></tr>", cs->name,peer->csporthit[i].hits);
				strcat( cell[4], temp );
			}
			strcat( cell[4], "</table>");
		}
	}



	// CELL4 # Request
	sprintf( cell[5],"%d",peer->reqnb);
	// CELL5 #
	sprintf( cell[6],"%d",peer->repok);

	sprintf( cell[7],"%d",peer->sentreq);
	sprintf( cell[8],"%d",peer->sentrep);

	// CELL8 # Cache Hits/Total
	getstatcell( peer->hitnb, cfg.cache.hits, cell[9] );
	// CELL9 # Instant Cache
	getstatcell( peer->ihitnb, peer->hitnb, cell[10] );
	// CELL10 # Last Used Cache
	if (peer->lastcaid) {
		sprintf( cell[11],"ch %s (%dms)", getchname(peer->lastcaid, peer->lastprov, peer->lastsid) , peer->lastdecodetime );
	}
	else strcpy( cell[11], " ");

	strcat( cell[11], "<br><span style='display:inline-flex;gap:2px;white-space:nowrap;margin-top:4px;'>");
	if ( !(peer->flags&(FLAG_DELETE|FLAG_EXPIRED)) ) {
		if (peer->flags&FLAG_DISABLE) {
			sprintf( temp," <span class='icobtn on' title='Enable' onclick=\"imgrequest('/cachepeer?id=%d&action=enable',this);setTimeout('updateDiv()',600)\">ON</span>",peer->id);
			strcat( cell[11], temp );
		}
		else {
			sprintf( temp," <span class='icobtn off' title='Disable' onclick=\"imgrequest('/cachepeer?id=%d&action=disable',this);setTimeout('updateDiv()',600)\">OFF</span>",peer->id);
			strcat( cell[11], temp );
		}
	}
	sprintf( temp," <span class='icobtn dbg' title='Debug' onclick=\"toggleDbgRow(%d,'/cachepeer?id=%d&action=dbginfo')\">DBG</span>",peer->id,peer->id);
	strcat( cell[11], temp );
	strcat( cell[11], "</span>");

}

struct cacheserver_data *getcacheserverbyid(uint32_t id)
{
	struct cacheserver_data *cache = cfg.cache.server;
	while (cache) {
		if (!(cache->flags&FLAG_DELETE))
			if (cache->id==id) return cache;
		cache = cache->next;
	}
	return NULL;
}


void http_send_cache(int sock, http_request *req)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;
	struct cachepeer_data *peer;
	char cell[12][2048];

	char *peerid = isset_get( req, "peerid");
	// Get Peer ID
	if (peerid)	{
		int i = atoi(peerid);
		//look for server
		struct cacheserver_data *cache = cfg.cache.server;
		while (cache) {
			peer = cache->peer;
			while (peer) {
				if (peer->id==(uint32_t)i) break;
				peer = peer->next;
			}
			cache = cache->next;
		}
		if (!peer) return;
		// Send XML CELLS
		getcachecells(peer,cell);
		for(i=0; i<12; i++) xmlescape( cell[i] );
		char buf[5000] = "";
		snprintf( buf, sizeof(buf), "<peer>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2>%s</c2>\n<c3_c>%s</c3_c>\n<c3>%s</c3>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6><c7>%s</c7><c8>%s</c8><c9>%s</c9><c10>%s</c10>\n</peer>\n",cell[0],cell[1],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8],cell[9],cell[10],cell[11] );
		http_send_xml( sock, req, buf, strlen(buf));
		return;
	}

	// Param Action
	char *str_action = isset_get( req, "action");
	int get_action = 0;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = 1;
		else if (!strcmp(str_action,"row")) get_action = 2;
		else if (!strcmp(str_action,"disable")) {
			str_action = NULL;
			char *str_z = isset_get( req, "z"); // server ID
			if (str_z) {
				int get_z = atoi(str_z);
				struct cacheserver_data *cache = cfg.cache.server;
				while (cache) {
					peer = cache->peer;
					while (peer) {
						if ( (peer->ping>0)&&(peer->hitnb<get_z) ) {
							peer->flags |= FLAG_DISABLE;
							peer->nbcards = 0;
							peer->ping = 0;
						}
						peer = peer->next;
					}
					cache = cache->next;
				}
			}
		}
		else if (!strcmp(str_action,"config")) {
			tcp_init(&tcpbuf);
			struct cacheserver_data *cache = cfg.cache.server;
			while (cache) {
				sprintf( http_buf, "\n\nCACHE PORT: %d\n", cache->port);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				peer = cache->peer;
				while (peer) {
					if (peer->ping>0) {
						sprintf( http_buf, "CACHE PEER: %s %d\n", peer->host->name, peer->port);
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
					peer = peer->next;
				}
				cache = cache->next;
			}
			tcp_flush(&tcpbuf, sock);
			return;
		}
		else str_action = NULL;
	}
	if (!str_action) str_action = "page";
	//
	char *str_id = isset_get( req, "id"); // server ID
	int get_id = 0;
	if (str_id)	get_id = atoi(str_id);
	// Param List
	char *str_list = isset_get( req, "list");
	int get_list = LIST_ALL; // default: mostrar todos os peers
	if (str_list) {
		if (!strcmp(str_list,"active")) get_list = LIST_CONNECTED;
		else if (!strcmp(str_list,"inactive")) get_list = LIST_DISCONNECTED;
		else if (!strcmp(str_list,"all")) get_list = LIST_ALL;
		else str_list=NULL;
	}
	if (!str_list) str_list = "all";
	//
	//
	struct cacheserver_data *cache = NULL;
	if (get_id) {
		cache = getcacheserverbyid(get_id);
		if (!cache) return;
	}

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	if (get_action==0) {
		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, html_title, cfg.http.title, "Cache"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		// JS
		tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
        tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
		// ACTIONS REQUEST
		tcp_writestr(&tcpbuf, sock, "\nfunction imgrequest( url, el )\n{\n	var httpRequest;\n	try { httpRequest = new XMLHttpRequest(); }\n	catch (trymicrosoft) { try { httpRequest = new ActiveXObject('Msxml2.XMLHTTP'); } catch (oldermicrosoft) { try { httpRequest = new ActiveXObject('Microsoft.XMLHTTP'); } catch(failed) { httpRequest = false; } } }\n	if (!httpRequest) { alert('Your browser does not support Ajax.'); return false; }\n	if ( typeof(el)!='undefined' ) {\n		el.onclick = null;\n		el.style.opacity = '0.7';\n		httpRequest.onreadystatechange = function()\n		{\n			if (httpRequest.readyState == 4) if (httpRequest.status == 200) el.style.opacity = '0.3';\n		}\n	}\n	httpRequest.open('GET', url, true);\n	httpRequest.send(null);\n}\n");
		// UPD ROW
		tcp_writestr(&tcpbuf, sock, "\nfunction xmlupdateRow( xmlDoc, id )\n{\n	var row = document.getElementById(id);\n	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n	row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n	row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n	row.cells.item(3).className = xmlDoc.getElementsByTagName('c3_c')[0].childNodes[0].nodeValue;\n	row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n	row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n	row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n	row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n	row.cells.item(7).innerHTML = xmlDoc.getElementsByTagName('c7')[0].childNodes[0].nodeValue;\n	row.cells.item(8).innerHTML = xmlDoc.getElementsByTagName('c8')[0].childNodes[0].nodeValue;\n	row.cells.item(9).innerHTML = xmlDoc.getElementsByTagName('c9')[0].childNodes[0].nodeValue;\n	row.cells.item(10).innerHTML = xmlDoc.getElementsByTagName('c10')[0].childNodes[0].nodeValue;\n}\n");
		char url[256];
		sprintf( url, "'/cache?peerid='+idx");
		sprintf( http_buf, HTTP_UPDATE_ROW, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// UPD DIV
		sprintf( url, "/cache?action=div&list=%s", str_list);
		sprintf( http_buf, HTTP_UPDATE_DIV, cfg.http.autorefresh*1000, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	setautorefresh(autorefresh);\n}");
		tcp_writestr(&tcpbuf, sock, "\n</script>\n");
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
		tcp_write_menu(&tcpbuf, sock,PAGE_CACHE);
		// Info de servidores (acima da div principal)
		{
			tcp_writestr(&tcpbuf, sock, "<div style='margin:12px 12px 0 12px'><div class=stat-section style='margin:0'>");
			sprintf( http_buf, "<h3 class=stitle>Cache Servers (%d)</h3>", cfg.cache.totalservers);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			tcp_writestr(&tcpbuf, sock, "<table class=maintable><tr><th>Server</th><th>Port</th><th>Status</th><th>Active Peers</th></tr>");
			int itotal, iactive;
			total_cache_peers( &itotal, &iactive );
			sprintf( http_buf, "<tr><td class=left>TOTAL</td><td class=right>-</td><td class=right>-</td><td class=right>%d / %d</td></tr>", iactive, itotal);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf( http_buf, "<tr><td class=left>AliveTime</td><td class=right colspan=2>%ds</td><td class=right>Auto-Add: %s | Filter: %s</td></tr>", cfg.cache.alivetime/1000, yesno(cfg.cache.autoadd), onoff(cfg.cache.filter));
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf( http_buf, "<tr><td class=left>Requests / Replies</td><td class=right colspan=3>%d / %d</td></tr>", cfg.cache.req, cfg.cache.rep);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			struct cacheserver_data *box = cfg.cache.server;
			while ( box ) {
				int btotal, bactive;
				cache_peers( box, &btotal, &bactive );
				if (box->handle>0) sprintf( http_buf, "<tr><td class=left><a href='/cache?id=%d'>cache %d</a></td><td class=right>%d</td><td class=right><span class=success>ONLINE</span></td><td class=right>%d / %d</td></tr>", box->id, box->id, box->port, bactive, btotal);
				else sprintf( http_buf, "<tr><td class=left><a href='/cache?id=%d'>cache %d</a></td><td class=right>%d</td><td class=right><span class=failed>OFFLINE</span></td><td class=right>%d / %d</td></tr>", box->id, box->id, box->port, bactive, btotal);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				box = box->next;
			}
			tcp_writestr(&tcpbuf, sock, "</table></div></div>");
		}
		// DIV
		tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");
	}

	// Buttons
	int total, active;
	tcp_writestr(&tcpbuf, sock, "<select style=\"width:200px;\" onchange=\"parent.location.href='/cache?id='+this.value\">");
	sprintf( http_buf, "<option value=0>ALL (%d)</option>", cfg.cache.totalservers);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	struct cacheserver_data *tmp = cfg.cache.server;
	while (tmp) {
		if (get_id==tmp->id) sprintf( http_buf, "<option value=%d selected>[%d] cache %d</option>",tmp->id,tmp->port, tmp->id );
		else sprintf( http_buf, "<option value=%d>[%d] cache %d</option>",tmp->id,tmp->port, tmp->id );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tmp = tmp->next;
	}
	tcp_writestr(&tcpbuf, sock, "</select> ");
	//
	if (cache) cache_peers( cache, &total, &active ); else total_cache_peers( &total, &active );
	char *class1 = "button"; char *class2 = "sbutton";
	char *class;
	if (get_list==LIST_CONNECTED) class = class2; else class = class1;
	sprintf( http_buf, "<input type=button class=%s onclick=\"parent.location='/cache?id=%d&amp;list=active'\" value='Active Peers (%d)'>", class, get_id, active);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (get_list==LIST_DISCONNECTED) class = class2; else class = class1;
	sprintf( http_buf, "<input type=button class=%s onclick=\"parent.location='/cache?id=%d&amp;list=inactive'\" value='Inactive Peers (%d)'>", class, get_id, total-active);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (get_list==LIST_ALL) class = class2; else class = class1;
	sprintf( http_buf, " <input type=button class=%s onclick=\"parent.location='/cache?id=%d&amp;list=all'\" value='All Peers (%d)'>", class, get_id, total);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	// Peers MainTable
	tcp_writestr(&tcpbuf,sock, "\n<table class=maintable width=100%>");
	tcp_writestr(&tcpbuf,sock, "\n<tr><th width=200px>Host</th><th width=110px>IP Address</th><th width=80px>Program</th><th width=30px>Ping</th><th width=70px>Requests</th><th width=70px>Replies</th><th width=70px>Sent REQ</th><th width=70px>Sent REP</th><th width=90px>Cache Hits</th><th width=80px>Instant Hits</th><th>Last Cache Hit</th></tr>\n");
	int alt=0;
	if (cache) {
		int total, active;
		cache_peers( cache, &total, &active );
		snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=12> cache %d (%d)</td></tr>", cache->id, active); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		peer = cache->peer;
		while (peer) {
			int isactive = isactivepeer(peer);
			if ( (isactive&&(get_list&LIST_CONNECTED)) || (!isactive&&(get_list&LIST_DISCONNECTED)) ) {
				if (alt==1) alt=2; else alt=1;
				getcachecells(peer, cell);
				if (peer->runtime) alt=3;
				snprintf( http_buf, sizeof(http_buf),"<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'><td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",peer->id,alt,peer->id,cell[0],cell[1],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8],cell[9],cell[10],cell[11]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			peer = peer->next;
		}
		// Total
		cache_peers( cache, &total, &active );
		snprintf( http_buf, sizeof(http_buf),"<tr class=alt3><td align=right>Total</td><td colspan=3>%d</td>",total);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		int totreq = 0;
		int totrepok = 0;
		int tothits = 0;
		int totihits = 0;

		peer = cache->peer;
		while (peer) {
			totreq += peer->reqnb;
			totrepok += peer->repok;
			tothits += peer->hitnb;
			totihits += peer->ihitnb;
			peer = peer->next;
		}
		sprintf( http_buf,"<td>%d</td>",totreq); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf,"<td>%d</td>",totrepok); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf,"<td colspan=2> </td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_writeecmdata(&tcpbuf, sock, tothits, cfg.cache.hits);
		tcp_writeecmdata(&tcpbuf, sock, totihits, tothits);
		sprintf( http_buf,"<td> </td></tr>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	else {
		cache = cfg.cache.server;
		while (cache) {
			int total, active;
			cache_peers( cache, &total, &active );
			snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=12> cache %d (%d)</td></tr>", cache->id, active); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			peer = cache->peer;
			while (peer) {
				int isactive = isactivepeer(peer);
				if ( (isactive&&(get_list&LIST_CONNECTED)) || (!isactive&&(get_list&LIST_DISCONNECTED)) ) {
					if (alt==1) alt=2; else alt=1;
					getcachecells(peer, cell);
					if (peer->runtime) alt=3;
					snprintf( http_buf, sizeof(http_buf),"<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'><td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",peer->id,alt,peer->id,cell[0],cell[1],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8],cell[9],cell[10],cell[11]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				peer = peer->next;
			}
			cache = cache->next;
		}

		// Total
		total_cache_peers( &total, &active );
		snprintf( http_buf, sizeof(http_buf),"<tr class=alt3><td align=right>Total</td><td colspan=3>%d</td>",total);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		int totreq = 0;
		int totrepok = 0;
		int tothits = 0;
		int totihits = 0;

		cache = cfg.cache.server;
		while (cache) {
			peer = cache->peer;
			while (peer) {
				totreq += peer->reqnb;
				totrepok += peer->repok;
				tothits += peer->hitnb;
				totihits += peer->ihitnb;
				peer = peer->next;
			}
			cache = cache->next;
		}

		sprintf( http_buf,"<td>%d</td>",totreq); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf,"<td>%d</td>",totrepok); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf,"<td colspan=2> </td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_writeecmdata(&tcpbuf, sock, tothits, cfg.cache.hits);
		tcp_writeecmdata(&tcpbuf, sock, totihits, tothits);
		sprintf( http_buf,"<td> </td></tr>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}


	tcp_writestr(&tcpbuf, sock, "</table>");

	if (get_action==0) {
		tcp_writestr(&tcpbuf, sock, "</div></body></html>");
	}
	tcp_flush(&tcpbuf, sock);
}

struct sms_data *cache_new_sms(char *msg);
void cache_send_sms(struct cachepeer_data *peer, struct sms_data *sms);

///////////////////////////////////////////////////////////////////////////////
void http_send_cache_peer(int sock, http_request *req)
{
	char *str_id = isset_get( req, "id");
	if (!str_id) return; //error
	int get_id = atoi(str_id);
	//
	struct cachepeer_data *peer = getpeerbyid( get_id );
	if (!peer) {
		http_send_redirect(sock, "/cache");
		return;
	}
	// Action
	char *str_action = isset_get( req, "action");
	int get_action = 0;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = 1;
		else if (!strcmp(str_action,"row")) get_action = 2;
		else if (!strcmp(str_action,"disable")) get_action = 3;
		else if (!strcmp(str_action,"enable")) get_action = 4;
		else if (!strcmp(str_action,"status")) get_action = 5;
		else if (!strcmp(str_action,"dbginfo")) get_action = 8;
		else if (!strcmp(str_action,"sms")) get_action = 10;
		else str_action = NULL;
	}
	if (!str_action) str_action = "page";
	//
	if (get_action==8) {
		char dbg[1024];
		sprintf( dbg, "<div class='dbginfo'><b>%s:%d</b> (id %d) | Ping: %dms | Status: %s | Replies: %d | Hits: %d (%d instant)<br>Flags: 0x%08x | Last ch: %04x:%06x:%04x (%dms) | Program: %s %s</div>",
			peer->host->name, peer->port, peer->id, peer->ping,
			IS_DISABLED(peer->flags)?"DISABLED":"ENABLED",
			peer->repok, peer->hitnb, peer->ihitnb,
			peer->flags, peer->lastcaid, peer->lastprov, peer->lastsid, peer->lastdecodetime,
			peer->program, peer->version);
		http_send_text(sock, dbg);
		return;
	}
	if (get_action==3) {
		peer->flags |= FLAG_DISABLE;
		peer->ping = 0;
 		http_send_ok(sock);
		return;
	}
	else if (get_action==4) {
		peer->flags &= ~FLAG_DISABLE;
		peer->ping = 0;
		peer->lastpingsent = 0;
		peer->program[0] = 0;
		peer->version[0] = 0;
		http_send_ok(sock);
		return;
	}
	else if (get_action==5) {
		if ( peer->ping>0 ) http_send_text(sock,"active"); else http_send_text(sock,"inactive");
		return;
	}
	else if (get_action==10) {
		// Terminate the string
		req->dbf.data[req->dbf.datasize] = 0;
		char *msg = strstr( (char*)req->dbf.data, "\r\n\r\n" );
		if (msg) {
			// Check Length
			int len = strlen(msg);
			if (len<2) return;
			if (len>1000) msg[1000] = 0;
			// Create MSG
			struct sms_data *sms = cache_new_sms(msg+4);
			cache_send_sms( peer, sms);
			// Wait ACK
			sleep(1);
			if (sms->status&2) {
				http_send_ok(sock);
				return;
			}
		}
		return;
	}


	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) ); // header tambem no div (XHR exige status line)
	if (!get_action) {
		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, html_title, cfg.http.title, " Cache Peer"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );

		// JS
		tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
        tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
		// ACTIONS REQUEST
		tcp_writestr(&tcpbuf, sock, "\nfunction smsrequest( peerid , button )\n{\n	msg = document.getElementById('message').value;\n	if (msg=='') return;\n	var httpRequest;\n	try { httpRequest = new XMLHttpRequest(); }\n	catch (trymicrosoft) { try { httpRequest = new ActiveXObject('Msxml2.XMLHTTP'); } catch (oldermicrosoft) { try { httpRequest = new ActiveXObject('Microsoft.XMLHTTP'); } catch(failed) { httpRequest = false; } } }\n	if (!httpRequest) { alert('Your browser does not support Ajax.'); return false; }\n	button.disabled = true;\n	mydiv = document.getElementById('smsdiv');\n	mydiv.innerHTML = 'Sending message to peer...';\n	clearTimeout(tautorefresh);\n	httpRequest.onreadystatechange = function()\n	{\n		if (httpRequest.readyState == 4) {\n			if (httpRequest.status == 200) {\n				mydiv.innerHTML = 'Message Sent Successfully';\n				document.getElementById('message').value = '';\n			}\n			else mydiv.innerHTML = 'Failed to send message';\n			button.disabled = false;\n			if (!autorefresh) autorefresh = 3000;\n			updateDiv();\n		}\n	}\n	httpRequest.open('POST', '/cachepeer?action=sms&id='+peerid, true);\n	httpRequest.send( msg );\n}\n");
		// UPD DIV
		char url[255];
		sprintf( url, "/cachepeer?id=%d&action=div", peer->id);
		sprintf( http_buf, HTTP_UPDATE_DIV, cfg.http.autorefresh*1000, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		tcp_writestr(&tcpbuf, sock, "\n</script>\n");
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_writestr(&tcpbuf, sock, "<body>");
		tcp_write_menu(&tcpbuf, sock,0);
		//
		tcp_writestr(&tcpbuf, sock, "<table style='padding:0px; margin:0px;' width='100%'><tbody>");
		tcp_writestr(&tcpbuf, sock, "<tr><td style='vertical-align:top; width:400px;'>");
		// Peer Infos
		tcp_writestr(&tcpbuf, sock, "<table class='infotable'><tbody><tr><th colspan=2>Cache Peer Informations</th></tr>");
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Host:Port</td><td class=right>%s:%d</td></tr>", peer->host->name, peer->port); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		if (peer->cards[0]) {
			tcp_writestr(&tcpbuf, sock, "<tr><td class=left>Card list</td><td class=right><select>");
			int i;
			for (i=0; i<1024; i++) {
				if (!peer->cards[i]) break;
				if ( (peer->cards[i]>>24)==5 )
					sprintf( http_buf,"<option>0500:%06x</option>", peer->cards[i]&0xffffff);
				else
					sprintf( http_buf,"<option>%04x:%06x</option>", peer->cards[i]>>16, peer->cards[i]&0xffff);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			tcp_writestr(&tcpbuf, sock, "</select></td></tr>");
		}
		tcp_writestr(&tcpbuf, sock, "</tbody></table>");

		// Stat
		tcp_writestr(&tcpbuf, sock, "<br><table class='infotable'><tbody><tr><th colspan=2>Peer Statistics</th></tr>");
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Total Sent Requests</td><td class=right>%d</td></tr>", peer->sentreq);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Total Sent Replies</td><td class=right>%d</td></tr>", peer->sentrep);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Total Received Requests</td><td class=right>%d</td></tr>", peer->reqnb);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Total Received Replies</td><td class=right>%d</td></tr>", peer->repok);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Total Cache Hits</td><td class=right>%d</td></tr>", peer->hitnb);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_writestr(&tcpbuf, sock, "</tbody></table>");

		// Profiles Hits
		tcp_writestr(&tcpbuf, sock, "<br><table class='infotable'><tbody><tr><th colspan=2>Profiles Hits</th></tr>");
		int i;
		for(i=0; i<MAX_CSPORTS; i++) {
			if (!peer->csporthit[i].csid) break;
			struct cardserver_data *cs = getcsbyid(peer->csporthit[i].csid);
			if (!cs) continue;
			snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>%s</td><td class=right>%d</td></tr>", cs->name,peer->csporthit[i].hits);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		tcp_writestr(&tcpbuf, sock, "</tbody></table>");
		tcp_writestr(&tcpbuf, sock, "</td><td style='vertical-align:top;'>");
		// Messages
		tcp_writestr(&tcpbuf, sock, "<table class='infotable' width=100%><tr><th colspan=2>Send Message</td></tr><tr>");
		tcp_writestr(&tcpbuf, sock, "<td><textarea id='message' name='message' style='width:100%; height:50px;'></textarea></td>");
		sprintf( http_buf,"<td width=150px align=center><input type=button style='width=120px' value='Send Message' onclick='smsrequest(%d, this)'><br><div id=smsdiv></div></td>", peer->id);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_writestr(&tcpbuf, sock, "</tr><tr><td colspan=2>");
		tcp_writestr(&tcpbuf, sock, "<div id=mainDiv>");
	}
	if (peer->sms) {
		struct sms_data *sms = peer->sms;
		while (sms) {
			// Get Time
			char timebuf [80];
			struct tm * timeinfo = localtime (&sms->rawtime);
			strftime (timebuf,80,"%x %X",timeinfo);
			//
			char *color;
			if (sms->status&1) {
				if (sms->status&2) color = "blue"; else color = "grey";
			}
			else {
				if (sms->status&2) color = "green"; else color = "red";
				sms->status = 2;
			}
			//
			sprintf( http_buf,"<font color=%s><pre>-----[%s]-------\n%s</pre></font>", color, timebuf, sms->msg);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sms = sms->next;
		}
	}

	if (!get_action) {
		tcp_writestr(&tcpbuf, sock, "</div></td></tr></table> </td></tr></table></body></html>");
	}
	tcp_flush(&tcpbuf, sock);
}


///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
void getprofilecells(struct cardserver_data *cs, char cell[11][4096])
{
	char temp[2048];
	// CELL0 # Profile name
	sprintf( cell[0],"<a href=\"/profile?id=%d\">%s</a>", cs->id, cs->name);
	// CELL1 # Port
	sprintf( cell[1],"<a href=\"/newcamd?pid=%d\">%d</a>", cs->id, cs->newcamd.port); 
	if (cs->newcamd.handle>0) sprintf( cell[10],"online"); else sprintf( cell[10],"offline"); 
	// CELL2 # Ecm Time
	if (cs->ecmok) sprintf( cell[2],"%d ms",(cs->ecmoktime/cs->ecmok) ); else sprintf( cell[2],"-- ms");

	// CELL3 # TotalECM
	int ecmnb = cs->ecmaccepted+cs->ecmdenied;
	sprintf( cell[3], "%d", ecmnb );
	// CELL4 # AcceptedECM
	getstatcell( cs->ecmaccepted, ecmnb, cell[4] );
	// CELL5 # ECM OK
	getstatcell( cs->ecmok, cs->ecmaccepted, cell[5] );
	// CELL6 # CacheHits
	getstatcell( cs->hits.csp, cs->ecmok, cell[6] );
#ifdef CACHEEX
	if (cs->option.fallowcacheex) {
		getstatcell( cs->hits.cacheex, cs->ecmok, temp );
		strcat( cell[6], "<br>" );
		strcat( cell[6], temp );
	}
#endif 
	// CELL7 # Cache iHits
	getstatcell( cs->hits.instant.csp, cs->hits.csp, cell[7] );
#ifdef CACHEEX
	if (cs->option.fallowcacheex) {
		getstatcell( cs->hits.instant.cacheex, cs->hits.cacheex, temp );
		strcat( cell[7], "<br>" );
		strcat( cell[7], temp );
	}
#endif 
	// CELL8 # Clients
	int i=0;
	int j=0;
	struct cs_client_data *usr = cs->newcamd.client;
	while (usr) {
		i++;
		if (usr->handle>0) j++;
		usr = usr->next;
	}
	getstatcell2(j,i,cell[8]);
	// CELL9 # Caid:Providers (com nomes de ident do CCcam.providers)
	char *provname = providerID(cs->card.caid, cs->card.prov[0].id);
	if (provname) sprintf( cell[9],"<b>%04X:</b> %06x <font color=#CC3300>%s</font>",cs->card.caid,cs->card.prov[0].id,provname);
	else sprintf( cell[9],"<b>%04X:</b> %06x",cs->card.caid,cs->card.prov[0].id);
	for(i=1; i<cs->card.nbprov; i++) {
		provname = providerID(cs->card.caid, cs->card.prov[i].id);
		if (provname) sprintf( temp,", %06x <font color=#CC3300>%s</font>",cs->card.prov[i].id,provname);
		else sprintf( temp,", %06x",cs->card.prov[i].id);
		if ( (strlen(cell[9])+strlen(temp)) < (sizeof(cell[9])-16) ) strcat( cell[9], temp );
	}

	// botoes em linha propria (guard de tamanho)
	#define C9ADD(s) if ( (strlen(cell[9])+strlen(s)) < (sizeof(cell[9])-16) ) strcat( cell[9], s )
	C9ADD("<br><span style='display:inline-flex;gap:2px;white-space:nowrap;margin-top:4px;'>");
	if (cs->flags&FLAG_DISABLE) {
		sprintf( temp," <span class='icobtn on' title='Ativar' onclick=\"imgrequest('/profile?id=%d&action=enable',this);setTimeout('updateDiv()',3000);setTimeout('updateDiv()',6000);setTimeout('updateDiv()',9000)\">ON</span>",cs->id);
		C9ADD(temp);
	}
	else {
		sprintf( temp," <span class='icobtn off' title='Desativar (comenta o perfil no profiles.cfg)' onclick=\"imgrequest('/profile?id=%d&action=off',this);setTimeout('updateDiv()',3000);setTimeout('updateDiv()',6000);setTimeout('updateDiv()',9000)\">OFF</span>",cs->id);
		C9ADD(temp);
	}
	sprintf( temp," <span class='icobtn dbg' title='Debug' onclick=\"toggleDbgRow(%d,'/profile?id=%d&action=dbginfo')\">DBG</span>",cs->id,cs->id);
	C9ADD(temp);
	C9ADD("</span>");
	#undef C9ADD
}


#define PFILE_MAX (1024*1024)
static char pfilebuf[PFILE_MAX+1];
static pthread_mutex_t pfile_mutex = PTHREAD_MUTEX_INITIALIZER;

// comenta (on=0) ou descomenta (on=1) a seccao [name] no profiles.cfg.
// Escreve em-place: o reload e feito pela thread de config (inotify).
int profile_config_toggle(char *name, int on)
{
	char *fname = NULL;
	struct filename_data *fs = cfg.files;
	while (fs) {
		if (strstr(fs->name,"profiles.cfg")) { fname = fs->name; break; }
		fs = fs->next;
	}
	if (!fname) return -1;
	pthread_mutex_lock(&pfile_mutex);
	FILE *fp = fopen(fname, "r");
	if (!fp) { pthread_mutex_unlock(&pfile_mutex); return -1; }
	int size = (int)fread(pfilebuf, 1, PFILE_MAX, fp);
	fclose(fp);
	if ((size<=0)||(size>=PFILE_MAX)) { pthread_mutex_unlock(&pfile_mutex); return -1; }
	pfilebuf[size] = 0;

	FILE *fo = fopen(fname, "w");
	if (!fo) { pthread_mutex_unlock(&pfile_mutex); return -1; }

	char *p = pfilebuf;
	int intarget = 0;
	while (*p) {
		char *start = p;
		while (*p && *p!='\n') p++;
		int hasnl = (*p=='\n');
		char *eol = p;
		if (*p) p++;
		*eol = 0;
		char *q = start;
		while (*q==' '||*q=='\t') q++;
		char *after = q;
		while (*after=='#') after++;
		while (*after==' '||*after=='\t') after++;
		if (*after=='[') {
			char secname[128] = "";
			char *a = after+1;
			int n = 0;
			while (*a && *a!=']' && n<120) secname[n++] = *a++;
			secname[n] = 0;
			if (!strcmp(secname, name)) intarget = on ? 2 : 1;
			else intarget = 0;
		}
		if (intarget==1) {
			if (*start!='#') fwrite("#",1,1,fo);
			fwrite(start,1,strlen(start),fo);
		}
		else if (intarget==2) {
			char *w = start;
			if (*w=='#') w++;
			while (*w==' '||*w=='\t') w++;
			fwrite(w,1,strlen(w),fo);
		}
		else fwrite(start,1,strlen(start),fo);
		if (hasnl) fwrite("\n",1,1,fo);
	}
	fclose(fo);
	pthread_mutex_unlock(&pfile_mutex);
	mlogf(LOGINFO, DBG_HTTP, " http: profile '%s' %s no %s (reload automatico)\n", name, on?"ativado":"desativado", fname);
	return 0;
}


// ja renderizado na tabela de profiles?
static int profile_is_rendered(int *rendered, int nrendered, int id)
{
	int k;
	for (k=0; k<nrendered; k++) if (rendered[k]==id) return 1;
	return 0;
}

// decode %XX e '+' num string de query
void urldecode(char *dst, const char *src, int max)
{
	int i = 0;
	while (*src && i<max-1) {
		if (*src=='%' && src[1] && src[2]) {
			char h[3] = { src[1], src[2], 0 };
			dst[i++] = (char)strtol(h, NULL, 16);
			src += 3;
		}
		else if (*src=='+') { dst[i++] = ' '; src++; }
		else dst[i++] = *src++;
	}
	dst[i] = 0;
}


void http_send_profiles(int sock, http_request *req)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;

	char cell[11][4096];

	//  Get Params
	char *str_action = isset_get( req, "action");
	int get_action = 0;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = ACTION_DIV;
		else if (!strcmp(str_action,"row")) get_action = ACTION_ROW;
		else if (!strcmp(str_action,"onprof")) get_action = ACTION_ENABLE; // descomentar perfil por nome
#ifndef PUBLIC
		else if (!strcmp(str_action,"xml")) get_action = ACTION_XML; // Get Clients info in xml
#endif
		else str_action = NULL;
	}
	if (get_action==ACTION_ENABLE) {
		char *pname = isset_get( req, "name");
		if (pname && pname[0]) {
			char namebuf[128];
			urldecode(namebuf, pname, sizeof(namebuf));
			profile_config_toggle(namebuf, 1);
		}
		http_send_ok(sock);
		return;
	}
	if (!str_action) { str_action = "page"; get_action = ACTION_PAGE; }
	//
	if (get_action==ACTION_XML) {
		char *str_id = isset_get( req, "id"); // CCcam server ID
		int get_id = 0;
		if (str_id) get_id = atoi( str_id );
		struct cardserver_data *cs = getcsbyid( get_id );

		tcp_init(&tcpbuf);
		tcp_writestr(&tcpbuf, sock, "HTTP/1.1 200 OK\r\nAccept-Ranges: bytes\r\nConnection: close\r\nContent-Type: application/xml\r\n\r\n");
		tcp_writestr(&tcpbuf, sock, "<multics>");
		struct cardserver_data *srv;
		if (cs) srv = cs; else srv = cfg.cardserver;
		while (srv) {
			tcp_writestr(&tcpbuf, sock, "\n<profile>");
			sprintf(http_buf, "<id>%d</id>", srv->id); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf(http_buf, "<name>%s</name>", srv->name); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf(http_buf, "<caid>%04x</caid>", srv->card.caid); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			int i;
			for (i=0; i<srv->card.nbprov; i++) {
				sprintf(http_buf, "<provider>%06x</provider>", srv->card.prov[i].id);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			sprintf(http_buf, "<totalecm>%d</totalecm>", srv->ecmaccepted+srv->ecmdenied); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf(http_buf, "<acceptedecm>%d</acceptedecm>", srv->ecmaccepted); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf(http_buf, "<ecmok>%d</ecmok>", srv->ecmok); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			tcp_writestr(&tcpbuf, sock, "\n</profile>");
			if (cs) break; else srv = srv->next;
		}
		tcp_writestr(&tcpbuf, sock, "\n</multics>");
		tcp_flush(&tcpbuf, sock);
		return;
	}
	//
	int i;
	char *id = isset_get( req, "id");
	// Get Peer ID
	if (id)	{
		i = atoi(id);
		//look for server
		struct cardserver_data *cs = cfg.cardserver;
		while (cs) {
			if (cs->id==(uint32_t)i) break;
			cs = cs->next;
		}
		if (!cs) return;
		// Send XML CELLS
		getprofilecells(cs,cell);
		// FIX v1.26: cap na coluna Caid:Providers (pode ser grande) antes de escapar
		if (strlen(cell[9])>3000) cell[9][3000] = 0;
		for(i=0; i<11; i++) xmlescape( cell[i] );
		char buf[5000] = "";
		snprintf( buf, sizeof(buf), "<profile>\n<c0>%s</c0>\n<c1_c>%s</c1_c>\n<c1>%s</c1>\n<c2>%s</c2>\n<c3>%s</c3>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6>\n<c7>%s</c7>\n<c8>%s</c8>\n<c9>%s</c9>\n</profile>\n",cell[0],cell[10],cell[1],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8],cell[9] );
		http_send_xml( sock, req, buf, strlen(buf));
		return;
	}


	tcp_init(&tcpbuf);

	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) ); // header tambem no div (XHR exige status line)
	if (get_action==ACTION_PAGE) {
		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, html_title, cfg.http.title, "Profiles"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		// JS
		tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
        tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
		// ACTIONS REQUEST
		tcp_writestr(&tcpbuf, sock, "\nfunction imgrequest( url, el )\n{\n	var httpRequest;\n	try { httpRequest = new XMLHttpRequest(); }\n	catch (trymicrosoft) { try { httpRequest = new ActiveXObject('Msxml2.XMLHTTP'); } catch (oldermicrosoft) { try { httpRequest = new ActiveXObject('Microsoft.XMLHTTP'); } catch(failed) { httpRequest = false; } } }\n	if (!httpRequest) { alert('Your browser does not support Ajax.'); return false; }\n	if ( typeof(el)!='undefined' ) {\n		el.onclick = null;\n		el.style.opacity = '0.7';\n		httpRequest.onreadystatechange = function()\n		{\n			if (httpRequest.readyState == 4) if (httpRequest.status == 200) el.style.opacity = '0.3';\n		}\n	}\n	httpRequest.open('GET', url, true);\n	httpRequest.send(null);\n}\n");
		// UPD ROW
		tcp_writestr(&tcpbuf, sock, "\nfunction xmlupdateRow( xmlDoc, id )\n{\n	var row = document.getElementById(id);\n	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n	row.cells.item(1).className = xmlDoc.getElementsByTagName('c1_c')[0].childNodes[0].nodeValue;\n	row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n	row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n	row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n	row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n	row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n	row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n	row.cells.item(7).innerHTML = xmlDoc.getElementsByTagName('c7')[0].childNodes[0].nodeValue;\n	row.cells.item(8).innerHTML = xmlDoc.getElementsByTagName('c8')[0].childNodes[0].nodeValue;\n	row.cells.item(9).innerHTML = xmlDoc.getElementsByTagName('c9')[0].childNodes[0].nodeValue;\n}\n");
		char url[256];
		sprintf( url, "'/profiles?id='+idx");
		sprintf( http_buf, HTTP_UPDATE_ROW, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// UPD DIV
		sprintf( url, "/profiles?action=div");
		sprintf( http_buf, HTTP_UPDATE_DIV, cfg.http.autorefresh*1000, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	setautorefresh(autorefresh);\n}");
		tcp_writestr(&tcpbuf, sock, "\n</script>\n");
		//
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_writestr(&tcpbuf, sock, "\n<body onload=\"start();\">");
		tcp_write_menu(&tcpbuf, sock,PAGE_PROFILES);
		// DIV
		tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");
	}

	sprintf( http_buf, "Total Profiles: %d", total_profiles() ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_writestr(&tcpbuf, sock, "<br>\n<table class=maintable width=100%%><tr><th width=150px>Profile name</th><th width=50px>Port</th><th width=60px>EcmTime</th><th width=60px>TotalECM</th><th width=90px>AcceptedECM</th><th width=80px>Ecm OK</th><th width=85px>Cache/Ex Hits</th><th width=80px>Instant Hits</th><th width=80px>Clients</th><th>Caid:Providers</th></tr>");

	int alt=0;
	int rendered[128];
	int nrendered = 0;

	// 1) perfis por ordem do profiles.cfg (ativos normais + comentados com ON)
	{
		char *fname = NULL;
		struct filename_data *fsx = cfg.files;
		while (fsx) {
			if (strstr(fsx->name,"profiles.cfg")) { fname = fsx->name; break; }
			fsx = fsx->next;
		}
		if (fname) {
			pthread_mutex_lock(&pfile_mutex);
			FILE *fp = fopen(fname, "r");
			if (fp) {
				int sz = (int)fread(pfilebuf, 1, PFILE_MAX, fp);
				fclose(fp);
				if ((sz>0)&&(sz<PFILE_MAX)) {
					pfilebuf[sz] = 0;
					char *p = pfilebuf;
					while (*p) {
						char *start = p;
						while (*p && *p!='\n') p++;
						int hasnl = (*p=='\n');
						char *eol = p;
						if (*p) p++;
						*eol = 0;
						char *q = start;
						while (*q==' '||*q=='\t') q++;
						int commented = 0;
						if (*q=='#') { commented = 1; q++; while (*q==' '||*q=='\t') q++; }
						if (*q=='[') {
							char secname[128] = "";
							char *a = q+1;
							int n = 0;
							while (*a && *a!=']' && n<120) secname[n++] = *a++;
							secname[n] = 0;
							if (secname[0]) {
								// perfis internos (DEFAULT/BISS Emu) nao aparecem na lista
								if (!strcmp(secname,"DEFAULT") || !strcmp(secname,"BISS Emu")) {
									struct cardserver_data *w2 = cfg.cardserver;
									while (w2) {
										if (!strcmp(w2->name, secname)) { if (nrendered<128) rendered[nrendered++] = w2->id; break; }
										w2 = w2->next;
									}
								}
								else {
								struct cardserver_data *fcs = NULL;
								struct cardserver_data *walk = cfg.cardserver;
								while (walk) {
									if (!profile_is_rendered(rendered, nrendered, walk->id) && !strcmp(walk->name, secname)) { fcs = walk; break; }
									walk = walk->next;
								}
								if (fcs) {
									if (alt==1) alt=2; else alt=1;
									getprofilecells( fcs, cell );
									snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'><td>%s</td><td class=%s>%s</td><td align=center>%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>",fcs->id,alt,fcs->id,cell[0],cell[10],cell[1],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8],cell[9]);
									tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
									if (nrendered<128) rendered[nrendered++] = fcs->id;
								}
								else if (commented) {
									char encname[160];
									int a = 0, b = 0;
									while (secname[b] && a<155) {
										if (secname[b]==' ') { encname[a++]='%'; encname[a++]='2'; encname[a++]='0'; }
										else encname[a++] = secname[b];
										b++;
									}
									encname[a] = 0;
									if (alt==1) alt=2; else alt=1;
									snprintf( http_buf, sizeof(http_buf),"\n<tr class=alt%d><td>%s (comentado)</td><td align=center>-</td><td align=center>-</td><td align=center>-</td><td align=center>-</td><td align=center>-</td><td align=center>-</td><td align=center>-</td><td align=center>-</td><td><span style='float:right;'><span class='icobtn on' title='Ativar (remove o # no profiles.cfg)' onclick=\"imgrequest('/profiles?action=onprof&name=%s',this);setTimeout('updateDiv()',3000);setTimeout('updateDiv()',6000);setTimeout('updateDiv()',9000)\">ON</span></span></td></tr>", alt, secname, encname);
									tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
								}
								} // fim do else (skip DEFAULT/BISS Emu)
							}
						}
						if (hasnl) *eol = '\n';
					}
				}
			}
			pthread_mutex_unlock(&pfile_mutex);
		}
	}

	// 2) perfis ativos que nao estao no profiles.cfg
	struct cardserver_data *cs = cfg.cardserver;
	while(cs) {
		if ( strcmp(cs->name,"DEFAULT") && strcmp(cs->name,"BISS Emu") && !profile_is_rendered(rendered, nrendered, cs->id)) {
			if (alt==1) alt=2; else alt=1;
			getprofilecells( cs, cell );
			snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'><td>%s</td><td class=%s>%s</td><td align=center>%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>",cs->id,alt,cs->id,cell[0],cell[10],cell[1],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8],cell[9]);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		cs = cs->next;
	}
	// Total
	if (alt==1) alt=2; else alt=1;
	snprintf( http_buf, sizeof(http_buf),"\n<tr class=alt3><td align=right>Total</td><td align=center>%d</td><td align=center>--</td>",total_profiles()); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	int totecm = 0;
	int totecmaccepted = 0;
	int totecmok = 0;
	int totcachehits = 0;
	int totcacheihits = 0;
	cs = cfg.cardserver;
	while(cs) {
		totecm += cs->ecmaccepted+cs->ecmdenied;
		totecmaccepted += cs->ecmaccepted;
		totecmok += cs->ecmok;
		totcachehits += cs->hits.csp;
#ifdef CACHEEX
		totcachehits += cs->hits.cacheex;
#endif
		totcacheihits += cs->hits.instant.csp;
#ifdef CACHEEX
		totcacheihits += cs->hits.instant.cacheex;
#endif
		cs = cs->next;
	}
	sprintf( http_buf,"<td align=center>%d</td>",totecm); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_writeecmdata(&tcpbuf, sock, totecmaccepted, totecm);
	tcp_writeecmdata(&tcpbuf, sock, totecmok, totecmaccepted);
	tcp_writeecmdata(&tcpbuf, sock, totcachehits, totecmok);
	tcp_writeecmdata(&tcpbuf, sock, totcacheihits, totcachehits);
	sprintf( http_buf, "<td colspan=2> </td></tr>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// Speed
	uint32_t ticks = GetTickCount()/1000;
	snprintf( http_buf, sizeof(http_buf),"<tr class=alt2><td align=right>Average speed</td><td colspan=2 align=center>(per minute)</td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<td align=center>%d</td>", totecm*60/ticks); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<td>%d</td>", totecmaccepted*60/ticks); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<td>%d</td>", totecmok*60/ticks); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<td>%d</td>", totcachehits*60/ticks); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<td>%d</td>", totcacheihits*60/ticks); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<td colspan=2> </td></tr>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	sprintf( http_buf, "\n</table>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	if (get_action==ACTION_PAGE) {
		tcp_writestr(&tcpbuf, sock, "\n</div></body></html>");
	}
	tcp_flush(&tcpbuf, sock);
}

///////////////////////////////////////////////////////////////////////////////
// PROFILE
///////////////////////////////////////////////////////////////////////////////

void cs_clients( struct cardserver_data *cs, int *total, int *connected, int *active )
{
	*total = 0;
	*connected = 0;
	*active = 0;
	struct cs_client_data *cli=cs->newcamd.client;
	while (cli) {
		(*total)++;
		if (cli->connection.status>0) {
			(*connected)++;
			if ( (GetTickCount()-cli->lastecmtime) < 20000 ) (*active)++;
		}
		cli=cli->next;
	}
}

void cs_allclients( int *total, int *connected, int *active )
{
	*total = 0;
	*connected = 0;
	*active = 0;

	struct cardserver_data *cs = cfg.cardserver;
	while (cs) {
		struct cs_client_data *cli = cs->newcamd.client;
		while (cli) {
			(*total)++;
			if (cli->connection.status>0) {
				(*connected)++;
				if ( (GetTickCount()-cli->lastecmtime) < 20000 ) (*active)++;
			}
			cli=cli->next;
		}
		cs = cs->next;
	}
}


#ifdef RADEGAST_SRV

int connected_radegast_clients(struct cardserver_data *cs)
{
	int nb=0;
	struct rdgd_client_data *rdgdcli=cs->radegast.client;
	if (cs->radegast.handle)
	while (rdgdcli) {
		if (rdgdcli->handle>0) nb++;
		rdgdcli=rdgdcli->next;
	}
	return nb;
}

#endif

char *programid(unsigned int id)
{
	typedef struct {
		char name[13];
		unsigned int id;
	} tnewcamdprog; 

	static tnewcamdprog camdp[] = { 
		{ "Generic", 0x0000 },
		{ "VDRSC",   0x5644 },
		{ "LCE", 0x4C43 },
		{ "Camd3", 0x4333 },
		{ "Radegast", 0x7264 },
		{ "Gbox2CS", 0x6762 },
		{ "Mgcamd", 0x6D67 },
		{ "WinCSC", 0x7763 },
		{ "newcs", 0x6E73 },
		{ "cx", 0x6378 },
		{ "Kaffeine", 0x6B61 },
		{ "Evocamd", 0x6576 },
		{ "CCcam", 0x4343 },
		{ "Tecview", 0x5456 },
		{ "AlexCS", 0x414C },
		{ "Rqcamd", 0x0666 },
		{ "Rq-echo", 0x0667 },
		{ "Acamd", 0x9911 },
		{ "Cardlink", 0x434C },
		{ "Octagon", 0x4765 },
		{ "sbcl", 0x5342 },
		{ "NextYE2k", 0x6E65 },
		{ "NextYE2k", 0x4E58 },
		{ "DiabloCam/UW", 0x4453 },
		{ "OScam", 0x8888 },
		{ "Scam", 0x7363 },
		{ "Rq-sssp/CW", 0x0669 },
		{ "Rq-sssp/CS", 0x0665 },
		{ "JlsRq", 0x0769 },
		{ "eyetvCamd", 0x4543 }
	};
	static char unknown[] = "Unknown";
	unsigned int i;
	id = id & 0xffff;
	for( i=0; i<sizeof(camdp)/sizeof(tnewcamdprog); i++ )
		if (camdp[i].id==id) return camdp[i].name;
	return unknown;
}

char* str_laststatus[] = { "NOK", "OK", "BISS EMU" };


///////////////////////////////////////////////////////////////////////////////

void getnewcamdclientcells(struct cs_client_data *cli, char cell[10][2048])
{
	char temp[2048];
	unsigned int ticks = GetTickCount();
	unsigned int d;
	// CELL0 # User name
	sprintf( cell[0],"<a href='/newcamdclient?id=%d'>%s</a>", cli->id, cli->user);
	// CELL1 # PROGRAM ID
	if (cli->connection.status>0)
		sprintf( cell[1],"<span title='%04x'>%s</span>", cli->progid, programid(cli->progid));
	else
		strcpy( cell[1], " ");
	// CELL2 # IP
	if (cli->connection.status>0) {
		char *p = getcountrycodebyip(cli->ip);
		if (p) sprintf( cell[2],"<img src='/flag_%s.gif' title='%s'> %s", p, getcountryname(p), (char*)ip2string(cli->ip) ); else sprintf( cell[2], "%s", (char*)ip2string(cli->ip) );
	}
	else
		strcpy( cell[2], " ");
	// CELL3 # CONNECTION TIME
	if (cli->connection.status>0) {
		d = (ticks-cli->connection.time)/1000;
		if (cli->ecm.busy) sprintf( cell[9], "busy"); else sprintf( cell[9], "online");
		sprintf( cell[3],"%02dd %02d:%02d:%02d", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
	}
	else {
		sprintf( cell[9], "offline");
		if (cli->flags&FLAG_DELETE) sprintf( cell[3],"Removed");
		else if (cli->flags&FLAG_EXPIRED) sprintf( cell[3],"Expired");
		else if (cli->flags&FLAG_DISABLE) sprintf( cell[3],"Disabled");
		else sprintf( cell[3],"offline");
	}
#ifdef EXPIREDATE
	if (cli->enddate.tm_year) {
		sprintf( temp,"<br>Expire: %d-%02d-%02d", 1900+cli->enddate.tm_year, cli->enddate.tm_mon+1, cli->enddate.tm_mday);
		strcat( cell[3], temp );
	}
#endif
	sprintf( temp, "<table class=\"connect_data\"><tr><td>Successful Login: %d</td><td>Aborted Connections: %d</td><td>Total Zap: %d</td><td>Channel Freeze: %d</td></tr></table>", cli->nblogin, cli->nbloginerror, cli->zap, cli->freeze );
	strcat( cell[3], temp );

	// ECM STAT
#ifdef SRV_CSCACHE
	if (cli->cachedcw) sprintf( cell[4], "%d [%d]", cli->ecmnb, cli->cachedcw); else sprintf( cell[4], "%d", cli->ecmnb );
#else
	sprintf( cell[4], "%d", cli->ecmnb );
#endif
	//
	int ecmaccepted = cli->ecmnb-cli->ecmdenied;
	getstatcell( ecmaccepted, cli->ecmnb, cell[5]);
	getstatcell( cli->ecmok, ecmaccepted, cell[6]);
	// Ecm Time
	if (cli->ecmok)
		sprintf( cell[7],"%d ms",(cli->ecmoktime/cli->ecmok) );
	else
		sprintf( cell[7],"-- ms");

	//Last Used Share
	if (cli->connection.status<=0 && cli->connection.lastseen) {
		d = (ticks-cli->connection.lastseen)/1000;
		sprintf( cell[8],"Last Seen %02dd %02d:%02d:%02d", d/(3600*24),(d/3600)%24,(d/60)%60,d%60);
	}
	else if ( cli->lastecm.caid ) {
		if (cli->lastecm.status) sprintf( cell[8],"<span class=success"); else sprintf( cell[8],"<span class=failed");
		sprintf( temp," title='%04x:%06x:%04x'>ch %s (%dms) %s ",cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid, getchname(cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid) , cli->lastecm.decodetime, str_laststatus[cli->lastecm.status] );
		strcat( cell[8], temp);
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->lastecm.status) {
				strcat( cell[8], " / from ");
				src2string(cli->lastecm.dcwsrctype, cli->lastecm.dcwsrcid, temp);
				strcat( cell[8], temp);
			}
		}
		strcat( cell[8], "</span>");
	}
	else strcpy( cell[8], " ");

	strcat( cell[8], "<br><span style='display:inline-flex;gap:2px;white-space:nowrap;margin-top:4px;'>");
	if ( !(cli->flags&(FLAG_DELETE|FLAG_EXPIRED)) ) {
		if (cli->flags&FLAG_DISABLE) {
			sprintf( temp," <span class='icobtn on' title='Enable' onclick=\"imgrequest('/newcamdclient?id=%d&action=enable',this);setTimeout('updateDiv()',600)\">ON</span>",cli->id);
			strcat( cell[8], temp );
		}
		else {
			sprintf( temp," <span class='icobtn off' title='Disable' onclick=\"imgrequest('/newcamdclient?id=%d&action=disable',this);setTimeout('updateDiv()',600)\">OFF</span>",cli->id);
			strcat( cell[8], temp );
		}
	}
	sprintf( temp," <span class='icobtn dbg' title='Debug' onclick=\"toggleDbgRow(%d,'/newcamdclient?id=%d&action=dbginfo')\">DBG</span>",cli->id,cli->id);
	strcat( cell[8], temp );
	strcat( cell[8], "</span>");

}

void http_send_newcamd(int sock, http_request *req) // page, div, row
{
	char http_buf[2048];
	char cell[10][2048];
	struct tcp_buffer_data tcpbuf;
	struct cs_client_data *cli;

	//  Get Params
	char *str_list = isset_get( req, "list");
	int get_list = LIST_ALL;
	if (str_list) {
		if (!strcmp(str_list,"connected")) get_list = LIST_CONNECTED;
		else if (!strcmp(str_list,"all")) get_list = LIST_ALL;
		else str_list=NULL;
	}
	if (!str_list) str_list = "all";
	// Param 'action'
	char *str_action = isset_get( req, "action");
	int get_action;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = ACTION_DIV;
		else if (!strcmp(str_action,"row")) get_action = ACTION_ROW;
#ifndef PUBLIC
		else if (!strcmp(str_action,"xml")) get_action = ACTION_XML; // Get Clients info in xml
#endif
		else if (!strcmp(str_action,"disable")) get_action = ACTION_DISABLE;
		else if (!strcmp(str_action,"enable")) get_action = ACTION_ENABLE;
		else if (!strcmp(str_action,"status")) get_action = ACTION_STATUS;
		else if (!strcmp(str_action,"debug")) get_action = ACTION_DEBUG;
		else str_action = NULL;
	}
	if (!str_action) { str_action = "page"; get_action = ACTION_PAGE; }

	// Profile ID
	char *str_pid = isset_get( req, "pid");
	int get_pid;
	if (str_pid) get_pid = atoi(str_pid); else get_pid = 0;
	struct cardserver_data *cs = getcsbyid(get_pid);
////
	if (get_action==ACTION_XML) {
		tcp_init(&tcpbuf);
		tcp_writestr(&tcpbuf, sock, "HTTP/1.1 200 OK\r\nAccept-Ranges: bytes\r\nConnection: close\r\nContent-Type: application/xml\r\n\r\n");

		tcp_writestr(&tcpbuf, sock, "<multics>");

		struct cardserver_data *srv;
		if (cs) srv = cs; else srv = cfg.cardserver;
		while (srv) {
			tcp_writestr(&tcpbuf, sock, "\n<newcamd>");
			sprintf(http_buf, "<id>%d</id>", srv->id); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf(http_buf, "<port>%d</port>", srv->newcamd.port); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf(http_buf, "<status>%d</status>", (srv->newcamd.handle>0) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			uint32_t ticks = GetTickCount();
			struct cs_client_data *cli = srv->newcamd.client;
			while (cli) {
				tcp_writestr(&tcpbuf, sock, "<user>");
				sprintf(http_buf, "<name>%s</name>", cli->user); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				if (cli->connection.status>0) {
					tcp_writestr(&tcpbuf, sock, "<status>1</status>");
					sprintf( http_buf,"<ip>%s</ip>", (char*)ip2string(cli->ip) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					char *p = getcountrycodebyip(cli->ip);
					if (p) sprintf(http_buf, "<country>%s</country>", p); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					uint32_t d = (ticks - cli->connection.time)/1000;
					sprintf(http_buf, "<connected>%02dd %02d:%02d:%02d</connected>", d/(3600*24), (d/3600)%24, (d/60)%60, d%60); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				else {
					sprintf(http_buf, "<status>%d</status>",cli->flags&0x0E);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				tcp_writestr(&tcpbuf, sock, "</user>");
				cli = cli->next;
			}
			tcp_writestr(&tcpbuf, sock, "\n</newcamd>");

			if (cs) break; else srv = srv->next;
		}
		tcp_writestr(&tcpbuf, sock, "\n</multics>");
		tcp_flush(&tcpbuf, sock);
		return;
	}

	char *id = isset_get( req, "id");
	if (id)	{ // XML
		int i = atoi(id);
		struct cs_client_data *cli = getnewcamdclientbyid(i);
		if (!cli) return;
		// Send XML CELLS
		getnewcamdclientcells(cli,cell);
		for(i=0; i<10; i++) xmlescape( cell[i] );
		char buf[5000] = "";
		snprintf( buf, sizeof(buf), "<newcamd>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2>%s</c2>\n<c3_c>%s</c3_c>\n<c3>%s</c3>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6>\n<c7>%s</c7>\n<c8>%s</c8>\n</newcamd>\n",cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8] );
		http_send_xml( sock, req, buf, strlen(buf));
		return;
	}

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) ); // header tambem no div (XHR exige status line)
	if (get_action==ACTION_PAGE) {
		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, html_title, cfg.http.title, "Newcamd"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		// JS
        tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
		tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
		// ACTIONS REQUEST
		tcp_writestr(&tcpbuf, sock, "\nfunction imgrequest( url, el )\n{\n	var httpRequest;\n	try { httpRequest = new XMLHttpRequest(); }\n	catch (trymicrosoft) { try { httpRequest = new ActiveXObject('Msxml2.XMLHTTP'); } catch (oldermicrosoft) { try { httpRequest = new ActiveXObject('Microsoft.XMLHTTP'); } catch(failed) { httpRequest = false; } } }\n	if (!httpRequest) { alert('Your browser does not support Ajax.'); return false; }\n	if ( typeof(el)!='undefined' ) {\n		el.onclick = null;\n		el.style.opacity = '0.7';\n		httpRequest.onreadystatechange = function()\n		{\n			if (httpRequest.readyState == 4) if (httpRequest.status == 200) el.style.opacity = '0.3';\n		}\n	}\n	httpRequest.open('GET', url, true);\n	httpRequest.send(null);\n}\n");
		// UPD ROW
		tcp_writestr(&tcpbuf, sock, "\nfunction xmlupdateRow( xmlDoc, id )\n{\n	var row = document.getElementById(id);\n	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n	row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n	row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n	row.cells.item(3).className = xmlDoc.getElementsByTagName('c3_c')[0].childNodes[0].nodeValue;\n	row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n	row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n	row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n	row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n	row.cells.item(7).innerHTML = xmlDoc.getElementsByTagName('c7')[0].childNodes[0].nodeValue;\n	row.cells.item(8).innerHTML = xmlDoc.getElementsByTagName('c8')[0].childNodes[0].nodeValue;\n}\n");
		char url[256];
		sprintf( url, "'/newcamd?id='+idx");
		sprintf( http_buf, HTTP_UPDATE_ROW, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// UPD DIV
		sprintf( url, "/newcamd?pid=%d&list=%s&action=div", get_pid, str_list);
		sprintf( http_buf, HTTP_UPDATE_DIV, cfg.http.autorefresh*1000, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	setautorefresh(autorefresh);\n}");
		tcp_writestr(&tcpbuf, sock, "\n</script>\n");
		//
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_writestr(&tcpbuf, sock, "\n<body onload=\"start();\">");
		tcp_write_menu(&tcpbuf, sock,PAGE_NEWCAMD);
		// Info de servidores (acima da div principal)
		{
			tcp_writestr(&tcpbuf, sock, "<div style='margin:12px 12px 0 12px'><div class=stat-section style='margin:0'>");
			sprintf( http_buf, "<h3 class=stitle>Newcamd Profiles (%d)</h3>", total_profiles());
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			tcp_writestr(&tcpbuf, sock, "<table class=maintable><tr><th>Profile</th><th>Port</th><th>Status</th><th>Connected</th></tr>");
			int itotal, iconnected, iactive;
			cs_allclients( &itotal, &iconnected, &iactive );
			sprintf( http_buf, "<tr><td class=left>TOTAL</td><td class=right>-</td><td class=right>-</td><td class=right>%d / %d</td></tr>", iconnected, itotal);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			struct cardserver_data *box = cfg.cardserver;
			while (box) {
				int btotal, bconnected, bactive;
				cs_clients( box, &btotal, &bconnected, &bactive );
				if (box->newcamd.handle>0) sprintf( http_buf, "<tr><td class=left><a href='/newcamd?pid=%d'>%s</a></td><td class=right>%d</td><td class=right><span class=success>ONLINE</span></td><td class=right>%d / %d</td></tr>", box->id, box->name, box->newcamd.port, bconnected, btotal);
				else sprintf( http_buf, "<tr><td class=left><a href='/newcamd?pid=%d'>%s</a></td><td class=right>%d</td><td class=right><span class=failed>OFFLINE</span></td><td class=right>%d / %d</td></tr>", box->id, box->name, box->newcamd.port, bconnected, btotal);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				box = box->next;
			}
			tcp_writestr(&tcpbuf, sock, "</table></div></div>");
		}
		// DIV
		tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");
	}

	sprintf( http_buf, "<select style=\"width:200px;\" onchange=\"parent.location.href='/newcamd?pid='+this.value\"> ><option value=0>All Profiles(%d)</option>", total_profiles());
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	struct cardserver_data *tmp = cfg.cardserver;
	while (tmp) {
		if (tmp->id==get_pid) sprintf( http_buf, "<option value=%d selected> [%d] %s </option>", tmp->id, tmp->newcamd.port, tmp->name);
		else sprintf( http_buf, "<option value=%d> [%d] %s </option>", tmp->id, tmp->newcamd.port, tmp->name);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tmp = tmp->next;
	}
	sprintf( http_buf, "</select> ");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	//
	int total, connected, active;
	if (cs) cs_clients( cs, &total, &connected, &active ); else cs_allclients( &total, &connected, &active );
	char *class1 = "button"; char *class2 = "sbutton";
	char *class;
	if (get_list==LIST_ACTIVE) class = class2; else class = class1;
	sprintf( http_buf, " <input type=button class=%s onclick=\"parent.location='/newcamd?pid=%d&amp;list=active'\" value='Active Clients(%d)'>", class, get_pid,active);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (get_list==LIST_CONNECTED) class = class2; else class = class1;
	sprintf( http_buf, " <input type=button class=%s onclick=\"parent.location='/newcamd?pid=%d&amp;list=connected'\" value='Connected Clients(%d)'>", class, get_pid,connected);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (get_list==LIST_ALL) class = class2; else class = class1;
	sprintf( http_buf, " <input type=button class=%s onclick=\"parent.location='/newcamd?pid=%d&amp;list=all'\" value='All Clients(%d)'>", class, get_pid,total);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );


	//NEWCAMD CLIENTS
	if (cs) {
		//
		sprintf( http_buf, "<br><table class=maintable width=100%%><tr><th width=100px>Client</th><th width=70px>Program</th><th width=120px>IP Address</th><th width=100px>Connected</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>\n");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		cli = cs->newcamd.client;
		int alt=0;

		if (get_list==LIST_ACTIVE) {
			while (cli) {
				if ( (cli->connection.status>0)&&((GetTickCount()-cli->lastecmtime) < 20000) ) {
					if (alt==1) alt=2; else alt=1;
					getnewcamdclientcells(cli, cell);
					snprintf( http_buf, sizeof(http_buf),"<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=%s>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				cli = cli->next;
			}
		}
		else if (get_list==LIST_ALL) {
			while (cli) {
				if (alt==1) alt=2; else alt=1;
				getnewcamdclientcells(cli, cell);
				snprintf( http_buf, sizeof(http_buf),"<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=%s>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				cli = cli->next;
			}
		}
		else if (get_list==LIST_CONNECTED) {
			while (cli) {
				if (cli->connection.status>0) {
					if (alt==1) alt=2; else alt=1;
					getnewcamdclientcells(cli, cell);
					snprintf( http_buf, sizeof(http_buf),"<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=%s>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				cli = cli->next;
			}
		}
	
		sprintf( http_buf, "</table>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	else {
		//
		sprintf( http_buf, "<br><table class=maintable width=100%%><tr><th width=100px>Client</th><th width=70px>Program</th><th width=120px>IP Address</th><th width=100px>Connected</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>\n");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		int alt=0;
		struct cardserver_data *cs = cfg.cardserver;
		while (cs) {
			int total, connected, active;
			cs_clients( cs, &total, &connected, &active );
			if ( (get_list==LIST_ACTIVE) && active ) {
				sprintf( http_buf, "<tr><td class=alt3 colspan=9>%s (%d)</td></tr>\n", cs->name, active); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				cli = cs->newcamd.client;
				while (cli) {
					if ( (cli->connection.status>0)&&((GetTickCount()-cli->lastecmtime) < 20000) ) {
						if (alt==1) alt=2; else alt=1;
						getnewcamdclientcells(cli, cell);
						snprintf( http_buf, sizeof(http_buf),"<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=%s>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
					cli = cli->next;
				}
			}
			else if ( (get_list==LIST_ALL) && total ) {
				sprintf( http_buf, "<tr><td class=alt3 colspan=9>%s (%d)</td></tr>\n", cs->name, total); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				cli = cs->newcamd.client;
				while (cli) {
					if (alt==1) alt=2; else alt=1;
					getnewcamdclientcells(cli, cell);
					snprintf( http_buf, sizeof(http_buf),"<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=%s>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					cli = cli->next;
				}
			}
			else if ( (get_list==LIST_CONNECTED) && connected ) {
				sprintf( http_buf, "<tr><td class=alt3 colspan=9>%s (%d)</td></tr>\n", cs->name, connected); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				cli = cs->newcamd.client;
				while (cli) {
					if (cli->connection.status>0) {
						if (alt==1) alt=2; else alt=1;
						getnewcamdclientcells(cli, cell);
						snprintf( http_buf, sizeof(http_buf),"<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=%s>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
					cli = cli->next;
				}
			}
			cs = cs->next;
		}
		sprintf( http_buf, "</table>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}

	if (get_action==ACTION_PAGE) {
		tcp_writestr(&tcpbuf, sock, "</div></body></html>");
	}

	tcp_flush(&tcpbuf, sock);
}


///////////////////////////////////////////////////////////////////////////////
void http_send_newcamd_client(int sock, http_request *req)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;
	char *str_id = isset_get( req, "id");
	if (!str_id) return; //error
	int get_id = atoi(str_id);
	//
	struct cs_client_data *cli = getnewcamdclientbyid( get_id );
	if (!cli) return;
	// Action
	char *str_action = isset_get( req, "action");
	int get_action = 0;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = 1;
		else if (!strcmp(str_action,"row")) get_action = 2;
		else if (!strcmp(str_action,"disable")) get_action = 3;
		else if (!strcmp(str_action,"enable")) get_action = 4;
		else if (!strcmp(str_action,"status")) get_action = 5;
		else if (!strcmp(str_action,"info")) get_action = 6; // XML info
		else if (!strcmp(str_action,"debug")) get_action = 7;
		else if (!strcmp(str_action,"dbginfo")) get_action = 8;
		else str_action = NULL;
	}
	if (!str_action) str_action = "page";
	//
	if (get_action==8) {
		char dbg[1536];
		sprintf( dbg, "<div class='dbginfo'><b>%s</b> | IP: %s | Status: %s | Logins: %d (err %d, dif IP %d)<br>ECM: %d pedidos, %d denied, %d OK | Last ECM: %us ago | Last DCW: %us ago<br>Last channel: %04x:%06x:%04x (%dms)%s | Type: %d | Flags: 0x%08x | Profile: %s</div>",
			cli->user, (char*)ip2string(cli->ip),
			cli->connection.status>0?"CONNECTED":(cli->connection.status<0?"CONNECTING...":"OFFLINE"),
			cli->nblogin, cli->nbloginerror, cli->nbdiffip,
			cli->ecmnb, cli->ecmdenied, cli->ecmok,
			cli->lastecmtime?(GetTickCount()-cli->lastecmtime)/1000:0,
			cli->lastdcwtime?(GetTickCount()-cli->lastdcwtime)/1000:0,
			cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid, cli->lastecm.decodetime,
			(cli->lastecm.status==2)?" <span class=nok-yellow>NOK (BISS EMU)</span>":"",
			cli->type, cli->flags, cli->cs?cli->cs->name:"-");
		http_send_text(sock, dbg);
		return;
	}
	if (get_action==3) {
		cli->flags |= FLAG_DISABLE;
		if (cli->connection.status>0) cs_disconnect_cli(cli);
		http_send_ok(sock);
		return;
	}
	else if (get_action==4) {
		cli->flags &= ~FLAG_DISABLE;
		http_send_ok(sock);
		return;
	}
	else if (get_action==5) {
		if (cli->connection.status>0) http_send_text(sock,"connected"); else http_send_text(sock,"disconnected");
		return;
	}
	else if (get_action==7) {
		flagdebug = getdbgflag( DBG_NEWCAMD, cli->pid, cli->id);
		http_send_ok(sock);
		return;
	}
	//
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	if (get_action==0) {
		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, html_title, cfg.http.title, "Newcamd Client"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		// JS
        tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
		tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
		// UPD DIV
		char url[256];
		sprintf( url, "/newcamdclient?id=%d&action=div", get_id);
		sprintf( http_buf, HTTP_UPDATE_DIV, cfg.http.autorefresh*1000, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	setautorefresh(autorefresh);\n}");
		tcp_writestr(&tcpbuf, sock, "\n</script>\n");
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
		tcp_write_menu(&tcpbuf, sock,0);
		// DIV
		tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");
	}
	//
	tcp_writestr(&tcpbuf, sock, "<table style=\"padding:0px; margin:0px;\" width=\"100%%\"><tbody>\n" );
	tcp_writestr(&tcpbuf, sock, "<tr><td style=\"vertical-align:top; width:400px;\">\n" );
	//
	tcp_writestr(&tcpbuf, sock, "<table class=infotable><tbody>\n<tr><th colspan=2>Newcamd Client Informations</th></tr>\n" );
	// Profile
	struct cardserver_data *cs = getcsbyid( cli->pid );
	if (cs) {
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Profile</td><td class=right><a href='/profile?id=%d'>%s</a></td></tr>\n", cs->id, cs->name);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	// NAME
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>User name</td><td class=right>%s</td></tr>\n",cli->user);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// Connection Time
	if (cli->connection.status>0) {
		tcp_writestr(&tcpbuf, sock, "<tr><td class=left>Status</td><td class=right>Connected</td></tr>\n");
		uint32_t d = (GetTickCount()-cli->connection.time)/1000;
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Connection time</td><td class=right>%02dd %02d:%02d:%02d</td></tr>\n", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// IP
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>IP Address</td><td class=right>%s</td></tr>\n",(char*)ip2string(cli->ip) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// Program ID
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Client Program</td><td class=right>%s(%04x)</td></tr>",programid(cli->progid), cli->progid );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	else {
		tcp_writestr(&tcpbuf, sock, "<tr><td class=left>Status</td><td class=right>Disconnected</td></tr>\n");
		if ( cli->connection.status<=0 && cli->connection.lastseen) {
			uint32_t d = (GetTickCount()-cli->connection.lastseen)/1000;
			snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Last Seen</td><td class=right>%02dd %02d:%02d:%02d</td></tr>\n", d/(3600*24),(d/3600)%24,(d/60)%60,d%60);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
	}
	// UPTIME
	if ( cli->connection.uptime || (cli->connection.status>0) ) {
		uint32_t uptime;
		if (cli->connection.status>0) uptime = (GetTickCount()-cli->connection.time)+cli->connection.uptime; else uptime = cli->connection.uptime;
		uptime /= 1000;
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Uptime</td><td class=right>%02dd %02d:%02d:%02d</td></tr>",uptime/(3600*24),(uptime/3600)%24,(uptime/60)%60,uptime%60);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
#ifdef CHECK_NEXTDCW
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>DCW CHECK</td><td class=right>%s</td></tr>", yesno(cli->dcwcheck) );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
	tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );

	// INFO
	struct client_info_data *info = cli->info;
	if (info) {
		tcp_writestr(&tcpbuf, sock, "<table class=\"infotable\"><tbody>\n" );
		tcp_writestr(&tcpbuf, sock, "<tr><th colspan=2>Additional Informations</th></tr>\n" );
		while (info) {
			snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>%s</td><td class=right>%s</td></tr>\n",info->name,info->value);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			info = info->next;
		}
		tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );
	}

	// Ecm Stat
	tcp_writestr(&tcpbuf, sock, "<table class=\"infotable\"><tbody>\n" );
	tcp_writestr(&tcpbuf, sock, "<tr><th colspan=2>ECM Statistics</th></tr>\n" );
	int ecmaccepted = cli->ecmnb-cli->ecmdenied;
	sprintf( http_buf, "<tr><td class=left>Total ECM requests</td><td class=right>%d</td></tr>\n", cli->ecmnb);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<tr><td class=left>Accepted ECM requests</td><td class=right>%d</td></tr>\n", ecmaccepted);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<tr><td class=left>Good ECM answer</td><td class=right>%d</td></tr>\n", cli->ecmok);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//Ecm Time
	if (cli->ecmok) {
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Average Time</td><td class=right>%d ms</td></tr>\n",(cli->ecmoktime/cli->ecmok) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
#ifdef SRV_CSCACHE
	sprintf( http_buf, "<tr><td class=left>Cached DCW</td><td class=right>%d</td></tr>\n", cli->cachedcw);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
	// Freeze
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Total Freeze</td><td class=right>%d</td></tr>\n", cli->freeze);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );


	tcp_writestr(&tcpbuf, sock, "</td><td style=\"vertical-align:top;\">\n" );

	//Last Used Share
	if ( cli->lastecm.caid ) {
		tcp_writestr(&tcpbuf, sock, "<table class=\"infotable\"><tbody>\n" );
		tcp_writestr(&tcpbuf, sock, "<tr><th>Last Used share</th></tr>\n");
		// Decode Status
		if (cli->lastecm.status)
			snprintf( http_buf, sizeof(http_buf),"<tr><td>Decode success</td></tr>\n");
		else
			snprintf( http_buf, sizeof(http_buf),"<tr><td>Decode failed</td></tr>\n");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// Channel
		snprintf( http_buf, sizeof(http_buf),"<tr><td>Channel %s (%dms) %s</td></tr>\n", getchname(cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid) , cli->lastecm.decodetime, str_laststatus[cli->lastecm.status] );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		// Server
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->lastecm.status) {
				tcp_writestr(&tcpbuf, sock, "<tr><td>From ");
				src2string(cli->lastecm.dcwsrctype, cli->lastecm.dcwsrcid, http_buf );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				tcp_writestr(&tcpbuf, sock, "</td></tr>");
			}
			// Last ECM
			ECM_DATA *ecm = cli->lastecm.request;
			// ECM
			snprintf( http_buf, sizeof(http_buf),"<tr><td>ECM(%d): ", ecm->ecmlen); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			array2hex( ecm->ecm, http_buf, ecm->ecmlen );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf( http_buf,"</td></tr>\n"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			// DCW
			if (cli->lastecm.status) {
				snprintf( http_buf, sizeof(http_buf),"<tr><td>CW: ");	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->cw, http_buf, 16 );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				sprintf( http_buf,"</td></tr>\n"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
#ifdef CHECK_NEXTDCW
			if ( ecm->lastdecode.ecm && (ecm->lastdecode.counter>0) ) {
				snprintf( http_buf, sizeof(http_buf),"<tr><td>Previous CW: "); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->lastdecode.dcw, http_buf, 16 ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				tcp_writestr(&tcpbuf, sock, "</td></tr>\n");
				if (ecm->lastdecode.error) {
					snprintf( http_buf, sizeof(http_buf),"<tr><td>Errors = %d</td></tr>\n", ecm->lastdecode.error);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				snprintf( http_buf, sizeof(http_buf),"<tr><td>Total Cycles = %d</td></tr>\n<tr><td>ECM Interval = %ds</td></tr>\n", ecm->lastdecode.counter, ecm->lastdecode.dcwchangetime/1000);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
#endif
			// Last used share (status do ultimo decode)
			if (cli->lastecm.status==1) {
				tcp_writestr(&tcpbuf, sock, "<tr><td class=success>Decode Success</td></tr>");
			}
			else if (cli->lastecm.status==2) {
				snprintf( http_buf, sizeof(http_buf),"<tr><td class=nok-yellow>channel %s (%dms) NOK (BISS EMU)</td></tr>", getchname(cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid), cli->lastecm.decodetime);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			//
			if (ecm->server[0].srvid) {
				sprintf( http_buf, "<tr><td><table class='infotable'><tbody><tr><th width='30px'>ID</th><th width='250px'>Server</th><th width='50px'>Status</th><th width='70px'>Start time</th><th width='70px'>End time</th><th width='90px'>Elapsed time</th><th>CW</th></tr></tbody>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				int i;
				for(i=0; i<20; i++) {
					if (!ecm->server[i].srvid) break;
					char* str_srvstatus[] = { "WAIT", "OK", "NOK", "BUSY" };
					struct server_data *srv = getsrvbyid(ecm->server[i].srvid);
					if (srv) {
						snprintf( http_buf, sizeof(http_buf),"<tr><td>%d</td><td>%s:%d</td><td>%s</td><td>%dms</td>", i+1, srv->host->name, srv->port, str_srvstatus[ecm->server[i].flag], ecm->server[i].sendtime - ecm->recvtime );
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// Recv Time
						if (ecm->server[i].statustime>ecm->server[i].sendtime)
							sprintf( http_buf,"<td>%dms</td><td>%dms</td>", ecm->server[i].statustime - ecm->recvtime, ecm->server[i].statustime-ecm->server[i].sendtime );
						else
							sprintf( http_buf,"<td>--</td><td>--</td>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// DCW
						if (ecm->server[i].flag==ECM_SRV_REPLY_GOOD) {
							sprintf( http_buf,"<td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							array2hex( ecm->server[i].dcw, http_buf, 16 );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							sprintf( http_buf,"</td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						else {
							sprintf( http_buf,"<td>--</td>");
							tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						sprintf( http_buf,"</tr>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
				}
				tcp_writestr(&tcpbuf, sock, "</tbody></table></td></tr>\n" );
			}
		}
		tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );
	}

	// Current Busy Ecm
	if (cli->ecm.busy) {
		ECM_DATA *ecm = cli->ecm.request;
		if (ecm) http_send_ecmstatus(&tcpbuf, sock, ecm);
	}

	tcp_writestr(&tcpbuf, sock, "</td></tr></tbody></table>" );

	if (get_action==0) {
		tcp_writestr(&tcpbuf, sock, "</div>");
		tcp_writestr(&tcpbuf, sock, "</body></html>");
	}
	tcp_flush(&tcpbuf, sock);
}


void http_send_profile(int sock, http_request *req)
{
	char http_buf[1024];
	struct tcp_buffer_data tcpbuf;
	// Get Profile
	int get_id = 0;
	char *str_id = isset_get( req, "id");
	if (str_id)	get_id = atoi(str_id);
	struct cardserver_data *cs = getcsbyid(get_id);
	if (!cs) {
		// perfil comentado no profiles.cfg (ou inexistente): vai para a lista, onde o ON existe
		http_send_redirect(sock, "/profiles");
		return;
	}
	// Action
	char *str_action = isset_get( req, "action");
	int get_action = 0;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = 1;
		else if (!strcmp(str_action,"row")) get_action = 2;
		else if (!strcmp(str_action,"disable")) get_action = 3;
		else if (!strcmp(str_action,"enable")) get_action = 4;
		else if (!strcmp(str_action,"status")) get_action = 5;
#ifndef PUBLIC
		else if (!strcmp(str_action,"xml")) get_action = 6; // XML info
#endif
		else if (!strcmp(str_action,"debug")) get_action = 7;
		else if (!strcmp(str_action,"dbginfo")) get_action = 8;
		else if (!strcmp(str_action,"off")) get_action = 9;  // comentar perfil no profiles.cfg
		else if (!strcmp(str_action,"on")) get_action = 10;  // descomentar perfil no profiles.cfg
		else str_action = NULL;
	}
	if (!str_action) str_action = "page";
	//
	if (get_action==9) {
		profile_config_toggle(cs->name, 0);
		http_send_ok(sock);
		return;
	}
	else if (get_action==10) {
		profile_config_toggle(cs->name, 1);
		http_send_ok(sock);
		return;
	}
	if (get_action==8) {
		char dbg[1536];
		int btotal, bconnected, bactive;
		cs_clients( cs, &btotal, &bconnected, &bactive );
		sprintf( dbg, "<div class='dbginfo'><b>%s</b> (id %d) | CAID: %04x | Port: %d | Clients: %d (%d ligados)<br>ECM checks: DCW=%s | DCW timeout: %dms | Max servers: %d | Cache: %s%s%s%s%s%s</div>",
			cs->name, cs->id, cs->card.caid, cs->newcamd.port, btotal, bconnected,
			cs->option.dcw.check?"YES":"NO", cs->option.dcw.timeout, cs->option.server.max,
			cs->option.fallowcache?"ON":"OFF",
			IS_DISABLED(cs->flags)?" | DISABLED":"",
			cs->option.dcw.cak7?" | CAK7: ON":"",
			cs->option.ecmfilter.enable? (cs->option.ecmfilter.mode?" | ECM FILTER: DROP":" | ECM FILTER: LOGONLY"):"",
			cs->option.dcwfilter.enable? (cs->option.dcwfilter.mode==2?(cs->option.dcwfilter.auto_active?" | DCW FILTER: AUTO (ATIVO)":" | DCW FILTER: AUTO"):(cs->option.dcwfilter.mode?" | DCW FILTER: DROP":" | DCW FILTER: LOGONLY")):"",
			cs->option.ratelimit.sidtime||cs->option.ratelimit.maxecm?" | RATELIMIT: ON":"");
		http_send_text(sock, dbg);
		return;
	}
	if (get_action==3) {
		cs->flags |= FLAG_DISABLE;
		////////// cc_disconnect_cli(cli);
		http_send_ok(sock);
		return;
	}
	else if (get_action==4) {
		cs->flags &= ~FLAG_DISABLE;
		http_send_ok(sock);
		return;
	}
	else if (get_action==5) {
		if (IS_DISABLED(cs->flags)) http_send_text(sock,"active"); else http_send_text(sock,"inactive");
		return;
	}
	else if (get_action==7) {
		flagdebug = getdbgflag( DBG_NEWCAMD, cs->id, 0);
		http_send_ok(sock);
		return;
	}

	//
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, html_title, cfg.http.title, "Profile"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
	tcp_write_menu(&tcpbuf, sock,0);

	sprintf( http_buf, "<input type=button onclick=\"parent.location='/newcamd?pid=%d'\" value='Newcamd Clients'>", cs->id);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	sprintf( http_buf, "<br><br><div class=\"outer\"> <div class=\"top\"><b>Profile: %s</b><ul><li>Newcamd Port = %d</li>",cs->name, cs->newcamd.port);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#ifdef RADEGAST_SRV
	sprintf( http_buf, "<li>Radegast Port = %d</li>", cs->radegast.port);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
	sprintf( http_buf, "<li>Network ID = %04X</li>", cs->option.onid);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<li>Caid = %04X</li><li>Providers =", cs->card.caid);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	int i;
	for (i=0;i<cs->card.nbprov;i++) {
		sprintf( http_buf, " %06x",cs->card.prov[i].id);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	sprintf( http_buf, "</li><li>Total ECM = %d</li> <li>Accepted ECM = %d</li><li>Ecm OK = %d</li><li>Ecm Time = %dms</li><li>Total Cache Hits = %d</li><li>Instant Cache Hits = %d</li></ul>", cs->ecmaccepted+cs->ecmdenied, cs->ecmaccepted, cs->ecmok, cs->ecmoktime/(cs->ecmok+1), cs->hits.csp, cs->hits.instant.csp);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	sprintf( http_buf, "</div><span style=\"float:right\"><table class=option border=1px cellspacing=0><tr><th width=150px>Option</th><th width=50px>Value</th></tr>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	snprintf( http_buf, sizeof(http_buf),"<tr><td>ECM CHECK</td><td>%s</td></tr>", yesno(cs->option.checkecm)); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ECM CHECK LENGTH</td><td>%s</td></tr>", yesno(cs->option.checkecmlength)); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>DCW TIMEOUT</td><td>%dms</td></tr>", cs->option.dcw.timeout); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#ifndef PUBLIC
	snprintf( http_buf, sizeof(http_buf),"<tr><td>DCW RETRY</td><td>%d</td></tr>", cs->option.dcw.retry ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
	snprintf( http_buf, sizeof(http_buf),"<tr><td>DCW CHECK</td><td>%s</td></tr>", yesno(cs->option.dcw.check) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>DCW HALFNULLED</td><td>%s</td></tr>", yesno(cs->option.dcw.halfnulled) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#ifdef DCWSWAP
	snprintf( http_buf, sizeof(http_buf),"<tr><td>DCW SWAP</td><td>%s</td></tr>", yesno(cs->option.dcw.swap) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
	snprintf( http_buf, sizeof(http_buf),"<tr><td>DCW MAXFAILED</td><td>%d</td></tr>", cs->option.maxfailedecm); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>SERVER MAX</td><td>%d</td></tr>", cs->option.server.max); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>SERVER INTERVAL</td><td>%dms</td></tr>", cs->option.server.interval); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>SERVER TIMEOUT</td><td>%dms</td></tr>", cs->option.server.timeout); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
//	sprintf( http_buf,"<tr><td>SERVER TIMEPERECM:</td><td>%d</td></tr>", cs->option.server.timeperecm); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>SERVER VALIDECMTIME</td><td>%dms</td></tr>", cs->option.server.validecmtime); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>SERVER FIRST</td><td>%d</td></tr>", cs->option.server.first); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ACCEPT NULL CAID</td><td>%s</td></tr>", yesno(cs->option.faccept0caid) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ACCEPT NULL PROVIDER</td><td>%s</td></tr>", yesno(cs->option.faccept0provider) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ACCEPT NULL SID</td><td>%s</td></tr>", yesno(cs->option.faccept0sid) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ENABLE CCCAM</td><td>%s</td></tr>", yesno(cs->option.fallowcccam) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ENABLE NEWCAMD</td><td>%s</td></tr>", yesno(cs->option.fallownewcamd) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ENABLE RADEGAST</td><td>%s</td></tr>", yesno(cs->option.fallowradegast) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ENABLE CAMD35</td><td>%s</td></tr>", yesno(cs->option.fallowcamd35) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ENABLE CS378X</td><td>%s</td></tr>", yesno(cs->option.fallowcs378x) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ENABLE SKIPCWC</td><td>%s</td></tr>", yesno(cs->option.fallowskipcwc) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ENABLE CWC</td><td>%s (sens:%d dropold:%s keep:%dm onbad:%s)</td></tr>", yesno(cs->option.cwc.enable), cs->option.cwc.sensitive, yesno(cs->option.cwc.dropold), cs->option.cwc.keepcycletime, yesno(cs->option.cwc.dropbad) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ENABLE NAGRA</td><td>%s (chk:%s prov:%s cycle:%s onbad:%s sens:%d)</td></tr>", yesno(cs->option.nagra.enable), yesno(cs->option.nagra.chk), yesno(cs->option.nagra.prov), yesno(cs->option.nagra.cycle), yesno(cs->option.nagra.onbad), cs->option.nagra.sensitive ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ENABLE HEALTH</td><td>%s</td></tr>", yesno(cs->option.health.enable) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>DCW CAK7</td><td>%s</td></tr>", yesno(cs->option.dcw.cak7) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ECM FILTER</td><td>%s (%d regras)</td></tr>", cs->option.ecmfilter.enable?(cs->option.ecmfilter.mode?"DROP":"LOGONLY"):"OFF", cs->option.ecmfilter.nrules ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>DCW FILTER</td><td>%s (%d regras)</td></tr>", cs->option.dcwfilter.enable?(cs->option.dcwfilter.mode==2?(cs->option.dcwfilter.auto_active?"AUTO (ATIVO)":"AUTO"):(cs->option.dcwfilter.mode?"DROP":"LOGONLY")):"OFF", cs->option.dcwfilter.nrules ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ECMRATELIMIT</td><td>sid:%dms max:%d/s</td></tr>", cs->option.ratelimit.sidtime, cs->option.ratelimit.maxecm ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ENABLE FALLBACK</td><td>%s</td></tr>", yesno(cs->option.fallback.enable) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ENABLE TIMING</td><td>%s</td></tr>", yesno(cs->option.timing.enable) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ENABLE EMULATOR BISS</td><td>%s</td></tr>", yesno(cs->option.fenableemu) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ENABLE LITE</td><td>%s (channels:%d)</td></tr>", yesno(cs->option.fenablelite), lite_count() ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ENABLE CACHE</td><td>%s</td></tr>", yesno(cs->option.fallowcache) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#ifdef CACHEEX
	snprintf( http_buf, sizeof(http_buf),"<tr><td>ENABLE CACHEEX</td><td>%s</td></tr>", yesno(cs->option.fallowcacheex) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>CACHEEX MAXHOP</td><td>%d</td></tr>", cs->option.cacheex.maxhop ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//sprintf( http_buf,"<tr><td>CACHEEX VALIDECMTIME</td><td>%dms</td></tr>", cs->option.cacheexvalidtime ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
	//
	snprintf( http_buf, sizeof(http_buf),"<tr><td>RETRY NEWCAMD</td><td>%d</td></tr>", cs->option.retry.newcamd); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>RETRY CCCAM</td><td>%d</td></tr>", cs->option.retry.cccam); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>CACHE TIMEOUT</td><td>%dms</td></tr>", cs->option.cachetimeout); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>CACHE SENDREQ</td><td>%s</td></tr>", yesno(cs->option.cachesendreq) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#ifndef PUBLIC
	//sprintf( http_buf,"<tr><td>CACHE RESENDREQ</td><td>%s</td></tr>", yesno(cs->option.cacheresendreq) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>CACHE SENDREP</td><td>%s</td></tr>", yesno(cs->option.cachesendrep) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	snprintf( http_buf, sizeof(http_buf),"<tr><td>CACHE STATIC</td><td>%s</td></tr>", yesno(cs->option.cachestatic) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
	sprintf( http_buf, "</table></span></div><br><br>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_writestr(&tcpbuf, sock, "<div style=\"clear:both\"></div>" );

/*
#ifdef RADEGAST_SRV
	struct rdgd_client_data *rdgdcli;
	if (cs->radegast.handle && cs->radegast.client) {
		//READEGAST CLIENTS
		sprintf( http_buf, "<br>Connected Radegast Clients: %d<br><table class=maintable width=100%%><tr><th width=110px>IP Address</th><th width=100px>Connected</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>", connected_radegast_clients(cs));
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		rdgdcli = cs->radegast.client;
		int alt=0;
		while (rdgdcli) {
			if (rdgdcli->handle>0) {
				if (alt==1) alt=2; else alt=1;
				d = (GetTickCount()-rdgdcli->connected)/1000;
				if (rdgdcli->ecm.busy)
					snprintf( http_buf, sizeof(http_buf),"<tr class=alt%d><td>%s</td><td class=\"busy\">%02dd %02d:%02d:%02d</td>",alt,(char*)ip2string(rdgdcli->ip), d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
				else
					snprintf( http_buf, sizeof(http_buf),"<tr class=alt%d><td>%s</td><td class=\"online\">%02dd %02d:%02d:%02d</td>",alt,(char*)ip2string(rdgdcli->ip), d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

				sprintf( http_buf, "<td align=center>%d</td>", rdgdcli->ecmnb );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				int ecmaccepted = rdgdcli->ecmnb-rdgdcli->ecmdenied;
				tcp_writeecmdata(&tcpbuf, sock, ecmaccepted, rdgdcli->ecmnb);
				tcp_writeecmdata(&tcpbuf, sock, rdgdcli->ecmok, ecmaccepted);
				//Ecm Time
				if (rdgdcli->ecmok)
					sprintf( http_buf,"<td align=center>%d ms</td>",(rdgdcli->ecmoktime/rdgdcli->ecmok) );
				else
					sprintf( http_buf,"<td align=center>-- ms</td>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				//Last Used Share
				if ( rdgdcli->ecm.lastcaid ) {
					if (rdgdcli->ecm.laststatus) sprintf( http_buf,"<td class=success>"); else sprintf( http_buf,"<td class=failed>");
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					sprintf( http_buf,"ch %s (%dms) %s ", getchname(rdgdcli->ecm.lastcaid, rdgdcli->ecm.lastprov, rdgdcli->ecm.lastsid) , rdgdcli->ecm.lastdecodetime, str_laststatus[rdgdcli->ecm.laststatus] );
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					if ( (GetTickCount()-rdgdcli->ecm.recvtime) < 20000 ) {
						if (rdgdcli->ecm.lastdcwsrctype==DCW_SOURCE_SERVER) {
							struct server_data *srv = getsrvbyid(rdgdcli->ecm.lastdcwsrcid);
							if (srv) {
								sprintf( http_buf," / from server (%s:%d)", srv->host->name, srv->port);
								tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							}
						}
						else if (rdgdcli->ecm.lastdcwsrctype==DCW_SOURCE_CACHE) {
							struct cachepeer_data *peer = getpeerbyid(rdgdcli->ecm.lastdcwsrcid);
							if (peer) {
								sprintf( http_buf," / from cache peer (%s:%d)", peer->host->name, peer->port);
								tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							}
						}
					}
					sprintf( http_buf,"</td>");
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				else {
					sprintf( http_buf,"<td> </td>");
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				sprintf( http_buf,"</tr>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			rdgdcli = rdgdcli->next;
		}
		sprintf( http_buf, "</table>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
#endif

*/

	// Send Stat
	sprintf( http_buf, "<style type=\"text/css\">\n.mainborder\n{ background: #d2d2d2; border: 1px solid #0B198C; border-spacing: 0px; font: 10px verdana, geneva, lucida, 'lucida grande', arial, helvetica, sans-serif; padding: 1 2; }\n.redborder { background: #d25555; border-left: 1px solid #eee; border-right: 1px solid #eee; border-bottom: 1px solid #eee; border-spacing: 0px; font: 9px verdana, geneva, lucida, 'lucida grande', arial, helvetica, sans-serif; }\n.greenborder { background: #55d255; border-left: 1px solid #eee; border-right: 1px solid #eee; border-bottom: 1px solid #eee; border-spacing: 0px; font: 9px verdana, geneva, lucida, 'lucida grande', arial, helvetica, sans-serif; }\n.cacheborder { background: #5555e2; border-left: 1px solid #eee; border-right: 1px solid #eee; border-bottom: 1px solid #eee; border-spacing: 0px; font: 9px verdana, geneva, lucida, 'lucida grande', arial, helvetica, sans-serif; }\n</style>\n");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#ifdef SRV_CSCACHE
	sprintf( http_buf, "<style type=\"text/css\">\n.clientsborder { background: goldenrod; border-left: 1px solid #eee; border-right: 1px solid #eee; border-bottom: 1px solid #eee; border-spacing: 0px; font: 9px verdana, geneva, lucida, 'lucida grande', arial, helvetica, sans-serif; }\n</style>\n");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
#ifdef CACHEEX
	sprintf( http_buf, "<style type=\"text/css\">\n.cacheexborder { background: darkblue; border-left: 1px solid #eee; border-right: 1px solid #eee; border-bottom: 1px solid #eee; border-spacing: 0px; font: 9px verdana, geneva, lucida, 'lucida grande', arial, helvetica, sans-serif; }\n</style>\n");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
	if (cs->ecmok) {
		sprintf( http_buf, "\n<br><br><table class=\"mainborder\" width=100%%>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf, "<tr><td><div class=\"redborder\" style=\"height: 10px; width: 10px;\"></div> </td><td width=100%%>Total DCW number</td></tr><tr><td><div class=greenborder style=\"height: 10px; width: 10px;\"></div> </td><td width=100%%>Number of DCW from servers</td></tr><tr><td><div class=cacheborder style=\"height: 10px; width: 10px;\"></div> </td><td width=100%%>Number of DCW from Cache</td></tr><tr><td><div class=cacheexborder style=\"height: 10px; width: 10px;\"></div> </td><td width=100%%>Number of DCW from CacheEX</td></tr><tr><td><div class=clientsborder style=\"height: 10px; width: 10px;\"></div> </td><td width=100%%>Number of DCW from Newcamd/Mgcamd Clients</td></tr>\n"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//Get Max of ttime
		int max=1;
		int timeout = cs->option.dcw.timeout * (cs->option.dcw.retry+1);
		if (timeout>10000) timeout = 10000;
		for(i=0; i<(timeout/100); i++) if (max<cs->ttime[i]) max=cs->ttime[i];
		for(i=0; i<(timeout/100); i++) {
			sprintf( http_buf, "<tr><td>%d.%ds</td><td>", i/10,i%10 );
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			// RED
			int width = cs->ttime[i]*100/max;
			if (width>10)
				sprintf( http_buf, "<div class=redborder style='height:3px; width:%d%%'><span style=\"float: right;\">%d</span></div>", width, cs->ttime[i] );
			else {
				if (!width && cs->ttime[i])
					sprintf( http_buf, "<div class=redborder style='height:2px; width:1px;'></div>");
				else
					sprintf( http_buf, "<div class=redborder style='height:2px; width:%d%%'></div>", width );
			}
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			// GREEN
			width = cs->ttimecards[i]*100/max;
			if ( !width && cs->ttimecards[i] )
				sprintf( http_buf, "<div class=greenborder style='height:2px; width:1px;'></div>");
			else
				sprintf( http_buf, "<div class=greenborder style='height:2px; width:%d%%'></div>", width );
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			// CACHE - BLUE
			width = cs->ttimecache[i]*100/max;
			if ( !width && cs->ttimecache[i] )
				sprintf( http_buf, "<div class=cacheborder style='height:2px; width:1px;'></div>");
			else
				sprintf( http_buf, "<div class=cacheborder style='height:2px; width:%d%%'></div>", width );
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			// CACHEEX
			width = cs->ttimecacheex[i]*100/max;
			if ( !width && cs->ttimecacheex[i] )
				sprintf( http_buf, "<div class=cacheexborder style='height:2px; width:1px;'></div>");
			else
				sprintf( http_buf, "<div class=cacheexborder style='height:2px; width:%d%%'></div>", width );
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#ifdef SRV_CSCACHE
			// YELLOW
			width = cs->ttimeclients[i]*100/max;
			if ( !width && cs->ttimeclients[i] )
				sprintf( http_buf, "<div class=clientsborder style='height:2px; width:1px;'></div>");
			else
				sprintf( http_buf, "<div class=clientsborder style='height:2px; width:%d%%'></div>", width );
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
			//
			sprintf( http_buf, "</td></tr>");
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		sprintf( http_buf, "</table><br>\n"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}

#ifndef PUBLIC
	// Runtime SIDS
	if (cs->deniedsids[0].sid) {
		sprintf( http_buf, "<br><b>Available Servers</b>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		int maxcards = 0;
		for(i=0; i<1024; i++) {
			if (cs->deniedsids[i].sid) {
				if (cs->deniedsids[i].nbsrv>maxcards) maxcards = cs->deniedsids[i].nbsrv;
			}
			else break;
		}

		int icard = 0;
		while(icard<=maxcards) {
			sprintf( http_buf, "<br>%d cards: ",icard);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			for(i=0; i<1024; i++)
				if (cs->deniedsids[i].sid) {
					if (cs->deniedsids[i].nbsrv==icard) {
						sprintf( http_buf, "%04x ", cs->deniedsids[i].sid);
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
				}
				else break;
			icard++;
		}
	}

	if (cs->sidlist.data) {
		sprintf( http_buf, "<br><table> <tr><th>SID:CHID:ECMLEN.CW1CYCLE</th> <th>ECMNB</td> <th>ECMOK</th> </tr>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		int i;
		struct sid_chid_ecmlen_data *sids = cs->sidlist.data;
		for(i=0;i<MAX_SIDS;i++,sids++) {
			if (!sids->sid) break;
			sprintf( http_buf, "<tr> <td>%04x:%04x:%04x.%02x</td> <td>%d</td> <td>%d</td> </tr>", sids->sid, sids->chid, sids->ecmlen, sids->cw1cycle, sids->ecmnb, sids->ecmok );
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}

		sprintf( http_buf, "</table>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		// show sid file
		if (cs->card.nbprov==1) {
			struct sid_chid_ecmlen_data *sids = cs->sidlist.data;
			for(i=0;i<MAX_SIDS;i++,sids++) {
				if (!sids->sid) break;
				struct chninfo_data *chn = getchninfo(cs->card.caid, cs->card.prov[0].id, sids->sid);
				if (!chn) continue;
				if (sids->cw1cycle)
					sprintf( http_buf, "%04x:%06x:%04x.%02x \"%s\"<br>", cs->card.caid, cs->card.prov[0].id, sids->sid, sids->cw1cycle, chn->name );
				else
					sprintf( http_buf, "%04x:%06x:%04x \"%s\"<br>", cs->card.caid, cs->card.prov[0].id, sids->sid, chn->name );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
	}

	for(i=0; i<cs->card.nbprov; i++) {
		if (cs->card.prov[i].sidlist.data) {
			sprintf( http_buf, "<br><br>SIDLIST FOR PROVIDER: %06x<br><table> <tr><th>SID:CHID:ECMLEN.CW1CYCLE</th> <th>ECMNB</td> <th>ECMOK</th> </tr>",cs->card.prov[i].id);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			struct sid_chid_ecmlen_data *sids = cs->card.prov[i].sidlist.data;
			int j;
			for(j=0;j<MAX_SIDS;j++,sids++) {
				if (j>=cs->card.prov[i].sidlist.total) break;
				if (!sids->sid) break;
				sprintf( http_buf, "<tr> <td>%04x:%04x:%04x.%02x</td> <td>%d</td> <td>%d</td> </tr>", sids->sid, sids->chid, sids->ecmlen, sids->cw1cycle, sids->ecmnb, sids->ecmok );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			sprintf( http_buf, "</table>");
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

			tcp_writestr(&tcpbuf, sock, "<pre style='width:80%;height:50%; border:1 solid #777'>");
			sids = cs->card.prov[i].sidlist.data;
			for(i=0;i<MAX_SIDS;i++,sids++) {
				if (!sids->sid) break;
				struct chninfo_data *chn = getchninfo(cs->card.caid, cs->card.prov[0].id, sids->sid);
				if (!chn) continue;
				if (sids->cw1cycle)
					sprintf( http_buf, "%04x:%06x:%04x.%02x \"%s\"\n", cs->card.caid, cs->card.prov[0].id, sids->sid, sids->cw1cycle, chn->name );
				else
					sprintf( http_buf, "%04x:%06x:%04x \"%s\"\n", cs->card.caid, cs->card.prov[0].id, sids->sid, chn->name );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			tcp_writestr(&tcpbuf, sock, "</pre>");
		}
	}

#endif

	tcp_flush(&tcpbuf, sock);
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#ifdef CCCAM_SRV

void cccam_clients( struct cccam_server_data *cccam, int *total, int *connected, int *active )
{
	*total = 0;
	*connected = 0;
	*active = 0;
	struct cc_client_data *cli = cccam->client;
	while (cli) {
		(*total)++;
		if (cli->connection.status>0) {
			(*connected)++;
			if ( (GetTickCount()-cli->lastecmtime) < 20000 ) (*active)++;
		}
		cli=cli->next;
	}
}

void total_cccam_clients( struct config_data *cfg, int *total, int *connected, int *active )
{
	*total = 0;
	*connected = 0;
	*active = 0;
	struct cccam_server_data *cccam = cfg->cccam.server;
	while (cccam) {
		struct cc_client_data *cli = cccam->client;
		while (cli) {
			(*total)++;
			if (cli->connection.status>0) {
				(*connected)++;
				if ( (GetTickCount()-cli->lastecmtime) < 20000 ) (*active)++;
			}
			cli=cli->next;
		}
		cccam = cccam->next;
	}
}

void getcccamcells(struct cc_client_data *cli, char cell[10][2048])
{
	char temp[2048];
	unsigned int ticks = GetTickCount();
	unsigned int d;
	// CELL0 # NAME
	if (cli->realname)
		sprintf( cell[0],"<a href='/cccamclient?id=%d'>%s<br>%s</a>",cli->id,cli->user,cli->realname);
	else
		sprintf( cell[0],"<a href='/cccamclient?id=%d'>%s</a>",cli->id,cli->user);
	// CELL1 # VERSION
	if (strlen(cli->version)) sprintf( cell[1],"CCcam %s<br>%02x%02x%02x%02x%02x%02x%02x%02x", cli->version, cli->nodeid[0],cli->nodeid[1],cli->nodeid[2],cli->nodeid[3],cli->nodeid[4],cli->nodeid[5],cli->nodeid[6],cli->nodeid[7]);
	else strcpy( cell[1]," " ); 
	// CELL2 # IP
	char *p = getcountrycodebyip(cli->ip);
	if (cli->host)
		if (p) sprintf( cell[2],"<img src='/flag_%s.gif' title='%s'> %s<br>%s", p, getcountryname(p), (char*)ip2string(cli->ip), cli->host->name ); else sprintf( cell[2],"%s<br>%s",(char*)ip2string(cli->ip), cli->host->name );
	else
		if (p) sprintf( cell[2],"<img src='/flag_%s.gif' title='%s'> %s", p, getcountryname(p), (char*)ip2string(cli->ip) ); else sprintf( cell[2],"%s",(char*)ip2string(cli->ip) );
	// CELL3 # Connection Time
	if (cli->connection.status>0) {
		if (cli->ecm.busy) sprintf( cell[9],"busy"); else sprintf( cell[9],"online");
		d = (ticks-cli->connection.time)/1000;
		sprintf( cell[3], "%02dd %02d:%02d:%02d", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
	}
	else {
		strcpy( cell[9], "offline" );
		if (cli->flags&FLAG_DELETE) sprintf( cell[3],"Removed");
		else if (cli->flags&FLAG_EXPIRED) sprintf( cell[3],"Expired");
		else if (cli->flags&FLAG_DISABLE) sprintf( cell[3],"Disabled");
		else sprintf( cell[3],"offline");
	}
#ifdef EXPIREDATE
	if (cli->enddate.tm_year) {
		sprintf( temp,"<br>Expire: %d-%02d-%02d", 1900+cli->enddate.tm_year, cli->enddate.tm_mon+1, cli->enddate.tm_mday);
		strcat( cell[3], temp );
	}
#endif
	sprintf( temp, "<table class=\"connect_data\"><tr><td>Successful Login: %d</td><td>Aborted Connections: %d</td><td>Total Zapping: %d</td><td>Channel Freeze: %d</td></tr></table>", cli->nblogin, cli->nbloginerror, cli->zap, cli->freeze );
	strcat( cell[3], temp );

	// CELL4+5+6 # ECM STAT: TOTAL/ACCEPTED/OK
	sprintf( cell[4], "%d", cli->ecmnb);
	int ecmaccepted = cli->ecmnb-cli->ecmdenied;
	getstatcell( ecmaccepted, cli->ecmnb, cell[5]);
	getstatcell( cli->ecmok, ecmaccepted, cell[6]);
	// CELL7 # Ecm Time
	if (cli->ecmok) sprintf( cell[7],"%d ms",(cli->ecmoktime/cli->ecmok) ); else sprintf( cell[7],"-- ms");
	// CELL8 # Last Used Share
	if ( cli->connection.status<=0 && cli->connection.lastseen) {
		d = (ticks-cli->connection.lastseen)/1000;
		sprintf( cell[8],"Last Seen %02dd %02d:%02d:%02d", d/(3600*24),(d/3600)%24,(d/60)%60,d%60);
	}
	else if ( cli->lastecm.caid ) {
		if (cli->lastecm.status)  strcpy( cell[8],"<span class=success"); else strcpy( cell[8],"<span class=failed");
		sprintf( temp," title='%04x:%06x:%04x'>ch %s (%dms) %s ",cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid, getchname(cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid) , cli->lastecm.decodetime, str_laststatus[cli->lastecm.status] );
		strcat( cell[8], temp );
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->lastecm.status) {
				strcat( cell[8], " / from ");
				src2string(cli->lastecm.dcwsrctype, cli->lastecm.dcwsrcid, temp);
				strcat( cell[8], temp);
			}
		}
		strcat( cell[8], "</span>" );
	}
	else strcpy( cell[8], " ");
	strcat( cell[8], "<br><span style='display:inline-flex;gap:2px;white-space:nowrap;margin-top:4px;'>");
	if ( !(cli->flags&(FLAG_DELETE|FLAG_EXPIRED)) ) {
		if (cli->flags&FLAG_DISABLE) {
			sprintf( temp," <span class='icobtn on' title='Enable' onclick=\"imgrequest('/cccamclient?action=enable&id=%d',this);setTimeout('updateDiv()',600)\">ON</span>",cli->id);
			strcat( cell[8], temp );
		}
		else {
			sprintf( temp," <span class='icobtn off' title='Disable' onclick=\"imgrequest('/cccamclient?action=disable&id=%d',this);setTimeout('updateDiv()',600)\">OFF</span>",cli->id);
			strcat( cell[8], temp );
		}
	}
	sprintf( temp," <span class='icobtn dbg' title='Debug' onclick=\"imgrequest('/cccamclient?action=debug&id=%d',this)\">DBG</span>",cli->id);
	strcat( cell[8], temp );
	strcat( cell[8], "</span>");
}

int total_cccam_servers()
{
	int count=0;
	struct cccam_server_data *srv = cfg.cccam.server;
	while (srv) {
		count++;
		srv = srv->next;
	}
	return count;
}	


void http_send_cccam(int sock, http_request *req)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;
	char cell[10][2048];

	// Get Params
	char *str_action = isset_get( req, "action");
	char *str_list = isset_get( req, "list");
	char *str_id = isset_get( req, "id"); // CCcam server ID
	char *str_clid = isset_get( req, "clid"); // Client ID
#ifndef PUBLIC
	char *str_clname = isset_get( req, "clname"); // Client NAME
#endif
	// Param 'action'
	int get_action;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = ACTION_DIV;
		else if (!strcmp(str_action,"row")) get_action = ACTION_ROW;
		else if (!strcmp(str_action,"xml")) get_action = ACTION_XML; // Get Clients info in xml
        else if (!strcmp(str_action,"json")) get_action = ACTION_JSON; // Get Clients info in xml
		else if (!strcmp(str_action,"disable")) get_action = ACTION_DISABLE;
		else if (!strcmp(str_action,"enable")) get_action = ACTION_ENABLE;
		else if (!strcmp(str_action,"status")) get_action = ACTION_STATUS;
		else if (!strcmp(str_action,"debug")) get_action = ACTION_DEBUG;
		else str_action = NULL;
	}
	if (!str_action) { str_action = "page"; get_action = ACTION_PAGE; }

	/////////////////////////////////////////////

	if (get_action==ACTION_ROW) {
		// Check for XML ROW
		struct cc_client_data *cli = NULL;
		if (str_clid) {
			cli = getcccamclientbyid( atoi(str_clid) );
			if (!cli) return;
		}

		else {
			if (str_id && str_clname) {
				struct cccam_server_data *cccam = getcccamserverbyid( atoi(str_id) );
				if (!cccam) return;
				cli = getcccamclientbyname( cccam, str_clname );
				if (!cli) return;
			}
			else return;
		}

		// Send XML CELLS
		getcccamcells(cli,cell);
		int i; for(i=0; i<10; i++) xmlescape( cell[i] );
		char buf[5000] = "";
		snprintf( buf, sizeof(buf), "<cccam>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2>%s</c2>\n<c3_c>%s</c3_c>\n<c3>%s</c3>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6>\n<c7>%s</c7>\n<c8>%s</c8>\n</cccam>\n",cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8] );
		http_send_xml( sock, req, buf, strlen(buf));
		return;
	}			
	else if (get_action==ACTION_XML) {
		struct cccam_server_data *cccam = NULL;
		if (str_id) cccam = getcccamserverbyid( atoi(str_id) );
		tcp_init(&tcpbuf);
		tcp_writestr(&tcpbuf, sock, "HTTP/1.1 200 OK\r\nAccept-Ranges: bytes\r\nConnection: close\r\nContent-Type: application/xml\r\n\r\n");

		tcp_writestr(&tcpbuf, sock, "<multics>");

		struct cccam_server_data *srv;
		if (cccam) srv = cccam; else srv = cfg.cccam.server;
		while (srv) {
			tcp_writestr(&tcpbuf, sock, "\n<cccam>");
			sprintf(http_buf, "<id>%d</id>", srv->id); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf(http_buf, "<port>%d</port>", srv->port); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf(http_buf, "<status>%d</status>", (srv->handle>0) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			uint32_t ticks = GetTickCount();
			struct cc_client_data *cli = srv->client;
			while (cli) {
				tcp_writestr(&tcpbuf, sock, "<user>");
				sprintf(http_buf, "<name>%s</name>", cli->user); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				if (cli->connection.status>0) {
					tcp_writestr(&tcpbuf, sock, "<status>1</status>");
					sprintf( http_buf,"<ip>%s</ip>", (char*)ip2string(cli->ip) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					char *p = getcountrycodebyip(cli->ip);
					if (p) sprintf(http_buf, "<country>%s</country>", p); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					uint32_t d = (ticks - cli->connection.time)/1000;
					sprintf(http_buf, "<connected>%02dd %02d:%02d:%02d</connected>", d/(3600*24), (d/3600)%24, (d/60)%60, d%60); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				else {
					sprintf(http_buf, "<status>%d</status>",cli->flags&0x0E);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				tcp_writestr(&tcpbuf, sock, "</user>");
				cli = cli->next;
			}
			tcp_writestr(&tcpbuf, sock, "\n</cccam>");

			if (cccam) break; else srv = srv->next;
		}
		tcp_writestr(&tcpbuf, sock, "\n</multics>");
		tcp_flush(&tcpbuf, sock);
		return;
	}
    else if (get_action==ACTION_JSON) {
		struct cccam_server_data *cccam = NULL;
		if (str_id) cccam = getcccamserverbyid( atoi(str_id) );
		tcp_init(&tcpbuf);
		tcp_writestr(&tcpbuf, sock, "HTTP/1.1 200 OK\r\nAccept-Ranges: bytes\r\nConnection: close\r\nContent-Type: application/json\r\n\r\n");

		tcp_writestr(&tcpbuf, sock, "[");

		struct cccam_server_data *srv;
		if (cccam) srv = cccam; else srv = cfg.cccam.server;
		while (srv) {
			tcp_writestr(&tcpbuf, sock, "{");
			sprintf(http_buf, "\"id\": %d,", srv->id); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf(http_buf, "\"port\": %d,", srv->port); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf(http_buf, "\"status\": %d,", (srv->handle>0) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			tcp_writestr(&tcpbuf, sock, "\"users\": [");
            uint32_t ticks = GetTickCount();
			struct cc_client_data *cli = srv->client;
			while (cli) {
				sprintf(http_buf, "{\"name\": \"%s\",", cli->user); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				if (cli->connection.status>0) {
					tcp_writestr(&tcpbuf, sock, "\"status\": 1,");
					sprintf( http_buf,"\"ip\": \"%s\",", (char*)ip2string(cli->ip) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					char *p = getcountrycodebyip(cli->ip);
					if (p) sprintf(http_buf, "\"country\": \"%s\",", p); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					uint32_t d = (ticks - cli->connection.time)/1000;
                sprintf(http_buf, "\"connected\": %d}", d); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				else {
                sprintf(http_buf, "\"status\": %d}",cli->flags&0x0E);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
                cli = cli->next;
                if (cli) tcp_writestr(&tcpbuf, sock, ",");
				
			}
            tcp_writestr(&tcpbuf, sock, "]");

			if (cccam) break; else srv = srv->next;
		}
    tcp_writestr(&tcpbuf, sock, "}]");
		tcp_flush(&tcpbuf, sock);
		return;
	}
    
	// Param 'id'
	int get_id = 0;
	if (str_id)	get_id = atoi(str_id);
	// Param 'list'
	int get_list = LIST_ALL;
	if (str_list) {
		if (!strcmp(str_list,"connected")) get_list = LIST_CONNECTED;
		else if (!strcmp(str_list,"all")) get_list = LIST_ALL;
		else str_list = NULL;
	}
	if (!str_list) str_list = "all";
	//
	struct cccam_server_data *cccam = NULL;
	if (get_id) {
		cccam = getcccamserverbyid(get_id);
		if (!cccam) return;
	}

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	if (get_action==ACTION_PAGE) {
		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, html_title, cfg.http.title, "CCcam"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		// JS
        tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
		tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
		// ACTIONS REQUEST
		tcp_writestr(&tcpbuf, sock, "\nfunction imgrequest( url, el )\n{\n	var httpRequest;\n	try { httpRequest = new XMLHttpRequest(); }\n	catch (trymicrosoft) { try { httpRequest = new ActiveXObject('Msxml2.XMLHTTP'); } catch (oldermicrosoft) { try { httpRequest = new ActiveXObject('Microsoft.XMLHTTP'); } catch(failed) { httpRequest = false; } } }\n	if (!httpRequest) { alert('Your browser does not support Ajax.'); return false; }\n	if ( typeof(el)!='undefined' ) {\n		el.onclick = null;\n		el.style.opacity = '0.7';\n		httpRequest.onreadystatechange = function()\n		{\n			if (httpRequest.readyState == 4) if (httpRequest.status == 200) el.style.opacity = '0.3';\n		}\n	}\n	httpRequest.open('GET', url, true);\n	httpRequest.send(null);\n}\n");
		// UPD ROW
		tcp_writestr(&tcpbuf, sock, "\nfunction xmlupdateRow( xmlDoc, id )\n{\n	var row = document.getElementById(id);\n	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n	row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n	row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n	row.cells.item(3).className = xmlDoc.getElementsByTagName('c3_c')[0].childNodes[0].nodeValue;\n	row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n	row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n	row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n	row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n	row.cells.item(7).innerHTML = xmlDoc.getElementsByTagName('c7')[0].childNodes[0].nodeValue;\n	row.cells.item(8).innerHTML = xmlDoc.getElementsByTagName('c8')[0].childNodes[0].nodeValue;\n}\n");
		char url[256];
		sprintf( url, "'/cccam?action=row&clid='+idx");
		sprintf( http_buf, HTTP_UPDATE_ROW, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// UPD DIV
		sprintf( url, "/cccam?action=div&id=%d&list=%s", get_id, str_list);
		sprintf( http_buf, HTTP_UPDATE_DIV, cfg.http.autorefresh*1000, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	setautorefresh(autorefresh);\n}");
		tcp_writestr(&tcpbuf, sock, "\n</script>\n");
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
		tcp_write_menu(&tcpbuf, sock,PAGE_CCCAM);
		// Info de servidores (acima da div principal)
		{
			tcp_writestr(&tcpbuf, sock, "<div style='margin:12px 12px 0 12px'><div class=stat-section style='margin:0'>");
			sprintf( http_buf, "<h3 class=stitle>CCcam Servers (%d)</h3>", total_cccam_servers());
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			tcp_writestr(&tcpbuf, sock, "<table class=maintable><tr><th>Server</th><th>Port</th><th>Status</th><th>Connected</th></tr>");
			int itotal = total_cc_clients();
			int iconnected = connected_cc_clients();
			sprintf( http_buf, "<tr><td class=left>TOTAL</td><td class=right>-</td><td class=right>-</td><td class=right>%d / %d</td></tr>", iconnected, itotal);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf( http_buf, "<tr><td class=left>NodeID</td><td class=right colspan=3>%02x%02x%02x%02x%02x%02x%02x%02x</td></tr>", cfg.nodeid[0], cfg.nodeid[1], cfg.nodeid[2], cfg.nodeid[3], cfg.nodeid[4], cfg.nodeid[5], cfg.nodeid[6], cfg.nodeid[7]);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf( http_buf, "<tr><td class=left>Version</td><td class=right colspan=3>%s</td></tr>", cfg.cccam.version);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			struct cccam_server_data *box = cfg.cccam.server;
			while ( box ) {
				int btotal, bconnected, bactive;
				cccam_clients( box, &btotal, &bconnected, &bactive );
				if (box->handle>0) sprintf( http_buf, "<tr><td class=left><a href='/cccam?id=%d'>CCcam %d</a></td><td class=right>%d</td><td class=right><span class=success>ONLINE</span></td><td class=right>%d / %d</td></tr>", box->id, box->id, box->port, bconnected, btotal);
				else sprintf( http_buf, "<tr><td class=left><a href='/cccam?id=%d'>CCcam %d</a></td><td class=right>%d</td><td class=right><span class=failed>OFFLINE</span></td><td class=right>%d / %d</td></tr>", box->id, box->id, box->port, bconnected, btotal);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				box = box->next;
			}
			tcp_writestr(&tcpbuf, sock, "</table></div></div>");
		}
		// DIV
		tcp_writestr(&tcpbuf, sock, "\n<div id='mainDiv'>");
	}

	tcp_writestr(&tcpbuf, sock, "<select style=\"width:200px;\" onchange=\"parent.location.href='/cccam?id='+this.value\">");
	sprintf( http_buf, "<option value=0>ALL (%d)</option>", total_cccam_servers());
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	struct cccam_server_data *tmp = cfg.cccam.server;
	while (tmp) {
		if (get_id==tmp->id) sprintf( http_buf, "<option value=%d selected>[%d] CCcam %d</option>",tmp->id,tmp->port, tmp->id );
		else sprintf( http_buf, "<option value=%d>[%d] CCcam %d</option>",tmp->id,tmp->port, tmp->id );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tmp = tmp->next;
	}
	tcp_writestr(&tcpbuf, sock, "</select> ");
	//
	int total, connected, active;
	if (cccam) cccam_clients( cccam, &total, &connected, &active ); else total_cccam_clients( &cfg, &total, &connected, &active );
	char *class1 = "button"; char *class2 = "sbutton";
	char *class;
	if (get_list==LIST_ACTIVE) class = class2; else class = class1;
	sprintf( http_buf, "<input type=button class=%s onclick=\"parent.location='/cccam?id=%d&amp;list=active'\" value='Active Clients (%d)'>", class, get_id, active);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (get_list==LIST_CONNECTED) class = class2; else class = class1;
	sprintf( http_buf, " <input type=button class=%s onclick=\"parent.location='/cccam?id=%d&amp;list=connected'\" value='Connected Clients (%d)'>", class, get_id, connected);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (get_list==LIST_ALL) class = class2; else class = class1;
	sprintf( http_buf, " <input type=button class=%s onclick=\"parent.location='/cccam?id=%d&amp;list=all'\" value='All Clients (%d)'>", class, get_id, total);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//
	if (get_id) { // One Server Selected
		// Table
		sprintf( http_buf, "\n<table class=maintable width=100%%><tr><th width=100px>Client</th><th width=70px>version</th><th width=120px>ip</th><th width=110px>Connected</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		struct cc_client_data *cli = cccam->client;
		int alt=0;
		if (get_list==LIST_ACTIVE) {
			while (cli) {
				if ( (cli->connection.status>0)&&((GetTickCount()-cli->lastecmtime) < 20000) ) {
					if (alt==1) alt=2; else alt=1;
					getcccamcells(cli,cell);
					snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				cli = cli->next;
			}
		}
		else if (get_list==LIST_CONNECTED) {
			while (cli) {
				if (cli->connection.status>0) {
					if (alt==1) alt=2; else alt=1;
					getcccamcells(cli,cell);
					snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				cli = cli->next;
			}
		}
		else { // ALL
			while (cli) {
				if (alt==1) alt=2; else alt=1;
				getcccamcells(cli,cell);
				snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				cli = cli->next;
			}
		}
		sprintf( http_buf, "\n</table>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}

	else {
		// Table
		tcp_writestr(&tcpbuf,sock, "\n<table class=maintable width=100%>");
		tcp_writestr(&tcpbuf,sock, "\n<tr><th width=100px>Client</th><th width=70px>version</th><th width=120px>ip</th><th width=110px>Connected</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>");
		int alt=0;
		cccam = cfg.cccam.server;
		while (cccam) {
			int total, connected, active;
			cccam_clients( cccam, &total, &connected, &active );
			if ( (get_list==LIST_ACTIVE) && active ) {
				snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=9> CCcam %d (%d)</td></tr>", cccam->id, active); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				struct cc_client_data *cli = cccam->client;
				while (cli) {
					if ( (cli->connection.status>0)&&((GetTickCount()-cli->lastecmtime) < 20000) ) {
						if (alt==1) alt=2; else alt=1;
						getcccamcells(cli,cell);
						snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
					cli = cli->next;
				}
			}
			else if ( (get_list==LIST_ALL) && total ) {
				snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=9> CCcam %d (%d)</td></tr>", cccam->id, total); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				struct cc_client_data *cli = cccam->client;
				while (cli) {
					if (alt==1) alt=2; else alt=1;
					getcccamcells(cli,cell);
					snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					cli = cli->next;
				}
			}
			else if ( (get_list==LIST_CONNECTED) && connected ) {
				snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=9> CCcam %d (%d)</td></tr>", cccam->id, connected); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				struct cc_client_data *cli = cccam->client;
				while (cli) {
					if (cli->connection.status>0) {
						if (alt==1) alt=2; else alt=1;
						getcccamcells(cli,cell);
						snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
					cli = cli->next;
				}
			}
			cccam = cccam->next;
		}
		sprintf( http_buf, "</table>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	if (get_action==ACTION_PAGE) {
		tcp_writestr(&tcpbuf, sock, "</div>");
		tcp_writestr(&tcpbuf, sock, "</body></html>");
	}

	tcp_flush(&tcpbuf, sock);
}



#ifdef CS378X_SRV

void getcs378xcells(struct camd35_client_data *cli, char cell[10][2048])
{
	char temp[2048];

	// CELL0 # NAME
	sprintf( cell[0],"<a href='/cs378xclient?id=%d'>%s</a>",cli->id,cli->user);

	// CELL1 # IP
	if ( cli->ip ) { // Get Last IP
		char *p = getcountrycodebyip(cli->ip);
		if (p) sprintf( cell[1],"<img src='/flag_%s.gif' title='%s'> %s", p, getcountryname(p), (char*)ip2string(cli->ip) ); else sprintf( cell[1],"%s",(char*)ip2string(cli->ip) );
	}
	else strcpy( cell[1], " ");

	// CELL2 # Connection Time
	if (cli->connection.status>0) {
		if (cli->ecm.busy) sprintf( cell[9],"busy"); else sprintf( cell[9],"online");
		uint32_t d = (GetTickCount()-cli->connection.time)/1000;
		sprintf( cell[2], "%02dd %02d:%02d:%02d", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
	}
	else {
		sprintf( cell[9],"offline");
		if (cli->flags&FLAG_DELETE) sprintf( cell[2],"Removed");
		else if (cli->flags&FLAG_EXPIRED) sprintf( cell[2],"Expired");
		else if (cli->flags&FLAG_DISABLE) sprintf( cell[2],"Disabled");
		else sprintf( cell[2],"offline");
	}
	// CELL3+4+5 # ECM STAT: TOTAL/ACCEPTED/OK
	// ECM STAT
	sprintf( cell[3], "%d", cli->ecmnb );

	int ecmaccepted = cli->ecmnb-cli->ecmdenied;
	getstatcell( ecmaccepted, cli->ecmnb, cell[4]);
	getstatcell( cli->ecmok, ecmaccepted, cell[5]);

	// CELL6 # Ecm Time
	if (cli->ecmok) sprintf( cell[6],"%d ms",(cli->ecmoktime/cli->ecmok) ); else sprintf( cell[6],"-- ms");

	// CELL7 # Last Used Share
/*
	if ( srv->connection.status<=0 && srv->connection.lastseen) {
		int d = (GetTickCount()-cli->connection.lastseen)/1000;
		sprintf( cell[7],"Last Seen %02dd %02d:%02d:%02d", d/(3600*24),(d/3600)%24,(d/60)%60,d%60);
	}
	else
*/
	if ( cli->lastecm.caid ) {
		if (cli->lastecm.status)  strcpy( cell[7],"<span class=success"); else strcpy( cell[7],"<span class=failed");
		sprintf( temp," title='%04x:%06x:%04x'>ch %s (%dms) %s ",cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid, getchname(cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid) , cli->lastecm.decodetime, str_laststatus[cli->lastecm.status] );
		strcat( cell[7], temp );
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->lastecm.status) {
				strcat( cell[7], " / from ");
				src2string(cli->lastecm.dcwsrctype, cli->lastecm.dcwsrcid, temp);
				strcat( cell[7], temp);
			}
		}
		strcat( cell[7], "</span>" );
	}
	else strcpy( cell[7], " ");

	strcat( cell[7], "<br><span style='display:inline-flex;gap:2px;white-space:nowrap;margin-top:4px;'>");
	if ( !(cli->flags&(FLAG_DELETE|FLAG_EXPIRED)) ) {
		if (cli->flags&FLAG_DISABLE) {
			sprintf( temp," <span class='icobtn on' title='Enable' onclick=\"imgrequest('/cs378xclient?id=%d&action=enable',this);setTimeout('updateDiv()',600)\">ON</span>",cli->id);
			strcat( cell[7], temp );
		}
		else {
			sprintf( temp," <span class='icobtn off' title='Disable' onclick=\"imgrequest('/cs378xclient?id=%d&action=disable',this);setTimeout('updateDiv()',600)\">OFF</span>",cli->id);
			strcat( cell[7], temp );
		}
	}
	sprintf( temp," <span class='icobtn dbg' title='Debug' onclick=\"toggleDbgRow(%d,'/cs378xclient?id=%d&action=dbginfo')\">DBG</span>",cli->id,cli->id);
	strcat( cell[7], temp );
	strcat( cell[7], "</span>");
}

void total_cs378x_clients( int *total, int *connected, int *active )
{
	*total = 0;
	*connected = 0;
	*active = 0;
	struct camd35_server_data *cs378x = cfg.cs378x.server;
	while (cs378x) {
		struct camd35_client_data *cli = cs378x->client;
		while (cli) {
			(*total)++;
			if (cli->connection.status>0) {
				(*connected)++;
				if ( (GetTickCount()-cli->lastecmtime) < 20000 ) (*active)++;
			}
			cli=cli->next;
		}
		cs378x = cs378x->next;
	}
}

void cs378x_clients( struct camd35_server_data *cs378x, int *total, int *connected, int *active )
{
	*total = 0;
	*connected = 0;
	*active = 0;
	struct camd35_client_data *cli = cs378x->client;
	while (cli) {
		(*total)++;
		if (cli->connection.status>0) {
			(*connected)++;
			if ( (GetTickCount()-cli->lastecmtime) < 20000 ) (*active)++;
		}
		cli=cli->next;
	}
}

void http_send_cs378x(int sock, http_request *req)
{
	char http_buf[4096];
	struct tcp_buffer_data tcpbuf;
	char cell[10][2048];

	// Get Params
	char *str_action = isset_get( req, "action");
	char *str_list = isset_get( req, "list");
	char *str_id = isset_get( req, "id"); // server ID
	char *str_clid = isset_get( req, "clid"); // Client ID
	// Param 'action'
	int get_action;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = ACTION_DIV;
		else if (!strcmp(str_action,"row")) get_action = ACTION_ROW;
#ifndef PUBLIC
		else if (!strcmp(str_action,"xml")) get_action = ACTION_XML; // Get Clients info in xml
#endif
		else if (!strcmp(str_action,"disable")) get_action = ACTION_DISABLE;
		else if (!strcmp(str_action,"enable")) get_action = ACTION_ENABLE;
		else if (!strcmp(str_action,"status")) get_action = ACTION_STATUS;
		else if (!strcmp(str_action,"debug")) get_action = ACTION_DEBUG;
		else str_action = NULL;
	}
	if (!str_action) { str_action = "page"; get_action = ACTION_PAGE; }
	/////////////////////////////////////////////
	if (get_action==ACTION_ROW) {
		// Check for XML ROW
		if (str_clid) {
			int id = atoi(str_clid);
			struct camd35_server_data *cs378x = cfg.cs378x.server;
			while (cs378x) {
				if (!(cs378x->flags&FLAG_DELETE)) {
					struct camd35_client_data *cli = cs378x->client;
					while (cli) {
						if ( !(cli->flags&FLAG_DELETE) && (cli->id==id) ) {
							// Send XML CELLS
							getcs378xcells(cli,cell);
							int i; for(i=0; i<10; i++) xmlescape( cell[i] );
							sprintf( http_buf, "<cs378x>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2_c>%s</c2_c>\n<c2>%s</c2>\n<c3>%s</c3>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6>\n<c7>%s</c7>\n</cs378x>\n",cell[0],cell[1],cell[9],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7] );
							http_send_xml( sock, req, http_buf, strlen(http_buf));
						}
						cli = cli->next;
					}
				}
				cs378x = cs378x->next;
			}
		}
		return;
	}			

	// Param 'list'
	int get_list = LIST_ALL;
	if (str_list) {
		if (!strcmp(str_list,"connected")) get_list = LIST_CONNECTED;
		else if (!strcmp(str_list,"all")) get_list = LIST_ALL;
		else str_list = NULL;
	}
	if (!str_list) str_list = "all";
	// Param 'id'
	int get_id = 0;
	struct camd35_server_data *cs378x = NULL;
	if (str_id)	{
		get_id = atoi(str_id);
		cs378x = cfg.cs378x.server;
		while (cs378x) {
			if (cs378x->id == get_id) break;
			cs378x = cs378x->next;
		}
		if (!cs378x) get_id = 0;
	}
	//
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) ); // header tambem no div (XHR exige status line)
	if (get_action==ACTION_PAGE) {

		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, html_title, cfg.http.title, "cs378x"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		// JS
        tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
		tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
		// ACTIONS REQUEST
		tcp_writestr(&tcpbuf, sock, "\nfunction imgrequest( url, el )\n{\n	var httpRequest;\n	try { httpRequest = new XMLHttpRequest(); }\n	catch (trymicrosoft) { try { httpRequest = new ActiveXObject('Msxml2.XMLHTTP'); } catch (oldermicrosoft) { try { httpRequest = new ActiveXObject('Microsoft.XMLHTTP'); } catch(failed) { httpRequest = false; } } }\n	if (!httpRequest) { alert('Your browser does not support Ajax.'); return false; }\n	if ( typeof(el)!='undefined' ) {\n		el.onclick = null;\n		el.style.opacity = '0.7';\n		httpRequest.onreadystatechange = function()\n		{\n			if (httpRequest.readyState == 4) if (httpRequest.status == 200) el.style.opacity = '0.3';\n		}\n	}\n	httpRequest.open('GET', url, true);\n	httpRequest.send(null);\n}\n");
		// UPD ROW
		tcp_writestr(&tcpbuf, sock, "\nfunction xmlupdateRow( xmlDoc, id ) \n{\n    var row = document.getElementById(id);\n    	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n    row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n    row.cells.item(2).className = xmlDoc.getElementsByTagName('c2_c')[0].childNodes[0].nodeValue;\n    row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n    row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n    row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n    row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n    row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n    row.cells.item(7).innerHTML = xmlDoc.getElementsByTagName('c7')[0].childNodes[0].nodeValue;\n}");
		char url[256];
		sprintf( url, "'/cs378x?action=row&clid='+idx");
		sprintf( http_buf, HTTP_UPDATE_ROW, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// UPD DIV
		sprintf( url, "/cs378x?action=div&id=%d&list=%s", get_id, str_list);
		sprintf( http_buf, HTTP_UPDATE_DIV, cfg.http.autorefresh*1000, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	setautorefresh(autorefresh);\n}");
		tcp_writestr(&tcpbuf, sock, "\n</script>\n");
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
		tcp_write_menu(&tcpbuf, sock,PAGE_CS378X);
		// Info de servidores (acima da div principal)
		{
			tcp_writestr(&tcpbuf, sock, "<div style='margin:12px 12px 0 12px'><div class=stat-section style='margin:0'>");
			sprintf( http_buf, "<h3 class=stitle>cs378x Servers (%d)</h3>", cfg.cs378x.totalservers);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			tcp_writestr(&tcpbuf, sock, "<table class=maintable><tr><th>Server</th><th>Port</th><th>Status</th><th>Connected</th></tr>");
			int itotal, iconnected, iactive;
			total_cs378x_clients( &itotal, &iconnected, &iactive );
			sprintf( http_buf, "<tr><td class=left>TOTAL</td><td class=right>-</td><td class=right>-</td><td class=right>%d / %d</td></tr>", iconnected, itotal);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			struct camd35_server_data *box = cfg.cs378x.server;
			while ( box ) {
				int btotal, bconnected, bactive;
				cs378x_clients( box, &btotal, &bconnected, &bactive );
				if (box->handle>0) sprintf( http_buf, "<tr><td class=left><a href='/cs378x?id=%d'>cs378x %d</a></td><td class=right>%d</td><td class=right><span class=success>ONLINE</span></td><td class=right>%d / %d</td></tr>", box->id, box->id, box->port, bconnected, btotal);
				else sprintf( http_buf, "<tr><td class=left><a href='/cs378x?id=%d'>cs378x %d</a></td><td class=right>%d</td><td class=right><span class=failed>OFFLINE</span></td><td class=right>%d / %d</td></tr>", box->id, box->id, box->port, bconnected, btotal);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				box = box->next;
			}
			tcp_writestr(&tcpbuf, sock, "</table></div></div>");
		}
		// DIV
		tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");
	}

	int total, connected, active;
	tcp_writestr(&tcpbuf, sock, "<select style=\"width:200px;\" onchange=\"parent.location.href='/cs378x?id='+this.value\">");
	sprintf( http_buf, "<option value=0>ALL (%d)</option>", cfg.cs378x.totalservers);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	struct camd35_server_data *tmp = cfg.cs378x.server;
	while (tmp) {
		if (get_id==tmp->id) sprintf( http_buf, "<option value=%d selected>[%d] cs378x %d</option>",tmp->id,tmp->port, tmp->id );
		else sprintf( http_buf, "<option value=%d>[%d] cs378x %d</option>",tmp->id,tmp->port, tmp->id );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tmp = tmp->next;
	}
	tcp_writestr(&tcpbuf, sock, "</select> ");
	//
	if (cs378x) cs378x_clients( cs378x, &total, &connected, &active ); else total_cs378x_clients( &total, &connected, &active );
	char *class1 = "button"; char *class2 = "sbutton";
	char *class;
	if (get_list==LIST_ACTIVE) class = class2; else class = class1;
	sprintf( http_buf, "<input type=button class=%s onclick=\"parent.location='/cs378x?id=%d&amp;list=active'\" value='Active Clients (%d)'>", class, get_id, active);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (get_list==LIST_CONNECTED) class = class2; else class = class1;
	sprintf( http_buf, " <input type=button class=%s onclick=\"parent.location='/cs378x?id=%d&amp;list=connected'\" value='Connected Clients (%d)'>", class, get_id, connected);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (get_list==LIST_ALL) class = class2; else class = class1;
	sprintf( http_buf, " <input type=button class=%s onclick=\"parent.location='/cs378x?id=%d&amp;list=all'\" value='All Clients (%d)'>", class, get_id, total);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//
	if (get_id) { // One Server Selected
		// Table
		sprintf( http_buf, "\n<table class=maintable width=100%%><tr><th width=100px>Client</th><th width=120px>ip</th><th width=110px>Connected</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		struct camd35_client_data *cli = cs378x->client;
		int alt=0;
		if (get_list==LIST_ACTIVE) {
			while (cli) {
				if ( (cli->connection.status>0)&&((GetTickCount()-cli->lastecmtime) < 20000) ) {
					if (alt==1) alt=2; else alt=1;
					getcs378xcells(cli,cell);
					snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[9],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				cli = cli->next;
			}
		}
		else if (get_list==LIST_CONNECTED) {
			while (cli) {
				if (cli->connection.status>0) {
					if (alt==1) alt=2; else alt=1;
					getcs378xcells(cli,cell);
					snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[9],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				cli = cli->next;
			}
		}
		else { // ALL
			while (cli) {
				if (alt==1) alt=2; else alt=1;
				getcs378xcells(cli,cell);
				snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[9],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				cli = cli->next;
			}
		}
		sprintf( http_buf, "\n</table>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}

	else {
		// Table
		tcp_writestr(&tcpbuf,sock, "\n<table class=maintable width=100%>");
		tcp_writestr(&tcpbuf,sock, "\n<tr><th width=100px>Client</th><th width=120px>ip</th><th width=110px>Connected</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>");
		int alt=0;
		cs378x = cfg.cs378x.server;
		while (cs378x) {
			int total, connected, active;
			cs378x_clients( cs378x, &total, &connected, &active );
			if ( (get_list==LIST_ACTIVE) && active ) {
				snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=9> cs378x %d (%d)</td></tr>", cs378x->id, active); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				struct camd35_client_data *cli = cs378x->client;
				while (cli) {
					if ( (cli->connection.status>0)&&((GetTickCount()-cli->lastecmtime) < 20000) ) {
						if (alt==1) alt=2; else alt=1;
						getcs378xcells(cli,cell);
						snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[9],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7]);
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
					cli = cli->next;
				}
			}
			else if ( (get_list==LIST_ALL) && total ) {
				snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=9> cs378x %d (%d)</td></tr>", cs378x->id, total); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				struct camd35_client_data *cli = cs378x->client;
				while (cli) {
					if (alt==1) alt=2; else alt=1;
					getcs378xcells(cli,cell);
					snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[9],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					cli = cli->next;
				}
			}
			else if ( (get_list==LIST_CONNECTED) && connected ) {
				snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=9> cs378x %d (%d)</td></tr>", cs378x->id, connected); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				struct camd35_client_data *cli = cs378x->client;
				while (cli) {
					if (cli->connection.status>0) {
						if (alt==1) alt=2; else alt=1;
						getcs378xcells(cli,cell);
						snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[9],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7]);
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
					cli = cli->next;
				}
			}
			cs378x = cs378x->next;
		}
		sprintf( http_buf, "</table>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	if (get_action==ACTION_PAGE) {
		tcp_writestr(&tcpbuf, sock, "</div>");
		tcp_writestr(&tcpbuf, sock, "</body></html>");
	}

	tcp_flush(&tcpbuf, sock);
}

///////////////////////////////////////////////////////////////////////////////

void http_send_cs378x_client(int sock, http_request *req)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;

	// Get Params
	char *str_action = isset_get( req, "action");
	char *str_id = isset_get( req, "id"); // Client ID
	char *str_name = isset_get( req, "name"); // Client NAME
	char *str_srvid = isset_get( req, "srvid"); // CCcam Server ID

	// Action
	int get_action = ACTION_PAGE;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = ACTION_DIV;
		else if (!strcmp(str_action,"row")) get_action = ACTION_ROW;
		else if (!strcmp(str_action,"disable")) get_action = ACTION_DISABLE;
		else if (!strcmp(str_action,"enable")) get_action = ACTION_ENABLE;
		else if (!strcmp(str_action,"status")) get_action = ACTION_STATUS;
		else if (!strcmp(str_action,"debug")) get_action = ACTION_DEBUG;
		else if (!strcmp(str_action,"dbginfo")) get_action = ACTION_DBGINFO;
		else if (!strcmp(str_action,"update")) get_action = ACTION_UPDATE;
		else str_action = NULL;
	}
	if (!str_action) { str_action = "page"; get_action = ACTION_PAGE; }

	/////////////////////////////////////////////

	// GET CLIENT
	struct camd35_client_data *cli = NULL;
	if (str_id) cli = getcs378xclientbyid( atoi(str_id) );
	if (!cli) return;
	//
	if (get_action==ACTION_DISABLE) {
		cli->flags |= FLAG_DISABLE;
		if (cli->connection.status>0) cs378x_disconnect_cli(cli);
		http_send_ok(sock);
		return;
	}
	else if (get_action==ACTION_ENABLE) {
		cli->flags &= ~FLAG_DISABLE;
		http_send_ok(sock);
		return;
	}
	else if (get_action==ACTION_STATUS) {
		if (cli->connection.status>0) http_send_text(sock,"connected"); else http_send_text(sock,"disconnected");
		return;
	}
	else if (get_action==ACTION_DEBUG) {
		flagdebug = getdbgflag( DBG_CS378X, 0, cli->id);
		http_send_ok(sock);
		return;
	}
	else if (get_action==ACTION_DBGINFO) {
		char dbg[1024];
		sprintf( dbg, "<div class='dbginfo'><b>%s</b> | IP: %s | Status: %s<br>ECM: %d pedidos, %d denied, %d OK | Last ECM: %us ago | Last DCW: %us ago</div>",
			cli->user, (char*)ip2string(cli->ip),
			cli->connection.status>0?"CONNECTED":(cli->connection.status<0?"CONNECTING...":"OFFLINE"),
			cli->ecmnb, cli->ecmdenied, cli->ecmok,
			cli->lastecmtime?(GetTickCount()-cli->lastecmtime)/1000:0,
			cli->lastdcwtime?(GetTickCount()-cli->lastdcwtime)/1000:0);
		http_send_text(sock, dbg);
		return;
	}
	else if (get_action==ACTION_UPDATE) {
/*		char *str = isset_get( req, "expire"); // Client ID
		if (str) {
			if ( (str[4]=='-')&&(str[7]=='-') ) strptime(  str, "%Y-%m-%d %H", &cli->enddate);
			else if ( (str[2]=='-')&&(str[5]=='-') ) strptime(  str, "%d-%m-%Y %H", &cli->enddate);
		}
		str = isset_get( req, "active"); // Client ID
		if (str) {
			if (str[0]=='0') {
				cli->flags |= FLAG_DISABLE;
				if (cli->connection.status>0) cs378x_disconnect_cli(cli);
			}
			else cli->flags &= ~FLAG_DISABLE;
		}*/
		http_send_text(sock, "OK");
		return;
	}

	//
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) ); // header tambem no div (XHR exige status line)
	if (get_action==ACTION_PAGE) {

		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, html_title, cfg.http.title, "Cs378x Client"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		// JS
        tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
		tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
		// UPD DIV
		char url[256];
		sprintf( url, "/cs378xclient?id=%d&action=div", cli->id);
		sprintf( http_buf, HTTP_UPDATE_DIV, cfg.http.autorefresh*1000, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	setautorefresh(autorefresh);\n}");
		tcp_writestr(&tcpbuf, sock, "\n</script>\n");
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
		tcp_write_menu(&tcpbuf, sock,0);
		// DIV
		tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");
	}

	tcp_writestr(&tcpbuf, sock, "<table style=\"padding:0px; margin:0px;\" width=\"100%%\"><tbody>\n" );
	tcp_writestr(&tcpbuf, sock, "<tr><td style=\"vertical-align:top; width:400px;\">\n" );

	tcp_writestr(&tcpbuf, sock, "<table class=infotable><tbody>\n<tr><th colspan=2>Client Informations</th></tr>\n" );
	// NAME
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>User name</td><td class=right>%s</td></tr>\n",cli->user);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// Connection Time
	if (cli->connection.status>0) {
		tcp_writestr(&tcpbuf, sock, "<tr><td class=left>Status</td><td class=right>Connected</td></tr>\n");
		uint32_t d = (GetTickCount()-cli->connection.time)/1000;
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Connection time</td><td class=right>%02dd %02d:%02d:%02d</td></tr>\n", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// IP
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>IP Address</td><td class=right>%s</td></tr>\n",(char*)ip2string(cli->ip) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		/*// Program ID
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Client Program</td><td class=right>%s(%04x)</td></tr>",programid(cli->progid), cli->progid );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );*/
	}
	else {
		tcp_writestr(&tcpbuf, sock, "<tr><td class=left>Status</td><td class=right>Disconnected</td></tr>\n");
		if ( cli->connection.lastseen ) {
			uint32_t d = (GetTickCount()-cli->connection.lastseen)/1000;
			snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Last Seen</td><td class=right>%02dd %02d:%02d:%02d</td></tr>\n", d/(3600*24),(d/3600)%24,(d/60)%60,d%60);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
	}
	// UPTIME
	if ( cli->connection.uptime || (cli->connection.status>0) ) {
		uint32_t uptime;
		if (cli->connection.status>0) uptime = (GetTickCount()-cli->connection.time)+cli->connection.uptime; else uptime = cli->connection.uptime;
		uptime /= 1000;
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Uptime</td><td class=right>%02dd %02d:%02d:%02d</td></tr>",uptime/(3600*24),(uptime/3600)%24,(uptime/60)%60,uptime%60);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
#ifdef CHECK_NEXTDCW
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>DCW CHECK</td><td class=right>%s</td></tr>", yesno(cli->dcwcheck) );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
	tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );


	// INFO
	struct client_info_data *info = cli->info;
	if (info) {
		tcp_writestr(&tcpbuf, sock, "<table class=\"infotable\"><tbody>\n" );
		tcp_writestr(&tcpbuf, sock, "<tr><th colspan=2>Additional Informations</th></tr>\n" );
		while (info) {
			snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>%s</td><td class=right>%s</td></tr>\n",info->name,info->value);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			info = info->next;
		}
		tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );
	}

	// Ecm Stat
	tcp_writestr(&tcpbuf, sock, "<table class=\"infotable\"><tbody>\n" );
	tcp_writestr(&tcpbuf, sock, "<tr><th colspan=2>ECM Statistics</th></tr>\n" );
	int ecmaccepted = cli->ecmnb-cli->ecmdenied;
	sprintf( http_buf, "<tr><td class=left>Total ECM requests</td><td class=right>%d</td></tr>\n", cli->ecmnb);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<tr><td class=left>Accepted ECM requests</td><td class=right>%d</td></tr>\n", ecmaccepted);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<tr><td class=left>Good ECM answer</td><td class=right>%d</td></tr>\n", cli->ecmok);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//Ecm Time
	if (cli->ecmok) {
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Average Time</td><td class=right>%d ms</td></tr>\n",(cli->ecmoktime/cli->ecmok) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
//#ifdef SRV_CSCACHE
//	sprintf( http_buf, "<tr><td class=left>Cached CW</td><td class=right>%d</td></tr>\n", cli->cachedcw);
//	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
//#endif
	// Freeze
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Total Freeze</td><td class=right>%d</td></tr>\n", cli->freeze);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );


	tcp_writestr(&tcpbuf, sock, "</td><td style=\"vertical-align:top;\">\n" );

	//Last Used Share
	if ( cli->lastecm.caid ) {
		tcp_writestr(&tcpbuf, sock, "<table class=\"infotable\"><tbody>\n" );
		tcp_writestr(&tcpbuf, sock, "<tr><th>Last Used share</th></tr>\n");
		// Decode Status
		if (cli->lastecm.status)
			snprintf( http_buf, sizeof(http_buf),"<tr><td>Decode success</td></tr>\n");
		else
			snprintf( http_buf, sizeof(http_buf),"<tr><td>Decode failed</td></tr>\n");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// Channel
		snprintf( http_buf, sizeof(http_buf),"<tr><td>Channel %s (%dms) %s</td></tr>\n", getchname(cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid) , cli->lastecm.decodetime, str_laststatus[cli->lastecm.status] );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		// Server
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->lastecm.status) {
				tcp_writestr(&tcpbuf, sock, "<tr><td>From ");
				src2string(cli->lastecm.dcwsrctype, cli->lastecm.dcwsrcid, http_buf );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				tcp_writestr(&tcpbuf, sock, "</td></tr>");
			}
			// Last ECM
			ECM_DATA *ecm = cli->lastecm.request;
			// ECM
			snprintf( http_buf, sizeof(http_buf),"<tr><td>ECM(%d): ", ecm->ecmlen); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			array2hex( ecm->ecm, http_buf, ecm->ecmlen );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf( http_buf,"</td></tr>\n"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			// DCW
			if (cli->lastecm.status) {
				snprintf( http_buf, sizeof(http_buf),"<tr><td>CW: ");	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->cw, http_buf, 16 );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				sprintf( http_buf,"</td></tr>\n"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
#ifdef CHECK_NEXTDCW
			if ( ecm->lastdecode.ecm && (ecm->lastdecode.counter>0) ) {
				snprintf( http_buf, sizeof(http_buf),"<tr><td>Previous CW: "); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->lastdecode.dcw, http_buf, 16 ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				tcp_writestr(&tcpbuf, sock, "</td></tr>\n");
				if (ecm->lastdecode.error) {
					snprintf( http_buf, sizeof(http_buf),"<tr><td>Errors = %d</td></tr>\n", ecm->lastdecode.error);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				snprintf( http_buf, sizeof(http_buf),"<tr><td>Total Cycles = %d</td></tr>\n<tr><td>ECM Interval = %ds</td></tr>\n", ecm->lastdecode.counter, ecm->lastdecode.dcwchangetime/1000);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
#endif
			// Last used share (status do ultimo decode)
			if (cli->lastecm.status==1) {
				tcp_writestr(&tcpbuf, sock, "<tr><td class=success>Decode Success</td></tr>");
			}
			else if (cli->lastecm.status==2) {
				snprintf( http_buf, sizeof(http_buf),"<tr><td class=nok-yellow>channel %s (%dms) NOK (BISS EMU)</td></tr>", getchname(cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid), cli->lastecm.decodetime);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			//
			if (ecm->server[0].srvid) {
				sprintf( http_buf, "<tr><td><table class='infotable'><tbody><tr><th width='30px'>ID</th><th width='250px'>Server</th><th width='50px'>Status</th><th width='70px'>Start time</th><th width='70px'>End time</th><th width='90px'>Elapsed time</th><th>CW</th></tr></tbody>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				int i;
				for(i=0; i<20; i++) {
					if (!ecm->server[i].srvid) break;
					char* str_srvstatus[] = { "WAIT", "OK", "NOK", "BUSY" };
					struct server_data *srv = getsrvbyid(ecm->server[i].srvid);
					if (srv) {
						snprintf( http_buf, sizeof(http_buf),"<tr><td>%d</td><td>%s:%d</td><td>%s</td><td>%dms</td>", i+1, srv->host->name, srv->port, str_srvstatus[ecm->server[i].flag], ecm->server[i].sendtime - ecm->recvtime );
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// Recv Time
						if (ecm->server[i].statustime>ecm->server[i].sendtime)
							sprintf( http_buf,"<td>%dms</td><td>%dms</td>", ecm->server[i].statustime - ecm->recvtime, ecm->server[i].statustime-ecm->server[i].sendtime );
						else
							sprintf( http_buf,"<td>--</td><td>--</td>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// DCW
						if (ecm->server[i].flag==ECM_SRV_REPLY_GOOD) {
							sprintf( http_buf,"<td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							array2hex( ecm->server[i].dcw, http_buf, 16 );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							sprintf( http_buf,"</td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						else {
							sprintf( http_buf,"<td>--</td>");
							tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						sprintf( http_buf,"</tr>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
				}
				tcp_writestr(&tcpbuf, sock, "</tbody></table></td></tr>\n" );
			}
		}
		tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );
	}

	// Current Busy Ecm
	if (cli->ecm.busy) {
		ECM_DATA *ecm = cli->ecm.request;
		if (ecm) http_send_ecmstatus(&tcpbuf, sock, ecm);
	}

	tcp_writestr(&tcpbuf, sock, "</td></tr></tbody></table>" );

	if (get_action==ACTION_PAGE) {
		tcp_writestr(&tcpbuf, sock, "</div>");
		tcp_writestr(&tcpbuf, sock, "</body></html>");
	}
	tcp_flush(&tcpbuf, sock);
}


#endif

















#ifdef CAMD35_SRV

void getcamd35cells(struct camd35_client_data *cli, char cell[10][2048])
{
	char temp[2048];
	uint32_t d;

	// CELL0 # NAME
	sprintf( cell[0],"<a href='/camd35client?id=%d'>%s</a>",cli->id,cli->user);

	// CELL1 # IP
	if ( cli->ip ) { // Get Last IP
		char *p = getcountrycodebyip(cli->ip);
		if (p) sprintf( cell[1],"<img src='/flag_%s.gif' title='%s'> %s", p, getcountryname(p), (char*)ip2string(cli->ip) ); else sprintf( cell[1],"%s",(char*)ip2string(cli->ip) );
	}
	else strcpy( cell[1], " ");

	// CELL2 # Connection Time
	// Camd35 is UDP so there's no connection. Use cli->lastecmtime to check last received ecm time is less than 90 seconds
	if ((GetTickCount()-cli->lastecmtime) < 90000) {
		if (cli->ecm.busy) sprintf( cell[9],"busy"); else sprintf( cell[9],"online");
		sprintf( cell[2], "online");
	}
	else {
		sprintf( cell[9],"offline");
		if (cli->flags&FLAG_DELETE) sprintf( cell[2],"Removed");
		else if (cli->flags&FLAG_EXPIRED) sprintf( cell[2],"Expired");
		else if (cli->flags&FLAG_DISABLE) sprintf( cell[2],"Disabled");
		else sprintf( cell[2],"offline");
	}
	// CELL3+4+5 # ECM STAT: TOTAL/ACCEPTED/OK
	// ECM STAT
	sprintf( cell[3], "%d", cli->ecmnb );

	int ecmaccepted = cli->ecmnb-cli->ecmdenied;
	getstatcell( ecmaccepted, cli->ecmnb, cell[4]);
	getstatcell( cli->ecmok, ecmaccepted, cell[5]);

	// CELL6 # Ecm Time
	if (cli->ecmok) sprintf( cell[6],"%d ms",(cli->ecmoktime/cli->ecmok) ); else sprintf( cell[6],"-- ms");

	// CELL7 # Last Used Share
	if ( cli->lastecm.caid ) {
		if (cli->lastecm.status)  strcpy( cell[7],"<span class=success"); else strcpy( cell[7],"<span class=failed");
		sprintf( temp," title='%04x:%06x:%04x'>ch %s (%dms) %s ",cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid, getchname(cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid) , cli->lastecm.decodetime, str_laststatus[cli->lastecm.status] );
		strcat( cell[7], temp );
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->lastecm.status) {
				strcat( cell[7], " / from ");
				src2string(cli->lastecm.dcwsrctype, cli->lastecm.dcwsrcid, temp);
				strcat( cell[7], temp);
			}
		}
		strcat( cell[7], "</span>" );
	}
	else strcpy( cell[7], " ");

	strcat( cell[7], "<br><span style='display:inline-flex;gap:2px;white-space:nowrap;margin-top:4px;'>");
	if ( !(cli->flags&(FLAG_DELETE|FLAG_EXPIRED)) ) {
		if (cli->flags&FLAG_DISABLE) {
			sprintf( temp," <span class='icobtn on' title='Enable' onclick=\"imgrequest('/camd35client?id=%d&action=enable',this);setTimeout('updateDiv()',600)\">ON</span>",cli->id);
			strcat( cell[7], temp );
		}
		else {
			sprintf( temp," <span class='icobtn off' title='Disable' onclick=\"imgrequest('/camd35client?id=%d&action=disable',this);setTimeout('updateDiv()',600)\">OFF</span>",cli->id);
			strcat( cell[7], temp );
		}
	}
	sprintf( temp," <span class='icobtn dbg' title='Debug' onclick=\"toggleDbgRow(%d,'/camd35client?id=%d&action=dbginfo')\">DBG</span>",cli->id,cli->id);
	strcat( cell[7], temp );
	strcat( cell[7], "</span>");
}

void total_camd35_clients( int *total, int *connected, int *active )
{
	*total = 0;
	*connected = 0;
	*active = 0;
	struct camd35_server_data *camd35 = cfg.camd35.server;
	while (camd35) {
		struct camd35_client_data *cli = camd35->client;
		while (cli) {
			(*total)++;
			if ((GetTickCount()-cli->lastecmtime) < 90000) {   // No connection status in camd35 use lastecmtime < 90 seconds
				(*connected)++;
				if ( (GetTickCount()-cli->lastecmtime) < 20000 ) (*active)++;
			}
			cli=cli->next;
		}
		camd35 = camd35->next;
	}
}

void camd35_clients( struct camd35_server_data *camd35, int *total, int *connected, int *active )
{
	*total = 0;
	*connected = 0;
	*active = 0;
	struct camd35_client_data *cli = camd35->client;
	while (cli) {
		(*total)++;
		if ((GetTickCount()-cli->lastecmtime) < 90000) {
			(*connected)++;
			if ( (GetTickCount()-cli->lastecmtime) < 20000 ) (*active)++;
		}
		cli=cli->next;
	}
}

void http_send_camd35(int sock, http_request *req)
{
	char http_buf[4096];
	struct tcp_buffer_data tcpbuf;
	char cell[10][2048];

	// Get Params
	char *str_action = isset_get( req, "action");
	char *str_list = isset_get( req, "list");
	char *str_id = isset_get( req, "id"); // server ID
	char *str_clid = isset_get( req, "clid"); // Client ID
	// Param 'action'
	int get_action;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = ACTION_DIV;
		else if (!strcmp(str_action,"row")) get_action = ACTION_ROW;
#ifndef PUBLIC
		else if (!strcmp(str_action,"xml")) get_action = ACTION_XML; // Get Clients info in xml
#endif
		else if (!strcmp(str_action,"disable")) get_action = ACTION_DISABLE;
		else if (!strcmp(str_action,"enable")) get_action = ACTION_ENABLE;
		else if (!strcmp(str_action,"status")) get_action = ACTION_STATUS;
		else if (!strcmp(str_action,"debug")) get_action = ACTION_DEBUG;
		else str_action = NULL;
	}
	if (!str_action) { str_action = "page"; get_action = ACTION_PAGE; }
	/////////////////////////////////////////////

	if (get_action==ACTION_ROW) {
		// Check for XML ROW
		struct camd35_client_data *cli = NULL;
		if (str_clid) {
			int id = atoi(str_clid);
			struct camd35_server_data *camd35 = cfg.camd35.server;
			while (camd35) {
				if (!(camd35->flags&FLAG_DELETE)) {
					struct camd35_client_data *cli = camd35->client;
					while (cli) {
						if ( !(cli->flags&FLAG_DELETE) && (cli->id==id) ) {
							// Send XML CELLS
							getcamd35cells(cli,cell);
							int i; for(i=0; i<10; i++) xmlescape( cell[i] );
							sprintf( http_buf, "<camd35>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2_c>%s</c2_c>\n<c2>%s</c2>\n<c3>%s</c3>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6>\n<c7>%s</c7>\n</camd35>\n",cell[0],cell[1],cell[9],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7] );
							http_send_xml( sock, req, http_buf, strlen(http_buf));
						}
						cli = cli->next;
					}
				}
				camd35 = camd35->next;
			}
		}
		return;
	}			

	// Param 'list'
	int get_list = LIST_ALL;
	if (str_list) {
		if (!strcmp(str_list,"connected")) get_list = LIST_CONNECTED;
		else if (!strcmp(str_list,"all")) get_list = LIST_ALL;
		else str_list = NULL;
	}
	if (!str_list) str_list = "all";
	// Param 'id'
	int get_id = 0;
	struct camd35_server_data *camd35 = NULL;
	if (str_id)	{
		get_id = atoi(str_id);
		camd35 = cfg.camd35.server;
		while (camd35) {
			if (camd35->id == get_id) break;
			camd35 = camd35->next;
		}
		if (!camd35) get_id = 0;
	}
	//
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) ); // header tambem no div (XHR exige status line)
	if (get_action==ACTION_PAGE) {

		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, html_title, cfg.http.title, "Cs358x/Camd35"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		// JS
        tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
		tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
		// ACTIONS REQUEST
		tcp_writestr(&tcpbuf, sock, "\nfunction imgrequest( url, el )\n{\n	var httpRequest;\n	try { httpRequest = new XMLHttpRequest(); }\n	catch (trymicrosoft) { try { httpRequest = new ActiveXObject('Msxml2.XMLHTTP'); } catch (oldermicrosoft) { try { httpRequest = new ActiveXObject('Microsoft.XMLHTTP'); } catch(failed) { httpRequest = false; } } }\n	if (!httpRequest) { alert('Your browser does not support Ajax.'); return false; }\n	if ( typeof(el)!='undefined' ) {\n		el.onclick = null;\n		el.style.opacity = '0.7';\n		httpRequest.onreadystatechange = function()\n		{\n			if (httpRequest.readyState == 4) if (httpRequest.status == 200) el.style.opacity = '0.3';\n		}\n	}\n	httpRequest.open('GET', url, true);\n	httpRequest.send(null);\n}\n");
		// UPD ROW
		tcp_writestr(&tcpbuf, sock, "\nfunction xmlupdateRow( xmlDoc, id ) \n{\n    var row = document.getElementById(id);\n    	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n    row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n    row.cells.item(2).className = xmlDoc.getElementsByTagName('c2_c')[0].childNodes[0].nodeValue;\n    row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n    row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n    row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n    row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n    row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n    row.cells.item(7).innerHTML = xmlDoc.getElementsByTagName('c7')[0].childNodes[0].nodeValue;\n}");
		char url[256];
		sprintf( url, "'/camd35?action=row&clid='+idx");
		sprintf( http_buf, HTTP_UPDATE_ROW, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// UPD DIV
		sprintf( url, "/camd35?action=div&id=%d&list=%s", get_id, str_list);
		sprintf( http_buf, HTTP_UPDATE_DIV, cfg.http.autorefresh*1000, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	setautorefresh(autorefresh);\n}");
		tcp_writestr(&tcpbuf, sock, "\n</script>\n");
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
		tcp_write_menu(&tcpbuf, sock,PAGE_CAMD35);
		// Info de servidores (acima da div principal)
		{
			tcp_writestr(&tcpbuf, sock, "<div style='margin:12px 12px 0 12px'><div class=stat-section style='margin:0'>");
			sprintf( http_buf, "<h3 class=stitle>Camd35 Servers (%d)</h3>", cfg.camd35.totalservers);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			tcp_writestr(&tcpbuf, sock, "<table class=maintable><tr><th>Server</th><th>Port</th><th>Status</th><th>Connected</th></tr>");
			int itotal, iconnected, iactive;
			total_camd35_clients( &itotal, &iconnected, &iactive );
			sprintf( http_buf, "<tr><td class=left>TOTAL</td><td class=right>-</td><td class=right>-</td><td class=right>%d / %d</td></tr>", iconnected, itotal);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			struct camd35_server_data *box = cfg.camd35.server;
			while ( box ) {
				int btotal, bconnected, bactive;
				camd35_clients( box, &btotal, &bconnected, &bactive );
				if (box->handle>0) sprintf( http_buf, "<tr><td class=left><a href='/camd35?id=%d'>camd35 %d</a></td><td class=right>%d</td><td class=right><span class=success>ONLINE</span></td><td class=right>%d / %d</td></tr>", box->id, box->id, box->port, bconnected, btotal);
				else sprintf( http_buf, "<tr><td class=left><a href='/camd35?id=%d'>camd35 %d</a></td><td class=right>%d</td><td class=right><span class=failed>OFFLINE</span></td><td class=right>%d / %d</td></tr>", box->id, box->id, box->port, bconnected, btotal);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				box = box->next;
			}
			tcp_writestr(&tcpbuf, sock, "</table></div></div>");
		}
		// DIV
		tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");
	}

	int total, connected, active;
	tcp_writestr(&tcpbuf, sock, "<select style=\"width:200px;\" onchange=\"parent.location.href='/camd35?id='+this.value\">");
	sprintf( http_buf, "<option value=0>ALL (%d)</option>", cfg.camd35.totalservers); //total_camd35_servers());
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	struct camd35_server_data *tmp = cfg.camd35.server;
	while (tmp) {
		if (get_id==tmp->id) sprintf( http_buf, "<option value=%d selected>[%d] camd35 %d</option>",tmp->id,tmp->port, tmp->id );
		else sprintf( http_buf, "<option value=%d>[%d] camd35 %d</option>",tmp->id,tmp->port, tmp->id );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tmp = tmp->next;
	}
	tcp_writestr(&tcpbuf, sock, "</select> ");
	//
	if (camd35) camd35_clients( camd35, &total, &connected, &active ); else total_camd35_clients( &total, &connected, &active );
	char *class1 = "button"; char *class2 = "sbutton";
	char *class;
	if (get_list==LIST_ACTIVE) class = class2; else class = class1;
	sprintf( http_buf, "<input type=button class=%s onclick=\"parent.location='/camd35?id=%d&amp;list=active'\" value='Active Clients (%d)'>", class, get_id, active);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (get_list==LIST_CONNECTED) class = class2; else class = class1;
	sprintf( http_buf, " <input type=button class=%s onclick=\"parent.location='/camd35?id=%d&amp;list=connected'\" value='Connected Clients (%d)'>", class, get_id, connected);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (get_list==LIST_ALL) class = class2; else class = class1;
	sprintf( http_buf, " <input type=button class=%s onclick=\"parent.location='/camd35?id=%d&amp;list=all'\" value='All Clients (%d)'>", class, get_id, total);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//
	if (camd35) { // One Server Selected
		// Table
		sprintf( http_buf, "\n<table class=maintable width=100%%><tr><th width=100px>Client</th><th width=120px>ip</th><th width=110px>Connected</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		struct camd35_client_data *cli = camd35->client;
		int alt=0;
		if (get_list==LIST_ACTIVE) {
			while (cli) {
				if ( ((GetTickCount()-cli->lastecmtime) < 20000) ) {
					if (alt==1) alt=2; else alt=1;
					getcamd35cells(cli,cell);
					snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[9],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				cli = cli->next;
			}
		}
		else if (get_list==LIST_CONNECTED) {
			while (cli) {
				if (((GetTickCount()-cli->lastecmtime) < 90000)) {
					if (alt==1) alt=2; else alt=1;
					getcamd35cells(cli,cell);
					snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[9],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				cli = cli->next;
			}
		}
		else { // ALL
			while (cli) {
				if (alt==1) alt=2; else alt=1;
				getcamd35cells(cli,cell);
				snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[9],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				cli = cli->next;
			}
		}
		sprintf( http_buf, "\n</table>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}

	else {
		// Table
		tcp_writestr(&tcpbuf,sock, "\n<table class=maintable width=100%>");
		tcp_writestr(&tcpbuf,sock, "\n<tr><th width=100px>Client</th><th width=120px>ip</th><th width=110px>Connected</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>");
		int alt=0;
		camd35 = cfg.camd35.server;
		while (camd35) {
			int total, connected, active;
			camd35_clients( camd35, &total, &connected, &active );
			if ( (get_list==LIST_ACTIVE) && active ) {
				snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=9> camd35 %d (%d)</td></tr>", camd35->id, active); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				struct camd35_client_data *cli = camd35->client;
				while (cli) {
					if ( ((GetTickCount()-cli->lastecmtime) < 20000) ) {
						if (alt==1) alt=2; else alt=1;
						getcamd35cells(cli,cell);
						snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[9],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7]);
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
					cli = cli->next;
				}
			}
			else if ( (get_list==LIST_ALL) && total ) {
				snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=9> camd35 %d (%d)</td></tr>", camd35->id, total); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				struct camd35_client_data *cli = camd35->client;
				while (cli) {
					if (alt==1) alt=2; else alt=1;
					getcamd35cells(cli,cell);
					snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[9],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					cli = cli->next;
				}
			}
			else if ( (get_list==LIST_CONNECTED) && connected ) {
				snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=9> camd35 %d (%d)</td></tr>", camd35->id, connected); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				struct camd35_client_data *cli = camd35->client;
				while (cli) {
					if (((GetTickCount()-cli->lastecmtime) < 90000)) {
						if (alt==1) alt=2; else alt=1;
						getcamd35cells(cli,cell);
						snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[9],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7]);
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
					cli = cli->next;
				}
			}
			camd35 = camd35->next;
		}
		sprintf( http_buf, "</table>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
#ifdef CS378X_SRV
	// ===== seccao Cs378x (TCP) - mesma familia, so na pagina completa =====
	if (get_action==ACTION_PAGE) {
		tcp_writestr(&tcpbuf, sock, "<div class=stat-section style='margin:10px 0'>");
		sprintf( http_buf, "<h3 class=stitle>Cs378x Servers (%d, TCP)</h3>", cfg.cs378x.totalservers);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_writestr(&tcpbuf, sock, "<table class=maintable><tr><th>Server</th><th>Port</th><th>Status</th><th>Connected</th></tr>");
		int itotal, iconnected, iactive;
		total_cs378x_clients( &itotal, &iconnected, &iactive );
		sprintf( http_buf, "<tr><td class=left>TOTAL</td><td class=right>-</td><td class=right>-</td><td class=right>%d / %d</td></tr>", iconnected, itotal);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		struct camd35_server_data *box = cfg.cs378x.server;
		while ( box ) {
			int btotal, bconnected, bactive;
			cs378x_clients( box, &btotal, &bconnected, &bactive );
			if (box->handle>0) sprintf( http_buf, "<tr><td class=left><a href='/cs378x?id=%d'>cs378x %d</a></td><td class=right>%d</td><td class=right><span class=success>ONLINE</span></td><td class=right>%d / %d</td></tr>", box->id, box->id, box->port, bconnected, btotal);
			else sprintf( http_buf, "<tr><td class=left><a href='/cs378x?id=%d'>cs378x %d</a></td><td class=right>%d</td><td class=right><span class=failed>OFFLINE</span></td><td class=right>%d / %d</td></tr>", box->id, box->id, box->port, bconnected, btotal);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			box = box->next;
		}
		tcp_writestr(&tcpbuf, sock, "</table>");

		tcp_writestr(&tcpbuf, sock, "<table class=maintable><tr><th width=100px>Client</th><th width=120px>ip</th><th width=110px>Connected</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>");
		box = cfg.cs378x.server;
		int altx = 0;
		while (box) {
			struct camd35_client_data *cli = box->client;
			int ctotal, cconnected, cactive;
			cs378x_clients( box, &ctotal, &cconnected, &cactive );
			if (ctotal) {
				snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=9> cs378x %d (%d)</td></tr>", box->id, ctotal);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				while (cli) {
					if (altx==1) altx=2; else altx=1;
					getcs378xcells(cli,cell);
					snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,altx,cli->id,cell[0],cell[1],cell[9],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					cli = cli->next;
				}
			}
			box = box->next;
		}
		tcp_writestr(&tcpbuf, sock, "</table></div>");
	}
#endif
	if (get_action==ACTION_PAGE) {
		tcp_writestr(&tcpbuf, sock, "</div>");
		tcp_writestr(&tcpbuf, sock, "</body></html>");
	}

	tcp_flush(&tcpbuf, sock);
}

///////////////////////////////////////////////////////////////////////////////

void http_send_camd35_client(int sock, http_request *req)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;

	// Get Params
	char *str_action = isset_get( req, "action");
	char *str_id = isset_get( req, "id"); // Client ID
	char *str_name = isset_get( req, "name"); // Client NAME
	char *str_srvid = isset_get( req, "srvid"); // CCcam Server ID

	// Action
	int get_action = ACTION_PAGE;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = ACTION_DIV;
		else if (!strcmp(str_action,"row")) get_action = ACTION_ROW;
		else if (!strcmp(str_action,"disable")) get_action = ACTION_DISABLE;
		else if (!strcmp(str_action,"enable")) get_action = ACTION_ENABLE;
		else if (!strcmp(str_action,"status")) get_action = ACTION_STATUS;
		else if (!strcmp(str_action,"debug")) get_action = ACTION_DEBUG;
		else if (!strcmp(str_action,"dbginfo")) get_action = ACTION_DBGINFO;
		else if (!strcmp(str_action,"update")) get_action = ACTION_UPDATE;
		else str_action = NULL;
	}
	if (!str_action) { str_action = "page"; get_action = ACTION_PAGE; }

	/////////////////////////////////////////////

	// GET CLIENT
	struct camd35_client_data *cli = NULL;
	if (str_id) cli = getcamd35clientbyid( atoi(str_id) );
	if (!cli) return;
	//
	if (get_action==ACTION_DISABLE) {
		cli->flags |= FLAG_DISABLE;
		if (cli->connection.status>0) camd35_disconnect_cli(cli);
		http_send_ok(sock);
		return;
	}
	else if (get_action==ACTION_ENABLE) {
		cli->flags &= ~FLAG_DISABLE;
		http_send_ok(sock);
		return;
	}
	else if (get_action==ACTION_STATUS) {
		if (cli->connection.status>0) http_send_text(sock,"connected"); else http_send_text(sock,"disconnected");
		return;
	}
	else if (get_action==ACTION_DEBUG) {
		flagdebug = getdbgflag( DBG_CAMD35, 0, cli->id);
		http_send_ok(sock);
		return;
	}
	else if (get_action==ACTION_DBGINFO) {
		char dbg[1024];
		sprintf( dbg, "<div class='dbginfo'><b>%s</b> | IP: %s | Status: %s<br>ECM: %d pedidos, %d denied, %d OK | Last ECM: %us ago | Last DCW: %us ago</div>",
			cli->user, (char*)ip2string(cli->ip),
			cli->connection.status>0?"CONNECTED":(cli->connection.status<0?"CONNECTING...":"OFFLINE"),
			cli->ecmnb, cli->ecmdenied, cli->ecmok,
			cli->lastecmtime?(GetTickCount()-cli->lastecmtime)/1000:0,
			cli->lastdcwtime?(GetTickCount()-cli->lastdcwtime)/1000:0);
		http_send_text(sock, dbg);
		return;
	}
	else if (get_action==ACTION_UPDATE) {
/*		char *str = isset_get( req, "expire"); // Client ID
		if (str) {
			if ( (str[4]=='-')&&(str[7]=='-') ) strptime(  str, "%Y-%m-%d %H", &cli->enddate);
			else if ( (str[2]=='-')&&(str[5]=='-') ) strptime(  str, "%d-%m-%Y %H", &cli->enddate);
		}
		str = isset_get( req, "active"); // Client ID
		if (str) {
			if (str[0]=='0') {
				cli->flags |= FLAG_DISABLE;
				if (cli->connection.status>0) camd35_disconnect_cli(cli);
			}
			else cli->flags &= ~FLAG_DISABLE;
		}*/
		http_send_text(sock, "OK");
		return;
	}

	//
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) ); // header tambem no div (XHR exige status line)
	if (get_action==ACTION_PAGE) {

		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, html_title, cfg.http.title, "Cs358x/Camd35 Client"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		// JS
        tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
		tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
		// UPD DIV
		char url[256];
		sprintf( url, "/camd35client?id=%d&action=div", cli->id);
		sprintf( http_buf, HTTP_UPDATE_DIV, cfg.http.autorefresh*1000, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	setautorefresh(autorefresh);\n}");
		tcp_writestr(&tcpbuf, sock, "\n</script>\n");
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
		tcp_write_menu(&tcpbuf, sock,0);
		// DIV
		tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");
	}

	tcp_writestr(&tcpbuf, sock, "<table style=\"padding:0px; margin:0px;\" width=\"100%%\"><tbody>\n" );
	tcp_writestr(&tcpbuf, sock, "<tr><td style=\"vertical-align:top; width:400px;\">\n" );

	tcp_writestr(&tcpbuf, sock, "<table class=infotable><tbody>\n<tr><th colspan=2>Client Informations</th></tr>\n" );
	// NAME
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>User name</td><td class=right>%s</td></tr>\n",cli->user);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#ifdef CHECK_NEXTDCW
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>DCW CHECK</td><td class=right>%s</td></tr>", yesno(cli->dcwcheck) );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
	tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );


	// INFO
	struct client_info_data *info = cli->info;
	if (info) {
		tcp_writestr(&tcpbuf, sock, "<table class=\"infotable\"><tbody>\n" );
		tcp_writestr(&tcpbuf, sock, "<tr><th colspan=2>Additional Informations</th></tr>\n" );
		while (info) {
			snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>%s</td><td class=right>%s</td></tr>\n",info->name,info->value);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			info = info->next;
		}
		tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );
	}

	// Ecm Stat
	tcp_writestr(&tcpbuf, sock, "<table class=\"infotable\"><tbody>\n" );
	tcp_writestr(&tcpbuf, sock, "<tr><th colspan=2>ECM Statistics</th></tr>\n" );
	int ecmaccepted = cli->ecmnb-cli->ecmdenied;
	sprintf( http_buf, "<tr><td class=left>Total ECM requests</td><td class=right>%d</td></tr>\n", cli->ecmnb);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<tr><td class=left>Accepted ECM requests</td><td class=right>%d</td></tr>\n", ecmaccepted);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<tr><td class=left>Good ECM answer</td><td class=right>%d</td></tr>\n", cli->ecmok);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//Ecm Time
	if (cli->ecmok) {
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Average Time</td><td class=right>%d ms</td></tr>\n",(cli->ecmoktime/cli->ecmok) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
//#ifdef SRV_CSCACHE
//	sprintf( http_buf, "<tr><td class=left>Cached CW</td><td class=right>%d</td></tr>\n", cli->cachedcw);
//	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
//#endif
	// Freeze
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Total Freeze</td><td class=right>%d</td></tr>\n", cli->freeze);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );


	tcp_writestr(&tcpbuf, sock, "</td><td style=\"vertical-align:top;\">\n" );

	//Last Used Share
	if ( cli->lastecm.caid ) {
		tcp_writestr(&tcpbuf, sock, "<table class=\"infotable\"><tbody>\n" );
		tcp_writestr(&tcpbuf, sock, "<tr><th>Last Used share</th></tr>\n");
		// Decode Status
		if (cli->lastecm.status)
			snprintf( http_buf, sizeof(http_buf),"<tr><td>Decode success</td></tr>\n");
		else
			snprintf( http_buf, sizeof(http_buf),"<tr><td>Decode failed</td></tr>\n");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// Channel
		snprintf( http_buf, sizeof(http_buf),"<tr><td>Channel %s (%dms) %s</td></tr>\n", getchname(cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid) , cli->lastecm.decodetime, str_laststatus[cli->lastecm.status] );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		// Server
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->lastecm.status) {
				tcp_writestr(&tcpbuf, sock, "<tr><td>From ");
				src2string(cli->lastecm.dcwsrctype, cli->lastecm.dcwsrcid, http_buf );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				tcp_writestr(&tcpbuf, sock, "</td></tr>");
			}
			// Last ECM
			ECM_DATA *ecm = cli->lastecm.request;
			// ECM
			snprintf( http_buf, sizeof(http_buf),"<tr><td>ECM(%d): ", ecm->ecmlen); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			array2hex( ecm->ecm, http_buf, ecm->ecmlen );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf( http_buf,"</td></tr>\n"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			// DCW
			if (cli->lastecm.status) {
				snprintf( http_buf, sizeof(http_buf),"<tr><td>CW: ");	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->cw, http_buf, 16 );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				sprintf( http_buf,"</td></tr>\n"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
#ifdef CHECK_NEXTDCW
			if ( ecm->lastdecode.ecm && (ecm->lastdecode.counter>0) ) {
				snprintf( http_buf, sizeof(http_buf),"<tr><td>Previous CW: "); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->lastdecode.dcw, http_buf, 16 ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				tcp_writestr(&tcpbuf, sock, "</td></tr>\n");
				if (ecm->lastdecode.error) {
					snprintf( http_buf, sizeof(http_buf),"<tr><td>Errors = %d</td></tr>\n", ecm->lastdecode.error);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				snprintf( http_buf, sizeof(http_buf),"<tr><td>Total Cycles = %d</td></tr>\n<tr><td>ECM Interval = %ds</td></tr>\n", ecm->lastdecode.counter, ecm->lastdecode.dcwchangetime/1000);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
#endif
			// Last used share (status do ultimo decode)
			if (cli->lastecm.status==1) {
				tcp_writestr(&tcpbuf, sock, "<tr><td class=success>Decode Success</td></tr>");
			}
			else if (cli->lastecm.status==2) {
				snprintf( http_buf, sizeof(http_buf),"<tr><td class=nok-yellow>channel %s (%dms) NOK (BISS EMU)</td></tr>", getchname(cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid), cli->lastecm.decodetime);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			//
			if (ecm->server[0].srvid) {
				sprintf( http_buf, "<tr><td><table class='infotable'><tbody><tr><th width='30px'>ID</th><th width='250px'>Server</th><th width='50px'>Status</th><th width='70px'>Start time</th><th width='70px'>End time</th><th width='90px'>Elapsed time</th><th>CW</th></tr></tbody>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				int i;
				for(i=0; i<20; i++) {
					if (!ecm->server[i].srvid) break;
					char* str_srvstatus[] = { "WAIT", "OK", "NOK", "BUSY" };
					struct server_data *srv = getsrvbyid(ecm->server[i].srvid);
					if (srv) {
						snprintf( http_buf, sizeof(http_buf),"<tr><td>%d</td><td>%s:%d</td><td>%s</td><td>%dms</td>", i+1, srv->host->name, srv->port, str_srvstatus[ecm->server[i].flag], ecm->server[i].sendtime - ecm->recvtime );
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// Recv Time
						if (ecm->server[i].statustime>ecm->server[i].sendtime)
							sprintf( http_buf,"<td>%dms</td><td>%dms</td>", ecm->server[i].statustime - ecm->recvtime, ecm->server[i].statustime-ecm->server[i].sendtime );
						else
							sprintf( http_buf,"<td>--</td><td>--</td>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// DCW
						if (ecm->server[i].flag==ECM_SRV_REPLY_GOOD) {
							sprintf( http_buf,"<td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							array2hex( ecm->server[i].dcw, http_buf, 16 );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							sprintf( http_buf,"</td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						else {
							sprintf( http_buf,"<td>--</td>");
							tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						sprintf( http_buf,"</tr>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
				}
				tcp_writestr(&tcpbuf, sock, "</tbody></table></td></tr>\n" );
			}
		}
		tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );
	}

	// Current Busy Ecm
	if (cli->ecm.busy) {
		ECM_DATA *ecm = cli->ecm.request;
		if (ecm) http_send_ecmstatus(&tcpbuf, sock, ecm);
	}

	tcp_writestr(&tcpbuf, sock, "</td></tr></tbody></table>" );

	if (get_action==ACTION_PAGE) {
		tcp_writestr(&tcpbuf, sock, "</div>");
		tcp_writestr(&tcpbuf, sock, "</body></html>");
	}
	tcp_flush(&tcpbuf, sock);
}

#endif


















#ifdef CACHEEX

void cacheex_server_cells(struct server_data *srv, char cell[10][2048], int off )
{
	char temp[2048];
	unsigned int ticks = GetTickCount();
	uint32_t d;

	memset(cell, 0, 10*2048);
	if (
		(srv->type!=TYPE_CCCAM)
#ifdef CAMD35_CLI
		&&(srv->type!=TYPE_CAMD35)
#endif
#ifdef CS378X_CLI
		&&(srv->type!=TYPE_CS378X) 
#endif
	) return;
	if (!srv->cacheex_mode) return;
	// CELL0
	sprintf( cell[0],"%s:%d",srv->host->name,srv->port);
	// CELL1*IP
	if (!srv->host->ip && srv->host->clip)
		sprintf( temp,"0.0.0.0 (%s)",(char*)ip2string(srv->host->ip) );
	else {
		char *p = getcountrycodebyip(srv->host->ip);
		if (p) sprintf( temp,"<img src='/flag_%s.gif' title='%s'> %s", p, getcountryname(p), (char*)ip2string(srv->host->ip) );
		else sprintf( temp,"%s",(char*)ip2string(srv->host->ip) );
	}
	strcat( cell[1], temp );
	// CELL2 (assinatura)
	if (srv->connection.status>0) {
		if (srv->type==TYPE_CCCAM) sprintf( cell[2],"Cache EX | Mcs1000 (mode%d CCcam)", srv->cacheex_mode);
#ifdef CAMD35_CLI
		else if (srv->type==TYPE_CAMD35) sprintf( cell[2],"Cache EX | Mcs1000 (mode%d camd35)", srv->cacheex_mode);
#endif
#ifdef CS378X_CLI
		else if (srv->type==TYPE_CS378X) sprintf( cell[2],"Cache EX | Mcs1000 (mode%d cs378x)", srv->cacheex_mode);
#endif
		else sprintf( cell[2],"Cache EX | Mcs1000");
	}
	else sprintf( cell[2]," ");
	// CELL3
	if (srv->connection.status>0)
		sprintf( cell[3],"%02x%02x%02x%02x%02x%02x%02x%02x", srv->nodeid[0],srv->nodeid[1],srv->nodeid[2],srv->nodeid[3],srv->nodeid[4],srv->nodeid[5],srv->nodeid[6],srv->nodeid[7]);
	else sprintf( cell[3]," ");
	// CELL4
	if (srv->connection.status>0) {
		d = (ticks-srv->connection.time)/1000;
		sprintf( cell[4],"%02dd %02d:%02d:%02d", d/(3600*24),(d/3600)%24,(d/60)%60,d%60);
		sprintf( cell[9],"online");
	}
	else {
		sprintf( cell[9],"offline");
		if (srv->flags&FLAG_DELETE) sprintf( cell[4],"Removed");
		else if (srv->flags&FLAG_EXPIRED) sprintf( cell[4],"Expired");
		else if (srv->flags&FLAG_DISABLE) sprintf( cell[4],"Disabled");
		else sprintf( cell[4],"offline");
	}
	if ( (srv->cacheex_mode==2) && srv->csporthit[0].csid ) {
		strcat( cell[4], "<table class=\"connect_data\">" );
		strcat( cell[4], "<tr><td width=150px>Profile</td><td>Hits</td></tr>" );
		int i; char temp[512];
		for(i=0; i<10; i++) {
			if (!srv->csporthit[i].csid) break;
			struct cardserver_data *cs = getcsbyid(srv->csporthit[i].csid);
			if (!cs) continue;
			sprintf( temp,"<tr><td>%s</td><td>%d</td></tr>", cs->name,srv->csporthit[i].hits);
			strcat( cell[4], temp );
		}
		strcat( cell[4], "</table>");
	}

	// CELL5
	sprintf( cell[5], "%d",srv->cacheex.push[0]); // PUSH
	int i;
	for (i=1; i<10; i++ ) {
		if (srv->cacheex.push[i]>0) {
			sprintf( temp,"<br>[%d] %d", i, srv->cacheex.push[i]);
			strcat( cell[5], temp );
		}
	}

	// CELL6
	sprintf( cell[6], "%d",srv->cacheex.got[0]); // GOT
	for (i=1; i<10; i++ ) {
		if (srv->cacheex.got[i]>0) {
			sprintf( temp,"<br>[%d] %d", i, srv->cacheex.got[i]);
			strcat( cell[6], temp );
		}
	}

	// CELL7
	sprintf( cell[7], "%d",srv->cacheex.hits); // HIT
	if ( (srv->cacheex_mode==2) && srv->cacheex.badcw ) {
		sprintf( temp,"<br>bad=%d", srv->cacheex.badcw);
		strcat( cell[7], temp );
	}

	// CELL8
	if (srv->cacheex_mode==3) {
		if (srv->sharelimits[0].caid==0xffff) strcpy( cell[8], " ");
		else {
			sprintf( cell[8]," Shares = %04x:%x", srv->sharelimits[0].caid, srv->sharelimits[0].provid);
			int i;
			for (i=1; i<100; i++) {
				if (srv->sharelimits[i].caid==0xffff) break;
				sprintf( temp,", %04x:%x", srv->sharelimits[i].caid, srv->sharelimits[i].provid);
				strcat( cell[8], temp );
			}
		}
	}
	else if (srv->cacheex_mode==2) {
		if (srv->cacheex.lastcaid) {
			sprintf( cell[8],"ch %s (%dms)", getchname(srv->cacheex.lastcaid, srv->cacheex.lastprov, srv->cacheex.lastsid) , srv->cacheex.lastdecodetime );
		}
		else strcpy( cell[8], " ");
	}
	strcat( cell[8], "<br><span style='display:inline-flex;gap:2px;white-space:nowrap;margin-top:4px;'>");
	if (srv->flags&FLAG_DISABLE) {
		sprintf( temp," <span class='icobtn on' title='Enable' onclick=\"imgrequest('/cacheex?action=enable&id=%d',this);setTimeout('updateDiv()',600)\">ON</span>",srv->id+off);
	}
	else {
		sprintf( temp," <span class='icobtn off' title='Disable' onclick=\"imgrequest('/cacheex?action=disable&id=%d',this);setTimeout('updateDiv()',600)\">OFF</span>",srv->id+off);
	}
	strcat( cell[8], temp );
	sprintf( temp," <span class='icobtn dbg' title='Debug' onclick=\"toggleDbgRow(%d,'/cacheex?action=dbginfo&id=%d')\">DBG</span>",srv->id+off,srv->id+off);
	strcat( cell[8], temp );
	strcat( cell[8], "</span>");
}

void cacheex_cccamclient_cells(struct cc_client_data *cli, char cell[10][2048], int off)
{
	char temp[2048];
	unsigned int ticks = GetTickCount();
	unsigned int d;
	memset(cell, 0, 10*2048);
	if (!cli->cacheex_mode) return;
	// CELL0 # NAME
	sprintf( cell[0],"%s",cli->user);
	// CELL1 # IP
	char *p = getcountrycodebyip(cli->ip);
	if (cli->host)
		if (p) sprintf( cell[1],"<img src='/flag_%s.gif' title='%s'> %s<br>%s", p, getcountryname(p), (char*)ip2string(cli->ip), cli->host->name ); else sprintf( cell[1],"%s<br>%s",(char*)ip2string(cli->ip), cli->host->name );
	else
		if (p) sprintf( cell[1],"<img src='/flag_%s.gif' title='%s'> %s", p, getcountryname(p), (char*)ip2string(cli->ip) ); else sprintf( cell[1],"%s",(char*)ip2string(cli->ip) );

	// CELL2 # VERSION (assinatura)
	sprintf( cell[2],"Cache EX | Mcs1000 (mode%d)", cli->cacheex_mode);
	// CELL3 # nodeid
	if (strlen(cli->version)) sprintf( cell[3],"%02x%02x%02x%02x%02x%02x%02x%02x", cli->nodeid[0],cli->nodeid[1],cli->nodeid[2],cli->nodeid[3],cli->nodeid[4],cli->nodeid[5],cli->nodeid[6],cli->nodeid[7]);
	// CELL4 # Connection Time
	if (cli->connection.status>0) {
		sprintf( cell[9],"online");
		d = (ticks-cli->connection.time)/1000;
		sprintf( cell[4], "%02dd %02d:%02d:%02d", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
	}
	else {
		strcpy( cell[9], "offline" );
		if (cli->flags&FLAG_DELETE) sprintf( cell[4],"Removed");
		else if (cli->flags&FLAG_EXPIRED) sprintf( cell[4],"Expired");
		else if (cli->flags&FLAG_DISABLE) sprintf( cell[4],"Disabled");
		else sprintf( cell[4],"offline");
	}
	if (cli->csporthit[0].csid) {
		strcat( cell[4], "<table class=\"connect_data\">" );
		strcat( cell[4], "<tr><td width=150px>Profile</td><td>Hits</td></tr>" );
		int i; char temp[512];
		for(i=0; i<10; i++) {
			if (!cli->csporthit[i].csid) break;
			struct cardserver_data *cs = getcsbyid(cli->csporthit[i].csid);
			if (!cs) continue;
			sprintf( temp,"<tr><td>%s</td><td>%d</td></tr>", cs->name,cli->csporthit[i].hits);
			strcat( cell[4], temp );
		}
		strcat( cell[4], "</table>");
	}
	// CELL5
	sprintf( cell[5], "%d", cli->cacheex.push[0]);
	int i;
	for (i=1; i<10; i++ ) {
		if (cli->cacheex.push[i]>0) {
			sprintf( temp,"<br>[%d] %d", i, cli->cacheex.push[i]);
			strcat( cell[5], temp );
		}
	}

	// CELL6
	sprintf( cell[6], "%d", cli->cacheex.got[0]);
	for (i=1; i<10; i++ ) {
		if (cli->cacheex.got[i]>0) {
			sprintf( temp,"<br>[%d] %d", i, cli->cacheex.got[i]);
			strcat( cell[6], temp );
		}
	}

	// CELL7
	sprintf( cell[7], "%d", cli->cacheex.hits);
	if ( (cli->cacheex_mode==3) && cli->cacheex.badcw ) {
		sprintf( temp,"<br>bad=%d", cli->cacheex.badcw);
		strcat( cell[7], temp );
	}

	// CELL8
	if (cli->cacheex_mode==2) {
		if (cli->sharelimits[0].caid==0xffff) strcpy( cell[8], " ");
		else {
			sprintf( cell[8]," Shares = %04x:%x", cli->sharelimits[0].caid, cli->sharelimits[0].provid);
			int i;
			for (i=1; i<100; i++) {
				if (cli->sharelimits[i].caid==0xffff) break;
				sprintf( temp,", %04x:%x", cli->sharelimits[i].caid, cli->sharelimits[i].provid);
				strcat( cell[8], temp );
			}
		}
	}
	else if (cli->cacheex_mode==3) {
		if (cli->cacheex.lastcaid) {
			sprintf( cell[8],"ch %s (%dms)", getchname(cli->cacheex.lastcaid, cli->cacheex.lastprov, cli->cacheex.lastsid) , cli->cacheex.lastdecodetime );
		}
		else strcpy( cell[8], " ");
	}
	strcat( cell[8], "<br><span style='display:inline-flex;gap:2px;white-space:nowrap;margin-top:4px;'>");
	if (cli->flags&FLAG_DISABLE) {
		sprintf( temp," <span class='icobtn on' title='Enable' onclick=\"imgrequest('/cacheex?action=enable&id=%d',this);setTimeout('updateDiv()',600)\">ON</span>",cli->id+off);
	}
	else {
		sprintf( temp," <span class='icobtn off' title='Disable' onclick=\"imgrequest('/cacheex?action=disable&id=%d',this);setTimeout('updateDiv()',600)\">OFF</span>",cli->id+off);
	}
	strcat( cell[8], temp );
	sprintf( temp," <span class='icobtn dbg' title='Debug' onclick=\"toggleDbgRow(%d,'/cacheex?action=dbginfo&id=%d')\">DBG</span>",cli->id+off,cli->id+off);
	strcat( cell[8], temp );
	strcat( cell[8], "</span>");
}

#if defined(CAMD35_SRV) || defined(CS378X_SRV)

void cacheex_camd35client_cells(struct camd35_client_data *cli, char cell[10][2048], int off)
{
	char temp[2048];
	unsigned int ticks = GetTickCount();
	unsigned int d;
	memset(cell, 0, 10*2048);
	if (!cli->cacheex_mode) return;
	// CELL0 # NAME
	sprintf( cell[0],"%s",cli->user);
	// CELL1 # IP
	char *p = getcountrycodebyip(cli->ip);
	if (p) sprintf( cell[1],"<img src='/flag_%s.gif' title='%s'> %s", p, getcountryname(p), (char*)ip2string(cli->ip) );
	else sprintf( cell[1],"%s",(char*)ip2string(cli->ip) );

	// CELL2 # VERSION (assinatura)
	sprintf( cell[2],"Cache EX | Mcs1000 (mode%d)", cli->cacheex_mode);
	// CELL3 # nodeid
	sprintf( cell[3],"%02x%02x%02x%02x%02x%02x%02x%02x", cli->nodeid[0],cli->nodeid[1],cli->nodeid[2],cli->nodeid[3],cli->nodeid[4],cli->nodeid[5],cli->nodeid[6],cli->nodeid[7]);
	// CELL4 # Connection Time
	if (cli->connection.status>0) {
		sprintf( cell[9],"online");
		d = (ticks-cli->connection.time)/1000;
		sprintf( cell[4], "%02dd %02d:%02d:%02d", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
	}
	else {
		strcpy( cell[9], "offline" );
		if (cli->flags&FLAG_DELETE) sprintf( cell[4],"Removed");
		else if (cli->flags&FLAG_EXPIRED) sprintf( cell[4],"Expired");
		else if (cli->flags&FLAG_DISABLE) sprintf( cell[4],"Disabled");
		else sprintf( cell[4],"offline");
	}
	if (cli->csporthit[0].csid) {
		strcat( cell[4], "<table class=\"connect_data\">" );
		strcat( cell[4], "<tr><td width=150px>Profile</td><td>Hits</td></tr>" );
		int i; char temp[512];
		for(i=0; i<10; i++) {
			if (!cli->csporthit[i].csid) break;
			struct cardserver_data *cs = getcsbyid(cli->csporthit[i].csid);
			if (!cs) continue;
			sprintf( temp,"<tr><td>%s</td><td>%d</td></tr>", cs->name,cli->csporthit[i].hits);
			strcat( cell[4], temp );
		}
		strcat( cell[4], "</table>");
	}

	// CELL5
	sprintf( cell[5], "%d", cli->cacheex.push[0]); // sent
	int i;
	for (i=1; i<10; i++ ) {
		if (cli->cacheex.push[i]>0) {
			sprintf( temp,"<br>[%d] %d", i, cli->cacheex.push[i]);
			strcat( cell[5], temp );
		}
	}

	// CELL6
	sprintf( cell[6], "%d", cli->cacheex.got[0]); // received
	for (i=1; i<10; i++ ) {
		if (cli->cacheex.got[i]>0) {
			sprintf( temp,"<br>[%d] %d", i, cli->cacheex.got[i]);
			strcat( cell[6], temp );
		}
	}

	// CELL7
	sprintf( cell[7], "%d", cli->cacheex.hits);
	if ( (cli->cacheex_mode==3) && cli->cacheex.badcw ) {
		sprintf( temp,"<br>bad=%d", cli->cacheex.badcw);
		strcat( cell[7], temp );
	}

	// CELL8
	if (cli->cacheex_mode==2) {
		if (cli->sharelimits[0].caid==0xffff) strcpy( cell[8], " ");
		else {
			sprintf( cell[8]," Shares = %04x:%x", cli->sharelimits[0].caid, cli->sharelimits[0].provid);
			int i;
			for (i=1; i<100; i++) {
				if (cli->sharelimits[i].caid==0xffff) break;
				sprintf( temp,", %04x:%x", cli->sharelimits[i].caid, cli->sharelimits[i].provid);
				strcat( cell[8], temp );
			}
		}
	}
	else if (cli->cacheex_mode==3) {
		if (cli->cacheex.lastcaid) {
			sprintf( cell[8],"ch %s (%dms)", getchname(cli->cacheex.lastcaid, cli->cacheex.lastprov, cli->cacheex.lastsid) , cli->cacheex.lastdecodetime );
		}
		else strcpy( cell[8], " ");
	}
	strcat( cell[8], "<br><span style='display:inline-flex;gap:2px;white-space:nowrap;margin-top:4px;'>");
	if (cli->flags&FLAG_DISABLE) {
		sprintf( temp," <span class='icobtn on' title='Enable' onclick=\"imgrequest('/cacheex?action=enable&id=%d',this);setTimeout('updateDiv()',600)\">ON</span>",cli->id+off);
	}
	else {
		sprintf( temp," <span class='icobtn off' title='Disable' onclick=\"imgrequest('/cacheex?action=disable&id=%d',this);setTimeout('updateDiv()',600)\">OFF</span>",cli->id+off);
	}
	strcat( cell[8], temp );
	sprintf( temp," <span class='icobtn dbg' title='Debug' onclick=\"toggleDbgRow(%d,'/cacheex?action=dbginfo&id=%d')\">DBG</span>",cli->id+off,cli->id+off);
	strcat( cell[8], temp );
	strcat( cell[8], "</span>");
}

#endif

void http_send_cacheex(int sock, http_request *req)
{
	char http_buf[4096];
	struct tcp_buffer_data tcpbuf;
	char cell[10][2048];

	// Get Params
	char *str_action = isset_get( req, "action");
	// Param 'action'
	int get_action;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = ACTION_DIV;
		else if (!strcmp(str_action,"disable")) get_action = ACTION_DISABLE;
		else if (!strcmp(str_action,"enable")) get_action = ACTION_ENABLE;
		else if (!strcmp(str_action,"dbginfo")) get_action = 8;
		else str_action = NULL;
	}
	if (!str_action) { str_action = "page"; get_action = ACTION_PAGE; }

	// OFF/ON/DBG por linha (id codificado: 1=cccam, 2=camd35, 3=cs378x, 4=server)
	if ( (get_action==ACTION_DISABLE)||(get_action==ACTION_ENABLE)||(get_action==8) ) {
		char dbg[1024] = "";
		char *sid = isset_get( req, "id");
		if (sid) {
			int enc = atoi(sid);
			int kind = enc>>16;
			int iid = enc&0xffff;
			int on = (get_action==ACTION_ENABLE);
			if (kind==4) {
				struct server_data *srv = cfg.cacheexserver;
				while (srv) { if (!(srv->flags&FLAG_DELETE) && srv->id==iid) break; srv=srv->next; }
				if (srv) {
					if (get_action==8)
						sprintf( dbg, "<div class='dbginfo'><b>%s:%d</b> (id %d) | Mode: %d | Status: %s<br>Push: %d | Got: %d | Hits: %d | Last ch: %04x:%06x:%04x (%dms)</div>",
							srv->host->name, srv->port, srv->id, srv->cacheex_mode,
							(srv->flags&FLAG_DISABLE)?"DISABLED":"ENABLED",
							srv->cacheex.push[0], srv->cacheex.got[0], srv->cacheex.hits,
							srv->cacheex.lastcaid, srv->cacheex.lastprov, srv->cacheex.lastsid, srv->cacheex.lastdecodetime);
					else { if (on) srv->flags &= ~FLAG_DISABLE; else srv->flags |= FLAG_DISABLE; }
				}
			}
			else if (kind==1) {
				struct cccam_server_data *cc = cfg.cccam.server;
				struct cc_client_data *cli = NULL;
				while (cc && !cli) {
					struct cc_client_data *c = cc->cacheexclient;
					while (c) { if (c->id==iid) { cli=c; break; } c=c->next; }
					cc = cc->next;
				}
				if (cli) {
					if (get_action==8)
						sprintf( dbg, "<div class='dbginfo'><b>%s</b> (id %d) | Mode: %d | Status: %s<br>Push: %d | Got: %d | Hits: %d | Last ch: %04x:%06x:%04x (%dms)</div>",
							cli->user, cli->id, cli->cacheex_mode,
							(cli->flags&FLAG_DISABLE)?"DISABLED":"ENABLED",
							cli->cacheex.push[0], cli->cacheex.got[0], cli->cacheex.hits,
							cli->cacheex.lastcaid, cli->cacheex.lastprov, cli->cacheex.lastsid, cli->cacheex.lastdecodetime);
					else { if (on) cli->flags &= ~FLAG_DISABLE; else cli->flags |= FLAG_DISABLE; }
				}
			}
			else if ((kind==2)||(kind==3)) {
				struct camd35_server_data *csx = (kind==2) ? cfg.camd35.server : cfg.cs378x.server;
				struct camd35_client_data *cli = NULL;
				while (csx && !cli) {
					struct camd35_client_data *c = csx->cacheexclient;
					while (c) { if (c->id==iid) { cli=c; break; } c=c->next; }
					csx = csx->next;
				}
				if (cli) {
					if (get_action==8)
						sprintf( dbg, "<div class='dbginfo'><b>%s</b> (id %d) | Mode: %d | Status: %s<br>Push: %d | Got: %d | Hits: %d | Last ch: %04x:%06x:%04x (%dms)</div>",
							cli->user, cli->id, cli->cacheex_mode,
							(cli->flags&FLAG_DISABLE)?"DISABLED":"ENABLED",
							cli->cacheex.push[0], cli->cacheex.got[0], cli->cacheex.hits,
							cli->cacheex.lastcaid, cli->cacheex.lastprov, cli->cacheex.lastsid, cli->cacheex.lastdecodetime);
					else { if (on) cli->flags &= ~FLAG_DISABLE; else cli->flags |= FLAG_DISABLE; }
				}
			}
		}
		if (get_action==8) http_send_text(sock, dbg);
		else http_send_ok(sock);
		return;
	}

	char *id = isset_get( req, "id");
	if (id) {
		int i = atoi(id);
		if ( (i>>16)==1 ) { // CCcam Clients
			struct cccam_server_data *cccam = cfg.cccam.server;
			while (cccam) {
				if (!(cccam->flags&FLAG_DELETE)) {
					struct cc_client_data *cli = cccam->cacheexclient;
					while (cli) {
						if ( !(cli->flags&FLAG_DELETE) && cli->id==(i&0xffff) ) {
			                cacheex_cccamclient_cells(cli,cell,0x10000);
			                for(i=0; i<10; i++) xmlescape( cell[i] );
			                char xmlbuf[5000] = "";
				                snprintf( xmlbuf, sizeof(xmlbuf), "<cacheex>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2>%s</c2>\n<c3>%s</c3>\n<c4_c>%s</c4_c>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6><c7>%s</c7><c8>%s</c8>\n</cacheex>\n",cell[0],cell[1],cell[2],cell[3],cell[9],cell[4],cell[5],cell[6],cell[7],cell[8] );
			                http_send_xml( sock, req, xmlbuf, strlen(xmlbuf));
							return;
						}
						cli = cli->next;
					}
				}
				cccam = cccam->next;
			}
		}
		else if ( (i>>16)==2 ) { // camd35 Clients
			struct camd35_server_data *camd35 = cfg.camd35.server;
			while (camd35) {
				if (!(camd35->flags&FLAG_DELETE)) {
					struct camd35_client_data *cli = camd35->cacheexclient;
					while (cli) {
						if ( !(cli->flags&FLAG_DELETE) && cli->id==(i&0xffff) ) {
			                cacheex_camd35client_cells(cli,cell,0x20000);
			                for(i=0; i<10; i++) xmlescape( cell[i] );
			                char xmlbuf[5000] = "";
				                snprintf( xmlbuf, sizeof(xmlbuf), "<cacheex>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2>%s</c2>\n<c3>%s</c3>\n<c4_c>%s</c4_c>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6><c7>%s</c7><c8>%s</c8>\n</cacheex>\n",cell[0],cell[1],cell[2],cell[3],cell[9],cell[4],cell[5],cell[6],cell[7],cell[8] );
			                http_send_xml( sock, req, xmlbuf, strlen(xmlbuf));
							return;
						}
						cli = cli->next;
					}
				}
				camd35 = camd35->next;
			}
		}
		else if ( (i>>16)==3 ) { // cs378x Clients
			struct camd35_server_data *cs378x = cfg.cs378x.server;
			while (cs378x) {
				if (!(cs378x->flags&FLAG_DELETE)) {
					struct camd35_client_data *cli = cs378x->cacheexclient;
					while (cli) {
						if ( !(cli->flags&FLAG_DELETE) && cli->id==(i&0xffff) ) {
			                cacheex_camd35client_cells(cli,cell,0x30000);
			                for(i=0; i<10; i++) xmlescape( cell[i] );
			                char xmlbuf[5000] = "";
				                snprintf( xmlbuf, sizeof(xmlbuf), "<cacheex>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2>%s</c2>\n<c3>%s</c3>\n<c4_c>%s</c4_c>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6><c7>%s</c7><c8>%s</c8>\n</cacheex>\n",cell[0],cell[1],cell[2],cell[3],cell[9],cell[4],cell[5],cell[6],cell[7],cell[8] );
			                http_send_xml( sock, req, xmlbuf, strlen(xmlbuf));
							return;
						}
						cli = cli->next;
					}
				}
				cs378x = cs378x->next;
			}
		}
		else if ( (i>>16)==4 ) { // Server
			struct server_data *srv = cfg.cacheexserver;
			while (srv) {
				if ( !(srv->flags&FLAG_DELETE) && srv->id==(i&0xffff) ) {
					cacheex_server_cells(srv,cell,0x40000);
					for(i=0; i<10; i++) xmlescape( cell[i] );
					char xmlbuf[5000] = "";
				                snprintf( xmlbuf, sizeof(xmlbuf), "<cacheex>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2>%s</c2>\n<c3>%s</c3>\n<c4_c>%s</c4_c>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6><c7>%s</c7><c8>%s</c8>\n</cacheex>\n",cell[0],cell[1],cell[2],cell[3],cell[9],cell[4],cell[5],cell[6],cell[7],cell[8] );
					http_send_xml( sock, req, xmlbuf, strlen(xmlbuf));
					return;
				}
				srv = srv->next;
			}
		}
		return;
	}

	/////////////////////////////////////////////
	struct cccam_server_data *cccam = NULL;
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	if (get_action==ACTION_PAGE) {
		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, html_title, cfg.http.title, "CacheEX"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		// JS
        tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
		tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
		// ACTIONS REQUEST
		tcp_writestr(&tcpbuf, sock, "\nfunction imgrequest( url, el )\n{\n	var httpRequest;\n	try { httpRequest = new XMLHttpRequest(); }\n	catch (trymicrosoft) { try { httpRequest = new ActiveXObject('Msxml2.XMLHTTP'); } catch (oldermicrosoft) { try { httpRequest = new ActiveXObject('Microsoft.XMLHTTP'); } catch(failed) { httpRequest = false; } } }\n	if (!httpRequest) { alert('Your browser does not support Ajax.'); return false; }\n	if ( typeof(el)!='undefined' ) {\n		el.onclick = null;\n		el.style.opacity = '0.7';\n		httpRequest.onreadystatechange = function()\n		{\n			if (httpRequest.readyState == 4) if (httpRequest.status == 200) el.style.opacity = '0.3';\n		}\n	}\n	httpRequest.open('GET', url, true);\n	httpRequest.send(null);\n}\n");
		// UPD ROW
		tcp_writestr(&tcpbuf, sock, "\nfunction xmlupdateRow( xmlDoc, id )\n{\n	var row = document.getElementById(id);\n	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n	row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n	row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n	row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n	row.cells.item(4).className = xmlDoc.getElementsByTagName('c4_c')[0].childNodes[0].nodeValue;\n	row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n	row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n	row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n	row.cells.item(7).innerHTML = xmlDoc.getElementsByTagName('c7')[0].childNodes[0].nodeValue;\n	row.cells.item(8).innerHTML = xmlDoc.getElementsByTagName('c8')[0].childNodes[0].nodeValue;\n}\n");
		char url[256];
		sprintf( url, "'/cacheex?action=row&id='+idx");
		sprintf( http_buf, HTTP_UPDATE_ROW, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// UPD DIV
		sprintf( url, "/cacheex?action=div");
		sprintf( http_buf, HTTP_UPDATE_DIV, cfg.http.autorefresh*1000, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	setautorefresh(autorefresh);\n}");
		tcp_writestr(&tcpbuf, sock, "\n</script>\n");
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
		tcp_write_menu(&tcpbuf, sock,PAGE_CACHEEX);
		// DIV
		tcp_writestr(&tcpbuf, sock, "\n<div id='mainDiv'>");
	}

	sprintf( http_buf,"<br> * Total Replies = %d</td>",cfg.cacheex.rep); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf,"<br> * Total Hits = %d</td>",cfg.cacheex.hits); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	// TABLE
	tcp_writestr(&tcpbuf,sock, "\n<br><table class=maintable width=100%>");
	tcp_writestr(&tcpbuf,sock, "\n<tr><th width=170px>Name / Host</th><th width=120px>ip</th><th width=80px>Mode</th><th width=100px>NodeID</th><th width=110px>Connected</th><th width=80px>Push</th><th width=80px>Got</th><th width=80px>Hits</th><th>Last CacheEX Hit</th></tr>");
	int alt=0;

	// CCcam Clients
	cccam = cfg.cccam.server;
	while (cccam) {
		// Count Clients
		int counter = 0;
		struct cc_client_data *cli = cccam->cacheexclient;
		while (cli) {
			if (cli->cacheex_mode) counter++;
			cli = cli->next;
		}
		// test
		if (counter) {
			snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=9> CCcam %d (%d)</td></tr>", cccam->id, counter); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			struct cc_client_data *cli = cccam->cacheexclient;
			while (cli) {
				if (cli->cacheex_mode) {
					if (alt==1) alt=2; else alt=1;
					cacheex_cccamclient_cells(cli,cell,0x10000);
					snprintf( http_buf, sizeof(http_buf),"\n<tr class=alt%d id=\"Row%d\" onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",alt,(cli->id+0x10000),(cli->id+0x10000),cell[0],cell[1],cell[2],cell[3],cell[9],cell[4],cell[5],cell[6],cell[7],cell[8]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				cli = cli->next;
			}
		}
		cccam = cccam->next;
	}

#ifdef CAMD35_SRV
	// CAMD35 CLIENTS
	struct camd35_server_data *camd35 = cfg.camd35.server;
	while (camd35) {
		struct camd35_client_data *cli = camd35->cacheexclient;
		// Count Clients
		int counter = 0;
		while (cli) {
			if (cli->cacheex_mode) counter++;
			cli = cli->next;
		}
		if (counter) {
			snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=9> Camd35 %d (%d)</td></tr>", camd35->id, counter); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			cli = camd35->cacheexclient;
			while (cli) {
				if (cli->cacheex_mode) {
					if (alt==1) alt=2; else alt=1;
					cacheex_camd35client_cells(cli,cell,0x20000);
					snprintf( http_buf, sizeof(http_buf),"\n<tr class=alt%d id=\"Row%d\" onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",alt,(cli->id+0x20000),(cli->id+0x20000),cell[0],cell[1],cell[2],cell[3],cell[9],cell[4],cell[5],cell[6],cell[7],cell[8]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				cli = cli->next;
			}
		}
		camd35 = camd35->next;
	}
#endif

#ifdef CS378X_SRV
	// cs378x CLIENTS
	struct camd35_server_data *cs378x = cfg.cs378x.server;
	while (cs378x) {
		struct camd35_client_data *cli = cs378x->cacheexclient;
		// Count Clients
		int counter = 0;
		while (cli) {
			if (cli->cacheex_mode) counter++;
			cli = cli->next;
		}
		if (counter) {
			snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=9> cs378x %d (%d)</td></tr>", cs378x->id, counter); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			cli = cs378x->cacheexclient;
			while (cli) {
				if (cli->cacheex_mode) {
					if (alt==1) alt=2; else alt=1;
					cacheex_camd35client_cells(cli,cell,0x30000);
					snprintf( http_buf, sizeof(http_buf),"\n<tr class=alt%d id=\"Row%d\" onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",alt,(cli->id+0x30000),(cli->id+0x30000),cell[0],cell[1],cell[2],cell[3],cell[9],cell[4],cell[5],cell[6],cell[7],cell[8]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				cli = cli->next;
			}
		}
		cs378x = cs378x->next;
	}
#endif

	// CACHEEX Servers
	int counter = 0;
	struct server_data *srv = cfg.cacheexserver;
	while (srv) {
		if (!(srv->flags&FLAG_DELETE))
		if ( (srv->type==TYPE_CCCAM)
#ifdef CAMD35_CLI
			||(srv->type==TYPE_CAMD35)
#endif
#ifdef CS378X_CLI
			||(srv->type==TYPE_CS378X)
#endif
		)
		if (srv->cacheex_mode) counter++;
		srv = srv->next;
	}
	// test
	if (counter) {
		snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=9> Servers (%d)</td></tr>", counter); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		struct server_data *srv = cfg.cacheexserver;
		while (srv) {
			if (!(srv->flags&FLAG_DELETE))
			if ( (srv->type==TYPE_CCCAM)
#ifdef CAMD35_CLI
				||(srv->type==TYPE_CAMD35)
#endif
#ifdef CS378X_CLI
				||(srv->type==TYPE_CS378X)
#endif
			)
			if (srv->cacheex_mode) {
				if (alt==1) alt=2; else alt=1;
				cacheex_server_cells(srv,cell,0x40000);
				snprintf( http_buf, sizeof(http_buf),"<tr class=alt%d id=\"Row%d\" onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",alt,(srv->id+0x40000),(srv->id+0x40000),cell[0],cell[1],cell[2],cell[3],cell[9],cell[4],cell[5],cell[6],cell[7],cell[8]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			srv = srv->next;
		}
	}

	//
	sprintf( http_buf, "</table>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	//
	if (get_action==ACTION_PAGE) {
		tcp_writestr(&tcpbuf, sock, "</div>");
		tcp_writestr(&tcpbuf, sock, "</body></html>");
	}
	tcp_flush(&tcpbuf, sock);
}

#endif

///////////////////////////////////////////////////////////////////////////////

void http_send_cccam_client(int sock, http_request *req)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;


	// Get Params
	char *str_action = isset_get( req, "action");
	char *str_id = isset_get( req, "id"); // Client ID
	char *str_name = isset_get( req, "name"); // Client NAME
	char *str_srvid = isset_get( req, "srvid"); // CCcam Server ID

	// Param 'action'
	int get_action;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = ACTION_DIV;
#ifndef PUBLIC
		else if (!strcmp(str_action,"xml")) get_action = ACTION_XML; // Get Clients info in xml
#endif
		else if (!strcmp(str_action,"disable")) get_action = ACTION_DISABLE;
		else if (!strcmp(str_action,"enable")) get_action = ACTION_ENABLE;
		else if (!strcmp(str_action,"status")) get_action = ACTION_STATUS;
		else if (!strcmp(str_action,"debug")) get_action = ACTION_DEBUG;
		else if (!strcmp(str_action,"dbginfo")) get_action = ACTION_DBGINFO;
		else if (!strcmp(str_action,"update")) get_action = ACTION_UPDATE;
		else str_action = NULL;
	}
	if (!str_action) { str_action = "page"; get_action = ACTION_PAGE; }

	/////////////////////////////////////////////

	// GET CLIENT
	struct cc_client_data *cli = NULL;
	if (str_id) cli = getcccamclientbyid( atoi(str_id) );
	else if (str_srvid && str_name) {
		struct cccam_server_data *cccam = getcccamserverbyid( atoi(str_srvid) );
		if (cccam) cli = getcccamclientbyname( cccam, str_name );
	}
	if (!cli) return;
	//

	if (get_action==ACTION_XML) {
		tcp_init(&tcpbuf);
		tcp_writestr(&tcpbuf, sock, "HTTP/1.1 200 OK\r\nAccept-Ranges: bytes\r\nConnection: close\r\nContent-Type: application/xml\r\n\r\n");
		tcp_writestr(&tcpbuf, sock, "<client>");
		uint32_t ticks = GetTickCount();
		sprintf(http_buf, "<id>%d</id>", cli->id); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf(http_buf, "<name>%s</name>", cli->user); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		if (cli->connection.status>0) {
			tcp_writestr(&tcpbuf, sock, "<status>1</status>");
			sprintf( http_buf,"<ip>%s</ip>", (char*)ip2string(cli->ip) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			char *p = getcountrycodebyip(cli->ip);
			if (p) sprintf(http_buf, "<country>%s</country>", p); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			uint32_t d = (ticks - cli->connection.time)/1000;
			sprintf(http_buf, "<connected>%02dd %02d:%02d:%02d</connected>", d/(3600*24), (d/3600)%24, (d/60)%60, d%60); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf( http_buf,"<version>%s</version>", cli->version ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf( http_buf,"<busy>%d</busy>", cli->ecm.busy ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			d = (ticks - cli->lastactivity)/1000;
			sprintf(http_buf, "<lastactivity>%02dd %02d:%02d:%02d</lastactivity>", d/(3600*24), (d/3600)%24, (d/60)%60, d%60); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			if ( cli->lastecm.caid ) {
				sprintf(http_buf, "<lastshare>%04x:%06x:%04x</lastshare>",cli->lastecm.caid,cli->lastecm.prov,cli->lastecm.sid); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				sprintf(http_buf, "<lastsharestatus>%d</lastsharestatus>",cli->lastecm.status); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
		else {
			sprintf(http_buf, "<status>%d</status>",cli->flags&0x0E);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		sprintf(http_buf, "<ecmnb>%d</ecmnb>", cli->ecmnb); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf(http_buf, "<ecmaccepted>%d</ecmaccepted>", cli->ecmnb-cli->ecmdenied); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf(http_buf, "<ecmok>%d</ecmok>", cli->ecmok); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_writestr(&tcpbuf, sock, "\n</client>");
		tcp_flush(&tcpbuf, sock);
		return;
	}
	else if (get_action==ACTION_DISABLE) {
		cli->flags |= FLAG_DISABLE;
		if (cli->connection.status>0) cc_disconnect_cli(cli);
		http_send_ok(sock);
		return;
	}
	else if (get_action==ACTION_ENABLE) {
		cli->flags &= ~FLAG_DISABLE;
		http_send_ok(sock);
		return;
	}
	else if (get_action==ACTION_DEBUG) {
		flagdebug = getdbgflag( DBG_CCCAM, cli->parent->id, cli->id);
		http_send_ok(sock);
		return;
	}
	else if (get_action==ACTION_DBGINFO) {
		char dbg[1024];
		sprintf( dbg, "<div class='dbginfo'><b>%s</b> | IP: %s | Status: %s<br>ECM: %d pedidos, %d denied, %d OK | Last ECM: %us ago | Last DCW: %us ago</div>",
			cli->user, (char*)ip2string(cli->ip),
			cli->connection.status>0?"CONNECTED":(cli->connection.status<0?"CONNECTING...":"OFFLINE"),
			cli->ecmnb, cli->ecmdenied, cli->ecmok,
			cli->lastecmtime?(GetTickCount()-cli->lastecmtime)/1000:0,
			cli->lastdcwtime?(GetTickCount()-cli->lastdcwtime)/1000:0);
		http_send_text(sock, dbg);
		return;
	}	else if (get_action==ACTION_STATUS) {
		if (cli->connection.status>0) http_send_text(sock,"connected"); else http_send_text(sock,"disconnected");
		return;
	}
	else if (get_action==ACTION_UPDATE) {
		char *str = isset_get( req, "expire"); // Client ID
		if (str) {
			if ( (str[4]=='-')&&(str[7]=='-') ) strptime(  str, "%Y-%m-%d %H", &cli->enddate);
			else if ( (str[2]=='-')&&(str[5]=='-') ) strptime(  str, "%d-%m-%Y %H", &cli->enddate);
		}
		str = isset_get( req, "active"); // Client ID
		if (str) {
			if (str[0]=='0') {
				cli->flags |= FLAG_DISABLE;
				if (cli->connection.status>0) cc_disconnect_cli(cli);
			}
			else cli->flags &= ~FLAG_DISABLE;
		}
		http_send_text(sock, "OK");
		return;
	}

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	if (get_action==0) {
		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, html_title, cfg.http.title, "CCcam Client"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		// JS
        tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
		tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
		// UPD DIV
		char url[256];
		sprintf( url, "/cccamclient?action=div&id=%d", cli->id);
		sprintf( http_buf, HTTP_UPDATE_DIV, cfg.http.autorefresh*1000, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	setautorefresh(autorefresh);\n}");
		tcp_writestr(&tcpbuf, sock, "\n</script>\n");
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
		tcp_write_menu(&tcpbuf, sock,0);
		// DIV
		tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");
	}

	tcp_writestr(&tcpbuf, sock, "<br><table style=\"padding:0px; margin:0px;\" width=\"100%%\"><tbody>\n" );
	tcp_writestr(&tcpbuf, sock, "<tr><td style=\"vertical-align:top; width:400px;\">\n" );


	tcp_writestr(&tcpbuf, sock, "<table class=infotable><tbody>\n<tr><th colspan=2>Client Informations</th></tr>\n" );
	// NAME
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>User name</td><td class=right>%s</td></tr>\n",cli->user);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// Connection Time
	if (cli->connection.status>0) {
		tcp_writestr(&tcpbuf, sock, "<tr><td class=left>Status</td><td class=right>Connected</td></tr>\n");
		uint32_t d = (GetTickCount()-cli->connection.time)/1000;
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Connection time</td><td class=right>%02dd %02d:%02d:%02d</td></tr>\n", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// IP
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>IP Address</td><td class=right>%s</td></tr>\n",(char*)ip2string(cli->ip) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// fd
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>File Descriptor</td><td class=right>%d</td></tr>\n", cli->handle );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// CCcam Version
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>CCcam Version</td><td class=right>%s</td></tr>",cli->version );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	else {
		tcp_writestr(&tcpbuf, sock, "<tr><td class=left>Status</td><td class=right>Disconnected</td></tr>\n");
		if (cli->connection.lastseen) {
			uint32_t d = (GetTickCount()-cli->connection.lastseen)/1000;
			snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Last Seen</td><td class=right>%02dd %02d:%02d:%02d</td></tr>\n", d/(3600*24),(d/3600)%24,(d/60)%60,d%60);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
	}
	// UPTIME
	if ( cli->connection.uptime || (cli->connection.status>0) ) {
		uint32_t uptime;
		if (cli->connection.status>0) uptime = (GetTickCount()-cli->connection.time)+cli->connection.uptime; else uptime = cli->connection.uptime;
		uptime /= 1000;
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Uptime</td><td class=right>%02dd %02d:%02d:%02d</td></tr>",uptime/(3600*24),(uptime/3600)%24,(uptime/60)%60,uptime%60);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
#ifdef CHECK_NEXTDCW
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>DCW CHECK</td><td class=right>%s</td></tr>", yesno(cli->dcwcheck) );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
/*
	if (cli->option.nodeid[0]) {
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>NodeID</td><td class=right>%lx</td></tr>", cli->option.nodeid);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
*/
	tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );


	// INFO
	struct client_info_data *info = cli->info;
	if (info) {
		tcp_writestr(&tcpbuf, sock, "<table class=\"infotable\"><tbody>\n" );
		tcp_writestr(&tcpbuf, sock, "<tr><th colspan=2>Additional Informations</th></tr>\n" );
		while (info) {
			snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>%s</td><td class=right>%s</td></tr>\n",info->name,info->value);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			info = info->next;
		}
		tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );
	}

	// Ecm Stat
	tcp_writestr(&tcpbuf, sock, "<table class=\"infotable\"><tbody>\n" );
	tcp_writestr(&tcpbuf, sock, "<tr><th colspan=2>ECM Statistics</th></tr>\n" );
	int ecmaccepted = cli->ecmnb-cli->ecmdenied;
	sprintf( http_buf, "<tr><td class=left>Total ECM requests</td><td class=right>%d</td></tr>\n", cli->ecmnb);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<tr><td class=left>Accepted ECM requests</td><td class=right>%d</td></tr>\n", ecmaccepted);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<tr><td class=left>Good ECM answer</td><td class=right>%d</td></tr>\n", cli->ecmok);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//Ecm Time
	if (cli->ecmok) {
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Average Time</td><td class=right>%d ms</td></tr>\n",(cli->ecmoktime/cli->ecmok) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	// Freeze
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Total Freeze</td><td class=right>%d</td></tr>\n", cli->freeze);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Total Zap</td><td class=right>%d</td></tr>\n", cli->zap);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Total Logins</td><td class=right>%d</td></tr>\n", cli->nblogin);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Total Bad Logins</td><td class=right>%d</td></tr>\n", cli->nbloginerror);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Total differents ip logins</td><td class=right>%d</td></tr>\n", cli->nbdiffip);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Total DCW client Errors</td><td class=right>%d</td></tr>\n", cli->nbdcwerr);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );


	tcp_writestr(&tcpbuf, sock, "</td><td style=\"vertical-align:top;\">\n" );

	//Last Used Share
	if ( cli->lastecm.caid ) {
		tcp_writestr(&tcpbuf, sock, "<table class=\"infotable\"><tbody>\n" );
		tcp_writestr(&tcpbuf, sock, "<tr><th>Last Used share</th></tr>\n");
		// Decode Status
		if (cli->lastecm.status)
			snprintf( http_buf, sizeof(http_buf),"<tr><td>Decode success</td></tr>\n");
		else
			snprintf( http_buf, sizeof(http_buf),"<tr><td>Decode failed</td></tr>\n");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// Channel
		snprintf( http_buf, sizeof(http_buf),"<tr><td>Channel %s (%dms) %s</td></tr>\n", getchname(cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid) , cli->lastecm.decodetime, str_laststatus[cli->lastecm.status] );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		// Server
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->lastecm.status) {
				tcp_writestr(&tcpbuf, sock, "<tr><td>From ");
				src2string(cli->lastecm.dcwsrctype, cli->lastecm.dcwsrcid, http_buf );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				tcp_writestr(&tcpbuf, sock, "</td></tr>");
			}
			// Last ECM
			ECM_DATA *ecm = cli->lastecm.request;
			// ECM
			snprintf( http_buf, sizeof(http_buf),"<tr><td>ECM(%d): ", ecm->ecmlen); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			array2hex( ecm->ecm, http_buf, ecm->ecmlen );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf( http_buf,"</td></tr>\n"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			// DCW
			if (cli->lastecm.status) {
				snprintf( http_buf, sizeof(http_buf),"<tr><td>CW: ");	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->cw, http_buf, 16 );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				sprintf( http_buf,"</td></tr>\n"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
#ifdef CHECK_NEXTDCW
			if ( ecm->lastdecode.ecm && (ecm->lastdecode.counter>0) ) {
				snprintf( http_buf, sizeof(http_buf),"<tr><td>Previous CW: "); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->lastdecode.dcw, http_buf, 16 ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				tcp_writestr(&tcpbuf, sock, "</td></tr>\n");
				if (ecm->lastdecode.error) {
					snprintf( http_buf, sizeof(http_buf),"<tr><td>Errors = %d</td></tr>\n", ecm->lastdecode.error);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				snprintf( http_buf, sizeof(http_buf),"<tr><td>Total Cycles = %d</td></tr>\n<tr><td>ECM Interval = %ds</td></tr>\n", ecm->lastdecode.counter, ecm->lastdecode.dcwchangetime/1000);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
#endif

			// Last used share (status do ultimo decode)
			if (cli->lastecm.status==1) {
				tcp_writestr(&tcpbuf, sock, "<tr><td class=success>Decode Success</td></tr>");
			}
			else if (cli->lastecm.status==2) {
				snprintf( http_buf, sizeof(http_buf),"<tr><td class=nok-yellow>channel %s (%dms) NOK (BISS EMU)</td></tr>", getchname(cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid), cli->lastecm.decodetime);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			//
			if (ecm->server[0].srvid) {
				sprintf( http_buf, "<tr><td><table class='infotable'><tbody><tr><th width='30px'>ID</th><th width='250px'>Server</th><th width='50px'>Status</th><th width='70px'>Start time</th><th width='70px'>End time</th><th width='90px'>Elapsed time</th><th>CW</th></tr></tbody>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				int i;
				for(i=0; i<20; i++) {
					if (!ecm->server[i].srvid) break;
					char* str_srvstatus[] = { "WAIT", "OK", "NOK", "BUSY" };
					struct server_data *srv = getsrvbyid(ecm->server[i].srvid);
					if (srv) {
						snprintf( http_buf, sizeof(http_buf),"<tr><td>%d</td><td>%s:%d</td><td>%s</td><td>%dms</td>", i+1, srv->host->name, srv->port, str_srvstatus[ecm->server[i].flag], ecm->server[i].sendtime - ecm->recvtime );
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// Recv Time
						if (ecm->server[i].statustime>ecm->server[i].sendtime)
							sprintf( http_buf,"<td>%dms</td><td>%dms</td>", ecm->server[i].statustime - ecm->recvtime, ecm->server[i].statustime-ecm->server[i].sendtime );
						else
							sprintf( http_buf,"<td>--</td><td>--</td>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// DCW
						if (ecm->server[i].flag==ECM_SRV_REPLY_GOOD) {
							sprintf( http_buf,"<td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							array2hex( ecm->server[i].dcw, http_buf, 16 );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							sprintf( http_buf,"</td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						else {
							sprintf( http_buf,"<td>--</td>");
							tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						sprintf( http_buf,"</tr>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
				}
				tcp_writestr(&tcpbuf, sock, "</tbody></table></td></tr>\n" );
			}
		}
		tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );
	}

	// Current Busy Ecm
	if (cli->ecm.busy) {
		ECM_DATA *ecm = cli->ecm.request;
		if (ecm) http_send_ecmstatus(&tcpbuf, sock, ecm);
	}

	tcp_writestr(&tcpbuf, sock, "</td></tr></tbody></table>" );

	if (get_action==0) {
		tcp_writestr(&tcpbuf, sock, "</div>");
		tcp_writestr(&tcpbuf, sock, "</body></html>");
	}
	tcp_flush(&tcpbuf, sock);
}


#ifdef FREECCCAM_SRV

int freecccam_connectedclients()
{
	int nb=0;
	struct cc_client_data *cli=cfg.freecccam.server.client;
	while (cli) {
		if (cli->connection.status>0) nb++;
		cli=cli->next;
	}
	return nb;
}


void http_send_freecccam(int sock, http_request *req)
{
	char http_buf[4096];
	struct tcp_buffer_data tcpbuf;
	char cell[10][2048];

	char *str_clid = isset_get( req, "clid"); // Client ID
	if (str_clid) {
		int id = atoi(str_clid);
		struct cc_client_data *cli = cfg.freecccam.server.client;
		while (cli) {
			if ( cli->id==id ) {
				// Send XML CELLS
				getcccamcells(cli,cell);
				int i; for(i=0; i<10; i++) xmlescape( cell[i] );
				sprintf( http_buf, "<cccam>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2_c>%s</c2_c>\n<c2>%s</c2>\n<c3>%s</c3>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6>\n<c7>%s</c7>\n</cccam>\n",cell[2],cell[1],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8] );
				http_send_xml( sock, req, http_buf, strlen(http_buf));
				return;
			}			
			cli = cli->next;
		}
		return;
	}

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, html_title, cfg.http.title, "FreeCCcam"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	// JS
    tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
	tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
	// UPD ROW
	tcp_writestr(&tcpbuf, sock, "\nfunction xmlupdateRow( xmlDoc, id )\n{\n	var row = document.getElementById(id);\n	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n	row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n	row.cells.item(2).className = xmlDoc.getElementsByTagName('c2_c')[0].childNodes[0].nodeValue;\n	row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n	row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n	row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n	row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n	row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n	row.cells.item(7).innerHTML = xmlDoc.getElementsByTagName('c7')[0].childNodes[0].nodeValue;\n}\n");
	char url[256];
	sprintf( url, "'/freecccam?action=row&clid='+idx");
	sprintf( http_buf, HTTP_UPDATE_ROW, url);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//
	tcp_writestr(&tcpbuf, sock, "\n</script>\n");

	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
	tcp_write_menu(&tcpbuf, sock,PAGE_FREECCCAM);

	if (cfg.freecccam.server.handle>0) { sprintf( http_buf, "FreeCCcam Server [<font color=#00ff00>ENABLED</font>]");tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) ); }
	else {
		sprintf( http_buf, "FreeCCcam Server [<font color=#ff0000>DISABLED</font>]");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_flush(&tcpbuf, sock);
		return;
	}

	sprintf( http_buf, "<br>Port = %d<br>Connected Clients: %d<br><center><table class=maintable width=100%%><tr><th width=200px>Client ip</th><th width=70px>version</th><th width=110px>connected</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>", cfg.freecccam.server.port, freecccam_connectedclients());
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	int alt=0;
	struct cc_client_data *cli = cfg.freecccam.server.client;

	while (cli) {
		if (cli->connection.status>0) {
			if (alt==1) alt=2; else alt=1;
			getcccamcells( cli,cell);
			snprintf( http_buf, sizeof(http_buf),"\n<tr class=alt%d id=\"Row%d\" onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",alt,cli->id,cli->id,cell[2],cell[1],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
		cli = cli->next;
	}

	sprintf( http_buf, "</table></center>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	tcp_flush(&tcpbuf, sock);
}

#endif

#endif




#ifdef MGCAMD_SRV

void getmgcamdcells(struct mg_client_data *cli, char cell[10][2048])
{
	char temp[2048];
	unsigned int ticks = GetTickCount();
	unsigned int d;

	// CELL0 # NAME
	if (cli->realname)
		sprintf( cell[0],"<a href='/mgcamdclient?id=%d'>%s<br>%s</a>",cli->id,cli->user,cli->realname);
	else
		sprintf( cell[0],"<a href='/mgcamdclient?id=%d'>%s</a>",cli->id,cli->user);
	// CELL1 # PROGRAM ID
	if (cli->connection.status>0)
		sprintf( cell[1],"<span title='%04x'>%s</span>", cli->progid, programid(cli->progid));
	else strcpy( cell[1], " ");
	// CELL2 # IP
	if ( cli->ip ) { // Get Last IP
		char *p = getcountrycodebyip(cli->ip);
		if (cli->host)
			if (p) sprintf( cell[2],"<img src='/flag_%s.gif' title='%s'> %s<br>%s", p, getcountryname(p), (char*)ip2string(cli->ip), cli->host->name ); else sprintf( cell[2],"%s<br>%s",(char*)ip2string(cli->ip), cli->host->name );
		else
			if (p) sprintf( cell[2],"<img src='/flag_%s.gif' title='%s'> %s", p, getcountryname(p), (char*)ip2string(cli->ip) ); else sprintf( cell[2],"%s",(char*)ip2string(cli->ip) );
	}
	else strcpy( cell[2], " ");
	// CELL3 # Connection Time
	if (cli->connection.status>0) {
		if (cli->ecm.busy) sprintf( cell[9],"busy"); else sprintf( cell[9],"online");
		uint32_t d = (GetTickCount()-cli->connection.time)/1000;
		sprintf( cell[3], "%02dd %02d:%02d:%02d", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
	}
	else {
		sprintf( cell[9],"offline");
		if (cli->flags&FLAG_DELETE) sprintf( cell[3],"Removed");
		else if (cli->flags&FLAG_EXPIRED) sprintf( cell[3],"Expired");
		else if (cli->flags&FLAG_DISABLE) sprintf( cell[3],"Disabled");
		else sprintf( cell[3],"offline");
	}
#ifdef EXPIREDATE
	if (cli->enddate.tm_year) {
		sprintf( temp,"<br>Expire: %d-%02d-%02d", 1900+cli->enddate.tm_year, cli->enddate.tm_mon+1, cli->enddate.tm_mday);
		strcat( cell[3], temp );
	}
#endif
	sprintf( temp, "<table class=\"connect_data\"><tr><td>Successful Login: %d</td><td>Aborted Connections: %d</td><td>Total Zapping: %d</td><td>Channel Freeze: %d</td></tr></table>", cli->nblogin, cli->nbloginerror, cli->zap, cli->freeze );
	strcat( cell[3], temp );
	// CELL4+5+6 # ECM STAT: TOTAL/ACCEPTED/OK
	// ECM STAT

#ifdef SRV_CSCACHE
	if (cli->cachedcw) sprintf( cell[4], "%d [%d]", cli->ecmnb, cli->cachedcw); else sprintf( cell[4], "%d", cli->ecmnb );
#else
	sprintf( cell[4], "%d", cli->ecmnb );
#endif

	int ecmaccepted = cli->ecmnb-cli->ecmdenied;
	getstatcell( ecmaccepted, cli->ecmnb, cell[5]);
	getstatcell( cli->ecmok, ecmaccepted, cell[6]);

	// CELL7 # Ecm Time
	if (cli->ecmok) sprintf( cell[7],"%d ms",(cli->ecmoktime/cli->ecmok) ); else sprintf( cell[7],"-- ms");

	// CELL8 # Last Used Share
        if ( cli->connection.status<=0 && cli->connection.lastseen) {
                d = (ticks-cli->connection.lastseen)/1000;
                sprintf( cell[8],"Last Seen %02dd %02d:%02d:%02d", d/(3600*24),(d/3600)%24,(d/60)%60,d%60);
        }
	else if ( cli->lastecm.caid ) {
		if (cli->lastecm.status)  strcpy( cell[8],"<span class=success"); else strcpy( cell[8],"<span class=failed");
		sprintf( temp," title='%04x:%06x:%04x'>ch %s (%dms) %s ",cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid, getchname(cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid) , cli->lastecm.decodetime, str_laststatus[cli->lastecm.status] );
		strcat( cell[8], temp );
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->lastecm.status) {
				strcat( cell[8], " / from ");
				src2string(cli->lastecm.dcwsrctype, cli->lastecm.dcwsrcid, temp);
				strcat( cell[8], temp);
			}
		}
		strcat( cell[8], "</span>" );
	}
	else strcpy( cell[8], " ");

	strcat( cell[8], "<br><span style='display:inline-flex;gap:2px;white-space:nowrap;margin-top:4px;'>");
	if ( !(cli->flags&(FLAG_DELETE|FLAG_EXPIRED)) ) {
		if (cli->flags&FLAG_DISABLE) {
			sprintf( temp," <span class='icobtn on' title='Enable' onclick=\"imgrequest('/mgcamdclient?id=%d&action=enable',this);setTimeout('updateDiv()',600)\">ON</span>",cli->id);
			strcat( cell[8], temp );
		}
		else {
			sprintf( temp," <span class='icobtn off' title='Disable' onclick=\"imgrequest('/mgcamdclient?id=%d&action=disable',this);setTimeout('updateDiv()',600)\">OFF</span>",cli->id);
			strcat( cell[8], temp );
		}
	}
	sprintf( temp," <span class='icobtn dbg' title='Debug' onclick=\"toggleDbgRow(%d,'/mgcamdclient?id=%d&action=dbginfo')\">DBG</span>",cli->id,cli->id);
	strcat( cell[8], temp );
	strcat( cell[8], "</span>");

}


int total_mgcamd_servers()
{
	int count=0;
	struct mgcamdserver_data *srv = cfg.mgcamd.server;
	while (srv) {
		count++;
		srv = srv->next;
	}
	return count;
}	

void mgcamd_clients( struct mgcamdserver_data *mgcamd, int *total, int *connected, int *active )
{
	*total = 0;
	*connected = 0;
	*active = 0;
	struct mg_client_data *cli = mgcamd->client;
	while (cli) {
		(*total)++;
		if (cli->connection.status>0) {
			(*connected)++;
			if ( (GetTickCount()-cli->lastecmtime) < 20000 ) (*active)++;
		}
		cli=cli->next;
	}
}

void total_mgcamd_clients( int *total, int *connected, int *active )
{
	*total = 0;
	*connected = 0;
	*active = 0;
	struct mgcamdserver_data *mgcamd = cfg.mgcamd.server;
	while (mgcamd) {
		struct mg_client_data *cli = mgcamd->client;
		while (cli) {
			(*total)++;
			if (cli->connection.status>0) {
				(*connected)++;
				if ( (GetTickCount()-cli->lastecmtime) < 20000 ) (*active)++;
			}
			cli=cli->next;
		}
		mgcamd = mgcamd->next;
	}
}


void http_send_mgcamd(int sock, http_request *req)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;
	char cell[10][2048];

	// Get Params
	char *str_action = isset_get( req, "action");
	char *str_list = isset_get( req, "list");
	char *str_id = isset_get( req, "id"); // server ID
	char *str_clid = isset_get( req, "clid"); // Client ID
#ifndef PUBLIC
	char *str_clname = isset_get( req, "clname"); // Client NAME
#endif
	// Param 'action'
	int get_action;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = ACTION_DIV;
		else if (!strcmp(str_action,"row")) get_action = ACTION_ROW;
#ifndef PUBLIC
		else if (!strcmp(str_action,"xml")) get_action = ACTION_XML; // Get Clients info in xml
#endif
		else if (!strcmp(str_action,"disable")) get_action = ACTION_DISABLE;
		else if (!strcmp(str_action,"enable")) get_action = ACTION_ENABLE;
		else if (!strcmp(str_action,"status")) get_action = ACTION_STATUS;
		else if (!strcmp(str_action,"debug")) get_action = ACTION_DEBUG;
		else str_action = NULL;
	}
	if (!str_action) { str_action = "page"; get_action = ACTION_PAGE; }

	/////////////////////////////////////////////

	if (get_action==ACTION_ROW) {
		// Check for XML ROW
		struct mg_client_data *cli = NULL;
		if (str_clid) {
			cli = getmgcamdclientbyid( atoi(str_clid) );
			if (!cli) return;
		}
#ifndef PUBLIC
		else {
			if (str_id && str_clname) {
				struct mgcamdserver_data *mgcamd = getmgcamdserverbyid( atoi(str_id) );
				if (!mgcamd) return;
				cli = getmgcamdclientbyname( mgcamd, str_clname );
				if (!cli) return;
			}
			else return;
		}
#else
		else return;
#endif
		// Send XML CELLS
		getmgcamdcells(cli,cell);
		int i; for(i=0; i<10; i++) xmlescape( cell[i] );
		char buf[5000] = "";
		snprintf( buf, sizeof(buf), "<mgcamd>\n<c0>%s</c0>\n<c1>%s</c1>\n<c2>%s</c2>\n<c3_c>%s</c3_c>\n<c3>%s</c3>\n<c4>%s</c4>\n<c5>%s</c5>\n<c6>%s</c6>\n<c7>%s</c7>\n<c8>%s</c8>\n</mgcamd>\n",cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8] );
		http_send_xml( sock, req, buf, strlen(buf));
		return;
	}			
	else if (get_action==ACTION_XML) {
		struct mgcamdserver_data *mgcamd = NULL;
		if (str_id) mgcamd = getmgcamdserverbyid( atoi(str_id) );
		tcp_init(&tcpbuf);
		tcp_writestr(&tcpbuf, sock, "HTTP/1.1 200 OK\r\nAccept-Ranges: bytes\r\nConnection: close\r\nContent-Type: application/xml\r\n\r\n");

		tcp_writestr(&tcpbuf, sock, "<multics>");

		struct mgcamdserver_data *srv;
		if (mgcamd) srv = mgcamd; else srv = cfg.mgcamd.server;
		while (srv) {
			tcp_writestr(&tcpbuf, sock, "\n<mgcamd>");
			sprintf(http_buf, "<id>%d</id>", srv->id); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf(http_buf, "<port>%d</port>", srv->port); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf(http_buf, "<status>%d</status>", (srv->handle>0) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			uint32_t ticks = GetTickCount();
			struct mg_client_data *cli = srv->client;
			while (cli) {
				tcp_writestr(&tcpbuf, sock, "<user>");
				sprintf(http_buf, "<name>%s</name>", cli->user); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				if (cli->connection.status>0) {
					tcp_writestr(&tcpbuf, sock, "<status>1</status>");
					sprintf( http_buf,"<ip>%s</ip>", (char*)ip2string(cli->ip) ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					char *p = getcountrycodebyip(cli->ip);
					if (p) sprintf(http_buf, "<country>%s</country>", p); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					uint32_t d = (ticks - cli->connection.time)/1000;
					sprintf(http_buf, "<connected>%02dd %02d:%02d:%02d</connected>", d/(3600*24), (d/3600)%24, (d/60)%60, d%60); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				else {
					sprintf(http_buf, "<status>%d</status>",cli->flags&0x0E);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				tcp_writestr(&tcpbuf, sock, "</user>");
				cli = cli->next;
			}
			tcp_writestr(&tcpbuf, sock, "\n</mgcamd>");

			if (mgcamd) break; else srv = srv->next;
		}
		tcp_writestr(&tcpbuf, sock, "\n</multics>");
		tcp_flush(&tcpbuf, sock);
		return;
	}


	// Param 'id'
	int get_id = 0;
	if (str_id)	get_id = atoi(str_id);
	// Param 'list'
	int get_list = LIST_ALL;
	if (str_list) {
		if (!strcmp(str_list,"connected")) get_list = LIST_CONNECTED;
		else if (!strcmp(str_list,"all")) get_list = LIST_ALL;
		else str_list = NULL;
	}
	if (!str_list) str_list = "all";
	//
	struct mgcamdserver_data *mgcamd = NULL;
	if (get_id) {
		mgcamd = getmgcamdserverbyid(get_id);
		if (!mgcamd) return;
	}

	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) ); // header tambem no div (XHR exige status line)
	if (get_action==ACTION_PAGE) {

		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, html_title, cfg.http.title, "MGcamd"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		// JS
        tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
		tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
		// ACTIONS REQUEST
		tcp_writestr(&tcpbuf, sock, "\nfunction imgrequest( url, el )\n{\n	var httpRequest;\n	try { httpRequest = new XMLHttpRequest(); }\n	catch (trymicrosoft) { try { httpRequest = new ActiveXObject('Msxml2.XMLHTTP'); } catch (oldermicrosoft) { try { httpRequest = new ActiveXObject('Microsoft.XMLHTTP'); } catch(failed) { httpRequest = false; } } }\n	if (!httpRequest) { alert('Your browser does not support Ajax.'); return false; }\n	if ( typeof(el)!='undefined' ) {\n		el.onclick = null;\n		el.style.opacity = '0.7';\n		httpRequest.onreadystatechange = function()\n		{\n			if (httpRequest.readyState == 4) if (httpRequest.status == 200) el.style.opacity = '0.3';\n		}\n	}\n	httpRequest.open('GET', url, true);\n	httpRequest.send(null);\n}\n");
		// UPD ROW
		tcp_writestr(&tcpbuf, sock, "\nfunction xmlupdateRow( xmlDoc, id )\n{\n	var row = document.getElementById(id);\n	row.cells.item(0).innerHTML = xmlDoc.getElementsByTagName('c0')[0].childNodes[0].nodeValue;\n	row.cells.item(1).innerHTML = xmlDoc.getElementsByTagName('c1')[0].childNodes[0].nodeValue;\n	row.cells.item(2).innerHTML = xmlDoc.getElementsByTagName('c2')[0].childNodes[0].nodeValue;\n	row.cells.item(3).className = xmlDoc.getElementsByTagName('c3_c')[0].childNodes[0].nodeValue;\n	row.cells.item(3).innerHTML = xmlDoc.getElementsByTagName('c3')[0].childNodes[0].nodeValue;\n	row.cells.item(4).innerHTML = xmlDoc.getElementsByTagName('c4')[0].childNodes[0].nodeValue;\n	row.cells.item(5).innerHTML = xmlDoc.getElementsByTagName('c5')[0].childNodes[0].nodeValue;\n	row.cells.item(6).innerHTML = xmlDoc.getElementsByTagName('c6')[0].childNodes[0].nodeValue;\n	row.cells.item(7).innerHTML = xmlDoc.getElementsByTagName('c7')[0].childNodes[0].nodeValue;\n	row.cells.item(8).innerHTML = xmlDoc.getElementsByTagName('c8')[0].childNodes[0].nodeValue;\n}\n");
		char url[256];
		sprintf( url, "'/mgcamd?action=row&clid='+idx");
		sprintf( http_buf, HTTP_UPDATE_ROW, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// UPD DIV
		sprintf( url, "/mgcamd?action=div&id=%d&list=%s", get_id, str_list);
		sprintf( http_buf, HTTP_UPDATE_DIV, cfg.http.autorefresh*1000, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	setautorefresh(autorefresh);\n}");
		tcp_writestr(&tcpbuf, sock, "\n</script>\n");
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
		tcp_write_menu(&tcpbuf, sock,PAGE_MGCAMD);
		// Info de servidores (acima da div principal)
		{
			tcp_writestr(&tcpbuf, sock, "<div style='margin:12px 12px 0 12px'><div class=stat-section style='margin:0'>");
			sprintf( http_buf, "<h3 class=stitle>Mgcamd Servers (%d)</h3>", total_mgcamd_servers());
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			tcp_writestr(&tcpbuf, sock, "<table class=maintable><tr><th>Server</th><th>Port</th><th>Status</th><th>Connected</th></tr>");
			int itotal, iconnected, iactive;
			total_mgcamd_clients( &itotal, &iconnected, &iactive );
			sprintf( http_buf, "<tr><td class=left>TOTAL</td><td class=right>-</td><td class=right>-</td><td class=right>%d / %d</td></tr>", iconnected, itotal);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			struct mgcamdserver_data *box = cfg.mgcamd.server;
			while ( box ) {
				int btotal, bconnected, bactive;
				mgcamd_clients( box, &btotal, &bconnected, &bactive );
				if (box->handle>0) sprintf( http_buf, "<tr><td class=left><a href='/mgcamd?id=%d'>mgcamd %d</a></td><td class=right>%d</td><td class=right><span class=success>ONLINE</span></td><td class=right>%d / %d</td></tr>", box->id, box->id, box->port, bconnected, btotal);
				else sprintf( http_buf, "<tr><td class=left><a href='/mgcamd?id=%d'>mgcamd %d</a></td><td class=right>%d</td><td class=right><span class=failed>OFFLINE</span></td><td class=right>%d / %d</td></tr>", box->id, box->id, box->port, bconnected, btotal);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				box = box->next;
			}
			tcp_writestr(&tcpbuf, sock, "</table></div></div>");
		}
		// DIV
		tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");
	}

	int total, connected, active;
	tcp_writestr(&tcpbuf, sock, "<select style=\"width:200px;\" onchange=\"parent.location.href='/mgcamd?id='+this.value\">");
	sprintf( http_buf, "<option value=0>ALL (%d)</option>", total_mgcamd_servers());
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	struct mgcamdserver_data *tmp = cfg.mgcamd.server;
	while (tmp) {
		if (get_id==tmp->id) sprintf( http_buf, "<option value=%d selected>[%d] mgcamd %d</option>",tmp->id,tmp->port, tmp->id );
		else sprintf( http_buf, "<option value=%d>[%d] mgcamd %d</option>",tmp->id,tmp->port, tmp->id );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tmp = tmp->next;
	}
	tcp_writestr(&tcpbuf, sock, "</select> ");
	//
	if (mgcamd) mgcamd_clients( mgcamd, &total, &connected, &active ); else total_mgcamd_clients( &total, &connected, &active );
	char *class1 = "button"; char *class2 = "sbutton";
	char *class;
	if (get_list==LIST_ACTIVE) class = class2; else class = class1;
	sprintf( http_buf, "<input type=button class=%s onclick=\"parent.location='/mgcamd?id=%d&amp;list=active'\" value='Active Clients (%d)'>", class, get_id, active);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (get_list==LIST_CONNECTED) class = class2; else class = class1;
	sprintf( http_buf, " <input type=button class=%s onclick=\"parent.location='/mgcamd?id=%d&amp;list=connected'\" value='Connected Clients (%d)'>", class, get_id, connected);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (get_list==LIST_ALL) class = class2; else class = class1;
	sprintf( http_buf, " <input type=button class=%s onclick=\"parent.location='/mgcamd?id=%d&amp;list=all'\" value='All Clients (%d)'>", class, get_id, total);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//
	if (get_id) { // One Server Selected
		// Table
		sprintf( http_buf, "\n<table class=maintable width=100%%><tr><th width=100px>Client</th><th width=70px>version</th><th width=120px>ip</th><th width=110px>Connected</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		struct mg_client_data *cli = mgcamd->client;
		int alt=0;
		if (get_list==LIST_ACTIVE) {
			while (cli) {
				if ( (cli->connection.status>0)&&((GetTickCount()-cli->lastecmtime) < 20000) ) {
					if (alt==1) alt=2; else alt=1;
					getmgcamdcells(cli,cell);
					snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				cli = cli->next;
			}
		}
		else if (get_list==LIST_CONNECTED) {
			while (cli) {
				if (cli->connection.status>0) {
					if (alt==1) alt=2; else alt=1;
					getmgcamdcells(cli,cell);
					snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				cli = cli->next;
			}
		}
		else { // ALL
			while (cli) {
				if (alt==1) alt=2; else alt=1;
				getmgcamdcells(cli,cell);
				snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				cli = cli->next;
			}
		}
		sprintf( http_buf, "\n</table>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}

	else {
		// Table
		tcp_writestr(&tcpbuf,sock, "\n<table class=maintable width=100%>");
		tcp_writestr(&tcpbuf,sock, "\n<tr><th width=100px>Client</th><th width=70px>version</th><th width=120px>ip</th><th width=110px>Connected</th><th width=60px>TotalEcm</th><th width=90px>AcceptedEcm</th><th width=90px>EcmOK</th><th width=50px>EcmTime</th><th>Last used share</th></tr>");
		int alt=0;
		mgcamd = cfg.mgcamd.server;
		while (mgcamd) {
			int total, connected, active;
			mgcamd_clients( mgcamd, &total, &connected, &active );
			if ( (get_list==LIST_ACTIVE) && active ) {
				snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=9> mgcamd %d (%d)</td></tr>", mgcamd->id, active); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				struct mg_client_data *cli = mgcamd->client;
				while (cli) {
					if ( (cli->connection.status>0)&&((GetTickCount()-cli->lastecmtime) < 20000) ) {
						if (alt==1) alt=2; else alt=1;
						getmgcamdcells(cli,cell);
						snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
					cli = cli->next;
				}
			}
			else if ( (get_list==LIST_ALL) && total ) {
				snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=9> mgcamd %d (%d)</td></tr>", mgcamd->id, total); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				struct mg_client_data *cli = mgcamd->client;
				while (cli) {
					if (alt==1) alt=2; else alt=1;
					getmgcamdcells(cli,cell);
					snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					cli = cli->next;
				}
			}
			else if ( (get_list==LIST_CONNECTED) && connected ) {
				snprintf( http_buf, sizeof(http_buf),"\n<tr><td class=alt3 colspan=9> mgcamd %d (%d)</td></tr>", mgcamd->id, connected); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				struct mg_client_data *cli = mgcamd->client;
				while (cli) {
					if (cli->connection.status>0) {
						if (alt==1) alt=2; else alt=1;
						getmgcamdcells(cli,cell);
						snprintf( http_buf, sizeof(http_buf),"\n<tr id=\"Row%d\" class=alt%d onMouseOver='setupdateRow(%d)' onMouseOut='setupdateRow(0)'> <td>%s</td><td>%s</td><td>%s</td><td class=\"%s\">%s</td><td align=center>%s</td><td>%s</td><td>%s</td><td align=center>%s</td><td>%s</td></tr>\n",cli->id,alt,cli->id,cell[0],cell[1],cell[2],cell[9],cell[3],cell[4],cell[5],cell[6],cell[7],cell[8]);
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
					cli = cli->next;
				}
			}
			mgcamd = mgcamd->next;
		}
		sprintf( http_buf, "</table>");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	if (get_action==ACTION_PAGE) {
		tcp_writestr(&tcpbuf, sock, "</div>");
		tcp_writestr(&tcpbuf, sock, "</body></html>");
	}

	tcp_flush(&tcpbuf, sock);
}


///////////////////////////////////////////////////////////////////////////////

void http_send_mgcamd_client(int sock, http_request *req)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;

	// Get Params
	char *str_action = isset_get( req, "action");
	char *str_id = isset_get( req, "id"); // Client ID
	char *str_name = isset_get( req, "name"); // Client NAME
	char *str_srvid = isset_get( req, "srvid"); // CCcam Server ID

	// Action
	int get_action = ACTION_PAGE;
	if (str_action) {
		if (!strcmp(str_action,"div")) get_action = ACTION_DIV;
		else if (!strcmp(str_action,"row")) get_action = ACTION_ROW;
		else if (!strcmp(str_action,"disable")) get_action = ACTION_DISABLE;
		else if (!strcmp(str_action,"enable")) get_action = ACTION_ENABLE;
		else if (!strcmp(str_action,"status")) get_action = ACTION_STATUS;
		else if (!strcmp(str_action,"debug")) get_action = ACTION_DEBUG;
		else if (!strcmp(str_action,"dbginfo")) get_action = ACTION_DBGINFO;
		else if (!strcmp(str_action,"update")) get_action = ACTION_UPDATE;
		else str_action = NULL;
	}
	if (!str_action) { str_action = "page"; get_action = ACTION_PAGE; }

	/////////////////////////////////////////////

	// GET CLIENT
	struct mg_client_data *cli = NULL;
	if (str_id) cli = getmgcamdclientbyid( atoi(str_id) );
	else if (str_srvid && str_name) {
		struct mgcamdserver_data *mgcamd = getmgcamdserverbyid( atoi(str_srvid) );
		if (mgcamd) cli = getmgcamdclientbyname( mgcamd, str_name );
	}
	if (!cli) return;
	//
	if (get_action==ACTION_DISABLE) {
		cli->flags |= FLAG_DISABLE;
		if (cli->connection.status>0) mg_disconnect_cli(cli);
		http_send_ok(sock);
		return;
	}
	else if (get_action==ACTION_ENABLE) {
		cli->flags &= ~FLAG_DISABLE;
		http_send_ok(sock);
		return;
	}
	else if (get_action==ACTION_STATUS) {
		if (cli->connection.status>0) http_send_text(sock,"connected"); else http_send_text(sock,"disconnected");
		return;
	}
	else if (get_action==ACTION_DEBUG) {
		flagdebug = getdbgflag( DBG_MGCAMD, 0, cli->id);
		http_send_ok(sock);
		return;
	}
	else if (get_action==ACTION_DBGINFO) {
		char dbg[1024];
		sprintf( dbg, "<div class='dbginfo'><b>%s</b> | IP: %s | Status: %s<br>ECM: %d pedidos, %d denied, %d OK | Last ECM: %us ago | Last DCW: %us ago</div>",
			cli->user, (char*)ip2string(cli->ip),
			cli->connection.status>0?"CONNECTED":(cli->connection.status<0?"CONNECTING...":"OFFLINE"),
			cli->ecmnb, cli->ecmdenied, cli->ecmok,
			cli->lastecmtime?(GetTickCount()-cli->lastecmtime)/1000:0,
			cli->lastdcwtime?(GetTickCount()-cli->lastdcwtime)/1000:0);
		http_send_text(sock, dbg);
		return;
	}
	else if (get_action==ACTION_UPDATE) {
		char *str = isset_get( req, "expire"); // Client ID
		if (str) {
			if ( (str[4]=='-')&&(str[7]=='-') ) strptime(  str, "%Y-%m-%d %H", &cli->enddate);
			else if ( (str[2]=='-')&&(str[5]=='-') ) strptime(  str, "%d-%m-%Y %H", &cli->enddate);
		}
		str = isset_get( req, "active"); // Client ID
		if (str) {
			if (str[0]=='0') {
				cli->flags |= FLAG_DISABLE;
				if (cli->connection.status>0) mg_disconnect_cli(cli);
			}
			else cli->flags &= ~FLAG_DISABLE;
		}
		http_send_text(sock, "OK");
		return;
	}

	//
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) ); // header tambem no div (XHR exige status line)
	if (get_action==ACTION_PAGE) {

		tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
		tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
		sprintf( http_buf, html_title, cfg.http.title, "Mgcamd Client"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
		tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
		// JS
        tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
		tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
		// UPD DIV
		char url[256];
		sprintf( url, "/mgcamdclient?id=%d&action=div", cli->id);
		sprintf( http_buf, HTTP_UPDATE_DIV, cfg.http.autorefresh*1000, url);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		//
		tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	setautorefresh(autorefresh);\n}");
		tcp_writestr(&tcpbuf, sock, "\n</script>\n");
		tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
		tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
		tcp_write_menu(&tcpbuf, sock,0);
		// DIV
		tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");
	}

	tcp_writestr(&tcpbuf, sock, "<table style=\"padding:0px; margin:0px;\" width=\"100%%\"><tbody>\n" );
	tcp_writestr(&tcpbuf, sock, "<tr><td style=\"vertical-align:top; width:400px;\">\n" );

	tcp_writestr(&tcpbuf, sock, "<table class=infotable><tbody>\n<tr><th colspan=2>Client Informations</th></tr>\n" );
	// NAME
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>User name</td><td class=right>%s</td></tr>\n",cli->user);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	// Connection Time
	if (cli->connection.status>0) {
		tcp_writestr(&tcpbuf, sock, "<tr><td class=left>Status</td><td class=right>Connected</td></tr>\n");
		uint32_t d = (GetTickCount()-cli->connection.time)/1000;
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Connection time</td><td class=right>%02dd %02d:%02d:%02d</td></tr>\n", d/(3600*24), (d/3600)%24, (d/60)%60, d%60);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// IP
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>IP Address</td><td class=right>%s</td></tr>\n",(char*)ip2string(cli->ip) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// Program ID
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Client Program</td><td class=right>%s(%04x)</td></tr>",programid(cli->progid), cli->progid );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	else {
		tcp_writestr(&tcpbuf, sock, "<tr><td class=left>Status</td><td class=right>Disconnected</td></tr>\n");
		if ( cli->connection.lastseen ) {
			uint32_t d = (GetTickCount()-cli->connection.lastseen)/1000;
			snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Last Seen</td><td class=right>%02dd %02d:%02d:%02d</td></tr>\n", d/(3600*24),(d/3600)%24,(d/60)%60,d%60);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		}
	}
	// UPTIME
	if ( cli->connection.uptime || (cli->connection.status>0) ) {
		uint32_t uptime;
		if (cli->connection.status>0) uptime = (GetTickCount()-cli->connection.time)+cli->connection.uptime; else uptime = cli->connection.uptime;
		uptime /= 1000;
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Uptime</td><td class=right>%02dd %02d:%02d:%02d</td></tr>",uptime/(3600*24),(uptime/3600)%24,(uptime/60)%60,uptime%60);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
#ifdef CHECK_NEXTDCW
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>DCW CHECK</td><td class=right>%s</td></tr>", yesno(cli->dcwcheck) );
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
	tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );


	// INFO
	struct client_info_data *info = cli->info;
	if (info) {
		tcp_writestr(&tcpbuf, sock, "<table class=\"infotable\"><tbody>\n" );
		tcp_writestr(&tcpbuf, sock, "<tr><th colspan=2>Additional Informations</th></tr>\n" );
		while (info) {
			snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>%s</td><td class=right>%s</td></tr>\n",info->name,info->value);
			tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			info = info->next;
		}
		tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );
	}

	// Ecm Stat
	tcp_writestr(&tcpbuf, sock, "<table class=\"infotable\"><tbody>\n" );
	tcp_writestr(&tcpbuf, sock, "<tr><th colspan=2>ECM Statistics</th></tr>\n" );
	int ecmaccepted = cli->ecmnb-cli->ecmdenied;
	sprintf( http_buf, "<tr><td class=left>Total ECM requests</td><td class=right>%d</td></tr>\n", cli->ecmnb);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<tr><td class=left>Accepted ECM requests</td><td class=right>%d</td></tr>\n", ecmaccepted);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	sprintf( http_buf, "<tr><td class=left>Good ECM answer</td><td class=right>%d</td></tr>\n", cli->ecmok);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	//Ecm Time
	if (cli->ecmok) {
		snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Average Time</td><td class=right>%d ms</td></tr>\n",(cli->ecmoktime/cli->ecmok) );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
#ifdef SRV_CSCACHE
	sprintf( http_buf, "<tr><td class=left>Cached CW</td><td class=right>%d</td></tr>\n", cli->cachedcw);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
	// Freeze
	snprintf( http_buf, sizeof(http_buf),"<tr><td class=left>Total Freeze</td><td class=right>%d</td></tr>\n", cli->freeze);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );


	tcp_writestr(&tcpbuf, sock, "</td><td style=\"vertical-align:top;\">\n" );

	//Last Used Share
	if ( cli->lastecm.caid ) {
		tcp_writestr(&tcpbuf, sock, "<table class=\"infotable\"><tbody>\n" );
		tcp_writestr(&tcpbuf, sock, "<tr><th>Last Used share</th></tr>\n");
		// Decode Status
		if (cli->lastecm.status)
			snprintf( http_buf, sizeof(http_buf),"<tr><td>Decode success</td></tr>\n");
		else
			snprintf( http_buf, sizeof(http_buf),"<tr><td>Decode failed</td></tr>\n");
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		// Channel
		snprintf( http_buf, sizeof(http_buf),"<tr><td>Channel %s (%dms) %s</td></tr>\n", getchname(cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid) , cli->lastecm.decodetime, str_laststatus[cli->lastecm.status] );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		// Server
		if ( (GetTickCount()-cli->ecm.recvtime) < 20000 ) {
			// From ???
			if (cli->lastecm.status) {
				tcp_writestr(&tcpbuf, sock, "<tr><td>From ");
				src2string(cli->lastecm.dcwsrctype, cli->lastecm.dcwsrcid, http_buf );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				tcp_writestr(&tcpbuf, sock, "</td></tr>");
			}
			// Last ECM
			ECM_DATA *ecm = cli->lastecm.request;
			// ECM
			snprintf( http_buf, sizeof(http_buf),"<tr><td>ECM(%d): ", ecm->ecmlen); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			array2hex( ecm->ecm, http_buf, ecm->ecmlen );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			sprintf( http_buf,"</td></tr>\n"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			// DCW
			if (cli->lastecm.status) {
				snprintf( http_buf, sizeof(http_buf),"<tr><td>CW: ");	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->cw, http_buf, 16 );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				sprintf( http_buf,"</td></tr>\n"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
#ifdef CHECK_NEXTDCW
			if ( ecm->lastdecode.ecm && (ecm->lastdecode.counter>0) ) {
				snprintf( http_buf, sizeof(http_buf),"<tr><td>Previous CW: "); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				array2hex( ecm->lastdecode.dcw, http_buf, 16 ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				tcp_writestr(&tcpbuf, sock, "</td></tr>\n");
				if (ecm->lastdecode.error) {
					snprintf( http_buf, sizeof(http_buf),"<tr><td>Errors = %d</td></tr>\n", ecm->lastdecode.error);
					tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				}
				snprintf( http_buf, sizeof(http_buf),"<tr><td>Total Cycles = %d</td></tr>\n<tr><td>ECM Interval = %ds</td></tr>\n", ecm->lastdecode.counter, ecm->lastdecode.dcwchangetime/1000);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
#endif
			// Last used share (status do ultimo decode)
			if (cli->lastecm.status==1) {
				tcp_writestr(&tcpbuf, sock, "<tr><td class=success>Decode Success</td></tr>");
			}
			else if (cli->lastecm.status==2) {
				snprintf( http_buf, sizeof(http_buf),"<tr><td class=nok-yellow>channel %s (%dms) NOK (BISS EMU)</td></tr>", getchname(cli->lastecm.caid, cli->lastecm.prov, cli->lastecm.sid), cli->lastecm.decodetime);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			//
			if (ecm->server[0].srvid) {
				sprintf( http_buf, "<tr><td><table class='infotable'><tbody><tr><th width='30px'>ID</th><th width='250px'>Server</th><th width='50px'>Status</th><th width='70px'>Start time</th><th width='70px'>End time</th><th width='90px'>Elapsed time</th><th>CW</th></tr></tbody>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
				int i;
				for(i=0; i<20; i++) {
					if (!ecm->server[i].srvid) break;
					char* str_srvstatus[] = { "WAIT", "OK", "NOK", "BUSY" };
					struct server_data *srv = getsrvbyid(ecm->server[i].srvid);
					if (srv) {
						snprintf( http_buf, sizeof(http_buf),"<tr><td>%d</td><td>%s:%d</td><td>%s</td><td>%dms</td>", i+1, srv->host->name, srv->port, str_srvstatus[ecm->server[i].flag], ecm->server[i].sendtime - ecm->recvtime );
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// Recv Time
						if (ecm->server[i].statustime>ecm->server[i].sendtime)
							sprintf( http_buf,"<td>%dms</td><td>%dms</td>", ecm->server[i].statustime - ecm->recvtime, ecm->server[i].statustime-ecm->server[i].sendtime );
						else
							sprintf( http_buf,"<td>--</td><td>--</td>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						// DCW
						if (ecm->server[i].flag==ECM_SRV_REPLY_GOOD) {
							sprintf( http_buf,"<td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							array2hex( ecm->server[i].dcw, http_buf, 16 );	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
							sprintf( http_buf,"</td>"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						else {
							sprintf( http_buf,"<td>--</td>");
							tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
						}
						sprintf( http_buf,"</tr>");
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
				}
				tcp_writestr(&tcpbuf, sock, "</tbody></table></td></tr>\n" );
			}
		}
		tcp_writestr(&tcpbuf, sock, "</tbody></table><br>\n" );
	}

	// Current Busy Ecm
	if (cli->ecm.busy) {
		ECM_DATA *ecm = cli->ecm.request;
		if (ecm) http_send_ecmstatus(&tcpbuf, sock, ecm);
	}

	tcp_writestr(&tcpbuf, sock, "</td></tr></tbody></table>" );

	if (get_action==ACTION_PAGE) {
		tcp_writestr(&tcpbuf, sock, "</div>");
		tcp_writestr(&tcpbuf, sock, "</body></html>");
	}
	tcp_flush(&tcpbuf, sock);
}


#endif




#ifdef TESTCHANNEL

// grava/remove a linha TESTCHANNEL: no multics.cfg (escrita atomica)
void testchannel_cfg_save(uint16_t caid, uint32_t prid, uint16_t sid);

void http_send_testchannel(int sock, http_request *req){

	char *caid = isset_get( req, "caid");
	char *sid = isset_get( req, "sid");
	char *prid = isset_get( req, "prid");
	char *action = isset_get( req, "action");

	if (action && !strcmp(action,"off")) {
		cfg.testchn.caid = 0;
		cfg.testchn.provid = 0;
		cfg.testchn.sid = 0;
		testchannel_cfg_save(0,0,0);
	}
	else if (action && !strcmp(action,"save") && caid && prid && sid) {
		cfg.testchn.caid = hex2int( caid );
		cfg.testchn.provid = hex2int( prid );
		cfg.testchn.sid = hex2int( sid );
		testchannel_cfg_save(cfg.testchn.caid, cfg.testchn.provid, cfg.testchn.sid);
	}
	else if (caid && sid && prid) {
		cfg.testchn.caid = hex2int( caid );
		cfg.testchn.provid = hex2int( prid );
		cfg.testchn.sid = hex2int( sid );
		unlink( debug_file );
	}
	
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, html_title, cfg.http.title, "Test Channel"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
	tcp_write_menu(&tcpbuf, sock,0);

	tcp_writestr(&tcpbuf, sock, "<div style='margin:10px'>");
	sprintf( http_buf, "<h3 class=stitle>Test Channel</h3><br>CAID = %04X<br>PROVIDER = %06X<br>SID = %04X<br><br>\n", cfg.testchn.caid, cfg.testchn.provid, cfg.testchn.sid);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_writestr(&tcpbuf, sock, "<p style='font-size:12px;'>Define o canal de teste (CAID:PROVIDER:SID). Os pedidos deste canal sao registados no Debug Log com o detalhe completo (ECMs recebidos, CWs, falhas com o motivo exato).</p>");
	tcp_writestr(&tcpbuf, sock, "<form method='GET' action='/testchannel'>");
	tcp_writestr(&tcpbuf, sock, "CAID: <input type='text' name='caid' size='5' placeholder='1802'>&nbsp;");
	tcp_writestr(&tcpbuf, sock, "PROVIDER: <input type='text' name='prid' size='7' placeholder='000000'>&nbsp;");
	tcp_writestr(&tcpbuf, sock, "SID: <input type='text' name='sid' size='5' placeholder='03F9'>&nbsp;");
	tcp_writestr(&tcpbuf, sock, "<input type='submit' value='Ativar (so nesta sessao)'>");
	tcp_writestr(&tcpbuf, sock, "</form><br>");
	tcp_writestr(&tcpbuf, sock, "<form method='GET' action='/testchannel'>");
	tcp_writestr(&tcpbuf, sock, "CAID: <input type='text' name='caid' size='5' placeholder='1802'>&nbsp;");
	tcp_writestr(&tcpbuf, sock, "PROVIDER: <input type='text' name='prid' size='7' placeholder='000000'>&nbsp;");
	tcp_writestr(&tcpbuf, sock, "SID: <input type='text' name='sid' size='5' placeholder='03F9'>&nbsp;");
	tcp_writestr(&tcpbuf, sock, "<input type='hidden' name='action' value='save'>");
	tcp_writestr(&tcpbuf, sock, "<input type='submit' class='sbutton' value='Gravar no multics.cfg (fica apos restart)'>");
	tcp_writestr(&tcpbuf, sock, "</form><br>");
	tcp_writestr(&tcpbuf, sock, "<a class='sbutton' href='/testchannel?action=off'>Desativar</a>");
	tcp_writestr(&tcpbuf, sock, "</div>");

	tcp_flush(&tcpbuf, sock);
}

// grava/remove a linha TESTCHANNEL: no multics.cfg (escrita atomica)
void testchannel_cfg_save(uint16_t caid, uint32_t prid, uint16_t sid)
{
	char fname[512];
	strcpy(fname, config_file);
	FILE *in = fopen(fname, "r");
	if (!in) return;
	char tmpf[520];
	sprintf(tmpf, "%s.tmpfix", fname);
	FILE *out = fopen(tmpf, "w");
	if (!out) { fclose(in); return; }
	char line[1024];
	int done = 0;
	while (fgets(line, sizeof(line), in)) {
		if (!strncmp(line, "TESTCHANNEL", 11)) { // remove linhas antigas
			if (!done && caid) {
				fprintf(out, "TESTCHANNEL: %04X:%06X:%04X\n", caid, prid, sid);
				done = 1;
			}
			continue;
		}
		fputs(line, out);
	}
	if (caid && !done) fprintf(out, "TESTCHANNEL: %04X:%06X:%04X\n", caid, prid, sid);
	fclose(in);
	fclose(out);
	rename(tmpf, fname);
}

// ---------------- PACKAGES (dashboard por satelite/pacote) ----------------
struct pkg_data {
	const char *sat;
	const char *name;
	uint16_t caid;
	uint32_t ident;
	const char *note;
};
static const struct pkg_data pkg_table[] = {
	{ "Hispasat 30W", "Abertis TDT (BISS)", 0x2600, 0x000000, "chaves no Softcam.cfg" },
	{ "Hispasat 30W", "MEO", 0x1814, 0x005211, "ident real" },
	{ "Hispasat 30W", "MEO", 0x1814, 0x005221, "" },
	{ "Hispasat 30W", "MEO", 0x1814, 0x000007, "ID_SAT" },
	{ "Hispasat 30W", "NOS", 0x1802, 0x000000, "wildcard" },
	{ "Hispasat 30W", "NOS", 0x1802, 0x004801, "ident real" },
	{ "Hotbird 13E", "Canal+ Polska", 0x1813, 0x000068, "tunel Seca/Nagra" },
	{ "Hotbird 13E", "Canal+ Polska", 0x1884, 0x000000, "Cayman" },
	{ "Hotbird 13E", "Polsat Box", 0x1803, 0x000000, "" },
	{ "Hotbird 13E", "Polsat Box", 0x186C, 0x000000, "Merlin" },
	{ "Hotbird 13E", "Tivusat", 0x1856, 0x000000, "4K" },
	{ "Hotbird 13E", "Tivusat", 0x183E, 0x000000, "HD" },
	{ "Hotbird 13E", "Tivusat", 0x183D, 0x000000, "SD" },
	{ "Hotbird 13E", "SRG SSR", 0x0500, 0x060200, "" },
	{ "Hotbird 13E", "Bis TV", 0x0500, 0x042830, "" },
	{ "Hotbird 13E", "Adultos", 0x0500, 0x051E00, "" },
	{ "Astra 19.2E", "Movistar+", 0x1810, 0x000000, "" },
	{ "Astra 19.2E", "Movistar+", 0x1810, 0x004001, "Seca tunel" },
	{ "Astra 19.2E", "Canal+ France", 0x1811, 0x003311, "Nagra Merlin" },
	{ "Astra 19.2E", "Canal+ France", 0x0500, 0x032830, "Viaccess" },
	{ "Astra 19.2E", "TNT SAT", 0x1818, 0x000000, "TNTSAT 7" },
	{ "Astra 19.2E", "TNT SAT", 0x0500, 0x030B00, "Viaccess" },
	{ "Astra 19.2E", "Sky DE", 0x098D, 0x000000, "V15" },
	{ "Astra 19.2E", "Sky DE", 0x098C, 0x000000, "V14" },
	{ "Astra 19.2E", "HD+", 0x1830, 0x000000, "HD01" },
	{ "Astra 19.2E", "HD+", 0x1843, 0x000000, "HD02" },
	{ "Astra 19.2E", "HD+", 0x1860, 0x000000, "HD03" },
	{ "Astra 19.2E", "HD+", 0x186A, 0x003411, "HD04" },
	{ "Astra 19.2E", "HD+", 0x188A, 0x000000, "HD05" },
	{ "Astra 19.2E", "Canal Digitaal", 0x1817, 0x000000, "Nagra Merlin" },
	{ "Astra 19.2E", "Canal Digitaal", 0x0100, 0x00006A, "Seca3" },
	{ "Astra 19.2E", "Canal Digitaal", 0x0500, 0x051900, "Viaccess" },
	{ "Astra 19.2E", "TV Vlaanderen", 0x181D, 0x000000, "" },
	{ "Astra 19.2E", "Austriasat", 0x0624, 0x000000, "Irdeto2" },
	{ "Astra 19.2E", "Austriasat", 0x098C, 0x000002, "NDS" },
};

// perfil que aceita este caid:ident (procura exata como getcsbycaidprov)
static struct cardserver_data *pkg_findcs(uint16_t caid, uint32_t ident)
{
	struct cardserver_data *cs = cfg.cardserver;
	while (cs) {
		if (cs->card.caid==caid) {
			int i;
			for (i=0; i<cs->card.nbprov; i++)
				if (cs->card.prov[i].id==ident) return cs;
			if ( (caid>=0x1800 && caid<=0x19FF) || (caid>=0x0900 && caid<=0x09FF) || (caid>=0x0B00 && caid<=0x0BFF) ) {
				if (ident==0) return cs;
			}
			if ( (caid!=0x0100) && (caid!=0x0500) && ident==0 ) return cs;
		}
		cs = cs->next;
	}
	return NULL;
}

// readers com card compativel com este caid:ident
static int pkg_count_readers(uint16_t caid, uint32_t ident, char *out, int outsz)
{
	int n = 0;
	out[0] = 0;
	struct server_data *srv = cfg.server;
	while (srv) {
		if ( IS_DISABLED(srv->flags) ) { srv = srv->next; continue; }
		struct cs_card_data *card = srv->card;
		while (card) {
			if (card->caid==caid) {
				int match = 0;
				if (ident==0) {
					if ( (caid!=0x0100) && (caid!=0x0500) ) match = 1;
					else {
						int i;
						for (i=0; i<card->nbprov; i++) if (card->prov[i]==ident) { match = 1; break; }
					}
				}
				else {
					int i;
					for (i=0; i<card->nbprov; i++) if (card->prov[i]==ident) { match = 1; break; }
					if (!match && card->nbprov==1 && card->prov[0]==0) match = 1; // card "todos"
				}
				if (match) {
					n++;
					if ( (int)strlen(out)+strlen(srv->host->name)+12 < outsz )
						sprintf(out+strlen(out), "%s%s:%d", n>1?", ":"", srv->host->name, srv->port);
					break;
				}
			}
			card = card->next;
		}
		srv = srv->next;
	}
	return n;
}

void http_send_packages(int sock, http_request *req)
{
	char http_buf[4096];
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, html_title, cfg.http.title, "Packages"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
	tcp_write_menu(&tcpbuf, sock, PAGE_PACKAGES);
	tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");

	int np = (int)(sizeof(pkg_table)/sizeof(pkg_table[0]));
	const char *lastsat = "";
	int i, alt = 0;
	for (i=0; i<np; i++) {
		if (strcmp(pkg_table[i].sat, lastsat)) {
			if (lastsat[0]) tcp_writestr(&tcpbuf, sock, "</table><br>");
			lastsat = pkg_table[i].sat;
			sprintf( http_buf, "<h3 class=stitle>%s</h3>", lastsat);
			tcp_writestr(&tcpbuf, sock, http_buf);
			tcp_writestr(&tcpbuf, sock, "<table class=maintable width=100%><tr><th width=160px>Package</th><th width=120px>CAID:Ident</th><th>Nota</th><th width=110px>Perfil</th><th width=90px>Ecm OK</th><th width=170px>Filtros</th><th>Readers</th></tr>");
		}
		if (alt==1) alt=2; else alt=1;

		struct cardserver_data *cs = pkg_findcs(pkg_table[i].caid, pkg_table[i].ident);
		char profcell[160];
		char okcell[80];
		char filtcell[320];
		if (cs) {
			sprintf( profcell, "<a href='/profile?id=%d'>%s</a> (%d)", cs->id, cs->name, cs->newcamd.port);
			if (cs->ecmaccepted) sprintf( okcell, "%d%%", (cs->ecmok*100)/cs->ecmaccepted);
			else sprintf( okcell, "--");
			char b[256] = "";
			if (cs->option.dcw.cak7) strcat(b, " <span class='badge-blue'>CAK7</span>");
			if (cs->option.dcwfilter.enable) {
				if (cs->option.dcwfilter.mode==2) strcat(b, cs->option.dcwfilter.auto_active?" <span class='badge-green'>CWPK ATIVO</span>":" <span class='badge-gray'>CWPK AUTO</span>");
				else if (cs->option.dcwfilter.mode==1) strcat(b, " <span class='badge-green'>CWPK DROP</span>");
				else strcat(b, " <span class='badge-gray'>CWPK LOGONLY</span>");
				if (cs->option.dcwfilter.learn) strcat(b, " <span class='badge-blue'>LEARN</span>");
			}
			if (cs->option.ecmfilter.enable) strcat(b, cs->option.ecmfilter.mode?" <span class='badge-green'>ECM DROP</span>":" <span class='badge-gray'>ECM LOGONLY</span>");
			sprintf( filtcell, "%s", b[0]?b:" <span class='badge-gray'>sem filtros</span>");
		}
		else { sprintf( profcell, "<span style='color:#8899aa'>sem perfil</span>"); sprintf( okcell, "--"); sprintf( filtcell, " <span class='badge-gray'>--</span>"); }

		char rdcell[512];
		pkg_count_readers(pkg_table[i].caid, pkg_table[i].ident, rdcell, sizeof(rdcell));

		sprintf( http_buf, "<tr class=alt%d><td>%s</td><td><b>%04X:</b> %06X</td><td style='font-size:11px;color:#8899aa'>%s</td><td style='font-size:12px;'>%s</td><td>%s</td><td style='font-size:11px;'>%s</td><td style='font-size:12px;'>%s</td></tr>",
			alt, pkg_table[i].name, pkg_table[i].caid, pkg_table[i].ident, pkg_table[i].note, profcell, okcell, filtcell, rdcell);
		tcp_writestr(&tcpbuf, sock, http_buf);
	}
	tcp_writestr(&tcpbuf, sock, "</table>");
	tcp_writestr(&tcpbuf, sock, "\n</div></body></html>");
	tcp_flush(&tcpbuf, sock);
}

#endif

void http_send_config(int sock, http_request *req)
{
	char *type = isset_get( req, "type");

	if ( type && !strcmp(type,"delay") ) {
		char *str = isset_get( req, "thread");
		if (str) {
			cfg.delay.thread = atoi( str );
		}
		str = isset_get( req, "connect");
		if (str) {
			cfg.delay.connect = atoi( str );
		}
	}

	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_flush(&tcpbuf, sock);
}

void http_send_host(int sock, http_request *req)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
	sprintf( http_buf, html_title, cfg.http.title, "Host"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
	tcp_write_menu(&tcpbuf, sock,0);

	sprintf( http_buf, "<table class=maintable width=100%%><tr><th width=200px>HostName</th><th width=70px>IP</th><th width=100px>Check Time (sec)</th></tr>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	struct host_data *host = cfg.host;
	int alt=0;
	while (host) {
		if (alt==1) alt=2; else alt=1;
		snprintf( http_buf, sizeof(http_buf),"<tr class=alt%d><td>%s</td><td>%s</td><td>%d</td>",alt, host->name, (char*)ip2string(host->ip), host->checkiptime-getseconds() );
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		host = host->next;
	}

	sprintf( http_buf, "</table>");
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

	tcp_flush(&tcpbuf, sock);
}


void http_send_threads(int sock, http_request *req)
{
        char http_buf[2048];
        struct tcp_buffer_data tcpbuf;
        tcp_init(&tcpbuf);
        tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
        tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
        tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );
        sprintf( http_buf, html_title, cfg.http.title, "threads"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
        tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
        tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
        tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
        tcp_write(&tcpbuf, sock, http_body, strlen(http_body) );
		tcp_write_menu(&tcpbuf, sock,0);


		tcp_writestr(&tcpbuf, sock, "<br>Load Average = ");
		FILE *fp = fopen ("/proc/loadavg", "r");
		fgets(http_buf, sizeof(http_buf), fp);
		fclose(fp);
		tcp_writestr(&tcpbuf, sock, http_buf);

		// /proc/meminfo 
		fp = fopen ("/proc/meminfo", "r");
		fgets(http_buf, sizeof(http_buf), fp); tcp_writestr(&tcpbuf, sock, http_buf);
		fgets(http_buf, sizeof(http_buf), fp); tcp_writestr(&tcpbuf, sock, http_buf);
		fclose(fp);

		sprintf( http_buf, "<br>THREADID Main = %d",prg.pid_main ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf, "<br>THREADID Config = %d",prg.pid_cfg ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf, "<br> THREADID DNS = %d",prg.pid_dns ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf, "<br> THREADID Servers Connections = %d",prg.pid_srv ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf, "<br> THREADID Recv Messages = %d",prg.pid_msg ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf, "<br> THREADID SET DCW = %d",prg.pid_setdcw ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf, "<br> THREADID Cache = %d",prg.pid_cache ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf, "<br> THREADID Date = %d",prg.pid_date ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		sprintf( http_buf, "<br> THREADID Connect Clients = %d",prg.pid_connect ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		sprintf( http_buf, "<br> THREADID Newcamd messages = %d",prg.pid_cs_msg ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf, "<br> THREADID Mgcamd messages = %d",cfg.mgcamd.pid_recvmsg ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		sprintf( http_buf, "<br> THREADID CCcam messages = %d",cfg.cccam.pid_recvmsg ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#ifdef CS378X_SRV
		sprintf( http_buf, "<br> THREADID CS378X messages = %d",cfg.cs378x.pid_recvmsg ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif
#ifdef CACHEEX
        sprintf( http_buf, "<br> THREADID Ccacheex messages = %d",prg.pid_ccex_msg ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
#endif

		// CACHEEX Servers
#ifdef CACHEEX
		struct server_data *srv = cfg.server;
		while (srv) {
			if ( !(srv->flags&FLAG_DELETE) && (srv->cacheex_mode==2) ) {
		        sprintf( http_buf, "<br> THREADID Cacheex Server (%s:%d) =  %d", srv->host->name, srv->port, srv->pid );
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			srv = srv->next;
		}
#endif
		sprintf( http_buf, "<br><br> TOTAL ECM REQUESTS = %d",totalecm ); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
		tcp_flush(&tcpbuf, sock);
}




#include "bmsearch.c"

static int find_tool(const char *name, char *out, int outsz);
static void resolve_cfg_path(const char *name, char *out, int outsz);

void http_send_editor(int sock, http_request *req, int index)
{
	char http_buf[2048];
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );

	// ===== ACTIONS (ajax) =====
	char *str_action = isset_get( req, "action");
	if (str_action && !strcmp(str_action,"reloadchinfo")) {
		read_chinfo( &cfg );
		mlogf(LOGINFO, DBG_HTTP, " http: channelinfo reloaded from disk\n");
		http_send_text(sock, "<span class='success'>OK</span>");
		return;
	}
	if (str_action && !strcmp(str_action,"reread")) {
		free_filenames( &cfg );
		reread_config( &cfg );
		check_config( &cfg );
		cfg_set_id_counters( &cfg );
		emu_load();
		lite_load();
		ipblock_load();
		mlogf(LOGINFO, DBG_HTTP, " http: config reread from disk\n");
		http_send_text(sock, "<span class='success'>OK</span>");
		return;
	}
	if (str_action && !strcmp(str_action,"updatechinfo")) {
		static uint32_t lastupdatechinfo = 0;
		uint32_t now = GetTickCount();
		if (lastupdatechinfo && ((now-lastupdatechinfo)<300000)) {
			http_send_text(sock, "<span class='miss'>Aguarda 5 minutos entre atualizacoes</span>");
			return;
		}
		lastupdatechinfo = now;
		char tool[512];
		char chinfo[512];
		if (find_tool("tools_update_channelinfo.py", tool, sizeof(tool))) {
			resolve_cfg_path("CCcam.channelinfo", chinfo, sizeof(chinfo));
			sprintf( http_buf, "python3 %s --apply --out \"%s\" >/var/tmp/chinfo_update.log 2>&1 &", tool, chinfo);
			system(http_buf);
			mlogf(LOGINFO, DBG_HTTP, " http: channelinfo update iniciado (KingOfSat) -> %s\n", chinfo);
			http_send_text(sock, "<span class='success'>OK</span>");
		}
		else http_send_text(sock, "<span class='miss'>Ferramenta nao encontrada (tools_update_channelinfo.py)</span>");
		return;
	}

	sprintf( http_buf, html_title, cfg.http.title, "Editor"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_writestr(&tcpbuf, sock, "<body onload=\"document.getElementById('submitbutton').disabled=false;\">");
	tcp_write_menu(&tcpbuf, sock,PAGE_EDITOR);
	//
	int i;
	struct filename_data *fs = cfg.files;
	for( i =0; i<index; i++) {
		if (!fs) break;
		fs = fs->next;
	}
	if ( (i!=index)||(!fs) ) return;
	char fname[512];
	strcpy( fname, fs->name );
	int noeditor = fs->noeditor;
	//
	if ( (req->type==HTTP_POST) && !noeditor ) {
		// Check Content-Type
		char *content = isset_header(req, "Content-Type");
		if (!content) {
			mlogf(LOGERROR,getdbgflag(DBG_HTTP,0,0)," Invalid form\n");
			return;
		}
		// Parse Content-type
		if ( memcmp(content,"multipart/form-data",19) ) {
			mlogf(LOGERROR,getdbgflag(DBG_HTTP,0,0)," Invalid Content-type\n");
			return;
		}
		// Get ';'
		while (*content!=';') {
			if (*content==0)  {
				mlogf(LOGERROR,getdbgflag(DBG_HTTP,0,0)," Invalid header data\n");
				return;
			}
			content++;
		}
		content++;
		// Skip Spaces
		while (*content==' ') content++;
		// Get Boundry
		if ( memcmp(content,"boundary",8) ) {
			mlogf(LOGERROR,getdbgflag(DBG_HTTP,0,0)," Invalid Content-type\n");
			return;
		}
		// Get '='
		while (*content!='=') {
			if (*content==0)  {
				mlogf(LOGERROR,getdbgflag(DBG_HTTP,0,0)," Invalid header data\n");
				return;
			}
			content++;
		}
		content++;
		// Skip Spaces
		while (*content==' ') content++;
		// Get Boundary Value
		char boundary[255];
		char endboundary[255];
		sprintf( boundary, "--%s", content);
		sprintf( endboundary, "\r\n--%s", content);

		// search for boundary in file
		content = req->dbf.data;

		content = (char*) boyermoore_horspool_memmem( (uint8_t*)content, req->dbf.datasize, (uint8_t*)boundary, strlen(boundary) );
		if (content) {
			content += strlen(boundary);
			if ( *content=='\r' && *(content+1)=='\n' ) {
				content+=2;
				// Get Content-Disposition
				// Content-Disposition: form-data; name="textedit"
				char *p = content;
				while (*p!='\r') p++;
				if ( *p=='\r' && *(p+1)=='\n' && *(p+2)=='\r' && *(p+3)=='\n' ) { // Good
					*p=0;
					char *pdata = p+4;
					// search for next boundary
					char *end = (char*)boyermoore_horspool_memmem( (uint8_t*)pdata, req->dbf.datasize-(pdata-(char*)req->dbf.data), (uint8_t*)endboundary, strlen(endboundary) );
					if (end && end>pdata) {
						*end = 0;
						// save
						FILE *cfgfd = fopen( fname, "w");
						if (!cfgfd) {
							sprintf( http_buf, "<h2>Error opening file '%s'</h2>", fname);
						}
						else {
							int k;
							for (k=0;k<end-pdata;k++)
							{
								if ( ( *(pdata+k)=='\r' ) && ( (k+1) < (end-pdata) ) ) {
									if ( *(pdata+k+1) =='\n' ) {
										k++;
									}
								}
								fwrite( pdata+k,1,1,cfgfd);
							}
							fclose(cfgfd);
							sprintf( http_buf, "<script type=\"text/JavaScript\"><!--\nsetTimeout(\"location.href = '/editor%d';\",3000);\n--></script>\n<h3><center>file '%s' is Successfully Saved</center></h3>", index, fname);
						}
						tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
					}
				}
			}
		}
		tcp_flush(&tcpbuf, sock);
		// aplicar alteracoes imediatamente (sem restart)
		mlogf(LOGINFO, DBG_HTTP, " http: config saved '%s' - reloading config...\n", fname);
		free_filenames( &cfg );
		reread_config( &cfg );
		check_config( &cfg );
		cfg_set_id_counters( &cfg );
		emu_load();
		lite_load();
		ipblock_load();
	}
	else {

		tcp_writestr(&tcpbuf, sock, "<div class=stat-section style='margin:10px 0'> <input type=button class='sbutton' value='Load Channel Info' title='Rele o /var/etc/CCcam.channelinfo do disco (o teu ficheiro proprio)' onclick=\"imgrequest('/editor?action=reloadchinfo',this)\">&nbsp;<span style='font-size:11px;'>parse do teu CCcam.channelinfo sem restart</span><br><input type=button class='sbutton' value='Update Channel Info' title='Atualiza o CCcam.channelinfo do KingOfSat (so feeds ativos)' onclick=\"imgrequest('/editor?action=updatechinfo',this)\">&nbsp;<span style='font-size:11px;'>reconstroi CCcam.channelinfo do KingOfSat + reload automatico</span></div>");

		tcp_writestr(&tcpbuf, sock, "<form enctype=\"multipart/form-data\" method=\"post\">");

		tcp_writestr(&tcpbuf, sock, "<span style='float:right'><select onchange=\"window.location=this.value\" style='width:250px;'>");
		struct filename_data *fs = cfg.files;
		int i =0;
		while (fs) {
			if (!fs->noeditor) {
				if (i==index) sprintf( http_buf, "<option value=\"/editor%d\" selected>%s</option>",i, fs->name);
				else sprintf( http_buf, "<option value=\"/editor%d\">%s</option>",i, fs->name);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			i++;
			fs = fs->next;
		}
		tcp_writestr(&tcpbuf, sock, "</select></span>");

		sprintf( http_buf, "<input type=submit id='submitbutton' value=\"Save '%s'\" disabled><br>",fname);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );

		if (!noeditor) {
			FILE *fd = fopen(fname, "r");
			if (fd) {
				tcp_writestr(&tcpbuf, sock, "<center><textarea cols=\"40\" wrap=\"off\" rows=\"9\" spellcheck=\"false\" name=\"textedit\">");
				while( !feof(fd) ) {
					int len = fread(http_buf, 1, sizeof(http_buf), fd);
					if (len<=0) break;
					tcp_write(&tcpbuf, sock, http_buf, len );
				}
				fclose(fd);
				sprintf( http_buf, "</textarea></center></form>");
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
			else {
				sprintf( http_buf, "<br>Cant open file '%s'", fname);
				tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
			}
		}
		// footer (igual as outras pages)
		tcp_writestr(&tcpbuf, sock, "<div class='home-footer'><span class='hf-ver'>MultiCS r1000 v"VERSION_STR" - All Rights Reserved - Sharillas@2026</span></div>");
		tcp_writestr(&tcpbuf, sock, "</body></html>");
		tcp_flush(&tcpbuf, sock);
	}
}


// ============================================================
// CONFIGURATIONS: pagina unica com Iptables (1a div) + Edit Config (2a div)
// ============================================================

// ficheiros extra editaveis (whitelist do upload) mesmo que nao estejam
// registados no parse (FILE directives) - sao lidos/gravados de /var/etc/
static const char *editor_extra_files[] = {
	"multics.cfg","profiles.cfg","CCcam.channelinfo","CCcam.providers","CCcam.lite",
	"servidores.cfg","clientes_cccam.cfg","clientes_mgcamd.cfg",
	"clientes_cs378x.cfg","clientes_camd35.cfg","clientes_cache.cfg",
	"Softcam.cfg","blocked_ips.cfg", NULL };

// o nome (basename) ja esta registado na lista do parse?
static int editor_in_cfgfiles(const char *name)
{
	struct filename_data *fs = cfg.files;
	while (fs) {
		const char *b = strrchr(fs->name, '/');
		const char *base = b ? b+1 : fs->name;
		if (!strcmp(base, name)) return 1;
		fs = fs->next;
	}
	return 0;
}

// procura uma ferramenta python: pasta do binario -> /opt/multics -> /var/etc
static int find_tool(const char *name, char *out, int outsz)
{
	char exe[512];
	int n = readlink("/proc/self/exe", exe, sizeof(exe)-1);
	if (n>0) {
		exe[n]=0;
		char *sl = strrchr(exe,'/');
		if (sl) {
			snprintf(out, outsz, "%.*s/%s", (int)(sl-exe), exe, name);
			if (!access(out, F_OK)) return 1;
		}
	}
	snprintf(out, outsz, "/opt/multics/%s", name);
	if (!access(out, F_OK)) return 1;
	snprintf(out, outsz, "/var/etc/%s", name);
	if (!access(out, F_OK)) return 1;
	return 0;
}

// resolve o caminho REAL de um ficheiro de config (basename) a partir
// da config em execucao - funciona com qualquer layout (-C /emu/multics/...)
// ordem: ficheiros especiais -> lista do parse (INCLUDE/FILE) -> pasta do multics.cfg
static void resolve_cfg_path(const char *name, char *out, int outsz)
{
	const char *wanted = NULL;
	if (!strcmp(name,"multics.cfg")) wanted = config_file;
	else if (!strcmp(name,"CCcam.channelinfo")) wanted = cfg.channelinfo_file;
	else if (!strcmp(name,"CCcam.providers")) wanted = cfg.providers_file;
	else if (!strcmp(name,"CCcam.lite")) wanted = cfg.lite_file;
	else if (!strcmp(name,"Softcam.cfg")) wanted = cfg.constcw_file;
	else if (!strcmp(name,"blocked_ips.cfg")) wanted = cfg.blockedip_file;
	else if (!strcmp(name,"ip2country.csv")) wanted = cfg.ip2country_file;
	else if (!strcmp(name,"multics.css")) wanted = cfg.stylesheet_file;
	if (wanted && wanted[0]) {
		snprintf(out, outsz, "%s", wanted);
		return;
	}
	struct filename_data *fs = cfg.files;
	while (fs) {
		const char *b = strrchr(fs->name, '/');
		const char *base = b ? b+1 : fs->name;
		if (!strcmp(base, name)) {
			snprintf(out, outsz, "%s", fs->name);
			return;
		}
		fs = fs->next;
	}
	const char *sl = strrchr(config_file, '/');
	if (sl) snprintf(out, outsz, "%.*s/%s", (int)(sl-config_file), config_file, name);
	else snprintf(out, outsz, "%s", name);
}

// resolve o ficheiro do editor pelo index:
//   devolve 1 = ficheiro da lista do parse (index < n)
//   devolve 2 = ficheiro extra em /var/etc/ (index >= n)
//   devolve 0 = invalido
static int editor_get_filename(int index, char *fname, int fname_sz, int *noeditor)
{
	struct filename_data *fs = cfg.files;
	int n = 0;
	while (fs) { n++; fs = fs->next; }
	if (index < n) {
		fs = cfg.files;
		int k = 0;
		while (fs && k<index) { fs = fs->next; k++; }
		if (!fs) return 0;
		snprintf(fname, fname_sz, "%s", fs->name);
		if (noeditor) *noeditor = fs->noeditor;
		return 1;
	}
	int e = index - n;
	if (e>=0 && editor_extra_files[e]) {
		resolve_cfg_path(editor_extra_files[e], fname, fname_sz);
		if (noeditor) *noeditor = 0;
		return 2;
	}
	return 0;
}

// comenta as linhas indicadas (1-based) de um ficheiro, prefixando "# "
// linhas ja comentadas ficam como estao
static int config_comment_lines(const char *fname, const int *lines, int nlines)
{
	char tmpf[600];
	snprintf(tmpf, sizeof(tmpf), "%s.tmpfix", fname);
	FILE *in = fopen(fname, "r");
	if (!in) return -1;
	FILE *out = fopen(tmpf, "w");
	if (!out) { fclose(in); return -1; }
	char line[10240];
	int nb = 0;
	int k;
	while (fgets(line, sizeof(line), in)) {
		nb++;
		int bad = 0;
		for (k=0;k<nlines;k++) if (lines[k]==nb) { bad = 1; break; }
		if (bad) {
			char *p = line;
			while (*p==' '||*p=='\t') p++;
			if (*p && (*p!='#') && (*p!='\n') && (*p!='\r')) fputs("# ", out);
		}
		fputs(line, out);
	}
	fclose(in);
	fclose(out);
	rename(tmpf, fname);
	return 0;
}

// fragmento da div de edicao (select + save + textarea) - escreve num dyn_buffer
void http_send_editdiv(struct dyn_buffer *db, int index)
{
	char http_buf[2048];
	struct filename_data *fs;
	int i;
	int noeditor_ed = 0;
	char fname[512];
	int ftype = editor_get_filename(index, fname, sizeof(fname), &noeditor_ed);
	if (!ftype) { index = 0; ftype = editor_get_filename(0, fname, sizeof(fname), &noeditor_ed); }
	int noeditor = noeditor_ed;

	sprintf( http_buf, "<div class=stat-section style='margin:10px 0'><div class='cfgbtns'>"
		"<div><input type=button class='sbutton' value='Load Channel Info' title='Rele o /var/etc/CCcam.channelinfo do disco (o teu ficheiro proprio)' onclick=\"imgrequest('/configurations?action=reloadchinfo',this)\"><span class='cfgbtns-info'>Parse do teu CCcam.channelinfo sem restart</span></div>"
		"<div><input type=button class='sbutton' value='Update Channel Info' title='Atualiza o CCcam.channelinfo do KingOfSat (so feeds ativos)' onclick=\"imgrequest('/configurations?action=updatechinfo',this)\"><span class='cfgbtns-info'>Reconstroi o CCcam.channelinfo do KingOfSat e recarrega automaticamente</span></div>"
		"<div><input type=button class='sbutton' value='Reload Main Config' title='Reler toda a configuracao do disco' onclick=\"imgrequest('/configurations?action=reread',this)\"><span class='cfgbtns-info'>Aplica o multics.cfg e includes sem restart</span></div>"
		"<div><a class='sbutton' href='/configurations?action=clearsessions' title='Termina todas as sessoes ativas (todos os browsers/scripts voltam ao login)' onclick=\"return confirm('Terminar TODAS as sessoes? Teras de voltar a fazer login.')\">Terminar todas as sessoes</a><span class='cfgbtns-info'>Invalida todas as cookies de sessao (tu incluido)</span></div>"
		"</div></div>");
	dynbuf_write( db, (unsigned char*)http_buf, strlen(http_buf) );

	sprintf( http_buf, "<form id='editform' enctype=\"multipart/form-data\" method=\"post\" action=\"/configurations?file=%d\">", index);
	dynbuf_write( db, (unsigned char*)http_buf, strlen(http_buf) );

	dynbuf_write( db, (unsigned char*)"<span style='float:right'><select onchange=\"loadEditor(this.value)\" style='width:250px;'>", strlen("<span style='float:right'><select onchange=\"loadEditor(this.value)\" style='width:250px;'>") );
	fs = cfg.files;
	i = 0;
	while (fs) {
		if (!fs->noeditor) {
			if (i==index) sprintf( http_buf, "<option value=\"/configurations?file=%d\" selected>%s</option>",i, fs->name);
			else sprintf( http_buf, "<option value=\"/configurations?file=%d\">%s</option>",i, fs->name);
			dynbuf_write( db, (unsigned char*)http_buf, strlen(http_buf) );
		}
		i++;
		fs = fs->next;
	}
		int e = 0;
	while (editor_extra_files[e]) {
		if (!editor_in_cfgfiles(editor_extra_files[e])) {
			char xp[512];
			resolve_cfg_path(editor_extra_files[e], xp, sizeof(xp));
			int x = i + e;
			if (x==index) sprintf( http_buf, "<option value=\"/configurations?file=%d\" selected>%s</option>",x, xp);
			else sprintf( http_buf, "<option value=\"/configurations?file=%d\">%s</option>",x, xp);
			dynbuf_write( db, (unsigned char*)http_buf, strlen(http_buf) );
		}
		e++;
	}
	dynbuf_write( db, (unsigned char*)"</select></span>", strlen("</select></span>") );

	sprintf( http_buf, "<input type=hidden id='edfileidx' value='%d'>", index);
	dynbuf_write( db, (unsigned char*)http_buf, strlen(http_buf) );
	sprintf( http_buf, "<input type=button id='submitbutton' value=\"Save '%s'\" onclick=\"saveEditor()\">&nbsp;<span id='savestatus'></span><br>",fname);
	dynbuf_write( db, (unsigned char*)http_buf, strlen(http_buf) );

	if (!noeditor) {
		FILE *fd = fopen(fname, "r");
		if (fd) {
			dynbuf_write( db, (unsigned char*)"<center><textarea cols=\"40\" wrap=\"off\" rows=\"9\" spellcheck=\"false\" name=\"textedit\">", strlen("<center><textarea cols=\"40\" wrap=\"off\" rows=\"9\" spellcheck=\"false\" name=\"textedit\">") );
			while( !feof(fd) ) {
				int len = fread(http_buf, 1, sizeof(http_buf), fd);
				if (len<=0) break;
				dynbuf_write( db, (unsigned char*)http_buf, len );
			}
			fclose(fd);
			sprintf( http_buf, "</textarea></center></form>");
			dynbuf_write( db, (unsigned char*)http_buf, strlen(http_buf) );
		}
		else {
			sprintf( http_buf, "<br>Cant open file '%s'", fname);
			dynbuf_write( db, (unsigned char*)http_buf, strlen(http_buf) );
		}
	}
}

void http_send_configurations(int sock, http_request *req)
{
	char http_buf[2048];
	int i;
	struct tcp_buffer_data tcpbuf;
	tcp_init(&tcpbuf);
	tcp_write(&tcpbuf, sock, http_replyok, strlen(http_replyok) );
	tcp_write(&tcpbuf, sock, http_html, strlen(http_html) );
	tcp_write(&tcpbuf, sock, http_head, strlen(http_head) );

	// ===== ACTIONS =====
	char *str_action = isset_get( req, "action");
	if (str_action && !strcmp(str_action,"clearsessions")) {
		http_session_clearall();
		mlogf(LOGINFO, DBG_HTTP, " http: todas as sessoes terminadas\n");
		http_send_redirect(sock, "/configurations");
		return;
	}
	if (str_action && !strcmp(str_action,"block")) {
		char *ip = isset_get( req, "ip");
		if (ip && ip[0]) {
			uint32_t ipv4 = inet_addr(ip);
			if (ipv4!=INADDR_NONE) ipblock_add(ipv4);
		}
		http_send_redirect(sock, "/configurations");
		return;
	}
	if (str_action && !strcmp(str_action,"unblock")) {
		char *ip = isset_get( req, "ip");
		if (ip && ip[0]) {
			uint32_t ipv4 = inet_addr(ip);
			if (ipv4!=INADDR_NONE) ipblock_del(ipv4);
		}
		http_send_redirect(sock, "/configurations");
		return;
	}
	if (str_action && !strcmp(str_action,"reloadchinfo")) {
		read_chinfo( &cfg );
		mlogf(LOGINFO, DBG_HTTP, " http: channelinfo reloaded from disk\n");
		http_send_text(sock, "<span class='success'>OK</span>");
		return;
	}
	if (str_action && !strcmp(str_action,"reread")) {
		free_filenames( &cfg );
		reread_config( &cfg );
		check_config( &cfg );
		cfg_set_id_counters( &cfg );
		emu_load();
		lite_load();
		ipblock_load();
		mlogf(LOGINFO, DBG_HTTP, " http: config reread from disk\n");
		http_send_text(sock, "<span class='success'>OK</span>");
		return;
	}
	if (str_action && !strcmp(str_action,"updatechinfo")) {
		static uint32_t lastupdatechinfo = 0;
		uint32_t now = GetTickCount();
		if (lastupdatechinfo && ((now-lastupdatechinfo)<300000)) {
			http_send_text(sock, "<span class='miss'>Aguarda 5 minutos entre atualizacoes</span>");
			return;
		}
		lastupdatechinfo = now;
		char tool[512];
		char chinfo[512];
		if (find_tool("tools_update_channelinfo.py", tool, sizeof(tool))) {
			resolve_cfg_path("CCcam.channelinfo", chinfo, sizeof(chinfo));
			sprintf( http_buf, "python3 %s --apply --out \"%s\" >/var/tmp/chinfo_update.log 2>&1 &", tool, chinfo);
			system(http_buf);
			mlogf(LOGINFO, DBG_HTTP, " http: channelinfo update iniciado (KingOfSat) -> %s\n", chinfo);
			http_send_text(sock, "<span class='success'>OK</span>");
		}
		else http_send_text(sock, "<span class='miss'>Ferramenta nao encontrada (tools_update_channelinfo.py)</span>");
		return;
	}
	if (str_action && !strcmp(str_action,"editdiv")) {
		// so a div de edicao (sem refresh da pagina)
		int idx = 0;
		char *sf = isset_get( req, "file");
		if (sf) idx = atoi(sf);
		struct dyn_buffer db;
		dynbuf_init(&db, 16384);
		http_send_editdiv(&db, idx);
		http_send_answer(sock, req, "text/html", db.data, db.datasize);
		dynbuf_free(&db);
		return;
	}
	if (str_action && !strcmp(str_action,"upload")) {
		// upload de ficheiro de config (whitelist) para /var/etc/
		static const char *upload_whitelist[] = {
			"multics.cfg","profiles.cfg","CCcam.channelinfo","CCcam.providers","CCcam.lite",
			"servidores.cfg","clientes_cccam.cfg","clientes_mgcamd.cfg",
			"clientes_cs378x.cfg","clientes_camd35.cfg","clientes_cache.cfg",
			"Softcam.cfg","blocked_ips.cfg", NULL };
		char *fnameparam = isset_get( req, "file");
		int okname = 0;
		if (fnameparam) {
			int w = 0;
			while (upload_whitelist[w]) {
				if (!strcmp(upload_whitelist[w], fnameparam)) { okname = 1; break; }
				w++;
			}
		}
		if (!okname) {
			http_send_text(sock, "<span class='miss'>Ficheiro nao permitido</span>");
			return;
		}
		if (req->type==HTTP_POST) {
			char *content = isset_header(req, "Content-Type");
			if (content && !memcmp(content,"multipart/form-data",19)) {
				while (*content!=';') { if (*content==0) break; content++; }
				if (*content==';') {
					content++;
					while (*content==' ') content++;
					if (!memcmp(content,"boundary",8)) {
						while (*content!='=') { if (*content==0) break; content++; }
						if (*content=='=') {
							content++;
							while (*content==' '||*content=='\t') content++;
							char boundary[255];
							char endboundary[255];
							sprintf( boundary, "--%s", content);
							sprintf( endboundary, "\r\n--%s", content);
							char *p = req->dbf.data;
							p = (char*) boyermoore_horspool_memmem( (uint8_t*)p, req->dbf.datasize, (uint8_t*)boundary, strlen(boundary) );
							if (p) {
								p += strlen(boundary);
								if ( *p=='\r' && *(p+1)=='\n' ) {
									p += 2;
									char *h = p;
									while ( !(h[0]=='\r'&&h[1]=='\n'&&h[2]=='\r'&&h[3]=='\n') ) {
										if (h[0]==0) break;
										h++;
									}
									char *pdata = h+4;
									char *end = (char*) boyermoore_horspool_memmem( (uint8_t*)pdata, req->dbf.datasize-(pdata-(char*)req->dbf.data), (uint8_t*)endboundary, strlen(endboundary) );
									if (end && end>pdata) {
										char fname[512];
										resolve_cfg_path( fnameparam, fname, sizeof(fname) );
										char tmpsave[600];
										sprintf( tmpsave, "%s.tmp", fname );
										FILE *fd = fopen( tmpsave, "wb");
										if (fd) {
											fwrite( pdata, 1, end-pdata, fd );
											fclose(fd);
											char backup[600];
											sprintf( backup, "%s.bak-%ld", fname, (long)time(NULL) );
											if (rename( fname, backup )) { unlink(tmpsave); sprintf( http_buf, "<center><h3><span class='failed'>ERRO: sem permissoes em '%s'</span></h3><p>Executa a build como root ou da permissoes:<br>chmod 666 \"%s\"</p></center>", fname, fname); http_send_text(sock, http_buf); return; }
											if (rename( tmpsave, fname )) { rename( backup, fname ); sprintf( http_buf, "<center><h3><span class='failed'>ERRO: nao consegui gravar '%s'</span></h3><p>O ficheiro anterior foi reposto.</p></center>", fname); http_send_text(sock, http_buf); return; }
										mlogf(LOGINFO, DBG_HTTP, " http: upload '%s' (%d bytes, backup %s)\n", fname, (int)(end-pdata), backup);
										// reler e apanhar erros de parse deste ficheiro
										char oldpass[64];
										strcpy( oldpass, cfg.http.pass );
										int err0 = g_config_errors;
										g_cfg_err_nb = 0; // reset do historico (so interessa este reload)
										free_filenames( &cfg );
										reread_config( &cfg );
										check_config( &cfg );
										cfg_set_id_counters( &cfg );
										emu_load();
										lite_load();
										ipblock_load();
										// password do admin mudou? termina todas as sessoes
										if ( strcmp(oldpass, cfg.http.pass) ) {
											mlogf(LOGINFO, DBG_HTTP, " http: password alterada - sessoes invalidadas\n");
											http_session_clearall();
										}
										int badlines[MAX_CFG_ERRS];
											int nbad = 0;
											int e;
											for (e=0; e<g_cfg_err_nb; e++) {
												if (!strcmp(g_cfg_errs[e].file, fname) && (g_cfg_errs[e].line>0)) {
													int dup = 0;
													int b;
													for (b=0;b<nbad;b++) if (badlines[b]==g_cfg_errs[e].line) dup=1;
													if (!dup && (nbad<MAX_CFG_ERRS)) badlines[nbad++] = g_cfg_errs[e].line;
												}
											}
											int errs = g_config_errors - err0;
											int rollback = 0;
											if (nbad>0) {
												// comentar as linhas com erro e reler (opcoes default ficam ativas)
												if (!config_comment_lines(fname, badlines, nbad)) {
													mlogf(LOGWARNING, DBG_HTTP, " http: upload '%s': %d linha(s) com erro comentadas\n", fname, nbad);
													g_cfg_err_nb = 0;
													free_filenames( &cfg );
													reread_config( &cfg );
													check_config( &cfg );
													cfg_set_id_counters( &cfg );
													emu_load();
													lite_load();
													ipblock_load();
													// ainda ha erros neste ficheiro?
													for (e=0; e<g_cfg_err_nb; e++) {
														if (!strcmp(g_cfg_errs[e].file, fname)) { rollback = 1; break; }
													}
												}
											}
											else if (errs>0) {
												// erros noutros ficheiros (includes) - o parse ignora as linhas e usa defaults
												mlogf(LOGWARNING, DBG_HTTP, " http: upload '%s': %d erro(s) de config noutros ficheiros (linhas ignoradas, defaults ativos)\n", fname, errs);
											}
											if (rollback) {
												char invalid[600];
												sprintf( invalid, "%s.invalid-%ld", fname, (long)time(NULL) );
												rename( fname, invalid );
												rename( backup, fname );
												mlogf(LOGERROR, DBG_HTTP, " http: upload '%s' REJEITADO (erros de parse) - restaurado '%s', enviado para '%s'\n", fname, backup, invalid);
												free_filenames( &cfg );
												reread_config( &cfg );
												check_config( &cfg );
												cfg_set_id_counters( &cfg );
												emu_load();
												lite_load();
												ipblock_load();
											}
											// resposta
											if (rollback) sprintf( http_buf, "<center><h3>Upload REJEITADO</h3><p>O ficheiro tem erros de config que nao foi possivel corrigir.<br>O anterior foi reposto e a build continua a correr.<br>O ficheiro enviado ficou guardado em '%s.invalid-*' para corrigires.</p><p><a href='/configurations'>Voltar a Configs</a></p></center>", fname);
											else if (nbad>0) sprintf( http_buf, "<center><h3>Upload aceite</h3><p>%d linha(s) com erro foram comentadas automaticamente.<br>As opcoes default ficam ativas para essas linhas.</p><p><a href='/configurations'>Voltar a Configs</a></p></center>", nbad);
											else if (errs>0) sprintf( http_buf, "<center><h3>Upload aceite (com avisos)</h3><p>%d erro(s) de config noutros ficheiros - as linhas foram ignoradas<br>e as opcoes default ficam ativas (sem crash).</p><p><a href='/configurations'>Voltar a Configs</a></p></center>", errs);
											else sprintf( http_buf, "<center><h3>Upload aceite</h3><p>Config recarregada sem erros.</p><p><a href='/configurations'>Voltar a Configs</a></p></center>");
											http_send_text(sock, http_buf);
											return;
										}
										else {
											sprintf( http_buf, "<center><h3><span class='failed'>ERRO: sem permissoes de escrita para '%s'</span></h3><p>Executa a build como root ou da permissoes:<br>chmod 666 \"%s\"</p><p><a href='/configurations'>Voltar a Configs</a></p></center>", fname, fname);
											http_send_text(sock, http_buf);
											return;
										}
									}
								}
							}
						}
					}
				}
			}
		}
		http_send_redirect(sock, "/configurations");
		return;
	}

	// ===== ficheiro selecionado =====
	int index = 0;
	char *str_file = isset_get( req, "file");
	if (str_file) index = atoi(str_file);
	int noeditor_ed2 = 0;
	char fname[512];
	int ftype = editor_get_filename(index, fname, sizeof(fname), &noeditor_ed2);
	if (!ftype) { index = 0; ftype = editor_get_filename(0, fname, sizeof(fname), &noeditor_ed2); }
	int noeditor = noeditor_ed2;

	// ===== POST multipart (save) =====
	if ( (req->type==HTTP_POST) && !noeditor ) {
		char *content = isset_header(req, "Content-Type");
		if (content && !memcmp(content,"multipart/form-data",19)) {
			while (*content!=';') { if (*content==0) break; content++; }
			if (*content==';') {
				content++;
				while (*content==' ') content++;
				if (!memcmp(content,"boundary",8)) {
					while (*content!='=') { if (*content==0) break; content++; }
					if (*content=='=') {
						content++;
						while (*content==' '||*content=='\t') content++;
						char boundary[255];
						char endboundary[255];
						sprintf( boundary, "--%s", content);
						sprintf( endboundary, "\r\n--%s", content);
						char *p = req->dbf.data;
						p = (char*) boyermoore_horspool_memmem( (uint8_t*)p, req->dbf.datasize, (uint8_t*)boundary, strlen(boundary) );
						if (p) {
							p += strlen(boundary);
							if ( *p=='\r' && *(p+1)=='\n' ) {
								p += 2;
								char *h = p;
								while ( !(h[0]=='\r'&&h[1]=='\n'&&h[2]=='\r'&&h[3]=='\n') ) {
									if (h[0]==0) break;
									h++;
								}
								char *pdata = h+4;
								char *end = (char*) boyermoore_horspool_memmem( (uint8_t*)pdata, req->dbf.datasize-(pdata-(char*)req->dbf.data), (uint8_t*)endboundary, strlen(endboundary) );
								if (end && end>pdata) {
									char tmpsave[600];
									sprintf( tmpsave, "%s.tmp", fname );
									FILE *cfgfd = fopen( tmpsave, "w");
									if (cfgfd) {
										int k;
										for (k=0;k<end-pdata;k++) {
											if ( ( *(pdata+k)=='\r' ) && ( (k+1) < (end-pdata) ) ) {
												if ( *(pdata+k+1) =='\n' ) k++;
											}
											fwrite( pdata+k,1,1,cfgfd);
										}
										fclose(cfgfd);
										if (!rename( tmpsave, fname )) {
											sprintf( http_buf, "<script type=\"text/JavaScript\"><!--\nsetTimeout(\"location.href = '/configurations?file=%d';\",3000);\n--></script>\n<h3><center>file '%s' is Successfully Saved</center></h3>", index, fname);
											tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
										}
										else {
											unlink(tmpsave);
											sprintf( http_buf, "<h3><center><span class='failed'>ERRO: nao consegui gravar '%s' (rename falhou - permissoes?)</span><br><br>Executa a build como root ou da permissoes:<br>chmod 666 \"%s\"</center></h3>", fname, fname);
											tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
										}
									}
									else {
										sprintf( http_buf, "<h3><center><span class='failed'>ERRO: sem permissoes de escrita para '%s'</span><br><br>Executa a build como root ou da permissoes:<br>chmod 666 \"%s\"</center></h3>", fname, fname);
										tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
									}
								}
							}
						}
					}
				}
			}
		}
	tcp_flush(&tcpbuf, sock);
	// aplicar alteracoes imediatamente (sem restart)
	mlogf(LOGINFO, DBG_HTTP, " http: config saved '%s' - reloading config...\n", fname);
	{
		char oldpass[64];
		strcpy( oldpass, cfg.http.pass );
		free_filenames( &cfg );
		reread_config( &cfg );
		check_config( &cfg );
		cfg_set_id_counters( &cfg );
		emu_load();
		lite_load();
		ipblock_load();
		if ( strcmp(oldpass, cfg.http.pass) ) {
			mlogf(LOGINFO, DBG_HTTP, " http: password alterada - sessoes invalidadas\n");
			http_session_clearall();
		}
	}
	return;
}

	// ===== PAGE =====
	sprintf( http_buf, html_title, cfg.http.title, "Configurations"); tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	tcp_write(&tcpbuf, sock, http_link, strlen(http_link) );
	tcp_write(&tcpbuf, sock, http_style, strlen(http_style) );
	tcp_write(&tcpbuf, sock, http_javascript, strlen(http_javascript) );
	tcp_writestr(&tcpbuf, sock, "\n<script type='text/javascript'>");
	tcp_writestr(&tcpbuf, sock, "\nfunction start()\n{\n	 document.getElementById('submitbutton').disabled=false;\n}");
	tcp_writestr(&tcpbuf, sock, "\nfunction saveEditor()\n{\n	var f=document.getElementById('editform');\n	if(!f)return;\n	var s=document.getElementById('savestatus');\n	if(s)s.innerHTML='<span class=busy>A guardar...</span>';\n	var x=new XMLHttpRequest();\n	x.open('POST',f.action,true);\n	x.onreadystatechange=function()\n	{\n		if(x.readyState==4){\n			var m='';\n			if(x.status==200){\n				var t=x.responseText;\n				if(t.indexOf('Successfully Saved')>=0)m='<span class=success>Guardado com sucesso!</span>';\n				else if(t.indexOf('ERRO')>=0){var em=t.match(/ERRO[^<]*/);m='<span class=failed>'+(em?em[0]:'Erro ao guardar')+'</span>';}\n				else m='<span class=failed>Resposta inesperada do servidor</span>';\n			}else m='<span class=failed>Erro HTTP '+x.status+'</span>';\n			if(s)s.innerHTML=m;\n			setTimeout(function(){\n				var idx=document.getElementById('edfileidx');\n				var d=document.getElementById('editsection');\n				if(d&&idx){\n					var x2=new XMLHttpRequest();\n					x2.open('GET','/configurations?action=editdiv&file='+idx.value+'&t='+new Date().getTime(),true);\n					x2.onreadystatechange=function(){if(x2.readyState==4&&x2.status==200)d.innerHTML=x2.responseText;};\n					x2.send(null);\n				}\n			},1500);\n		}\n	};\n	x.send(new FormData(f));\n}");
	tcp_writestr(&tcpbuf, sock, "\nfunction loadEditor(url)\n{\n	var f = url.split('file=')[1];\n	var x = new XMLHttpRequest();\n	x.open('GET','/configurations?action=editdiv&file='+f+'&t='+new Date().getTime(),true);\n	x.onreadystatechange=function()\n	{\n		if (x.readyState==4 && x.status==200) {\n			var d=document.getElementById('editsection');\n			if (d) d.innerHTML = x.responseText;\n			var b=document.getElementById('submitbutton');\n			if (b) b.disabled = false;\n		}\n	};\n	x.send(null);\n}");
	tcp_writestr(&tcpbuf, sock, "\n</script>\n");
	tcp_write(&tcpbuf, sock, http_head_, strlen(http_head_) );
	tcp_writestr(&tcpbuf, sock, "<body onload=\"start();\">");
	tcp_write_menu(&tcpbuf, sock, PAGE_CONFIGURATIONS);
	tcp_writestr(&tcpbuf, sock, "<div id='mainDiv'>");

	// ---- DIV 0: Upload Configs ----
	tcp_writestr(&tcpbuf, sock, "<div class=stat-section style='margin:10px 0'>");
	tcp_writestr(&tcpbuf, sock, "<h3 class=stitle>Upload Configs</h3><div class=stat-value>");
	tcp_writestr(&tcpbuf, sock, "<form method='POST' enctype='multipart/form-data' action='/configurations?action=upload' onsubmit=\"this.action='/configurations?action=upload&file='+this.elements['file'].value;\">");
	tcp_writestr(&tcpbuf, sock, "Ficheiro de destino: <select name='file' style='width:250px;'>");
	tcp_writestr(&tcpbuf, sock, "<option value='multics.cfg'>multics.cfg (mestre - pode conter tudo)</option>");
	tcp_writestr(&tcpbuf, sock, "<option value='profiles.cfg'>profiles.cfg (perfis + users newcamd)</option>");
	tcp_writestr(&tcpbuf, sock, "<option value='servidores.cfg'>servidores.cfg (N:/C:/L:, cache, cacheex, camd35)</option>");
	tcp_writestr(&tcpbuf, sock, "<option value='clientes_cccam.cfg'>clientes_cccam.cfg (F-lines)</option>");
	tcp_writestr(&tcpbuf, sock, "<option value='clientes_mgcamd.cfg'>clientes_mgcamd.cfg</option>");
	tcp_writestr(&tcpbuf, sock, "<option value='clientes_cs378x.cfg'>clientes_cs378x.cfg</option>");
	tcp_writestr(&tcpbuf, sock, "<option value='clientes_camd35.cfg'>clientes_camd35.cfg</option>");
	tcp_writestr(&tcpbuf, sock, "<option value='clientes_cache.cfg'>clientes_cache.cfg</option>");
	tcp_writestr(&tcpbuf, sock, "<option value='CCcam.channelinfo'>CCcam.channelinfo</option>");
	tcp_writestr(&tcpbuf, sock, "<option value='CCcam.providers'>CCcam.providers</option>");
	tcp_writestr(&tcpbuf, sock, "<option value='CCcam.lite'>CCcam.lite</option>");
	tcp_writestr(&tcpbuf, sock, "<option value='Softcam.cfg'>Softcam.cfg</option>");
	tcp_writestr(&tcpbuf, sock, "<option value='blocked_ips.cfg'>blocked_ips.cfg</option>");
	tcp_writestr(&tcpbuf, sock, "</select>&nbsp; <input type='file' name='uploadfile'>&nbsp;<input type='submit' value='Upload'></form>");
	tcp_writestr(&tcpbuf, sock, "<span style='font-size:11px;'>envia o teu ficheiro para o caminho real da config (onde o parse o le). Faz backup automatico do anterior. A config e recarregada apos o upload. Um ficheiro unico com todas as seccoes (servers, perfis, clientes) carrega-se como multics.cfg.</span></div>");

	// ---- DIV 1: Iptables ----
	tcp_writestr(&tcpbuf, sock, "<div class=stat-section style='margin:10px 0'>");
	tcp_writestr(&tcpbuf, sock, "<h3 class=stitle >Block IP</h3><div class=stat-value><form method='GET' action='/configurations'><input type='hidden' name='action' value='block'>IP Address: <input type='text' name='ip' placeholder='192.168.1.100' style='width:200px;margin-right:8px'><input type='submit' value='Block IP'></form></div>");
	tcp_writestr(&tcpbuf, sock, "</div>");
	sprintf( http_buf, "<div class=stat-section style='margin:10px 0'><h3 class=stitle >Blocked IPs (%d)</h3>", ipblock_count);
	tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	if (cfg.blockedip_file[0]) {
		sprintf( http_buf, "<div class=stat-value>File: <b>%s</b><br></div>", cfg.blockedip_file);
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	tcp_writestr(&tcpbuf, sock, "<table class=maintable><tr><th>IP</th><th>Country</th><th>Blocked since</th><th>Actions</th></tr>");
	for (i=0; i<ipblock_count; i++) {
		char *pc = getcountrycodebyip(ipblock_list[i].ip);
		char country[64] = "-";
		if (pc) {
			char *n = getcountryname(pc);
			snprintf(country, sizeof(country), "<img src='/flag_%s.gif' title='%s'> %s", pc, n?n:pc, pc);
		}
		uint32_t blk = (GetTickCount()-ipblock_list[i].time)/1000;
		sprintf( http_buf, "<tr><td>%s</td><td>%s</td><td>%02ud %02d:%02d:%02d</td><td><a href='/configurations?action=unblock&ip=%s' class='btn-del'>Unblock</a></td></tr>",
			(char*)ip2string(ipblock_list[i].ip), country,
			blk/(3600*24), (blk/3600)%24, (blk/60)%60, blk%60,
			(char*)ip2string(ipblock_list[i].ip));
		tcp_write(&tcpbuf, sock, http_buf, strlen(http_buf) );
	}
	if (!ipblock_count)
		tcp_writestr(&tcpbuf, sock, "<tr><td colspan=4 style='text-align:center;color:#888'>No blocked IPs</td></tr>");
	tcp_writestr(&tcpbuf, sock, "</table></div>");

	// ---- DIV 2: Edit Config (div com id para refresh ajax) ----
	tcp_writestr(&tcpbuf, sock, "<div id='editsection'>");
	{
		struct dyn_buffer db;
		dynbuf_init(&db, 16384);
		http_send_editdiv(&db, index);
		tcp_write(&tcpbuf, sock, db.data, db.datasize );
		dynbuf_free(&db);
	}
	tcp_writestr(&tcpbuf, sock, "</div>");

	// footer adicionado pelo JS (fora do mainDiv)
	tcp_writestr(&tcpbuf, sock, "</div></body></html>");
	tcp_flush(&tcpbuf, sock);
}


int atoint(char *index)
{
  int n=0;
  while (*index)
  { 
    if ( (*index<'0')||(*index>'9') ) return n;
    else n = n*10 + (*index - '0');
    index++;
  }
  return n;
}

#include "base64.c"

struct connect_data {
	int sock;
	uint32_t ip;
};

void *gererClient(struct connect_data *param)
{
	int sock = param->sock;
	uint32_t ip = param->ip;
	free(param);

	http_request req;

	struct pollfd pfd;
	pfd.fd = sock;
	pfd.events = POLLIN | POLLPRI;
	int retval = poll(&pfd, 1, 2000);

	//printf("\n*Connexion de %s(%d)\n", tt, sock); // print pid
	dynbuf_init(&req.dbf, 1024);

	if ( retval>0 )
	if ( pfd.revents & (POLLIN|POLLPRI) ) 
	if ( parse_http_request(sock, &req) ) {
		req.sock = sock;
		req.ip = ip;

		int auth=0;
		if ( (req.type==HTTP_GET)||(req.type==HTTP_POST) ) {
			// LOGIN PAGE (sem auth)
			if (!strcmp(req.path,"/login")) {
				if (req.type==HTTP_POST) http_login_submit(sock,&req);
				else {
					char *str_action = isset_get( &req, "action");
					if (str_action && !strcmp(str_action,"logout")) http_logout(sock,&req);
					else {
						// ja autenticado? vai direto para o dashboard
						char *token = http_get_cookie(&req, "multics_session");
						if (http_session_check(token)) {
							dynbuf_free(&req.dbf);
							http_send_redirect(sock, "/dashboard");
							close(sock);
							return NULL;
						}
						char *err = isset_get( &req, "error");
						http_send_login(sock,&req, err?1:0);
					}
				}
				dynbuf_free(&req.dbf);
				close(sock);
				return NULL;
			}
			// ASSETS publicos (usados pela pagina de login)
			if (!strcmp(req.path,"/customjs.js")) {
				if (strlen(cfg.javascript_file)) http_send_file(sock, &req, "text/javascript", cfg.javascript_file);
				else http_send_answer(sock, &req, "text/javascript", java_file, strlen(java_file));
				dynbuf_free(&req.dbf);
				close(sock);
				return NULL;
			}
			if (!strcmp(req.path,"/style.css")) {
				if (strlen(cfg.stylesheet_file)) http_send_file(sock, &req, "text/css", cfg.stylesheet_file);
				else http_send_answer(sock, &req, "text/css", style_css, strlen(style_css));
				dynbuf_free(&req.dbf);
				close(sock);
				return NULL;
			}
			// check for auth
			if (!cfg.http.user[0] || !cfg.http.pass[0]) auth = 1;
			else {
				int i;
				for(i=0; i<req.hdrcount; i++) {
					if( !strncmp(req.headers[i].name,"Authorization", 1024) ) {
						//printf("Authorization: %s\n", req.headers[i].value);
						//get auth type
						if (!memcmp(req.headers[i].value, "Basic ",6)) {
							// get encrypted login
							char pass[256];
							char realpass[256];
							base64_pdecode( &req.headers[i].value[6], pass);

							//mlogf(LOGDEBUG,0,"[ADMIN] Login Successful!");

							sprintf(realpass,"%s:%s", cfg.http.user, cfg.http.pass);
							if (!strncmp(pass,realpass,256)) auth=1;
						}
						break;
					}
				}
				// SESSION COOKIE
				if (!auth) {
					char *token = http_get_cookie(&req, "multics_session");
					if (http_session_check(token)) auth = 1;
				}
			}
			if ( auth ) {
				if (strcmp(req.path,"/")==0) http_send_index(sock,&req);
				else if (strcmp(req.path,"/dashboard")==0) http_send_index(sock,&req);
				else if (strcmp(req.path,"/debug")==0) {
					if (!cfg.http.show.nodebug) http_send_debug(sock,&req);
				}
				else if (strcmp(req.path,"/profiles")==0) {
					if (!cfg.http.show.noprofiles) http_send_profiles(sock,&req);
				}
				else if (strcmp(req.path,"/profile")==0) {
					if (!cfg.http.show.noprofiles) http_send_profile(sock,&req);
				}
				else if (strcmp(req.path,"/newcamd")==0) {
					if (!cfg.http.show.nonewcamd) http_send_newcamd(sock,&req);
				}
				else if (strcmp(req.path,"/newcamdclient")==0) {
					if (!cfg.http.show.nonewcamd) http_send_newcamd_client(sock,&req);
				}
				else if (strcmp(req.path,"/servers")==0) {
					if (!cfg.http.show.noservers) http_send_servers(sock,&req);
				}
				else if (strcmp(req.path,"/server")==0) {
					if (!cfg.http.show.noservers) http_send_server(sock,&req);
				}
				else if (strcmp(req.path,"/cache")==0) {
					if (!cfg.http.show.nocache) http_send_cache(sock,&req);
				}
				else if (strcmp(req.path,"/cachepeer")==0) {
					if (!cfg.http.show.nocache) http_send_cache_peer(sock,&req);
				}
#ifdef CCCAM_SRV
				else if (strcmp(req.path,"/cccam")==0) {
					if (!cfg.http.show.nocccam) http_send_cccam(sock,&req);
				}
				else if (strcmp(req.path,"/cccamclient")==0) {
					if (!cfg.http.show.nocccam) http_send_cccam_client(sock,&req);
				}
#endif
#ifdef CS378X_SRV
				else if (strcmp(req.path,"/cs378x")==0) {
					http_send_cs378x(sock,&req);
				}
				else if (strcmp(req.path,"/cs378xclient")==0) {
					http_send_cs378x_client(sock,&req);
				}
#endif
#ifdef CAMD35_SRV
				else if (strcmp(req.path,"/camd35")==0) {
					http_send_camd35(sock,&req);
				}
				else if (strcmp(req.path,"/camd35client")==0) {
					http_send_camd35_client(sock,&req);
				}
#endif
#ifdef CACHEEX
				else if (strcmp(req.path,"/cacheex")==0) {
					http_send_cacheex(sock,&req);
				}
#endif
				else if ( !memcmp(req.path,"/emulator",9) ) {
					http_send_emulator(sock,&req);
				}
				else if (strcmp(req.path,"/configurations")==0) {
					http_send_configurations(sock,&req);
				}
				else if (strcmp(req.path,"/iptables")==0) {
					http_send_iptables(sock,&req);
				}
#ifdef FREECCCAM_SRV
				else if (strcmp(req.path,"/freecccam")==0) {
					http_send_freecccam(sock,&req);
				}
#endif
#ifdef MGCAMD_SRV
				else if (strcmp(req.path,"/mgcamd")==0) {
					if (!cfg.http.show.nomgcamd) http_send_mgcamd(sock,&req);
				}
				else if (strcmp(req.path,"/mgcamdclient")==0) {
					if (!cfg.http.show.nomgcamd) http_send_mgcamd_client(sock,&req);
				}
#endif
				else if (strcmp(req.path,"/restart")==0) {
					if (!cfg.http.show.norestart) http_send_restart(sock,&req);
				}
				else if ( !memcmp(req.path,"/editor",7) )  {
					if (!cfg.http.show.noeditor) {
						int index=0;
						if ( (req.path[7]>='0')&&(req.path[7]<='9') ) index = req.path[7] - '0';
						if ( (req.path[8]>='0')&&(req.path[8]<='9') ) index = (index*10) + (req.path[8]-'0');
						http_send_editor(sock,&req, index);
					}
				}
				else if (strcmp(req.path,"/style.css")==0) {
					if (strlen(cfg.stylesheet_file)) {
						http_send_file(sock, &req, "text/css", cfg.stylesheet_file);
					}
					else http_send_answer(sock, &req, "text/css", style_css, strlen(style_css));
				}                
                else if (strcmp(req.path,"/customjs.js")==0) {
					if (strlen(cfg.javascript_file)) {
						mlogf(LOGDEBUG,DBG_HTTP," http: send file %s\n",cfg.javascript_file);
						http_send_file(sock, &req, "text/javascript", cfg.javascript_file);
					}
					else {
						mlogf(LOGDEBUG,DBG_HTTP," http: send answer %s\n",java_file);
						http_send_answer(sock, &req, "text/javascript", java_file, strlen(java_file));
					}
				}                
				else if (strcmp(req.path,"/connect.png")==0) {
					http_send_image(sock, &req, connect_png, sizeof(connect_png), "png");
				}
				else if (strcmp(req.path,"/disconnect.png")==0) {
					http_send_image(sock, &req, disconnect_png, sizeof(disconnect_png), "png");
				}
				else if (strcmp(req.path,"/enable.png")==0) {
					http_send_image(sock, &req, enable_png, sizeof(enable_png), "png");
				}
				else if (strcmp(req.path,"/disable.png")==0) {
					http_send_image(sock, &req, disable_png, sizeof(disable_png), "png");
				}
				else if (strcmp(req.path,"/debug.png")==0) {
					http_send_image(sock, &req, debug_png, sizeof(debug_png), "png");
				}
				else if (strcmp(req.path,"/refresh.png")==0) {
					http_send_image(sock, &req, refresh_png, sizeof(refresh_png), "png");
				}
				else if (strcmp(req.path,"/sms_new.gif")==0) {
					http_send_image(sock, &req, sms_new_gif, sizeof(sms_new_gif), "gif");
				}
				else if (strcmp(req.path,"/sms_old.gif")==0) {
					http_send_image(sock, &req, sms_old_gif, sizeof(sms_old_gif), "gif");
				}
				else if (strcmp(req.path,"/host")==0) {
					http_send_host(sock,&req);
				}
				else if (strcmp(req.path,"/threads")==0) {
					http_send_threads(sock,&req);
				}

#ifdef TESTCHANNEL
				else if (!strcmp(req.path,"/testchannel")) {
					http_send_testchannel(sock,&req);
				}
#endif
				else if (!strcmp(req.path,"/packages")) {
					http_send_packages(sock,&req);
				}
				else if ( !memcmp(req.path,"/flag_",6) && !memcmp(req.path+8,".gif",4) ) {
					// check for code
					char code[3];
					code[0] = req.path[6];
					code[1] = req.path[7];
					code[2] = 0;
					int i;
					for(i=0; i<MAX_COUNTRY_IMAGES; i++) {
						if ( !strcmp(country_images[i].code, code) ) {
							http_send_image(sock, &req, country_images[i].data, country_images[i].len, "gif");
							break;
						}
					}
					if (i>=MAX_COUNTRY_IMAGES) http_send_image(sock, &req, country_images[0].data, country_images[0].len, "gif");
				}

				else {
					struct http_file_data *file = cfg.http.files;
					while (file) {
						if ( !strcmp(req.path,file->url) ) {
							http_send_file( sock, &req, file->mime, file->path);
							break;
						}
						file = file->next;
					}
				}
			}
			else { // sem auth: redirect para /login (o popup Basic fica como fallback)
				struct tcp_buffer_data tcpbuf;
				char auth[1024];
				tcp_init(&tcpbuf);
				sprintf( auth, "HTTP/1.1 302 Found\r\nWWW-Authenticate: Basic realm=\"%s\"\r\nLocation: /login\r\nCache-Control: no-cache, no-store, must-revalidate\r\nConnection: close\r\n\r\n", cfg.http.title);
				tcp_write(&tcpbuf, sock, auth, strlen(auth) );
				tcp_flush(&tcpbuf, sock);
				//return;
			}
		}
	}

	dynbuf_free(&req.dbf);

	//printf("*Deconnexion de %s(%d)\n", tt, sock);

	if ( close(sock) ) mlogf(LOGERROR,getdbgflag(DBG_HTTP,0,0)," HTTP Server: socket close failed(%d)\n",sock);

	return NULL;
}



void *http_thread(void *param)
{
	int clientsock;
	struct sockaddr_in client_addr;
	socklen_t socklen = sizeof(client_addr);

	prctl(PR_SET_NAME,"HTTP Server",0,0,0);

	while (!prg.restart) {
		if (cfg.http.handle>0) {
			//pthread_mutex_lock(&prg.lockhttp);

			struct pollfd pfd;
			pfd.fd = cfg.http.handle;
			pfd.events = POLLIN | POLLPRI;
			int retval = poll(&pfd, 1, 3002);
			if ( retval>0 ) {
				if ( pfd.revents & (POLLIN|POLLPRI) ) {
					clientsock = accept(cfg.http.handle, (struct sockaddr*)&client_addr, /*(socklen_t*)*/&socklen);
					if ( clientsock<0 ) {
						mlogf(LOGERROR,getdbgflag(DBG_HTTP,0,0)," HTTP Server: Accept Error\n");
						break;
					}
					else {
						//SetSocketNoDelay(clientsock);
						pthread_t cli_tid;
						struct connect_data *newdata = malloc( sizeof(struct connect_data) );
						newdata->sock = clientsock; 
						newdata->ip = client_addr.sin_addr.s_addr;
						if (!create_thread(&cli_tid, (threadfn)gererClient, newdata)) {
							close(clientsock);
							free( newdata );
						}
					}
				}
			}
			else if (retval<0) {
				mlogf(LOGERROR,getdbgflag(DBG_HTTP,0,0)," THREAD HTTP: poll error %d(errno=%d)\n", retval, errno);
				usleep(99000);
			}
			//pthread_mutex_unlock(&prg.lockhttp);
			//usleep(10);
		} else usleep(100000);
	}// While

	mlogf(LOGINFO,getdbgflag(DBG_HTTP,0,0),"Exiting HTTP Thread\n");
	return NULL;
}

pthread_t http_tid;
int start_thread_http()
{
	create_thread(&http_tid, http_thread, NULL);
	return 0;
}

