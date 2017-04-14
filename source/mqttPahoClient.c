/******************************************************************************
**	COPYRIGHT (c) 2016		Bosch Connected Devices and Solutions GmbH
**
**	The use of this software is subject to the XDK SDK EULA
**
*******************************************************************************
**
**	OBJECT NAME:	mqttClient.c
**
**	DESCRIPTION:	Source Code for the MQTT Paho Client
**
**	PURPOSE:        Initializes the Paho Client and sets up subscriptions,
**	                starts the task to pubish and receive data,
**	                initializes the timer to stream data,
**	                defines the callback function for subscibed topics
**
**	AUTHOR(S):		Bosch Connected Devices and Solutions GmbH (BCDS)
**
**	Revision History:
**
**	Date			 Name		Company      Description
**  2016.Apr         crk        BCDS         Initial Release
**
*******************************************************************************/

/* system header files */
#include <stdint.h>

/* own header files */
#include "mqttPahoClient.h"
#include "mqttConfig.h"
#include "mqttSensor.h"

/* additional interface header files */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "PTD_portDriver_ph.h"
#include "PTD_portDriver_ih.h"
#include "WDG_watchdog_ih.h"
/* paho header files */
#include "MQTTConnect.h"
#include "cat1_API.h"


/* constant definitions ***************************************************** */

/* local variables ********************************************************** */
// Buffers
static unsigned char buf[CLIENT_BUFF_SIZE];
static unsigned char readbuf[CLIENT_BUFF_SIZE];

// Client Task/Timer Variables
static xTimerHandle     clientStreamTimerHandle      = POINTER_NULL;  // timer handle for data stream
static xTaskHandle      clientTaskHandler            = POINTER_NULL;  // task handle for MQTT Client
static uint8_t clientDataGetFlag = NUMBER_UINT8_ZERO;
static uint32_t clientMessageId = 0;

// Subscribe topics variables
char clientTopicRed[CLIENT_BUFF_SIZE];
char clientTopicOrange[CLIENT_BUFF_SIZE];
char clientTopicYellow[CLIENT_BUFF_SIZE];
char clientTopicDataGet[CLIENT_BUFF_SIZE];
char clientTopicDataStream[CLIENT_BUFF_SIZE];
const char *clientTopicRed_ptr = TOPIC_LED_RED;
const char *clientTopicOrange_ptr = TOPIC_LED_ORANGE;
const char *clientTopicYellow_ptr = TOPIC_LED_YELLOW;
const char *clientTopicDataGet_ptr = TOPIC_DATA_GET;
const char *clientTopicDataStream_ptr = TOPIC_DATA_STREAM;

static uint32_t water_pum_threshlod_1 = 400; // Less than this value, switch on pump!
static uint32_t water_pum_threshlod_2 = 2100; // Larger than this value, switch off Pump


/* global variables ********************************************************* */
// Network and Client Configuration
Network n;
Client c;

/* inline functions ********************************************************* */

/* local functions ********************************************************** */
static void clientRecv(MessageData* md);
static void clientTask(void *pvParameters);

/**
 * @brief callback function for subriptions
 *        toggles LEDS or sets read data flag
 *
 * @param[in] md - received message from the MQTT Broker
 *
 * @return NONE
 */
