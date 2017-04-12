//#include "stdafx.h"

#undef WIN_PLATFORM
//#define WIN_PLATFORM

#ifdef WIN_PLATFORM
/* system header files */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <windows.h>  
#include <winbase.h>  
#include <process.h> 

#include "parson.h"

#if __have_long32
#define INT32_C(x)	x##L
#define UINT32_C(x)	x##UL
#else
#define INT32_C(x)	x
#define UINT32_C(x)	x##U
#endif


#define CAT1_TIMEOUT					 UINT32_C(10000)
#define CAT1_DELAY_1000MS				UINT32_C(1000)
#define Cat1_TX_BUFFER_SIZE           	(UINT32_C(1492))
#define Cat1_RX_BUFFER_SIZE           	(UINT32_C(1492))


 typedef enum Cat1_return_e {
	Cat1_STATUS_SUCCESS =  UINT8_C(0),
	Cat1_STATUS_FAILED =  UINT8_C(1),
	Cat1_STATUS_INVALID_PARMS =  UINT8_C(2),
	Cat1_STATUS_TIMEOUT =  UINT8_C(3)

} Cat1_return_t;

typedef enum Lift_return_e {
	Lift_STATUS_SUCCESS =  UINT8_C(0),
	Lift_STATUS_FAILED =  UINT8_C(1),
	Lift_STATUS_INVALID_PARMS =  UINT8_C(2),
	Lift_STATUS_TIMEOUT =  UINT8_C(3)

} Lift_return_t;


/* local function prototype declarations */

#pragma  comment(lib,"ws2_32.lib")
Cat1_return_t cat1_recv(uint16_t *buf, uint16_t *len);
Cat1_return_t cat1_send(uint16_t *sendData, uint16_t len);

void WDG_feedingWatchdog(void){

}

#else

/* system header files */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>

/* own header files */
#include "mqttPahoClient.h"
#include "mqttConfig.h"
#include "mqttSensor.h"
#include "MQTTConnect.h"


/* additional interface header files */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h" 
#include "timers.h"
#include "PTD_portDriver_ph.h"
#include "PTD_portDriver_ih.h"
#include "WDG_watchdog_ih.h"
#include "cat1_API.h"

#include "parson.h"
#include "liftcheck.h"
#include "lift_UartDev.h"
#endif

#include "liftcheck_testdata.h"

struct timeval tv_speed1;
struct timeval tv_speed2;

#ifdef WIN_PLATFORM

#define   TIME_UNIT 1000000

int tv_cmp(struct timeval t1, struct timeval t2)
{
	return(t1.tv_sec - t2.tv_sec) * 1000000 + (t1.tv_usec - t2.tv_usec);
}


int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
	// Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
	// This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
	// until 00:00:00 January 1, 1970 
	static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

	SYSTEMTIME  system_time;
	FILETIME    file_time;
	uint64_t    time;

	GetSystemTime(&system_time);
	SystemTimeToFileTime(&system_time, &file_time);
	time = ((uint64_t)file_time.dwLowDateTime);
	time += ((uint64_t)file_time.dwHighDateTime) << 32;

	tp->tv_sec = (long)((time - EPOCH) / 10000000L);
	tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
	return 0;
}

#define LIFT_SLEEP_MS(ms)  Sleep(ms)

#else

#define   TIME_UNIT 1000


int tv_cmp(struct timeval t1, struct timeval t2)
{
	return(t1.tv_sec - t2.tv_sec) * 1000 + (t1.tv_usec - t2.tv_usec);
}

// TODO: 系统频率, 得到tick的时间
int _gettimeofday( struct timeval *tv, void *tzvp )
{
    uint64_t t = PowerMgt_GetSystemTimeMs();  // get uptime in microseconds
    tv->tv_sec = t / 1000;  // convert to seconds
    tv->tv_usec = ( t % 1000 ) ;  // get remaining microseconds
    return 0;  // return non-zero for error
} // end _gettimeofday()

#define LIFT_SLEEP_MS(ms)  vTaskDelay((portTickType) ms / portTICK_RATE_MS)


static xTimerHandle     liftCheckTimerHandle      = POINTER_NULL;  // timer handle for data stream
static xTaskHandle      liftCheckTaskHandler      = POINTER_NULL;  // task handle for MQTT Client
static xTaskHandle      liftWebServerTaskHandler  = POINTER_NULL;  // task handle for MQTT Client
static xQueueHandle		liftMsgQueueHandle;



#endif

#define CMD_LEN 1024
#define CMD_RECV 1024
#define SEND_LEN 1024

char socketCmdBuf[CMD_LEN];
char socketCmdRecvBuf[CMD_RECV];
char socketCmdRecvMsg[CMD_RECV];
char socketCmdRecvFragment[CMD_RECV];

#define LIFT_SEND_QUEUE_SIZE 					(10)
#define LIFT_SEND_MAX_PACKET_SIZE 				300  //(SEND_LEN)

struct LIFT_queueSendData_S {
	uint32_t dataSize;
	char packetData[LIFT_SEND_MAX_PACKET_SIZE];
};

typedef struct LIFT_queueSendData_S LIFT_queueSendData_T;


#define __DEBUG	//控制开关
#ifdef __DEBUG
#define DEBUG(format,...) printf("Line: %05d: "format"\n", __LINE__, ##__VA_ARGS__)
//#define DEBUG(format,...) printf("File: "__FILE__", Fun: "__FUNCTION__": "format"\n", ##__VA_ARGS__)
//#define DEBUG(format,...) printf("File: "__FILE__", Fun: __FUNCTION__: "format"\n",__FUNCTION__, ##__VA_ARGS__)
#else
#define DEBUG(format,...)
#endif

//#define __ERROR
#ifdef __ERROR
#else
#endif

#define __INFO
#ifdef __INFO
#define INFO(format,...) printf("Line: %05d: "format"\n", __LINE__, ##__VA_ARGS__)
//#define INFO(format,...) printf("File: "__FILE__", __LINE__,"format"\n", __LINE__, ##__VA_ARGS__)  F
//#define INFO(format,...) printf("Fun: "__FUNCTION__", __LINE__, "format"\n", __LINE__, ##__VA_ARGS__)
#else
#define INFO(format,...)
#endif

//#define __LOGING
#ifdef __LOGING
#else
#endif

#define bit8 0x80
#define bit7 0x40
#define bit6 0x20
#define bit5 0x10
#define bit4 0x08
#define bit3 0x04
#define bit2 0x02
#define bit1 0x01
char* thisVersion = "161107"; //"1.0.34 bulid-160518";

/*ON means happened, OFF means not happened
*the gpips have been pulled-up, if happened, gpios would be low
*the sensor inputs also been pulled-up, witch means:
*for normal closed sensors, when happened, sensor inputs pulled-up, optocoupler on, gpio: 1->0
*for normal open sensors, like a switch, when happened, sensor inputs pulled-down, optocoupler off, gpio: 0->1
*therefore: for normal closed, on = 1 ,off = 0; for normal open, on = 0, off = 1.
*default use normal closed.
*/
#define S_inPositionUp_ON	1
#define S_inPositionUp_OFF	0

#define S_inPositionDown_ON	1
#define S_inPositionDown_OFF	0

#define S_inPositionFlat_ON	1
#define S_inPositionFlat_OFF	0

#define S_doorOpen_ON		0     	//open: low level, change to high level, JC,20160412, back to low level, JC 20161008
#define S_doorOpen_OFF		1	//closed: high level, change to low level, JC,20160412, back to low level, JC 20161008

#define S_havingPeople_ON	0   	//having people: low level ---> high level, changed on May 23, 2016
#define S_havingPeople_OFF	1   	//not having people: high level ---> low level, changed on May 23, 2016

#define S_inRepairing_ON	1
#define S_inRepairing_OFF	0

#define S_alarmButtonPushed_ON	1
#define S_alarmButtonPushed_OFF	0

#define S_safeCircuit_ON	0
#define S_safeCircuit_OFF	1

#define S_runningContactor_ON	1
#define S_runningContactor_OFF	0

#define S_generalContactor_ON	1
#define S_generalContactor_OFF	0

#define S_baseStation_ON  1
#define S_baseStation_OFF  0

#define S_limitSwitch_ON  1
#define S_limitSwitch_OFF  0

//now_status 0-3 当前状态[0 正常 1 故障]
#define E_nowStatus_NORMAL 0
#define E_nowStatus_FAULT 1

//power_status 0-2 电力[0 电源 1 电池 2 其他]
#define E_powerStatus_POWER 0
#define E_powerStatus_BATTERY 1
#define E_powerStatus_OTHER 2

//direction 0-2 方向[0 停留 1 上行 2 下行]
#define E_direction_STOP 0
#define E_direction_UP 1
#define E_direction_DOWN 2

//floor_status 0-1 平层状态[0平层 1 非平层]
#define E_inPosition_FLAT 0
#define E_inPosition_NOFLAT 1

//door_status 0-3 门状态[0 关门 1 开门 2 关门中 3 开门中]
#define E_doorStatus_CLOSE 0
#define E_doorStatus_OPEN 1

//has_people 0-1 人状态[0 无人 1 有人]
#define E_havingPeople_NOBODY 0
#define E_havingPeople_YES 1

