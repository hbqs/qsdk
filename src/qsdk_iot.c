/*
 * File      : qsdk_iot.c
 * This file is part of iot in qsdk
 * Copyright (c) 2018-2030, longmain Development Team
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-11-14     longmain     first version
 * 2018-07-03     longmain     add iot finsh cmd
 */
 
#include "qsdk.h"
#include "at.h"
#include "string.h"
#include "stdlib.h"

#ifdef QSDK_USING_IOT

#ifdef RT_USING_ULOG
#define LOG_TAG              "[QSDK/IOT]"
#ifdef QSDK_USING_LOG
#define LOG_LVL              LOG_LVL_DBG
#else
#define LOG_LVL              LOG_LVL_INFO
#endif
#include <ulog.h>
#else
#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME              "[QSDK/IOT]"
#ifdef QSDK_USING_LOG
#define DBG_LEVEL                      DBG_LOG
#else
#define DBG_LEVEL                      DBG_INFO
#endif /* QSDK_DEBUG */

#include <rtdbg.h>
#endif



#define EVENT_UPDATE_OK						(1<<20)
#define EVENT_UPDATE_ERROR				(1<<21)

//如果启用IOT支持

extern at_response_t nb_resp;
extern at_client_t	nb_client;
extern  struct nb_device nb_device_table;
//定义事件控制块
extern rt_event_t nb_event;

