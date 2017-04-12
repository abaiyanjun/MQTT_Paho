# MQTT_Paho

1. 串口读取速度, 要看电梯运行速度, 同时需要测试串口反应时间. 否则会丢失状态数据. 尤其上下行的良传感器间隔只有5cm. 
2. 和上传服务器接口, 对方说是使用websocket 协议, +json数据包格式. 我们没有实现. 

 
821668758 

http://www.kancloud.cn/lingeasy/srioprotocol?token=8tJb2y3HXaBA
ヾ遺莣濄呿℡  15:18:06
websocket.elevatorcloud.com 25550
websocket.elevatorcloud.com 25500



ws://websocket.elevatorcloud.com:25550


 
114.55.42.28:25500
#10#{"type":"login","eid":"mx12345678"}#13#
233130237B2274797065223A226C6F67696E222C22656964223A226D783132333435363738227D23313323

#10#{"type":"login","eid":"mx12345678"}#13#

AT^SISH=0,#10#{"type":"login","eid":"mx12345678"}#13#
101400064D51497364700302006400067075625F7631

地址: 114.55.42.28:25500
发送字符串:  #10#{"type":"login","eid":"mx12345678"}#13#
应该收到:    #10#{"type":"login","res_id":1}#13#
函数原型:
sendbuf: 发送数据buf, 
sendlen: 发送数据长度, 
recvbuf: sockent返回的buf, 
*recvlen: 返回的数据长度, 
返回值: -1: 发送错误
        其他值: 发送的字节数
int send_to_socket(char* sendbuf, int sendlen, char* recvbuf, int* recvlen);


3.17
1. 电梯故障逻辑在1.105 server上还有代码, 我们这里没有. 
2. 更新我的cube模块, 


1. 串口读取速度, 要看电梯运行速度, 同时需要测试串口反应时间. 否则会丢失状态数据. 尤其上下行的良传感器间隔只有5cm. 
2. 和上传服务器接口, 对方说是使用websocket 协议, +json数据包格式. 我们没有实现. 

 
821668758 

http://www.kancloud.cn/lingeasy/srioprotocol?token=8tJb2y3HXaBA
ヾ遺莣濄呿℡  15:18:06
websocket.elevatorcloud.com 25550
websocket.elevatorcloud.com 25500



ws://websocket.elevatorcloud.com:25550


 
114.55.42.28:25500
#10#{"type":"login","eid":"mx12345678"}#13#
233130237B2274797065223A226C6F67696E222C22656964223A226D783132333435363738227D23313323

#10#{"type":"login","eid":"mx12345678"}#13#

AT^SISH=0,#10#{"type":"login","eid":"mx12345678"}#13#
101400064D51497364700302006400067075625F7631


地址: 114.55.42.28:25500

地址: 114.55.42.28:26500
发送字符串:  #10#{"type":"login","eid":"mx12345678"}#13#
应该收到:    #10#{"type":"login","res_id":1}#13#
函数原型:
sendbuf: 发送数据buf, 
sendlen: 发送数据长度, 
recvbuf: sockent返回的buf, 
*recvlen: 返回的数据长度, 
返回值: -1: 发送错误
        其他值: 发送的字节数
int send_to_socket(char* sendbuf, int sendlen, char* recvbuf, int* recvlen);


3.17
1. 电梯故障逻辑在1.105 server上还有代码, 我们这里没有. 
2. 更新我的cube模块, 


#10#{"type":"need_login"}#13#




ヾ遺莣濄呿℡ 2017/3/13 14:43:33
    /**
     * Check the integrity of the package.
     *
     * @param string        $buffer
     * @param TcpConnection $connection
     * @return int
     */
    public static function input($buffer, TcpConnection $connection)
    {
        // Judge whether the package length exceeds the limit.
        if (strlen($buffer) >= TcpConnection::$maxPackageSize) {
            $connection->close();
            return 0;
        }
        //  Find the position of  "\n".
        $pos = strpos($buffer, "#13#");
        // No "\n", packet length is unknown, continue to wait for the data so return 0.
        if ($pos === false) {
            return 0;
        }
        // Return the current package length.
        return $pos + 4;
    }

    /**
     * Encode.
     *
     * @param string $buffer
     * @return string
     */
    public static function encode($buffer)
    {
        // Add "\n"
        return "#10#" . $buffer . "#13#";
    }

    /**
     * Decode.
     *
     * @param string $buffer
     * @return string
     */
    public static function decode($buffer)
    {
		$pos = strpos($buffer, "#10#");
		$buffer2=substr($buffer,$pos+4);
        // Remove "\n"
        return trim(substr_replace($buffer2,'',-4));
    }
