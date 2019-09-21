/*
 * File      : qsdk_callback.c
 * This file is part of callback in qsdk
 * Copyright (c) 2018-2030, longmain Development Team
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-11-14     longmain     first version
 * 2018-12-13     longmain     update fun
 * 2019-06-13     longmain     add net close callback
 */

#include "qsdk.h"
#include "stdio.h"
#include "stdlib.h"

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME              "[QSDK/CALLBACK]"
#ifdef QSDK_USING_LOG
#define DBG_LEVEL                      DBG_LOG
#else
#define DBG_LEVEL                      DBG_INFO
#endif /* QSDK_DEBUG */

#include <rtdbg.h>


/****************************************************
* 函数名称： qsdk_rtc_set_time_callback
*
* 函数作用： RTC设置回调函数
*
* 入口参数： year：年份		month: 月份		day: 日期
*
*							hour: 小时		min: 分钟		sec: 秒		week: 星期
*
* 返回值： 0 处理成功	1 处理失败
*****************************************************/
void qsdk_rtc_set_time_callback(int year,char month,char day,char hour,char min,char sec,char week)
{ 
#ifdef RT_USING_RTC
	set_date(year,month,day);
	set_time(hour,min,sec);
#endif
}

/*************************************************************
*	函数名称：	qsdk_net_close_callback
*
*	函数功能：	TCP异常断开回调函数
*
*	入口参数：	无
*
*	返回参数：	无
*
*	说明：		
*************************************************************/
void qsdk_net_close_callback(void)
{
	LOG_E("now the network is abnormally disconnected\r\n");
	
}
/****************************************************
* 函数名称： qsdk_net_data_callback
*
* 函数作用： TCP/UDP 服务器下发数据回调函数
*
* 入口参数：	data: 数据首地址
*
*							len: 数据长度
*
* 返回值： 0 处理成功	1 处理失败
*****************************************************/
int qsdk_net_data_callback(char *data,int len)
{
	LOG_I("enter net callback,udp client rev data=%d,%s\r\n",len,data);
	return RT_EOK;
}

/****************************************************
* 函数名称： qsdk_iot_data_callback
*
* 函数作用： IOT平台下发数据回调函数
*
* 入口参数： data：下发数据首地址
*
*							len	:	下发数据长度
*
* 返回值： 0 处理成功	1 处理失败
*****************************************************/
int qsdk_iot_data_callback(char *data,int len)
{
	LOG_I("enter iot callback,rev data=%d,%s\r\n",len,data);
	return RT_EOK;
}	
/****************************************************
* 函数名称： qsdk_onenet_close_callback
*
* 函数作用： onenet平台强制断开连接回调函数
*
* 入口参数： 无
*
* 返回值： 0 处理成功	1 处理失败
*****************************************************/
int qsdk_onenet_close_callback()
{
	LOG_I("enter close onenent callback\r\n");

	return RT_EOK;
}
/****************************************************
* 函数名称： qsdk_onenet_read_rsp_callback
*
* 函数作用： onenet平台 read操作回调函数
*
* 入口参数： msgid：消息ID	insid：instance id	resid: resource id
*
* 返回值： 0 处理成功	1 处理失败
*****************************************************/
int qsdk_onenet_read_rsp_callback(int msgid,int insid,int resid)
{
	LOG_I("enter read dsp callback\r\n");

	return RT_EOK;
}
/****************************************************
* 函数名称： qsdk_onenet_write_rsp_callback
*
* 函数作用： onenet平台 write操作回调函数
*
* 入口参数： len:	需要写入的数据长度
*
*						 value:	需要写入的数据内容
*
* 返回值： 0 处理成功	1 处理失败
*****************************************************/
int qsdk_onenet_write_rsp_callback(int len,char* value)
{
	LOG_I("enter write dsp callback\r\n");	
	return RT_EOK;
}
/****************************************************
* 函数名称： qsdk_onenet_exec_rsp_callback
*
* 函数作用： onenet平台 exec操作回调函数
*
* 入口参数： len:	平台exec命令下发数据长度
*
*						 cmd:	平台exec命令下发数据内容
*
* 返回值： 0 处理成功	1 处理失败
*****************************************************/
int qsdk_onenet_exec_rsp_callback(int len,char* cmd)
{
	LOG_I("enter exec dsp callback,exec data len:%d   data=%s\r\n",len,cmd);
	return RT_EOK;
}



/****************************************************
* 函数名称： qsdk_onenet_fota_callback
*
* 函数作用： onenet 平台FOTA升级回调函数
*
* 入口参数： 无
*
* 返回值： 	 无
*****************************************************/
void qsdk_onenet_fota_callback(void)
{
	LOG_I("enter fota callback\r\n");
}

/****************************************************
* 函数名称： qsdk_mqtt_data_callback
*
* 函数作用： MQTT 服务器下发数据回调函数
*
* 入口参数：topic：主题    mesg：平台下发消息    mesg_len：下发消息长度
*
* 返回值： 	0 处理成功	1 处理失败
*****************************************************/


int qsdk_mqtt_data_callback(char *topic,char *mesg,int mesg_len)
{
	LOG_I("enter mqtt callback  mesg:%s,len:%d\r\n",mesg,mesg_len);

	return RT_EOK;
}

/****************************************************
* 函数名称： qsdk_gps_data_callback
*
* 函数作用： GPS定位成功回调函数
*
* 入口参数：  lon：经度    lat:纬度    speed：速度
*
* 返回值： 	0 处理成功	1 处理失败
*****************************************************/


int qsdk_gps_data_callback(char *lon,char *lat,float speed)
{
	LOG_I("enter gps callback  lon:%s,lat:%s,speed:%f\r\n",lon,lat,speed);

	return RT_EOK;
}
/****************************************************
* 函数名称： qsdk_nb_reboot_callback
*
* 函数作用： nb-iot模组意外复位回调函数
*
* 入口参数：无
*
* 返回值：  无
*****************************************************/
void qsdk_nb_reboot_callback(void)
{
	LOG_I("enter reboot callback\r\n");

}





