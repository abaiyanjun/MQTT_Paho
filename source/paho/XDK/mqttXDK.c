/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander - initial API and implementation and/or initial documentation
 *******************************************************************************/
/******************************************************************************
**	COPYRIGHT (c) 2016		Bosch Connected Devices and Solutions GmbH
**
**	The use of this software is subject to the XDK SDK EULA
**
*******************************************************************************
**
**	OBJECT NAME:	mqttXDK.c
**
**	DESCRIPTION:	Links the Paho MQTT Client to the XDK Platform
**
**	PURPOSE:        Defines the timer and network functions
**	                the Paho Stack uses to interface with the platform
**
**	AUTHOR(S):      Bosch Connected Devices and Solutions GmbH (BCDS)
**
**	Revision History:
**
**	Date			 Name		Company      Description
**  2016.Apr         crk        BCDS         Edited Paho's CC3200 Platform source
**                                           to port code to the XDK Platform
**
*******************************************************************************/

/* system header files */
#include <stdint.h>

/* interface header files */
#include "BCDS_PowerMgt.h"

/* own header files */
#include "mqttXDK.h"

//#define MQTT_BROKER_NAME    "113.59.226.244"  /**< MQTT Broker */
//#define MQTT_PORT           1883                          /**< MQTT Port Number */
#define MQTT_BROKER_NAME    "114.55.42.28"  /**< MQTT Broker */
#define MQTT_PORT           26500                          /**< MQTT Port Number */
#define WIFI_RESET_THRESHOLD 1800
#define AT_BASE_LENGTH 27
#define TIME_OUT   5000

int flag = 0;

int readcount = 0;
int writecount=0;


char expired(Timer* timer) {
    long left = timer->end_time - PowerMgt_GetSystemTimeMs();
    return (left < 0);
}


void countdown_ms(Timer* timer, unsigned int timeout) {
    timer->end_time = PowerMgt_GetSystemTimeMs() + timeout;
}


void countdown(Timer* timer, unsigned int timeout) {
    timer->end_time = PowerMgt_GetSystemTimeMs() + (timeout * 1000);
}


int left_ms(Timer* timer) {
    long left = timer->end_time - PowerMgt_GetSystemTimeMs();
    return (left < 0) ? 0 : left;
}


void InitTimer(Timer* timer) {
    timer->end_time = 0;
}
#if 1
void char_to_hex(char* des, char *src,int length)
{
	int i,j;

	for(j=0; j<length; j++)
	{
		*des=0;
		for (i = 0; i < 2; i++)
		{
			*des = *des<<4;
			if (*src >= '0' && *src <= '9')
			{
					*des += *src++ - '0';							
					//*des += *src++ - 0x30;
					
			}
			else if (*src >= 'A' && *src <= 'F')
			{
					*des += *src++ - 'A' + 10;
					//*des += *src++ - 0x37;
			}
			else if (*src >= 'a' && *src <= 'f')
			{
					*des += *src++ - 'a' + 10;
					//*des += *src++ - 0x37;
			}
		}
		des++;
	}
}


int hex_to_char(char *buff, const char *src, int len)
{
	int rc=0;
	unsigned char temp,value;
	char * dest;
	dest=buff;
	while(len)
	{ 
		temp = (unsigned char )*src++;
		value = temp &0x0f;
		temp = temp >>4;
		if(temp < 10)
			*dest++ = temp + 0x30; 
		
		else
			*dest++ = temp + 0x37;  //why is 0x31 because of lacking of 0x40
	
		if(value < 10)
			*dest++ = value + 0x30;
			
		else
			*dest++ = value + 0x37;

		len--;  
		
	} 
	*dest++ ='\r';
	*dest ='\n';
	
	return rc;
}

#endif


