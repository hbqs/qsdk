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

#ifdef RT_USING_ULOG
#define LOG_TAG              "[QSDK/CALLBACK]"
#ifdef QSDK_USING_LOG
#define LOG_LVL              LOG_LVL_DBG
#else
#define LOG_LVL              LOG_LVL_INFO
#endif
#include <ulog.h>
#else
#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME              "[QSDK/CALLBACK]"
#ifdef QSDK_USING_LOG
#define DBG_LEVEL                      DBG_LOG
#else
#define DBG_LEVEL                      DBG_INFO
#endif /* QSDK_DEBUG */

#include <rtdbg.h>
#endif


#include "qsdk_beep.h"
#include "qsdk_led.h"

//定义EVENT 事件
#define NB_REBOOT			(1<<1)
#define SEND_ERROR		(1<<2)
#define NET_CLOSE			(1<<3)
#define ONENET_CLOSE  (1<<4)
#define UPDATE_ERROR	(1<<5)

//引用led 控制块
extern struct led_state_type led0;
extern struct led_state_type led1;
extern struct led_state_type led2;
extern struct led_state_type led3;

//引用温湿度缓存变量
extern float humidity;
extern float temperature;

#ifdef QSDK_USING_NET
//引用net client
extern qsdk_net_client_t net_client;
#elif (defined QSDK_USING_IOT)||(defined QSDK_USING_ONENET)
//引用平台上报标志
extern rt_uint16_t update_status;
#if (defined QSDK_USING_ONENET)
//引入onenet stream
extern qsdk_onenet_stream_t temp_object;
extern qsdk_onenet_stream_t hump_object;
extern qsdk_onenet_stream_t light0_object;
extern qsdk_onenet_stream_t light1_object;
extern qsdk_onenet_stream_t light2_object;
#endif


#endif

//引用错误事件句柄
extern rt_event_t net_error;

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
	
#ifdef QSDK_USING_NET
		rt_event_send(net_error,NET_CLOSE);
#endif
	
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
	LOG_I("enter net callback\r\n");
#ifdef QSDK_USING_NET
	LOG_D("net client rev data=%d,%s\r\n",len,data);
	
	if(rt_strstr(data,"cmd:")!=RT_NULL)
	{
		//检测是否有led0 控制指令
		if(rt_strstr(data,"led0:1")!=RT_NULL)
		{
			led0.current_state=LED_ON;
		}
		else if(rt_strstr(data,"led0:0")!=RT_NULL)
		{
			led0.current_state=LED_OFF;
		}
		
		//检测是否有led1 控制指令
		if(rt_strstr(data,"led1:1")!=RT_NULL)
		{
			led1.current_state=LED_ON;
		}
		else if(rt_strstr(data,"led1:0")!=RT_NULL)
		{
			led1.current_state=LED_OFF;
		}
		
		//检测是否有led2 控制指令
		if(rt_strstr(data,"led2:1")!=RT_NULL)
		{
			led2.current_state=LED_ON;
		}
		else if(rt_strstr(data,"led2:0")!=RT_NULL)
		{
			led2.current_state=LED_OFF;
		}
		
		//检测是否有led3 控制指令
		if(rt_strstr(data,"led3:1")!=RT_NULL)
		{
			led3.current_state=LED_ON;
		}
		else if(rt_strstr(data,"led3:0")!=RT_NULL)
		{
			led3.current_state=LED_OFF;
		}
		
		//检测是否有beep 控制指令
		if(rt_strstr(data,"beep:1")!=RT_NULL)
		{
			qsdk_beep_on();
		}
		else if(rt_strstr(data,"beep:0")!=RT_NULL)
		{
			qsdk_beep_off();
		}
		
		//检测是否有sht20 读取指令
		if(rt_strstr(data,"read:sht20")!=RT_NULL)
		{
			char *buf=rt_calloc(1,len*2+50);
			if(buf==RT_NULL)
			{
				LOG_E("net create resp buf error\r\n");
			}
			else
			{
				sprintf(buf,"Now the board temperature is:%0.2f,humidity is:%0.2f",temperature,humidity);
				qsdk_net_send_data(net_client,buf);
			
			}
			rt_free(buf);
		}
	}
