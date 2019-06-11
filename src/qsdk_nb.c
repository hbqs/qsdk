/*
 * File      : qsdk_nb.c
 * This file is part of nb in qsdk
 * Copyright (c) 2018-2030, longmain Development Team
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-11-14     longmain     first version
 * 2019-04-19     longmain     v1.0.0
 * 2019-05-09     longmain     add m5311 module
 * 2019-05-19     longmain     add psm mode
 */

#include "qsdk.h"
#include <at.h>

#define LOG_TAG              "[QSDK/NB]"
#ifdef QSDK_USING_LOG
#define LOG_LVL              LOG_LVL_DBG
#else
#define LOG_LVL              LOG_LVL_INFO
#endif
#include <ulog.h>

//设置event事件
#define EVENT_REBOOT 						(0x99<<1)
#define EVENT_PSM_UNLOOK_AT		 	(0x98<<1)
#define EVENT_EXIT_PSM 					(0x97<<1)

//引用业务处理函数
#ifdef QSDK_USING_NET
extern void net_event_func(char *event);
#endif

#ifdef QSDK_USING_IOT
extern void iot_event_func(char *event);
#endif
			
#ifdef QSDK_USING_ONENET
extern void onenet_event_func(char *event);
#endif

#ifdef QSDK_USING_MQTT
extern void mqtt_event_func(char *event);
#endif



enum nb_reboot_type
{
	NB_MODULE_NO_INIT=0,
	NB_MODULE_INIT_SUCCESS=4,
	NB_MODULE_REBOOT_BY_PIN=8,
	NB_MODULE_REBOOT_BY_AT=12,
	NB_MODULE_REBOOT_BY_FOTA=16,
	NB_MODULE_REBOOT_BY_OTHER=20,
	NB_MODULE_REBOOT_FOR_PIN=30,
	NB_MODULE_REBOOT_FOR_AT=31,
	NB_MODULE_REBOOT_FOR_FOTA=32,
	NB_MODULE_REBOOT_CLOSE=35
};




//定义 NB模块命令相应结构体指针
at_client_t nb_client = RT_NULL;
at_response_t nb_resp=RT_NULL;

//定义任务控制块
rt_thread_t hand_thread_id=RT_NULL;


//声明 函数
void hand_thread_entry(void* parameter);

//定义邮箱控制块
static rt_mailbox_t nb_mail=RT_NULL;
//定义事件控制块
rt_event_t nb_event=RT_NULL;


struct nb_device nb_device_table={0};

/*************************************************************
*	函数名称：	nb_hw_io_init
*
*	函数功能：	NB-IOT 模块控制引脚初始化
*
*	入口参数：	无
*
*	返回参数：	0 成功  1 失败
*
*	说明：		
*************************************************************/
int nb_io_init(void)
{
	rt_pin_mode(QSDK_PWRKEY_PIN,PIN_MODE_OUTPUT);
	rt_pin_mode(QSDK_RESET_PIN,PIN_MODE_OUTPUT);

	//给开发板 pwrkey 引脚上电
	rt_pin_write(QSDK_PWRKEY_PIN,PIN_HIGH);
	rt_pin_write(QSDK_RESET_PIN,PIN_HIGH);
	
#if	(defined QSDK_USING_M5311)||(defined QSDK_USING_ME3616)	
	rt_pin_mode(QSDK_WAKEUP_IN_PIN,PIN_MODE_OUTPUT);
	rt_pin_write(QSDK_WAKEUP_IN_PIN,PIN_HIGH);
	rt_thread_delay(2000);
	rt_pin_write(QSDK_PWRKEY_PIN,PIN_LOW);
#endif
	rt_thread_delay(500);
	rt_pin_write(QSDK_RESET_PIN,PIN_LOW);

	
	return RT_EOK;
}

/*************************************************************
*	函数名称：	qsdk_hw_io_exit_psm
*
*	函数功能：	NB-IOT 模块退出psm模式
*
*	入口参数：	无
*
*	返回参数：	0 成功  1 失败
*
*	说明：		
*************************************************************/
int nb_io_exit_psm(void)
{
#if	(defined QSDK_USING_M5311)||(defined QSDK_USING_ME3616)
	rt_pin_write(QSDK_WAKEUP_IN_PIN,PIN_LOW);
	rt_thread_delay(200);
	rt_pin_write(QSDK_WAKEUP_IN_PIN,PIN_HIGH);
	rt_thread_delay(500);
#endif
	return RT_EOK;
}