//inPositionBase[0 非基站  1 基站]
#define E_inPositionBase_NO 0
#define E_inPositionBase_YES 1

//inRepairing[0 没有维修  1 维修状态]
#define E_inRepairing_NO 0
#define E_inRepairing_YES 1


/*App - Sensors*/
#define S_IN_POSITION_UP  	1  //up or down, need to confirm
#define S_IN_POSITION_FLAT 	2
#define S_IN_POSITION_DOWN 	3
#define S_DOOR 			4
#define S_PEOPLE		5
#define S_REPAIRING 		6
#define S_ALARM 		6
#define S_SAFE_CIRCUIT 		7
#define S_RUNNING_CONTACTOR 	8
#define S_GENERAL_CONTACTOR 	9

#define S_LED3_REPAIRING 	10
#define S_LED2_NORMAL		11
#define S_LED1_FAULT		10

#define S_LED_EMERGENCY 	13//GPD7 for V1.1; GPG5 for V1.0 --> change to GPC15(R_CTRL1) relay

#define S_POWER_STATUS 		14 //main power, shared with LCD_G0

int ringCount = 0;

int gsmCallResult = -1;

typedef unsigned short uint16_t;

typedef uint16_t UINT16;
//typedef unsigned short int UINT16
typedef unsigned char UCHAR8;

/*Macros*/
#define POSTURL "http://api.elevatorcloud.com"//"http://www.xiami.com/member/login"
#define POSTURLTEST "api.elevatorcloud.com"
//#define POSTFIELDS "email=myemail@163.com&password=mypassword&autologin=1&submit=登 录&type="
//#define FILENAME "curlposttest.log"


//#define HttpServerHost http:\/\/api.elevatorcloud.com
//#define RegisterAdrr /register
//#define IPSTR ""//"114.80.200.79"-->114.55.42.28
//#define POET 80

//#define IPSTR2 "220.191.224.88"
//#define IPSTR3 "192.168.2.105"
//#define PORT2 9000
//#define PORT3 9200
#define PORT_IO 13000
#define PORT_VIDEO 15000

#define UARTBUFSIZE 1024

#define BUFSIZE 1024
#define FaultTotalNumbers 27
#define MAXID 200 //10 min every piece, so 143 is for one day

/*Alerts*/
#define A_NORMAL_OPERARION 	0
#define A_HIT_CEILING 		1//冲顶；
#define A_HIT_GROUND 		2//蹲底；
#define A_OVERSPEED 		3//超速；
#define A_OPEN_WHILE_RUNNIN 	4//运行中开门；
#define A_OPEN_WITHOUT_STOPPIN 	5//开门走车；
#define A_STOP_OUT_AREA 	6//门区外停梯；
#define A_CLOSE_IN_POSITION 	7//平层关人；
#define A_CLOSE_OUT_POSITION 	8//非平层关人；
#define A_POWER_CUT 		9//停电；
#define A_SAFE_LOOP_BREAK 	10//安全回路断路；
#define A_OPEN_FAULT 		11//开门故障；
#define A_CLOSE_FAULT 		12//关门故障；
#define A_LOCK_LOOP_BREAK 	13//门锁回路断路；
#define A_BUTTON_ALWAYS_PASSED 	14//层站按钮粘连；
#define A_SPEED_ANOMALY 	15//电梯速度异常；
#define A_OVERSPEED_1_2 	16//超速1.2倍；
#define A_OVERSPEED_1_4 	17//超速1.4倍；
#define A_UNCTRL_BACKSPIN 	18//非操纵逆转；
#define A_BREAK_OUT_LIMIT 	19//制停超距故障；
#define A_LEFT_HANDRAIL_UNDER_SPEED 	20//左扶手欠速；
#define A_RIGHT_HANDRAIL_UNDER_SPEED 	21//右扶手欠速；
#define A_UP_STAIR_DEFICIENCY 		22//上梯级缺失；
#define A_DOWN_STAIR_DEFICIENCY 	23//下梯级缺失；
#define A_BREAK_ANOMALY 		24//工作制动器打开故障；
#define A_CHAIN_ANOMALY 		25//附加制动器动作故障；
#define A_COVER_PLATE_ANOMALY 		26//楼层

#define X_NORMAL_OPERARION 	0//NO faults
#define X_STOP_OUT_AREA 	1//门区外停梯
#define X_OPEN_WHILE_RUNNIN 	2//运行中开门；
#define X_HIT_CEILING 		3//冲顶；
#define X_HIT_GROUND 		4//蹲底；
#define X_OVERSPEED 		5//超速；
#define X_CLOSE_PEOPLE 		6//平层关人；

/*timers*/
#define Ta 0 //冲顶/蹲底计时器45
#define Tb 1 //平层关人计时器45
#define Tc 2 //平层关人产生后，到自动播放安抚音计时器30
#define Td 3 //催促音播放周期计时器300
#define Te 4 //非平层关人计时器30
#define Tf 5 //门区外停车计时器60
#define Tg 6 //运行中开门时间计时器10
#define Th 7 //（保留）
#define Ti 8 //开门走车计时器10
#define Tj 9 //（保留）
#define Tk 10 //（保留）,//having people25
#define Tl 11 //（保留）
#define Tm 12 //停车时开门时间计时器80
#define Tn 13 //（保留）//人感定时器,不及时检测, 等待 10s

int wsTimer = 0;
int timers[14] = { 0 };
//int timerThreshhold[14] = { 30, 60, 30, 300, 60, 30, 30, 30, 30, 30, 20, 30, 30, 30 }; //default values, change Td to 300, JC, 20160430
///////////////////////////  Ta, Tb, Tc, Td,  Te, Tf, Tg, Th, Ti, Tj, Tk, Tl, Tm, Tn
int timerThreshhold[14] =  { 20, 20, 20, 100, 20, 45, 50, 20, 50, 20, 20, 20, 60, 10 }; //default values, change Td to 300, JC, 20160430
int time_StreamCloud = 300;


int Cg = 0; //运行中开门过层层数计数器 1//not used now
int Ci = 0; //开门走车过层层数计数器  1//not used now
int Cm = 5; //停车时开关门次数计数器1

//special varibles
int Sk = 18;//额定梯速,m/s
int Sn = 12; //超速倍数定义1.2*1.8=2.2
int Sl = 2; //超速过层层数 //use local varible l later
int Si = 20;
int Lbase = 1; //number of base floor
int Lmax = 100; //number of max floor
int Lmin = -1; //number of min floor

/*structures*/
struct T_ElevatorStatus{
	//PT_SensorsStatus status_SensorsStatus;
	int EStatus_nowStatus;//01234
	int EStatus_powerStatus;//012
	int EStatus_direction;//012
	int EStatus_doorStatus;//0123
	int EStatus_inPosition;//01
	float EStatus_speed;
	int EStatus_havingPeople;//01
	int EStatus_currentFloor;
	int EStatus_backup1;
	int EStatus_backup2;
	int EStatus_inPositionBase;//01
	int EStatus_inRepairing;
};

struct T_SensorsStatus{
	int SStatus_inPositionUp;
	int SStatus_inPositionDown;
	int SStatus_inPositionFlat;
	int SStatus_doorOpen;
	int SStatus_havingPeople;
	int SStatus_inRepairing;
	int SStatus_alarmButtonPushed;
	int SStatus_safeCircuit;
	int SStatus_runningContactor;
	int SStatus_generalContactor;
	int SStatus_backup1;
	int SStatus_backup2;
    int SStatus_baseStation;
    int SStatus_limitSwitch;
};


/*global varibles*/
const char *ver = "1";
const char *secret;
const char *cid;
const char *hid;
const char *xid;
const char *model;
const char *uid;
//const char *phone;

char *ioServerIP;
char *videoServerIP;

int STAT_HeartBeat = 0;

unsigned char canRawData[24] = { 0 };

int glob_totalFaultNumbers = 0;
int currentFaultNumber = 0;
int faultTypeIndi[27] = { 0 };
int removeFaultTypeIndi[27] = { 0 };
int faultSentIndi[27] = { 0 };
//PT_ElevatorStatus *pElevatorStatus = NULL;
//PT_SensorsStatus *pSensorsStatus = NULL;
struct T_ElevatorStatus pElevatorStatus;
struct T_SensorsStatus pSensorsStatus;

int currentUp = 0;
int currentDown = 0;
int lastUp = 0;
int lastDown = 0;

int lastPosition = 9;  //here 9 means uncertain
int lastDoor = 9;
int lastDirection = 9;
int lastSStatus_havingPeople=0;

float UpDownDistance=0.1;

float overSpeed_speed = 0;
int overSpeed_directon = 0;
int overSpeed_currentFloor = 0;

int currentFloorRefreshed = 0;

//for: checkSoundPlayerAvailable()
int soundTimer = 0;
int soundDurationLast = 0;
//for: call
int callNeeded = 0;
int broadcastNeeded = 0;

//char strServerTime[11]="0";
char *strServerTime;

int count_ms1 = 0;
int count_normalSpeedFloor = 0;
int count_overSpeedFloor = 0;
int count_closeAndOpen = 0;
int count_normalSpeed = 0;

