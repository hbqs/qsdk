/*
 * File      : qsdk_onenet.c
 * This file is part of onenet in qsdk
 * Copyright (c) 2018-2030, Hbqs Development Team
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-11-14     longmain     first version
 * 2019-05-09     longmain     add m5311 module
 * 2019-06-18     longmain     auto init onenet environment
 */
#include "qsdk.h"
#include <at.h>
#include "string.h"
#include <stdlib.h>

#ifdef QSDK_USING_ONENET

#ifdef RT_USING_ULOG
#define LOG_TAG              "[QSDK/ONENET]"
#ifdef QSDK_USING_LOG
#define LOG_LVL              LOG_LVL_DBG
#else
#define LOG_LVL              LOG_LVL_INFO
#endif
#include <ulog.h>
#else
#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME              "[QSDK/ONENET]"
#ifdef QSDK_USING_LOG
#define DBG_LEVEL                      DBG_LOG
#else
#define DBG_LEVEL                      DBG_INFO
#endif /* QSDK_DEBUG */

#include <rtdbg.h>
#endif


//定义onenet事件
#define event_bootstrap_fail  				(1<< 1)
#define event_connect_fail						(1<< 2)
#define event_reg_success		 					(1<< 3)
#define event_reg_fail								(1<< 4)
#define event_reg_timeout							(1<< 5)
#define event_lifetime_timeout				(1<< 6)
#define event_update_success				 	(1<< 7)
#define event_update_fail				 			(1<< 8)
#define event_update_timeout				 	(1<< 9)
#define event_update_need				 			(1<<10)
#define event_unreg_done				 			(1<<11)
#define event_response_fail				 		(1<<12)
#define event_response_success		 		(1<<13)
#define event_notify_fail				 			(1<<14)
#define event_notify_success		 			(1<<15)
#define event_observer_success		 		(1<<16)
#define event_discover_success		 		(1<<17)

struct onenet_device
{
	int instance;
	int lifetime;
	int result;
	int dev_len;
	int initstep;
	int objcount;
	int inscount;
	int observercount;
	int discovercount;
	int event_status;
	int observer_status;
	int discover_status;
	int update_status;
	int update_time;
	int read_status;
	int write_status;
	int exec_status;
	int notify_status;
	int connect_status;
	int close_status;
	int notify_ack;
};


//初始化数据流结构体
static struct onenet_device onenet_device_table={0};
static struct onenet_stream onenet_stream_table[QSDK_ONENET_OBJECT_MAX_NUM]={0};

#if (defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)||(defined QSDK_USING_M5311)
//注册码生成函数预定义
static int head_node_parse(char *str);
static int init_node_parse(char *str);
static int net_node_parse(char *str);
static int sys_node_parse(char *str);
#endif
//引用AT命令部分
extern at_response_t nb_resp;
extern at_client_t nb_client;
static rt_event_t onenet_event=RT_NULL;
/*
************************************************************
*	函数名称：	qsdk_onenet_init_environment
*
*	函数功能：	初始化ONENET运行环境
*
*	入口参数：	无
*
*	返回参数：	无
*
*	说明：		
***********************************************************/
int qsdk_onenet_init_environment(void)
{
	onenet_event=rt_event_create("on_event",RT_IPC_FLAG_FIFO);
	rt_memset(&onenet_device_table,0,sizeof(onenet_device_table));		 	//清空数据流结构体
	onenet_device_table.dev_len=0;
	onenet_device_table.lifetime=QSDK_ONENET_LIFE_TIME;
	onenet_device_table.connect_status=qsdk_onenet_status_init;			 		//onenet连接状态初始化
	onenet_device_table.initstep=0;												 							//初始化步骤设置为0
	onenet_device_table.observercount=0;
	onenet_device_table.discovercount=0;
	return RT_EOK;
}

/*************************************************************
*	函数名称：	qsdk_onenet_object_init
*
*	函数功能：	初始化一个onenet平台object数据流
*
*	入口参数：	objid:    object id
*						  insid:    instance id
*						  resid: 		resource id
*						  inscount: instance 数量
*						  bitmap:   instance bitmap
*						  atts: 		attribute count (具有Read/Write操作的object有attribute)
*						  actis:    action count (具有Execute操作的object有action)
*						  type: 		数据类型
*
*	返回参数：	object指针：成功		RT_NULL:失败
*
*	说明：		
************************************************************/	
qsdk_onenet_stream_t qsdk_onenet_object_init(int objid,int insid,int resid,int inscount,char *bitmap,int atts,int acts,int type)
{
	int num=0;
	for(num=0;num<QSDK_ONENET_OBJECT_MAX_NUM&&onenet_stream_table[num].user_status;num++);
	if(num>=QSDK_ONENET_OBJECT_MAX_NUM)
	{
		LOG_E("onenet stream max error\r\n");
		return RT_NULL;
	}
	onenet_stream_table[num].objid=objid;
	onenet_stream_table[num].insid=insid;
	onenet_stream_table[num].resid=resid;
	onenet_stream_table[num].inscount=inscount;
	onenet_stream_table[num].atts=atts;
	onenet_stream_table[num].acts=acts;
	onenet_stream_table[num].valuetype=type;
	onenet_stream_table[num].user_status=1;
	onenet_device_table.dev_len++;
	strcpy(onenet_stream_table[num].bitmap,bitmap);
	return &onenet_stream_table[num];
}


/******************************************************
* 函数名称： onenet_create_instance
*
*	函数功能： 创建模组实例 instance
*
* 入口参数： config_t 设备注册码
*
* 返回值： 0 成功  1失败
*
********************************************************/
#if (defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)||(defined QSDK_USING_M5311)
static int onenet_create_instance(void)
{	

	char *str=rt_calloc(1,100);
	if(str==RT_NULL)
	{
		LOG_E("onenet create instance calloc str error\r\n");
		return RT_ERROR;
	}
	char *config_t=rt_calloc(1,120);
	if(config_t==RT_NULL)
	{
		LOG_E("onenet create instance calloc config_t error\r\n");
		rt_free(str);
		return RT_ERROR;
	}	
	
	if(head_node_parse(config_t)==RT_EOK)
	{
		if(init_node_parse(str)==RT_EOK)
		{
			if(net_node_parse(str)==RT_EOK)
			{
				if(sys_node_parse(str)!=RT_EOK)
				{
					LOG_E("onenet 平台注册码生成失败\r\n");
					goto __exit;
				}
			}
			else 
			{
				LOG_E("onenet 平台注册码生成失败\r\n");
				goto __exit;
			}
		}
		else 
			{
				LOG_E("onenet 平台注册码生成失败\r\n");
				goto __exit;
			}	
	}
	else 
			{
				LOG_E("onenet 平台注册码生成失败\r\n");
				goto __exit;
			}
	rt_sprintf(config_t,"%s%04x%s",config_t,rt_strlen(str)/2+3,str);
			
#ifdef QSDK_USING_DEBUG
	LOG_D("注册码==%s\r\n",config_t);
#endif
	LOG_D("AT+MIPLCREATE=%d,%s,0,%d,0\n",rt_strlen(config_t)/2,config_t,rt_strlen(config_t)/2);		
			
	//发送注册命令到模组
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLCREATE=%d,%s,0,%d,0",rt_strlen(config_t)/2,config_t,rt_strlen(config_t)/2)!=RT_EOK) 		
	{
		LOG_E("onenet 注册码设置失败\r\n");
		goto __exit;
	}

	//解析模组返回的响应值
	at_resp_parse_line_args(nb_resp,2,"+MIPLCREATE:%d",&onenet_device_table.instance);
	
#ifdef QSDK_USING_DEBUG	
	LOG_D("onenet create success,instance=%d\r\n",onenet_device_table.instance);
#endif

	rt_free(str);
	rt_free(config_t);

	return RT_EOK;
	
__exit:
	rt_free(str);
	rt_free(config_t);
	return RT_ERROR;
}
#elif (defined QSDK_USING_ME3616)
static int onenet_create_instance(void)
{	
	LOG_D("AT+MIPLCREATE\n");		
			
	//发送注册命令到模组
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLCREATE")!=RT_EOK) 
	{
		LOG_E("onenet 注册码设置失败\r\n");
		goto __exit;
	}

	//解析模组返回的响应值
	at_resp_parse_line_args(nb_resp,2,"+MIPLCREATE:%d",&onenet_device_table.instance);
	
#ifdef QSDK_USING_DEBUG	
	LOG_D("onenet create success,instance=%d\r\n",onenet_device_table.instance);
#endif
	return RT_EOK;
	
__exit:
	return RT_ERROR;
}
#endif
/******************************************************
* 函数名称： qsdk_onenet_delete_instance
*
*	函数功能： 删除模组实例 instance
*
* 入口参数： 无
*
* 返回值：	 0：成功   1：失败
*
********************************************************/
int qsdk_onenet_delete_instance(void)
{
	//发送删除instance 命令
	LOG_D("AT+MIPLDELETE=%d\n",onenet_device_table.instance);		

	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLDELETE=%d",onenet_device_table.instance)!=RT_EOK)
	{
		LOG_E("delete onenet instance failure\n");
		return RT_ERROR;
	}
	rt_memset(&onenet_device_table,0,sizeof(onenet_device_table));		 	//清空数据流结构体
	rt_memset(&onenet_stream_table,0,sizeof(onenet_stream_table));		 	//清空数据流结构体
#ifdef QSDK_USING_DEBUG
	LOG_D("onenet instace delete success\r\n");
#endif
	return RT_EOK;
}
/******************************************************
* 函数名称： onenet_create_object
*
*	函数功能： 添加 object 到模组
*
* 入口参数： 无
*
* 返回值： 0 成功  1失败
*
********************************************************/
static int onenet_create_object(void)
{
	int i=0,j=0,status=1;
	int str[QSDK_ONENET_OBJECT_MAX_NUM];
	onenet_device_table.objcount=0;
	//循环添加 object
	for(;i<QSDK_ONENET_OBJECT_MAX_NUM;i++)
	{			
		if(onenet_stream_table[i].user_status)
		{
			//循环查询object 并且添加到数组，用于记录，防止重复添加
			for(j=0;j<onenet_device_table.objcount;j++)
			{
				if(str[j]!=onenet_stream_table[i].objid)
					status=1;
				else 
				{
					status=0;		
					break;
				}
			}
			//添加object 到模组
			if(status)
			{	
				//记录当前object
				str[onenet_device_table.objcount++]=onenet_stream_table[i].objid;
#ifdef QSDK_USING_ME3616
				onenet_device_table.inscount+=onenet_device_table.inscount;
#else 
				onenet_device_table.inscount=onenet_device_table.dev_len;
#endif
			//向模组添加 objectid	
				LOG_D("AT+MIPLADDOBJ=%d,%d,%d,\"%s\",%d,%d\n",onenet_device_table.instance,onenet_stream_table[i].objid,\
				onenet_stream_table[i].inscount,onenet_stream_table[i].bitmap,onenet_stream_table[i].atts,onenet_stream_table[i].acts);

				if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLADDOBJ=%d,%d,%d,\"%s\",%d,%d",onenet_device_table.instance,\
					onenet_stream_table[i].objid,onenet_stream_table[i].inscount,onenet_stream_table[i].bitmap,\
					onenet_stream_table[i].atts,onenet_stream_table[i].acts)!=RT_EOK) return RT_ERROR;
#ifdef QSDK_USING_DEBUG
				LOG_D("create object success id:%d\r\n",onenet_stream_table[i].objid);
#endif
			}
		}
	}
