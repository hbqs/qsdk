/*
 * File      : qsdk.h
 * This file is part of fun in qsdk
 * Copyright (c) 2018-2030, longmain Development Team
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-11-14     longmain     first version
 * 2019-06-13     longmain     add hexstring to string
 * 2019-06-13     longmain     add net close callback
 * 2019-06-13     longmain     del qsdk_onenet_init_environment
 * 2019-06-30     longmain     add qsdk_nb_clear_environment
 */

#ifndef __QSDK_H__

#define __QSDK_H__
#include <rtthread.h>
#include "rtdevice.h"
enum qsdk_nb_status_type
{
	qsdk_nb_status_module_init_ok=0,
	qsdk_nb_status_reboot,
	qsdk_nb_status_fota,
	qsdk_nb_status_enter_psm,
	qsdk_nb_status_exit_psm,
	qsdk_nb_status_psm_run,
#ifdef QSDK_USING_IOT
	qsdk_iot_status_reg_init,
	qsdk_iot_status_reg_failure,
	qsdk_iot_status_reg_success,
	qsdk_iot_status_observer_failure,
	qsdk_iot_status_observer_success,
	qsdk_iot_status_unreg_success,
	qsdk_iot_status_notify_init,
	qsdk_iot_status_notify_failure,
	qsdk_iot_status_notify_success,
	qsdk_iot_status_notify_timeout
#endif
};

struct nb_device
{
	int  sim_state;
	int  device_ok;
	int  reboot_open;
	int  fota_open;
	int  reboot_type;
	int  psm_status;
	int  net_connect_ok;
	int  error;
	int  csq;
	char imsi[16];
	char imei[16];
	char ip[16];
};


//qsdk_nb_fun
int qsdk_nb_quick_connect(void);
int qsdk_nb_wait_connect(void);
int qsdk_nb_reboot(void);
int qsdk_nb_sim_check(void);
int qsdk_nb_set_psm_mode(char *tau_time,char *active_time);
char *qsdk_nb_get_imsi(void);
char *qsdk_nb_get_imei(void);
int qsdk_nb_get_time(void);
int qsdk_nb_get_csq(void);
int qsdk_nb_set_net_start(void);
int qsdk_nb_get_net_connect(void);
int qsdk_nb_get_net_connect_status(void);
int qsdk_nb_get_reboot_event(void);
int qsdk_nb_open_net_light(void);
int qsdk_nb_close_net_light(void);
int qsdk_nb_open_auto_psm(void);
int qsdk_nb_close_auto_psm(void);
void qsdk_nb_enter_psm(void);
int qsdk_nb_exit_psm(void);
int qsdk_nb_get_psm_status(void);
char *qsdk_nb_query_ip(void);
int qsdk_nb_ping_ip(char *ip);
void qsdk_nb_clear_environment(void);
int string_to_hex(const char *pString, int len, char *pHex);
void hexstring_to_string(char * pHex,int len, char * pString);

//qsdk_iot_fun


#ifdef QSDK_USING_IOT
int qsdk_iot_open_update_status(void);
int qsdk_iot_open_down_date_status(void);
int qsdk_iot_update(char *str);
#endif



//qsdk_net_fun
#ifdef QSDK_USING_NET

enum	NET_DATA_TYPE
{
	QSDK_NET_TYPE_TCP=1,
	QSDK_NET_TYPE_UDP,
};

struct net_stream
{
	int		type;
	int 	port;
	int 	socket;
	int 	server_port;
	int 	revice_status;
	int 	revice_len;
	int		user_status;
	int		connect_status;
	char	*server_ip;
	char 	*rev_data;
};
typedef struct net_stream *qsdk_net_client_t;


qsdk_net_client_t qsdk_net_client_init(int type,int port,char *server_ip,unsigned short server_port);
int qsdk_net_create_socket(qsdk_net_client_t client);
int qsdk_net_connect_to_server(qsdk_net_client_t client);
int qsdk_net_send_data(qsdk_net_client_t client,char *str);
int qsdk_net_get_client_revice(qsdk_net_client_t client);
int qsdk_net_get_client_connect(qsdk_net_client_t client);
int qsdk_net_close_socket(qsdk_net_client_t client);
int qsdk_net_clear_environment(void);
#endif

//qsdk_onenet_fun

#ifdef QSDK_USING_ONENET

enum  qsdk_onenet_value_type
{
	 qsdk_onenet_value_string=1,
	 qsdk_onenet_value_opaque,
	 qsdk_onenet_value_integer,
	 qsdk_onenet_value_float,
	 qsdk_onenet_value_bool,
	 qsdk_onenet_value_hexStr
};