#ifdef XDK
int xdk_read(Network* n, uint8_t* buffer, int len, int timeout_ms) {

	int rc =0;
	int i;
	char *str = NULL;
	char *token=NULL;
	uint8_t cmdRecv[1049];
	
	char buf[12]="AT^SISHR=0\r\n";
	memset(cmdRecv,0,1049);
	//memset(buffer,0x00,strlen(buffer));


	//send_AT_cmd(buf,cmdRecv, TIME_OUT);
	vTaskDelay(1000);
	send_recv_cmd(buf,cmdRecv, TIME_OUT);


	/****jumpe the length of cmdRecv*****/
	
	printf("the cmdRecv length is %d\r\n",strlen(cmdRecv));
	printf("the cmdRecv is %s\n",cmdRecv);
	if(AT_BASE_LENGTH >= strlen(cmdRecv))
		return rc;

	printf("----------------1------------------------------");

	/***** has ^SISHR: or not ******/
	str = strstr(cmdRecv, "AT^SISHR=0");
	if(str == NULL)
		return rc;

	/***** when Socket is not connect, the dog timer will restart the xdk ******/
	str = strstr(cmdRecv, "Socket");   
	if(str != NULL)
		while(1);

	/***** when ERROR, the dog timer will restart the xdk ******/
	str = strstr(cmdRecv, "ERROR");   
	if( NULL != str)
		while(1);

	printf("-----------------2-------------------\r\n");


	//vTaskDelay(1000);
	token = strtok(cmdRecv, "\r\n");
	char *atCmd = token;
	if (token != NULL) {
		token = strtok(NULL, ":");
	}
	char *echo=token;
	if (token != NULL) {
		token = strtok(NULL, "\r\n");
	}

	char *data = token;
	
	//token = strtok(Cmddata, "\r\n");
	if (token != NULL) {
		token = strtok(NULL, "\r\n");
	}
	char *status = token;
	printf("atCmd is :%s\r\n",atCmd);
	printf("echo is %s\r\n",echo);
	printf("data is :\r\n%s\r\n",data);
	printf("status is :\r\n%s\r\n",status);

	if (!memcmp(status,"OK",sizeof("OK")-1)) {


		char_to_hex(buffer,data,strlen(data));
		#if 0
		for (i = 0;i < strlen(data);i ++) {
			printf("bufferLen[%d]\r\n",i);
			printf("%02x\t",buffer[i]);
		}
		#endif
	}

	rc = 1;
	return rc;
}

#else
int xdk_read(Network *n, unsigned char *buffer, int len, int timeout_ms)
{
    SlTimeval_t timeVal;
    SlFdSet_t fdset;
    int rc = 0;
    int recvLen = 0;
	int readySock = -1;

    SL_FD_ZERO(&fdset);
    SL_FD_SET(n->my_socket, &fdset);


    timeVal.tv_sec = 0;
    timeVal.tv_usec = timeout_ms * 1000;
    readySock = sl_Select(n->my_socket + 1, &fdset, NULL, NULL, &timeVal);
    DBG1("mqtt read timer=%d ms", timeout_ms);
    if (readySock < 0) {
        DBG1("mqtt read(select) fail ret=%d", readySock);
		wifiInit();
		sl_Close(n->my_socket);
		ConnectNetwork(&n, MQTT_BROKER_NAME, MQTT_PORT);
        return -1;
    } else if (readySock == 0) {
        DBG1("mqtt read(select) timeout");
		readcount++;
		if(readcount>WIFI_RESET_THRESHOLD)
		{
			wifiInit();
			sl_Close(n->my_socket);
			ConnectNetwork(&n, MQTT_BROKER_NAME, MQTT_PORT);
			readcount=0;
    	}
		printf("the readcount is %d\r\n",readcount);
        return -2;
    } else if (readySock == 1) {
        do {
			readcount=0;
            DBG1("mqtt read recv len = %d, recvlen = %d", len, recvLen);
            rc =sl_Recv(n->my_socket, buffer + recvLen, len - recvLen, 0);
            if (rc > 0) {
            recvLen += rc;
                DBG1("mqtt read ret=%d, rc = %d, recvlen = %d", readySock, rc, recvLen);
				printf("the readcount is %d\r\n",readcount);
            } else {
                DBG1("mqtt read fail: ret=%d, rc = %d, recvlen = %d", readySock, rc, recvLen);
				wifiInit();
				sl_Close(n->my_socket);
            	ConnectNetwork(&n, MQTT_BROKER_NAME, MQTT_PORT);
                return -3;
            }
        } while (recvLen < len);
    }
    return recvLen;
}

#endif

#ifdef XDK

int xdk_write(Network* n, uint8_t* buffer, int len, int timeout_ms) {
    //int rc = 0;

	int i = 0;

	uint8_t cmdRecv[1049];
	char *token = NULL;  
	char buff[1000]={0};
	//char buf[400]={0};
	char data[1000]={0};
	memset(cmdRecv,0,sizeof(cmdRecv));
	hex_to_char(buff,buffer,len);
    sprintf(data,"AT^SISH=0,%s",buff);
    // strcat(data,buff);
	printf("Wdata %s\nlength %d\n",data,strlen(data));

	//send_AT_cmd(data,cmdRecv,10000);
	send_send_cmd(data,cmdRecv,10000);

	printf("the send data is %s\r\n",cmdRecv);
	//vTaskDelay(1000);
	// SER_serialWrite(&Cat1_uartHandle, NULL, data, strlen(data)+2);
	
	//receive_data_from_device(cmdRecv);

    return len;
}