#ifdef QSDK_USING_DEBUG
			LOG_D("object count=%d\r\n",onenet_device_table.objcount);
#endif
	
	return RT_EOK;
}
/******************************************************
* 函数名称： qsdk_onenet_delete_object
*
*	函数功能： 删除模组内已注册 object 数据流
*
* 入口参数： stream : object编号
*
* 返回值： 0：成功   1：失败
*
********************************************************/
int qsdk_onenet_delete_object(qsdk_onenet_stream_t stream)
{
	int i=0;
	//循环查询并且删除 object
	for(i=0;i<QSDK_ONENET_OBJECT_MAX_NUM;i++)
	{
		//判断当前 object 是否为要删除的object
		if(onenet_stream_table[i].objid==stream->objid)
		{
			//发送命令删除 object
			LOG_D("AT+MIPLDELOBJ=%d,%d\r\n",onenet_device_table.instance,onenet_stream_table[i].objid);

			if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLDELOBJ=%d,%d",onenet_device_table.instance,onenet_stream_table[i].objid)!=RT_EOK) return RT_ERROR;
			//删除当前 object
			rt_memset(&onenet_stream_table[i],0,sizeof(onenet_stream_table[i]));

			//数据流中设备长度-1
			onenet_device_table.dev_len-=1;
			onenet_device_table.objcount-=1;
#ifdef QSDK_USING_DEBUG
			LOG_D("delete onenet object success id:%d \r\n",stream->objid);
#endif
			return RT_EOK;
		}
	}
	return RT_ERROR;
}
#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)||	(defined QSDK_USING_M5311)
static int onenet_discover_rsp(void)
{
	char *resourcemap=RT_NULL;
	int *str_res;
	int *str_obj;
	int i=0,j=0,k=0,status=1,resourcecount=0,objectcount=0;
	resourcemap=rt_calloc(1,50);
	if(resourcemap==RT_NULL)
	{
		LOG_E("dicover rsp calloc fail\n");
		return RT_ERROR;
	}
	str_res=rt_calloc(1,QSDK_ONENET_OBJECT_MAX_NUM);
	if(str_res==RT_NULL)
	{
		LOG_E("dicover rsp calloc fail\n");
		rt_free(resourcemap);
		return RT_ERROR;
	}
	str_obj=rt_calloc(1,QSDK_ONENET_OBJECT_MAX_NUM);
	if(str_obj==RT_NULL)
	{
		LOG_E("dicover rsp calloc fail\n");
		rt_free(resourcemap);
		rt_free(str_res);
		return RT_ERROR;
	}
	//循环查找未discover object
	for(;i<QSDK_ONENET_OBJECT_MAX_NUM;i++)
	{			
		if(onenet_stream_table[i].user_status)
		{
			//循环查询object 并且添加到数组，用于记录，防止重复添加
			for(j=0;j<objectcount;j++)
			{	
				if(str_obj[j]!=onenet_stream_table[i].objid)
				{
					status=1;	
				}
				else 
				{
					status=0;		
					break;
				}
			}
			//discover object 到模组
			if(status)
			{	
				resourcecount=0;
				//记录当前object
				str_obj[objectcount++]=onenet_stream_table[i].objid;
				status=1;		
				for(k=0;k<onenet_device_table.dev_len;k++)
				{
					//判断当前objectid 是否为 discover objectid
					if(onenet_stream_table[k].objid==onenet_stream_table[i].objid)
					{
						//判断当前 resourceid 是否上报过
						for(j=0;j<resourcecount;j++)
						{
							//如果 resourceid没有上报过
							if(str_res[j]!=onenet_stream_table[k].resid)
									status=1;		//上报标识置一
							else 
							{
								status=0;
								break;
							}									
						}
						//判断该 resourceid 是否需要上报
						if(status)
						{
							//记录需要上报的 resourceid
							str_res[resourcecount++]=onenet_stream_table[k].resid;
							
							//判断本次上报的 resourceid 是否为第一次找到
							if(resourcecount-1)
								rt_sprintf(resourcemap,"%s;%d",resourcemap,onenet_stream_table[k].resid);
							else
								rt_sprintf(resourcemap,"%d",onenet_stream_table[k].resid);
						}	
					}
				}
#ifdef QSDK_USING_DEBUG
				j=0;
				//循环打印已经记录的 resourceid
				for(;j<resourcecount;j++)
				LOG_D("resourcecount=%d\r\n",str_res[j]);

				//打印 resourceid map
				LOG_D("map=%s\r\n",resourcemap);
#endif
				LOG_D("AT+MIPLDISCOVERRSP=%d,%d,1,%d,\"%s\"\n",onenet_device_table.instance,onenet_stream_table[i].objid,rt_strlen(resourcemap),resourcemap);
				if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLDISCOVERRSP=%d,%d,1,%d,\"%s\"",onenet_device_table.instance,onenet_stream_table[i].objid,rt_strlen(resourcemap),resourcemap)!=RT_EOK)
				{
					LOG_E("+MIPLDISCOVERRSP  failure \r\n");
					return RT_ERROR;
				}	
				rt_memset(str_res,0,sizeof(str_res));
				rt_memset(resourcemap,0,sizeof(resourcemap));
			}
		}
	}
	rt_free(str_obj);
	rt_free(str_res);
	rt_free(resourcemap);
	return RT_EOK;
}
#endif
/****************************************************
* 函数名称： qsdk_onenet_open
*
* 函数作用： 设备登录到 onenet 平台
*
* 入口参数： 无
*
* 返回值： 0：成功   1：失败
*****************************************************/
int qsdk_onenet_open(void)
{
	rt_uint32_t status;
	onenet_device_table.lifetime=QSDK_ONENET_LIFE_TIME;
	
#ifndef QSDK_USING_ME3616
	if(onenet_create_object()!=RT_EOK)							//创建 object
	{
		LOG_E("onenet add objece failure\n");
		return RT_ERROR;
	}
#endif 
	
	onenet_device_table.initstep=2;
#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)||	(defined QSDK_USING_M5311)
	if(onenet_discover_rsp()!=RT_EOK)						// notify 设备参数到模组
	{
		LOG_E("onenet add discover failure\n");
		return RT_ERROR;
	}
	onenet_device_table.initstep=3;
#endif
	LOG_D("AT+MIPLOPEN=%d,%d\n",onenet_device_table.instance,onenet_device_table.lifetime);
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLOPEN=%d,%d",onenet_device_table.instance,onenet_device_table.lifetime)!=RT_EOK) 
	{
		LOG_E("onener open failure\r\n");
		return RT_ERROR;
	}
#ifdef QSDK_USING_DEBUG
	LOG_D("onenet open success\r\n");
#endif
	onenet_device_table.initstep=4;
	onenet_device_table.event_status=qsdk_onenet_status_init;	
	if(rt_event_recv(onenet_event,event_bootstrap_fail|event_connect_fail|event_reg_fail|event_reg_success|event_reg_timeout,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,90000,&status)!=RT_EOK)
	{
		LOG_E("onenet wait reg timeout\r\n");
		return RT_ERROR;
	}
	if(status==event_bootstrap_fail||status==event_connect_fail||status==event_reg_fail) 
	{
		LOG_E("onenet reg failure\r\n");
		return RT_ERROR;
	}
	if(status==event_reg_timeout) 
	{
		LOG_E("onenet 平台登陆超时，设备未在平台注册或者在平台填写了Auth_Code，需要清空 Auth_Code 信息");
		return RT_ERROR;
	}
#ifdef QSDK_USING_DEBUG
		LOG_D("onenet reg success\r\n");
#endif
		onenet_device_table.initstep=5;
	if(rt_event_recv(onenet_event,event_observer_success,RT_EVENT_FLAG_AND|RT_EVENT_FLAG_CLEAR,60000,&status)!=RT_EOK)
	{
		LOG_E("wait onenet observe event timeout\r\n");
		return RT_ERROR;
	}
#ifdef QSDK_USING_DEBUG
		LOG_D("onenet observe success\r\n");
#endif
	onenet_device_table.initstep=6;
	if(rt_event_recv(onenet_event,event_discover_success,RT_EVENT_FLAG_AND|RT_EVENT_FLAG_CLEAR,60000,&status)!=RT_EOK)
	{
		LOG_E("wait onenet discover event timeout,请在onenet平台LEM2M产品->设备管理查看<自动发现资源>功能是否已打开\n");
		return RT_ERROR;
	}
#ifdef QSDK_USING_DEBUG
		LOG_D("onenet discover success\r\n");
#endif
	if(onenet_device_table.discover_status==qsdk_onenet_status_success)						//判断	discover 是否初始化成功
	{
		LOG_D("onenet lwm2m connect success\r\n");
		onenet_device_table.initstep=13;
		return RT_EOK;
	}
	else 
	{
		LOG_E("discover failure\r\n");	
		return RT_ERROR;
	}	
}
/****************************************************
* 函数名称： qsdk_onenet_close
*
* 函数作用： 在onenet 平台注销设备
*
* 入口参数： 无
*
* 返回值： 0：成功   1：失败
*****************************************************/
int qsdk_onenet_close(void)
{
	rt_uint32_t status;
	onenet_device_table.close_status=qsdk_onenet_status_close_start;
	LOG_D("AT+MIPLCLOSE=%d\n",onenet_device_table.instance);
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLCLOSE=%d",onenet_device_table.instance)!=RT_EOK)
	{
		LOG_E("close onenet instance failure\r\n");
		return RT_ERROR;
	}
	//等待平台返回EVENT：15事件
	if(rt_event_recv(onenet_event,event_unreg_done,RT_EVENT_FLAG_AND|RT_EVENT_FLAG_CLEAR,20000,&status)!=RT_EOK)
	{
		LOG_E("close onenet instance failure\r\n");
		return RT_ERROR;
	}
#ifdef QSDK_USING_DEBUG
	LOG_D("close onenet instance success\r\n");
#endif
	onenet_device_table.connect_status=qsdk_onenet_status_init;
	return RT_EOK;
}
/****************************************************
* 函数名称： qsdk_onenet_update_time
*
* 函数作用： 更新onenet 设备维持时间
*
* 入口参数： flge :是否同时更新云端object 信息
*
* 返回值： 0：成功   1：失败
*****************************************************/
int qsdk_onenet_update_time(int flge)
{
	rt_uint32_t status;
	//向平台发送更新命令
	onenet_device_table.update_time=qsdk_onenet_status_update_init;
	LOG_D("AT+MIPLUPDATE=%d,%d,%d\n",onenet_device_table.instance,onenet_device_table.lifetime,flge);
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLUPDATE=%d,%d,%d",onenet_device_table.instance,onenet_device_table.lifetime,flge)!=RT_EOK)
	{
		return  RT_ERROR;
	}
	//等待平台返回event事件
	if(rt_event_recv(onenet_event,event_lifetime_timeout|event_update_fail|event_update_timeout|event_update_success,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,20000,&status)!=RT_EOK)
	{
		goto __exit;
	}
	//判断是否更新成功
	if(status==event_lifetime_timeout||status==event_update_fail||status==event_update_timeout)
	{
		goto __exit;
	}
#ifdef QSDK_USING_DEBUG
	LOG_D("onenet update time success\r\n");
#endif
	return RT_EOK;
	
__exit:
	LOG_E("onenet update time failure\r\n");
	return  RT_ERROR;
}