enum qsdk_onenet_status_type
{
	qsdk_onenet_status_init=0,
	qsdk_onenet_status_run,
	qsdk_onenet_status_failure,
	qsdk_onenet_status_success=4,
	qsdk_onenet_status_close_init=0,
	qsdk_onenet_status_close_start,
	qsdk_onenet_status_update_init=10,
	qsdk_onenet_status_update_failure,
	qsdk_onenet_status_update_success,
	qsdk_onenet_status_update_timeout=14,
	qsdk_onenet_status_update_need=18,
	qsdk_onenet_status_result_read_success=1,
	qsdk_onenet_status_result_write_success,
	qsdk_onenet_status_result_Bad_Request=11,
	qsdk_onenet_status_result_Unauthorized,
	qsdk_onenet_status_result_Not_Found,
	qsdk_onenet_status_result_Method_Not_Allowed,
	qsdk_onenet_status_result_Not_Acceptable
};

union qsdk_onenet_value
{
	 char *string_value;
	 uint16_t int_value;
	 float float_value;
	 _Bool bool_value;
};

typedef union qsdk_onenet_value *qsdk_onenet_value_t;

struct onenet_stream
{
	int objid;
	int inscount;
	char *bitmap;
	int atts;
	int acts;
	int insid;
	int resid;
	int valuetype;
	int msgid;
	int read_status;
	int write_status;
	int exec_status;
	int user_status;
};

typedef struct onenet_stream *qsdk_onenet_stream_t;

qsdk_onenet_stream_t qsdk_onenet_object_init(int objid,int insid,int resid,int inscount,char *bitmap,int atts,int acts,int type);
int qsdk_onenet_delete_instance(void);
int qsdk_onenet_delete_object(qsdk_onenet_stream_t stream);
int qsdk_onenet_open(void);
int qsdk_onenet_close(void);
int qsdk_onenet_update_time(int flge);
int qsdk_onenet_notify(qsdk_onenet_stream_t stream,int len,qsdk_onenet_value_t data,int flge);
int qsdk_onenet_read_rsp(int msgid,int result,qsdk_onenet_stream_t stream,int len,qsdk_onenet_value_t data,int index,int flge);
int qsdk_onenet_quick_start(void);
int qsdk_onenet_get_connect(void);
int qsdk_onenet_get_object_read(qsdk_onenet_stream_t stream);
int qsdk_onenet_get_object_write(qsdk_onenet_stream_t stream);
int qsdk_onenet_get_object_exec(qsdk_onenet_stream_t stream);
int qsdk_onenet_clear_environment(void);

#if	(defined QSDK_USING_M5310)||(defined QSDK_USING_M5310A)
int qsdk_onenet_notify_and_ack(qsdk_onenet_stream_t stream,int len,qsdk_onenet_value_t data,int flge);
#endif
#endif


#ifdef QSDK_USING_MQTT

struct mqtt_stream
{
	char topic[MQTT_TOPIC_NAME_MAX];
	int	qos;
	int	retain;
	int	dup;
	int	mesg_type;
	int	suback_ststus;
	int puback_status;
	int	user_status;
	int revice_status;
};

typedef struct mqtt_stream *qsdk_mqtt_topic_t;

int qsdk_mqtt_config(void);
int qsdk_mqtt_check_config(void);
qsdk_mqtt_topic_t qsdk_mqtt_topic_init(const char *topic,int qos,int retain,int dup,int mesg_type);
int qsdk_mqtt_open(void);
int qsdk_mqtt_get_connect_status(void);
int qsdk_mqtt_sub(qsdk_mqtt_topic_t topic);
int qsdk_mqtt_check_sub(qsdk_mqtt_topic_t topic);
int qsdk_mqtt_pub_topic(qsdk_mqtt_topic_t topic,char *mesg);
int qsdk_mqtt_pub_stream(char *mesg,int qos);
int qsdk_mqtt_set_ack_timeout(int timeout);
int qsdk_mqtt_open_ack_dis(void);
int qsdk_mqtt_close_ack_dis(void);
int qsdk_mqtt_unsub(qsdk_mqtt_topic_t topic);
int qsdk_mqtt_close(void);
int qsdk_mqtt_delete(void);
int qsdk_mqtt_get_connect(void);
int qsdk_mqtt_get_error_type(void);
#endif

void qsdk_rtc_set_time_callback(int year,char month,char day,char hour,char min,char sec,char week);

int qsdk_net_data_callback(char *data,int len);
void qsdk_net_close_callback(void);
int qsdk_iot_data_callback(char *data,int len);

int qsdk_onenet_close_callback(void);
int qsdk_onenet_read_rsp_callback(int msgid,int insid,int resid);
int qsdk_onenet_write_rsp_callback(int len,char* value);
int qsdk_onenet_exec_rsp_callback(int len,char* cmd);
void qsdk_onenet_fota_callback(void);



int qsdk_mqtt_data_callback(char *topic,char *mesg,int mesg_len);
void qsdk_nb_reboot_callback(void);

#endif	//qsdk.h end

