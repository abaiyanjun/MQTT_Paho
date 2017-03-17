

//#include "stdafx.h"

#undef WIN_PLATFORM

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
#include "timers.h"
#include "PTD_portDriver_ph.h"
#include "PTD_portDriver_ih.h"
#include "WDG_watchdog_ih.h"
#include "cat1_API.h"

#include "parson.h"
#include "liftcheck.h"
#include "lift_UartDev.h"
#endif


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

#endif



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

#define S_havingPeople_ON	1   	//having people: low level ---> high level, changed on May 23, 2016
#define S_havingPeople_OFF	0   	//not having people: high level ---> low level, changed on May 23, 2016

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

//type elevator_status 标识电梯状态上传。
//now_status 0-3 当前状态[0 正常 1 故障 2 平层关人 3 非平层关人]
//direction 0-2 方向[0 停留 1 上行 2 下行]
//floor_status 0-1 平层状态[0平层 1 非平层]
//speed real 梯速
//door_status 0-3 门状态[0 关门 1 开门 2 关门中 3 开门中]
//has_people 0-1 人状态[0 无人 1 有人]
//power_status 0-2 电力[0 电源 1 电池 2 其他]
//now_floor int 当前楼层

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

#define SOCKETBUFSIZE 1024

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
#define Tn 13 //（保留）

int Cg = 0; //运行中开门过层层数计数器 1//not used now
int Ci = 0; //开门走车过层层数计数器  1//not used now
int Cm = 5; //停车时开关门次数计数器1

//#define Sk 0 //额定梯速
//#define Sn 1 //超速倍数定义
//#define Sl 2 //超速过层层数
//#define Lbase 3 //number of base floor
//#define Lmax 4 //number of max floor
//#define Lmin 5 //number of min floor
//special varibles
int Sk = 18;//额定梯速,m/s
int Sn = 12; //超速倍数定义1.2*1.8=2.2
int Sl = 2; //超速过层层数 //use local varible l later
int Si = 20;
int Lbase = 1; //number of base floor
int Lmax = 3; //number of max floor
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

struct T_CanData{
	unsigned char CData_errorCode;
	int CData_compFault;
	int CData_inRepairing;
	int CData_inRunning;
	int CData_openDoorCommand;
	int CData_doorOpenFinish;
	int CData_layerDoorOpen;
	int CData_closeDoorFinish;
	int CData_inPosition;
	int CData_inFireControl;
	int CData_innerCall;
	int CData_directionUp;
	int CData_directionDown;
	int CData_nowLayer;
	int CData_totalRunCounts;
	int CData_totalRunTime;
	int CData_totalRopeBendCounts;
};

struct T_IOData1{//dongzhi
	int IOData1_errorCode;//P0-7,8bits
	int IOData1_inRepairing;//P9
	int IOData1_carDoorOpenFinish;//P12
	int IOData1_hallDoorStatus;//P13
	int IOData1_inPosition;//P15
	int IOData1_directionUp;//P18
	int IOData1_directionDown;//P19
	int IOData1_nowFloor;//P24-P39,16bits
};

struct T_IOData2{//disen
	int IOData2_nowFloorH;			//#1当前楼层
	int IOData2_nowFloorL;
	int IOData2_nowStatus;			//#2电梯状态
	int IOData2_direction;			//#3运行方向
	int IOData2_carDoorStatus;		//#4厢门开闭
	int IOData2_powerCut;			//#5电梯停电
	int IOData2_stopOutArea;		//#6门区外停梯
	int IOData2_hitCeiling;			//#7冲顶
	int IOData2_hitGround;			//#8蹲底
	int IOData2_openWhileRunning;	//#9运行中开门
	int IOData2_overSpeed;			//#10超速
	int IOData2_openWithoutStopping;	//#11开门走梯
	int IOData2_peopleTrapped;		//#12电梯困人
	int IOData2_absoluteFloor;			//#13绝对楼层
	int IOData2_inRepairing;			//#14保留
	int IOData2_reserved2;			//#15保留
	int IOData2_reserved3;			//#16保留
};

struct T_IOData3{//sanrong
	int IOData3_safeLoopBreak;
	int IOData3_lockLoopBreak; //off: door open, on: door close
	int IOData3_inPositionUp;
	int IOData3_inPositionDown;
	int IOData3_forceReductionUp;
	int IOData3_locationLimitUp;
	int IOData3_forceReductionDown;
	int IOData3_locationLimitDown;
	int IOData3_Locked;
	int IOData3_fireControl;
	int IOData3_overLoaded;
	int IOData3_directionUp;
	int IOData3_directionDown;
	int IOData3_brakeDetction;
	int IOData3_brakeContactor;
	int IOData3_mainContactor;
	int IOData3_inRepairing;//reserved
	int IOData3_overSpeed;//reserved
	int IOData3_nowFloor;
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
//struct T_CanData pCanData;
struct T_IOData1 pIOData1;
struct T_IOData2 pIOData2;
struct T_IOData3 pIOData3;
int currentUp = 0;
int currentDown = 0;
int lastUp = 0;
int lastDown = 0;

//int currentPosition = 0;
int lastPosition = 9;  //here 9 means uncertain
//int currentDoor = 0;
int lastDoor = 9;

int BaseFloor = 1;
float UpDownDistance;

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

int wsTimer = 0;
int timers[14] = { 0 };
//byj int timerThreshhold[14] = { 30, 60, 30, 300, 60, 30, 30, 30, 30, 30, 20, 30, 30, 30 }; //default values, change Td to 300, JC, 20160430
int timerThreshhold[14] = { 3, 6, 3, 30, 6, 3, 3, 3, 3, 3, 2, 3, 3, 3 }; //default values, change Td to 300, JC, 20160430
///////////////////////////Ta, Tb, Tc, Td,  Te, Tf, Tg, Th, Ti, Tj, Tk, Tl, Tm, Tn
int time_StreamCloud = 300;
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

int connectLost = 1;
int connectVideoLost = 1;
//int fd_io = NULL;
//int fd_video = NULL;//connection for video, audio and player

//int fd_status = NULL;//unused
//int fd_Alarm = NULL;//unused

#define SECOND 1
#define USECOND 0


#define TITEMLEN 22
#define TITEMCOUNT 64

unsigned char TDATA[64][TITEMLEN] = { { 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x01, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x7D },
{ 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x7D }
};

/*
模拟数据
*/

//正常运行状态中，检测到开门信号
unsigned char T01[][TITEMLEN] ={
    {0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x7D},
    {0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x7D},
    {0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x7D},
    {0x00}
};

//A_STOP_OUT_AREA 	6//门区外停梯；
unsigned char T02[][TITEMLEN] ={
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x7D},
    {0x00}
};










char UartBuf[SOCKETBUFSIZE];
size_t UartBufLen=0;


void getSensorsStatus();



time_t t_timers;

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
	return time(&t_timers) - timers[num];
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

int timerVideo = 0;

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

int timerAudio = 0;

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



/*
int counterGetThreshhold(int num)
{
return counterThreshhold[num];
}

int counterSetThreshhold(int num, int value)
{
counterThreshhold[num] = value;
}
*/

int fd_ttysac0 = NULL;
int qstatus;

/*
struct T_IOData1{//dongzhi
int IOData1_errorCode;//P0-7,8bits
int IOData1_inRepairing;//P9
int IOData1_carDoorOpenFinish;//P12
int IOData1_hallDoorStatus;//P13
int IOData1_inPosition;//P15
int IOData1_directionUp;//P18
int IOData1_directionDown;//P19
int IOData1_nowFloor;//P24-P39,16bits
};
*/

#define SLIMIT	15
#define SPEOPLE	16
#define SDOOR	17
#define SBASE	18
#define SDOWN	19
#define SUP		20