阿白 2017/3/13 14:43:59
3ks. 有问题再请教. 
阿白 2017/3/22 13:40:49
请教一下, 上报故障, 是什么格式, 之前文档里面没有看到. 



数据格式
数据格式
服务端 -> 客户端
心跳包
传输内容：{"type":"ping"}
需要返回：{"type":"ping"}
协议作用：当服务端一段时间未收到客户端消息时，会向客户端发送心跳请求，客户端收到此消息后应向服务端返回
信息。超出一定次数客户端无消息返回的情况下，服务端会自动断开与客户端的连接。
要求登陆
传输内容：{"type":"need_login"}
需要返回：[客户端->服务端协议：登陆请求]
协议作用：当客户端首次连接成功后，服务端会向客户端发送登陆请求，客户端收到此消息后应向服务端发送登陆请求
信息，用于识别客户端。如果未向服务端发送登陆请求，则服务端会自动踢出客户端。
强制踢出客户端
传输内容：{"type":"kick","code":踢出原因代码}
需要返回：无
协议作用：当服务端收到非法数据信息时，会将客户端踢出。并且在踢出前发送通知，告知客户端被踢出原因。
代码说明
1 当同一个设备ID有其他设备登陆时，会收到此信息，意味着有相同设备登陆，自身被踢出
2 服务端收到了错误的客户端数据，认为为非法请求，将客户端踢出。
客户端 -> 服务端
心跳包
传输内容：{"type":"cping"}
需要返回：{"type":"cping"}
协议作用：当客户端一段时间未收到服务端消息时，需要向服务端发送心跳请求。服务端收到此消息会向客户端返回
信息。如果客户端发送请求后一段时间内收不到任何信息，则需要判定自身已经离线，需要进行断线重连操作。
登陆请求
传输内容：{"type":"login","eid":"设备编号"}
返回内容：{"type":"login","res_id":返回代码}
协议作用：客户端需要先登陆注册当前设备编号，服务端才可正确识别客户端链接。需要在收到服务端要求登陆请求后
才可发送。
返回代码：
代码说明
0 登陆成功，信息已经注册到服务端。
数据格式
本文档使用 看云 构建 - 8 -
1 登陆失败，不存在此设备。
2 登陆失败，未知错误。
发送信息
传输内容：
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
返回内容：无
协议作用：客户端将收到的数据代码传递给服务端，服务端根据数据代码进行数据解析。
协议前提：完成登陆请求，并且获得登陆成功通知。
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
数据格式
本文档使用 看云 构建 - 9 -

hy_xylw 密码111111，


121.196.219.49 26500


863867025992437


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

#endif


以下问题为今天测出的BUG，需要白艳君查看修改
case1 门区外停梯故障告警：定时器30s时间短了，最少需要45s；
case2 运行中开门故障告警：希望加些30s延时，电梯本身会有一些问题，比如在安装的时候门开关安装不到位，容易产生误报；
case3 开门走梯故障告警:同上；加延时主要是为了防止误报；
case4 平层困人故障：逻辑部分需要更新；
	故障步骤：1、有人，2、关门，3、平层，4、关门后的时间；
	解除步骤：1、平层，2、开门；
case5 非平层困人故障：同时；（故障步骤3、非平层）

人感工作情况：人感传感器在感应到有人的时候，会有人无人反复跳转；在感应不到人的时候，会一直保持为无人状态；

人感出现的BUG：传感器在检测到有人的时候，在反复有人无人的跳转时间小于延时的10s的，会出现传感器为有人，页面显示为无人，过会页面变成有人状态。

人感部分逻辑要注意：人在电梯中不能凭空出现和消失。
以上是 卞宏宇 给出的部分建议

谢谢！