/*************************************************************
*	函数名称：	qsdk_onenet_quick_start
*
*	函数功能：	一键连接到onenet平台
*
*	入口参数：	无
*
*	返回参数：	0：成功   1：失败
*
*	说明：		
************************************************************/	
int qsdk_onenet_quick_start(void)
{	
	if(onenet_create_instance()!=RT_EOK)	 //创建 instance
	{
		LOG_E("onenet create instance failure\n");
		goto failure;
	}
	onenet_device_table.initstep=1;
	
#ifdef QSDK_USING_ME3616
	if(onenet_create_object()!=RT_EOK)							//创建 object
	{
		LOG_E("onenet add objece failure\n");
		return RT_ERROR;
	}
#endif 

	if(qsdk_onenet_open()!=RT_EOK)	goto failure;							// 执行 onenet 登录函数

	return RT_EOK;

//错误处理函数
	failure:
#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)||(defined QSDK_USING_M5311)		
	if(onenet_device_table.initstep>5)				//如果错误值大于 5 将关闭设备 instance
#endif
#ifdef QSDK_USING_ME3616
	if(onenet_device_table.initstep>3)				//如果错误值大于 5 将关闭设备 instance
#endif
	{
		qsdk_onenet_close();		
	}								
	return RT_ERROR;	
}

/****************************************************
* 函数名称： qsdk_onenet_notify
*
* 函数作用： notify 设备数据到平台（无ACK）
*
* 入口参数： stream：object结构体		len：消息长度		data：消息内容		flge：消息标识		
*
* 返回值： 0 成功	1失败
*****************************************************/
int qsdk_onenet_notify(qsdk_onenet_stream_t stream,int len,qsdk_onenet_value_t data,int flge)
{
	if(stream->valuetype==qsdk_onenet_value_string||stream->valuetype==qsdk_onenet_value_opaque||stream->valuetype==qsdk_onenet_value_hexStr)
	{
		LOG_D("AT+MIPLNOTIFY=%d,%d,%d,%d,%d,%d,%d,\"%s\",0,%d\n",onenet_device_table.instance,stream->msgid,stream->objid,\
			stream->insid,stream->resid,stream->valuetype,len,&data->string_value,flge);

		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLNOTIFY=%d,%d,%d,%d,%d,%d,%d,\"%s\",0,%d",onenet_device_table.instance,\
			stream->msgid,stream->objid,stream->insid,stream->resid,stream->valuetype,len,&data->string_value,flge)!=RT_EOK)  
		goto __exit;
	}
	else if(stream->valuetype==qsdk_onenet_value_integer)	
	{
		LOG_D("AT+MIPLNOTIFY=%d,%d,%d,%d,%d,%d,1,\"%d\",0,%d\n",onenet_device_table.instance,stream->msgid,stream->objid,\
			stream->insid,stream->resid,stream->valuetype,data->int_value,flge);

		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLNOTIFY=%d,%d,%d,%d,%d,%d,1,\"%d\",0,%d",onenet_device_table.instance,\
			stream->msgid,stream->objid,stream->insid,stream->resid,stream->valuetype,data->int_value,flge)!=RT_EOK)  
		goto __exit;	
	}				
	else if(stream->valuetype==qsdk_onenet_value_float)
	{	
		LOG_D("AT+MIPLNOTIFY=%d,%d,%d,%d,%d,%d,1,\"%f\",0,%d\n",onenet_device_table.instance,stream->msgid,stream->objid,\
				stream->insid,stream->resid,stream->valuetype,data->float_value,flge);
	
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLNOTIFY=%d,%d,%d,%d,%d,%d,1,\"%f\",0,%d",onenet_device_table.instance,\
			stream->msgid,stream->objid,stream->insid,stream->resid,stream->valuetype,data->float_value,flge)!=RT_EOK)  
		goto __exit;
	}				
	else if(stream->valuetype==qsdk_onenet_value_bool)
	{
		LOG_D("AT+MIPLNOTIFY=%d,%d,%d,%d,%d,%d,1,\"%d\",0,%d\n",onenet_device_table.instance,stream->msgid,stream->objid,\
			stream->insid,stream->resid,stream->valuetype,data->bool_value,flge);			

		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLNOTIFY=%d,%d,%d,%d,%d,%d,1,\"%d\",0,%d",onenet_device_table.instance,\
			stream->msgid,stream->objid,stream->insid,stream->resid,stream->valuetype,data->bool_value,flge)!=RT_EOK)   
		goto __exit;		
	}

	return RT_EOK;	
	
