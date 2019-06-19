/*
 * File      : qsdk_net.c
 * This file is part of net in qsdk
 * Copyright (c) 2018-2030, longmain Development Team
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-11-14     longmain     first version
 * 2019-06-12     longmain     Fix UDP connect errors
 * 2019-06-13     longmain     add hexstring to string
 * 2019-06-13     longmain     add net close callback
 */

#include "qsdk.h"
#include "at.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG              "[QSDK/NET]"
#ifdef QSDK_USING_LOG
#define LOG_LVL              LOG_LVL_DBG
#else
#define LOG_LVL              LOG_LVL_INFO
#endif
#include <ulog.h>




#ifdef QSDK_USING_NET

enum	NET_CONNECT_TYPE
{
	NET_CONNECT_START=10,
	NET_CONNECT_SUCCESS,
	NET_CONNECT_FAIL,
};

#define EVENT_NET_CONNECT_SUCCESS		(89<<1)
#define EVENT_NET_CONNECT_FAILURE		(88<<1)

extern at_response_t nb_resp;
extern at_client_t nb_client;
extern rt_event_t nb_event;

static struct net_stream  net_client_table[QSDK_NET_CLIENT_MAX]={0};

/*************************************************************
*	函数名称：	qsdk_net_client_init
*
*	函数功能：	net client 初始化
*
*	入口参数：	type :网络类型  1：TCP	2：UDP
*
*							port:本地端口号
*
*							server_ip:服务器IP
*	
*							server_port:服务器端口号
*
*	返回参数：	成功返回client地址  失败：RT_NULL
*
*	说明：		
*************************************************************/
qsdk_net_client_t qsdk_net_client_init(int type,int port,char *server_ip,unsigned short server_port)
{
	int num=0;
	for(num=0;num<QSDK_NET_CLIENT_MAX&&net_client_table[num].user_status;num++);
	if(num>=QSDK_NET_CLIENT_MAX)
	{
		LOG_E("net client max error\r\n");
		return RT_NULL;
	}
	net_client_table[num].type=type;
	net_client_table[num].port=port;
	net_client_table[num].server_ip=server_ip;
	net_client_table[num].server_port=server_port;
	net_client_table[num].user_status=1;
	
	return &net_client_table[num];
}

