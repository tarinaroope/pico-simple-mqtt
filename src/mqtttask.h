#pragma once

#include "task.h"
#include "core_mqtt_agent.h"

typedef struct 
{
    const char* topic;
    size_t topic_length;
    const char* payload;
    size_t payload_length;
    TaskHandle_t parentTask;
    MQTTAgentContext_t* pMqttAgentContext;
} MQTTTaskParameters;

MQTTStatus_t mqtttask_publish(MQTTAgentContext_t* mqttAgentContext, MQTTPublishInfo_t* publishInfo);
MQTTStatus_t mqtttask_subscribe(MQTTAgentContext_t* mqttAgentContext, MQTTSubscribeInfo_t* subscribeInfo);
MQTTStatus_t mqtttask_ping(MQTTAgentContext_t* mqttAgentContext);