//int count_videoRequest = 0;
int timerThreshholdVideo = 360; //6 minutes
//int timerThreshholdAudio = 300;

int lastNormalSpeed = 0;

int parent_id_StreamCloud_int = 0;
const char *parent_id_StreamCloud_string;
int isParentIdInt = 0;
//char * address_StreamCloud;
int vaStartResult = 0;
//int videoStartResult = 0;
int audioStartResult = 0;
int videoPubEnabled = 0;
int audioOpenResult = 0;

time_t now;
struct tm *timenow;
char *strTimeText;



char* msgRevVA;
char* msgRevPlayer;

int sem_a;
int sem_b;
int sem_c;
int sem_d;
int sem_e;
int sem_f;
int sem_g;

int gFlag_UploadStatus = 0;
int gFlag_Player = 0;
int gFlag_PlayerOpen = 0;
int gFlag_VA = 0;
int gFlag_VAOpen = 0;
//int gFlag_HeartBeat = 0;

int gFlag_NeedStatus = 0;
int gFlag_FaultStatus = 0;
int gFlag_Play = 0;
int gFlag_HeartBeat = 0;
int gFlag_HeartBeatVideo = 0;
int gFlag_HeartBeatResp = 0;
int gFlag_HeartBeatRespVideo = 0;
//int connectLost = 0;

static int inpositionCalled = 0;	//flag of being call for CLOSE_IN_POSITION
static int outpositionCalled = 0;	//flag of being call for CLOSE_OUT_POSITION
static int inpositionBroadcasted = 0;
static int outpositionBroadcasted = 0;
static int flag_hitceiling = 0;
static int flag_hitgroud = 0;


int connectLost = 1;
int connectVideoLost = 1;

#define SPRE "#10#"
#define SEND "#13#"
#define S2C_NEED_LOGIN  "{\"type\":\"need_login\"}"
#define S2C_PING  "{\"type\":\"ping\"}"
#define C2S_PING  "{\"type\":\"ping\"}"
#define S2C_KICk  "{\"type\":\"kick\",\"code\":"
#define S2C_CPING  "{\"type\":\"cping\"}"
#define C2S_CPING  "{\"type\":\"cping\"}"
#define C2S_LOGIN  "{\"type\":\"login\",\"eid\":\"%s\"}"
#define S2C_LOGIN  "{\"type\":\"login\",\"res_id\":"

#define S2C_PING_COUNT_MAX    100  //服务器发送的ping, 如果超过次数没有收到, 就重启xdk
unsigned int S2C_PING_COUNT=1;

#define C2S_CPING_COUNT_MAX    100  //设备向服务器发送cping, 如果没有收到服务器应答超过次数, 则重启xdk 
unsigned int C2S_CPING_COUNT=0;

#define IMEI_BUF_LEN 100
static char IMEI_BUF[IMEI_BUF_LEN];
//#define DEVICE_ID_DEFAULT    "mx_863867025992437"  //测试临时使用的设备id


#define CAT1_SEND_RETRY_MAX 10	//如果cat1发送识别, 最大重复发送次数, 超过则重启xdk系统
#define DEVICE_REGISTER_WAIT 10000 //如果注册识别, 等待30秒重新注册
#define HEART_BEAT_RATE         60000/portTICK_RATE_MS //如果60秒没有收到服务器的心跳包, 则重启xdk
#define WATCH_DOG_TIMEOUT    300000 //5分钟看门狗超时

#define SECOND 1
#define USECOND 0


#define TITEMLEN 22
#define TITEMCOUNT 64



char UartBuf[UARTBUFSIZE];
char FaultArray[100];
char HttpPostStatusFaultBuffer[SEND_LEN];


time_t t_timers;
int timerVideo = 0;
int timerAudio = 0;


/*
* @brief Allows to encode the provided content, leaving the output on
* the buffer allocated by the caller.
*
* @param(void)
*
* @return(void)
*/
void timerStart(int num)
{
	timers[num] = time(&t_timers);
}

int timerRead(int num)
{
	return timers[num];
}

int timerCheck(int num)
{
	int a = time(&t_timers);
	int k=0;

	printf("******  TIMER: ");
	for (k=0; k<14; k++)
	{
		printf("   %c", 'a'+k);
	}
	printf("  ******\r\n");
	
	printf("******  TIMER: ");
	for (k=0; k<14; k++)
	{
		printf(" %03d", (timers[k]!=0)?(a-timers[k]):0);
	}
	printf("  ******\r\n");
	
	int v = time(&t_timers) - timers[num];
	printf("TIMER[%d]=%d\r\n", num, v);
	return v;
}

void timerEnd(int num)
{
	timers[num] = 0;
}

int timerGetThreshhold(int num)
{
	return timerThreshhold[num];
}

int timerSetThreshhold(int num, int value)
{
	timerThreshhold[num] = value;
	return value;
}

/***************Video*******************/


/*
* @brief Allows to encode the provided content, leaving the output on
* the buffer allocated by the caller.
*
* @param(void)
*
* @return(void)
*/
void timerStartVideo(void)
{
	timerVideo = time(&t_timers);
}

int timerReadVideo(void)
{
	return timerVideo;
}

int timerCheckVideo(void)
{
	return time(&t_timers) - timerVideo;
}

void timerEndVideo(void)
{
	timerVideo = 0;
}

/***************Audio, not used yet*******************/


/*
* @brief Allows to encode the provided content, leaving the output on
* the buffer allocated by the caller.
*
* @param(void)
*
* @return(void)
*/
void timerStartAudio(void)
{
	timerAudio = time(&t_timers);
}

int timerReadAudio(void)
{
	return timerAudio;
}

int timerCheckAudio(void)
{
	return time(&t_timers) - timerAudio;
}

void timerEndAudio(void)
{
	timerAudio = 0;
}

void rebootXDK(char* where){
	DEBUG("%s() where=%s\r\n", __FUNCTION__, where);
#ifndef WIN_PLATFORM
	//assert(0);
	//利用看门狗超时重启xdk
    WDG_init(WDG_FREQ, 100);
	for(;;){};
#endif
}

void rebootCat1(char* where){
	DEBUG("%s() where=%s\r\n", __FUNCTION__, where);
	rebootXDK(where);
	//assert(0);
	// TODO: cat1_reset 好像不能工作
	//cat1_reset();
 	//cat1_server_init(CAT1_SERVER_IP,CAT1_SERVER_PORT);
}

int set_SStatus_emergencyLED(int sig)//maybe changed from other sources
{
	//DEBUG("set_SStatus_emergencyLED %d\n", sig);
	return -1;
}

int set_SStatus_LED(int led, int sig)//maybe changed from other sources
{
	//DEBUG("set_SStatus_LED %d %d\n", led, sig);
	return -1;
}

/*get-functions of EStatus Status*/

int get_EStatus_nowStatus()//not used
{
	return 0;
}

int get_EStatus_powerStatus()
{
	return E_powerStatus_POWER;
}

int get_EStatus_doorStatus()
{
	if (pSensorsStatus.SStatus_doorOpen == S_doorOpen_ON)
		return E_doorStatus_OPEN;
	else
		return E_doorStatus_CLOSE;
}

int get_EStatus_inPosition()
{
	if (pSensorsStatus.SStatus_inPositionUp == S_inPositionUp_ON && pSensorsStatus.SStatus_inPositionDown == S_inPositionDown_ON)
	{
		pElevatorStatus.EStatus_direction = E_direction_STOP;
		return E_inPosition_FLAT; //flat floor
	}
	else
		return E_inPosition_NOFLAT; //not flat floor
}

int get_EStatus_havingPeople()
{
	//INFO("SStatus_havingPeople: %d\n",pSensorsStatus.SStatus_havingPeople);
	if (pSensorsStatus.SStatus_havingPeople == S_havingPeople_ON)
	{
		return E_havingPeople_YES;
	}
	else
	{
		return E_havingPeople_NOBODY;
	}
}

void sensorsStatus_init(void)//It's best to read first
{
	pSensorsStatus.SStatus_inPositionUp = S_inPositionUp_ON;//should be on, cause only changed after moving
	pSensorsStatus.SStatus_inPositionDown = S_inPositionDown_ON;
	pSensorsStatus.SStatus_inPositionFlat = S_inPositionFlat_OFF;
	pSensorsStatus.SStatus_doorOpen = S_doorOpen_OFF;
	pSensorsStatus.SStatus_havingPeople = S_havingPeople_OFF;
	pSensorsStatus.SStatus_inRepairing = S_inRepairing_OFF; //
	pSensorsStatus.SStatus_alarmButtonPushed = S_alarmButtonPushed_OFF; //
	pSensorsStatus.SStatus_safeCircuit = S_safeCircuit_ON; //not used now
	pSensorsStatus.SStatus_runningContactor = S_runningContactor_OFF; //not used now
	pSensorsStatus.SStatus_generalContactor = S_generalContactor_OFF; //not used now
	pSensorsStatus.SStatus_backup1 = NULL;
	pSensorsStatus.SStatus_backup2 = NULL;
    pSensorsStatus.SStatus_baseStation = S_baseStation_OFF;
    pSensorsStatus.SStatus_limitSwitch = S_limitSwitch_OFF;
}


