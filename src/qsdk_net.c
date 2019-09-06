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
 * 2019-07-03     longmain     add net finsh cmd
 */

#include "qsdk.h"
#include "at.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef QSDK_USING_NET

#ifdef RT_USING_ULOG
#define LOG_TAG              "[QSDK/NET]"
#ifdef QSDK_USING_LOG
#define LOG_LVL              LOG_LVL_DBG
#else
#define LOG_LVL              LOG_LVL_INFO
#endif
#include <ulog.h>
#else
#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME              "[QSDK/NET]"
#ifdef QSDK_USING_LOG
#define DBG_LEVEL                      DBG_LOG
#else
#define DBG_LEVEL                      DBG_INFO
#endif /* QSDK_DEBUG */

#include <rtdbg.h>
#endif




enum	NET_CONNECT_TYPE
{
	NET_CONNECT_START=10,
	NET_CONNECT_SUCCESS,
	NET_CONNECT_FAIL,
	NET_CONNECT_ERROR,
};

#define EVENT_NET_CONNECT_SUCCESS		(1<<16)
#define EVENT_NET_CONNECT_FAILURE		(1<<17)

extern at_response_t nb_resp;
extern at_client_t nb_client;
extern rt_event_t nb_event;

static struct net_stream  net_client_table[QSDK_NET_CLIENT_MAX]={0};

