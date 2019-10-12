/*
 * File      : qsdk_gps.c
 * This file is part of onenet in qsdk
 * Copyright (c) 2018-2030, Hbqs Development Team
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-09-15     longmain     first version
 */
#include "qsdk.h"
#include <at.h>
#include "string.h"
#include <stdlib.h>

#ifdef QSDK_USING_GPS

#ifdef RT_USING_ULOG
#define LOG_TAG              "[QSDK/GPS]"
#ifdef QSDK_USING_LOG
#define LOG_LVL              LOG_LVL_DBG
#else
#define LOG_LVL              LOG_LVL_INFO
#endif
#include <ulog.h>
#else
#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME              "[QSDK/GPS]"
#ifdef QSDK_USING_LOG
#define DBG_LEVEL                      DBG_LOG
#else
#define DBG_LEVEL                      DBG_INFO
#endif /* QSDK_DEBUG */

#include <rtdbg.h>
#endif


struct gnrmc_info
{
	unsigned int year;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char min;
	unsigned char sec;


	char longitude[11];             //经度 
	unsigned char longitude_suffix; //E=东或 W=西

	char latitude[12];               //纬度
	unsigned char latitude_suffix;   //N=北或 S=南

	float speed;                     //速度
	unsigned char position_valid;    // 数据有效性  0：无效   1：有效

};

int comma_split(unsigned char *pstr, char *buf, int buflen, int s, int e)
{
    int i, k;
    int commaCount = 0;
    int rc = -1;
    i = k = 0;
    if (e <= s)
    {
        return -1;
    }
    while (pstr[i] != '\0')
    {
        if (pstr[i] == ',')
        {
            commaCount++;
        }
        if (commaCount == s)
        {
            k = ++i;
            break;
        }
        i++;
    }

    while (pstr[i] != '\0')
    {
        if (pstr[i] == ',')
        {
            commaCount++;
        }
        if (commaCount == e)
        {
            if (i - k + 1 > buflen)
            {
                return -1;
            }
            rt_memset(buf, 0, sizeof(buf));
            memcpy(buf, pstr + k, i - k);

            rc = i - k;
            buf[rc] = '\0';
            break;
        }
        i++;
    }
    return rc;
}

int gnrmc_parse(unsigned char *pstr, struct gnrmc_info *pgnrmc_info)
{
    int len = 0;
    int commaCount = 0;
    char buf[32] = {0};
    // 计算字符串长度
    while (pstr[len] != '\0')
    {
        if (pstr[len] == ',')
        {
            commaCount++;
        }
        len++;
    }
    // 标准的逗号个数是12个,但是ME3616是13个
    if (commaCount >13)
    {
        return -1;
    }
    // 判断开始字符是:$GNRMC
    if (!((pstr[0] == '$') && (pstr[1] == 'G') && (pstr[2] == 'N') && (pstr[3] == 'R') && (pstr[4] == 'M') && (pstr[5] == 'C')))
    {
        return -1;
    }
    // 判断结束字符是:\r\n
    if ((pstr[len - 2] != '\r') && (pstr[len - 1] != '\n'))
    {
        return -1;
    }
    // UTC时间:第1个逗号与第2个逗号之前 (eg:092846.400 hhmmss.sss )
    len = comma_split(pstr, buf, sizeof(buf), 1, 2);
    if (len < 0)
    {
        return -1;
    }
    pgnrmc_info->hour = (buf[0] - '0') * 10 + (buf[1] - '0');
    pgnrmc_info->min = (buf[2] - '0') * 10 + (buf[3] - '0');
    pgnrmc_info->sec = (buf[4] - '0') * 10 + (buf[5] - '0');

    // 定位状态:第2个逗号与第3个逗号之前
    len = comma_split(pstr, buf, sizeof(buf), 2, 3);
    if (len != 1)
    {
        return -1;
    }
    pgnrmc_info->position_valid = ((buf[0] == 'A') ? 1 : 0);

    // 纬度
    len = comma_split(pstr, buf, sizeof(buf), 3, 4);
    if (len < 0)
    {
        return -1;
    }
    strcpy(pgnrmc_info->latitude,buf);
    // 纬度半球N(北半球)或S(南半球)
    len = comma_split(pstr, buf, sizeof(buf), 4, 5);
    if (len != 1)
    {
        return -1;
    }
    pgnrmc_info->latitude_suffix = buf[0];
    // 经度
    len = comma_split(pstr, buf, sizeof(buf), 5, 6);
    if (len < 0)
    {
        return -1;
    }
    strcpy(pgnrmc_info->longitude,buf);
    // 经度半球E(东经)或W(西经)
    len = comma_split(pstr, buf, sizeof(buf), 6, 7);
    if (len != 1)
    {
        return -1;
    }
    pgnrmc_info->longitude_suffix = buf[0];

		
		// 速度
    len = comma_split(pstr, buf, sizeof(buf), 7, 8);
    if (len < 0)
    {
        return -1;
    }
		pgnrmc_info->speed=atof(buf);
    // UTC日期:ddmmyy(日月年)格式 eg:070417
    len = comma_split(pstr, buf, sizeof(buf), 9, 10);
    if (len < 0)
    {
        return -1;
    }
    pgnrmc_info->day = (buf[0] - '0') * 10 + (buf[1] - '0');
    pgnrmc_info->month = (buf[2] - '0') * 10 + (buf[3] - '0');
    pgnrmc_info->year = (buf[4] - '0') * 10 + (buf[5] - '0') + 2000;

    return 0;
}