void elevatorStatus_init()
{
	pElevatorStatus.EStatus_nowStatus = E_nowStatus_NORMAL;
	pElevatorStatus.EStatus_powerStatus = E_powerStatus_POWER;
	pElevatorStatus.EStatus_direction = E_direction_STOP;
	pElevatorStatus.EStatus_doorStatus = E_doorStatus_CLOSE;
	pElevatorStatus.EStatus_inPosition = E_inPosition_FLAT;
	pElevatorStatus.EStatus_speed = 0.0;
	pElevatorStatus.EStatus_havingPeople = E_havingPeople_NOBODY;
	pElevatorStatus.EStatus_currentFloor = Lbase;
	pElevatorStatus.EStatus_backup1 = NULL;
	pElevatorStatus.EStatus_backup2 = NULL;
    pElevatorStatus.EStatus_inPositionBase = E_inPositionBase_YES;
	pElevatorStatus.EStatus_inRepairing = E_inRepairing_NO;
}





/*****************************/
int setFaultSent(int faultNUM)
{
	faultSentIndi[faultNUM] = 1;
	return faultNUM;
}

int getFaultSent(int faultNUM)
{
	return faultSentIndi[faultNUM];
}

int clearFaultSent(int faultNUM)
{
	faultSentIndi[faultNUM] = 0;
	return faultNUM;
}

/*****************************/
void setFault(int faultNUM)
{
	faultTypeIndi[faultNUM] = 1;
	DEBUG("******       Fault: %d        ******\n", faultNUM);	
#ifdef WIN_PLATFORM
	//getchar();
#endif
}

int getFaultState(int faultNUM)
{
	return faultTypeIndi[faultNUM];
}

/*****************************/
void removeFault(int faultNUM)
{
	if (getFaultState(faultNUM) == 1) 
		removeFaultTypeIndi[faultNUM] = 1;
	faultTypeIndi[faultNUM] = 0;
}

int getFaultRemoveState(int faultNUM)
{
	return removeFaultTypeIndi[faultNUM];
}

int clearFaultRemoveState(int faultNUM)
{
	removeFaultTypeIndi[faultNUM] = 0;
	return faultNUM;
}

/*****************************/
int getTotalFaultNumbers(void)
{
	return glob_totalFaultNumbers;
}

void setTotalFaultNumbers(int n)
{
	glob_totalFaultNumbers = n;
}

void makeACall(void){

}

void checkElevatorFault(void)
{
	int counttime = 0;
	/*assign sensor data to local varibles*/
	int status_powerStatus = pElevatorStatus.EStatus_powerStatus;
  	int status_direction = pElevatorStatus.EStatus_direction;
	int status_doorStatus = pElevatorStatus.EStatus_doorStatus;
	int status_inPosition = pElevatorStatus.EStatus_inPosition;
	float status_speed = pElevatorStatus.EStatus_speed;
	int status_havingPeople = pElevatorStatus.EStatus_havingPeople;
	int status_currentFloor = pElevatorStatus.EStatus_currentFloor;
    int status_inPositionBase = pElevatorStatus.EStatus_inPositionBase;
	int status_alarmButtonPushed = pSensorsStatus.SStatus_alarmButtonPushed;  //pushed ==1
	int status_inRepairing = pElevatorStatus.EStatus_inRepairing;
	
#if 0
	char mstatus_direction[][100] = { "停", "上行", "下行" };
	char mstatus_doorStatus[][100] = { "关", "开" };
	char mstatus_inPosition[][100] = { "平层", "非平层" };
	char mstatus_inPositionBase[][100] = { "非基站", "基站" };
	char mstatus_havingPeople[][100] = { "无人", "有人" };
#else
	char mstatus_direction[][100] = { "stop", "up", "down" };
	char mstatus_doorStatus[][100] = { "close", "open" };
	char mstatus_inPosition[][100] = { "flat", "not flat" };
	char mstatus_inPositionBase[][100] = { "not base", "base" };
	char mstatus_havingPeople[][100] = { "nobody", "has people" };
	char mstatus_inRepairing[][100] = { "not in reparing", "in reparing" };

#endif
	DEBUG("+++Checking list+++\n \
		direction:%d/%s,\n \
		doorStatus:%d/%s,\n \
		inPosition:%d/%s,\n \
		inPositionBase:%d/%s,\n \
		havingPeople:%d/%s,\n \
		currentFloor:%d,\n \
		inRepairing:%d/%s,\n" \
		, status_direction, mstatus_direction[status_direction]
		, status_doorStatus, mstatus_doorStatus[status_doorStatus]
		, status_inPosition, mstatus_inPosition[status_inPosition]
		, status_inPositionBase, mstatus_inPositionBase[status_inPositionBase]
		, status_havingPeople, mstatus_havingPeople[status_havingPeople]
		, status_currentFloor
		, status_inRepairing, mstatus_inRepairing[status_havingPeople]);
	
	DEBUG("+++Checking list+++\n \
		lastPosition: %d,\n \
		lastDoor:%d,\n \
		lastDirection:%d\n" \
		,lastPosition \
		,lastDoor \
		,lastDirection);

	if(status_inRepairing == E_inRepairing_NO)
	{
		/*here to check if A_HIT_GROUND or A_HIT_CEILING occurs*/
		if((status_currentFloor == Lmin)&&(status_direction == E_direction_DOWN)&&(status_inPosition == E_inPosition_NOFLAT)) 
		{//all 3 conditions will stay if this occurs
			flag_hitgroud = 1;
			flag_hitceiling = 0;
			INFO("IN: current floor, derection and inpostion are: %d, %d, %d\n",status_currentFloor,status_direction,status_inPosition);
			if(timerRead(Ta) == 0) timerStart(Ta);
			//INFO("check time - ground: %d\n",timerCheck(Ta));
			if(timerCheck(Ta)>= timerGetThreshhold(Ta))
			{
				DEBUG("Fault: A_HIT_GROUND!\n");
				setFault(A_HIT_GROUND);
			}
		}
		else
		{
			if(flag_hitceiling == 0)
			{
				timerEnd(Ta);
				flag_hitgroud = 0;
			}
		}

		if((status_currentFloor == Lmax)&&(status_direction==E_direction_UP)&&(status_inPosition == E_inPosition_NOFLAT))
		{
			flag_hitceiling = 1;
			flag_hitgroud = 0;
			if(timerRead(Ta) == 0) timerStart(Ta);
			INFO("check time - ceiling: %d\n",timerCheck(Ta));
			if(timerCheck(Ta)>= timerGetThreshhold(Ta))
			{
				DEBUG("Fault: A_HIT_CEILING!\n");
				setFault(A_HIT_CEILING);
			}
		}
		else
		{
			if(flag_hitgroud == 0)
			{
				timerEnd(Ta);
				flag_hitceiling = 0;
			}
		}

		/*here to check if A_STOP_OUT_AREA or  A_CLOSE_IN_POSITION, or A_CLOSE_OUT_POSITIONoccurs*/	
		if(status_havingPeople == E_havingPeople_NOBODY)//no people in the cube
		{
			//refresh, seperate before and after fault occurs, if faults occur, should not reset here.
			timerEnd(Tb);
			//timerEnd(Tc);
			//timerEnd(Td);
			timerEnd(Te);
			//inpositionCalled = 0;
			removeFault(A_CLOSE_IN_POSITION);
			removeFault(A_CLOSE_OUT_POSITION);
			if(status_inPosition == E_inPosition_NOFLAT)
			{
				if(timerRead(Tf) == 0) timerStart(Tf);
				if(timerCheck(Tf)>= timerGetThreshhold(Tf))
				{
					if(getFaultState(A_HIT_GROUND)==0 && getFaultState(A_HIT_CEILING)==0)
					{
						DEBUG("Fault: A_STOP_OUT_AREA!\n");
						setFault(A_STOP_OUT_AREA);
					}

				}
			}
			else
			{
				timerEnd(Tf);
				//removeFault(A_STOP_OUT_AREA);
			}		
		}
		else//having people in the elevator
		{
			timerEnd(Tf);//stop all timers related with having no people in the cube
			//removeFault(A_STOP_OUT_AREA);

			/*********************************************/
			/*here to check if A_CLOSE_IN_POSITION occurs*/
			if(status_inPosition == E_inPosition_FLAT && status_doorStatus == E_doorStatus_CLOSE)
			{
				if(timerRead(Tb) == 0) timerStart(Tb);
				if((timerRead(Tb)>0)&&(timerCheck(Tb)>= timerGetThreshhold(Tb)))
				{
					DEBUG("Fault: A_CLOSE_IN_POSITION!\n");
					setFault(A_CLOSE_IN_POSITION);
				}
			}
			else //1)reset all timers and counters; 2)reset fault alert
			{
				removeFault(A_CLOSE_IN_POSITION);			
				timerEnd(Tb);
			}
			/**********************************************/
			/*here to check if A_CLOSE_OUT_POSITION occurs*/
			if(status_inPosition == E_inPosition_NOFLAT && status_doorStatus == E_doorStatus_CLOSE)
			{
				if(timerRead(Te) == 0) timerStart(Te);
				if((timerRead(Te)>0)&&(timerCheck(Te)>= timerGetThreshhold(Te)))
				{
					DEBUG("Fault: A_CLOSE_OUT_POSITION!\n");
					setFault(A_CLOSE_OUT_POSITION);
				}
			}
			else
			{			
				removeFault(A_CLOSE_OUT_POSITION);
				timerEnd(Te);
			}
		}

		if((lastDoor == E_doorStatus_CLOSE && lastPosition == E_inPosition_NOFLAT)&&(status_doorStatus == E_doorStatus_OPEN && status_inPosition == E_inPosition_NOFLAT))
		{
			DEBUG("Fault: A_OPEN_WHILE_RUNNIN!\n");
			setFault(A_OPEN_WHILE_RUNNIN);
		}

		//0:door closed; 1:door open, 0:in position, 1: not in position
		if((lastDoor == E_doorStatus_OPEN && lastPosition == E_inPosition_NOFLAT)&&(status_doorStatus == E_doorStatus_OPEN && status_inPosition == E_inPosition_NOFLAT))
		{
			DEBUG("Fault: A_OPEN_WITHOUT_STOPPIN!\n");
			setFault(A_OPEN_WITHOUT_STOPPIN);
		}

		//added by JC, 20161113
		if((lastDoor == E_doorStatus_CLOSE && lastPosition == E_inPosition_FLAT)&&(status_doorStatus == E_doorStatus_CLOSE && status_inPosition == E_inPosition_NOFLAT))
		{
			removeFault(A_OPEN_WITHOUT_STOPPIN);
		}

		/*check if fualts should be removed*/
		if(status_inPosition == E_inPosition_FLAT)
		{
			removeFault(A_HIT_GROUND);

			removeFault(A_HIT_CEILING);

			removeFault(A_STOP_OUT_AREA);
			
			removeFault(A_CLOSE_OUT_POSITION);
			timerEnd(Ta);
			timerEnd(Te);
			timerEnd(Tc);
			timerEnd(Td);
			inpositionCalled = 0;
			removeFault(A_OPEN_WHILE_RUNNIN);
			removeFault(A_OPEN_WITHOUT_STOPPIN);
			//removeFault(A_OVERSPEED);
			//removeFault(A_CLOSE_FAULT);
		}else{
			removeFault(A_CLOSE_IN_POSITION);
			timerEnd(Tb);
		}
			
		if(status_doorStatus == E_doorStatus_CLOSE && status_inPosition == E_inPosition_NOFLAT) //close and move
		{
			removeFault(A_CLOSE_FAULT);
		}

		if(status_inPosition == E_inPosition_FLAT && status_doorStatus == E_doorStatus_OPEN)
		{
			removeFault(A_CLOSE_IN_POSITION);

			timerEnd(Tb);
			timerEnd(Td);
			outpositionCalled = 0;
		}
		if(status_inPosition == E_inPosition_NOFLAT && status_doorStatus == E_doorStatus_OPEN)
		{
			removeFault(A_CLOSE_OUT_POSITION);

			timerEnd(Te);
			timerEnd(Td);
			outpositionCalled = 0;
		}

		lastPosition = status_inPosition;
		lastDoor = status_doorStatus;
		lastDirection = status_direction;

    }//status_inRepairing

}