static void clientRecv(MessageData* md)
{
	/* Initialize Variables */
	MQTTMessage* message = md->message;
	char *pointer;
	int  rec_thr;

	if((strncmp(md->topicName->lenstring.data, clientTopicRed_ptr, md->topicName->lenstring.len) == 0)) {
		/* Toggle the Red LED  During Configurat */
		PTD_pinOutToggle(PTD_PORT_LED_RED, PTD_PIN_LED_RED);
	}
	else if((strncmp(md->topicName->lenstring.data, clientTopicOrange_ptr, md->topicName->lenstring.len) == 0)) {
		/* Toggle the Red LED  During Configurat */
		PTD_pinOutToggle(PTD_PORT_LED_ORANGE, PTD_PIN_LED_ORANGE);
	}
	else if((strncmp(md->topicName->lenstring.data, clientTopicYellow_ptr, md->topicName->lenstring.len) == 0)) {
		/* Toggle the Red LED  During Configurat */
		PTD_pinOutToggle(PTD_PORT_LED_YELLOW, PTD_PIN_LED_YELLOW);
	}
	else if((strncmp(md->topicName->lenstring.data, clientTopicDataGet_ptr, md->topicName->lenstring.len) == 0)) {
		/* Immediately Stream the Sensor Data */
		clientDataGetFlag = (uint8_t) ENABLED;
	}
	if((int)message->payloadlen > SENSOR_DATA_BUF_SIZE)
		return ;

	printf("Subscribed Topic, %.*s, Message Received: %.*s\r\n", md->topicName->lenstring.len, md->topicName->lenstring.data,
			                                                   (int)message->payloadlen, (char*)message->payload);

	pointer = strstr(message->payload,"water_depth");
	if(NULL == pointer)
		return ;

	printf("the water is %s\n",pointer+13);

	rec_thr = atoi(pointer+13);
	printf("the thre is :%d\r\n",rec_thr);

	memset(message->payload, 0x00, (int)message->payloadlen);
	if(rec_thr<water_pum_threshlod_1 )// Need to switch on pumper
		switchPUMP_LEDML(1);
	else if(rec_thr>water_pum_threshlod_2 )
		switchPUMP_LEDML(0);
}

/**
 * @brief publish sensor data, get sensor data, or
 *        yield mqtt client to check subscriptions
 *
 * @param[in] pvParameters UNUSED/PASSED THROUGH
 *
 * @return NONE
 */
static void clientTask(void *pvParameters)
{
	/* Initialize Variables */
	MQTTMessage msg;

	/* Forever Loop Necessary for freeRTOS Task */
    for(;;)
    {
    	WDG_feedingWatchdog();
    	/* Publish Live Data Stream */
		DBG("the sensorStreamBuffer.length is %d\r\n",sensorStreamBuffer.length);
		DBG("the clientDataGetFlag is %d\r\n",clientDataGetFlag);
        if(sensorStreamBuffer.length > NUMBER_UINT32_ZERO)
        {
            msg.id = clientMessageId++;
            msg.qos = 0;
            msg.payload = sensorStreamBuffer.data;
            msg.payloadlen = sensorStreamBuffer.length;
            MQTTPublish(&c, "topic", &msg);//clientTopicDataStream_ptr
            
			DBG("the sensorStreamBuffer.length is %d\r\n",sensorStreamBuffer.length);
        	memset(sensorStreamBuffer.data, 0x00, SENSOR_DATA_BUF_SIZE);
        	sensorStreamBuffer.length = NUMBER_UINT32_ZERO;
        }
        else if(clientDataGetFlag) {
			DBG("the clientDataGetFlag is %d\r\n",clientDataGetFlag);
        	sensorStreamData(pvParameters);
        	clientDataGetFlag = DISABLED;
        }
        else {
            MQTTYield(&c, CLIENT_YIELD_TIMEOUT);
        }
    }
}

/* global functions ********************************************************* */

/**
 * @brief starts the data streaming timer
 *
 * @return NONE
 */
void clientStartTimer(void)
{
	/* Start the timers */
	xTimerStart(clientStreamTimerHandle, UINT32_MAX);
	return;
}
/**
 * @brief stops the data streaming timer
 *
 * @return NONE
 */
void clientStopTimer(void)
{
	/* Stop the timers */
	xTimerStop(clientStreamTimerHandle, UINT32_MAX);
	return;
}

/**
 * @brief Initializes the MQTT Paho Client, set up subscriptions and initializes the timers and tasks
 *
 * @return NONE
 */