#endif
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
	LOG_I("enter iot callback\r\n");
	
#ifdef QSDK_USING_IOT
	LOG_D("rev data=%d,%s\r\n",len,data);
	if(rt_strstr(data,"cmd:")!=RT_NULL)
	{
		//检测是否有led0 控制指令
		if(rt_strstr(data,"led0:1")!=RT_NULL)
		{
			led0.current_state=LED_ON;
		}
		else if(rt_strstr(data,"led0:0")!=RT_NULL)
		{
			led0.current_state=LED_OFF;
		}
		
		//检测是否有led1 控制指令
		if(rt_strstr(data,"led1:1")!=RT_NULL)
		{
			led1.current_state=LED_ON;
		}
		else if(rt_strstr(data,"led1:0")!=RT_NULL)
		{
			led1.current_state=LED_OFF;
		}
		
		//检测是否有led2 控制指令
		if(rt_strstr(data,"led2:1")!=RT_NULL)
		{
			led2.current_state=LED_ON;
		}
		else if(rt_strstr(data,"led2:0")!=RT_NULL)
		{
			led2.current_state=LED_OFF;
		}
		
		//检测是否有led3 控制指令
		if(rt_strstr(data,"led3:1")!=RT_NULL)
		{
			led3.current_state=LED_ON;
		}
		else if(rt_strstr(data,"led3:0")!=RT_NULL)
		{
			led3.current_state=LED_OFF;
		}
		
		//检测是否有beep 控制指令
		if(rt_strstr(data,"beep:1")!=RT_NULL)
		{
			qsdk_beep_on();
		}
		else if(rt_strstr(data,"beep:0")!=RT_NULL)
		{
			qsdk_beep_off();
		}
		
		//检测是否有sht20 读取指令
		if(rt_strstr(data,"read:sht20")!=RT_NULL)
		{
			update_status=10;
		}
	}
#endif
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
#ifdef QSDK_USING_ONENET
	rt_event_send(net_error,ONENET_CLOSE);
#endif
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
#ifdef QSDK_USING_ONENET
	if(insid== -1&& resid== -1)
	{
		//判断是否为读取温度值
		if(qsdk_onenet_get_object_read(temp_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,temp_object,0,(qsdk_onenet_value_t)&temperature,0,0);
		}		
		//判断是否为读取湿度值
		else if(qsdk_onenet_get_object_read(hump_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,hump_object,0,(qsdk_onenet_value_t)&humidity,0,0);
		}
		//判断是否为读取light
		else if(qsdk_onenet_get_object_read(light0_object)==RT_EOK)
		{
			if(qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light0_object,0,(qsdk_onenet_value_t)&led0.current_state,2,1)==RT_ERROR)
			{
				LOG_E("onenet read rsp light0 error\r\n");
				return RT_ERROR;
			}
			if(qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light1_object,0,(qsdk_onenet_value_t)&led1.current_state,1,2)==RT_ERROR)
			{
				LOG_E("onenet read rsp light1 error\r\n");
				return RT_ERROR;
			}
			if(qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light2_object,0,(qsdk_onenet_value_t)&led2.current_state,0,0)==RT_ERROR)
			{
				LOG_E("onenet read rsp light2 error\r\n");
				return RT_ERROR;
			}
		}
	}
	else if(resid== -1)
	{
		//判断是否为读取温度值
		if(qsdk_onenet_get_object_read(temp_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,temp_object,0,(qsdk_onenet_value_t)&temperature,0,0);
		}
		//判断是否为读取湿度值
		else if(qsdk_onenet_get_object_read(hump_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,hump_object,0,(qsdk_onenet_value_t)&humidity,0,0);
		}
		//判断是否为读取led0 状态
		else if(qsdk_onenet_get_object_read(light0_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light0_object,0,(qsdk_onenet_value_t)&led0.current_state,0,0);
		}
		//判断是否为读取led1 状态
		else if(qsdk_onenet_get_object_read(light1_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light1_object,0,(qsdk_onenet_value_t)&led1.current_state,0,0);
		}
		//判断是否为读取led2 状态
		else if(qsdk_onenet_get_object_read(light2_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light2_object,0,(qsdk_onenet_value_t)&led2.current_state,0,0);
		}
	}
	else
	{
		//判断是否为读取温度值
		if(qsdk_onenet_get_object_read(temp_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,temp_object,0,(qsdk_onenet_value_t)&temperature,0,0);
		}
		//判断是否为读取湿度值
		else if(qsdk_onenet_get_object_read(hump_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,hump_object,0,(qsdk_onenet_value_t)&humidity,0,0);
		}
		//判断是否为读取led0 状态
		else if(qsdk_onenet_get_object_read(light0_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light0_object,0,(qsdk_onenet_value_t)&led0.current_state,0,0);
		}
		//判断是否为读取led1 状态
		else if(qsdk_onenet_get_object_read(light1_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light1_object,0,(qsdk_onenet_value_t)&led1.current_state,0,0);
		}
		//判断是否为读取led2 状态
		else if(qsdk_onenet_get_object_read(light2_object)==RT_EOK)
		{
			qsdk_onenet_read_rsp(msgid,qsdk_onenet_status_result_read_success,light2_object,0,(qsdk_onenet_value_t)&led2.current_state,0,0);
		}
	}
