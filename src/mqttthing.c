#include "mqttthing.h"
#include "wifihelper.h"
#include "mqttagent.h"
#include "queue.h"

#define TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)

static void MQTTOfflineCallback(void *context)
{
    MQTTThing* self = (MQTTThing*) context;
    if (self->connectCallback)
    {
        self->connectCallback(false);
    }
}

static void MQTTOnlineCallback(void *context)
{
    MQTTThing* self = (MQTTThing*) context;

    // Update the last message timestamp to ensure we send a keepalive ping
    xSemaphoreTake(self->mutex, portMAX_DELAY);
    self->lastMessageTimestamp = pdTICKS_TO_MS(xTaskGetTickCount());
    xSemaphoreGive(self->mutex);
    if (self->connectCallback)
    {
        self->connectCallback(true);
    }
}

static void MQTTIncomingCallback(   void *context,
                                    const char *topic,
                                    size_t topic_length,
                                    const char *payload,
                                    size_t payload_length)
{
    MQTTThing* self = (MQTTThing*) context;
    if (self->subscribeCallback)
    {
        self->subscribeCallback(topic, topic_length, payload, payload_length);
    }
}

static void mqttthing_vTaskConnectLoop(void *params)
{
    MQTTThing* self = (MQTTThing*) params;

    if (wifi_init()) 
    {
        LogDebug(("Wifi Controller Initialised\n"));
    } 
    else 
    {
        LogError(("Failed to initialise controller\n"));
        return;
    }

    // Join wifi, no errors as we try forever
    wifi_join(self->ssid, self->password, true); 

    // Print MAC Address
    wifi_getMACAddressStr(self->macStr);
    LogDebug(("MAC ADDRESS: %s\n", self->macStr));

    self->mqttObs.MQTTOffline = MQTTOfflineCallback;
    self->mqttObs.MQTTOnline = MQTTOnlineCallback;
    self->mqttObs.MQTTIncomingPublish = MQTTIncomingCallback;
    self->mqttObs.context = self;
    mqttagent_setObserver(&self->mqttAgent, &self->mqttObs);
    sprintf(self->topicBuffer, "%s/%s", self->macStr, TOPICSTATUS);
    char willpayload[2];
    sprintf(willpayload, "%d", 0);
    mqttagent_setData(&self->mqttAgent, 
                        self->mqtt_user, 
                        self->mqtt_passwd, 
                        self->macStr, 
                        self->topicBuffer, 
                        willpayload);
   
    LogDebug(("Connecting to: %s(%d)\n", self->mqtt_host, self->mqtt_port));
    LogDebug(("Client id: %.4s...\n", mqttagent_getId(&self->mqttAgent)));
    LogDebug(("User id: %.4s...\n", self->mqtt_user));

    mqttagent_connect(&self->mqttAgent, self->mqtt_host, self->mqtt_port, true);
    mqttagent_start(&self->mqttAgent, TASK_PRIORITY);

    while (true)
    {
        if (mqttagent_getConnectionState(&self->mqttAgent) == Online)
        {
            // Check if we need to send keepalive ping
            TickType_t currentTime = pdTICKS_TO_MS(xTaskGetTickCount());
            xSemaphoreTake(self->mutex, portMAX_DELAY);
            TickType_t ttlTimestamp = self->lastMessageTimestamp;
            xSemaphoreGive(self->mutex);

            if ((currentTime - ttlTimestamp) > (MQTTKEEPALIVETIME * 1000 / 2))
            {
                LogDebug(("Sending keepalive ping after %d ms of idling\n", currentTime - ttlTimestamp));
                mqttagent_mqttPing(&self->mqttAgent);
                xSemaphoreTake(self->mutex, portMAX_DELAY);
                self->lastMessageTimestamp = currentTime;
                xSemaphoreGive(self->mutex);
            }
        }

        if (!wifi_isJoined())
        {
            LogError(("AP Link is down\n"));
            wifi_join(self->ssid, self->password, true);
        }
    }
}

void mqttthing_connectLoop(MQTTThing* self, void (*callback)(bool online))
{  
    if (callback)
    {
        self->connectCallback = callback;
    }
    xTaskCreate(
        mqttthing_vTaskConnectLoop,
        "MQTTThing_Main",
        2048,
        (void *) self,
        (tskIDLE_PRIORITY + 1UL),
        &self->mainTask);
}

void mqttthing_publish(MQTTThing* self, char* topic, char* payload)
{
    sprintf(self->topicBuffer, "%s/%s", self->macStr, topic); // copy to
    mqttagent_mqttPublish(&self->mqttAgent, self->topicBuffer, payload);
    
    xSemaphoreTake(self->mutex, portMAX_DELAY);
    self->lastMessageTimestamp = pdTICKS_TO_MS(xTaskGetTickCount());
    xSemaphoreGive(self->mutex);
}

void mqttthing_subscribe(MQTTThing* self, char* topic, void (*callback)())
{
    self->subscribeCallback = callback;
    sprintf(self->topicBuffer, "%s/%s", self->macStr, topic); // copy to
    mqttagent_mqttSubscribe(&self->mqttAgent, self->topicBuffer);
    xSemaphoreTake(self->mutex, portMAX_DELAY);
    self->lastMessageTimestamp = pdTICKS_TO_MS(xTaskGetTickCount());
    xSemaphoreGive(self->mutex);
}


bool mqttthing_init(
                    MQTTThing* self,
                    const char* ssid,
                    const char* password,
                    const char* mqtt_host,
                    const int mqtt_port,
                    const char* mqtt_user,
                    const char* mqtt_passwd
                    )

{
    memset(self, 0, sizeof(MQTTThing));

    self->ssid = pvPortMalloc(strlen(ssid) + 1);
    self->password = pvPortMalloc(strlen(password) + 1);
    self->mqtt_host = pvPortMalloc(strlen(mqtt_host) + 1);
    self->mqtt_port = mqtt_port;
    self->mqtt_user = pvPortMalloc(strlen(mqtt_user) + 1);
    self->mqtt_passwd = pvPortMalloc(strlen(mqtt_passwd) + 1);

    self->mutex = xSemaphoreCreateMutex();
   
    if (self->ssid == NULL || 
        self->password == NULL || 
        self->mqtt_host == NULL || 
        self->mqtt_user == NULL || 
        self->mqtt_passwd == NULL ||
        self->mutex == NULL)
    {
        mqttthing_destroy(self);
        return false;
    }

    strcpy(self->ssid, ssid);
    strcpy(self->password, password);
    strcpy(self->mqtt_host, mqtt_host);
    strcpy(self->mqtt_user, mqtt_user);
    strcpy(self->mqtt_passwd, mqtt_passwd);

    return true;
}

void mqttthing_destroy(MQTTThing* self)
{
    vPortFree(self->ssid);
    vPortFree(self->password);
    vPortFree(self->mqtt_host);
    vPortFree(self->mqtt_user);
    vPortFree(self->mqtt_passwd);
    vSemaphoreDelete(self->mutex);
}