void clientInit(void)
{
	Cat1_return_t ret;
	/* Initialize Variables */
    int rc = 0;
    NewNetwork(&n);
#if 1
	ret=cat1_server_init(CAT1_SERVER_IP,CAT1_SERVER_PORT);
	if (ret != Cat1_STATUS_SUCCESS )
	{
		printf("cat1 init failed\r\n");
		return;
	}
	printf("Cat1 init Success!\r\n");
#else
	
    ConnectNetwork(&n, MQTT_BROKER_NAME, MQTT_PORT);
#endif
    MQTTClient(&c, &n, 1000, buf, CLIENT_BUFF_SIZE, readbuf, CLIENT_BUFF_SIZE);

    /* Configure the MQTT Connection Data */
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.willFlag = 0;
    data.MQTTVersion = 3;
    data.clientID.cstring = MQTT_CLIENT_ID;
    data.keepAliveInterval = 100;
    data.cleansession = 1;

    printf("Connecting to %s %d\r\n", MQTT_BROKER_NAME, MQTT_PORT);

    /* Connect to the MQTT Broker */
    rc = MQTTConnect(&c, &data);

#if 0
	/* Set Subscribe Topic Strings */
	memset(clientTopicRed, 0x00, CLIENT_BUFF_SIZE);
	sprintf((char*) clientTopicRed, TOPIC_LED_RED, (const char*) MQTT_CLIENT_ID);
	clientTopicRed_ptr = (char*) clientTopicRed;

	memset(clientTopicOrange, 0x00, CLIENT_BUFF_SIZE);
	sprintf((char*) clientTopicOrange, TOPIC_LED_ORANGE, (const char*) MQTT_CLIENT_ID);
	clientTopicOrange_ptr = (char*) clientTopicOrange;

	memset(clientTopicYellow, 0x00, CLIENT_BUFF_SIZE);
	sprintf((char*) clientTopicYellow, TOPIC_LED_YELLOW, (const char*) MQTT_CLIENT_ID);
	clientTopicYellow_ptr = (char*) clientTopicYellow;

	memset(clientTopicDataGet, 0x00, CLIENT_BUFF_SIZE);
	sprintf((char*) clientTopicDataGet, TOPIC_DATA_GET, (const char*) MQTT_CLIENT_ID);
	clientTopicDataGet_ptr = (char*) clientTopicDataGet;

	memset(clientTopicDataStream, 0x00, CLIENT_BUFF_SIZE);
	sprintf((char*) clientTopicDataStream, TOPIC_DATA_STREAM, (const char*) MQTT_CLIENT_ID);
	clientTopicDataStream_ptr = (char*) clientTopicDataStream;
#endif
	/* Subscribe to the Topics (set callback functions) */
    rc = MQTTSubscribe(&c, "topic", QOS0, clientRecv);//clientTopicRed_ptr
   // rc = MQTTSubscribe(&c, clientTopicOrange_ptr, QOS0, clientRecv);
   // rc = MQTTSubscribe(&c, clientTopicYellow_ptr, QOS0, clientRecv);
   // rc = MQTTSubscribe(&c, clientTopicDataGet_ptr, QOS0, clientRecv);

	/* Create Live Data Stream Timer */
    clientStreamTimerHandle = xTimerCreate(
			(const char * const) "Data Stream",
			STREAM_RATE,
			TIMER_AUTORELOAD_ON,
			NULL,
			sensorStreamData);

	/* Create MQTT Client Task */
    rc = xTaskCreate(clientTask, (const char * const) "Mqtt Client App",
                    		CLIENT_TASK_STACK_SIZE, NULL, CLIENT_TASK_PRIORITY, &clientTaskHandler);

    /* Error Occured Exit App */
    if(rc < 0)
    {
    	clientDeinit();
    }

    return;
}

/**
 * @brief Disconnect from the MQTT Client
 *
 * @return NONE
 */
void clientDeinit(void)
{
    MQTTDisconnect(&c);
    n.disconnect(&n);
}
