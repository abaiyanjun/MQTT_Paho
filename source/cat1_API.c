
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "cat1_API.h"





#define DEBUG_TEST

#ifdef DEBUG_TEST
#define DBG(x,args...) printf(x,args)
#else
#define DBG(x,args...)
#endif

/*****************************************************************************/
/*  API INTERFACE                                       */
/*****************************************************************************/


void  Characters_Converts_Hex(char *buf,char *src,int len)
{
	int i =0 ;
	unsigned char temp,value;

	char *dest;
	dest = buf;

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
		//*dest++ = *src++;
		len--;
	}
	*dest++ ='\r';
	*dest ='\n';
}



void Characters_Converts_char(char* des, char *src,int length)
{
		int i,j;

	for(j=0; j<length; j++)
	{
		*des = 0;
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


/*
 * @brief judgement the woking state
 *
 *
 * @return the cat1 State of value .>0 is error
 */

Cat1_return_t cat1_health_check(void)
{
	Cat1_return_t ret;
	uint8_t cmdRecv[Cat1_RX_BUFFER_SIZE];
	char *token = NULL;
	memset(cmdRecv,0,sizeof(cmdRecv));

	ret = send_AT_cmd(AT_CMD_AT,cmdRecv,CAT1_TIMEOUT);
	DBG("-------sendCmd-------\n%s",AT_CMD_AT);
	if (ret == Cat1_STATUS_TIMEOUT) {
		return ret;

	} else if (ret == Cat1_STATUS_SUCCESS) {

		DBG("-------cmdRecv-------\n%s\n",cmdRecv);
		token = strtok(cmdRecv, "\r\n");
		char *atCmd = token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *status = token;
		DBG("atCmd[%s],status[%s]\n", atCmd, status);

		if (!memcmp(status,AT_CMD_OK,sizeof(AT_CMD_OK)-1)) {
			return Cat1_STATUS_SUCCESS;
		} else if (!memcmp(status,AT_CMD_ERROR,sizeof(AT_CMD_ERROR)-1)) {
			return Cat1_STATUS_FAILED;
		} else {
			return Cat1_STATUS_INVALID_PARMS;
		}
	}
}



/*
 * @brief config the information to connnect the serve
 *
 * @param[in] ip - connnect the serve ip address
 * @param[in] port - connnect the serve port
 *
 * @return the cat1 state of value. > 0 is error
 */


Cat1_return_t cat1_server_init(uint8_t *ip, uint16_t *port)
{
	int i = 0;
	Cat1_return_t ret;
	uint8_t cmdRecv[100];
	uint8_t *atCmd;
	uint8_t *status ;
	uint8_t data1[100];
	uint8_t data2[100];
	char *token = NULL;
	
	ret = Cat1_DriverInit();
	if(ret == Cat1_STATUS_SUCCESS) {
		printf("Cat1 Uart_1 initialization SUCCESSFUL\r\n");
	} else {
		printf("Cat1 Uart_1 initialization FAILED\r\n");
	}

	do
	{
		ret=cat1_health_check();
		vTaskDelay(CAT1_DELAY_1000MS);
		i++;
	}while(ret != Cat1_STATUS_SUCCESS && i<10);
	
	vTaskDelay(16000);
	sprintf(data1,AT_CMD_SEVER_IP,ip);
	sprintf(data2,AT_CMD_SERVER_PORT,port);

	
	vTaskDelay(CAT1_DELAY_1000MS);
	memset(cmdRecv,0,sizeof(cmdRecv));
	printf("AT_CMD_CLOSE---\n");
	ret=  send_AT_cmd(AT_CMD_CLOSE,cmdRecv,CAT1_TIMEOUT);
	printf("-----[ret=%d]cmdRecv--\n%s",ret,cmdRecv);
 
	vTaskDelay(CAT1_DELAY_1000MS);
	memset(cmdRecv,0,sizeof(cmdRecv));
	printf("AT_CMD_CLOSE---\n");
	ret=  send_AT_cmd(AT_CMD_CLOSE,cmdRecv,CAT1_TIMEOUT);
	printf("-----[ret=%d]cmdRecv--\n%s",ret,cmdRecv);

	vTaskDelay(CAT1_DELAY_1000MS);
	memset(cmdRecv,0,sizeof(cmdRecv));
	printf("AT_CMD_CLOSE---\n");
	ret=  send_AT_cmd(AT_CMD_CLOSE,cmdRecv,CAT1_TIMEOUT);
	printf("-----[ret=%d]cmdRecv--\n%s",ret,cmdRecv);
	
	vTaskDelay(CAT1_DELAY_1000MS);
	memset(cmdRecv,0,sizeof(cmdRecv));
	printf("AT_CMD_SRVTYPE---\n");
	ret= send_AT_cmd(AT_CMD_SRVTYPE,cmdRecv,CAT1_TIMEOUT);
	printf("-----[ret=%d]cmdRecv--\n%s",ret,cmdRecv);
	  
	vTaskDelay(CAT1_DELAY_1000MS);
	memset(cmdRecv,0,sizeof(cmdRecv));
	printf("AT_CMD_CONID---\n");
	ret=  send_AT_cmd(AT_CMD_CONID,cmdRecv,CAT1_TIMEOUT);
	printf("-----[ret=%d]cmdRecv--\n%s",ret,cmdRecv);
	
	vTaskDelay(CAT1_DELAY_1000MS);
	memset(cmdRecv,0,sizeof(cmdRecv));
	printf("AT_CMD_SEVER_IP---\n");
	ret=  send_AT_cmd(data1,cmdRecv,CAT1_TIMEOUT);
	printf("-----[ret=%d]cmdRecv--\n%s",ret,cmdRecv);
	
	vTaskDelay(CAT1_DELAY_1000MS);
	memset(cmdRecv,0,sizeof(cmdRecv));
	printf("AT_CMD_SERVER_PORT---\n");
	ret=  send_AT_cmd(data2,cmdRecv,CAT1_TIMEOUT);
	printf("-----[ret=%d]cmdRecv--\n%s",ret,cmdRecv);
	  
	vTaskDelay(CAT1_DELAY_1000MS);
	memset(cmdRecv,0,sizeof(cmdRecv));
	printf("AT_CMD_INIT---\n");
	ret= send_AT_cmd(AT_CMD_INIT,cmdRecv,CAT1_TIMEOUT);
	printf("-----[ret=%d]cmdRecv--\n%s",ret,cmdRecv);
	
	vTaskDelay(CAT1_DELAY_1000MS);
	memset(cmdRecv,0,sizeof(cmdRecv));
	printf("AT_CMD_INIT---\n");
	ret= send_AT_cmd(AT_CMD_INIT,cmdRecv,CAT1_TIMEOUT);
	printf("-----[ret=%d]cmdRecv--\n%s",ret,cmdRecv);
	 
	vTaskDelay(CAT1_DELAY_1000MS);
	memset(cmdRecv,0,sizeof(cmdRecv));
	printf("AT_CMD_OPEN---\n");
	ret= send_AT_cmd(AT_CMD_OPEN,cmdRecv,CAT1_TIMEOUT);
	printf("-----[ret=%d]cmdRecv--\n%s",ret,cmdRecv);


	if (ret == Cat1_STATUS_TIMEOUT) {
		
		return ret;

	} else if (ret == Cat1_STATUS_SUCCESS) {

		DBG("-------cmdRecv---AT_CMD_OPEN----\n%s\n",cmdRecv);

		token = strtok(cmdRecv, "\r\n");
		char *atCmd = token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *status = token;
		DBG("atCmd[%s],status[%s]\n", atCmd, status);
		DBG("ret2[%d]\n", ret);
		if (!memcmp(status,AT_CMD_OK,sizeof(AT_CMD_OK)-1)) {
			return Cat1_STATUS_SUCCESS;
		} else if (!memcmp(status,AT_CMD_ERROR,sizeof(AT_CMD_ERROR)-1)) {
			return Cat1_STATUS_FAILED;
		} else {
			return Cat1_STATUS_INVALID_PARMS;
		}
		}
}


/*
 * @brief reset the cat1 module
 *
 *
 * @return the cat1 State of value .>0 is error
 */
 

Cat1_return_t cat1_reset(void)
{
	int i = 0;
	Cat1_return_t ret;
	uint8_t cmdRecv[Cat1_RX_BUFFER_SIZE];
	char *token = NULL;
	memset(cmdRecv,0,sizeof(cmdRecv));
	
	do
	{
		ret=cat1_health_check();
		vTaskDelay(CAT1_DELAY_1000MS);
		i++;
	}while(ret != Cat1_STATUS_SUCCESS && i<10);

	ret = send_AT_cmd(AT_CMD_RESET,cmdRecv,CAT1_TIMEOUT);
	DBG("-------sendCmd-------\n%s",AT_CMD_RESET);
	if (ret == Cat1_STATUS_TIMEOUT) {
		return ret;

	} else if (ret == Cat1_STATUS_SUCCESS) {

		DBG("-------cmdRecv-------\n%s\n",cmdRecv);

		token = strtok(cmdRecv, "\r\n");
		char *atCmd = token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *status = token;
		DBG("atCmd[%s],status[%s]\n", atCmd, status);
		if (!memcmp(status,AT_CMD_OK,sizeof(AT_CMD_OK)-1)) {
			return Cat1_STATUS_SUCCESS;
		} else if (!memcmp(status,AT_CMD_ERROR,sizeof(AT_CMD_ERROR)-1)) {
			return Cat1_STATUS_FAILED;
		} else {
			return Cat1_STATUS_INVALID_PARMS;
		}
	}

}


/*
 * @brief close the serve links
 *
 *
 * @return the cat1 State of value .>0 is error
 */
 
 

Cat1_return_t cat1_close(void)
{
	int i = 0;
	Cat1_return_t ret;
	uint8_t cmdRecv[Cat1_RX_BUFFER_SIZE];
	char *token = NULL;
	memset(cmdRecv,0,sizeof(cmdRecv));

	do
	{
		ret=cat1_health_check();
		vTaskDelay(CAT1_DELAY_1000MS);
		i++;
	}while(ret != Cat1_STATUS_SUCCESS && i<10);

	ret = send_AT_cmd(AT_CMD_CLOSE,cmdRecv,CAT1_TIMEOUT);
	DBG("-------sendCmd-------\n%s",AT_CMD_CLOSE);
	if (ret == Cat1_STATUS_TIMEOUT) {

		return ret;

	} else if (ret == Cat1_STATUS_SUCCESS) {

		DBG("-------cmdRecv-------\n%s\n",cmdRecv);

		token = strtok(cmdRecv, "\r\n");
		char *atCmd = token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *status = token;
		DBG("atCmd[%s],status[%s]\n", atCmd, status);
		if (!memcmp(status,AT_CMD_OK,sizeof(AT_CMD_OK)-1)) {
			return Cat1_STATUS_SUCCESS;
		} else if (!memcmp(status,AT_CMD_ERROR,sizeof(AT_CMD_ERROR)-1)) {
			return Cat1_STATUS_FAILED;
		} else {
			return Cat1_STATUS_INVALID_PARMS;
		}
	}
}


/*
 * @brief send the buf data 
 *
 * @param[in] buf - send the data to 
 * @param[in] len - the data length
 *
 * @return the cat1 state of value. > 0 is error
 */

Cat1_return_t cat1_send(uint16_t *buf, uint16_t len)
{
	Cat1_return_t ret;
	uint8_t cmdRecv[Cat1_RX_BUFFER_SIZE];
	char *token = NULL;  
	uint8_t buffer[400];
	uint8_t  data_buf[800];
	memset(buffer,0,sizeof(buffer));
	memset(cmdRecv,0,sizeof(cmdRecv));
	memset(data_buf,0,sizeof(data_buf));
	

	
	Characters_Converts_Hex(buffer,buf,len);

    sprintf(data_buf,AT_CMD_SENDDATA_CHARACTER,buffer);

	DBG("-------start-------%d\n%s",strlen(data_buf),data_buf);

	ret = send_send_cmd(data_buf,cmdRecv,CAT1_TIMEOUT);
	DBG("-------sendCmd-------%d\n%s",strlen(data_buf),data_buf);
	DBG("len[%d]\n",len);
	if(len > Cat1_RX_BUFFER_SIZE){
		printf(" the data is overflow!\n");
		return Cat1_STATUS_FAILED;
	}else{
		if (ret == Cat1_STATUS_TIMEOUT) {
			return ret;
			
		} else if (ret == Cat1_STATUS_SUCCESS) {

			DBG("-------cmdRecv-------\n%s\n",cmdRecv);

			token = strtok(cmdRecv, "\r\n");
			char *atCmd = token;  
			if (token != NULL) {  
				token = strtok(NULL, "\r\n");  
			}  
			char *status = token;  
				
			DBG("atCmd[%s],status[%s]\n", atCmd, status);
			
			if (!memcmp(status,AT_CMD_OK,sizeof(AT_CMD_OK)-1)) {
				return Cat1_STATUS_SUCCESS;
			} else if (!memcmp(status,AT_CMD_ERROR,sizeof(AT_CMD_ERROR)-1)) {
				return Cat1_STATUS_FAILED;
			} else {
				return Cat1_STATUS_INVALID_PARMS;
			}
		}
	}	
}



/*
* @brief receive the data from the serve
*
* @param[in] buf- receive the data
* @param[in] len - receive the data length
*
* @return the cat1 state of value. > 0 is error
*/

Cat1_return_t cat1_recv(uint16_t *buf, uint16_t *len)
{	
	Cat1_return_t ret;
	uint16_t cmdRecv[Cat1_RX_BUFFER_SIZE];

	char *token = NULL;
	char *token1 = NULL;
	memset(cmdRecv,0,sizeof(cmdRecv));

	ret = send_recv_cmd(AT_CMD_RECEICE_CHARACTER,cmdRecv,CAT1_TIMEOUT);
	DBG("-------sendCmd-------\n%s",AT_CMD_RECEICE_CHARACTER);
	if (ret == Cat1_STATUS_TIMEOUT) {
		return ret;

	} else if (ret == Cat1_STATUS_SUCCESS) {

		DBG("-------cmdRecv-------\n%s\n",cmdRecv);

		token = strtok(cmdRecv, "\r\n");
		char *atCmd = token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		   char *Cmddata=token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *status = token;
		
	
		token1 = strtok(Cmddata,":");
		char *Cmddata1=token1;
		if (token1 != NULL) {
			token1 = strtok(NULL, "\r\n");
		}
		char *Cmddata2 = token1;
		
		Characters_Converts_char(buf,Cmddata2,strlen(Cmddata2));
		*len = strlen(buf);
		DBG("-------cmdRecv-------\n%s\n",Cmddata2);

		if (!memcmp(status,AT_CMD_OK,sizeof(AT_CMD_OK)-1)) {
			return Cat1_STATUS_SUCCESS;
		} else if (!memcmp(status,AT_CMD_ERROR,sizeof(AT_CMD_ERROR)-1)) {
			return Cat1_STATUS_FAILED;
		} else {
			return Cat1_STATUS_INVALID_PARMS;
		}

	}
}

/*
 * @brief  get the manufacturer information from the cat1 module
 *
 * @param[in] manufacturer - the manufacturer information
 *
 * @return the cat1 state of value. > 0 is error
 */

 Cat1_return_t cat1_get_manufacturer(char *manufacturer)
 {
 	
 	Cat1_return_t ret;
	uint8_t cmdRecv[Cat1_RX_BUFFER_SIZE];
	char *token = NULL;
	memset(cmdRecv,0,sizeof(cmdRecv));

	

	ret = send_AT_cmd(AT_CMD_MANUFACTUREER,cmdRecv,CAT1_TIMEOUT);
	DBG("-------sendCmd-------\n%s",AT_CMD_MANUFACTUREER);
	if (ret == Cat1_STATUS_TIMEOUT) {
		return ret;

	} else if (ret == Cat1_STATUS_SUCCESS) {

		DBG("-------cmdRecv-------\n%s\n",cmdRecv);

		token = strtok(cmdRecv, "\r\n");
		char *atCmd = token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *Cmddata=token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *status = token;
		strcpy(manufacturer,Cmddata);
		DBG("atCmd[%s]\nmanufacturer[%s]\nstatus[%s]",atCmd,manufacturer, status);

		if (!memcmp(status,AT_CMD_OK,sizeof(AT_CMD_OK)-1)) {
			return Cat1_STATUS_SUCCESS;
		} else if (!memcmp(status,AT_CMD_ERROR,sizeof(AT_CMD_ERROR)-1)) {
			return Cat1_STATUS_FAILED;
		} else {
			return Cat1_STATUS_INVALID_PARMS;
		}
	}
 }


/*
 * @brief  get the revision  information from the cat1 module
 *
 * @param[in] revision - the revsion information
 *
 * @return the cat1 state of value. > 0 is error
 */

 Cat1_return_t cat1_get_revision(char * revision)
 {
 	
 	Cat1_return_t ret;
	uint8_t cmdRecv[Cat1_RX_BUFFER_SIZE];
	char *token = NULL;
	memset(cmdRecv,0,sizeof(cmdRecv));

	

	ret = send_AT_cmd(AT_CMD_REVISION,cmdRecv,CAT1_TIMEOUT);
	DBG("-------sendCmd-------\n%s",AT_CMD_REVISION);
	if (ret == Cat1_STATUS_TIMEOUT) {
		return ret;

	} else if (ret == Cat1_STATUS_SUCCESS) {

		DBG("-------cmdRecv-------\n%s\n",cmdRecv);

		token = strtok(cmdRecv, "\r\n");
		char *atCmd = token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *Cmddata=token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *status = token;
		strcpy(revision,Cmddata);
		DBG("atCmd[%s]\nrevision[%s]\nstatus[%s]\n",atCmd,revision, status);

		if (!memcmp(status,AT_CMD_OK,sizeof(AT_CMD_OK)-1)) {
			return Cat1_STATUS_SUCCESS;
		} else if (!memcmp(status,AT_CMD_ERROR,sizeof(AT_CMD_ERROR)-1)) {
			return Cat1_STATUS_FAILED;
		} else {
			return Cat1_STATUS_INVALID_PARMS;
		}
	}
 }




/*
 * @brief  get the IMEI information from the cat1 module
 *
 * @param[in] IMEI - the IMEI information
 *
 * @return the cat1 state of value. > 0 is error
 */

 Cat1_return_t cat1_get_IMEI(char * IMEI)
{

 	Cat1_return_t ret;
	uint8_t cmdRecv[Cat1_RX_BUFFER_SIZE];
	char *token = NULL;
	memset(cmdRecv,0,sizeof(cmdRecv));
	

	ret = send_AT_cmd(AT_CMD_IMEI,cmdRecv,CAT1_TIMEOUT);
	DBG("-------sendCmd-------\n%s",AT_CMD_IMEI);
	if (ret == Cat1_STATUS_TIMEOUT) {
		return ret;

	} else if (ret == Cat1_STATUS_SUCCESS) {

		DBG("-------cmdRecv-------\n%s\n",cmdRecv);

		token = strtok(cmdRecv, "\r\n");
		char *atCmd = token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *Cmddata=token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *status = token;
		strcpy(IMEI,Cmddata);
		DBG("atCmd[%s]\nrevision[%s]\nstatus[%s]\n",atCmd,IMEI, status);

		if (!memcmp(status,AT_CMD_OK,sizeof(AT_CMD_OK)-1)) {
			return Cat1_STATUS_SUCCESS;
		} else if (!memcmp(status,AT_CMD_ERROR,sizeof(AT_CMD_ERROR)-1)) {
			return Cat1_STATUS_FAILED;
		} else {
			return Cat1_STATUS_INVALID_PARMS;
		}
	}
}


/*
 * @brief  get the SIM card information from the cat1 module
 *
 * @param[in] IMSI - the SIM card information
 *
 * @return the cat1 state of value. > 0 is error
 */

 Cat1_return_t cat1_get_IMSI(char * IMSI)
{
	
	Cat1_return_t ret;
	uint8_t cmdRecv[Cat1_RX_BUFFER_SIZE];
	char *token = NULL;
	memset(cmdRecv,0,sizeof(cmdRecv));

	
	
	ret = send_AT_cmd(AT_CMD_IMSI,cmdRecv,CAT1_TIMEOUT);
	DBG("-------sendCmd-------\n%s",AT_CMD_IMSI);
	if (ret == Cat1_STATUS_TIMEOUT) {
		return ret;
	} else if (ret == Cat1_STATUS_SUCCESS) {
		DBG("-------cmdRecv-------\n%s\n",cmdRecv);
		token = strtok(cmdRecv, "\r\n");
		char *atCmd = token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *Cmddata=token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *status = token;
		strcpy(IMSI,Cmddata);
		DBG("atCmd[%s]\nrevision[%s]\nstatus[%s]\n",atCmd,IMSI, status);
		if (!memcmp(status,AT_CMD_OK,sizeof(AT_CMD_OK)-1)) {
			return Cat1_STATUS_SUCCESS;
		} else if (!memcmp(status,AT_CMD_ERROR,sizeof(AT_CMD_ERROR)-1)) {
			return Cat1_STATUS_FAILED;
		} else {
			return Cat1_STATUS_INVALID_PARMS;
		}
	}
}


/*
 * @brief  get the signal quality information from the cat1 module
 *
 * @param[in] SQ - the signal quality information
 *
 * @return the cat1 state of value. > 0 is error
 */

 Cat1_return_t  cat1_get_SQ(char *SQ)
{

	Cat1_return_t ret;
	uint8_t cmdRecv[Cat1_RX_BUFFER_SIZE];
	char *token = NULL;
	memset(cmdRecv,0,sizeof(cmdRecv));


	
	ret = send_AT_cmd(AT_CMD_CSQ,cmdRecv,CAT1_TIMEOUT);
	DBG("-------sendCmd-------\n%s",AT_CMD_CSQ);
	if (ret == Cat1_STATUS_TIMEOUT) {
		return ret;

	} else if (ret == Cat1_STATUS_SUCCESS) {

		DBG("-------cmdRecv-------\n%s\n",cmdRecv);

		token = strtok(cmdRecv, "\r\n");
		char *atCmd = token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *Cmddata=token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *status = token;
		strcpy(SQ,Cmddata);
		DBG("atCmd[%s]\nrevision[%s]\nstatus[%s]\n",atCmd,Cmddata, status);

		if (!memcmp(status,AT_CMD_OK,sizeof(AT_CMD_OK)-1)) {
			return Cat1_STATUS_SUCCESS;
		} else if (!memcmp(status,AT_CMD_ERROR,sizeof(AT_CMD_ERROR)-1)) {
			return Cat1_STATUS_FAILED;
		} else {
			return Cat1_STATUS_INVALID_PARMS;
		}
	}
}


 /*
 * @brief  get the IP information from the cat1 module
 *
 * @param[in] IP - the IP information
 *
 * @return the cat1 state of value. > 0 is error
 */
 
 Cat1_return_t cat1_get_local_ip(char * IP)
{
	
	Cat1_return_t ret;
	uint8_t cmdRecv[Cat1_RX_BUFFER_SIZE];
	char *token = NULL;
	memset(cmdRecv,0,sizeof(cmdRecv));

	
	
	ret = send_AT_cmd(AT_CMD_CGCONTRDP,cmdRecv,CAT1_TIMEOUT);
	DBG("-------sendCmd-------\n%s",AT_CMD_CGCONTRDP);
	if (ret == Cat1_STATUS_TIMEOUT) {
		return ret;

	} else if (ret == Cat1_STATUS_SUCCESS) {

		DBG("-------cmdRecv-------\n%s\n",cmdRecv);

		token = strtok(cmdRecv, "\r\n");
		char *atCmd = token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *cmd = token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *Cmddata=token;
		if (token != NULL) {
			token = strtok(NULL, "\r\n");
		}
		char *status = token;
		strcpy(IP,cmd);
		DBG("atCmd[%s]\ncmd[%s]\nCmddata[%s]\nstatus[%s]\n",atCmd,cmd,IP, status);
		if (!memcmp(status,AT_CMD_OK,sizeof(AT_CMD_OK)-1)) {
			return Cat1_STATUS_SUCCESS;
		} else if (!memcmp(status,AT_CMD_ERROR,sizeof(AT_CMD_ERROR)-1)) {
			return Cat1_STATUS_FAILED;
		} else {
			return Cat1_STATUS_INVALID_PARMS;
		}
	}
}


  