#endif
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
#ifdef QSDK_USING_ONENET
	if(qsdk_onenet_get_object_write(light0_object)==RT_EOK)
	{
		led0.current_state=atoi(value);
	}
	else if(qsdk_onenet_get_object_write(light1_object)==RT_EOK)
	{
		led1.current_state=atoi(value);
	}
	else if(qsdk_onenet_get_object_write(light2_object)==RT_EOK)
	{
		led2.current_state=atoi(value);
	}
#endif
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
	LOG_I("enter exec dsp callback\r\n");
#ifdef QSDK_USING_ONENET
	if(rt_strstr(cmd,"cmd:")!=RT_NULL)
	{
		//检测是否有led0 控制指令
		if(rt_strstr(cmd,"led0:1")!=RT_NULL)
		{
			led0.current_state=LED_ON;
		}
		else if(rt_strstr(cmd,"led0:0")!=RT_NULL)
		{
			led0.current_state=LED_OFF;
		}
		
		//检测是否有led1 控制指令
		if(rt_strstr(cmd,"led1:1")!=RT_NULL)
		{
			led1.current_state=LED_ON;
		}
		else if(rt_strstr(cmd,"led1:0")!=RT_NULL)
		{
			led1.current_state=LED_OFF;
		}
		
		//检测是否有led2 控制指令
		if(rt_strstr(cmd,"led2:1")!=RT_NULL)
		{
			led2.current_state=LED_ON;
		}
		else if(rt_strstr(cmd,"led2:0")!=RT_NULL)
		{
			led2.current_state=LED_OFF;
		}
		
		//检测是否有led3 控制指令
		if(rt_strstr(cmd,"led3:1")!=RT_NULL)
		{
			led3.current_state=LED_ON;
		}
		else if(rt_strstr(cmd,"led3:0")!=RT_NULL)
		{
			led3.current_state=LED_OFF;
		}
		
		//检测是否有beep 控制指令
		if(rt_strstr(cmd,"beep:1")!=RT_NULL)
		{
			qsdk_beep_on();
		}
		else if(rt_strstr(cmd,"beep:0")!=RT_NULL)
		{
			qsdk_beep_off();
		}
		
		//检测是否有sht20 读取指令
		if(rt_strstr(cmd,"read:sht20")!=RT_NULL)
		{
			update_status=10;
		}
	}
#endif
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
	
	if(rt_event_send(net_error,NB_REBOOT)==RT_ERROR)
		LOG_I("event reboot send error\r\n");
}