/*************************************************************
*	函数名称：	qsdk_nb_reboot
*
*	函数功能：	NB-IOT 模块重启
*
*	入口参数：	无
*
*	返回参数：	0 成功  1 失败
*
*	说明：		
*************************************************************/
int qsdk_nb_reboot(void)
{
	rt_uint32_t status;
	rt_memset(&nb_device_table,0,sizeof(nb_device_table));
	nb_device_table.reboot_open=NB_MODULE_REBOOT_FOR_PIN;
	nb_device_table.reboot_type=NB_MODULE_NO_INIT;
	nb_device_table.net_connect_ok=NB_MODULE_NO_INIT;
	
	rt_pin_write(QSDK_RESET_PIN,PIN_HIGH);
	rt_thread_delay(500);
	rt_pin_write(QSDK_RESET_PIN,PIN_LOW);
	
	if(rt_event_recv(nb_event,EVENT_REBOOT,RT_EVENT_FLAG_AND|RT_EVENT_FLAG_CLEAR,20000,&status)!=RT_EOK)
	{
		LOG_E("nb-iot reboot failure\r\n");
		return  RT_ERROR;
	}
	rt_thread_delay(1000);
	nb_device_table.reboot_type=NB_MODULE_NO_INIT;
	nb_device_table.reboot_open=0;
	if(qsdk_nb_wait_connect()!=RT_EOK)
	{
		LOG_E("nb-iot reboot success,but connect nb-iot failure\r\n");
		return  RT_ERROR;
	}
	return RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_nb_wait_connect
*
*	函数功能：	等待模块连接
*
*	入口参数：	无
*
*	返回参数：	0 成功   1 失败
*
*	说明：		
*************************************************************/
int qsdk_nb_wait_connect(void)
{
	if(at_client_obj_wait_connect(nb_client,2000)!=RT_EOK)
	{
		at_delete_resp(nb_resp);							//删除 AT 命令相应结构体
		LOG_E("nb-iot module connect failure\r\n");
		return RT_ERROR;
	}

	return RT_EOK;
}

/*************************************************************
*	函数名称：	nb_sim_check
*
*	函数功能：	检测模块是否已经开机
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_nb_sim_check(void)
{
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CFUN?")!=RT_EOK)	return RT_ERROR;
	at_resp_parse_line_args(nb_resp,2,"+CFUN:%d",&nb_device_table.sim_state);

	return  RT_EOK;
}

/*************************************************************
*	函数名称：	nb_get_imsi
*
*	函数功能：	获取 SIM 卡的 imsi 
*
*	入口参数：	无
*
*	返回参数：	IMSI指针 成功    RT_NULL	失败
*
*	说明：		
*************************************************************/
char *qsdk_nb_get_imsi(void)
{
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CIMI")!=RT_EOK)	return RT_NULL;	
	
	at_resp_parse_line_args(nb_resp,2,"%s\r\n",nb_device_table.imsi);
	return  nb_device_table.imsi;
}
/*************************************************************
*	函数名称：	nb_get_imei
*
*	函数功能：	获取模块的 imei 
*
*	入口参数：	无
*
*	返回参数：	IMEI指针 成功    RT_NULL	失败
*
*	说明：		
*************************************************************/
char *qsdk_nb_get_imei(void)
{
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CGSN=1")!=RT_EOK)	return RT_NULL;	
	
	at_resp_parse_line_args(nb_resp,2,"+CGSN:%s",nb_device_table.imei);
	return  nb_device_table.imei;
}
/*************************************************************
*	函数名称：	nb_set_rtc_time
*
*	函数功能：	设置RTC时间为当前时间
*
*	入口参数：	year 年   month 月	day 日	hour 时	min 分	sec 秒
*
*	返回参数：	无
*
*	说明：		
*************************************************************/
static void qsdk_nb_set_rtc_time(int year,int month,int day,int hour,int min,int sec)
{
	int week,lastday;
	hour+=QSDK_TIME_ZONE;
	if ((0==year%4 && 0!=year%100) || 0==year%400)
       	lastday=29;
    else if(month==1||month==3||month==5||month==7||month==8||month==10||month==12)
    	lastday=31;
    else if(month==4||month==6||month==9||month==11)
   		 lastday=30;
     else
       	lastday=28;
   	if(hour>24)
   	{
   		hour-=24;
   		day++;
   		if(day>lastday)
   		{
   			day-=lastday;
   			month++;  			
   		}
   		if(month>12)
   		{
   			month-=12;	
   			year++;
   		}
   	}	
		week=(day+2*month+3*(month+1)/5+year+year/4-year/100+year/400)%7+1;
#ifdef QSDK_USING_M5311
		hour-=8;
#endif
		LOG_D("当前时间:%d-%d-%d,%d-%d-%d,星期:%d\r\n",year+2000,month,day,hour,min,sec,week);
		
		qsdk_rtc_set_time_callback(year+2000,month,day,hour,min,sec,week);

}
/*************************************************************
*	函数名称：	qsdk_nb_get_time
*
*	函数功能：	获取网络时间
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_nb_get_time(void)
{
	int year,mouth,day,hour,min,sec;
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CCLK?")!=RT_EOK)	return RT_ERROR;	
	
	at_resp_parse_line_args(nb_resp,2,"+CCLK:%d/%d/%d,%d:%d:%d+",&year,&mouth,&day,&hour,&min,&sec);
	
	qsdk_nb_set_rtc_time(year,mouth,day,hour,min,sec);
	return  RT_EOK;
}

/*************************************************************
*	函数名称：	qsdk_nb_get_csq
*
*	函数功能：	获取当前信号值
*
*	入口参数：	无
*
*	返回参数：	0-99 成功    -1	失败
*
*	说明：		
*************************************************************/
int qsdk_nb_get_csq(void)
{
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CSQ")!=RT_EOK)	return -1;	
	
	at_resp_parse_line_args(nb_resp,2,"+CSQ:%d\r\n",&nb_device_table.csq);
	return  nb_device_table.csq;

}
/*************************************************************
*	函数名称：	nb_set_net_start
*
*	函数功能：	手动附着网络
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_nb_set_net_start(void)
{
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CGATT=1")!=RT_EOK)	return RT_ERROR;
	
	return  RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_nb_get_net_connect
*
*	函数功能：	获取当前网络状态
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_nb_get_net_connect(void)
{
	int i;
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CEREG?")!=RT_EOK)
	{
		return  RT_ERROR;
	}
	at_resp_parse_line_args_by_kw(nb_resp,"+CEREG:","+CEREG:%d,%d",&i,&nb_device_table.net_connect_ok);

	if(nb_device_table.net_connect_ok==1) 
	{
		return RT_EOK;
	}
	else 
	{
		return  RT_ERROR;
	}
}
/*************************************************************
*	函数名称：	qsdk_nb_open_net_light
*
*	函数功能：	打开网络灯
*
*	入口参数：	无
*
*	返回参数：	0 成功    1失败
*
*	说明：		
*************************************************************/
int qsdk_nb_open_net_light(void)
{
#ifdef QSDK_USING_M5311
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CMSYSCTRL=0,2")!=RT_EOK)	return RT_ERROR;
#endif
#ifdef QSDK_USING_ME3616
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+ZCONTLED=1")!=RT_EOK)	return RT_ERROR;
#endif	
	return  RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_nb_close_net_light
*
*	函数功能：	关闭网络灯
*
*	入口参数：	无
*
*	返回参数：	0 成功    1失败
*
*	说明：		
*************************************************************/
int qsdk_nb_close_net_light(void)
{
#ifdef QSDK_USING_M5311
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CMSYSCTRL=0,0")!=RT_EOK)	return RT_ERROR;
#endif
#ifdef QSDK_USING_ME3616
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+ZCONTLED=0")!=RT_EOK)	return RT_ERROR;
#endif	
	return  RT_EOK;
}
/*************************************************************
*	函数名称：	nb_set_psm_mode
*
*	函数功能：	模块 PSM 模式设置
*
*	入口参数：	tau_time	TAU 时间		active_time active时间
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_nb_set_psm_mode(char *tau_time,char *active_time)
{
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CPSMS=1,,,\"%s\",\"%s\"",tau_time,active_time)!=RT_EOK)	return RT_ERROR;	

	return  RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_nb_open_auto_psm
*
*	函数功能：	打开自动进入PSM模式功能
*
*	入口参数：	无
*
*	返回参数：	0 成功    1失败
*
*	说明：		
*************************************************************/
int qsdk_nb_open_auto_psm(void)
{
#ifdef QSDK_USING_M5311
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+SM=UNLOCK")!=RT_EOK)	return RT_ERROR;
#endif	
	return  RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_nb_close_auto_psm
*
*	函数功能：	关闭自动进入PSM模式功能
*
*	入口参数：	无
*
*	返回参数：	0 成功    1失败
*
*	说明：		
*************************************************************/
int qsdk_nb_close_auto_psm(void)
{
#ifdef QSDK_USING_M5311
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+SM=LOCK")!=RT_EOK)	return RT_ERROR;
#endif
	return  RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_nb_enter_psm
*
*	函数功能：	关闭自动进入PSM模式功能
*
*	入口参数：	无
*
*	返回参数：	无
*
*	说明：		
*************************************************************/
void qsdk_nb_enter_psm(void)
{
	rt_uint32_t status;
#ifdef QSDK_USING_LOG
	LOG_D("nb-iot enter psm\r\n");
#endif
#ifdef QSDK_USING_ME3616
	LOG_D("AT+ZSLR\r\n");
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+ZSLR")!=RT_EOK)
	LOG_E("nb-iot set psm cmd failure\r\n");
#endif
#ifdef QSDK_USING_M5311
	LOG_D("AT*ENTERSLEEP\r\n");
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT*ENTERSLEEP")!=RT_EOK)
	LOG_E("nb-iot set psm cmd failure\r\n");
#endif
	rt_mutex_take(nb_client->lock,RT_WAITING_FOREVER);
	nb_device_table.psm_status=qsdk_nb_status_enter_psm;
	if(rt_event_recv(nb_event,EVENT_PSM_UNLOOK_AT,RT_EVENT_FLAG_AND|RT_EVENT_FLAG_CLEAR,RT_WAITING_FOREVER,&status)!=RT_EOK)
	{
		LOG_E("nb-iot wait exit psm failure\r\n");
	}
	rt_mutex_release(nb_client->lock);
	if(rt_event_recv(nb_event,EVENT_EXIT_PSM,RT_EVENT_FLAG_AND|RT_EVENT_FLAG_CLEAR,RT_WAITING_FOREVER,&status)!=RT_EOK)
	{
		LOG_E("nb-iot wait exit psm failure\r\n");
	}
#ifdef QSDK_USING_LOG
	LOG_D("nb-iot exit psm\r\n");
#endif
}
/*************************************************************
*	函数名称：	qsdk_nb_get_psm_status
*
*	函数功能：	查询nb-iot模块是否进入psm 模式
*
*	入口参数：	无
*
*	返回参数：	0：当前在psm模式      1：当前未进入psm模式
*
*	说明：		
*************************************************************/
int qsdk_nb_get_psm_status(void)
{
	if(nb_device_table.psm_status!=qsdk_nb_status_enter_psm)
	{
		return	RT_ERROR;
	}
	return  RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_nb_exit_psm
*
*	函数功能：	关闭自动进入PSM模式功能
*
*	入口参数：	无
*
*	返回参数：	0 成功    1失败
*
*	说明：		
*************************************************************/
int qsdk_nb_exit_psm(void)
{
	rt_event_send(nb_event,EVENT_PSM_UNLOOK_AT);
#if	(defined QSDK_USING_M5311)||(defined QSDK_USING_ME3616)
	if(nb_io_exit_psm()!=RT_EOK)
	{
		LOG_E("nb-iot exit psm failure\r\n");
		return RT_ERROR;
	}
#endif
	if(qsdk_nb_wait_connect()!=RT_EOK)
	{
		LOG_E("nb-iot exit psm failure\r\n");
		return RT_ERROR;
	}
	nb_device_table.psm_status=qsdk_nb_status_exit_psm;
	rt_event_send(nb_event,EVENT_EXIT_PSM);
	return  RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_nb_get_net_connect_status
*
*	函数功能：	获取查询到的网络状态
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_nb_get_net_connect_status(void)
{
	if(nb_device_table.net_connect_ok) return RT_EOK;

	return RT_ERROR;
}
/*************************************************************
*	函数名称：	qsdk_nb_get_reboot_event
*
*	函数功能：	获取nb-iot模块重启原因
*
*	入口参数：	无
*
*	返回参数：	无
*
*	说明：		
*************************************************************/
int qsdk_nb_get_reboot_event(void)
{
	return nb_device_table.reboot_type;
}
/*************************************************************
*	函数名称：	nb_query_ip
*
*	函数功能：	查询模块在核心网的IP地址
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
char *qsdk_nb_query_ip(void)
{
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CGPADDR")!=RT_EOK) return RT_NULL;
	
	at_resp_parse_line_args(nb_resp,2,"+CGPADDR:0,%s",nb_device_table.ip);
	return nb_device_table.ip;
}

#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
/*************************************************************
*	函数名称：	qsdk_iot_check_address
*
*	函数功能：	检查iot服务器地址是都正确
*
*	入口参数：	无
*
*	返回参数：	0 正确  1	失败
*
*	说明：		
*************************************************************/
int qsdk_iot_check_address(void)
{
	char str[50];
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NCDP?")!=RT_EOK) return RT_ERROR;
	
	at_resp_parse_line_args(nb_resp,2,"%s",str);


	LOG_D("NCDP=%s\r\n",str);

#if (defined QSDK_USING_M5310A) &&(defined QSDK_USING_IOT)
	if(rt_strstr(str,QSDK_IOT_ADDRESS)!=RT_NULL) 		return RT_EOK;
#else
	if(rt_strstr(str,"0.0.0.0")!=RT_NULL) 		return RT_EOK;
#endif

	return RT_ERROR;
}
/*************************************************************
*	函数名称：	qsdk_iot_set_address
*
*	函数功能：	设置 NCDP 服务器
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_iot_set_address(void)
{
	LOG_D("AT+CFUN=0\r\n");

	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+CFUN=0")!=RT_EOK)
	{
		LOG_E("开启模块最小功能失败\r\n");
		return RT_ERROR;
	}
	rt_thread_delay(200);
	
#if (defined QSDK_USING_M5310A) &&(defined QSDK_USING_IOT)
	LOG_D("AT+NCDP=%s,%s\r\n",QSDK_IOT_ADDRESS,QSDK_IOT_PORT);
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NCDP=%s,%s",QSDK_IOT_ADDRESS,QSDK_IOT_PORT)!=RT_EOK)
#else
	LOG_D("AT+NCDP=%s,%s\r\n","0.0.0.0","5683");
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+NCDP=%s,%s","0.0.0.0","5683")!=RT_EOK)
#endif
	{
		LOG_E("设置NCDP服务器失败\r\n");
		return RT_ERROR;
	}
	rt_thread_delay(200);

	if(qsdk_nb_reboot()==RT_EOK)	return RT_EOK;

	return RT_ERROR;	
}
#endif
/*************************************************************
*	函数名称：	string_to_hex
*
*	函数功能：	字符串转hex
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int string_to_hex(const char *pString, int len, char *pHex)
{
    int i = 0;
    if (NULL == pString || len <= 0 || NULL == pHex)
    {
        return RT_ERROR;
    }
    for(i = 0; i < len; i++)
    {
        rt_sprintf(pHex+i*2, "%02X", pString[i]);
    }
    return RT_EOK;
}

/*************************************************************
*	函数名称：	nb_reboot_func
*
*	函数功能：	模块主动下发处理事件
*
*	入口参数：	无
*
*	返回参数：	无
*
*	说明：		
*************************************************************/
void nb_reboot_func(char *data)
{
#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
	if(rt_strstr(data,"REBOOT_CAUSE_SECURITY_RESET_PIN")!=RT_NULL)
	{
		LOG_E("%s\r\n reboot by pin\r\n",data);
		
		if(nb_device_table.reboot_open==NB_MODULE_REBOOT_FOR_PIN)
		{
			nb_device_table.reboot_type=NB_MODULE_REBOOT_BY_PIN;
			rt_event_send(nb_event,EVENT_REBOOT);
		}			
		else qsdk_nb_reboot_callback();
	}
	else 	if(rt_strstr(data,"REBOOT_CAUSE_APPLICATION_AT")!=RT_NULL)
	{
		LOG_E("%s\r\n reboot by at\r\n",data);
		
		if(nb_device_table.reboot_open==NB_MODULE_REBOOT_FOR_AT)
		{
			nb_device_table.reboot_type=NB_MODULE_REBOOT_BY_AT;	
			rt_event_send(nb_event,EVENT_REBOOT);
		}
		else qsdk_nb_reboot_callback();
	}
	else 	if(rt_strstr(data,"REBOOT_CAUSE_SECURITY_FOTA_UPGRADE")!=RT_NULL)
	{
		LOG_E("%s\r\n reboot by fota\r\n",data);

		if(nb_device_table.fota_open==NB_MODULE_REBOOT_FOR_FOTA) nb_device_table.fota_open=0;
		nb_device_table.reboot_type=NB_MODULE_REBOOT_BY_FOTA;
		qsdk_nb_reboot_callback();

	}
	else
	{
		LOG_E("%s\r\n reboot by other\r\n",data);

		nb_device_table.reboot_type=NB_MODULE_REBOOT_BY_OTHER;
		qsdk_nb_reboot_callback();
	}
#endif
#ifdef QSDK_USING_M5311
if(rt_strstr(data,"*ATREADY: 1")!=RT_NULL)
	{
		LOG_E("%s\r\n nb-iot reboot\r\n",data);
		if(nb_device_table.reboot_open==NB_MODULE_REBOOT_FOR_PIN||nb_device_table.reboot_open==NB_MODULE_REBOOT_FOR_AT)
		{
			rt_event_send(nb_event,EVENT_REBOOT);
		}			
		else qsdk_nb_reboot_callback();
	}
#endif

#ifdef QSDK_USING_ME3616
	if(rt_strstr(data,"*MATREADY: 1")!=RT_NULL)
	{
		LOG_E("%s\r\n nb-iot reboot\r\n",data);
		if(nb_device_table.reboot_open==NB_MODULE_REBOOT_FOR_PIN||nb_device_table.reboot_open==NB_MODULE_REBOOT_FOR_AT)
		{
			rt_event_send(nb_event,EVENT_REBOOT);
		}			
		else qsdk_nb_reboot_callback();
	}
#endif
}

/*************************************************************
*	函数名称：	hand_thread_entry
*
*	函数功能：	模组主动上报数据处理函数
*
*	入口参数：	无
*
*	返回参数：	无
*
*	说明：		
*************************************************************/
void hand_thread_entry(void* parameter)
{
	rt_err_t status=RT_EOK;
	char *event;
	
	while(1)
	{
		//等待事件邮件 event_mail
		status=rt_mb_recv(nb_mail,(rt_ubase_t *)&event,RT_WAITING_FOREVER);
		//判断是否接收成功
		if(status==RT_EOK)
		{
#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
			if(rt_strstr(event,"REBOOT_")!=RT_NULL)
#endif
#ifdef QSDK_USING_M5311
			if(rt_strstr(event,"*ATREADY")!=RT_NULL)
#endif
#ifdef QSDK_USING_ME3616
			if(rt_strstr(event,"*MATREADY")!=RT_NULL)
#endif
			{
				nb_reboot_func(event);
			}
			else
			{
//引用业务处理函数
#ifdef QSDK_USING_NET
			net_event_func(event);
#endif

#ifdef QSDK_USING_IOT
			iot_event_func(event);
#endif
			
#ifdef QSDK_USING_ONENET
			onenet_event_func(event);
#endif

#ifdef QSDK_USING_MQTT
			mqtt_event_func(event);
#endif			
			}
			
		}
		else	LOG_E("event_mail recv fail\r\n");
		
	}
}

/*************************************************************
*	函数名称：	qsdk_nb_event_func
*
*	函数功能：	模块主动下发处理事件
*
*	入口参数：	无
*
*	返回参数：	无
*
*	说明：		
*************************************************************/
void nb_event_func(struct at_client *client, const char *data, rt_size_t size)
{
	rt_err_t status;
	if(client==nb_client)
	{
		status=rt_mb_send(nb_mail,(rt_uint32_t)data);
		if(status!=RT_EOK)
			LOG_E("mb send fail\r\n");
	}
	else LOG_E("nb event fun error\r\n");	
}


/*************************************************************
*
*	说明：		模块主动下发消息识别结构体
*
*************************************************************/
static struct at_urc nb_urc_table[]={
//模块开机重启检测
#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
	{"REBOOT_",           "\r",nb_event_func},
#endif	
#ifdef QSDK_USING_M5311
	{"*ATREADY:",           "\r",nb_event_func},
#endif	
#ifdef QSDK_USING_ME3616
	{"*MATREADY:",           "\r",nb_event_func},
#endif	
	
//如果启用TCP/UDP支持，增加NET函数调用
#ifdef QSDK_USING_NET
	{"+NSONMI",           "\r",nb_event_func},
	{"+NSOCLI",           "\r",nb_event_func},
	{"CONNECT OK",           "\r",nb_event_func},
#endif
	
//如果启用onenet支持，增加onenet函数调用
#ifdef QSDK_USING_ONENET
	{"+MIPLREAD",         "\r",nb_event_func},
	{"+MIPLWRITE",        "\r",nb_event_func},
	{"+MIPLEXECUTE",      "\r",nb_event_func},
	{"+MIPLOBSERVE",      "\r",nb_event_func},
	{"+MIPLDISCOVER",     "\r",nb_event_func},
	{"+MIPLEVENT",				"\r",nb_event_func},
#endif
	
//如果启用iot支持，增加iot函数调用
#ifdef QSDK_USING_IOT
	{"+NNMI",				 			"\r",nb_event_func},
	{"+NSMI",							"\r",nb_event_func},
#endif
	
//如果启用mqtt支持，增加mqtt函数调用
#ifdef QSDK_USING_MQTT
	{"+MQTTOPEN:",				"\r",nb_event_func},
	{"+MQTTSUBACK:",			"\r",nb_event_func},
	{"+MQTTPUBLISH:",			"\r",nb_event_func},
	{"+MQTTPUBACK:",			"\r",nb_event_func},
	{"+MQTTUNSUBACK:",		"\r",nb_event_func},
	{"+MQTTDISC:",				"\r",nb_event_func},
	{"+MQTTPUBREC:",			"\r",nb_event_func},
	{"+MQTTPUBCOMP:",			"\r",nb_event_func},
	{"+MQTTPINGRESP:",		"\r",nb_event_func},
	{"+MQTTTO:",					"\r",nb_event_func},
	{"+MQTTREC:",					"\r",nb_event_func},
#endif
};
/*************************************************************
*	函数名称：	qsdk_init_environment
*
*	函数功能：	QSDK 运行环境初始化
*
*	入口参数：	无
*
*	返回参数：	0：成功   1：失败
*
*	说明：		
*************************************************************/
int qsdk_init_environment(void)
{
		rt_uint32_t status;
		rt_device_t uart_device;
		struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT; 
		
		LOG_I("\r\nWelcome to use QSDK. This SDK by longmain.\r\n Our official website is www.longmain.cn.\r\n\r\n");
		//set uart buad
    uart_device=rt_device_find(QSDK_UART);

		config.baud_rate=QSDK_UART_BAUDRATE;
		rt_device_control(uart_device, RT_DEVICE_CTRL_CONFIG, &config);
		//at client init
		at_client_init(QSDK_UART,QSDK_CMD_REV_MAX_LEN*2+100);
		//create at resp
		nb_client = at_client_get(QSDK_UART);
		nb_resp=at_create_resp(QSDK_CMD_REV_MAX_LEN*2+100,0,5000);
		if (nb_resp == RT_NULL)
		{
			LOG_E("create at resp failure \r\n");
			return RT_ERROR;
		}	
		at_obj_set_urc_table(nb_client,nb_urc_table,sizeof(nb_urc_table)/sizeof(nb_urc_table[0]));
		//create mail
		nb_mail=rt_mb_create("nb_mail",
														10,
														RT_IPC_FLAG_FIFO);
		if(nb_mail==RT_NULL)
		{
			LOG_E("create mail failure\n");
			return RT_ERROR;
		}
		//create event
		nb_event=rt_event_create("nb_event",RT_IPC_FLAG_FIFO);
		if(nb_event==RT_NULL)
		{
			LOG_E("create event failure\r\n");
			return RT_ERROR;
		}
		//create event hand fun
		hand_thread_id=rt_thread_create("hand_thread",
																		hand_thread_entry,
																		RT_NULL,
																		QSDK_HAND_THREAD_STACK_SIZE,
																		7,
																		50);
		if(hand_thread_id!=RT_NULL)
			rt_thread_startup(hand_thread_id);
		else
		{
			LOG_E("create event hand fun failure\r\n");
			return RT_ERROR;
		}
		rt_memset(&nb_device_table,0,sizeof(nb_device_table));
		nb_device_table.reboot_open=NB_MODULE_REBOOT_FOR_PIN;
		nb_device_table.reboot_type=NB_MODULE_NO_INIT;
		nb_device_table.net_connect_ok=NB_MODULE_NO_INIT;
		//nb-iot gpio init
		if(nb_io_init()!=RT_EOK)
		{
			LOG_E("nb-iot gpio init failure\r\n");
			return RT_ERROR;
		}		
		if(rt_event_recv(nb_event,EVENT_REBOOT,RT_EVENT_FLAG_AND|RT_EVENT_FLAG_CLEAR,7500,&status)!=RT_EOK)
		{
			LOG_E("nb-iot power on reset failure\r\n");
			return RT_ERROR;
		}
		nb_device_table.reboot_type=NB_MODULE_NO_INIT;
		nb_device_table.reboot_open=0;
		rt_thread_delay(1000);
		
		if(qsdk_nb_wait_connect()!=RT_EOK)
		{
			LOG_E("nb-iot wait connect failure\r\n");
			return RT_ERROR;
		}

		return RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_nb_quick_connect
*
*	函数功能：	NB-IOT 模块一键联网初始化
*
*	入口参数：	无
*
*	返回参数：	0：成功   1：失败
*
*	说明：		
*************************************************************/
int qsdk_nb_quick_connect(void)
{

	int i=5;
		if(qsdk_init_environment()!=RT_EOK) return RT_ERROR;		

#ifdef QSDK_USING_M5310A
//如果启用M5310连接IOT平台
		if(qsdk_iot_check_address()!=RT_EOK)
		{
			LOG_D("ncdp error,now to set ncdp address\r\n");
			
			if(qsdk_iot_set_address()==RT_EOK)
			{
				if(qsdk_iot_check_address()!=RT_EOK)
					LOG_E("set ncdp address failure\r\n");
			}
			else	return RT_ERROR;
		}
#endif	
	rt_thread_delay(1000);
//首先确定模块是否开机	
	do{
			i--;
			if(qsdk_nb_sim_check()!=RT_EOK)
			{
				rt_thread_delay(500);
			}
			LOG_D("+CFUN=%d\r\n",nb_device_table.sim_state);

			if(nb_device_table.sim_state!=1)
				rt_thread_delay(1000);

		}	while(nb_device_table.sim_state==0&&i>0);
		
		if(i<=0)
		{
			LOG_E("NB-IOT boot failure, please check SIM card\r\n");
			return RT_ERROR;
		}
		else{
			i=5;
			rt_thread_delay(1000);
		}	
#if (defined QSDK_USING_M5311)||(defined QSDK_USING_ME3616)
		if(qsdk_nb_open_net_light()!=RT_EOK)
		{
			LOG_E("nb-iot open net light failure\r\n");
			return RT_ERROR;
		}
		if(qsdk_nb_open_auto_psm()!=RT_EOK)
			LOG_E("nb-iot open auto psm failure\r\n");
#endif
//获取SIM卡的IMSI号码		
		do{
				i--;
				if(qsdk_nb_get_imsi()==RT_NULL)
				{
					rt_thread_delay(500);				
				}
				else
				{
					LOG_D("IMSI=%s\r\n",nb_device_table.imsi);
					break;
				}
			}while(i>0);
		
			if(i<=0)
			{
				LOG_E("No SIM card found\r\n");
				return RT_ERROR;
			}
			else
			{
				i=15;
				rt_thread_delay(100);
			}

//获取模块IMEI
			if(qsdk_nb_get_imei()==RT_NULL)
			{
				LOG_E("Nb-iot IMEI not foundr\n");
				return RT_ERROR;				
			}
			else
			{
				LOG_D("IMEI=%s\r\n",nb_device_table.imei);
			}

//如果启用IOT平台支持
#if (defined QSDK_USING_IOT)&&(defined QSDK_USING_M5310A)

//如果启用M5310连接IOT平台
	
		rt_thread_delay(100);
		if(qsdk_iot_open_update_status()!=RT_EOK)
		{
			LOG_E("open iot update status error\r\n");
			return RT_ERROR;
		}
		else LOG_D("open iot update status success\r\n");
		rt_thread_delay(100);
		if(qsdk_iot_open_down_date_status()!=RT_EOK)
		{
			LOG_E("open iot down date status error\r\n");
			return RT_ERROR;
		}
			else LOG_D("open iot down date status success\r\n");	
#endif
//获取信号值
		do{
				i--;
				if(qsdk_nb_get_csq()==-1)
					{
						rt_thread_delay(500);
					}
					else if(nb_device_table.csq!=99&&nb_device_table.csq!=0)
					{
						break;
					}
					else
						{
							LOG_D("CSQ=%d\r\n",nb_device_table.csq);	
							rt_thread_delay(3000);
						}
		
			}while(i>0);
			
			if(i<=0)
			{
				LOG_E("nb-iot not find the signal\r\n");
				return RT_ERROR;
			}
			else
				{
					LOG_D("CSQ=%d\r\n",nb_device_table.csq);
					i=40;
					rt_thread_delay(100);
				}
		do{
				i--;
				if(qsdk_nb_get_net_connect()==RT_EOK) break;
				LOG_D("CEREG=%d\r\n",nb_device_table.net_connect_ok);
				rt_thread_delay(1000);
			}while(i>0);
		
		if(i<=0)
		{
			LOG_E("nb-iot connect network failurer\n");
			return RT_ERROR;
		}	
		rt_thread_delay(1000);			
		LOG_D("CEREG=%d\r\n",nb_device_table.net_connect_ok);

//获取ntp服务器时间

		if(qsdk_nb_get_time()!=RT_EOK)
		{
			LOG_E("getting network time errorsr\n");
		}
		LOG_D("nb-iot connect network success\r\n");		
				
		return RT_EOK;	
}


