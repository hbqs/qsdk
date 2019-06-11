/*
 * File      : qsdk_callback.c
 * This file is part of callback in qsdk
 * Copyright (c) 2018-2030, longmain Development Team
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-11-14     longmain     first version
 * 2018-12-13     longmain     update fun
 */

#include "qsdk.h"
#include "stdio.h"
#include "stdlib.h"

#define LOG_TAG              "[QSDK/CALLBACK]"
#ifdef QSDK_USING_LOG
#define LOG_LVL              LOG_LVL_DBG
#else
#define LOG_LVL              LOG_LVL_INFO
#endif
#include <ulog.h>


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
	set_date(year,month,day);
	set_time(hour,min,sec);
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

	LOG_D("enter net callback\r\n");
	LOG_D("udp client rev data=%d,%s\r\n",len,data);
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
	LOG_D("enter iot callback\r\n");
	LOG_D("rev data=%d,%s\r\n",len,data);
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
	LOG_D("enter close onenent callback\r\n");

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
	LOG_D("enter read dsp callback\r\n");

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
	LOG_D("enter write dsp callback\r\n");	
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

	LOG_D("enter exec dsp callback\r\n");
	LOG_D("exec data len:%d   data=%s\r\n",len,cmd);
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
	LOG_D("enter fota callback\r\n");



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
	LOG_D("enter mqtt data callback\r\n");
	rt_kprintf("enter mqtt callback  mesg:%s,len:%d\r\n",mesg,mesg_len);

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
	LOG_E("enter reboot callback\r\n");
	
}





