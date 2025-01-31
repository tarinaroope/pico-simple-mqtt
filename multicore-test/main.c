
#include <stdio.h>

#include "FreeRTOS.h"

#include "wifihelper.h"
#include "lwip/dns.h"
#include "lwip/ip4_addr.h"
#include "pico/cyw43_arch.h"

#include "pico/multicore.h"
#include "pico/util/queue.h"


#include "lwip/sockets.h"
#include "pico/stdlib.h"
#include "task.h"
#include "mqttagent.h"

// Check these definitions where added from the makefile
#ifndef WIFI_SSID
#error "WIFI_SSID not defined"
#endif
#ifndef WIFI_PASSWORD
#error "WIFI_PASSWORD not defined"
#endif
#ifndef MQTT_CLIENT
#error "MQTT_CLIENT not defined"
#endif
#ifndef MQTT_USER
#error "MQTT_PASSWD not defined"
#endif
#ifndef MQTT_PASSWD
#error "MQTT_PASSWD not defined"
#endif
#ifndef MQTT_HOST
#error "MQTT_HOST not defined"
#endif
#ifndef MQTT_PORT
#error "MQTT_PORT not defined"
#endif

#define TEST_PUBLISH_FREQUENCY 5000 // ms
static const char* WILLTOPIC = "SENSORHUB/STATUS";
static const char* WILLPAYLOAD = "{'online':0}";

#define TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)

typedef struct
{
    uint8_t sensorId;
    uint8_t sensorValue;
} queue_entry_t;

queue_t message_queue;

void MQTTOfflineCallback(void* context)
 {
    printf("Offline callback called\n");
 }
void MQTTOnlineCallback(void* context)
{
    printf("Online callback called\n");
}

void MQTTCommandCompleteCallback(void* context)
{
    printf("Command complete callback called\n");
}

void MQTTIncomingCallback(void* context, 
                        const char* topic, 
                        size_t topic_length, 
                        const char* payload, 
                        size_t payload_length)
{
    printf("Incoming publish to topic %.*s, payload %.*s\n", topic_length, topic, payload_length, payload);
}

void main_task(void *params) 
{
    if (wifi_init()) {
        LogDebug("Wifi Controller Initialised\n");
    } else {
        LogError("Failed to initialise controller\n");
        return;
    }

    LogInfo("Connecting to WiFi... %s \n", WIFI_SSID);

    if (wifi_join(WIFI_SSID, WIFI_PASSWORD)) {
        LogInfo("Connect to Wifi\n");
    } else {
        LogError("Failed to connect to Wifi \n");
    }

    // Setup for MQTT Connection
    char mqttTarget[] = MQTT_HOST;
    int mqttPort = MQTT_PORT;
    char mqttClient[] = MQTT_CLIENT;
    char mqttUser[] = MQTT_USER;
    char mqttPwd[] = MQTT_PASSWD;

    MQTTAgent mqttAgent;

    MQTTAgentObserver mqttObs = {0};
    // Store main task handle for further notifications
    mqttObs.context = (void*) xTaskGetCurrentTaskHandle();
    mqttObs.MQTTOffline = MQTTOfflineCallback;
    mqttObs.MQTTOnline = MQTTOnlineCallback;
    mqttObs.MQTTCommandCompleted = MQTTCommandCompleteCallback;
    mqttObs.MQTTIncomingPublish = MQTTIncomingCallback;
    mqttagent_setObserver(&mqttAgent, &mqttObs);
    mqttagent_setData(&mqttAgent, mqttUser, mqttPwd, mqttClient, WILLTOPIC, WILLPAYLOAD);
    LogDebug("Connecting to: %s(%d)\n", mqttTarget, mqttPort);
    LogDebug("Client id: %.4s...\n", mqttagent_getId(&mqttAgent));
    LogDebug("User id: %.4s...\n", mqttUser);

    mqttagent_connect(&mqttAgent, mqttTarget, mqttPort, true);
    mqttagent_start(&mqttAgent, TASK_PRIORITY);

    TickType_t ttlTimestamp = pdTICKS_TO_MS( xTaskGetTickCount() );

    while (true) 
    {
        if (mqttagent_getConnectionState(&mqttAgent) == Online)
        {
            queue_entry_t entry = {0};

            bool messageToSend = queue_try_remove(&message_queue, &entry);
            while (messageToSend)
            {
                // Todo proper preparing of message
                        char topic[40];
                        char payload[20];
                        sprintf(topic,"TOPIC/%d/UPDATE", cpEntry.sensorId);
                        sprintf(payload,"payload:%d", cpEntry.sensorValue);
                        printf("Publishing to %s, payload %s\n", topic, payload);
                        mqttagent_mqttPublish(&mqttAgent, topic, payload);
                        lastTimestamp = currentTime;
                        ttlTimestamp = currentTime;
                // Check if we have more messages to send
                messageToSend = queue_try_remove(&message_queue, &entry);  
            }
            // Check if we need to send keepalive ping
            TickType_t currentTime = pdTICKS_TO_MS( xTaskGetTickCount() );

            if ((currentTime - ttlTimestamp) > (MQTTKEEPALIVETIME * 1000 / 2))
            {
                LogDebug("Sending keepalive ping after %d ms of idling\n", currentTime - ttlTimestamp);
                mqttagent_mqttPing(&mqttAgent);
                ttlTimestamp = currentTime;
            }    
        }
      
        if (!wifi_isJoined()) 
        {
            LogError("AP Link is down\n");

            if (wifi_join(WIFI_SSID, WIFI_PASSWORD)) {
                LogInfo("Connect to Wifi\n");
            } else {
                LogError("Failed to connect to Wifi \n");
            }
        }
    }
    vTaskDelay( pdMS_TO_TICKS( 2000U ) );
}

void vLaunch(void) {
    TaskHandle_t task;

    xTaskCreate(main_task, "MainThread", 2048, NULL, TASK_PRIORITY, &task);

    /* Start the tasks and timer running. */
    vTaskStartScheduler();
}

void core1_main(void)
{
     /* Configure the hardware ready to run the demo. */
    const char *rtos_name;
    rtos_name = "FreeRTOS";
    printf("Starting %s on core 1:\n", rtos_name);
    sleep_ms(1000);

    vLaunch();
}

int main(void) 
{
    timer_hw->dbgpause = 0; // hack!
    stdio_init_all();

    sleep_ms(1000);
    
    queue_init(&message_queue, sizeof(queue_entry_t), 10);

    multicore_launch_core1(core1_main);

    sleep_ms(1000);
    queue_entry_t entry = {0};
    uint8_t i = 1;
    while (1)
    {
        entry.sensorId = i;
        entry.sensorValue = i+2;
        queue_try_add(&message_queue, &entry);
        sleep_ms(5000);
        i++;
    }

    /* Configure the hardware ready to run the demo. */
    /*
    const char *rtos_name;
    rtos_name = "FreeRTOS";
    printf("Starting %s on core 0:\n", rtos_name);
    sleep_ms(1000);

    vLaunch();
*/

    return 0;
}