__exit:
	LOG_E("nb-iot notify data error\r\n");
	return RT_ERROR;
}
/****************************************************
* 函数名称： qsdk_onenet_get_connect
*
* 函数作用： 获取onenet 平台连接状态
*
* 入口参数： 无
*
* 返回值： 0：连接成功	     1：连接已断开
*****************************************************/
int qsdk_onenet_get_connect(void)
{
	if(onenet_device_table.connect_status==qsdk_onenet_status_success)
		return RT_EOK;

		return RT_ERROR;
}
/****************************************************
* 函数名称： qsdk_onenet_get_object_read
*
* 函数作用： 查询当前object 是否收到平台read消息
*
* 入口参数： stream：object结构体
*
* 返回值： 0：收到	  1：没收到
*****************************************************/
int qsdk_onenet_get_object_read(qsdk_onenet_stream_t stream)
{
	if(stream->read_status==10)	return RT_EOK;
		
	return RT_ERROR;
}

/****************************************************
* 函数名称： qsdk_onenet_get_object_write
*
* 函数作用： 查询当前object 是否收到平台write消息
*
* 入口参数： stream：object结构体
*
* 返回值： 0：收到	  1：没收到
*****************************************************/
int qsdk_onenet_get_object_write(qsdk_onenet_stream_t stream)
{
	if(stream->write_status==10)	return RT_EOK;
		
	return RT_ERROR;
}
/****************************************************
* 函数名称： qsdk_onenet_get_object_exec
*
* 函数作用： 查询当前object 是否收到平台exec消息
*
* 入口参数： object：object分组	
*
* 返回值： 0：收到	  1：没收到
*****************************************************/
int qsdk_onenet_get_object_exec(qsdk_onenet_stream_t stream)
{
	if(stream->exec_status==10)	return RT_EOK;
		
	return RT_ERROR;
}
/****************************************************
* 函数名称： qsdk_onenet_read_rsp
*
* 函数作用： 响应onenet read 操作
*
* 入口参数： msgid：消息ID		
*
*						 result：读写状态
*	
*						 stream：object 结构体
*
*						 len：上报的消息长度
*
*						 data：上报的消息值			
*
*						 index：指令序列号
*
*						 flge: 消息标识
*
* 返回值： 0：成功   1：失败
*****************************************************/
int qsdk_onenet_read_rsp(int msgid,int result,qsdk_onenet_stream_t stream,int len,qsdk_onenet_value_t data,int index,int flge)
{
		//返回 onenet read 响应
		if(stream->valuetype==qsdk_onenet_value_string||stream->valuetype==qsdk_onenet_value_opaque||stream->valuetype==qsdk_onenet_value_hexStr)
		{
			LOG_D("AT+MIPLREADRSP=%d,%d,%d,%d,%d,%d,%d,%d,\"%s\",%d,%d\n",onenet_device_table.instance,msgid,result,stream->objid,\
								 stream->insid,stream->resid,stream->valuetype,len,&data->string_value,index,flge);
		
			if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLREADRSP=%d,%d,%d,%d,%d,%d,%d,%d,\"%s\",%d,%d",onenet_device_table.instance,msgid,result,stream->objid,\
								 stream->insid,stream->resid,stream->valuetype,len,&data->string_value,index,flge)!=RT_EOK) 
			{
				return RT_ERROR;
			}
		}
		else if(stream->valuetype==qsdk_onenet_value_integer)
		{
			LOG_D("AT+MIPLREADRSP=%d,%d,%d,%d,%d,%d,%d,1,\"%d\",%d,%d\n",onenet_device_table.instance,msgid,result,stream->objid,\
								 stream->insid,stream->resid,stream->valuetype,data->int_value,index,flge);			
		
			if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLREADRSP=%d,%d,%d,%d,%d,%d,%d,1,\"%d\",%d,%d",onenet_device_table.instance,msgid,result,stream->objid,\
								 stream->insid,stream->resid,stream->valuetype,data->int_value,index,flge)!=RT_EOK) 
			{
				return RT_ERROR;
			}
		}
		else if(stream->valuetype==qsdk_onenet_value_float)
		{
			LOG_D("AT+MIPLREADRSP=%d,%d,%d,%d,%d,%d,%d,1,\"%f\",%d,%d\n",onenet_device_table.instance,msgid,result,stream->objid,\
								 stream->insid,stream->resid,stream->valuetype,data->float_value,index,flge);
		
			if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLREADRSP=%d,%d,%d,%d,%d,%d,%d,1,\"%f\",%d,%d",onenet_device_table.instance,msgid,result,stream->objid,\
								 stream->insid,stream->resid,stream->valuetype,data->float_value,index,flge)!=RT_EOK)  
			{
				return RT_ERROR;
			}
		}
		else if(stream->valuetype==qsdk_onenet_value_bool)
		{
			LOG_D("AT+MIPLREADRSP=%d,%d,%d,%d,%d,%d,%d,1,\"%d\",%d,%d\n",onenet_device_table.instance,msgid,result,stream->objid,\
								 stream->insid,stream->resid,stream->valuetype,data->bool_value,index,flge);
			
			if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLREADRSP=%d,%d,%d,%d,%d,%d,%d,1,\"%d\",%d,%d",onenet_device_table.instance,msgid,result,stream->objid,\
								 stream->insid,stream->resid,stream->valuetype,data->bool_value,index,flge)!=RT_EOK) 
			{
				return RT_ERROR;
			}
		}
		stream->read_status=0;
		return RT_EOK;
}

/****************************************************
* 函数名称： onenet_read_rsp
*
* 函数作用： onenet read 响应
*
* 入口参数： msgid	消息ID			
*	
*							objid	即将操作的objectid
*
*							insid	即将操作的 instanceid
*
*							resid 即将操作的 instanceid							
*
* 返回值： 0：成功   1：失败
*****************************************************/
static int onenet_read_rsp(int msgid,int objid,int insid,int resid)
{
	int i=0;
	
	if(insid == -1&&resid == -1)
	{
		//循环检测objectid
		for(;i<onenet_device_table.dev_len;i++)
		{
			//判断当前objectid 是否为即将要读的信息
			if(onenet_stream_table[i].objid==objid)
			{
				onenet_stream_table[i].read_status=10;
			}
		}
	}
	else if(resid == -1)
	{
		//循环检测objectid
		for(;i<onenet_device_table.dev_len;i++)
		{
			//判断当前objectid 是否为即将要读的信息
			if(onenet_stream_table[i].objid==objid&&onenet_stream_table[i].insid==insid)
			{
				onenet_stream_table[i].read_status=10;
			}
		}
	}
	else
	{
		//循环检测objectid
		for(;i<onenet_device_table.dev_len;i++)
		{
			//判断当前objectid 是否为即将要读的信息
			if(onenet_stream_table[i].objid==objid&&onenet_stream_table[i].insid==insid&&onenet_stream_table[i].resid==resid)
			{
				onenet_stream_table[i].read_status=10;
			}
		}
	}

	//进入读回调函数
		if(qsdk_onenet_read_rsp_callback(msgid,insid,resid)==RT_EOK)
		{
#ifdef QSDK_USING_DEBUG
			LOG_D("onenet read rsp success\n");
#endif
			return RT_EOK;
		}
	// read 回调函数处理失败操作
		LOG_E("onenet read rsp failure\n");
		return  RT_ERROR;
}
/****************************************************
* 函数名称： onenet_write_rsp
*
* 函数作用： onenet write 响应
*
* 入口参数： msgid：消息ID			
*	
*						 objid：即将操作的objectid
*
*						 insid：即将操作的 instanceid
*
*						 resid：即将操作的 instanceid		
*
*						 valuetype：write值类型	
*
*						 len：write值长度	
*
*						 value：write值	
*
* 返回值：0：成功   1：失败
*****************************************************/
static int onenet_write_rsp(int msgid,int objid,int insid,int resid,int valuetype,int len,char* value)
{
	int i=0,result=qsdk_onenet_status_result_Not_Found;;
		//循环检测objectid
	for(;i<onenet_device_table.dev_len;i++)
	{
		//判断当前objectid 是否为即将要读的信息
		if(onenet_stream_table[i].objid==objid&&onenet_stream_table[i].insid==insid&&onenet_stream_table[i].resid==resid)
		{
			onenet_stream_table[i].write_status=10;
			//进入 write 回调函数
			if(qsdk_onenet_write_rsp_callback(len,value)==RT_EOK)
			{
				result=qsdk_onenet_status_result_write_success;
#ifdef QSDK_USING_DEBUG
				LOG_D("onenet write rsp success\r\n");
#endif
			}
			else	//write回调函数处理失败操作
			{
				result=qsdk_onenet_status_result_Bad_Request;
				LOG_E("onenet write rsp failure\n");
			}
			onenet_stream_table[i].write_status=0;
			LOG_D("AT+MIPLWRITERSP=%d,%d,%d\n",onenet_device_table.instance,msgid,result);
			if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLWRITERSP=%d,%d,%d",onenet_device_table.instance,msgid,result)!=RT_EOK) return RT_ERROR;
			return RT_EOK;
		}
	}
	//当前 objectid 不在列表中
		return RT_ERROR;	
}
/****************************************************
* 函数名称： onenet_execute_rsp
*
* 函数作用： onenet execute 响应
*
* 入口参数： msgid	消息ID			
*	
*							objid：即将操作的objectid
*
*							insid：即将操作的 instanceid
*
*							resid：即将操作的 instanceid		
*
*							len：cmd值长度	
*
*							cmd：本次cmd值	
*
* 返回值： 0：成功   1：失败
*****************************************************/
static int onenet_execute_rsp(int msgid,int objid,int insid,int resid,int len,char *cmd)
{
	int i=0,result=qsdk_onenet_status_result_Not_Found;
	
	for(;i<onenet_device_table.dev_len;i++)
	{
		//判断当前objectid 是否为即将要读的信息
		if(onenet_stream_table[i].objid==objid&&onenet_stream_table[i].insid==insid&&onenet_stream_table[i].resid==resid)
		{
			onenet_stream_table[i].exec_status=10;
			//进入 execute 回调函数
			if(qsdk_onenet_exec_rsp_callback(len,cmd)==RT_EOK)
			{
				result=qsdk_onenet_status_result_write_success;
#ifdef QSDK_USING_DEBUG
				LOG_D("onenet execute rsp success\n");
#endif
			}
			else	//execute回调函数处理失败操作
			{
				result=qsdk_onenet_status_result_Bad_Request;
				LOG_E("onenet execute rsp failure\n");
			}
			onenet_stream_table[i].exec_status=0;
			LOG_D("AT+MIPLEXECUTERSP=%d,%d,%d\n",onenet_device_table.instance,msgid,result);
			if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLEXECUTERSP=%d,%d,%d",onenet_device_table.instance,msgid,result)!=RT_EOK) return RT_ERROR;	
			
			return RT_EOK;	
		}
	}
	//返回 object 不在列表中
	return RT_ERROR;
}