#if (defined QSDK_USING_M5310A)

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
	LOG_D("AT+NSMI=1\n");
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NSMI=1")!=RT_EOK)	
	{
		LOG_E("open iot update status error\n");
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
	LOG_D("AT+NNMI=1\n");
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NNMI=1")!=RT_EOK)
	{
		LOG_E("open iot down status error\n");
		return RT_ERROR;
	}
	
	return RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_iot_notify
*
*	函数功能：	上报信息到IOT平台
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_iot_notify(char *str)
{
	rt_uint32_t status;
	char *buf=rt_calloc(1,strlen(str)*2);
	if(buf==RT_NULL)
	{
		LOG_E("net create resp buf error\n");
		return RT_ERROR;
	}
	string_to_hex(str,strlen(str),buf);

	LOG_D("AT+NMGS=%d,%s\n",strlen(buf)/2,buf);
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NMGS=%d,%s",strlen(buf)/2,buf)!=RT_EOK)
	{
		LOG_E("date notify send error\n");
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
			LOG_E("iot date notify error\n");
			return RT_ERROR;
		}
	}
	LOG_E("iot date notify error\n");
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

			char *buf=rt_calloc(1,atoi(len)+1);
			if(str==RT_NULL)
			{
				LOG_E("iot callack create buf error\n");
				rt_free(buf);
			}
			hexstring_to_string(str,atoi(len),buf);
			if(qsdk_iot_data_callback(buf,atoi(len))!=RT_EOK)
			{
				rt_free(buf);
				LOG_E("qsdk iot data callback error\n");
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
#elif (defined QSDK_USING_ME3616)

enum qsdk_iot_status_type
{
	iot_status_reg_success=10,
	iot_status_reg_failure,
	iot_status_unreg_success,
};

#define EVENT_REG_OK						(0x72<<1)
#define EVENT_REG_ERROR					(0x73<<1)
#define EVENT_OBSERVER_OK				(0x74<<1)
#define EVENT_DEREG_OK					(0x75<<1)
#define EVENT_DEREG_ERROR				(0x76<<1)

static rt_uint8_t iot_connect_status=0;
static rt_uint8_t iot_dereg_status=0;
/*************************************************************
*	函数名称：	qsdk_iot_create_new_client
*
*	函数功能：	注册到电信平台
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_iot_create_new_client(void)
{
	rt_uint32_t status;
	
	LOG_D("AT+M2MCLINEW=%s,%s,\"%s\",%d\n",QSDK_IOT_ADDRESS,QSDK_IOT_PORT,nb_device_table.imei,QSDK_IOT_REG_LIFE_TIMR);
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+M2MCLINEW=%s,%s,\"%s\",%d",QSDK_IOT_ADDRESS,QSDK_IOT_PORT,nb_device_table.imei,QSDK_IOT_REG_LIFE_TIMR)!=RT_EOK) return RT_ERROR;

	rt_event_recv(nb_event,EVENT_REG_OK|EVENT_REG_ERROR,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,60000,&status);
	if(status==EVENT_REG_ERROR)
	{
		iot_connect_status=iot_status_reg_failure;
		return RT_ERROR;
	}
	rt_event_recv(nb_event,EVENT_OBSERVER_OK,RT_EVENT_FLAG_AND|RT_EVENT_FLAG_CLEAR,60000,&status);
	if(status==EVENT_OBSERVER_OK)
	{
		iot_connect_status=iot_status_reg_success;
		return RT_EOK;
	}
		iot_connect_status=iot_status_reg_success;
		return RT_EOK;
}

/*************************************************************
*	函数名称：	qsdk_iot_del_client
*
*	函数功能：	在电信平台注销设备
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_iot_del_client(void)
{
	rt_uint32_t status;
	LOG_D("AT+M2MCLIDEL\n");
	iot_dereg_status=1;
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+M2MCLIDEL")!=RT_EOK)
	{
		iot_dereg_status=0;
		return RT_ERROR;
	}
	if(iot_connect_status==iot_status_reg_success)
	{
		rt_event_recv(nb_event,EVENT_DEREG_OK|EVENT_DEREG_ERROR,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,60000,&status);
		iot_dereg_status=0;
		if(status==EVENT_DEREG_OK)
			return RT_EOK;
		
		return	RT_ERROR;
	}
	return RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_iot_notify
*
*	函数功能：	在电信平台注销设备
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_iot_notify(char *str)
{
	rt_uint32_t status;
	LOG_D("AT+M2MCLISEND=\"%s\"\n",str);
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+M2MCLISEND=\"%s\"",str)!=RT_EOK)	return RT_ERROR;

	rt_event_recv(nb_event,EVENT_UPDATE_OK|EVENT_UPDATE_ERROR,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,60000,&status);
	if(status==EVENT_UPDATE_OK)
		return	RT_EOK;
	
	return RT_ERROR;
}

/*************************************************************
*	函数名称：	qsdk_iot_get_connect_status
*
*	函数功能：	获取电信平台连接状态
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_iot_get_connect_status(void)
{
	if(iot_connect_status==iot_status_reg_success)
		return	RT_EOK;
	
	return RT_ERROR;
}

void iot_event_func(char *event)
{
	char *result=NULL;

	if(rt_strstr(event,"+M2MCLI:")!=RT_NULL)
	{
#ifdef QSDK_USING_LOG
		LOG_D("%s\n",event);
#endif
		if(rt_strstr(event,"+M2MCLI:register failed"))
			rt_event_send(nb_event,EVENT_REG_ERROR);
		else if(rt_strstr(event,"+M2MCLI:register success"))
			rt_event_send(nb_event,EVENT_REG_OK);
		if(rt_strstr(event,"+M2MCLI:observe success"))
			rt_event_send(nb_event,EVENT_OBSERVER_OK);
		else if(rt_strstr(event,"+M2MCLI:deregister failed"))
			rt_event_send(nb_event,EVENT_DEREG_ERROR);
		else if(rt_strstr(event,"+M2MCLI:deregister success"))
		{
			iot_connect_status=iot_status_unreg_success;
			if(iot_dereg_status==1)
				rt_event_send(nb_event,EVENT_DEREG_OK);
		}
		else if(rt_strstr(event,"+M2MCLI:notify failed"))
			rt_event_send(nb_event,EVENT_UPDATE_ERROR);
		else if(rt_strstr(event,"+M2MCLI:notify success"))
			rt_event_send(nb_event,EVENT_UPDATE_OK);
	}		
	if(rt_strstr(event,"+M2MCLIRECV:")!=RT_NULL)
	{
		char *str;
#ifdef QSDK_USING_LOG
		LOG_D("%s\n",event);
#endif
		result=strtok(event,":");
		str=strtok(NULL,",");
		char *data=rt_calloc(1,strlen(str)/2);
		if(str==RT_NULL)
		{
			LOG_E("net callack create buf error\n");
		}
		else
		{
			hexstring_to_string(str,strlen(str)/2-1,data);
			if(qsdk_iot_data_callback(data,(strlen(str)/2)-1)!=RT_EOK)
			{
#ifdef QSDK_USING_DEBUD
				LOG_D("iot data callback failure\r\n");
#endif			
			}	
			rt_free(data);	
		}
	}
}




#endif  //endif iot using

#ifdef QSDK_USING_FINSH_CMD
void qsdk_iot(int argc,char**argv)
{
	if (argc > 1)
	{
		if (!strcmp(argv[1], "notify"))
		{
				if (argc > 2)
				{
						if(qsdk_iot_notify(argv[2])!=RT_EOK)
							rt_kprintf("iot data send error\n");
						else rt_kprintf("iot data send success\n");
				}
				else
				{
						rt_kprintf("qsdk_iot notify <data>            -  Please enter the data you need to send.\n");
				}
		}
#ifdef QSDK_USING_ME3616
		else if (!strcmp(argv[1], "create_new_client"))
		{
			if(qsdk_iot_create_new_client()!=RT_EOK)
			{
				LOG_E("iot reg failed\n");
			}
			else
			{
				LOG_D("iot reg success\n");
			}
		}
		else	if (!strcmp(argv[1], "del_client"))
		{
			if(qsdk_iot_del_client()!=RT_EOK)
			{
				LOG_E("iot dereg failed\n");
			}
			else
			{
				LOG_D("iot dereg success\n");
			}
		}
#endif
		else
		{
				rt_kprintf("Unknown command. Please enter 'qsdk_iot' for help\n");
		}
	}
	else
	{
			rt_kprintf("Usage:\n");
#ifdef QSDK_USING_ME3616
			rt_kprintf("qsdk_iot create_new_client      - reg client to iot server\n");
			rt_kprintf("qsdk_iot del_client             - dereg client to iot server\n");		
#endif
			rt_kprintf("qsdk_iot notify <data>          - Send data to iot server\n");
	}
}


MSH_CMD_EXPORT(qsdk_iot, qsdk iot function);
#endif //QSDK_USING_FINSH_CMD

#endif


