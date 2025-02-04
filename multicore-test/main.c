
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
#include "mqttthing.h"

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

#define TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)

const char *WIFISSID = WIFI_SSID;
const char *WIFIPASSWORD = WIFI_PASSWORD;
const char *MQTTHOST = MQTT_HOST;
const int MQTTPORT = MQTT_PORT;
const char *MQTTUSER = MQTT_USER;
const char *MQTTPASSWD = MQTT_PASSWD;
const char *TOPICROOT = "SENSOR";
const char *TOPICTEMPERATURE = "TEMPERATURE";

typedef struct
{
    uint8_t sensorId;
    uint8_t sensorValue;
} queue_entry_t;

queue_t message_queue;

void runTimeStats()
{
    TaskStatus_t *pxTaskStatusArray;
    volatile UBaseType_t uxArraySize, x;
    unsigned long ulTotalRunTime;

    /* Take a snapshot of the number of tasks in case it changes while this
    function is executing. */
    uxArraySize = uxTaskGetNumberOfTasks();
    printf("Number of tasks %d\n", uxArraySize);

    /* Allocate a TaskStatus_t structure for each task.  An array could be
    allocated statically at compile time. */
    pxTaskStatusArray = (TaskStatus_t *)pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

    if (pxTaskStatusArray != NULL)
    {
        /* Generate raw status information about each task. */
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray,
                                           uxArraySize,
                                           &ulTotalRunTime);

        /* For each populated position in the pxTaskStatusArray array,
        format the raw data as human readable ASCII data. */
        for (x = 0; x < uxArraySize; x++)
        {
            printf("Task: %d \t cPri:%d \t bPri:%d \t hw:%d \t%s\n",
                   pxTaskStatusArray[x].xTaskNumber,
                   pxTaskStatusArray[x].uxCurrentPriority,
                   pxTaskStatusArray[x].uxBasePriority,
                   pxTaskStatusArray[x].usStackHighWaterMark,
                   pxTaskStatusArray[x].pcTaskName);
        }

        /* The array is no longer needed, free the memory it consumes. */
        vPortFree(pxTaskStatusArray);
    }
    else
    {
        printf("Failed to allocate space for stats\n");
    }

    HeapStats_t heapStats;
    vPortGetHeapStats(&heapStats);
    printf("HEAP avl: %d, blocks %d, alloc: %d, free: %d\n",
           heapStats.xAvailableHeapSpaceInBytes,
           heapStats.xNumberOfFreeBlocks,
           heapStats.xNumberOfSuccessfulAllocations,
           heapStats.xNumberOfSuccessfulFrees);
}

void main_task(void *params)
{
    MQTTThing thing;

     // Setup for MQTT Connection
    char mqttTarget[] = MQTT_HOST;
    int mqttPort = MQTT_PORT;
    char mqttClient[] = MQTT_CLIENT;
    char mqttUser[] = MQTT_USER;
    char mqttPwd[] = MQTT_PASSWD;

    if (!mqttthing_init(&thing, WIFISSID, WIFIPASSWORD, MQTTHOST, MQTTPORT, MQTTUSER, MQTTPASSWD))
    {
        LogError(("Failed to initialize MQTTThing\n"));
        return;
    }

    mqttthing_connectLoop(&thing);

    char topicBuffer[40];
    char payloadBuffer[10];
    queue_entry_t entry = {0};
    int i = 0;
    while (true)
    {
        if (i ==9)
        {
            i = 0;
            runTimeStats();
        }
        if (queue_try_remove(&message_queue, &entry))
        {
            sprintf(topicBuffer, "%s/%s/%d", TOPICROOT, TOPICTEMPERATURE, entry.sensorId);
            sprintf(payloadBuffer, "%d", entry.sensorValue);
            printf("Publishing to %s, payload %s\n", topicBuffer, payloadBuffer);
            mqttthing_publish(&thing, topicBuffer, payloadBuffer);
        }
        vTaskDelay(pdTICKS_TO_MS(1000U));
        i++;
    }
    
}

void vLaunch(void)
{
    TaskHandle_t task;

    xTaskCreate(main_task, "MainThread", 512, NULL, TASK_PRIORITY, &task);

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

    sleep_ms(20000);
    queue_entry_t entry = {0};
    uint8_t i = 1;
    while (1)
    {
        entry.sensorId = i;
        entry.sensorValue = i + 2;
        queue_try_add(&message_queue, &entry);
        sleep_ms(5000);
        i++;
    }
    return 0;
}