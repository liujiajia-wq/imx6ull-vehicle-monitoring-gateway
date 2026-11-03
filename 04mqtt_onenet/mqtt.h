#ifndef HEAD_H
#define HEAD_H


#include <MQTTAsync.h>
#include <MQTTClient.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include <string.h>
//#define OLD_ADDRESS     "tcp://218.201.45.7:1883"
#define NEW_ADDRESS     "tcp://183.230.40.96:1883"
#define DEV_NAME    "car_device"   //"device_name"  //"first_device"
#define CLIENTID DEV_NAME
#define PRODUCT_ID    "01imm49Gt8"  //"63fg7NqRxY"  //"3XoV8495r6"
#define PASSWD     "version=2018-10-31&res=products%2F01imm49Gt8%2Fdevices%2Fdevice01&et=1837255523&method=md5&sign=suuWoPofZsTltbh6IDhZpw%3D%3D"
conn_opts.MQTTVersion = MQTTVERSION_31_1;   /* 4 = MQTT 3.1.1 */
//"version=2018-10-31&res=products%2F63fg7NqRxY%2Fdevices%2Fdevice_name&et=3937255523&method=md5&sign=oCu6NsiOgu2LWSV8HnjYUA%3D%3D"
//"version=2018-10-31&res=products%2F3XoV8495r6%2Fdevices%2Ffirst_device&et=1837255523&method=sha1&sign=dtVQufcgwCrn5zmdmWLXFEtJAQY%3D";
#define QOS         0
#define TIMEOUT     10000L



extern int mqtt_init();
extern int mqtt_send(char * key, float value);


//#define __cplusplus
#endif // HEAD_H
