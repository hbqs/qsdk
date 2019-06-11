/*
 * File      : qsdk_iot.c
 * This file is part of iot in qsdk
 * Copyright (c) 2018-2030, longmain Development Team
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-11-14     longmain     first version
 */
 
#include "qsdk.h"
#include "at.h"
#include "string.h"
#include "stdlib.h"

#define LOG_TAG              "[QSDK/IOT]"
#ifdef QSDK_USING_LOG
#define LOG_LVL              LOG_LVL_DBG
#else
#define LOG_LVL              LOG_LVL_INFO
#endif
#include <ulog.h>


#ifdef QSDK_USING_IOT


//如果启用IOT支持

extern at_response_t nb_resp;
extern at_client_t	nb_client;
extern struct nb_device nb_device_table;


/*************************************************************
*	函数名称：	qsdk_iot_open_update_status
*
*	函数功能：	开启上报信息提示
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_iot_open_update_status(void)
{
	LOG_D("AT+NSMI=1\r\n");
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NSMI=1")!=RT_EOK)	
	{
		LOG_E("open iot update status fail\r\n");
		return RT_ERROR;
	}
	return RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_iot_open_down_date_status
*
*	函数功能：	开启下发消息提示
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_iot_open_down_date_status(void)
{
	LOG_D("AT+NNMI=1\r\n");
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NNMI=1")!=RT_EOK)
	{
		LOG_E("open iot down status fail\r\n");
		return RT_ERROR;
	}
	
	return RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_iot_update
*
*	函数功能：	上报信息到IOT平台
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_iot_update(char *str)
{
	nb_device_table.notify_status=qsdk_iot_status_notify_init;
	char *buf=rt_calloc(1,strlen(str)*2);
	if(buf==RT_NULL)
	{
		LOG_E("net create resp buf error\r\n");
		return RT_ERROR;
	}
	string_to_hex(str,strlen(str),buf);

	LOG_D("AT+NMGS=%d,%s\r\n",strlen(buf)/2,buf);
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NMGS=%d,%s",strlen(buf)/2,buf)!=RT_EOK)
	{
		LOG_E("date notify send fail\r\n");
		rt_free(buf);
		return RT_ERROR;
	}
	
	rt_thread_delay(100);
	if(nb_device_table.notify_status==qsdk_iot_status_notify_success)
	{
#ifdef QSDK_USING_DEBUG
		LOG_D("qsdk iot notify success\r\n");
#endif
		rt_free(buf);
		return RT_EOK;
	}
	
	nb_device_table.error=qsdk_iot_status_notify_failure;
	LOG_E("iot date notify failer\r\n");
	rt_free(buf);
	return RT_ERROR;
}

void iot_event_func(char *event)
{
	char *result=NULL;
	if(rt_strstr(event,"+NNMI:")!=RT_NULL)
		{
			char *len;
			char *str;
#ifdef QSDK_USING_DEBUG
			LOG_D("%s\r\n",event);
#endif			
			result=strtok(event,":");
			len=strtok(NULL,",");
			str=strtok(NULL,",");

			if(qsdk_iot_data_callback(str,atoi(len))!=RT_EOK)
				LOG_E("qsdk iot data callback failure\r\n");
		}
		else if(rt_strstr(event,"+NSMI:")!=RT_NULL)
		{
#ifdef QSDK_USING_DEBUG
			LOG_D("%s\r\n",event);
#endif				
			nb_device_table.notify_status=qsdk_iot_status_notify_success;
		}
}
#endif