int process_server1(int *SocketConnect, char* SocketRxBuf, int SocketRxlen)
{
	int i;
	int timeIntervalUsec = 0;

	printf("received io data is:");
	for (i = 0; i<SocketRxlen; i++)
	{
		printf(" %02x", SocketRxBuf[i]);
	}
	printf("\r\n");

	if ((SocketRxBuf[0] == 0x7B) && (SocketRxBuf[2] == 0x00))//02: Dongzhi
	{
		int dataLen = SocketRxBuf[6];
		DEBUG("dataLen is %d\n",dataLen);
		
		if ((SocketRxBuf[SDOOR] & bit1) == 0)
		{
			pSensorsStatus.SStatus_doorOpen = 0;
			pIOData1.IOData1_hallDoorStatus = 0;
		}
		else
		{
			pSensorsStatus.SStatus_doorOpen = 1;
			pIOData1.IOData1_hallDoorStatus = 1;
		}

		if ((SocketRxBuf[SDOWN] & bit1) == 0)
		{
			pSensorsStatus.SStatus_inPositionDown = 0;
			if (pSensorsStatus.SStatus_inPositionUp == 1)
			{
				pElevatorStatus.EStatus_direction = 2;//0:stop, 1:up, 2:down
			}
			else
			{
				pElevatorStatus.EStatus_direction = 1;
			}
			pIOData1.IOData1_directionDown = 0;
		}
		else
		{
			pSensorsStatus.SStatus_inPositionDown = 1;
			if (pSensorsStatus.SStatus_inPositionUp == 1)
			{
				pElevatorStatus.EStatus_direction = 1;//0:stop, 1:up, 2:down
			}
			else
			{
				pElevatorStatus.EStatus_direction = 2;
				if (pElevatorStatus.EStatus_currentFloor > Lmin) pElevatorStatus.EStatus_currentFloor--;
				if (pElevatorStatus.EStatus_currentFloor == 0) pElevatorStatus.EStatus_currentFloor = -1;
			}
			pIOData1.IOData1_directionDown = 1;
		}

		if ((SocketRxBuf[SUP] & bit1) == 0)
		{
			pSensorsStatus.SStatus_inPositionUp = 0;
			gettimeofday(&tv_speed2, NULL);
			if ((timeIntervalUsec = tv_cmp(tv_speed2, tv_speed1)) > 0)
			{
				pElevatorStatus.EStatus_speed = TIME_UNIT * UpDownDistance / timeIntervalUsec;//10^6-->10^5
				//DEBUG("**** speed is %f ****\n",pElevatorStatus.EStatus_speed);
			}
			pIOData1.IOData1_directionUp = 0;
		}
		else
		{
			pSensorsStatus.SStatus_inPositionUp = 1;
			if (pSensorsStatus.SStatus_inPositionDown == 1)
			{
				//pElevatorStatus.EStatus_direction = 1;//0:stop, 1:up, 2:down
			}
			else
			{
				//pElevatorStatus.EStatus_direction = 2;
				if (pElevatorStatus.EStatus_currentFloor < Lmax) pElevatorStatus.EStatus_currentFloor++;
				if (pElevatorStatus.EStatus_currentFloor == 0) pElevatorStatus.EStatus_currentFloor = 1;
			}
			pIOData1.IOData1_directionUp = 1;
		}

		if ((SocketRxBuf[SPEOPLE] & bit1) == 0)
		{
			pSensorsStatus.SStatus_havingPeople = 0;
		}
		else
		{
			pSensorsStatus.SStatus_havingPeople = 1;
		}

		return 0;
	}
	return -1;
}



int GPIORead(int gpio){
	return 0;
}

int GPIOWrite(int gpio, int v){
	return v;
}
/*get-functions of Sensors Status*/
int get_SStatus_inPositionUp()//maybe changed from other sources
{
	return GPIORead(S_IN_POSITION_UP);
}

int get_SStatus_inPositionDown()
{
	return GPIORead(S_IN_POSITION_DOWN);
}

int get_SStatus_inPositionFlat()
{
	return GPIORead(S_IN_POSITION_FLAT);
}

int get_SStatus_doorOpen()
{
	return GPIORead(S_DOOR);
}

int get_SStatus_havingPeople()
{
	return GPIORead(S_PEOPLE);
}

int get_SStatus_inRepairing()
{
	return GPIORead(S_REPAIRING);
}

int get_SStatus_alarmButtonPushed()
{
	return GPIORead(S_ALARM);
}
int get_SStatus_safeCircuit()
{
	return GPIORead(S_SAFE_CIRCUIT);
}

int get_SStatus_runningContactor()
{
	return GPIORead(S_RUNNING_CONTACTOR);
}

int get_SStatus_generalContactor()
{
	return GPIORead(S_GENERAL_CONTACTOR);
}


int set_SStatus_emergencyLED(int sig)//maybe changed from other sources
{
	if (sig == 0 || sig == 1)
		return GPIOWrite(S_LED_EMERGENCY, sig);  //return 0 if successful
	else
		return -1;
}

int set_SStatus_LED(int led, int sig)//maybe changed from other sources
{
	if (sig == 0 || sig == 1)
	{
		if (led == 2)
			return GPIOWrite(S_LED2_NORMAL, sig);  //return 0 if successful
		else if (led == 1)
			return GPIOWrite(S_LED1_FAULT, sig);
		else if (led == 3)
			return GPIOWrite(S_LED3_REPAIRING, sig);
		else
			return -1;
	}
	else
		return -1;
}

/*get-functions of EStatus Status*/

int get_EStatus_nowStatus()//not used
{
	return 0;
}

int get_EStatus_powerStatus()
{
	return 0;//GPIORead(S_POWER_STATUS); 0 is normal 
}

int get_EStatus_doorStatus()
{
	if (pSensorsStatus.SStatus_doorOpen == S_doorOpen_ON)
		return 1;
	else
		return 0;
	//if(pSensorsStatus.SStatus_doorOpen == 0) return 1;
}

int get_EStatus_inPosition()//maybe should add speed = 0 or direction = 0,NO
{
	if (pSensorsStatus.SStatus_inPositionUp == S_inPositionUp_ON && pSensorsStatus.SStatus_inPositionDown == S_inPositionDown_ON)
	{
		pElevatorStatus.EStatus_direction = 0;//from get_EStatus_direction()
		return 0; //flat floor
	}
	else
		return 1; //not flat floor
}


//function: get_EStatus_havingPeople
//para: void
//return: 0:no people, 1:having people
//decripiton: when started, the sensor is off, timerCheck(Tk) is time - 0 > threshhold, it will return 0.
int get_EStatus_havingPeople()
{
	//INFO("SStatus_havingPeople: %d, Tk: %d\n",pSensorsStatus.SStatus_havingPeople,timerCheck(Tk));
	if (pSensorsStatus.SStatus_havingPeople == S_havingPeople_ON)
	{
		timerEnd(Tk);
		timerStart(Tk);
		return 1;
	}
	else
	{
		if (timerCheck(Tk) < timerGetThreshhold(Tk))
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}



	/*
	if((pSensorsStatus.SStatus_havingPeople == S_havingPeople_OFF)&&(timerCheck(Tk) < timerGetThreshhold(Tk)))
	{
	return 1;
	}else//!a || !b?
	return 0;
	*/
}
void getElevatorStatus_IO1(void)
{
	pElevatorStatus.EStatus_powerStatus = get_EStatus_powerStatus();
	pElevatorStatus.EStatus_havingPeople = get_EStatus_havingPeople();

	//if(pIOData1.IOData1_inRepairing == 1)
	//	pElevatorStatus.EStatus_nowStatus = 4;

	if (pIOData1.IOData1_directionUp == 1)
		pElevatorStatus.EStatus_direction = 1; //up
	else if (pIOData1.IOData1_directionDown == 1)
		pElevatorStatus.EStatus_direction = 2; //down
	else
		pElevatorStatus.EStatus_direction = 0; //stop

	if (pIOData1.IOData1_carDoorOpenFinish == 1)
		pElevatorStatus.EStatus_doorStatus = 1; //door open
	else	//NO carDoorCloseFinish
		pElevatorStatus.EStatus_doorStatus = 0; //door closed

	if (pIOData1.IOData1_inPosition == 1)
		pElevatorStatus.EStatus_inPosition = 0; //!!! 0 means in position
	else
		pElevatorStatus.EStatus_inPosition = 1;
	//pElevatorStatus.EStatus_speed = get_EStatus_speed(); //no needed for this version
	pElevatorStatus.EStatus_currentFloor = pIOData1.IOData1_nowFloor;

	//DEBUG("Info: ElevatorStatus got^\n");
}



void refreshStatus(void)
{
	getSensorsStatus();//replaced by signal

	//getElevatorStatus_IO1();
	DEBUG("Info: all status refreshed^\n");
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
}


void ioData1_init(void)
{
	pIOData1.IOData1_errorCode = 0x00;//P0-7,8bits
	pIOData1.IOData1_inRepairing = 0;//P9
	pIOData1.IOData1_carDoorOpenFinish = 0;//P12
	pIOData1.IOData1_hallDoorStatus = 0;//P13
	pIOData1.IOData1_inPosition = 1;//P15
	pIOData1.IOData1_directionUp = 0;//P18
	pIOData1.IOData1_directionDown = 0;//P19
	pIOData1.IOData1_nowFloor = 1;//P24-P39,16bits
}

void ioData2_init(void)
{
	pIOData2.IOData2_nowFloorH = 0x30;//ASCII
	pIOData2.IOData2_nowFloorL = 0x30;//ASCII

	pIOData2.IOData2_nowStatus = 0;//0123
	pIOData2.IOData2_direction = 0;//012		//#3运行方向
	pIOData2.IOData2_carDoorStatus = 2;//012	//#4厢门开闭
	pIOData2.IOData2_powerCut = 0;//01		//#5电梯停电
	pIOData2.IOData2_stopOutArea = 0;//01		//#6门区外停梯
	pIOData2.IOData2_hitCeiling = 0;//01		//#7冲顶
	pIOData2.IOData2_hitGround = 0;//01		//#8蹲底
	pIOData2.IOData2_openWhileRunning = 0;//01	//#9运行中开门
	pIOData2.IOData2_overSpeed = 0;//01		//#10超速
	pIOData2.IOData2_openWithoutStopping = 0;//01	//#11开门走梯
	pIOData2.IOData2_peopleTrapped = 0;//01		//#12电梯困人
	pIOData2.IOData2_absoluteFloor = 1;		//#13绝对楼层
}


