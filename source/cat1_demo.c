
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "cat1_API.h"
#include "mqttPahoClient.h"
#include "mqttConfig.h"


static xTaskHandle      cat1TaskHandler  =   POINTER_NULL;  // task handle for MQTT Client

/**
 * @brief The function  send the data 
 */
void cat1_data_send_test(void)
{
	Cat1_return_t ret;
	char *buf = "#10#{\"type\":\"login\",\"eid\":\"mx12345678\"}#13#";

	ret = cat1_send(buf,strlen(buf));
	if (ret == Cat1_STATUS_SUCCESS) {
		printf("send data:%s\r\n",buf);
	} else { 
		printf("send data is fail!\n");
	}
}

void cat1_data_recv_test(void)
{
	int len = 0;
	Cat1_return_t ret;
	uint16_t cmdRecv[600];
	memset(cmdRecv,0,sizeof(cmdRecv));
	
	ret=cat1_recv(cmdRecv,&len);
	if(ret == Cat1_STATUS_SUCCESS){
		printf("recv data:%s \r\n",cmdRecv);
	} else if (ret == Cat1_STATUS_TIMEOUT){
		printf("recv data timeout ! \r\n");
	} else {
		printf("recv data failed ! \r\n");
	}
}


void cat1_Task(void)
{

	for(;;) {
		cat1_data_send_test();
		vTaskDelay(CAT1_DELAY_1000MS);
		cat1_data_recv_test();
		vTaskDelay(CAT1_DELAY_1000MS);
	}
}

Cat1_return_t cat1_demo(void)
{
	char cmdRecv[300];
	int i = 0;
	int rc = 0;
	Cat1_return_t ret = Cat1_STATUS_INVALID_PARMS;

	ret=cat1_server_init(AT_CMD_IP,AT_CMD_PORT);
	if (ret != Cat1_STATUS_SUCCESS )
	{
		printf("cat1 init failed\r\n");
		return ret;
	}
	printf("Cat1 Initialize Success!\r\n");


	rc = xTaskCreate
	(
		cat1_Task, (const int8_t *) "CAT1 DEMO",
		CLIENT_TASK_STACK_SIZE, 
		NULL,
		CLIENT_TASK_PRIORITY,
		&cat1TaskHandler
	);

	if (rc < 0) {
		printf("cat1task create failed.!\r\n");
	}

}



