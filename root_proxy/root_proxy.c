/*
    #################################################################
    #    (c) Copyright 2009-2025 JRCS Ltd  - All Rights Reserved    #
    #################################################################
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <signal.h>
#include <sys/select.h>
#include <arpa/nameser.h>

#include "liball.h"
#include "log_message.h"
#include "udp_sock.h"

#define RCODE_NXDOMAIN 3
#define PORT_DNS 53

typedef uint16_t id_t;
#define MAX_HANDSHAKE 20
#define MAX_RESP 0x10000
#define MAX_QUERY 512
#define MAX_EACH_PROXY 0x10000
struct each_proxy_st {
	id_t id,old_id;
	struct net_addr_st from_ni;
	unsigned char query[MAX_QUERY];
	int query_len;
	} each_proxy[MAX_EACH_PROXY];

id_t proxy_sequence[MAX_EACH_PROXY];
int next_proxy = 0;

int server_sock = 0;
int icann_sock = 0;
int handshake_sock = 0;
struct net_addr_st server_ni;
struct net_addr_st icann_ni;
struct net_addr_st handshake_ni[MAX_HANDSHAKE];
int num_handshake = 0;


struct stats_count_st { uint64_t count,bytes; };
struct stats_channel_st { struct stats_count_st qry,resp; };
struct stats_st { struct stats_channel_st client,icann,handshake; } stats;
char prom_file[PATH_MAX] = { 0 };

void stats_add(struct stats_count_st *c,int sz) 
{
	if (sz < 0) return;
	c->count++;
	c->bytes += sz;
}


int interupt = 0;
void sig(int s) { if (s==SIGALRM) exit(91); interupt=s; }


void stats_channel(FILE *prom_fp, char *who,struct stats_channel_st *ch)
{
	logmsg(MSG_HIGH,"%10s: QRY %ld pkts, %ld bytes - RESP %ld pkts, %ld bytes\n",
		who,ch->qry.count,ch->qry.bytes,ch->resp.count,ch->resp.bytes);
	if (!prom_fp) return;
	fprintf(prom_fp,"root_proxy_%s_query_count %ld\n",who,ch->qry.count);
	fprintf(prom_fp,"root_proxy_%s_query_bytes %ld\n",who,ch->qry.bytes);
	fprintf(prom_fp,"root_proxy_%s_response_count %ld\n",who,ch->resp.count);
	fprintf(prom_fp,"root_proxy_%s_response_bytes %ld\n",who,ch->resp.bytes);
}


void start_interval()
{
FILE *prom_fp = NULL;

	if (prom_file[0]) prom_fp = fopen(prom_file,"w+");
	stats_channel(prom_fp,"client",&stats.client);
	stats_channel(prom_fp,"icann",&stats.icann);
	stats_channel(prom_fp,"handshake",&stats.handshake);
	if (prom_fp) fclose(prom_fp);
}


void process_handshake_option(char * arg)
{
	char * p = strdup(arg),*sv=NULL;
	char * sep = " ";
	if (strchr(p,',')!=NULL) sep = ",";
	if (strchr(p,';')!=NULL) sep = ";";

	num_handshake = 0;
	for(char *cp=strtok_r(p,sep,&sv);cp;cp=strtok_r(NULL,sep,&sv)) {
		if (decode_net_addr(&handshake_ni[num_handshake],cp)==0) {
			if (!handshake_ni[num_handshake].port) handshake_ni[num_handshake].port = PORT_DNS;
			num_handshake++;
			}
		}
}



struct each_proxy_st *get_proxy(char * who, unsigned char *pkt)
{
unsigned char *p = pkt;
id_t id_idx;

	GETSHORT(id_idx,p);
	struct each_proxy_st * t = &each_proxy[id_idx];
	logmsg(MSG_DEBUG,"%s: proxy %d has id %d, return %s:%d\n",who,id_idx,t->id,IPCHAR(t->from_ni),t->from_ni.port);
	if (!t->query_len) return NULL;
	return t;
}



int send_resp(struct each_proxy_st *t,unsigned char *packet,int pkt_len)
{
unsigned char *p = packet;

	logmsg(MSG_DEBUG,"response id %d to %s:%d\n",t->old_id,IPCHAR(t->from_ni),t->from_ni.port);

	PUTSHORT(t->old_id,p);
	int ret = write_udp_any(server_sock,&t->from_ni,packet,pkt_len);
	t->query_len = 0;
	stats_add(&stats.client.resp,ret);
	return ret;
}



int handle_handshake()
{
unsigned char packet[MAX_RESP];
int pkt_size = MAX_RESP;
struct net_addr_st from_ni;

    int pkt_len = read_udp_any(handshake_sock,handshake_ni[0].is_type,&from_ni,packet,pkt_size);
    if (pkt_len < 0) { logmsg(MSG_ERROR,"ERROR: failed to read from handshake - %s\n",ERRMSG); return 0; }

	struct each_proxy_st *this_proxy = get_proxy("handshake",packet);
	if (!this_proxy) { logmsg(MSG_DEBUG,"handshake extra respose dropped"); return 0; }

	stats_add(&stats.handshake.resp,pkt_len);

	return send_resp(this_proxy,packet,pkt_len);
}



int send_to_handshake(struct each_proxy_st *t)
{
int all_bad = -1;

	logmsg(MSG_DEBUG,"icann said nxdomain, sending to handshake\n");
	for(int l=0;l<num_handshake;l++)
		if (write_udp_any(handshake_sock,&handshake_ni[l],t->query,t->query_len)==t->query_len) all_bad=0;
	if (!all_bad) stats_add(&stats.handshake.qry,t->query_len);
	return all_bad;
}



int handle_icann()
{
unsigned char packet[MAX_RESP];
int pkt_size = MAX_RESP;
struct net_addr_st from_ni;

	int pkt_len = read_udp_any(icann_sock,icann_ni.is_type,&from_ni,packet,pkt_size);
	if (pkt_len < 0) { logmsg(MSG_ERROR,"ERROR: failed to read from icann - %s\n",ERRMSG); return 0; }
	stats_add(&stats.icann.resp,pkt_len);

	int rcode = packet[3] & 0xf;
	logmsg(MSG_DEBUG,"icann response %d bytes, rcode %d\n",pkt_len,rcode);

	struct each_proxy_st *this_proxy = get_proxy("icann",packet);
	if (!this_proxy) { puts("icann extra respose dropped"); return 0; }

	if (rcode == RCODE_NXDOMAIN) return send_to_handshake(this_proxy);

	return send_resp(this_proxy,packet,pkt_len);
}



int handle_query()
{
unsigned char *p;

	struct each_proxy_st *this_proxy = &each_proxy[proxy_sequence[next_proxy++]];
	next_proxy %= MAX_EACH_PROXY;
	this_proxy->old_id = this_proxy->query_len = 0;
	logmsg(MSG_DEBUG,"new id = %d\n",this_proxy->id);

	int pkt_size = sizeof(this_proxy->query);
	if ((this_proxy->query_len  = read_udp_any(server_sock,server_ni.is_type,&this_proxy->from_ni,this_proxy->query,pkt_size))<0) return -1;
	stats_add(&stats.client.qry,this_proxy->query_len);

	p = this_proxy->query;
	GETSHORT(this_proxy->old_id,p);

	logmsg(MSG_DEBUG,"read %d bytes, %s:%d, old id %d, new id %d\n",
		this_proxy->query_len,IPCHAR(this_proxy->from_ni),this_proxy->from_ni.port,this_proxy->old_id,this_proxy->id);

	p = this_proxy->query;
	PUTSHORT(this_proxy->id,p);

	int ret = write_udp_any(icann_sock,&icann_ni,this_proxy->query,this_proxy->query_len);
	if (ret < 0) { logmsg(MSG_ERROR,"ERROR: write to %s:%d - %s\n",IPCHAR(icann_ni),icann_ni.port,ERRMSG); return 0; }
	logmsg(MSG_DEBUG,"wrote %d bytes to %s:%d\n",ret,IPCHAR(icann_ni),icann_ni.port);
	stats_add(&stats.icann.qry,ret);

	return 0;
}



void end_sock(int sock)
{
	shutdown(sock,SHUT_RDWR);
	close(sock);
}



int main(int argc,char * argv[])
{
struct sigaction sa;
loglvl_t level = MSG_NORMAL|MSG_DEBUG|MSG_STDOUT|MSG_FILE_LINE;
time_t stats_interval = 10;
struct net_addr_st client_ni;

	init_log(argv[0],level);

	signal(SIGPIPE,SIG_IGN);
	ZERO(sa);
	sa.sa_handler = sig;
	sigaction(SIGINT,&sa,NULL);
	sigaction(SIGTERM,&sa,NULL);
	sigaction(SIGALRM,&sa,NULL);
	sigaction(SIGUSR1,&sa,NULL);

	ZERO(server_ni);
	ZERO(icann_ni);
	ZERO(client_ni);
	ZERO(stats);

	AZERO(handshake_ni);
	AZERO(each_proxy);

	srand(getpid()*time(NULL)*13);

	int opt;
	while ((opt=getopt(argc,argv,"c:s:i:h:l:S:p:")) > 0)
		{
		switch(opt)
			{
			case 'S': stats_interval = atoi(optarg); break;
			case 'c': decode_net_addr(&client_ni,optarg); break;
			case 's': decode_net_addr(&server_ni,optarg); break;
			case 'i': decode_net_addr(&icann_ni,optarg); break;
			case 'h': process_handshake_option(optarg); break;
			case 'p': strcpy(prom_file,optarg); break;
			case 'l': level = LEVEL(optarg); init_log(argv[0],level); break;
			}
		}


	if (!icann_ni.is_type) decode_net_addr(&icann_ni,"192.5.5.241"); // F-ROOT
	if (!server_ni.is_type) decode_net_addr(&server_ni,"127.0.0.9");
	if (!handshake_ni[0].is_type) process_handshake_option("192.168.8.31,192.168.8.41");
	if (!client_ni.is_type) decode_net_addr(&client_ni,"0.0.0.0");

	if (!server_ni.port) server_ni.port = PORT_DNS;
	if (!icann_ni.port) icann_ni.port = PORT_DNS;

	for(int l=0;l<num_handshake;l++)
		logmsg(MSG_DEBUG,"handshake %s:%d\n",IPCHAR(handshake_ni[l]),handshake_ni[l].port);
	logmsg(MSG_DEBUG,"icann %s:%d\n",IPCHAR(icann_ni),icann_ni.port);
	logmsg(MSG_DEBUG,"server %s:%d\n",IPCHAR(server_ni),server_ni.port);
	logmsg(MSG_DEBUG,"client %s:%d\n",IPCHAR(client_ni),client_ni.port);

	icann_sock = udp_client_any(&client_ni,0);
	handshake_sock = udp_client_any(&client_ni,0);

	for(int l=0;l<MAX_EACH_PROXY;l++) proxy_sequence[l] = each_proxy[l].id = l;
	for(int l=0;l<MAX_EACH_PROXY;l++) {
		int pos = rand() % MAX_EACH_PROXY;
		id_t swap = proxy_sequence[pos];
		proxy_sequence[pos] = proxy_sequence[l];
		proxy_sequence[l] = swap;
		}

	server_sock = udp_server_any(&server_ni,0,0);
	logmsg(MSG_DEBUG,"server_sock = %d\n",server_sock);
	logmsg(MSG_NORMAL,"Running...\n");

	time_t now = time(NULL),next_stats = now+stats_interval;
	while(!interupt) {

		now = time(NULL);
		if (now > next_stats) {
			start_interval();
			next_stats = now+stats_interval;
			}

		fd_set read_fds;
		FD_ZERO(&read_fds);
		FD_SET(server_sock,&read_fds);
		FD_SET(icann_sock,&read_fds);
		FD_SET(handshake_sock,&read_fds);

		struct timeval tv;
		tv.tv_sec = 1; tv.tv_usec = 0;
		int ret = select(server_sock+1,&read_fds,NULL,NULL,&tv);

		if (ret < 0) break;
		if (ret == 0) continue;

		if (FD_ISSET(server_sock,&read_fds)) { if (handle_query() < 0) break; }
		if (FD_ISSET(icann_sock,&read_fds)) { if (handle_icann() < 0) break; }
		if (FD_ISSET(handshake_sock,&read_fds)) { if (handle_handshake() < 0) break; }
		}

	logmsg(MSG_DEBUG,"end\n");
	end_sock(server_sock);
	end_sock(icann_sock);
	end_sock(handshake_sock);
	return 0;
}