#if (defined QSDK_USING_M5311)
/*************************************************************
*	函数名称：	qsdk_net_set_out_format
*
*	函数功能：	设置TCP/UCP下发信息输出格式为字符串
*
*	入口参数：	无
*
*	返回参数：	0:成功    1:失败
*
*	说明：		
*************************************************************/
int qsdk_net_set_out_format(void)
{
#ifdef QSDK_USING_M5311
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+IPRCFG=1,2,1")!=RT_EOK)	return RT_ERROR;
#elif(defined QSDK_USING_ME3616)
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+ESOSETRPT=1")!=RT_EOK)	return RT_ERROR;
#endif	
	return  RT_EOK;
}
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
*	返回参数：	成功:client地址  	失败：RT_NULL
*
*	说明：		
*************************************************************/
qsdk_net_client_t qsdk_net_client_init(int type,int socket,char *server_ip,unsigned short server_port)
{
	int num=0;
	for(num=0;num<QSDK_NET_CLIENT_MAX&&net_client_table[num].user_status;num++);
	if(num>=QSDK_NET_CLIENT_MAX)
	{
		LOG_E("The number of network clients exceeds QSDK_NET_CLIENT_MAX\n");
		return RT_NULL;
	}
	net_client_table[num].type=type;
	net_client_table[num].socket=socket;
	net_client_table[num].server_port=server_port;
	net_client_table[num].user_status=1;
	strcpy(net_client_table[num].server_ip,server_ip);

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
	rt_uint32_t status;
#ifdef QSDK_USING_DEBUD
	LOG_D("sever ip=%s ,sever port=%d\n",client->server_ip,client->server_port);
#endif
	if(client->type==QSDK_NET_TYPE_TCP)
	{
		client->connect_status=NET_CONNECT_START;
		LOG_D("AT+IPSTART=%d,\"TCP\",\"%s\",%d\n",client->socket,client->server_ip,client->server_port);
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+IPSTART=%d,\"TCP\",\"%s\",%d",client->socket,client->server_ip,client->server_port)!=RT_EOK) 
		{
			LOG_E("NET create socket send fail\n");
			client->connect_status=NET_CONNECT_FAIL;
			return RT_ERROR;	
		}
		rt_event_recv(nb_event,EVENT_NET_CONNECT_FAILURE|EVENT_NET_CONNECT_SUCCESS,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,10000,&status);

		if(status==EVENT_NET_CONNECT_SUCCESS)
		{
			LOG_D("net connect to server success\n");
			client->connect_status=NET_CONNECT_SUCCESS;
			return RT_EOK;
		}
		else
		{
			LOG_E("net connect to server failure\n");
			client->connect_status=NET_CONNECT_FAIL;
			return RT_ERROR;
		}
	}
	else if(client->type==QSDK_NET_TYPE_UDP)
	{
		LOG_D("AT+IPSTART=%d,\"UDP\",\"%s\",%d\n",client->socket,client->server_ip,client->server_port);
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+IPSTART=%d,\"UDP\",\"%s\",%d",client->socket,client->server_ip,client->server_port)!=RT_EOK) 
		{
				LOG_E("NET create socket send fail\n");
				return RT_ERROR;	
		}
		client->connect_status=NET_CONNECT_SUCCESS;
	}
	else 
	{
		LOG_E("net type is not udp or tcp\n");
		return RT_ERROR;	
	}
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
		LOG_E("net send data too long\n");
		return RT_ERROR;
	}
	if(client->type==QSDK_NET_TYPE_TCP)
	{
		LOG_D("AT+IPSEND=%d,0,\"%s\",1\n",client->socket,str);
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+IPSEND=%d,0,\"%s\",1",client->socket,str)!=RT_EOK)
		{
			LOG_E("net data send fail\n");
			client->connect_status=NET_CONNECT_ERROR;
			return RT_ERROR;
		}
		return RT_EOK;
	}
	else if(client->type==QSDK_NET_TYPE_UDP)
	{
		LOG_D("AT+IPSEND=%d,0,\"%s\",\"%s\",%d,1\n",client->socket,str,client->server_ip,client->server_port);
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+IPSEND=%d,0,\"%s\",\"%s\",%d,1",client->socket,str,client->server_ip,client->server_port)!=RT_EOK) 
		{
			LOG_E("net data send fail\n");
			client->connect_status=NET_CONNECT_ERROR;
			return RT_ERROR;
		}
		return RT_EOK;
	}
	else 
		{
			LOG_E("net client connect type fail\n");
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
*	函数名称：	qsdk_net_get_client_connect
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

	if(client->connect_status==NET_CONNECT_ERROR)	return 2;
	
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
	int i;
  if(client->connect_status==NET_CONNECT_SUCCESS||client->connect_status==NET_CONNECT_ERROR)
	{
		LOG_D("AT+IPCLOSE=%d\n",client->socket);
		at_obj_exec_cmd(nb_client,nb_resp,"AT+IPCLOSE=%d",client->socket);
	}
	//清空client信息
	for(i=0;i<QSDK_NET_CLIENT_MAX;i++)
	{
		if(&net_client_table[i]==client)
		{
			memset(&net_client_table[i],0,sizeof(net_client_table[i]));
			break;
		}	
	}
	return RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_net_clear_environment
*
*	函数功能：	清理net运行环境参数
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_net_clear_environment(void)
{	
	memset(net_client_table,0,sizeof(net_client_table));
	LOG_D("qsdk net client environment clear success\n");
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
	//判断是不是收到M5311 tcp 或者 udp 消息
	if(rt_strstr(event,"+IPRD:")!=RT_NULL)
	{
		char *eventid=NULL;
		char *socket=NULL;
		char *ip=NULL;
		char *port=NULL;
		char *len=NULL;
		char *data=NULL;
		int i;
		LOG_D("%s\r\n ",event);			
		eventid=strtok(event,":");
		socket=strtok(NULL,",");
		ip=strtok(NULL,",");
		port=strtok(NULL,",");
		len=strtok(NULL,",");
		data=strtok(NULL,",");
		for(i=0;i<QSDK_NET_CLIENT_MAX;i++)
		{
			if(net_client_table[i].socket==atoi(socket))
			{
				net_client_table[i].revice_status=10;
				break;
			}
		}
		char *str=rt_calloc(1,atoi(len)+1);
		if(str==RT_NULL)
		{
			LOG_E("net callack create buf error\n");
		}
		else
		{
			hexstring_to_string(data,atoi(len),str);
			//调用网络数据处理回调函数
			if(qsdk_net_data_callback(str,atoi(len))!=RT_EOK)
				LOG_E("rev net data failure\n");
			net_client_table[i].revice_status=0;
			rt_free(str);
		}
	}
	else if(rt_strstr(event,"+IPCLOSE:")!=RT_NULL)
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
#ifdef QSDK_USING_LOG
		LOG_D("%s\n",event);
#endif
			rt_event_send(nb_event,EVENT_NET_CONNECT_SUCCESS);
	}
}
#elif (defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
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
*	返回参数：	成功:client地址  	失败：RT_NULL
*
*	说明：		
*************************************************************/
qsdk_net_client_t qsdk_net_client_init(int type,int port,char *server_ip,unsigned short server_port)
{
	int num=0;
	for(num=0;num<QSDK_NET_CLIENT_MAX&&net_client_table[num].user_status;num++);
	if(num>=QSDK_NET_CLIENT_MAX)
	{
		LOG_E("The number of network clients exceeds QSDK_NET_CLIENT_MAX\n");
		return RT_NULL;
	}
	net_client_table[num].type=type;
	net_client_table[num].port=port;
	net_client_table[num].server_port=server_port;
	net_client_table[num].user_status=1;
	strcpy(net_client_table[num].server_ip,server_ip);

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
	rt_uint32_t status;
#ifdef QSDK_USING_DEBUD
	LOG_D("sever ip=%s ,sever port=%d\n",client->server_ip,client->server_port);
#endif
	if(client->type==QSDK_NET_TYPE_TCP)
	{
		LOG_D("AT+NSOCR=STREAM,6,%d,1\n",client->port);
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NSOCR=STREAM,6,%d,1",client->port)!=RT_EOK) 
		{
			LOG_E("NET create socket send fail\n");
			return RT_ERROR;	
		}
		at_resp_parse_line_args(nb_resp,2,"%d",&client->socket);

		client->connect_status=NET_CONNECT_START;
		LOG_D("AT+NSOCO=%d,%s,%d\n",client->socket,client->server_ip,client->server_port);
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NSOCO=%d,%s,%d",client->socket,client->server_ip,client->server_port)!=RT_EOK)
		{
			LOG_E("net connect to server send fail\n");
			client->connect_status=NET_CONNECT_FAIL;
			return RT_ERROR;
		}

		rt_event_recv(nb_event,EVENT_NET_CONNECT_SUCCESS,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,10000,&status);
		if(status==EVENT_NET_CONNECT_SUCCESS)
		{
			LOG_D("net connect to server success\n");
			client->connect_status=NET_CONNECT_SUCCESS;
		}
		else
		{
			LOG_E("net connect to server failure\n");
			client->connect_status=NET_CONNECT_FAIL;
			return RT_ERROR;
		}
	}
	else if(client->type==QSDK_NET_TYPE_UDP)
	{
		LOG_D("AT+NSOCR=DGRAM,17,%d,1\n",client->port);
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NSOCR=DGRAM,17,%d,1",client->port)!=RT_EOK)
		{
				LOG_E("NET create socket send fail\n");
				return RT_ERROR;	
		}
		client->connect_status=NET_CONNECT_SUCCESS;
		at_resp_parse_line_args(nb_resp,2,"%d",&client->socket);
	}
	else 
	{
		LOG_E("net type is not udp or tcp\n");
		return RT_ERROR;	
	}
	//连接好tcp/udp服务器后，防止设备端上报数据和服务器立马下发数据同时进行，在此之前没有延时，造成内部数据丢失，所以在此延时500ms解决。
	rt_thread_delay(500);
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
		LOG_E("net send data too long\n");
		return RT_ERROR;
	}
	char *buf=rt_calloc(1,strlen(str)*2+10);
	if(buf==RT_NULL)
	{
		LOG_E("net create resp buf error\n");
		return RT_ERROR;
	}
	string_to_hex(str,strlen(str),buf);

	if(client->type==QSDK_NET_TYPE_TCP)
	{
		LOG_D("AT+NSOSD=%d,%d,%s\n",client->socket,strlen(buf)/2,buf);
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NSOSD=%d,%d,%s",client->socket,strlen(buf)/2,buf)!=RT_EOK)
		{
			LOG_E("net data send fail\n");
			goto __exit;
		}
		rt_free(buf);
		return RT_EOK;
	}
	else if(client->type==QSDK_NET_TYPE_UDP)
	{
		LOG_D("AT+NSOST=%d,%s,%d,%d,%s\n",client->socket,client->server_ip,client->server_port,strlen(buf)/2,buf);
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NSOST=%d,%s,%d,%d,%s",client->socket,client->server_ip,client->server_port,strlen(buf)/2,buf)!=RT_EOK) 
		{
			LOG_E("net data send fail\n");
			goto __exit;
		}
		rt_free(buf);
		return RT_EOK;
	}
	else 
		{
			LOG_E("net client connect type fail\n");
			goto __exit;
		}
