#ifndef _CAT1_API_H_
#define _CAT1_API_H_

#include "cat1_UartDev.h"


/*send to device*/
#define      	AT_CMD_AT          		"AT\n"
#define		 	AT_CMD_ECHO				"ATE\n"
#define		 	AT_CMD_MANUFACTUREER	"AT+CGMI\n"
#define		 	AT_CMD_REVISION			"AT+CGMR\n"
#define		 	AT_CMD_IMEI				"AT+CGSN\n"
#define		 	AT_CMD_IMSI				"AT+CIMI\n"
#define 	 	AT_CMD_CGCONTRDP		"AT+CGCONTRDP\n"
#define		 	AT_CMD_CSQ				"AT%CSQ\n"
#define      	AT_CMD_SRVTYPE     		"AT^SISS=0,srvType,Socket\n"
#define 		AT_CMD_CONID			"AT^SISS=0,conId,0\n"
#define			AT_CMD_INIT				"AT^SICI=0\n"
#define			AT_CMD_OPEN				"AT^SISO=0\n"
#define			AT_CMD_CLOSE			"AT^SISC=0\n"
#define			AT_CMD_RECEIVE			"AT^SISR=0\n"
//#define			AT_CMD_SENDDATA			"AT^SISW=0,%s\n"
#define			AT_CMD_SEVER_IP			"AT^SISS=0,address,%s\n"

#define			AT_CMD_RECEICE_CHARACTER		"AT^SISHR=0\n"
#define			AT_CMD_SENDDATA_CHARACTER		"AT^SISH=0,%s\n"



#define			AT_CMD_SERVER_PORT		"AT^SISS=0,port,%s\n"
#define			AT_CMD_RESET			"AT%EXE=\"reboot\"\n"

/*recv from device*/
#define 		AT_CMD_OK				"OK"
#define 		AT_CMD_ERROR			"ERROR"
#define			AT_CMD_INVALID_CMD		"Invalid AT Command Format"

/*ip and port*/
//#define         AT_CMD_IP				"\"116.228.171.27\""
//#define			AT_CMD_PORT			"\"6800\""
#define         AT_CMD_IP				"\"113.59.226.244\""
#define			 AT_CMD_PORT				"\"1883\""






extern void  Characters_Converts_Hex1(char *buf,char *src,int len);
extern void Characters_Converts_char(char* des, char *src,int length);
extern Cat1_return_t cat1_health_check(void);
extern Cat1_return_t cat1_server_init(uint8_t *ip, uint16_t *port);
//Cat1_return_t cat1_server_init(uint8_t *ip, uint16_t *port);
//huyuxiang
extern Cat1_return_t cat1_reset(void);
extern Cat1_return_t cat1_open(void);
extern Cat1_return_t cat1_close(void);
extern Cat1_return_t cat1_send(uint16_t *buf, uint16_t len);
extern Cat1_return_t cat1_recv(uint16_t *buf, uint16_t *len);

extern Cat1_return_t cat1_get_manufacturer(char * manufacturer);
extern Cat1_return_t cat1_get_revision(char * revision);
extern Cat1_return_t cat1_get_IMEI(char * IMEI);
extern Cat1_return_t cat1_get_IMSI(char * IMSI);
extern Cat1_return_t cat1_get_SQ(char *SQ);
extern Cat1_return_t cat1_get_local_ip(char * IP);
extern Cat1_return_t cat1_loopback(void);
extern Cat1_return_t send_AT_cmd(uint8_t *cmdSend , uint8_t *cmdRecv,int timeoutMs);



#endif