int qsdk_rsp_onenet_parameter(int instance,int msgid,int result)
{


		return 0;
}


/*************************************************************
*	函数名称：	qsdk_onenet_clear_environment
*
*	函数功能：	清理onenet运行环境
*
*	入口参数：	无
*
*	返回参数：	0：成功  1：失败
*
*	说明：		
*************************************************************/
int qsdk_onenet_clear_environment(void)
{	
	rt_memset(&onenet_device_table,0,sizeof(onenet_device_table));		 	//清空数据流结构体
	rt_memset(&onenet_stream_table,0,sizeof(onenet_stream_table));		 	//清空数据流结构体

	return RT_EOK;
}
#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)||	(defined QSDK_USING_M5311)
/****************************************************
* 函数名称： qsdk_onenet_notify_and_ack
*
* 函数作用： notify 数据到onenet平台（带ACK）
*
* 入口参数： stream：object结构体		len：消息长度		data：消息内容		flge：消息标识	
*
* 返回值： 0 成功	1失败
*****************************************************/
int qsdk_onenet_notify_and_ack(qsdk_onenet_stream_t stream,int len,qsdk_onenet_value_t data,int flge)
{
	rt_uint32_t status;
	//发送 AT 命令上报数据到 onenet 平台
	if(stream->valuetype==qsdk_onenet_value_string||stream->valuetype==qsdk_onenet_value_opaque||stream->valuetype==qsdk_onenet_value_hexStr)
	{
		LOG_D("AT+MIPLNOTIFY=%d,%d,%d,%d,%d,%d,%d,\"%s\",0,%d,%d\n",onenet_device_table.instance,stream->msgid,stream->objid,\
			stream->insid,stream->resid,stream->valuetype,len,&data->string_value,flge,onenet_device_table.notify_ack);

		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLNOTIFY=%d,%d,%d,%d,%d,%d,%d,\"%s\",0,%d,%d",onenet_device_table.instance,\
			stream->msgid,stream->objid,stream->insid,stream->resid,stream->valuetype,len,&data->string_value,flge,\
			onenet_device_table.notify_ack)!=RT_EOK)  
		return RT_ERROR;			
	}		
	else if(stream->valuetype==qsdk_onenet_value_integer)
	{
		LOG_D("AT+MIPLNOTIFY=%d,%d,%d,%d,%d,%d,1,\"%d\",0,%d,%d\n",onenet_device_table.instance,stream->msgid,stream->objid,\
			stream->insid,stream->resid,stream->valuetype,data->int_value,flge,onenet_device_table.notify_ack);

		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLNOTIFY=%d,%d,%d,%d,%d,%d,1,\"%d\",0,%d,%d",onenet_device_table.instance,\
			stream->msgid,stream->objid,stream->insid,stream->resid,stream->valuetype,data->int_value,flge,\
			onenet_device_table.notify_ack)!=RT_EOK)  
		return RT_ERROR;							
	}
	else if(stream->valuetype==qsdk_onenet_value_float)
	{
		LOG_D("AT+MIPLNOTIFY=%d,%d,%d,%d,%d,%d,1,\"%f\",0,%d,%d\n",onenet_device_table.instance,stream->msgid,stream->objid,\
			stream->insid,stream->resid,stream->valuetype,data->float_value,flge,onenet_device_table.notify_ack);

		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLNOTIFY=%d,%d,%d,%d,%d,%d,1,\"%f\",0,%d,%d",onenet_device_table.instance,\
			stream->msgid,stream->objid,stream->insid,stream->resid,stream->valuetype,data->float_value,flge,onenet_device_table.notify_ack)!=RT_EOK)  
		return RT_ERROR;			
	}
	else if(stream->valuetype==qsdk_onenet_value_bool)
	{
		LOG_D("AT+MIPLNOTIFY=%d,%d,%d,%d,%d,%d,1,\"%d\",0,%d,%d\n",onenet_device_table.instance,stream->msgid,stream->objid,\
			stream->insid,stream->resid,stream->valuetype,data->bool_value,flge,onenet_device_table.notify_ack);			

		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLNOTIFY=%d,%d,%d,%d,%d,%d,1,\"%d\",0,%d,%d",onenet_device_table.instance,\
			stream->msgid,stream->objid,stream->insid,stream->resid,stream->valuetype,data->bool_value,flge,onenet_device_table.notify_ack)!=RT_EOK)   
		return RT_ERROR;
	}
		
	//等待模组返回 ACK响应
	if(rt_event_recv(onenet_event,event_notify_fail|event_notify_success,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,20000,&status)!=RT_EOK)
	{
	
	}
	if(status==event_notify_fail)
	{
		LOG_E("ack notify to onenet failure, objectid:%d 	instanceid:%d 	resourceid:%d		msgid:%d\n",stream->objid,\
		stream->insid,stream->resid,stream->msgid);
		onenet_device_table.notify_status=qsdk_onenet_status_init;
		return RT_ERROR;
	}		
	
#ifdef QSDK_USING_DEBUG 
	LOG_D("ack notify to onenet success, objectid:%d 	instanceid:%d 	resourceid:%d		msgid:%d\n",stream->objid,stream->insid,stream->resid,stream->msgid);
#endif
	//将当前设备数据流的上报状态设为初始化
	onenet_device_table.notify_status=qsdk_onenet_status_init;
	// ACK 增加一次
	onenet_device_table.notify_ack++;						
	//当ACK超过5000 时，清空一次
	if(onenet_device_table.notify_ack>5000)
		onenet_device_table.notify_ack=1;
	return RT_EOK;
}

