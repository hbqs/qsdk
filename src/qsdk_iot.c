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

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME              "[QSDK/IOT]"
#ifdef QSDK_USING_LOG
#define DBG_LEVEL                      DBG_LOG
#else
#define DBG_LEVEL                      DBG_INFO
#endif /* QSDK_DEBUG */

#include <rtdbg.h>


#ifdef QSDK_USING_IOT

#define EVENT_UPDATE_OK						(0x70<<1)
#define EVENT_UPDATE_ERROR				(0x71<<1)

//如果启用IOT支持

extern at_response_t nb_resp;
extern at_client_t	nb_client;

//定义事件控制块
extern rt_event_t nb_event;

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
	rt_uint32_t status;
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
	rt_free(buf);
	if(rt_event_recv(nb_event,EVENT_UPDATE_ERROR|EVENT_UPDATE_OK,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,60000,&status)==RT_EOK)
	{
		if(status==EVENT_UPDATE_OK)
			return  RT_EOK;
		else
		{
			LOG_E("iot date notify failer\r\n");
			return RT_ERROR;
		}
	}
	LOG_E("iot date notify failer\r\n");
	return RT_ERROR;
}

void iot_event_func(char *event)
{
	char *result=NULL;
	if(rt_strstr(event,"+NNMI:")!=RT_NULL)
		{
			char *len;
			char *str;
			LOG_D("%s\r\n",event);		
			result=strtok(event,":");
			len=strtok(NULL,",");
			str=strtok(NULL,"\r");

			char *buf=rt_calloc(1,atoi(len));
			if(str==RT_NULL)
			{
				LOG_E("iot callack create buf error\r\n");
				rt_free(buf);
			}
			hexstring_to_string(str,atoi(len),buf);
			if(qsdk_iot_data_callback(buf,atoi(len))!=RT_EOK)
			{
				rt_free(buf);
				LOG_E("qsdk iot data callback failure\r\n");
			}
			rt_free(buf);
		}
		else if(rt_strstr(event,"+NSMI:")!=RT_NULL)
		{
			LOG_D("%s\r\n",event);
			if(rt_strstr(event,"+NSMI:SENT")!=RT_NULL)
				rt_event_send(nb_event,EVENT_UPDATE_OK);
			if(rt_strstr(event,"+NSMI:DISCARDED")!=RT_NULL)
				rt_event_send(nb_event,EVENT_UPDATE_ERROR);
		}
}
#endif