#else
int xdk_write(Network* n, uint8_t* buffer, int len, int timeout_ms) {
    SlTimeval_t timeVal;
    SlFdSet_t fdset;
    int rc = 0;
    int readySock = -1;

    SL_FD_ZERO(&fdset);
    SL_FD_SET(n->my_socket, &fdset);

    timeVal.tv_sec = 0;
    timeVal.tv_usec = timeout_ms * 1000;
	readySock = sl_Select(n->my_socket + 1, NULL, &fdset, NULL, &timeVal);
	DBG1("mqtt read timer=%d ms", timeout_ms);

	if(readySock < 0)
	{
		DBG1("mqtt write fail");
		wifiInit();
		sl_Close(n->my_socket);
     	ConnectNetwork(&n, MQTT_BROKER_NAME, MQTT_PORT);
		return -1;
	}
	else if (readySock == 0)
	{   
		DBG1("mqtt write timeout");
		writecount++;
		if(writecount>WIFI_RESET_THRESHOLD)
		{
			wifiInit();
			sl_Close(n->my_socket);
			ConnectNetwork(&n, MQTT_BROKER_NAME, MQTT_PORT);
			readcount=0;
		}
		printf("the writecount is %d\r\n",writecount);
		return -2;    
	}
	else if (readySock == 1) 
	{     
		writecount=0;
		 rc = sl_Send(n->my_socket, buffer, len, 0);
		 printf("the writecount is %d\r\n",writecount);
		 DBG1("the send data is %s\r\n",buffer);
	}
  return rc;
   
}

#endif

#if 1
void xdk_disconnect(Network* n) {
	uint8_t cmdRecv[1049];
    //sl_Close(n->my_socket);
    char data[20] = "AT^SISC=0\n";
	
    send_AT_cmd(data,cmdRecv,10000);
}
#else
void xdk_disconnect(Network* n) {
    sl_Close(n->my_socket);
}
#endif

void NewNetwork(Network* n) {
    n->my_socket = 0;
    n->mqttread = xdk_read;
    n->mqttwrite = xdk_write;
    n->disconnect = xdk_disconnect;
}

int TLSConnectNetwork(Network *n, char* addr, int port, SlSockSecureFiles_t* certificates, unsigned char sec_method, unsigned int cipher, char server_verify) {
	SlSockAddrIn_t sAddr;
	int addrSize;
	int retVal;
	unsigned long ipAddress;

	retVal = sl_NetAppDnsGetHostByName((_i8*) addr, strlen(addr), &ipAddress, AF_INET);
	if (retVal < 0) {
		return -1;
	}

	sAddr.sin_family = AF_INET;
	sAddr.sin_port = sl_Htons((unsigned short)port);
	sAddr.sin_addr.s_addr = sl_Htonl(ipAddress);

	addrSize = sizeof(SlSockAddrIn_t);

	n->my_socket = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, SL_SEC_SOCKET);
	if (n->my_socket < 0) {
		return -1;
	}

	SlSockSecureMethod method;
	method.secureMethod = sec_method;
	retVal = sl_SetSockOpt(n->my_socket, SL_SOL_SOCKET, SL_SO_SECMETHOD, &method, sizeof(method));
	if (retVal < 0) {
		return retVal;
	}

	SlSockSecureMask mask;
	mask.secureMask = cipher;
	retVal = sl_SetSockOpt(n->my_socket, SL_SOL_SOCKET, SL_SO_SECURE_MASK, &mask, sizeof(mask));
	if (retVal < 0) {
		return retVal;
	}

	if (certificates != NULL) {
		retVal = sl_SetSockOpt(n->my_socket, SL_SOL_SOCKET, SL_SO_SECURE_FILES, certificates->secureFiles, sizeof(SlSockSecureFiles_t));
		if(retVal < 0)
		{
			return retVal;
		}
	}

	retVal = sl_Connect(n->my_socket, ( SlSockAddr_t *)&sAddr, addrSize);
	if( retVal < 0 ) {
		if (server_verify || retVal != -453) {
			sl_Close(n->my_socket);
			return retVal;
		}
	}

	return retVal;
}

int ConnectNetwork(Network* n, char* addr, int port)
{
    SlSockAddrIn_t sAddr;
    int addrSize;
    int retVal = -1;
    unsigned long ipAddress;
    _SlReturnVal_t       RetVal1;

    sl_NetAppDnsGetHostByName((_i8*) addr, strlen((char*)addr), &ipAddress, AF_INET);

    sAddr.sin_family = AF_INET;
    sAddr.sin_port = sl_Htons((unsigned short)port);
    sAddr.sin_addr.s_addr = sl_Htonl(ipAddress);

    addrSize = sizeof(SlSockAddrIn_t);

    n->my_socket = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, 0);
    if( n->my_socket < 0 ) {
        // error
        DBG1("creat socket failed %d\r\n",n->my_socket);
        return -1;
    }

   // retVal = sl_Connect(n->my_socket, ( SlSockAddr_t *)&sAddr, addrSize);
    RetVal1 = sl_Connect(n->my_socket, ( SlSockAddr_t *)&sAddr, addrSize);
    if( RetVal1 < 0 ) {
        // error
        sl_Close(n->my_socket);
		DBG1("creat socket failed RetVal1 is %d\r\n",RetVal1);
        return retVal;
    }
	DBG1("creat socket %d\r\n",retVal);
    return retVal;
}