/****************************************************
* 函数名称： head_node_parse
*
* 函数作用： 生成注册码协议 head 部分
*
* 入口参数： str:生成的协议文件
*
* 返回值： 0 成功	1失败
*****************************************************/
static int head_node_parse(char *str)
{
	int ver=1;
	int config=3;
	rt_sprintf(str,"%d%d",ver,config);
	
	return RT_EOK;
}
/****************************************************
* 函数名称： init_node_parse
*
* 函数作用： 生成注册码协议 init 部分
*
* 入口参数： str:生成的协议文件
*
* 返回值： 0 成功	1失败
*****************************************************/
static int init_node_parse(char *str)
{
	char *head="F";
	int config=1;
	int size=3;
	rt_sprintf(str+strlen(str),"%s%d%04x",head,config,size);
	
	return RT_EOK;
}
/****************************************************
* 函数名称： net_node_parse
*
* 函数作用： 生成注册码协议 net 部分
*
* 入口参数： str:生成的协议文件
*
* 返回值： 0 成功	1失败
*****************************************************/
static int net_node_parse(char *str)
{
	char *head="F";
	int config=2;
#ifdef QSDK_USING_ME3616
	int mtu_size=1280;
#else
	int mtu_size=1024;
#endif
	int link_t=1;
	int band_t=1;
	int boot_t=00;
	int apn_len=0;
	int apn=0;
	int user_name_len=0;
	int user_name=0;
	int passwd_len=0;
	int passwd=0;
	int host_len;
	
#ifdef QSDK_USING_ME3616
	char *user_data="4e554c4c";
#else
	char *user_data="31";
#endif
	int user_data_len=strlen(user_data)/2;
	
	char *host=rt_calloc(1,38);
	if(host==RT_NULL)
	{
		LOG_E("net_node_parse calloc host error\r\n");
		return RT_ERROR;
	}	
	char *buffer=rt_calloc(1,100);
	if(buffer==RT_NULL)
	{
		LOG_E("net_node_parse calloc buffer error\r\n");
		rt_free(host);
		return RT_ERROR;
	}
#ifdef QSDK_USING_ME3616	
	string_to_hex(QSDK_ONENET_ADDRESS,strlen(QSDK_ONENET_ADDRESS),host);
#else
	rt_sprintf(buffer,"%s:%s",QSDK_ONENET_ADDRESS,QSDK_ONENET_PORT);
	string_to_hex(buffer,strlen(buffer),host);
#endif
	host_len=strlen(host)/2;
	rt_sprintf(buffer,"%04x%x%x%02x%02x%02x%02x%02x%02x%02x%02x%s%04x%s",mtu_size,link_t,band_t,boot_t,apn_len,apn,user_name_len,user_name,passwd_len,passwd,host_len,host,user_data_len,user_data);
	rt_sprintf(str+strlen(str),"%s%d%04x%s",head,config,strlen(buffer)/2+3,buffer);
	
	rt_free(host);
	rt_free(buffer);
	return RT_EOK;
	
}
/****************************************************
* 函数名称： sys_node_parse
*
* 函数作用： 生成注册码协议 sys 部分
*
* 入口参数： str:生成的协议文件
*
* 返回值： 0 成功	1失败
*****************************************************/
static int sys_node_parse(char *str)
{
	char *head="F";
	int config=3;
#ifdef QSDK_USING_ME3616
	char *debug="ea0400";
#else
	char *debug="C00000";
#endif
	
#ifdef QSDK_USING_ME3616
	char *user_data="4e554c4c";
	int user_data_len=strlen(user_data)/2;
#else
	char *user_data="00";
	int user_data_len=0;
#endif
	char *buffer=rt_calloc(1,20);
	if(buffer==RT_NULL)
	{
		LOG_E("net_node_parse calloc buffer error\r\n");
		return RT_ERROR;
	}
	if(user_data_len)
	rt_sprintf(buffer,"%s%04x%s",debug,user_data_len,user_data);
	else
	rt_sprintf(buffer,"%s%04x",debug,user_data_len);
	rt_sprintf(str+strlen(str),"%s%x%04x%s",head,config,strlen(buffer)/2+3,buffer);
	rt_free(buffer);
	return 0;
	
}
#endif
/****************************************************
* 函数名称： onenet_event_func
*
* 函数作用： 模块下发数据处理
*
* 返回值： 无
*****************************************************/
void onenet_event_func(char *event)
{
	char *result=RT_NULL;
	char *instance=RT_NULL;
	char *msgid=RT_NULL;
	char *objectid=RT_NULL;
	char *instanceid=RT_NULL;
	char *resourceid=RT_NULL;
	rt_err_t status=RT_EOK;
	
//判断是否为事件消息
	if(rt_strstr(event,"+MIPLEVENT:")!=RT_NULL)
	{
		char *eventid=RT_NULL;
		LOG_D("%s\r\n ",event);
		result=strtok(event,":");
		instance=strtok(NULL,",");
		eventid=strtok(NULL,",");
#ifdef QSDK_USING_DEBUG
		LOG_D("instance=%d,eventid=%d\r\n",atoi(instance),atoi(eventid));
#endif
		//进入事件处理函数
		switch(atoi(eventid))
		{
			case  1:
#ifdef QSDK_USING_DEBUG 
					LOG_D("Bootstrap start    \r\n");
#endif
					onenet_device_table.event_status=qsdk_onenet_status_run;			
				break;
			case  2:
#ifdef QSDK_USING_DEBUG 
					LOG_D("Bootstrap success  \r\n"); 
#endif
					onenet_device_table.event_status=qsdk_onenet_status_run;			 
				break;
			case  3:
					LOG_E("Bootstrap failure/连接LWM2M引导机失败\n");	
					onenet_device_table.event_status=qsdk_onenet_status_failure;
					rt_event_send(onenet_event,event_bootstrap_fail);
				break;
			case  4:
#ifdef QSDK_USING_DEBUG 
					LOG_D("Connect success\n");
#endif
					onenet_device_table.event_status=qsdk_onenet_status_run;			
				break;
			case  5:
					LOG_E("Connect failure\n");
					onenet_device_table.event_status=qsdk_onenet_status_failure;
					rt_event_send(onenet_event,event_connect_fail);
				break;
			case  6:
#ifdef QSDK_USING_DEBUG 
					LOG_D("Reg onenet success\n"); 
#endif
					onenet_device_table.event_status=qsdk_onenet_status_success;
					rt_event_send(onenet_event,event_reg_success);			
				break;
			case  7:
					if(onenet_device_table.event_status!=qsdk_onenet_status_failure)
					{
						LOG_E("onenet 平台登陆失败  设备未在平台注册或者在平台填写了Auth_Code，需要清空 Auth_Code 信息\n");
						onenet_device_table.event_status=qsdk_onenet_status_failure;
						rt_event_send(onenet_event,event_reg_fail);	
					}						
				break;
			case  8:
#ifdef QSDK_USING_DEBUG 
					LOG_D("Reg onenet timeout\n");
#endif
					onenet_device_table.event_status=qsdk_onenet_status_failure;	
					rt_event_send(onenet_event,event_reg_timeout);	
				break;
			case  9:
#ifdef QSDK_USING_DEBUG 
					LOG_D("Life_time timeout\n");
#endif			
				break;
			case 10:
#ifdef QSDK_USING_DEBUG 
					LOG_D("Status halt\n"); 
#endif
				break;
			case 11:
#ifdef QSDK_USING_DEBUG 
					LOG_D("Update success\n");
#endif			
					onenet_device_table.update_time=qsdk_onenet_status_update_success;
					rt_event_send(onenet_event,event_update_success);
				break;
			case 12:
					LOG_E("Update failure\n"); 
					onenet_device_table.update_time=qsdk_onenet_status_update_failure;
					rt_event_send(onenet_event,event_update_fail);
				break;
			case 13:
					LOG_E("Update timeout\n"); 
					onenet_device_table.update_time=qsdk_onenet_status_update_timeout;
					rt_event_send(onenet_event,event_update_timeout);
				break;
			case 14:
#ifdef QSDK_USING_DEBUG 
					LOG_D("Update need\n");
#endif
					onenet_device_table.update_time=qsdk_onenet_status_update_need;
				break;
			case 15:
#ifdef QSDK_USING_DEBUG 
					LOG_D("Unreg success\n"); 
#endif
					onenet_device_table.connect_status=qsdk_onenet_status_failure;
					if(onenet_device_table.close_status==qsdk_onenet_status_close_start)
					{
						onenet_device_table.close_status=qsdk_onenet_status_close_init;
						rt_event_send(onenet_event,event_unreg_done);
					}
					else
					{
						if(qsdk_onenet_close_callback()!=RT_EOK)
						{
							LOG_E("close onenet instance failure\n");
						}
					}	
				break;
			case 20:
					LOG_E("Response failure\n");
				break;
			case 21:
#ifdef QSDK_USING_DEBUG 
					LOG_D("Response success\n"); 
#endif
				break;
			case 25:
					LOG_E("Notify failure\n"); 
					onenet_device_table.notify_status=qsdk_onenet_status_failure;
					rt_event_send(onenet_event,event_notify_fail);
				break;
			case 26:
#ifdef QSDK_USING_DEBUG 
					LOG_D("Notify success\n"); 
#endif
					onenet_device_table.notify_status=qsdk_onenet_status_success;
					rt_event_send(onenet_event,event_notify_success);
				break;
			case 40:
#ifdef QSDK_USING_DEBUG 
					LOG_D("enter onenet fota down start\n"); 
#endif
				break;
			case 41:
					LOG_E("onenet fota down fail\n"); 
				break;
			case 42:
#ifdef QSDK_USING_DEBUG 
					LOG_D("onenet fota down success\n"); 
#endif
				break;
			case 43:
					LOG_D("onenet enter fotaing \n");
			
				break;
			case 44:
					LOG_D("onenet fota success\n"); 
				break;
			case 45:
					LOG_E("onenet fota failure\n"); 
				break;				
			case 46:
					LOG_D("onenet fota update success\n"); 
				break;
			case 47:
					LOG_E("onenet fota event interrupt failure\n"); 
				break;
			default:break;
		}
	}
	//判断是否为 onenet 平台 read 事件
	else if(rt_strstr(event,"+MIPLREAD:")!=RT_NULL)
	{
		LOG_D("%s\n",event);
		result=strtok((char *)event,":");
		instance=strtok(NULL,",");
		msgid=strtok(NULL,",");
		objectid=strtok(NULL,",");
		instanceid=strtok(NULL,",");
		resourceid=strtok(NULL,",");
		
		//进入 onenet read 响应函数
		if(onenet_read_rsp(atoi(msgid),atoi(objectid),atoi(instanceid),atoi(resourceid))!=RT_EOK)
			LOG_E("rsp onener read failure\n");
	}
	//判断是否为 onenet write 事件
	else if(rt_strstr(event,"+MIPLWRITE:")!=RT_NULL)
	{
		char *valuetype=NULL;
		char *value_len=NULL;
		char *value=NULL;
		char *flge=NULL;
		LOG_D("%s\n",event);
		result=strtok(event,":");
		instance=strtok(NULL,",");
		msgid=strtok(NULL,",");
		objectid=strtok(NULL,",");
		instanceid=strtok(NULL,",");
		resourceid=strtok(NULL,",");
		valuetype=strtok(NULL,",");
		value_len=strtok(NULL,",");
		value=strtok(NULL,",");
		flge=strtok(NULL,",");
		//判断标识是否为0
		if(atoi(flge)==0)
		{
			//执行 onenet write 响应函数
			if(onenet_write_rsp(atoi(msgid),atoi(objectid),atoi(instanceid),atoi(resourceid),atoi(valuetype),atoi(value_len),value)!=RT_EOK)
				LOG_E("rsp onenet write failure\n");
		}			
	}
	//判断是否为 exec 事件
	else if(rt_strstr(event,"+MIPLEXECUTE:")!=RT_NULL)
	{	
		char *value_len=NULL;
		char *value=NULL;
		LOG_D("%s\n",event);
		result=strtok((char *)event,":");
		instance=strtok(NULL,",");
		msgid=strtok(NULL,",");
		objectid=strtok(NULL,",");
		instanceid=strtok(NULL,",");
		resourceid=strtok(NULL,",");
		value_len=strtok(NULL,",");
		value=strtok(NULL,",");
		//执行 onenet exec 响应函数
		if(onenet_execute_rsp(atoi(msgid),atoi(objectid),atoi(instanceid),atoi(resourceid),atoi(value_len),value)!=RT_EOK)
			LOG_E("rsp onenet execute failure\n");
	}
	//判断是否为 observe 事件
	else if(rt_strstr(event,"+MIPLOBSERVE:")!=RT_NULL)
	{	
		char *oper=RT_NULL;
		int i=0;
		LOG_D("%s\n",event);
		onenet_device_table.observercount++;

		result=strtok((char *)event,":");
		instance=strtok(NULL,",");
		msgid=strtok(NULL,",");
		oper=strtok(NULL,",");
		objectid=strtok(NULL,",");
		instanceid=strtok(NULL,",");
		
		for(;i<onenet_device_table.dev_len;i++)
			{
				if(onenet_stream_table[i].objid==atoi(objectid)&&onenet_stream_table[i].insid==atoi(instanceid))
				{
					onenet_stream_table[i].msgid=atoi(msgid);			
#ifdef QSDK_USING_DEBUG
					LOG_D("objece=%d,instanceid=%d msg=%d\n",onenet_stream_table[i].objid,onenet_stream_table[i].insid,\
					onenet_stream_table[i].msgid);
#endif					
				}
			}
#ifdef QSDK_USING_ME3616
				LOG_D("AT+MIPLOBSERVERSP=%d,%d,%d\r\n",onenet_device_table.instance,atoi(msgid),atoi(oper));
				if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLOBSERVERSP=%d,%d,%d",onenet_device_table.instance,atoi(msgid),atoi(oper))!=RT_EOK)
				{
					rt_kprintf("+MIPLOBSERVERSP  failure \r\n");
					onenet_device_table.observer_status=qsdk_onenet_status_failure;
				}	
#endif

#ifdef QSDK_USING_DEBUG
		LOG_D("observercount=%d\n",onenet_device_table.observercount);
#endif
		//判断 obcerve 事件是否执行完成
		if(onenet_device_table.observercount==onenet_device_table.objcount)
		{
			//observe 事件执行完成
			onenet_device_table.observercount=0;
			onenet_device_table.observer_status=qsdk_onenet_status_success;
#ifdef QSDK_USING_DEBUG
			LOG_D("observer success\n ");
#endif
			rt_event_send(onenet_event,event_observer_success);
		}
	}
	//判断是否为 discover 事件
	else if(rt_strstr(event,"+MIPLDISCOVER:")!=RT_NULL)
	{
		char resourcemap[50]="";
		int str[50];
		LOG_D("%s\n",event);
		result=strtok((char *)event,":");
		instance=strtok(NULL,",");
		msgid=strtok(NULL,",");
		objectid=strtok(NULL,",");
		
#ifdef QSDK_USING_ME3616		
		int i=0,j=0,status=1,resourcecount=0;
		//循环检测设备总数据流
		for(;i<onenet_device_table.dev_len;i++)
			{
					//判断当前objectid 是否为 discover 回复的 objectid
					if(onenet_stream_table[i].objid==atoi(objectid))
					{
							j=0;
							//判断当前 resourceid 是否上报过
							for(;j<resourcecount;j++)
							{
								//如果 resourceid没有上报过
								if(str[j]!=onenet_stream_table[i].resid)
										status=1;		//上报标识置一
								else 
								{
									status=0;
									break;
								}									
							}
						//判断该 resourceid 是否需要上报
						if(status)
						{
							//记录需要上报的 resourceid
							str[resourcecount++]=onenet_stream_table[i].resid;
							
							//判断本次上报的 resourceid 是否为第一次找到
							if(resourcecount-1)
								rt_sprintf(resourcemap,"%s;%d",resourcemap,onenet_stream_table[i].resid);
							else
								rt_sprintf(resourcemap,"%s%d",resourcemap,onenet_stream_table[i].resid);
						}	
					}
	//			}
			}
#ifdef QSDK_USING_DEBUG
			j=0;
			//循环打印已经记录的 resourceid
			for(;j<resourcecount;j++)
			LOG_D("resourcecount=%d\n",str[j]);

			//打印 resourceid map
			LOG_D("map=%s\r\n",resourcemap);
#endif
				LOG_D("AT+MIPLDISCOVERRSP=%d,%d,1,%d,\"%s\"\n",onenet_device_table.instance,atoi(msgid),strlen(resourcemap),resourcemap);
				if(at_obj_exec_cmd(nb_client,nb_resp,"AT+MIPLDISCOVERRSP=%d,%d,1,%d,\"%s\"",onenet_device_table.instance,atoi(msgid),strlen(resourcemap),resourcemap)!=RT_EOK)
				{
					LOG_E("+MIPLDISCOVERRSP  failure \n");
				}	
#endif

		onenet_device_table.discovercount++;
		onenet_device_table.discover_status=qsdk_onenet_status_run;
		
//	rt_kprintf("discover_count=%d    objcoun=%d",data_stream.discovercount,data_stream.objectcount);
		//判断 discover 事件是否完成
		if(onenet_device_table.discovercount==onenet_device_table.objcount)
		{	
			//discover 事件已经完成
#ifdef QSDK_USING_DEBUG
			LOG_D("discover success\n");			
#endif
			//onenet 连接成功
			onenet_device_table.discovercount=0;
			onenet_device_table.discover_status=qsdk_onenet_status_success;
			onenet_device_table.connect_status=qsdk_onenet_status_success;
			rt_event_send(onenet_event,event_discover_success);
		}					
	}	
}