__exit:
	rt_free(buf);
	client->connect_status=NET_CONNECT_ERROR;
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
		LOG_E("net create resp buf error\n");
		return RT_ERROR;
	}
	LOG_D("AT+NSORF=%d,%d\r\n",port,len);
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NSORF=%d,%d",port,len)!=RT_EOK) 
	{
		LOG_E("net revice data send fail\n");
		rt_free(buf);
		return RT_ERROR;
	}
#ifdef QSDK_USING_DEBUD
	LOG_D("rev port=%d   rev len=%d\n",port,len);
#endif
	at_resp_parse_line_args(nb_resp,2,"\n%s",buf);
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

	char *str=rt_calloc(1,len+1);
	if(str==RT_NULL)
	{
		LOG_E("net callack create buf error\n");
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
*	函数名称：	qsdk_net_get_client_connect
*
*	函数功能：	查看客户端连接状态
*
*	入口参数：	client：NET客户端ID
*
*	返回参数：	0 已连接  1	未连接   2 发送错误
*
*	说明：		
*************************************************************/
int qsdk_net_get_client_connect(qsdk_net_client_t client)
{
	if(client->connect_status==NET_CONNECT_SUCCESS)	return RT_EOK;
	
	if(client->connect_status==NET_CONNECT_ERROR)	return 2;

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
	int i;
	if(client->connect_status==NET_CONNECT_SUCCESS||client->connect_status==NET_CONNECT_ERROR)
	{
		LOG_D("AT+NSOCL=%d\n",client->socket);
		at_obj_exec_cmd(nb_client,nb_resp,"AT+NSOCL=%d",client->socket);
	}
	//清空client信息
	for(i=0;i<QSDK_NET_CLIENT_MAX;i++)
	{
		if(&net_client_table[i]==client)
		{
			memset(&net_client_table[i],0,sizeof(net_client_table[i]));
			break;
		}	
	}
	return RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_net_clear_environment
*
*	函数功能：	清理net运行环境参数
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_net_clear_environment(void)
{	
	memset(net_client_table,0,sizeof(net_client_table));

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
				LOG_E("rev net data failure\n");
			net_client_table[i].revice_status=0;
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
#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)||(defined QSDK_USING_M5311)
		else if(rt_strstr(event,"CONNECT")!=RT_NULL)
		{
#ifdef QSDK_USING_LOG
			LOG_D("%s\n",event);
#endif
			if(rt_strstr(event,"OK")!=RT_NULL)
				rt_event_send(nb_event,EVENT_NET_CONNECT_SUCCESS);
			else if(rt_strstr(event,"FAIL")!=RT_NULL)
				rt_event_send(nb_event,EVENT_NET_CONNECT_FAILURE);
		}
#endif
}
#endif

#ifdef QSDK_USING_FINSH_CMD

void qsdk_net(int argc,char**argv)
{
	 qsdk_net_client_t net_client;

    if (argc > 1)
    {
			if (!strcmp(argv[1], "init"))
			{
					if (argc > 5)
					{
						if (!strcmp(argv[2], "tcp"))
						{
							//net client tcp 初始化
							net_client=qsdk_net_client_init(QSDK_NET_TYPE_TCP,atoi(argv[3]),argv[4],atoi(argv[5]));
						}
						else if (!strcmp(argv[2], "udp"))
						{
							//net client udp 初始化
							net_client=qsdk_net_client_init(QSDK_NET_TYPE_UDP,atoi(argv[3]),argv[4],atoi(argv[5]));
						}
						if(net_client!=RT_NULL)
						{
							uint16_t i;
								//查找client id
							for(i=0;i<QSDK_NET_CLIENT_MAX;i++)
							{
								if(&net_client_table[i]==net_client)
								{
									rt_kprintf("this net client id is:%d\n",i);
									break;
								}	
							}
							if(!strcmp(argv[2], "tcp")||!strcmp(argv[2], "udp"))
							{
								//net client 创建 socket
								if(qsdk_net_create_socket(net_client)!=RT_EOK)
									rt_kprintf("net init error\n");
//								else
//								{
//									if (!strcmp(argv[2], "tcp"))
//									{
//										if(qsdk_net_connect_to_server(net_client)!=RT_EOK)
//											rt_kprintf("connect to server fail\n");
//										rt_kprintf("net client init success\n");	
//									}
//									else if (!strcmp(argv[2], "udp"))
//									{
//										rt_kprintf("net client init success\n");		
//									}
//								}
							}
							else rt_kprintf("Network type is not udp or tcp\n");
						}
					}
					else
					{
#if (defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
							rt_kprintf("qsdk_net init <tcp or udp> <Local Port> <server ip> <server Port>  - Setting up network info,\n");
#elif (defined QSDK_USING_M5311)
							rt_kprintf("qsdk_net init <tcp or udp> <socket> <server ip> <server Port>  - Setting up network info,\n");
#endif
					}
			}
#if (defined QSDK_USING_M5311)||(defined QSDK_USING_ME3616)
			else if (!strcmp(argv[1], "set_out_format"))
			{
				if(qsdk_net_set_out_format()!=RT_EOK)
				{
					rt_kprintf("Set the output data error\n");
				}
				else rt_kprintf("Set the output data success\n");
			}
#endif
			else if (!strcmp(argv[1], "send"))
			{
					if (argc > 3)
					{
						if(net_client_table[atoi(argv[2])].user_status)
						{
							net_client=&net_client_table[atoi(argv[2])];
							if(qsdk_net_send_data(net_client,argv[3])!=RT_EOK)
								rt_kprintf("net data send error\n");
							else rt_kprintf("net data send success\n");
						}
						else rt_kprintf("This id is error\n");
					}
					else
					{
							rt_kprintf("qsdk_net send <id> <data>            -  Please enter the data you need to send.\n");
					}
			}
			else if (!strcmp(argv[1], "close"))
			{
				if (argc > 2)
				{
					if(net_client_table[atoi(argv[2])].user_status)
					{
						net_client=&net_client_table[atoi(argv[2])];
						if(qsdk_net_close_socket(net_client)!=RT_EOK)
							rt_kprintf("net client close error\r\n");
						else rt_kprintf("net client close success\r\n");
					}
					else rt_kprintf("This id is error\n");
				}
				else
				{
						rt_kprintf("qsdk_net close <id>              -  Please enter the id you need to close.\n");
				}
			}
			else if (!strcmp(argv[1], "list"))
			{
				int i;
				for(i=0;i<QSDK_NET_CLIENT_MAX;i++)
				{
#if (defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
					rt_kprintf("client:%d  user_status:%d, socket=%d, local port=%d, server ip=%s, server port=%d, type=%d\n",i,net_client_table[i].user_status,net_client_table[i].socket,net_client_table[i].port,net_client_table[i].server_ip,net_client_table[i].server_port,net_client_table[i].type);
#elif	(defined QSDK_USING_M5311)
					rt_kprintf("client:%d  user_status:%d, socket=%d,server ip=%s, server port=%d, type=%d\n",i,net_client_table[i].user_status,net_client_table[i].socket,net_client_table[i].server_ip,net_client_table[i].server_port,net_client_table[i].type);

#endif					
				}
			}
			else if (!strcmp(argv[1], "clear"))
			{
				qsdk_net_clear_environment();
				LOG_D("qsdk net client environment clear success\n");
			}
			else
			{
				rt_kprintf("Unknown command. Please enter 'qsdk_net' for help\n");
			}
		}
    else
    {
        rt_kprintf("Usage:\n");
#if (defined QSDK_USING_M5311)||(defined QSDK_USING_ME3616)
				rt_kprintf("qsdk_net set_out_format <id>                                         - Set the output data to be a string\n");
        rt_kprintf("qsdk_net init <tcp or udp> <socket> <server ip> <server Port>        - Setting up network info,\n");
#elif (defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
        rt_kprintf("qsdk_net init <tcp or udp> <Local Port> <server ip> <server Port>    - Setting up network info,\n");
#endif
				rt_kprintf("qsdk_net send <id> <data>                                            - Send data to network server\n");
				rt_kprintf("qsdk_net close <id>                                                  - close qsdk net client socket\n");
				rt_kprintf("qsdk_net list                                                        - Display a list of network clients\n");
				rt_kprintf("qsdk_net clear                                                       - clear qsdk net environment data\n");
    }	
}


MSH_CMD_EXPORT(qsdk_net, qsdk tcp/udp function);

#endif //QSDK_USING_FINSH_CMD

#endif