int httpGet_ServerTime(void)
{
	JSON_Value *j_Value;
	JSON_Array *j_Array;
	JSON_Object *j_Object;


	DEBUG("httpGet_ServerTime finished^\n");

	return 0;
}

int httpPost_DeviceRegister(char* rsp)
{
	static int Count_MaxTry = 0;
	Cat1_return_t ret;
	int len=0;

	//一直注册不成功, 重试30次后重启xdk
	Count_MaxTry++;
	if(Count_MaxTry>30){
		rebootXDK(__FUNCTION__);
	}
	memset(IMEI_BUF, 0, IMEI_BUF_LEN);
	memset(socketCmdBuf,0,sizeof(socketCmdBuf));

#ifndef WIN_PLATFORM
	ret= cat1_get_IMEI(IMEI_BUF);
	
	DEBUG("%s IMEI_BUF=**%s**, ret=%d\n", __FUNCTION__, IMEI_BUF, ret);
	if(ret == Cat1_STATUS_SUCCESS){
		sprintf(socketCmdBuf, SPRE"{\"type\":\"login\",\"eid\":\"mx_%s\"}"SEND, IMEI_BUF);
		DEBUG("%s get IMEI OK. IMEI=%s", __FUNCTION__, IMEI_BUF);
	}else{
		DEBUG("%s get IMEI fail.", __FUNCTION__);
	}
#else
	sprintf(socketCmdBuf, SPRE"{\"type\":\"login\",\"eid\":\"%s\"}"SEND, "mx_863867025992437");
#endif
#ifdef DEVICE_ID_DEFAULT 
	memset(socketCmdBuf,0,sizeof(socketCmdBuf));
	sprintf(socketCmdBuf, SPRE"{\"type\":\"login\",\"eid\":\"%s\"}"SEND, DEVICE_ID_DEFAULT);
#endif
	
	LIFT_queueSendData_T msg;	
	memset(&msg, 0, sizeof(msg));
	msg.dataSize = strlen(socketCmdBuf);
	memcpy(msg.packetData, socketCmdBuf, strlen(socketCmdBuf));
#ifndef WIN_PLATFORM
	if(pdTRUE == xQueueSend( liftMsgQueueHandle, ( void* )&msg, 0 )){
		DEBUG("%s Send queue successful.", __FUNCTION__);
	}
#endif
	return 0;
}

int httpPost_DeviceRegister_rsp(char* rsp){
	DEBUG("%s rsp=>>%s<<", __FUNCTION__, rsp);
	JSON_Value *val1 = json_parse_string(rsp);
	JSON_Object *obj1 = NULL;

	obj1 = json_value_get_object(val1);
	int resid = (int)json_object_get_number(obj1, "res_id");
	json_value_free(val1);

	DEBUG("%s resid=>>%d<<", __FUNCTION__, resid);
	if(resid==0){
		return 0;
	}else{
		// TODO: retry ? httpPost_DeviceRegister();
		LIFT_SLEEP_MS(DEVICE_REGISTER_WAIT);
		httpPost_DeviceRegister("");
		return 1;
	}
}

int httpPost_HeartBeat_C2S(void)
{
	Cat1_return_t ret;
	int len=0;
	
	memset(socketCmdBuf,0,sizeof(socketCmdBuf));
	sprintf(socketCmdBuf, SPRE""C2S_CPING""SEND);
	
	LIFT_queueSendData_T msg;	
	memset(&msg, 0, sizeof(msg));
	msg.dataSize = strlen(socketCmdBuf);
	memcpy(msg.packetData, socketCmdBuf, strlen(socketCmdBuf));
#ifndef WIN_PLATFORM
	if(pdTRUE == xQueueSend( liftMsgQueueHandle, ( void* )&msg, 0 )){
		DEBUG("%s Send queue successful.", __FUNCTION__);
	}
#endif
	C2S_CPING_COUNT++;
	if(C2S_CPING_COUNT>C2S_CPING_COUNT_MAX){
		// TODO:  lost connection, reboot xdk and cat1
		rebootXDK(__FUNCTION__);
	}
	return 0;
}

int httpPost_HeartBeat_C2S_rsp(char* rsp)
{
	if(C2S_CPING_COUNT>0){
		C2S_CPING_COUNT--;
	}
	
	DEBUG("%s() C2S_CPING_COUNT=%d", __FUNCTION__, C2S_CPING_COUNT);
	return 0;
}

int httpPost_HeartBeat_S2C(char* rsp)
{
	Cat1_return_t ret;
	int len=0;
	
	S2C_PING_COUNT++;
	
	DEBUG("%s() S2C_PING_COUNT=%d", __FUNCTION__, S2C_PING_COUNT);
	
	memset(socketCmdBuf,0,sizeof(socketCmdBuf));
	sprintf(socketCmdBuf, SPRE""C2S_PING""SEND);
	
	LIFT_queueSendData_T msg;	
	memset(&msg, 0, sizeof(msg));
	msg.dataSize = strlen(socketCmdBuf);
	memcpy(msg.packetData, socketCmdBuf, strlen(socketCmdBuf));
#ifndef WIN_PLATFORM
	if(pdTRUE == xQueueSend( liftMsgQueueHandle, ( void* )&msg, 0 )){
		DEBUG("%s Send queue successful.", __FUNCTION__);
	}
#endif
	return 0;
}

int httpPost_KickOff(char* rsp)
{
	DEBUG("%s() ", __FUNCTION__);
	LIFT_SLEEP_MS(10000);
	rebootXDK(__FUNCTION__);
	return 0;
}


int httpPost_FaultAlert(int int_alertType)
{
	return 0;
}

int httpPost_FaultRemove(int int_alertType)
{
	return 0;
}