void ioData3_init(void)
{
	pIOData3.IOData3_safeLoopBreak = 1;//0(break):fault,1(close):safe
	pIOData3.IOData3_lockLoopBreak = 1; //0(break, open): hall door open, 1(close): hall door close
	pIOData3.IOData3_inPositionUp = 1;
	pIOData3.IOData3_inPositionDown = 1;
	pIOData3.IOData3_forceReductionUp = 0;
	pIOData3.IOData3_locationLimitUp = 0;
	pIOData3.IOData3_forceReductionDown = 0;
	pIOData3.IOData3_locationLimitDown = 0;
	pIOData3.IOData3_Locked = 0;
	pIOData3.IOData3_fireControl = 0;
	pIOData3.IOData3_overLoaded = 0;
	pIOData3.IOData3_directionUp = 0;
	pIOData3.IOData3_directionDown = 0;
	pIOData3.IOData3_brakeDetction = 0;//?
	pIOData3.IOData3_brakeContactor = 1;//?
	pIOData3.IOData3_mainContactor = 1;//?
	pIOData3.IOData3_inRepairing = 0;//reserved
	pIOData3.IOData3_overSpeed = 0;//reserved
	pIOData3.IOData3_nowFloor = 1;
}

void elevatorStatus_init()
{
	pElevatorStatus.EStatus_nowStatus = 0; //default normal
	pElevatorStatus.EStatus_powerStatus = 0;
	pElevatorStatus.EStatus_direction = 0;
	pElevatorStatus.EStatus_doorStatus = 0;
	pElevatorStatus.EStatus_inPosition = 0;
	pElevatorStatus.EStatus_speed = 0;
	pElevatorStatus.EStatus_havingPeople = 0;
	pElevatorStatus.EStatus_currentFloor = 0;//BaseFloor;because there is no floor #0;
	pElevatorStatus.EStatus_backup1 = NULL;
	pElevatorStatus.EStatus_backup2 = NULL;
}

int inpositionCalled = 0;	//flag of being call for CLOSE_IN_POSITION
int outpositionCalled = 0;	//flag of being call for CLOSE_OUT_POSITION
int inpositionBroadcasted = 0;
int outpositionBroadcasted = 0;
//int lastDoorStatus = 0;
//int lastInPosition = 0;


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
	DEBUG("***********          Fault: %d        ******\n", faultNUM);	
}

int getFaultState(int faultNUM)
{
	return faultTypeIndi[faultNUM];
}

