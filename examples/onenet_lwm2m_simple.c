/*
 * File      : main.c
 * This file is part of fun in evb_iot_m1 board
 * Copyright (c) 2018-2030, longmain Development Team
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-04-14     longmain     first version
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "qsdk.h"

//设置ulog 参数
#define LOG_TAG              "[main]"
//调试期间开启
#define LOG_LVL              LOG_LVL_DBG

//正式期间开启此项，关闭上方的调试选项
//#define LOG_LVL              LOG_LVL_INFO
#include <ulog.h>


//定义onenet stream
qsdk_onenet_stream_t temp_object;
qsdk_onenet_stream_t hump_object;
qsdk_onenet_stream_t light0_object;
qsdk_onenet_stream_t light1_object;
qsdk_onenet_stream_t light2_object;

int main(void)
{
	rt_uint16_t recon_count=0,recon_onenet_count=0;
	rt_uint32_t event_status=0;
net_recon:	
	//nb-iot模块快快速初始化联网
	if(qsdk_nb_quick_connect()!=RT_EOK)
	{
		LOG_E("module init failure\r\n");	
		goto net_recon;
	}
onenet_recon:	
	//onenet stream 初始化
	temp_object=qsdk_onenet_object_init(3303,0,5700,1,"1",1,0,qsdk_onenet_value_float);
	if(temp_object==RT_NULL)
	{
		LOG_E("temp object create failure\r\n");
	}
	hump_object=qsdk_onenet_object_init(3304,0,5700,1,"1",1,0,qsdk_onenet_value_float);
	if(hump_object==RT_NULL)
	{
		LOG_E("hump object create failure\r\n");
	}
	light0_object=qsdk_onenet_object_init(3311,0,5850,3,"111",3,0,qsdk_onenet_value_bool);
	if(light0_object==RT_NULL)
	{
		LOG_E("light object create failure\r\n");
	}
		light1_object=qsdk_onenet_object_init(3311,1,5850,3,"111",3,0,qsdk_onenet_value_bool);
	if(light1_object==RT_NULL)
	{
		LOG_E("light object create failure\r\n");
	}
		light2_object=qsdk_onenet_object_init(3311,2,5850,3,"111",3,0,qsdk_onenet_value_bool);
	if(light2_object==RT_NULL)
	{
		LOG_E("light object create failure\r\n");
	}
	//快速连接到onenet
	if(qsdk_onenet_quick_start()!=RT_EOK)
	{
		recon_onenet_count++;
		LOG_E("onenet quick start failure\r\n");
		rt_thread_delay(30000);
		if(qsdk_onenet_open()!=RT_EOK)
		{
			qsdk_onenet_delete_instance();
			goto onenet_recon;
		}
		qsdk_onenet_update_time(1);
	}
	
	//连接网络成功，beep提示一声
	qsdk_beep_on();
	rt_thread_delay(500);
	qsdk_beep_off();
	if(recon_count==0)
		rt_thread_startup(data_up_thread_id);
	else
	{
		rt_thread_resume(data_up_thread_id);
		recon_count=0;
	}
	qsdk_onenet_notify(light0_object,1,(qsdk_onenet_value_t)&led0.current_state,0);
	qsdk_onenet_notify(light1_object,1,(qsdk_onenet_value_t)&led1.current_state,0);
	qsdk_onenet_notify(light2_object,1,(qsdk_onenet_value_t)&led2.current_state,0);
	
	while(1)
	{
		//上报数据前首先判断当前lwm2m 协议是否正常连接
		if(qsdk_onenet_get_connect()==RT_EOK)
		{
			LOG_D("data UP is open\r\n");			
			if(qsdk_onenet_notify(temp_object,0,(qsdk_onenet_value_t)&temperature,0)!=RT_EOK)
			{
				LOG_E("onenet notify error\r\n");
			}
			rt_thread_delay(100);
			if(qsdk_onenet_notify(hump_object,0,(qsdk_onenet_value_t)&humidity,0)!=RT_EOK)
			{
				LOG_E("onenet notify error\r\n");
			}
			
			//每40分钟定时更新一次(设备在线时间) lwm2m lifetime
			if(lifetime>40)
			{
				if(qsdk_onenet_update_time(0)!=RT_EOK)
				{
					LOG_E("onenet lifetime update error");
					rt_thread_delay(5000);
					if(qsdk_onenet_update_time(0)!=RT_EOK)
					{
						LOG_E("Two Update Errors in OneNet Lifetime");
					}
				}
				lifetime=0;
			}
		}
		else LOG_E("Now lwm2m is no connect\r\n");
		rt_memset(buf,0,sizeof(buf));
		lifetime++;
		rt_thread_delay(60000);
	}
}