/*************************************************************
*	函数名称：	qsdk_net_create_socket
*
*	函数功能：	创建网络套子号 socket
*
*	入口参数：	client: net客户端结构体
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_net_create_socket(qsdk_net_client_t client)
{
	
#ifdef QSDK_USING_DEBUD
	LOG_D("\r\n sever ip=%s ,sever port=%d",client->server_ip,client->server_port);
#endif
	if(client->type==QSDK_NET_TYPE_TCP)
	{
		LOG_D("AT+NSOCR=STREAM,6,%d,1\r\n",client->port);
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NSOCR=STREAM,6,%d,1",client->port)!=RT_EOK) 
		{
			goto __exit;
		}
	}
	else if(client->type==QSDK_NET_TYPE_UDP)
	{
		LOG_D("AT+NSOCR=DGRAM,17,%d,1\r\n",client->port);
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NSOCR=DGRAM,17,%d,1",client->port)!=RT_EOK)
		{
			goto __exit;
		}
	}
	else goto __exit;

	at_resp_parse_line_args(nb_resp,2,"%d",&client->socket);
	return RT_EOK;

__exit:
	LOG_E("NET create socket send fail\r\n");
	return RT_ERROR;	
}
/*************************************************************
*	函数名称：	qsdk_net_connect_to_server
*
*	函数功能：	连接到服务器
*
*	入口参数：	client: net客户端结构体
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_net_connect_to_server(qsdk_net_client_t client)
{
	rt_uint32_t status;
	if(client->type==QSDK_NET_TYPE_TCP)
	{
		client->connect_status=NET_CONNECT_START;
		LOG_D("AT+NSOCO=%d,%s,%d\r\n",client->socket,client->server_ip,client->server_port);
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NSOCO=%d,%s,%d",client->socket,client->server_ip,client->server_port)!=RT_EOK)
		{
			LOG_E("net connect to server send fail\r\n");
			return RT_ERROR;
		}
		if(rt_event_recv(nb_event,EVENT_NET_CONNECT_FAILURE|EVENT_NET_CONNECT_SUCCESS,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,RT_WAITING_FOREVER,&status)!=RT_EOK)
		{
			LOG_E("net connect to server error\r\n");
		}
		if(status==EVENT_NET_CONNECT_FAILURE)
		{
			LOG_E("net connect to server failure\r\n");
			client->connect_status=NET_CONNECT_FAIL;
			return RT_ERROR;
		}
		else if(status==EVENT_NET_CONNECT_SUCCESS)
		{
			LOG_D("net connect to server success\r\n");
			client->connect_status=NET_CONNECT_SUCCESS;
			return RT_EOK;
		}
		

	}
	else if(client->type==QSDK_NET_TYPE_UDP)	client->connect_status=NET_CONNECT_SUCCESS;
	return RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_net_send_data
*
*	函数功能：	发送数据到服务器
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_net_send_data(qsdk_net_client_t client,char *str)
{
	if(str==NULL||strlen(str)>1024)
	{
		LOG_E("net send data too long\r\n");
		return RT_ERROR;
	}
	char *buf=rt_calloc(1,strlen(str)*2);
	if(buf==RT_NULL)
	{
		LOG_E("net create resp buf error\r\n");
		return RT_ERROR;
	}
	string_to_hex(str,strlen(str),buf);

	if(client->type==QSDK_NET_TYPE_TCP)
	{
		LOG_D("AT+NSOSD=%d,%d,%s\r\n",client->socket,strlen(buf)/2,buf);
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NSOSD=%d,%d,%s",client->socket,strlen(buf)/2,buf)!=RT_EOK)
		{
			LOG_E("net data send fail\r\n");
			goto __exit;
		}
		rt_free(buf);
		return RT_EOK;
	}
	else if(client->type==QSDK_NET_TYPE_UDP)
	{
		LOG_D("AT+NSOST=%d,%s,%d,%d,%s\r\n",client->socket,client->server_ip,client->server_port,strlen(buf)/2,buf);
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NSOST=%d,%s,%d,%d,%s",client->socket,client->server_ip,client->server_port,strlen(buf)/2,buf)!=RT_EOK) 
		{
			LOG_E("net data send fail\r\n");
			goto __exit;
		}
		rt_free(buf);
		return RT_EOK;
	}
	else 
		{
			LOG_E("net client connect type fail\r\n");
			goto __exit;
		}
__exit:
	rt_free(buf);
	return RT_ERROR;
}
/*************************************************************
*	函数名称：	net_rev_data
*
*	函数功能：	接收服务器返回的数据
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int net_rev_data(int port,int len)
{	
	char *result=RT_NULL;
	char *rev_socket=RT_NULL;
	char *rev_ip=RT_NULL;
	char *rev_port=RT_NULL;
	char *rev_len=RT_NULL;
	char *rev_data=RT_NULL;
	char *buf=rt_calloc(1,len*2+50);
	if(buf==RT_NULL)
	{
		LOG_E("net create resp buf error\r\n");
		return RT_ERROR;
	}
	LOG_D("AT+NSORF=%d,%d\r\n",port,len);
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NSORF=%d,%d",port,len)!=RT_EOK) 
	{
		LOG_E("net revice data send fail\r\n");
		rt_free(buf);
		return RT_ERROR;
	}
#ifdef QSDK_USING_DEBUD
	LOG_D("rev port=%d   rev len=%d",port,len);
#endif
	at_resp_parse_line_args(nb_resp,2,"\r\n%s",buf);
	LOG_D("%s\r\n",buf);
#ifdef QSDK_USING_M5310A
	result=strtok(buf,":");
	rev_socket=strtok(NULL,",");
#else
	rev_socket=strtok(buf,",");
#endif

	rev_ip=strtok(NULL,",");
	rev_port=strtok(NULL,",");
	rev_len=strtok(NULL,",");
	rev_data=strtok(NULL,",");

	char *str=rt_calloc(1,len);
	if(str==RT_NULL)
	{
		LOG_E("net callack create buf error\r\n");
		rt_free(buf);
		return RT_ERROR;
	}
	hexstring_to_string(rev_data,len,str);
	if(qsdk_net_data_callback(str,len)==RT_EOK)
	{
		rt_free(buf);
		rt_free(str);
		return RT_EOK;
	}
	else 
	{
		rt_free(buf);
		rt_free(str);
		return RT_ERROR;
	}
}

/*************************************************************
*	函数名称：	qsdk_net_get_client_revice
*
*	函数功能：	查看client是否收到消息
*
*	入口参数：	client：NET客户端ID
*
*	返回参数：	0 收到消息  1	未收到消息
*
*	说明：		
*************************************************************/
int qsdk_net_get_client_revice(qsdk_net_client_t client)
{
	if(client->revice_status==10)	return RT_EOK;

	return RT_ERROR;
}
/*************************************************************
*	函数名称：	qsdk_net_get_client_revice
*
*	函数功能：	查看客户端连接状态
*
*	入口参数：	client：NET客户端ID
*
*	返回参数：	0 已连接  1	未连接
*
*	说明：		
*************************************************************/
int qsdk_net_get_client_connect(qsdk_net_client_t client)
{
	if(client->connect_status==NET_CONNECT_SUCCESS)	return RT_EOK;

	return RT_ERROR;
}