/*****************************/
void removeFault(int faultNUM)
{
	if (getFaultState(faultNUM) == 1)removeFaultTypeIndi[faultNUM] = 1;
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

void checkElevatorFault_IO1(void)
{
	/*assign sensor data to local varibles that are used in checking*/

	int status_alarmButtonPushed = S_alarmButtonPushed_OFF; //not used.

	//entering mutex
	int status_doorStatus = pElevatorStatus.EStatus_doorStatus;
	int status_inPosition = pElevatorStatus.EStatus_inPosition;
	int status_powerStatus = pElevatorStatus.EStatus_powerStatus;//____NOT_CAN____
	int status_havingPeople = pElevatorStatus.EStatus_havingPeople;//____NOT_CAN____
	//leaving mutex

	DEBUG("+++Checking list+++\n \
		  	powerStatus: %d,\n \
			doorStatus:%d,\n \
			inPosition:%d,\n \
			havingPeople:%d,\n" \
				, status_powerStatus \
				, status_doorStatus \
				, status_inPosition \
				, status_havingPeople);

	/*here to check if A_STOP_OUT_AREA or  A_CLOSE_IN_POSITION, or A_CLOSE_OUT_POSITIONoccurs*/
	if (status_havingPeople == 1)//having people in the elevator
	{
		//timerEnd(Tf);//stop all timers related with having no people in the cube
		//removeFault(A_STOP_OUT_AREA);

		/*********************************************/
		/*here to check if A_CLOSE_IN_POSITION occurs*/
		//if(errorCode == 0x02)
		if (status_inPosition == 0 && status_doorStatus == 0)
		{
			if (timerRead(Tb) == 0) timerStart(Tb);
			if ((timerRead(Tb)>0) && (timerCheck(Tb) >= timerGetThreshhold(Tb)))
			{
				if (timerRead(Tc) == 0) //because this just occur once
				{
					timerStart(Tc);
					DEBUG("broadcastWarningTone^\n");
					//broadcastWarningTone(1);//warning tone item No. 1
					broadcastNeeded = 2;
				}
				//else //activated once, could be omitted, like above?
				//{
				if ((timerRead(Td)>0) && (timerCheck(Td) % timerGetThreshhold(Td) == 0))
				{
					DEBUG("broadcastUrgingTone^\n");
					//broadcastUrgingTone(1);
					broadcastNeeded = 3;
					//timerEnd(Td);
				}
				//}
				if (status_alarmButtonPushed == S_alarmButtonPushed_ON) //act immediately
				{
					//timerEnd(Tc);
					if (timerRead(Td) == 0)
					{
						timerStart(Td);
						//DEBUG("broadcastWarningTone^\n");
						//broadcastUrgingTone(1);
					}
					DEBUG("Fault: A_CLOSE_IN_POSITION!\n");
					setFault(A_CLOSE_IN_POSITION);

					DEBUG("Make a call^\n");
					makeACall();//how much time will it take before it returns?
					inpositionCalled == 1;
				}
				else
				{
					if ((timerRead(Tc)>0) && (timerCheck(Tc) >= timerGetThreshhold(Tc)))
					{
						DEBUG("Fault: A_CLOSE_IN_POSITION!\n");
						setFault(A_CLOSE_IN_POSITION);

						//DEBUG("broadcastPlacatingTone^\n");
						//broadcastPlacatingTone(1);//here to skip placating tone, 'cause the urging tone will come soon.
						if (inpositionBroadcasted == 0)
						{
							broadcastNeeded = 1;
							inpositionBroadcasted = 1;
						}

						if (timerRead(Td) == 0)
						{
							timerStart(Td);
							//DEBUG("broadcastUrgingTone^\n");
							//broadcastUrgingTone(1);
						}
						//timerEnd(Tc);

						//DEBUG("Make a call^\n");
						//makeACall();  //add by JC, 20160429
						if (inpositionCalled == 0)
						{
							if (broadcastNeeded == 0)
							{
								DEBUG("Make a call^\n");
								makeACall();  //add by JC, 20160429
								inpositionCalled = 1;
							}
						}
					}
				}
				//timerEnd(Tb);
			}
		}
		else //1)reset all timers and counters; 2)reset fault alert
		{
			//if(status_doorStatus == 1)
			//{
			timerEnd(Tb);
			timerEnd(Tc);
			//timerEnd(Td);//can not be cleared, or will clear its counting in CLOSE_OUT_POSITION section, JC, 20161113
			//removeFault(A_CLOSE_IN_POSITION);
			//}
		}
		/**********************************************/
		/*here to check if A_CLOSE_OUT_POSITION occurs*/
		//if(status_inPosition == 1 && status_doorStatus == 0)   //status_inPosition = 1,·???2?, and should ignore whether door is open or not.
		//if(errorCode == 0x04 || errorCode == 0x05 || errorCode == 0x06)
		if (status_inPosition == 1)
		{
			if (timerRead(Te) == 0) timerStart(Te);
			if ((timerRead(Te)>0) && (timerCheck(Te) >= timerGetThreshhold(Te)))
			{
				DEBUG("Fault: A_CLOSE_OUT_POSITION!\n");
				setFault(A_CLOSE_OUT_POSITION);
				//DEBUG("broadcastPlacatingTone^\n");
				if (outpositionBroadcasted == 0)
				{
					broadcastNeeded = 1;
					outpositionBroadcasted = 1;
				}

				if (timerRead(Td) != 0) //activated once, same with above &&
				{
					if (timerCheck(Td) % timerGetThreshhold(Td) == 0)
					{
						DEBUG("broadcastUrgingTone^\n");
						//broadcastUrgingTone(1);
						broadcastNeeded = 3;
						//timerEnd(Td);
					}
				}

				if (status_alarmButtonPushed == S_alarmButtonPushed_ON) //act immediately
				{

					//setFault(A_CLOSE_OUT_POSITION);
					DEBUG("Make a call^\n");
					makeACall();//how much time will it take before it returns?
					outpositionCalled = 1;
				}
				else
				{
					//if((timerRead(Tc)>0)&&(ttimerCheck(Tc)>= timerGetThreshhold(Tc))
					//{
					//setFault(A_CLOSE_OUT_POSITION);
					//DEBUG("broadcastWarningTone^\n");
					//broadcastWarningTone(1);
					if (timerRead(Td) == 0)
					{
						timerStart(Td);
						////DEBUG("broadcastUrgingTone^\n"); //once at first time
						//broadcastUrgingTone(1);
						////broadcastNeeded = 3;
					}
					//DEBUG("Make a call^\n");
					//makeACall();
					if (outpositionCalled == 0)
					{
						if (broadcastNeeded == 0)//actually not needed here, 'cause broadcasting will be handled first anyway.
						{
							DEBUG("Make a call^\n");
							makeACall();
							outpositionCalled = 1;
						}
					}
					//timerEnd(Tc);
					//}
				}


			}

		}
		else
		{
			//if(status_doorStatus == 1) //door open
			//{
			timerEnd(Te);
			//removeFault(A_CLOSE_OUT_POSITION);
			//}
		}
	}

	//A_OPEN_WITHOUT_STOPPIN 	5//?a??×?3μ￡?**
	/*
	if(status_doorStatus == 1 && status_inPosition == 1 && lastInPosition == 0)//open and in position changed
	{
	setFault(A_OPEN_WITHOUT_STOPPIN);
	DEBUG("Fault: A_OPEN_WITHOUT_STOPPIN!\n");
	}
	*/
	//A_OPEN_WHILE_RUNNIN 	4//??DD?D?a??￡?**
	//if(0)setFault(A_OPEN_WHILE_RUNNIN);

	/*
	if((lastDoor == 0 && lastPosition == 1)&&(status_doorStatus == 1 && status_inPosition == 1))
	{
	setFault(A_OPEN_WHILE_RUNNIN);
	//strFaultCode = 6100;
	//xSetFault(X_OPEN_WHILE_RUNNIN);
	///DEBUG("Fault: A_OPEN_WHILE_RUNNIN!\n");
	}
	*/

	//----------------------------------------------------------------------
	//A_OPEN_WITHOUT_STOPPIN
	if(pElevatorStatus.EStatus_doorStatus == 1 && pElevatorStatus.EStatus_direction != 0)
	{
    	setFault(A_OPEN_WITHOUT_STOPPIN);
    	DEBUG("Fault: A_OPEN_WITHOUT_STOPPIN!\n");
	}

	//----------------------------------------------------------------------
	

	//----------------------------------------------------------------------


	//0:door closed; 1:door open, 0:in position, 1: not in position
	if ((lastDoor == 1 && lastPosition == 0) && (status_doorStatus == 1 && status_inPosition != 0))
	{
		setFault(A_OPEN_WITHOUT_STOPPIN);
		DEBUG("Fault: A_OPEN_WITHOUT_STOPPIN!\n");
	}

	//added by JC, 20161113
	if ((lastDoor == 0 && lastPosition == 0) && (status_doorStatus == 0 && status_inPosition != 0))
	{
		removeFault(A_OPEN_WITHOUT_STOPPIN);
	}

	/*not needed for this version, JC, 20161117
	//A_POWER_CUT 		9//í￡μ?￡?
	if(status_powerStatus == 0) //normal
	{
	//DEBUG("Power on, set LED return: %d\n",set_SStatus_emergencyLED(0));//NPN,0:NC
	set_SStatus_emergencyLED(0);
	removeFault(A_POWER_CUT);
	}else //lost main power
	{
	//DEBUG("Power off, set LED return: %d\n",set_SStatus_emergencyLED(1));//NPN,1:NO
	set_SStatus_emergencyLED(1);
	///DEBUG("Fault: A_POWER_CUT!\n");
	setFault(A_POWER_CUT);
	}
	*/
	//A_CLOSE_FAULT 	12//1???1ê??￡?**

	/*//not needed for dongzhi
	if(status_doorStatus == 1) //open
	{
	if(timerRead(Tm) == 0) timerStart(Tm);
	if((timerRead(Tm) >0)&&(timerCheck(Tm)>= timerGetThreshhold(Tm)))
	{
	///DEBUG("Fault: A_CLOSE_FAULT!\n");
	setFault(A_CLOSE_FAULT);//Case #1: long-time open
	}
	}
	else
	{
	timerEnd(Tm);
	removeFault(A_CLOSE_FAULT);
	}
	*/
	//0:door closed; 1:door open, 0:in position, 1: not in position
	//<removed in this version for donzhi, JC, 20161113>
	////if(lastPosition == 0 && status_inPosition == 0 && lastDoor == 1 && status_doorStatus == 0)//only check when in positon
	////{
	////	count_closeAndOpen++;
	////	if(count_closeAndOpen >= Cm) setFault(A_CLOSE_FAULT);//Case #2: close and open repeatedly
	////}
	//else
	//{
	//count_closeAndOpen = 0;
	//}


	/*check if fualts should be removed*/
	if (status_inPosition == 0 && status_doorStatus == 1)
	{

		removeFault(A_CLOSE_IN_POSITION);
		removeFault(A_CLOSE_OUT_POSITION);
		timerEnd(Tb);//actually not needed here, 'cause cleared above
		timerEnd(Tc);//actually not needed here, 'cause cleared above
		timerEnd(Td);
		inpositionCalled = 0;
		outpositionCalled = 0;
		inpositionBroadcasted = 0;
		outpositionBroadcasted = 0;
		//removeFault(A_OPEN_WITHOUT_STOPPIN);//moved above

	}
	/*
	if(status_inPosition == 0 && status_doorStatus == 1)
	{
	//removeFault(A_HIT_GROUND);
	//removeFault(A_HIT_CEILING);
	//removeFault(A_STOP_OUT_AREA);
	removeFault(A_CLOSE_IN_POSITION);
	timerEnd(Tb);//actually not needed here, 'cause cleared above
	timerEnd(Tc);//actually not needed here, 'cause cleared above
	timerEnd(Td);
	inpositionCalled = 0;

	//removeFault(A_OPEN_WHILE_RUNNIN);
	removeFault(A_OPEN_WITHOUT_STOPPIN);
	//removeFault(A_OVERSPEED);
	//removeFault(A_CLOSE_FAULT);
	}
	////<removed in this version for donzhi, JC, 20161113>
	////if(status_doorStatus == 0 && status_inPosition == 1) //close and move
	////{
	////	count_closeAndOpen = 0;
	////	removeFault(A_CLOSE_FAULT);//moved above
	////}

	if(status_doorStatus == 1 && status_havingPeople == 0)
	{
	removeFault(A_CLOSE_OUT_POSITION);
	//xRemoveFault(X_CLOSE_PEOPLE);

	timerEnd(Te);//actually not needed here, 'cause cleared above
	timerEnd(Td);
	outpositionCalled = 0;
	}
	*/
	//lastInPosition = status_inPosition;
	lastPosition = status_inPosition;
	lastDoor = status_doorStatus;
}


/*
 * @brief Allows to encode the provided content, leaving the output on
 * the buffer allocated by the caller.
 *
 * (门区外停梯)、[超速]、运行中开门、(冲顶、蹲底)、开门走车、(停电、平层关人、非平层关人)
 *
 * @param(void)
 *
 * @return(void)
 */

int flag_hitceiling = 0;
int flag_hitgroud = 0;

/*
 * @brief Allows to encode the provided content, leaving the output on
 * the buffer allocated by the caller.
 *
 * @param(void)
 *
 * @return(void)
 */
void checkElevatorFault(void)
{
	int counttime = 0;
	/*assign sensor data to local varibles*/
	//entering mutex
	int status_inPositionUp = pSensorsStatus.SStatus_inPositionUp;
	int status_inPositionDown = pSensorsStatus.SStatus_inPositionDown;
	int status_inPositionFlat = pSensorsStatus.SStatus_inPositionFlat;
	int status_doorOpen = pSensorsStatus.SStatus_doorOpen;
	//int status_havingPeople = pSensorsStatus.SStatus_havingPeople;
	int status_inRepairing = pSensorsStatus.SStatus_inRepairing;
	int status_alarmButtonPushed = pSensorsStatus.SStatus_alarmButtonPushed;  //pushed ==1
	int status_safeCircuit = pSensorsStatus.SStatus_safeCircuit;
	int status_runningContactor = pSensorsStatus.SStatus_runningContactor;
	int status_generalContactor = pSensorsStatus.SStatus_generalContactor;
	//leaving mutex

	//getDirection();//in a thread or timer
	//getFloorLevel();//in a thread or timer

	//pElevatorStatus.EStatus_nowStatus = 0; //default normal
	//pElevatorStatus.EStatus_powerStatus = 0;
	//pElevatorStatus.EStatus_currentFloor = BaseFloor;

	int status_powerStatus = pElevatorStatus.EStatus_powerStatus;
  	int status_direction = pElevatorStatus.EStatus_direction;
	int status_doorStatus = pElevatorStatus.EStatus_doorStatus;
	int status_inPosition = pElevatorStatus.EStatus_inPosition;
	float status_speed = pElevatorStatus.EStatus_speed;

	int status_havingPeople = pElevatorStatus.EStatus_havingPeople;

	int status_currentFloor = pElevatorStatus.EStatus_currentFloor;  //for A_HIT_CEILING

	//int status_floorLevel = pElevatorStatus.EStatus_currentFloor;

	//Is these enough for checking if a fault happens?

	//DEBUG("Info: while-check A_STOP_OUT_AREA.\n");

	DEBUG("+++Checking list+++\n \
		powerStatus: %d,\n \
		direction:%d,\n \
		speed:%.2f,\n \
		doorStatus:%d,\n \
		inPosition:%d,\n \
		inPositionBase:%d,\n \
		havingPeople:%d,\n \
		currentFloor:%d,\n \
		inRepairing:%d,\n \
		alarmButtonPushed:%d,\n \
		safeCircuit:%d,\n" \
		,status_powerStatus \
		,status_direction \
		,status_speed \
		,status_doorStatus \
		,status_inPosition \
		,status_inPositionFlat \
		,status_havingPeople \
		,status_currentFloor \
		,status_inRepairing \
		,status_alarmButtonPushed \
		,status_safeCircuit);

//now_status 0-3 当前状态[0 正常 1 故障 2 平层关人 3 非平层关人]
//direction 0-2 方向[0 停留 1 上行 2 下行]
//floor_status 0-1 平层状态[0平层 1 非平层]
//speed real 梯速
//door_status 0-3 门状态[0 关门 1 开门 2 关门中 3 开门中]
//has_people 0-1 人状态[0 无人 1 有人]
//power_status 0-2 电力[0 电源 1 电池 2 其他]
//now_floor int 当前楼层

	//if(status_currentFloor == Lbase) currentFloorRefreshed = 1;//not valid in logic
	//if(status_inRepairing == 0)
	//{
		if(currentFloorRefreshed == 1)
		{
			/*here to check if A_HIT_GROUND or A_HIT_CEILING occurs*/
			if((status_currentFloor == Lmin)&&(status_direction == 2)&&(status_inPosition == 1)) //if(status_direction == 2) has chance to miss?
			{//all 3 conditions will stay if this occurs
				flag_hitgroud = 1;
				flag_hitceiling = 0;
				//INFO("IN: current floor, derection and inpostion are: %d, %d, %d\n",status_currentFloor,status_direction,status_inPosition);
				if(timerRead(Ta) == 0) timerStart(Ta);
				//INFO("check time - ground: %d\n",timerCheck(Ta));
				if(timerCheck(Ta)>= timerGetThreshhold(Ta))
				{
					setFault(A_HIT_GROUND);
					//strFaultCode = 6300;
					//xSetFault(X_HIT_GROUND);
					INFO("set fault: hit ground!\n");

				}
				//DEBUG("Fault: A_HIT_GROUND!\n");
			}
			else
			{
				if(flag_hitceiling == 0)
				{
					timerEnd(Ta);
					flag_hitgroud = 0;
				}
			}

			if((status_currentFloor == Lmax)&&(status_direction==1)&&(status_inPosition == 1))
			{
				flag_hitceiling = 1;
				flag_hitgroud = 0;
				if(timerRead(Ta) == 0) timerStart(Ta);
				//INFO("check time - ceiling: %d\n",timerCheck(Ta));
				if(timerCheck(Ta)>= timerGetThreshhold(Ta))
				{
					setFault(A_HIT_CEILING);
					//strFaultCode = 6200;
					//xSetFault(X_HIT_CEILING);
					INFO("set fault: hit ceiling!\n");
				}
				//DEBUG("Fault: A_HIT_CEILING!\n");
			}
			else
			{
				if(flag_hitgroud == 0)
				{
					timerEnd(Ta);
					flag_hitceiling = 0;
				}
			}
		}

		/*here to check if A_STOP_OUT_AREA or  A_CLOSE_IN_POSITION, or A_CLOSE_OUT_POSITIONoccurs*/
		if(status_havingPeople == 0)//no people in the cube
		{
			//refresh, seperate before and after fault occurs, if faults occur, should not reset here.
			timerEnd(Tb);
			//timerEnd(Tc);
			//timerEnd(Td);
			timerEnd(Te);
			//inpositionCalled = 0;
			//removeFault(A_CLOSE_IN_POSITION);
			//removeFault(A_CLOSE_OUT_POSITION);

			if(status_inPosition == 1)
			{
				if(timerRead(Tf) == 0) timerStart(Tf);
				if(timerCheck(Tf)>= timerGetThreshhold(Tf))
				{
					if(getFaultState(A_HIT_GROUND)==0 && getFaultState(A_HIT_CEILING)==0)
					{
						setFault(A_STOP_OUT_AREA);
						DEBUG("Fault: A_STOP_OUT_AREA!\n");
						//strFaultCode = 6000;
						//xSetFault(X_STOP_OUT_AREA);
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
			if(status_inPosition == 0 && status_doorStatus == 0)
			{
				if(timerRead(Tb) == 0) timerStart(Tb);
				if((timerRead(Tb)>0)&&(timerCheck(Tb)>= timerGetThreshhold(Tb)))
				{
					if(timerRead(Tc) == 0) //because this just occur once
					{
						timerStart(Tc);
						//DEBUG("broadcastWarningTone^\n");
						//checkSoundPlayerAvailable();
						broadcastNeeded = 1;//broadcastWarningTone(1);//warning tone item No. 1


					}
					//else //activated once, could be omitted, like above?
					//{
					if((timerRead(Td)>0)&&(timerCheck(Td)%timerGetThreshhold(Td)==0))
					{
						//DEBUG("broadcastUrgingTone^\n");
						broadcastNeeded = 1;//broadcastUrgingTone(1);
						//timerEnd(Td);
					}
					//}
					if(status_alarmButtonPushed==S_alarmButtonPushed_ON) //act immediately
					{
						//timerEnd(Tc);
						if(timerRead(Td) == 0)
					   	{
							timerStart(Td);
							//DEBUG("broadcastWarningTone^\n");
							//broadcastUrgingTone(1);
						}
						DEBUG("Fault: A_CLOSE_IN_POSITION!\n");
						setFault(A_CLOSE_IN_POSITION);
						//strFaultCode = 9000;
						//xSetFault(X_CLOSE_PEOPLE);

						//DEBUG("Make a call^\n");
						//if(inpositionCalled == 0)
						//{
							makeACall();//how much time will it take before it returns?
							inpositionCalled = 1;
						//}
					}
					else
					{
						if((timerRead(Tc)>0)&&(timerCheck(Tc)>= timerGetThreshhold(Tc)))
						{
							DEBUG("Fault: A_CLOSE_IN_POSITION!\n");
							setFault(A_CLOSE_IN_POSITION);
							//strFaultCode = 9000;
							//xSetFault(X_CLOSE_PEOPLE);

							///DEBUG("broadcastPlacatingTone^\n");
							//broadcastPlacatingTone(1);

							if(timerRead(Td) == 0)
						   	{
								timerStart(Td);
								///DEBUG("broadcastUrgingTone^\n");
								//broadcastUrgingTone(1);
							}
							//timerEnd(Tc);

							///DEBUG("Make a call^\n");
							if(inpositionCalled == 0)
							{
								if(broadcastNeeded == 0)
								{
								 	makeACall();  //add by JC, 20160429
									inpositionCalled = 1;
								}
							}

						}
					}
				}
			}
			else //1)reset all timers and counters; 2)reset fault alert
			{
				//if(status_doorStatus == 1)
				//{
					timerEnd(Tb);
					//timerEnd(Tc);
					//timerEnd(Td);
					//inpositionCalled = 0;
					//removeFault(A_CLOSE_IN_POSITION);
				//}
			}
			/**********************************************/
			/*here to check if A_CLOSE_OUT_POSITION occurs*/
			//if(status_inPosition == 1 && status_doorStatus == 0)   //status_inPosition = 1,非平层, and should ignore whether door is open or not.
			if(status_inPosition == 1)
			{
				if(timerRead(Te) == 0) timerStart(Te);
				if((timerRead(Te)>0)&&(timerCheck(Te)>= timerGetThreshhold(Te)))
				{
					DEBUG("Fault: A_CLOSE_OUT_POSITION!\n");
					setFault(A_CLOSE_OUT_POSITION);
					//strFaultCode = 9000;
					//xSetFault(X_CLOSE_PEOPLE);

					if(timerRead(Td) != 0) //activated once, same with above &&
					{
						//counttime = timerCheck(Td)%timerGetThreshhold(Td);
						//DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
						//DEBUG("counttime is %d !!!!!!!!!!!!!!!!!!!!!!!!!!\n", counttime);
						//if(counttime == 0)
						//INFO("timerRead(Td) not 0\n");
						if((timerCheck(Td)%timerGetThreshhold(Td))==0)
						{
							///DEBUG("broadcastUrgingTone^\n");
							broadcastNeeded = 1;//broadcastUrgingTone(1);
							//timerEnd(Td);
						}
					}

					if(status_alarmButtonPushed==S_alarmButtonPushed_ON) //act immediately
					{

						//setFault(A_CLOSE_OUT_POSITION);
						//INFO("AlarmButtonPushed, make a call^\n");
						//if(outpositionCalled == 0)  //call anytime when button pushed
						//{
						 	makeACall();
							outpositionCalled = 1;
						//}
					}
					else
					{
						//if((timerRead(Tc)>0)&&(ttimerCheck(Tc)>= timerGetThreshhold(Tc))
						//{
							//setFault(A_CLOSE_OUT_POSITION);
							///DEBUG("broadcastWarningTone^\n");
							//broadcastWarningTone(1);
							if(timerRead(Td) == 0)
						   	{
								//INFO("timerRead(Td) is 0\n");
								timerStart(Td);
								broadcastNeeded = 1;//broadcastWarningTone(1);
								//DEBUG("!!!!!set broadcastNeeded !!!!!!!!!!!!!!!!!!!!\n"); //once at first time
								//broadcastUrgingTone(1);
							}
							////DEBUG("Make a call^\n");
							if(outpositionCalled == 0)
							{
								if(broadcastNeeded == 0)
								{
								 	makeACall();
									outpositionCalled = 1;
								}
							}
							//timerEnd(Tc);
						//}
					}


				}

			}
			else
			{
				//if(status_doorStatus == 1) //door open
				//{
				 	timerEnd(Te);
					//outpositionCalled = 0;  //reset, so once for every time of fault, JC 20160526
					//removeFault(A_CLOSE_OUT_POSITION);
				//}
			}
		}

		//A_OVERSPEED 		3//超速；**
		//if(0)setFault(A_OVERSPEED);

		//A_OPEN_WHILE_RUNNIN 	4//运行中开门；**
		//if(0)setFault(A_OPEN_WHILE_RUNNIN);
		if((lastDoor == 0 && lastPosition == 1)&&(status_doorStatus == 1 && status_inPosition == 1))
		{
			setFault(A_OPEN_WHILE_RUNNIN);
			//strFaultCode = 6100;
			//xSetFault(X_OPEN_WHILE_RUNNIN);
			DEBUG("Fault: A_OPEN_WHILE_RUNNIN!\n");
		}

		//A_OPEN_WITHOUT_STOPPIN 	5//开门走车；**
/*
		if(pElevatorStatus.EStatus_doorStatus == 1 && pElevatorStatus.EStatus_direction != 0)
		{
			setFault(A_OPEN_WITHOUT_STOPPIN);
			DEBUG("Fault: A_OPEN_WITHOUT_STOPPIN!\n");
		}
*/
		//0:door closed; 1:door open, 0:in position, 1: not in position
		if((lastDoor == 1 && lastPosition == 0)&&(status_doorStatus == 1 && status_inPosition != 0))
		{
			setFault(A_OPEN_WITHOUT_STOPPIN);
			DEBUG("Fault: A_OPEN_WITHOUT_STOPPIN!\n");
		}

		//A_POWER_CUT 		9//停电；
		if(status_powerStatus == 0) //normal
		{
			//DEBUG("Power on, set LED return: %d\n",set_SStatus_emergencyLED(0));//NPN,0:NC
			set_SStatus_emergencyLED(0);
			removeFault(A_POWER_CUT);
		}else //lost main power
		{
			//DEBUG("Power off, set LED return: %d\n",set_SStatus_emergencyLED(1));//NPN,1:NO
			set_SStatus_emergencyLED(1);
			DEBUG("Fault: A_POWER_CUT!\n");
			setFault(A_POWER_CUT);
		}

		//A_SAFE_LOOP_BREAK 	10//安全回路断路；
		if(pSensorsStatus.SStatus_safeCircuit == S_safeCircuit_OFF )
			setFault(A_SAFE_LOOP_BREAK); //OFF means cut off, NC, so OFF sigal = 1
		else
			removeFault(A_SAFE_LOOP_BREAK);

		//A_OPEN_FAULT 		11//开门故障；
		if(0)setFault(A_OPEN_FAULT);

		//A_CLOSE_FAULT 	12//关门故障；**
		if(status_doorStatus == 1) //open
		{
			if(timerRead(Tm) == 0) timerStart(Tm);
			if((timerRead(Tm) >0)&&(timerCheck(Tm)>= timerGetThreshhold(Tm)))
			{
				DEBUG("Fault: A_CLOSE_FAULT!\n");
				setFault(A_CLOSE_FAULT);
			}
		}
		else
		{
			timerEnd(Tm);
			//removeFault(A_CLOSE_FAULT);
		}

		//0:door closed; 1:door open, 0:in position, 1: not in position

		if(lastPosition == 0 && status_inPosition == 0 && lastDoor == 1 && status_doorStatus == 0)//only check when in positon
		{
			count_closeAndOpen++;
			if(count_closeAndOpen >= Cm) setFault(A_CLOSE_FAULT);
		}
		else
		{
			count_closeAndOpen = 0;
		}

		//A_OVERSPEED 		3//超速；**
		///DEBUG("speed threshhold is%f\n",(((float)Sk)/10)*(((float)Sn)/10));

		if(status_speed <=(((float)Sk)/10)*(((float)Sn)/10))
		{
			count_normalSpeedFloor = status_currentFloor;
			if(status_speed >= 0.9*((float)Sk)/10)
			{

				if(lastNormalSpeed == 0)
				{
					lastNormalSpeed = 1; //mark NormalSpeed for the first time
				}
				else
				{
					removeFault(A_OVERSPEED); //it was NormalSpeed last time
					//xRemoveFault(X_OVERSPEED);
					lastNormalSpeed = 0;
				}
				/*
				if(count_normalSpeed = 0)
				count_normalSpeed++
				if(count_normalSpeed = 2)

				count_normalSpeed
				*/
			}
			else
				lastNormalSpeed = 0;
		}
		else
		{
			count_overSpeedFloor = status_currentFloor;
			lastNormalSpeed = 0;
		}
		if(count_overSpeedFloor != 0)//'cause there is no floor 0
	 	{
			if((count_overSpeedFloor - count_normalSpeedFloor >= Sl) ||(count_normalSpeedFloor - count_overSpeedFloor >= Sl))
			{
				count_normalSpeedFloor = 0;
				count_overSpeedFloor = 0;
				setFault(A_OVERSPEED);
				//strFaultCode = 6400;
				//xSetFault(X_OVERSPEED);
			}
		}


		//A_SPEED_ANOMALY 	15//电梯速度异常；
		if(0)setFault(A_SPEED_ANOMALY);

		//A_OVERSPEED_1_2 	16//超速1.2倍；
		if(0)setFault(A_OVERSPEED_1_2);

		//A_OVERSPEED_1_4 	17//超速1.4倍；
		if(0)setFault(A_OVERSPEED_1_4);

		/*check if fualts should be removed*/
		if(status_inPosition == 0 && status_doorStatus == 1)
		{
			removeFault(A_HIT_GROUND);
			//xRemoveFault(X_HIT_GROUND);

			removeFault(A_HIT_CEILING);
			//xRemoveFault(X_HIT_CEILING);

			removeFault(A_STOP_OUT_AREA);
			//xRemoveFault(X_STOP_OUT_AREA);
			{
				removeFault(A_CLOSE_IN_POSITION);
				timerEnd(Tb);
				timerEnd(Tc);
				timerEnd(Td);
				inpositionCalled = 0;
			}
			removeFault(A_OPEN_WHILE_RUNNIN);
			//xRemoveFault(X_OPEN_WHILE_RUNNIN);

			removeFault(A_OPEN_WITHOUT_STOPPIN);
			//removeFault(A_OVERSPEED);
			//removeFault(A_CLOSE_FAULT);
		}
		if(status_doorStatus == 0 && status_inPosition == 1) //close and move
		{
			removeFault(A_CLOSE_FAULT);
		}

		if(status_doorStatus == 1 && status_havingPeople == 0)
		{
			removeFault(A_CLOSE_OUT_POSITION);
			//xRemoveFault(X_CLOSE_PEOPLE);

			timerEnd(Te);
			timerEnd(Td);
			outpositionCalled = 0;
		}
/*
		if(status_powerStatus == 0)
		{
			removeFault(A_POWER_CUT);
 		}
*/

	lastPosition = status_inPosition;
	lastDoor = status_doorStatus;

    //}//status_inRepairing

    //nopoll_sleep(1000000);
}


int httpGet_ServerTime(void)
{
	JSON_Value *j_Value;
	JSON_Array *j_Array;
	JSON_Object *j_Object;


	DEBUG("httpGet_ServerTime finished^\n");

	return 0;
}

#define CMD_LEN 1024
#define CMD_RECV 1024

char socketCmdBuf[CMD_LEN];
char socketCmdRecvBuf[CMD_RECV];

int httpPost_DeviceRegister(void)
{
	int Count_MaxNoResponse = 10;
	time_t t;
	Lift_return_t ret=0;

	char *sign;
	char *signSrc;
	char md[16]={0};
	int signSrcLen = 0;

	memset(socketCmdBuf,0,CMD_LEN);
	memset(socketCmdRecvBuf,0,CMD_RECV);
	sprintf(socketCmdBuf, "#10#{\"type\":\"login\",\"eid\":\"mx12345678\"}#13#");
	ret = cat1_send(socketCmdBuf, strlen(socketCmdBuf));
	//ret = cat1_send(socketCmdBuf, strlen(socketCmdBuf), recvbuf, recvlen);


	DEBUG("httpPost_DeviceRegister finished. ret=%d. buf=%s\n", ret, socketCmdRecvBuf);

	return 0;
}

int httpPost_HeartBeat(void)//when to get uid? in httpPost_DeviceRegister()
{

	return 0;
}


int httpPost_FaultAlert(int int_alertType)
{
	time_t t;

		return 0;
}

/*get-set functions*/
/*
* @brief Allows to encode the provided content, leaving the output on
* the buffer allocated by the caller.
*
* @param(void)
*
* @return(void)
*/
int httpPost_FaultRemove(int int_alertType)
{
	time_t t;


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
			if (faultNumbers == A_CLOSE_IN_POSITION) pElevatorStatus.EStatus_nowStatus = 2;
			else if (faultNumbers == A_CLOSE_OUT_POSITION) pElevatorStatus.EStatus_nowStatus = 3;
			else pElevatorStatus.EStatus_nowStatus = 1;
			//pElevatorStatus.EStatus_nowStatus = 1;
			if (getFaultSent(faultNumbers) == 0)
			{
				//if(faultNumbers != 12)
				//{
				if (httpPost_FaultAlert(faultNumbers) == 0)
				{
					setFaultSent(faultNumbers); //set only send success, or getFaultSent(faultNumbers) is still 0
				}
				else
					INFO("httpPost_FaultAlert failed!!\n");
				currentFaultNumber = faultNumbers;//?? is it needed to put into above?
				//}
			}
			//DEBUG("Info: ElevatorFault No. %d occurs!\n",faultNumbers);
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

			//DEBUG("Info: ElevatorFault No. %d has been removed^\n",faultNumbers);
		}
		faultNumbers--;
	}
	if (count == 0)
	{
		set_SStatus_LED(S_LED1_FAULT, 1); //black out
		pElevatorStatus.EStatus_nowStatus = 0;
		currentFaultNumber = 0;
	}
	else
		set_SStatus_LED(S_LED1_FAULT, 0); //light the led
}

/*NEW get-functions of Sensors Status, by signals*/

int sensor_fd;

void my_signal_fun(int signum)
{
	unsigned char sensor_val='A';
	int timeIntervalUsec = 0;
	
	INFO("sensor_val: 0x%2x\n", sensor_val);//0x3,0x4

	if ((sensor_val & 0x80) == 0)//input "0": OFF, for example: 0x0A
	{
		switch (sensor_val)
		{
		case 0x01:
			pSensorsStatus.SStatus_doorOpen = 0;
			break;
		case 0x02:
			pSensorsStatus.SStatus_inPositionFlat = 0;//just for base, and sometimes there is no base
			break;
		case 0x03:
			pSensorsStatus.SStatus_inPositionDown = 0;
			if (pSensorsStatus.SStatus_inPositionUp == 1)
			{
				pElevatorStatus.EStatus_direction = 2;//0:stop, 1:up, 2:down
			}
			else
			{
				pElevatorStatus.EStatus_direction = 1;
			}
			break;
		case 0x04:
			pSensorsStatus.SStatus_inPositionUp = 0;
			gettimeofday(&tv_speed2, NULL);
			if ((timeIntervalUsec = tv_cmp(tv_speed2, tv_speed1)) > 0)
			{
				pElevatorStatus.EStatus_speed = TIME_UNIT * UpDownDistance / timeIntervalUsec;//10^6-->10^5
				//DEBUG("**** speed is %f ****\n",pElevatorStatus.EStatus_speed);
			}
			break;
		case 0x05:
			pSensorsStatus.SStatus_safeCircuit = 0;
			break;
		case 0x06:
			pSensorsStatus.SStatus_runningContactor = 0;
			break;
		case 0x07:
			pSensorsStatus.SStatus_alarmButtonPushed = 0;
			break;
		case 0x08:
			pSensorsStatus.SStatus_inRepairing = 0;
			break;
		case 0x09:
			pSensorsStatus.SStatus_generalContactor = 0;
			break;
		case 0x0A:
			pSensorsStatus.SStatus_havingPeople = 0;
			INFO("SStatus_havingPeople = 0\n");
			break;
		default:
			break;
		}
	}
	else//input "1":ON, for example: 0x8A
	{
		switch (sensor_val & 0x0F)
		{
		case 0x01:
			pSensorsStatus.SStatus_doorOpen = 1;
			break;
		case 0x02:
			pSensorsStatus.SStatus_inPositionFlat = 1;
			pElevatorStatus.EStatus_currentFloor = Lbase;
			currentFloorRefreshed = 1;
			break;
		case 0x03:
			pSensorsStatus.SStatus_inPositionDown = 1;
			if (pSensorsStatus.SStatus_inPositionUp == 1)
			{
				pElevatorStatus.EStatus_direction = 1;//0:stop, 1:up, 2:down
			}
			else
			{
				pElevatorStatus.EStatus_direction = 2;
				if (pElevatorStatus.EStatus_currentFloor > Lmin) pElevatorStatus.EStatus_currentFloor--;
				if (pElevatorStatus.EStatus_currentFloor == 0) pElevatorStatus.EStatus_currentFloor = -1;
			}
			break;
		case 0x04:
			pSensorsStatus.SStatus_inPositionUp = 1;
			gettimeofday(&tv_speed1, NULL);//get or set?, of course get, set clear the timer and make timerCheck(Tk)=0
			if (pSensorsStatus.SStatus_inPositionDown == 1)
			{
				//pElevatorStatus.EStatus_direction = 1;//0:stop, 1:up, 2:down
			}
			else
			{
				//pElevatorStatus.EStatus_direction = 2;
				if (pElevatorStatus.EStatus_currentFloor < Lmax) pElevatorStatus.EStatus_currentFloor++;
				if (pElevatorStatus.EStatus_currentFloor == 0) pElevatorStatus.EStatus_currentFloor = 1;
			}
			break;
		case 0x05:
			pSensorsStatus.SStatus_safeCircuit = 1;
			break;
		case 0x06:
			pSensorsStatus.SStatus_runningContactor = 1;
			break;
		case 0x07:
			pSensorsStatus.SStatus_alarmButtonPushed = 1;
			break;
		case 0x08:
			pSensorsStatus.SStatus_inRepairing = 1;
			break;
		case 0x09:
			pSensorsStatus.SStatus_generalContactor = 1;
			break;
		case 0x0A:
			pSensorsStatus.SStatus_havingPeople = 1;
			INFO("SStatus_havingPeople = 1\n");
			break;
		default:
			break;

		}
	}
}

void getSensorsStatus(){
    static int i=0;
    int j=0, k=0;
	int timeIntervalUsec = 0;

    memset(UartBuf, 0, SOCKETBUFSIZE);

#if 1 //def WIN_PLATFORM

    if(T02[i][0] != 0x00){
        memcpy(UartBuf, T01[i], TITEMLEN);
        i++;
    }else{
        i=0;
        INFO("=============================================================\n");
    }
	
#else

	send_recv_lift(UartBuf);

#endif 

	printf("******UART: ");
	for (k = 0; k<22; k++)
	{
		printf(" %02x", UartBuf[k]);
	}
	printf("******\r\n");



    if(UartBuf[0]!=0x7B || UartBuf[TITEMLEN-1]!=0x7D){
        return;
    }
/*    
00: 代表第6输入口的状态变化，建议接极限开关
00: 代表第5输入口的状态变化，建议接人感传感器
00: 代表第4输入口的状态变化，建议接门开关传感器
00: 代表第3输入口的状态变化，建议接基站校验传感器
00: 代表第2输入口的状态变化，建议接下行传感器
00: 代表第1输入口的状态变化，建议上行传感器
*/

    for(j=TITEMLEN-2;j>6;j--){
        switch (j)
    	{
    	case 20: //00: 代表第1输入口的状态变化，建议上行传感器
            pSensorsStatus.SStatus_inPositionUp = UartBuf[j];
            if(pSensorsStatus.SStatus_inPositionUp==0){
    			pSensorsStatus.SStatus_inPositionUp = 0;
    			gettimeofday(&tv_speed2, NULL);
    			if ((timeIntervalUsec = tv_cmp(tv_speed2, tv_speed1)) > 0)
    			{
    				pElevatorStatus.EStatus_speed = TIME_UNIT * UpDownDistance / timeIntervalUsec;//10^6-->10^5
    				//DEBUG("**** speed is %f ****\n",pElevatorStatus.EStatus_speed);
    			}

            }else{
    			pSensorsStatus.SStatus_inPositionUp = 1;
    			gettimeofday(&tv_speed1, NULL);//get or set?, of course get, set clear the timer and make timerCheck(Tk)=0
    			if (pSensorsStatus.SStatus_inPositionDown == 1)
    			{
    				//pElevatorStatus.EStatus_direction = 1;//0:stop, 1:up, 2:down
    			}
    			else
    			{
    				//pElevatorStatus.EStatus_direction = 2;
    				if (pElevatorStatus.EStatus_currentFloor < Lmax) pElevatorStatus.EStatus_currentFloor++;
    				if (pElevatorStatus.EStatus_currentFloor == 0) pElevatorStatus.EStatus_currentFloor = 1;
    			}
            }
            //pElevatorStatus.EStatus_direction = 1;//0:stop, 1:up, 2:down
    		break;
    	case 19: //00: 代表第2输入口的状态变化，建议接下行传感器
            pSensorsStatus.SStatus_inPositionDown = UartBuf[j];

            if(pSensorsStatus.SStatus_inPositionDown==0){
            	if (pSensorsStatus.SStatus_inPositionUp == 1)
    			{
    				pElevatorStatus.EStatus_direction = 2;//0:stop, 1:up, 2:down
    			}
    			else
    			{
    				pElevatorStatus.EStatus_direction = 1;
    			}
            }else{

    			pSensorsStatus.SStatus_inPositionDown = 1;
    			if (pSensorsStatus.SStatus_inPositionUp == 1)
    			{
    				pElevatorStatus.EStatus_direction = 1;//0:stop, 1:up, 2:down
    			}
    			else
    			{
    				pElevatorStatus.EStatus_direction = 2;
    				if (pElevatorStatus.EStatus_currentFloor > Lmin) pElevatorStatus.EStatus_currentFloor--;
    				if (pElevatorStatus.EStatus_currentFloor == 0) pElevatorStatus.EStatus_currentFloor = -1;
    			}
            }
            //pElevatorStatus.EStatus_direction = 2;//0:stop, 1:up, 2:down
    		break;
    	case 18: //00: 代表第3输入口的状态变化，建议接基站校验传感器
    		pSensorsStatus.SStatus_baseStation = UartBuf[j];
    		break;
    	case 17: //00: 代表第4输入口的状态变化，建议接门开关传感器
    		pSensorsStatus.SStatus_doorOpen = UartBuf[j];
    		break;
    	case 16: //00: 代表第5输入口的状态变化，建议接人感传感器
    		pSensorsStatus.SStatus_havingPeople = UartBuf[j];
    		break;
    	case 15: //00: 代表第6输入口的状态变化，建议接极限开关
    		pSensorsStatus.SStatus_limitSwitch = UartBuf[j];
    		break;
    	default:
    		break;
    	}
    }


	pElevatorStatus.EStatus_powerStatus = get_EStatus_powerStatus();
	pElevatorStatus.EStatus_havingPeople = get_EStatus_havingPeople();
    pElevatorStatus.EStatus_doorStatus = get_EStatus_doorStatus();
    pElevatorStatus.EStatus_inPosition = get_EStatus_inPosition();

    return;
}

//=================================================================


int CheckLiftStatus(void){
 	int count=0;

	INFO("CheckLiftStatus start\n");
	while (1)
	{
		count++;

		INFO("**[%d]**\n", count);
		
		/*refresh status of all sensors*/
		refreshStatus();

		/*check faults when elevator not in a repairing*/

		//sem_p(sem_d);
		//checkElevatorFault_IO1();
		checkElevatorFault();
		handleElevatorFault();
		//sem_v(sem_d);

		/*check every second*/
		LIFT_SLEEP_MS(500);
	}
	return 0;    
}

//=============================================================
#ifdef WIN_PLATFORM

HANDLE event;
HANDLE mutex;
//unsigned int __stdcall thread_6_checkStatus(void *unused)
DWORD WINAPI thread_6_checkStatus(LPVOID pM)
{
	INFO("thread_6_checkStatus start^\n");
    CheckLiftStatus();
	return 1;
}

DWORD WINAPI thread_7_in_main(LPVOID pM)
{
	int i, count=0;
	char SocketRxBuf[SOCKETBUFSIZE];
	size_t SocketRxlen=0;        //tcp?óêüμ?μ?êy?Y3¤?è
	int fd_io = NULL;
	srand((unsigned)time(0));
	while (1)
	{
		memset(SocketRxBuf, 0, strlen(SocketRxBuf));
		int idx = rand() % 64;
		SocketRxlen = TITEMLEN;
		memcpy(SocketRxBuf, TDATA[idx], TITEMLEN);
	
		DEBUG("[%d] buf_idx=[%d]", count++, idx);

		if (process_server1(&fd_io, SocketRxBuf, SocketRxlen) != 0){

		}

		LIFT_SLEEP_MS(3000);//try select in seconds accepted.
	}
	return 0;
}


int _tmain(int argc, _TCHAR* argv[])
{
	//mutex = CreateMutex(NULL, FALSE, _T("MUTEX"));
	//event = CreateEvent(NULL, FALSE, TRUE, _T("EVNET1"));
	//_beginthreadex(NULL, 0, thread_6_checkStatus, NULL, 0, NULL);
	HANDLE handle = CreateThread(NULL, 0, thread_6_checkStatus, NULL, 0, NULL);
	//WaitForSingleObject(handle, INFINITE);

	//HANDLE handle2 = CreateThread(NULL, 0, thread_7_in_main, NULL, 0, NULL);
	WaitForSingleObject(handle, INFINITE);
	//WaitForSingleObject(handle2, INFINITE);
	return 0;
}


#else

void liftCheckSensorStreamData(xTimerHandle pvParameters)
{

}



void liftCheckTask(void *pvParameters)
{
	int i, count=0;
	size_t SocketRxlen;        //tcp接受到的数据长度
	int fd_io = NULL;
	int ret = 0;

	DEBUG("liftCheckTask\r\n");

	ret = Lift_DriverInit();
	if(ret == Lift_STATUS_SUCCESS) {
		DEBUG("LIFT USart_2 initialization SUCCESSFUL\r\n");
	} else {
		DEBUG("LIFT USart_2 initialization FAILED\r\n");
	}

	// TODO: 向平台注册本设备
	//LIFT_SLEEP_MS(7000);
	//httpPost_DeviceRegister();

	//从io板串口读数据并处理

    CheckLiftStatus();
	
	return;
	
#if 0
	char SocketRxBuf[SOCKETBUFSIZE];
	uint8_t cmdRecv[1024];
	char buf[1024];

	while (1)
	{
		count++;
		memset(cmdRecv,0,1024);
		memset(buf,0,1024);
		//sprintf(buf, "abcde-%d-\r\n", count);
		//send_recv_lift(cmdRecv);

		// TODO: 注意串口数据格式, 16进制还是ascii
		//send_recv_cmd_lift(buf , cmdRecv, 1000);
		//DEBUG("[%d] uart=%s\n", count, cmdRecv);


		send_recv_lift(buf);
		printf(">>>IO uart[%d] ", count);
		for (i = 0; i<22; i++)
		{
			printf(" %02x", buf[i]);
		}
		printf("----\r\n");


		LIFT_SLEEP_MS(300);
		memset(SocketRxBuf, 0, strlen(SocketRxBuf));
		int idx = rand() % 64;
		SocketRxlen = TITEMLEN;
		memcpy(SocketRxBuf, TDATA[idx], TITEMLEN);
	
		DEBUG("[%d] buf_idx=[%d]", count++, idx);

		if (process_server1(&fd_io, SocketRxBuf, SocketRxlen) != 0){

		}

		/*refresh status of all sensors*/
		refreshStatus();

		/*check faults when elevator not in a repairing*/

		if (pIOData1.IOData1_inRepairing == 0)
		{
			set_SStatus_LED(S_LED3_REPAIRING, 1);
			//sem_p(sem_d);
			checkElevatorFault_IO1();
			handleElevatorFault();
			//sem_v(sem_d);
		}
		else
		{
			set_SStatus_LED(S_LED3_REPAIRING, 0);
			DEBUG("Elevator is in repairing, faults will not be checked^\n");
		}
		
		LIFT_SLEEP_MS(1000);
	}
	return;
#endif

}



//=======================================================


#endif