//处理从服务器的数据, 定时读取
int httpPost_ProcessServerMsg(unsigned int t)
{
	Cat1_return_t ret;
	uint16_t len=0;
	memset(socketCmdRecvMsg,0,sizeof(socketCmdRecvMsg));

	ret = cat1_recv((uint16_t*)socketCmdRecvMsg, &len);
	if(ret == Cat1_STATUS_SUCCESS){
		DEBUG("WebMsg: recv[%d]:>>%s<<\r\n",len,socketCmdRecvMsg);
	} else if (ret == Cat1_STATUS_TIMEOUT){
		DEBUG("WebMsg: recv data timeout ! \r\n");
		return 0;
	} else {
		DEBUG("WebMsg: recv data failed ! \r\n");
		return 0;
	}

	//开始处理服务器主动发的数据
    char *s = (char*)socketCmdRecvMsg;
    char *p;
	char *q;
	char *r;
	p = strstr(s, SPRE);
	q = strstr(s, SEND);

	while(p){
		if(q==NULL){
			continue;
		}

		memset(socketCmdRecvFragment,0,sizeof(socketCmdRecvFragment));
		memcpy(socketCmdRecvFragment, p + strlen(SPRE), q - p - strlen(SEND));

		DEBUG("WebMsg: fragment:>>%s<<\r\n", socketCmdRecvFragment);
		
		if(strstr(socketCmdRecvFragment, S2C_NEED_LOGIN)){
			httpPost_DeviceRegister(socketCmdRecvFragment);
		}else if(strstr(socketCmdRecvFragment, S2C_PING)){
			httpPost_HeartBeat_S2C(socketCmdRecvFragment);
		}else if(strstr(socketCmdRecvFragment, S2C_LOGIN)){
			httpPost_DeviceRegister_rsp(socketCmdRecvFragment);
		}else if(strstr(socketCmdRecvFragment, S2C_KICk)){
			httpPost_KickOff(socketCmdRecvFragment);
		}else if(strstr(socketCmdRecvFragment, S2C_CPING)){
			httpPost_HeartBeat_C2S_rsp(socketCmdRecvFragment);
		}

		//next 
		s = s + (q - p)+strlen(SPRE)+strlen(SEND);
		if (s > (((char*)(socketCmdRecvMsg[0])) + strlen((char*)socketCmdRecvMsg))){
			break;
		}
		p = strstr(s, SPRE);
		q = strstr(s, SEND);
		if (q == p){
			break;
		}
	};
	
	return 0;
}

int httpPost_StatusFault(int c){
	int f=0;
	int fault=0;
	Cat1_return_t ret;
	int len=0;
/*
{
"type": "info",
"data": {
	"direction": 0,
	"inposition": 0,
	"floor": 0,
	"door": 0,
	"haspeople": 0,
	"reparing": 0,
	"inmaintenance": 0,
	"fault": [1,2,3]
	}
}
标记说明
direction 方向：0 停留 1 上行 2 下行
inposition 平层：0 非平层 1 平层
floor 具体楼层
door 0关门 1开门 2 关门中 3 开门中
haspeople 有人：0 无人 1 有人
reparing 检修状态：0 否 1 是
inmaintenance 维保状态：0 否 1 是
fault 故障数组 Eg：[1,2,3]
故障编号说明
1 发生非平层困人故障
2 发生平层困人故障
3 发生运行中开门故障
4 发生门区外停梯故障
5 发生超速故障
6 发生冲顶故障
7 发生蹲底故障
8 发生开门走车故障

#define A_CLOSE_OUT_POSITION 	8//非平层关人；
#define A_CLOSE_IN_POSITION 	7//平层关人；
#define A_OPEN_WHILE_RUNNIN 	4//运行中开门；
#define A_STOP_OUT_AREA 	6//门区外停梯；
#define A_OVERSPEED 		3//超速；
#define A_HIT_CEILING 		1//冲顶；
#define A_HIT_GROUND 		2//蹲底；
#define A_OPEN_WITHOUT_STOPPIN 	5//开门走车；


*/
  	int status_direction = pElevatorStatus.EStatus_direction;
	int status_inPosition = (pElevatorStatus.EStatus_inPosition==E_inPosition_NOFLAT)?0:1;
	int status_currentFloor = pElevatorStatus.EStatus_currentFloor;
	int status_doorStatus = pElevatorStatus.EStatus_doorStatus;
	int status_havingPeople = pElevatorStatus.EStatus_havingPeople;
	int status_inReparing = pElevatorStatus.EStatus_inRepairing;
    int status_inMaintenance = 0;

	memset(FaultArray, 0, 100);

	fault = getFaultState(A_CLOSE_OUT_POSITION);
	if(fault){
		if(strlen(FaultArray)){
			strcat(FaultArray, ",1");
		}else{
			strcat(FaultArray, "1");
		}
	}
	fault = getFaultState(A_CLOSE_IN_POSITION);
	if(fault){
		if(strlen(FaultArray)){
			strcat(FaultArray, ",2");
		}else{
			strcat(FaultArray, "2");
		}
	}
	fault = getFaultState(A_OPEN_WHILE_RUNNIN);
	if(fault){
		if(strlen(FaultArray)){
			strcat(FaultArray, ",3");
		}else{
			strcat(FaultArray, "3");
		}
	}	
	fault = getFaultState(A_STOP_OUT_AREA);
	if(fault){
		if(strlen(FaultArray)){
			strcat(FaultArray, ",4");
		}else{
			strcat(FaultArray, "4");
		}
	}
	fault = getFaultState(A_OVERSPEED);
	if(fault){
		if(strlen(FaultArray)){
			strcat(FaultArray, ",5");
		}else{
			strcat(FaultArray, "5");
		}
	}
	fault = getFaultState(A_HIT_CEILING);
	if(fault){
		if(strlen(FaultArray)){
			strcat(FaultArray, ",6");
		}else{
			strcat(FaultArray, "6");
		}
	}
	fault = getFaultState(A_HIT_GROUND);
	if(fault){
		if(strlen(FaultArray)){
			strcat(FaultArray, ",7");
		}else{
			strcat(FaultArray, "7");
		}
	}
	fault = getFaultState(A_OPEN_WITHOUT_STOPPIN);
	if(fault){
		if(strlen(FaultArray)){
			strcat(FaultArray, ",8");
		}else{
			strcat(FaultArray, "8");
		}
	}

	memset(HttpPostStatusFaultBuffer, 0, SEND_LEN);
#if 1	
	sprintf(HttpPostStatusFaultBuffer, \
		SPRE"{\
\"type\": \"info\",\
\"data\": {\
\"direction\":%d,\
\"inposition\":%d,\
\"floor\":%d,\
\"door\":%d,\
\"haspeople\":%d,\
\"reparing\":%d,\
\"inmaintenance\":%d,\
\"fault\":[%s]\
}}"SEND, 
		status_direction,
		status_inPosition,
		status_currentFloor,
		status_doorStatus,
		status_havingPeople,
		status_inReparing,
		status_inMaintenance,
		FaultArray
	);
#else

	sprintf(HttpPostStatusFaultBuffer, \
		SPRE"{\"type\":\"info\",\"data\":{\"direction\":%d,\"inposition\":%d,\"floor\":%d,\"door\":%d,\"haspeople\":%d,\"reparing\":%d,\"inmaintenance\":%d,\"fault\":[%s]}}"SEND, 
			status_direction,
			status_inPosition,
			status_currentFloor,
			status_doorStatus,
			status_havingPeople,
			status_inReparing,
			status_inMaintenance,
			FaultArray
		);
#endif
	
	LIFT_queueSendData_T msg;	
	memset(&msg, 0, sizeof(msg));
	msg.dataSize = strlen(HttpPostStatusFaultBuffer);
	memcpy(msg.packetData, HttpPostStatusFaultBuffer, strlen(HttpPostStatusFaultBuffer));
#ifndef WIN_PLATFORM
	if(pdTRUE == xQueueSend( liftMsgQueueHandle, ( void* )&msg, 0 )){
		DEBUG("******httpPost_StatusFault xQueueSend OK. buf=**%s**\n", HttpPostStatusFaultBuffer);
	}else{
		DEBUG("******httpPost_StatusFault xQueueSend FAIL.");
	}
#endif
	//DEBUG("******httpPost_StatusFault OK. buf=**%s**\n", HttpPostStatusFaultBuffer);
	return 0;
}

void handleElevatorFault(void)
{
	//xCheckFault();
	int count = 0;
	int faultNumbers = getTotalFaultNumbers();
	while (faultNumbers)
	{
		if (getFaultState(faultNumbers) == 1)
		{
			count++;
			pElevatorStatus.EStatus_nowStatus = E_nowStatus_FAULT;
			if (getFaultSent(faultNumbers) == 0)
			{
				if (httpPost_FaultAlert(faultNumbers) == 0)
				{
					setFaultSent(faultNumbers); //set only send success, or getFaultSent(faultNumbers) is still 0
				}
				else
					INFO("httpPost_FaultAlert failed!!\n");
				currentFaultNumber = faultNumbers;//?? is it needed to put into above?
			}
			DEBUG("Info: ElevatorFault No. %d occurs!\n",faultNumbers);
		}
		if (getFaultRemoveState(faultNumbers) == 1)
		{
			if (httpPost_FaultRemove(faultNumbers) == 0)
			{
				clearFaultRemoveState(faultNumbers);//then no repeat removement
				clearFaultSent(faultNumbers); //allow to send alert
			}
			else
				INFO("httpPost_FaultRemove failed!!\n");

			DEBUG("Info: ElevatorFault No. %d has been removed^\n",faultNumbers);
		}
		faultNumbers--;
	}
	if (count == 0)
	{
		set_SStatus_LED(S_LED1_FAULT, 1); //black out
		pElevatorStatus.EStatus_nowStatus = E_nowStatus_NORMAL;
		currentFaultNumber = 0;
	}
	else
		set_SStatus_LED(S_LED1_FAULT, 0); //light the led
}