/*************************************************************
*	函数名称：	qsdk_net_close_socket
*
*	函数功能：	关闭网络套子号 socket
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_net_close_socket(qsdk_net_client_t client)
{	
	LOG_D("AT+NSOCL=%d\r\n",client->socket);
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NSOCL=%d",client->socket)!=RT_EOK) return RT_ERROR;
	
	rt_memset(client,0,sizeof(client));
	return RT_EOK;
}

/*************************************************************
*	函数名称：	net_event_func
*
*	函数功能：	模块下发NET信息处理函数
*
*	入口参数：	data:下发数据指针   size:数据长度
*
*	返回参数：	无
*
*	说明：		
*************************************************************/
void net_event_func(char *event)
{		
		//判断是不是M5310 tcp 或者 udp 消息
		if(rt_strstr(event,"+NSONMI:")!=RT_NULL)
		{
			char *eventid=NULL;
			char *socket=NULL;
			char *len=NULL;
			int i;
			LOG_D("%s\r\n ",event);			
			eventid=strtok(event,":");
			socket=strtok(NULL,",");
			len=strtok(NULL,",");
			for(i=0;i<QSDK_NET_CLIENT_MAX;i++)
			{
				if(net_client_table[i].socket==atoi(socket))
				{
					net_client_table[i].revice_status=10;
					break;
				}
			}
			
			//调用网络数据处理回调函数
			if(net_rev_data(atoi(socket),atoi(len))!=RT_EOK)
				LOG_E("rev net data failure\r\n");
			else	net_client_table[i].revice_status=0;
		}
		else if(rt_strstr(event,"+NSOCLI:")!=RT_NULL)
		{
			rt_uint32_t i;
			char *head=NULL;
			char *id=NULL;
			head=strtok(event,":");
			id=strtok(NULL,":");
			for(i=0;i<QSDK_NET_CLIENT_MAX;i++)
			{
				if(net_client_table[i].socket==atoi(id))
				{
					break;
				}
			}
			if(net_client_table[i].connect_status==NET_CONNECT_START)
			{
				rt_event_send(nb_event,EVENT_NET_CONNECT_FAILURE);
			}
			else if(net_client_table[i].connect_status==NET_CONNECT_SUCCESS)
			{
				net_client_table[i].connect_status=NET_CONNECT_FAIL;
				qsdk_net_close_callback();
			}
		}
		else if(rt_strstr(event,"CONNECT OK")!=RT_NULL)
		{
			rt_event_send(nb_event,EVENT_NET_CONNECT_SUCCESS);
		}

}

#endif

