#pragma once
#include "FreeRTOS.h"
#include "stdlib.h"
#include "task.h"
#include "queue.h"
#include "mqttagent.h"

#define TOPICSTATUS "STATUS"
#define MAXTOPICLEN 64


typedef struct  
{
    char* ssid;
    char* password;
    char* mqtt_host;
    int mqtt_port;
    char* mqtt_user;
    char* mqtt_passwd;
    char topicBuffer[MAXTOPICLEN];
    char macStr[20];

    MQTTAgent mqttAgent;
    MQTTAgentObserver mqttObs;

    TaskHandle_t mainTask;

    TickType_t lastMessageTimestamp;
    SemaphoreHandle_t mutex;

    void (*subscribeCallback)(const char* topic, 
                                size_t topic_length, 
                                const char* payload, 
                                size_t payload_length);
} MQTTThing;

bool mqttthing_init(
                    MQTTThing* self,
                    const char* ssid,
                    const char* password,
                    const char* mqtt_host,
                    const int mqtt_port,
                    const char* mqtt_user,
                    const char* mqtt_passwd
                    );

void mqttthing_connectLoop(MQTTThing* self);
//void mqttthing_publish(MQTTThing* self, PublishMessage* message);
void mqttthing_publish(MQTTThing* self, char* topic, char* payload);
void mqttthing_subscribe(MQTTThing* self, char* topic, void (*callback)());
void mqttthing_destroy(MQTTThing* self);