void handleElevatorStatus(void)
{
	int count = 0;
}

void getSensorsStatus(){
    static int i=0;
    int j=0, k=0;
	int timeIntervalUsec = 0;
	int len=0;
	char* p=NULL;
	int c=0;
	
    memset(UartBuf, 0, UARTBUFSIZE);

#ifdef WIN_PLATFORM

    if(T06[i][0] != 0x00){
        memcpy(UartBuf, T06[i], TITEMLEN);
        i++;
    }else{
        i=0;
        INFO("=============================================================\n");
		LIFT_SLEEP_MS(5000);
    }
	
#else
	Lift_return_t ret;
	ret = send_recv_lift(UartBuf);
	if(ret == Lift_STATUS_SUCCESS){
		INFO("send_recv_lift OK!\n");
	}else{
		INFO("send_recv_lift failed. ret=%d!\n", ret);
	}

#endif 
	// TODO: 读取buffer做组合处理

	p = UartBuf;
	
	while (*p == 0x7B){
		c++;

		printf("******UART: [%d]", c);
		for (k = 0; k<TITEMLEN; k++)
		{
			switch (k)
			{
			case 9: //00: 代表第1输入口的状态变化，建议上行传感器
				printf("  [UP]");
				break;
			case 10: //00: 代表第2输入口的状态变化，建议接下行传感器
				printf("  [DOWN]");
				break;
			case 11: //00: 代表第3输入口的状态变化，建议接基站校验传感器
				printf("  [BASE]");
				break;
			case 12: //00: 代表第4输入口的状态变化，建议接门开关传感器
				printf("  [DOOR]");
				break;
			case 13: //00: 代表第5输入口的状态变化，建议接人感传感器
				printf("  [PEOPLE]");
				break;
			case 14: //00: 代表第6输入口的状态变化，建议接极限开关
				printf("  [LIMIT]");
				break;
		
			default:
				break;
			}
			printf(" %02X", *(p+k));
		}
		printf("******\r\n");
		
		if(*p!=0x7B || *(p+TITEMLEN-1)!=0x7D){
			return;
		}
		
		for(j=9;j<15;j++){
			switch (j)
			{
			case 9: //00: 代表第1输入口的状态变化，建议上行传感器
				pSensorsStatus.SStatus_inPositionUp = *(p+j);
				break;
			case 10: //00: 代表第2输入口的状态变化，建议接下行传感器
				pSensorsStatus.SStatus_inPositionDown = *(p+j);
				break;
			case 11: //00: 代表第3输入口的状态变化，建议接基站校验传感器
				pSensorsStatus.SStatus_baseStation = *(p+j);
				break;
			case 12: //00: 代表第4输入口的状态变化，建议接门开关传感器
				pSensorsStatus.SStatus_doorOpen = *(p+j);
				break;
			case 13: //00: 代表第5输入口的状态变化，建议接人感传感器
			{
#if 0
				pSensorsStatus.SStatus_havingPeople = *(p+j);
#else
				if(timerRead(Tn) == 0){
					timerStart(Tn);
					pSensorsStatus.SStatus_havingPeople = *(p+j);
					lastSStatus_havingPeople = pSensorsStatus.SStatus_havingPeople;
				}
				if(timerCheck(Tn)>= timerGetThreshhold(Tn)){
					pSensorsStatus.SStatus_havingPeople = *(p+j);
					lastSStatus_havingPeople = pSensorsStatus.SStatus_havingPeople;
					timerEnd(Tn);
					timerStart(Tn);
				}
#endif				
			}
				break;
			case 14: //00: 代表第6输入口的状态变化，建议接极限开关
				pSensorsStatus.SStatus_limitSwitch = *(p+j);
				break;
		
			default:
				break;
			}
		}
		
		//根据上下行传感器确定电梯运行方向
		if(pSensorsStatus.SStatus_inPositionUp==S_inPositionUp_OFF 
			&& pSensorsStatus.SStatus_inPositionDown==S_inPositionDown_OFF){
			gettimeofday(&tv_speed2, NULL);
			if ((timeIntervalUsec = tv_cmp(tv_speed2, tv_speed1)) > 0)
			{
				pElevatorStatus.EStatus_speed = TIME_UNIT * UpDownDistance / timeIntervalUsec;
				//DEBUG("**** speed is %f ****\n",pElevatorStatus.EStatus_speed);
			}
		}else if (pSensorsStatus.SStatus_inPositionUp==S_inPositionUp_ON 
				&& pSensorsStatus.SStatus_inPositionDown==S_inPositionDown_ON){
			if(pElevatorStatus.EStatus_direction ==E_direction_UP){//up
				if (pElevatorStatus.EStatus_currentFloor < Lmax) pElevatorStatus.EStatus_currentFloor++;
				if (pElevatorStatus.EStatus_currentFloor == 0) pElevatorStatus.EStatus_currentFloor++;
			}else if(pElevatorStatus.EStatus_direction ==E_direction_DOWN){//down
				if (pElevatorStatus.EStatus_currentFloor > Lmin) pElevatorStatus.EStatus_currentFloor--;
				if (pElevatorStatus.EStatus_currentFloor == 0) pElevatorStatus.EStatus_currentFloor--;
			}
			pElevatorStatus.EStatus_direction = E_direction_STOP; //0:stop, 1:up, 2:down
			pElevatorStatus.EStatus_inPosition = E_inPosition_FLAT;//flat floor
			gettimeofday(&tv_speed1, NULL);
		
		}else if (pSensorsStatus.SStatus_inPositionUp==S_inPositionUp_ON 
				&& pSensorsStatus.SStatus_inPositionDown==S_inPositionDown_OFF){
			gettimeofday(&tv_speed1, NULL);
			if(pElevatorStatus.EStatus_direction == E_direction_STOP){
				pElevatorStatus.EStatus_direction = E_direction_DOWN;//0:stop, 1:up, 2:down
				pElevatorStatus.EStatus_inPosition = E_inPosition_NOFLAT;//not flat floor
			}else{
				DEBUG("****** ERROR at %s #%d: previous NOT FLAT\n",__FUNCTION__, __LINE__);
			}
		}else if (pSensorsStatus.SStatus_inPositionUp==S_inPositionUp_OFF 
				&& pSensorsStatus.SStatus_inPositionDown==S_inPositionDown_ON){
			gettimeofday(&tv_speed1, NULL);
			if(pElevatorStatus.EStatus_direction == E_direction_STOP){
				pElevatorStatus.EStatus_direction = E_direction_UP;//0:stop, 1:up, 2:down
				pElevatorStatus.EStatus_inPosition = E_inPosition_NOFLAT;//not flat floor
			}else{
				DEBUG("****** ERROR at %s #%d: previous NOT FLAT\n",__FUNCTION__, __LINE__);				
			}
		}
		
		pElevatorStatus.EStatus_powerStatus = get_EStatus_powerStatus();
		pElevatorStatus.EStatus_havingPeople = get_EStatus_havingPeople();
		pElevatorStatus.EStatus_doorStatus = get_EStatus_doorStatus();
		
		if(pSensorsStatus.SStatus_baseStation==S_baseStation_ON){
			pElevatorStatus.EStatus_currentFloor = Lbase;
			pElevatorStatus.EStatus_inPositionBase = E_inPositionBase_YES; //基站
			currentFloorRefreshed = 1;
		}
		else{
			pElevatorStatus.EStatus_inPositionBase = E_inPositionBase_NO;//非基站
			currentFloorRefreshed = 0;
		}

		//如果有多条io uart 数据, 循环处理
		p+=TITEMLEN;
	}
	


    return;
}

//=================================================================


int CheckLiftStatus(void){
 	static int count=0;
	DEBUG("%s()\r\n", __FUNCTION__);

	sensorsStatus_init();
	elevatorStatus_init();
	
	for(;;)
	{
		WDG_feedingWatchdog();
		count++;

		DEBUG("**[%d]**\n", count);
		
		if(count<10){
			LIFT_SLEEP_MS(100);
			continue;
		}
	
		if(count%60==0){
			//send heart beat
			DEBUG("send heart beat at %d\n", count);
			httpPost_HeartBeat_C2S();
		}
		
		/*refresh status of all sensors*/
		getSensorsStatus();

		/*check faults when elevator not in a repairing*/
		//sem_p(sem_d);
		checkElevatorFault();
		handleElevatorStatus();
		httpPost_StatusFault(0);
		//sem_v(sem_d);

		/*check every second*/
		LIFT_SLEEP_MS(500);
	}
	return 0;
}

//=============================================================
#ifdef WIN_PLATFORM
//这里是windows模拟使用. 
HANDLE event;
HANDLE mutex;