INIT_APP_EXPORT(qsdk_onenet_init_environment);

#ifdef QSDK_USING_FINSH_CMD

void qsdk_onenet(int argc,char**argv)
{
	qsdk_onenet_stream_t client;
	rt_uint16_t i;
    if (argc > 1)
    {
			if (!strcmp(argv[1], "quick_start"))
			{
				//nb-iot模块快速连接到onenet
				qsdk_onenet_quick_start();
			}
			else if (!strcmp(argv[1], "object_init"))
			{
				if(argc>9)
				{
					if(!strcmp(argv[9], "string"))
					{
						client=qsdk_onenet_object_init(atoi(argv[2]),atoi(argv[3]),atoi(argv[4]),atoi(argv[5]),argv[6],atoi(argv[7]),atoi(argv[8]),qsdk_onenet_value_string);	
					}
					else if(!strcmp(argv[9], "opaque"))
					{
						client=qsdk_onenet_object_init(atoi(argv[2]),atoi(argv[3]),atoi(argv[4]),atoi(argv[5]),argv[6],atoi(argv[7]),atoi(argv[8]),qsdk_onenet_value_opaque);
					}
					else if(!strcmp(argv[9], "int"))
					{
						client=qsdk_onenet_object_init(atoi(argv[2]),atoi(argv[3]),atoi(argv[4]),atoi(argv[5]),argv[6],atoi(argv[7]),atoi(argv[8]),qsdk_onenet_value_integer);
					}
					else if(!strcmp(argv[9], "float"))
					{
						client=qsdk_onenet_object_init(atoi(argv[2]),atoi(argv[3]),atoi(argv[4]),atoi(argv[5]),argv[6],atoi(argv[7]),atoi(argv[8]),qsdk_onenet_value_float);
					}
					else if(!strcmp(argv[9], "bool"))
					{
						client=qsdk_onenet_object_init(atoi(argv[2]),atoi(argv[3]),atoi(argv[4]),atoi(argv[5]),argv[6],atoi(argv[7]),atoi(argv[8]),qsdk_onenet_value_bool);
					}
					else if(!strcmp(argv[9], "hexstr"))
					{
						client=qsdk_onenet_object_init(atoi(argv[2]),atoi(argv[3]),atoi(argv[4]),atoi(argv[5]),argv[6],atoi(argv[7]),atoi(argv[8]),qsdk_onenet_value_hexStr);
					}
					if(!strcmp(argv[9], "string")||!strcmp(argv[9], "opaque")||!strcmp(argv[9], "int")||!strcmp(argv[9], "float")||!strcmp(argv[9], "bool")||!strcmp(argv[9], "hexstr"))
					{
						if(client!=RT_NULL)
						{
							for(i=0;i<QSDK_ONENET_OBJECT_MAX_NUM;i++)
							{
								if(&onenet_stream_table[i]==client)
								{
									rt_kprintf("this onenet client id is:%d\n",i);
									break;
								}	
							}
						}
					}
					else rt_kprintf("Value type error, please enter string、opaque、int、float、bool、hexstr.\n");					
				}
				else
				{
					rt_kprintf("qsdk_onenet object_init <objid> <insid> <resid> <inscount> <bitmap> <atts> <acts> <value_type>             - Please enter the correct command\n");
				}
			}
			else if (!strcmp(argv[1], "delete_ins"))
			{
				if(qsdk_onenet_delete_instance()==RT_EOK)
					rt_kprintf("onenet delete instance success\n");
			}
			else if (!strcmp(argv[1], "delete_object"))
			{
				if(argc>2)
				{
					if(onenet_stream_table[atoi(argv[2])].user_status)
					{
						if(qsdk_onenet_delete_object(&onenet_stream_table[atoi(argv[2])])!=RT_EOK)
							rt_kprintf("onenet delete object error\n");
						else rt_kprintf("onenet delete object success\n");
					}
					else rt_kprintf("This id is error\n");
				}
				else
				{
					rt_kprintf("qsdk_onenet delete_object <id>            -  Do you need to delete the object? Please enter id.\n");
				}
			}
			else if (!strcmp(argv[1], "open"))
			{
				qsdk_onenet_open();
			}
			else if (!strcmp(argv[1], "close"))
			{
				if(qsdk_onenet_close()==RT_EOK)
					rt_kprintf("onenet close success\n");
			}
			else if (!strcmp(argv[1], "update_time"))
			{
				if(argc>2)
				{
					if(qsdk_onenet_update_time(atoi(argv[2]))!=RT_EOK)
						rt_kprintf("onenet update time error\n");
					else rt_kprintf("onenet update time success\n");
				}
				else
				{
					rt_kprintf("qsdk_onenet update_time <0 or 1>            -  Do you need to update the object? Please enter 0 or 1.\n");
				}
			}
			else if (!strcmp(argv[1], "notify"))
			{
				if(argc>5)
				{
					if(onenet_stream_table[atoi(argv[2])].user_status)
					{
						client=&onenet_stream_table[atoi(argv[2])];
						if(client->valuetype==qsdk_onenet_value_string)
						{
							if(qsdk_onenet_notify(client,atoi(argv[3]),(qsdk_onenet_value_t)argv[4],atoi(argv[5]))==RT_EOK)
								rt_kprintf("net data send success\n");
						}
						else if(client->valuetype==qsdk_onenet_value_opaque)
						{
							if(qsdk_onenet_notify(client,atoi(argv[3]),(qsdk_onenet_value_t)argv[4],atoi(argv[5]))==RT_EOK)
								rt_kprintf("net data send success\n");
						}
						else if(client->valuetype==qsdk_onenet_value_integer)
						{
							int value_int=atoi(argv[4]);
							if(qsdk_onenet_notify(client,0,(qsdk_onenet_value_t)&value_int,atoi(argv[5]))==RT_EOK)
								rt_kprintf("net data send success\n");
						}
						else if(client->valuetype==qsdk_onenet_value_float)
						{
							float value_float=atof(argv[4]);
							LOG_D("%f\n",value_float);
							if(qsdk_onenet_notify(client,0,(qsdk_onenet_value_t)&value_float,atoi(argv[5]))==RT_EOK)
								rt_kprintf("net data send success\n");
						}
						else if(client->valuetype==qsdk_onenet_value_bool)
						{
							int value_bool=atoi(argv[4]);
							if(qsdk_onenet_notify(client,0,(qsdk_onenet_value_t)&value_bool,atoi(argv[5]))==RT_EOK)
								rt_kprintf("net data send success\n");
						}
						else if(client->valuetype==qsdk_onenet_value_hexStr)
						{
							if(qsdk_onenet_notify(client,atoi(argv[3]),(qsdk_onenet_value_t)argv[4],atoi(argv[5]))==RT_EOK)
								rt_kprintf("net data send success\n");
						}

					}
					else rt_kprintf("This id is error\n");				
				}
				else
				{
					rt_kprintf("qsdk_onenet notify <id> <len> <data> <flge>                  - Please enter the correct command\n");
				}
			}
#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)||	(defined QSDK_USING_M5311)
			else if (!strcmp(argv[1], "notify_ack"))
			{
				if(argc>5)
				{
					if(onenet_stream_table[atoi(argv[2])].user_status)
					{
						client=&onenet_stream_table[atoi(argv[2])];
						if(client->valuetype==qsdk_onenet_value_string)
						{
							if(qsdk_onenet_notify_and_ack(client,atoi(argv[3]),(qsdk_onenet_value_t)argv[4],atoi(argv[5]))==RT_EOK)
								rt_kprintf("net data send success\n");
						}
						else if(client->valuetype==qsdk_onenet_value_opaque)
						{
							if(qsdk_onenet_notify_and_ack(client,atoi(argv[3]),(qsdk_onenet_value_t)argv[4],atoi(argv[5]))==RT_EOK)
								rt_kprintf("net data send success\n");
						}
						else if(client->valuetype==qsdk_onenet_value_integer)
						{
							int value_int=atoi(argv[4]);
							if(qsdk_onenet_notify_and_ack(client,0,(qsdk_onenet_value_t)&value_int,atoi(argv[5]))==RT_EOK)
								rt_kprintf("net data send success\n");
						}
						else if(client->valuetype==qsdk_onenet_value_float)
						{
							float value_float=atof(argv[4]);
							LOG_D("%f\n",value_float);
							if(qsdk_onenet_notify_and_ack(client,0,(qsdk_onenet_value_t)&value_float,atoi(argv[5]))==RT_EOK)
								rt_kprintf("net data send success\n");
						}
						else if(client->valuetype==qsdk_onenet_value_bool)
						{
							int value_bool=atoi(argv[4]);
							if(qsdk_onenet_notify_and_ack(client,0,(qsdk_onenet_value_t)&value_bool,atoi(argv[5]))==RT_EOK)
								rt_kprintf("net data send success\n");
						}
						else if(client->valuetype==qsdk_onenet_value_hexStr)
						{
							if(qsdk_onenet_notify_and_ack(client,atoi(argv[3]),(qsdk_onenet_value_t)argv[4],atoi(argv[5]))==RT_EOK)
								rt_kprintf("net data send success\n");
						}

					}
					else rt_kprintf("This id is error\n");				
				}
				else
				{
					rt_kprintf("qsdk_onenet notify_ack <id> <len> <data> <flge>                  - Please enter the correct command\n");
				}
			}
#endif
			else if (!strcmp(argv[1], "list"))
			{
				int i;
				for(i=0;i<QSDK_ONENET_OBJECT_MAX_NUM;i++)
				{
					rt_kprintf("client:%d  user_status:%d, objid=%d, insid=%d, resid=%d, inscount=%d, bitmap=%s, valuetype=%d\n",i,onenet_stream_table[i].user_status,onenet_stream_table[i].objid,onenet_stream_table[i].insid,onenet_stream_table[i].resid,onenet_stream_table[i].inscount,onenet_stream_table[i].bitmap,onenet_stream_table[i].valuetype);
				}
			}
			else if (!strcmp(argv[1], "clear"))
			{
				qsdk_onenet_clear_environment();
				LOG_D("qsdk onenet environment clear success\n");
			}
			else
			{
					rt_kprintf("Unknown command. Please enter 'qsdk_onenet' for help\n");
			}
    }
    else
    {
      rt_kprintf("Usage:\n");
			rt_kprintf("qsdk_onenet object_init <objid> <insid> <resid> <inscount> <bitmap> <atts> <acts> <value_type>   - Init a lwm2m object\n");
			rt_kprintf("qsdk_onenet quick_start                                      - Fast connect to onenet\n");
			rt_kprintf("qsdk_onenet delete_ins                                       - Delete lwm2m instance\n");
			rt_kprintf("qsdk_onenet delete_object <id>                               - Delete lwm2m object\n");
			rt_kprintf("qsdk_onenet open                                             - Log in to onenet\n");
			rt_kprintf("qsdk_onenet close                                            - Log off on onenet\n");
			rt_kprintf("qsdk_onenet update_time <0 or 1>                             - Update online time\n");
			rt_kprintf("qsdk_onenet notify <id> <len> <data> <flge>                  - notify data to onenet\n");
#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)||	(defined QSDK_USING_M5311)
			rt_kprintf("qsdk_onenet notify_ack <id> <len> <data> <flge>              - notify data to onenet\n");
#endif
			rt_kprintf("qsdk_onenet list                                             - Display a list of onenet object clients\n");
			rt_kprintf("qsdk_onenet clear                                            - Clear the onenet object client list\n");
    }

		
		
}


MSH_CMD_EXPORT(qsdk_onenet, qsdk onenet function);
#endif // QSDK_USING_FINSH_CMD

#endif
