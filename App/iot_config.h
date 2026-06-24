#ifndef __IOT_CONFIG_H__
#define __IOT_CONFIG_H__
// clang-format off
#define WIFI_SSID "G16E_0563"
#define WIFI_PWD "0iM325]3"

#define WIFI_USE_SMART_CONFIG true
#define SMART_CONFIG_WAITING_TIME_MS 40000UL

#define MQTT_HOST "mqtts.heclouds.com"
#define MQTT_PORT "1883"
#define MQTT_DEVICE_NAME "GD32Air"
#define MQTT_PRODUCT_ID "4m3RoDJR8n"
#define MQTT_DEVICE_TOKEN "version=2018-10-31&res=products%2F4m3RoDJR8n%2Fdevices%2FGD32Air&et=1830268800&method=md5&sign=8k9ydYXFg443djGqlQPzAg%3D%3D"

#define MQTT_CONN_CMD(reconnect) "AT+MQTTCONN=0,\"" MQTT_HOST "\"," MQTT_PORT "," reconnect "\r\n"
#define MQTT_USERCFG_CMD(keepAlive, qos, retain)  "AT+MQTTUSERCFG=0,"keepAlive",\""MQTT_DEVICE_NAME"\",\""MQTT_PRODUCT_ID"\",\""MQTT_DEVICE_TOKEN"\","qos","retain",\"\"\r\n"
// clang-format on
#define MQTT_DAT_UPLOAD_PERIOD_MS 3000UL

#define UTC(x) #x
#define SNTP_SERVER_1 "cn.ntp.org.cn"
#define SNTP_SERVER_2 "ntp.sjtu.edu.cn"

#define SNTP_REQ_PERIOD_MS 3000

#endif // __IOT_CONFIG_H__