static struct gnrmc_info gnrmc;

#if (defined QSDK_USING_ME3616_GPS)

#define EVENT_GPS_DOWN_OK				(1<< 9)
#define EVENT_GPS_DOWN_FAILED		(1<<10)

//引用AT命令部分
extern at_response_t nb_resp;
extern at_client_t nb_client;

//定义事件控制块
extern rt_event_t nb_event;

/*************************************************************
*	函数名称：	qsdk_gps_set_mode
*
*	函数功能：	配置并且启用AGPS
*
*	入口参数：	mode : 0 冷启动     1 温启动     2 热启动
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_gps_start_mode(rt_uint8_t mode)
{
#ifdef QSDK_USING_LOG
	LOG_D("AT+ZGRST=%d\n",mode);
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+ZGRST=%d",mode)!=RT_EOK)	return RT_ERROR;

	return RT_EOK;
}

/*************************************************************
*	函数名称：	qsdk_gps_config
*
*	函数功能：	配置并且启用AGPS
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_agps_config(void)
{
	int i=90;
	char str[20];
	rt_uint32_t status;
	//设置定位模式为AGPS
#ifdef QSDK_USING_LOG
	LOG_D("AT+ZGMODE=1\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+ZGMODE=1")!=RT_EOK)	return RT_ERROR;
	//开始下载AGPS数据
#ifdef QSDK_USING_LOG
	LOG_D("AT+ZGDATA\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+ZGDATA")!=RT_EOK)	return RT_ERROR;

	//等待AGPS下载完成
	rt_event_recv(nb_event,EVENT_GPS_DOWN_FAILED|EVENT_GPS_DOWN_OK,RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,60000,&status);
	
	//AGPS下载成功后开启定位
	if(status==EVENT_GPS_DOWN_OK)
	{
		if(at_obj_exec_cmd(nb_client,nb_resp,"AT+ZGNMEA=2")!=RT_EOK)	return RT_ERROR;
		rt_thread_delay(100);
		return RT_EOK;
	}
	return	RT_ERROR;
}
/*************************************************************
*	函数名称：	qsdk_gps_config
*
*	函数功能：	配置并且启用GPS
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_gps_config(void)
{
	int i=90;
	char str[20];
#ifdef QSDK_USING_DEBUG
	LOG_D("AT+ZGMODE=2\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+ZGMODE=2")!=RT_EOK)	return RT_ERROR;
#ifdef QSDK_USING_DEBUG
	LOG_D("AT+ZGNMEA=2\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+ZGNMEA=2")!=RT_EOK)	return RT_ERROR;
	rt_thread_delay(100);
	return	RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_gps_set_out_time
*
*	函数功能：	配置GPS定位超时时间
*
*	入口参数：	time : 0-255
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_gps_set_out_time(rt_uint32_t time)
{
	if(time >255) time=255;
#ifdef QSDK_USING_DEBUG
	LOG_D("AT+ZGTMOUT=%d\n",time);
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+ZGTMOUT=%d",time)!=RT_EOK)	return RT_ERROR;

	return RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_gps_run
*
*	函数功能：	开启GPS
*
*	入口参数：	1 开始单次定位      2 开始跟踪定位
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_gps_run_mode(rt_uint8_t mode)
{
#ifdef QSDK_USING_DEBUG
	LOG_D("AT+ZGRUN=%d\n",mode);
#endif

	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+ZGRUN=%d",mode)!=RT_EOK)	return RT_ERROR;
	return	RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_gps_close
*
*	函数功能：	关闭GPS
*
*	入口参数：	无
*
*	返回参数：	0 成功  1	失败
*
*	说明：		
*************************************************************/
int qsdk_gps_close(void)
{
#ifdef QSDK_USING_LOG
	LOG_D("AT+ZGRUN=0\n");
#endif
	if(at_obj_exec_cmd(nb_client,nb_resp,"AT+ZGRUN=0")!=RT_EOK)	return RT_ERROR;
	return	RT_EOK;
}