DWORD WINAPI checkStatusTask(LPVOID pM)
{
	INFO("checkStatusTask start^\n");
    CheckLiftStatus();
	return 1;
}

DWORD WINAPI processServerMsgTask(LPVOID pM)
{
	static unsigned int count=0;
	for(;;)
	{
		count++;
		INFO("processServerMsgTask %d^\n", count);
		httpPost_ProcessServerMsg(count);
		LIFT_SLEEP_MS(500);
	}
	return 1;
}

void persistence_example(void) {
	JSON_Value *val1 = json_parse_string("{\"name\":\"abel\"}");
	JSON_Object *obj1 = NULL;

	obj1 = json_value_get_object(val1);
	const char * name = json_object_get_string(obj1, "name");

	json_value_free(val1);


	JSON_Value *schema = json_value_init_object();
	JSON_Object *schema_obj = json_value_get_object(schema);
	json_object_set_string(schema_obj, "first", "");
	json_object_set_string(schema_obj, "last", "");
	json_object_set_number(schema_obj, "age", 0);

	char* js = json_serialize_to_string((const JSON_Value *)schema);
	json_value_free(schema);

	return;
}

void testjson(void){
	persistence_example();	
}

static SOCKET sclient;

Cat1_return_t cat1_send(uint16_t *sendData, uint16_t len){
    int ret=0;
	send(sclient, (char*)sendData, strlen((char*)sendData), 0);
#if 0
    char recData[CMD_RECV];
	memset(recData, 0, CMD_RECV);
    ret = recv(sclient, recData, CMD_RECV, 0);
    if(ret > 0)
    {
        recData[ret] = 0x00;
        printf(recData);
		return Cat1_STATUS_SUCCESS;
    }
#endif	
	return Cat1_STATUS_SUCCESS;
}

Cat1_return_t cat1_recv(uint16_t *buf, uint16_t *len)
{	
	int ret = recv(sclient, (char*)buf, CMD_RECV, 0);
	*len = ret;
    if(ret > 0)
    {
		*len = ret;
		return Cat1_STATUS_SUCCESS;
    }else if(ret == -1){
    	return Cat1_STATUS_FAILED;
    }
}

int initSocket(char* ip, int port)
{
    WORD sockVersion = MAKEWORD(2,2);
    WSADATA data; 
    if(WSAStartup(sockVersion, &data) != 0)
    {
        return 0;
    }

    sclient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sclient == INVALID_SOCKET)
    {
        printf("invalid socket !");
        return 0;
    }

    sockaddr_in serAddr;
    serAddr.sin_family = AF_INET;
    serAddr.sin_port = htons(port);
    serAddr.sin_addr.S_un.S_addr = inet_addr(ip); 
    if (connect(sclient, (sockaddr *)&serAddr, sizeof(serAddr)) == SOCKET_ERROR)
    {
        printf("connect error !");
        closesocket(sclient);
        return 0;
    }

    return 0;
}

void testsocket(void){
	char* buf = "#10#{\"type\":\"login\",\"eid\":\"mx12345678\"}#13#";
	cat1_send((uint16_t*)buf, strlen(buf));
}

int _tmain(int argc, _TCHAR* argv[])
{
	initSocket("114.55.42.28", 26500);
	//testsocket();


	HANDLE handle1 = CreateThread(NULL, 0, checkStatusTask, NULL, 0, NULL);
	HANDLE handle2 = CreateThread(NULL, 0, processServerMsgTask, NULL, 0, NULL);
	WaitForSingleObject(handle1, INFINITE);
	WaitForSingleObject(handle2, INFINITE);


	return 0;
}


#else
//下面是运行在xdk系统内
//*************************************************************************

void liftCheckHeartBeatTimer(xTimerHandle pvParameters)
{
	(void) pvParameters;
	
	DEBUG("%s() S2C_PING_COUNT=%d\r\n", __FUNCTION__, S2C_PING_COUNT);
	if(S2C_PING_COUNT){
		S2C_PING_COUNT = 0;
	}else{
		// TODO: reboot xdk 
		//rebootXDK(__FUNCTION__);
	}
}

void liftCheckStartHeartBeatTimer(void)
{
	/* Start the timers */
	xTimerStart(liftCheckTimerHandle, UINT32_MAX);
	return;
}

void liftCheckStopHeartBeatTimer(void)
{
	/* Stop the timers */
	xTimerStop(liftCheckTimerHandle, UINT32_MAX);
	return;
}

void liftWebServerTask(void *pvParameters)
{
	static uint32_t count=0;
 	LIFT_queueSendData_T msg;	
	Cat1_return_t ret;
	int len=0;
	int c=0;
	
	DEBUG("%s()\r\n", __FUNCTION__);

	for(;;)  
    {  
		count++;
		
		DEBUG("%s(%d)\r\n", __FUNCTION__, count);
		
		//先处理接收数据
		httpPost_ProcessServerMsg(count);

		//后处理发送数据
		memset(&msg, 0, sizeof(msg));
        if(xQueueReceive( liftMsgQueueHandle, &msg, 10/portTICK_RATE_MS ) == pdPASS)  
        {  
            DEBUG("WebMsg: send:[%d]++%s++\r\n",msg.dataSize, msg.packetData);  
			c=0;
			do{
				ret = cat1_send((uint16_t*) msg.packetData, msg.dataSize);
				if (ret == Cat1_STATUS_SUCCESS) {
					DEBUG("WebMsg: send data OK :++%s++\r\n",msg.packetData);
					break;
				} else { 
					c++;
					DEBUG("WebMsg: send data FAIL! Try [%d] times.\n", c);
					LIFT_SLEEP_MS(10);
				}
			}while(c<CAT1_SEND_RETRY_MAX);
			if(c>=CAT1_SEND_RETRY_MAX){
				rebootCat1(__FUNCTION__);
			}
        }
		LIFT_SLEEP_MS(10);
    }  	
	return;
}

void liftCheckTask(void *pvParameters)
{
 	static int count=0;
	DEBUG("%s()\r\n", __FUNCTION__);
	
	CheckLiftStatus();
	
	return;
}

void liftCheckClientInit(void)
{
	Cat1_return_t ret;
	Lift_return_t retlift;
	/* Initialize Variables */
    int rc = 0;

	DEBUG("%s()\r\n", __FUNCTION__);

 	ret=cat1_server_init(CAT1_SERVER_IP,CAT1_SERVER_PORT);

 	//ret=cat1_server_init("121.196.219.49","26500");
	if (ret != Cat1_STATUS_SUCCESS )
	{
		DEBUG("cat1 init failed\r\n");
		//cat1 失败就延时后重启xdk
		LIFT_SLEEP_MS(10000);
		rebootXDK(__FUNCTION__);
		
		return;
	}else{
		DEBUG("Cat1 init Success!\r\n");
	}
	
	liftMsgQueueHandle = xQueueCreate( LIFT_SEND_QUEUE_SIZE , sizeof( LIFT_queueSendData_T ) );  
	if (liftMsgQueueHandle == NULL)
    {
        DEBUG("liftMsgQueueHandle Send Queue could not be created");
		
		//失败就延时后重启xdk
		LIFT_SLEEP_MS(10000);
		rebootXDK(__FUNCTION__);
		
		return;
    }else{
        DEBUG("liftMsgQueueHandle Send Queue OK!");
    }

	retlift = Lift_DriverInit();
	if(retlift == Lift_STATUS_SUCCESS) {
		DEBUG("LIFT USart_2 initialization SUCCESSFUL\r\n");
	} else {
		DEBUG("LIFT USart_2 initialization FAILED\r\n");
		//失败就延时后重启xdk
		LIFT_SLEEP_MS(10000);
		rebootXDK(__FUNCTION__);
		
		return;
	}

	/* Create Live Data Check */
    liftCheckTimerHandle = xTimerCreate(
			(const char * const) "LiftCheckTimer",
			HEART_BEAT_RATE,
			TIMER_AUTORELOAD_ON,
			NULL,
			liftCheckHeartBeatTimer);

    rc = xTaskCreate(liftCheckTask, (const char * const) "LiftCheckTask",
                    		1024*5, NULL, 1, &liftCheckTaskHandler);

    /* Error Occured Exit App */
    if(rc < 0){
		DEBUG("liftCheckTask initialization FAILED\r\n");
		
		//失败就延时后重启xdk
		LIFT_SLEEP_MS(10000);
		rebootXDK(__FUNCTION__);
		
		return;
    }else{
		DEBUG("liftCheckTask initialization OK\r\n");
    }

    rc = xTaskCreate(liftWebServerTask, (const char * const) "liftWebServerTask",
                    		1024*5, NULL, 1, &liftWebServerTaskHandler);

    /* Error Occured Exit App */
    if(rc < 0){
		DEBUG("liftWebServerTask initialization FAILED\r\n");
		
		//失败就延时后重启xdk
		LIFT_SLEEP_MS(10000);
		rebootXDK(__FUNCTION__);
		
		return;
	}else{
		DEBUG("liftWebServerTask initialization OK\r\n");
	}

	liftCheckStartHeartBeatTimer();

	//启动看门狗
    WDG_init(WDG_FREQ, WATCH_DOG_TIMEOUT);
    return;
}


#endif

