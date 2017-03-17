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