void qsdk_gps_event_func(char *event)
{
	if(rt_strstr(event,"+ZGPS:")!=RT_NULL)
	{
		if(rt_strstr(event,"+ZGPS: DATA DOWNLOAD SUCCESS")==RT_NULL)
		{
			rt_event_send(nb_event,EVENT_GPS_DOWN_OK);
		}	
		else 
		{
			rt_event_send(nb_event,EVENT_GPS_DOWN_FAILED);
		}	
	}
	else if(rt_strstr(event,"$GNRMC")!=RT_NULL)
	{
		gnrmc_parse((unsigned char *)event,&gnrmc);
		if(gnrmc.position_valid == 1)
		{
			qsdk_gps_data_callback(gnrmc.longitude,gnrmc.latitude,gnrmc.speed);
		}
	}
}
#elif (defined QSDK_USING_AIR530_GPS)

at_client_t	gps_client=RT_NULL;
static char gps_buffer[50];
static char cmd_buffer[50];

/*************************************************************
*	函数名称：	gps_checksum
*
*	函数功能：	checksum计算
*
*	入口参数：	无
*
*	返回参数：	无
*
*	说明：		
*************************************************************/
int gps_checksum(char* Bytes,int len)
{
	int i, result;
	for (result = Bytes[0], i = 1; i < len ; i++)
	{
		result ^= Bytes[i];
	}
	return result;
}
/*************************************************************
*	函数名称：	qsdk_gps_start_mode
*
*	函数功能：	gps 重启
*
*	入口参数：	type: 1 热启动   2 温启动  3 冷启动
*
*	返回参数：	0：成功   1：失败
*
*	说明：		
*************************************************************/
int qsdk_gps_start_mode(int type)
{
	memset(gps_buffer,0,strlen(gps_buffer));
	memset(cmd_buffer,0,strlen(cmd_buffer));
	rt_sprintf(gps_buffer,"PGKC030,%d,1",type);
	rt_sprintf(cmd_buffer,"$%s*%02x\r\n",gps_buffer,gps_checksum(gps_buffer,rt_strlen(gps_buffer)));
	LOG_D("%s\n",cmd_buffer);
	if(at_client_obj_send(gps_client,cmd_buffer,strlen(cmd_buffer))==0)
	{
		LOG_E("gps reboot send failed\n");
		return RT_ERROR;
	}

	return RT_EOK;
}

/*************************************************************
*	函数名称：	qsdk_gps_erase_flash
*
*	函数功能：	擦除 flash 中的辅助定位数据
*
*	入口参数：	无
*
*	返回参数：	0：成功   1：失败
*
*	说明：		
*************************************************************/
int qsdk_gps_erase_flash(void)
{
	memset(gps_buffer,0,strlen(gps_buffer));
	rt_sprintf(gps_buffer,"$PGKC040*2B\r\n");
	LOG_D("%s\n",gps_buffer);
	if(at_client_obj_send(gps_client,gps_buffer,strlen(gps_buffer))==0)
	{
		LOG_E("gps erase flash failed\n");
		return RT_ERROR;
	}
	return RT_EOK;
}

/*************************************************************
*	函数名称：	qsdk_gps_enter_standby
*
*	函数功能：	进入 standby 低功耗模式
*
*	入口参数：	type:  0 stop模式   1 sleep模式
*
*	返回参数：	0：成功   1：失败
*
*	说明：		
*************************************************************/
int qsdk_gps_enter_standby(int type)
{
	memset(gps_buffer,0,strlen(gps_buffer));
	memset(cmd_buffer,0,strlen(cmd_buffer));
	rt_sprintf(gps_buffer,"PGKC051,%d",type);

	rt_sprintf(cmd_buffer,"$%s*%02x\r\n",gps_buffer,gps_checksum(gps_buffer,rt_strlen(gps_buffer)));
	LOG_D("%s\n",cmd_buffer);
	if(at_client_obj_send(gps_client,cmd_buffer,strlen(cmd_buffer))==0)
	{
		LOG_E("gps enter standby send failed\n");
		return RT_ERROR;
	}
	return RT_EOK;
}

/*************************************************************
*	函数名称：	qsdk_gps_set_nmea_out_time
*
*	函数功能：	配置输出 NMEA 消息的间隔（ms 单位）
*
*	入口参数：	time:  200-10000
*
*	返回参数：	0：成功   1：失败
*
*	说明：		
*************************************************************/
int qsdk_gps_set_nmea_out_time(int sec)
{
	if(sec<200)
		sec=200;
	memset(gps_buffer,0,strlen(gps_buffer));
	memset(cmd_buffer,0,strlen(cmd_buffer));
	rt_sprintf(gps_buffer,"PGKC101,%d",sec);

	rt_sprintf(cmd_buffer,"$%s*%02x\r\n",gps_buffer,gps_checksum(gps_buffer,rt_strlen(gps_buffer)));
	LOG_D("%s\n",cmd_buffer);
	if(at_client_obj_send(gps_client,cmd_buffer,strlen(cmd_buffer))==0)
	{
		LOG_E("gps set nmea out time send failed\n");
		return RT_ERROR;
	}
	return RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_gps_enter_low_power
*
*	函数功能：	进入周期性低功耗模式
*
*	入口参数：	time:  200-10000
*
*	返回参数：	0：成功   1：失败
*
*	说明：		
*************************************************************/
int qsdk_gps_enter_low_power(int mode,int run_time,int sleep_time)
{
	memset(gps_buffer,0,strlen(gps_buffer));
	memset(cmd_buffer,0,strlen(cmd_buffer));
	if(mode==1||mode==2)
	{
		rt_sprintf(gps_buffer,"PGKC105,%d,%d,%d",mode,run_time,sleep_time);	
	}
	else	rt_sprintf(gps_buffer,"PGKC105,%d,",mode);	
	
	rt_sprintf(cmd_buffer,"$%s*%02x\r\n",gps_buffer,gps_checksum(gps_buffer,rt_strlen(gps_buffer)));
	LOG_D("%s\n",cmd_buffer);
	if(at_client_obj_send(gps_client,cmd_buffer,strlen(cmd_buffer))==0)
	{
		LOG_E("gps enter low power send failed\n");
		return RT_ERROR;
	}
	return RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_gps_search_mode
*
*	函数功能：	搜星模式
*
*	入口参数：	无
*
*	返回参数：	无
*
*	说明：		
*************************************************************/
int qsdk_gps_search_mode(int gps_status,int glonass_status,int beidou_status,int galieo_status)
{
	memset(gps_buffer,0,strlen(gps_buffer));
	memset(cmd_buffer,0,strlen(cmd_buffer));
	rt_sprintf(gps_buffer,"PGKC115,%d,%d,%d,%d",gps_status,glonass_status,beidou_status,galieo_status);
	
	rt_sprintf(cmd_buffer,"$%s*%02x\r\n",gps_buffer,gps_checksum(gps_buffer,rt_strlen(gps_buffer)));
	LOG_D("%s\n",cmd_buffer);
	if(at_client_obj_send(gps_client,cmd_buffer,strlen(cmd_buffer))==0)
	{
		LOG_E("gps search mode send failed\n");
		return RT_ERROR;
	}
	return RT_EOK;
}
/*************************************************************
*	函数名称：	qsdk_gps_set_nmea_dis
*
*	函数功能：	配置输出 NMEA 语句输出使能
*
*	入口参数：	status:  0 关闭     1开启
*
*	返回参数：	0：成功   1：失败
*
*	说明：		
*************************************************************/
int qsdk_gps_set_nmea_dis(int status)
{
	memset(gps_buffer,0,strlen(gps_buffer));
	memset(cmd_buffer,0,strlen(cmd_buffer));
	rt_sprintf(gps_buffer,"PGKC242,0,%d,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0",status);

	rt_sprintf(cmd_buffer,"$%s*%02x\r\n",gps_buffer,gps_checksum(gps_buffer,rt_strlen(gps_buffer)));
	LOG_D("%s\n",cmd_buffer);
	if(at_client_obj_send(gps_client,cmd_buffer,strlen(cmd_buffer))==0)
	{
		LOG_E("gps set nmea dis send failed\n");
		return RT_ERROR;
	}
	return RT_EOK;
}

/*************************************************************
*	函数名称：	qsdk_gps_set_locat_info
*
*	函数功能：	设置大概位置以便快速定位
*
*	入口参数：	lon：经度 lat：纬度 year：年 month：月 day：日 hour：时 min：分 sec秒
*
*	返回参数：	0：成功   1：失败
*
*	说明：		
*************************************************************/
int qsdk_gps_set_locat_info(char *lon,char *lat,int year,char month,char day,char hour,char min,char sec)
{
	memset(gps_buffer,0,strlen(gps_buffer));
	memset(cmd_buffer,0,strlen(cmd_buffer));
	rt_sprintf(gps_buffer,"$PGKC639,%s,%s,0,%d,%d,%d,%d,%d,%d",lon,lat,year,month,day,hour,min,sec);

	rt_sprintf(cmd_buffer,"$%s*%02x\r\n",gps_buffer,gps_checksum(gps_buffer,rt_strlen(gps_buffer)));
	LOG_D("%s\n",cmd_buffer);
	if(at_client_obj_send(gps_client,cmd_buffer,strlen(cmd_buffer))==0)
	{
		LOG_E("gps set locat info send failed\n");
		return RT_ERROR;
	}
	return RT_EOK;
}


/*************************************************************
*	函数名称：	gps_event_func
*
*	函数功能：	模块主动下发处理事件
*
*	入口参数：	无
*
*	返回参数：	无
*
*	说明：		
*************************************************************/
void gps_event_func(struct at_client *client,const char *data, rt_size_t size)
{
	if(rt_strstr(data,"$GNRMC")!=RT_NULL)
	{
		gnrmc_parse((unsigned char *)data,&gnrmc);
		if(gnrmc.position_valid == 1)
		{
			qsdk_gps_data_callback(gnrmc.longitude,gnrmc.latitude,gnrmc.speed);
		}
	}
}

/*************************************************************
*
*	说明：		模块主动下发消息识别结构体
*
*************************************************************/
static struct at_urc gps_urc_table[]={
	{"$GNRMC",		"\r\n",gps_event_func},
};
/*************************************************************
*	函数名称：	qsdk_mqtt_config
*
*	函数功能：	MQTT客户端配置
*
*	入口参数：	无
*
*	返回参数：	0：成功   1：失败
*
*	说明：		
*************************************************************/
int gps_init_environment(void)
{
	rt_device_t uart_device;
	struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT; 
	uart_device=rt_device_find(QSDK_GPS_UART);
	if(uart_device==RT_NULL)
	{
		LOG_D("qsdk gps init failed,(%s) find failed\n",QSDK_GPS_UART);
		return RT_ERROR;
	}
	//修改串口波特率
	config.baud_rate=9600;
	rt_device_control(uart_device, RT_DEVICE_CTRL_CONFIG, &config);
	
	//初始化 GPS 串口
	at_client_init(QSDK_GPS_UART,1024);
	gps_client=at_client_get(QSDK_GPS_UART);
	at_obj_set_urc_table(gps_client,gps_urc_table,sizeof(gps_urc_table)/sizeof(gps_urc_table[0]));
	qsdk_gps_set_nmea_dis(1);
	return RT_EOK;
}

INIT_APP_EXPORT(gps_init_environment);

#endif //endif GPS选择

#endif //endif QSDK_USING_GPS
