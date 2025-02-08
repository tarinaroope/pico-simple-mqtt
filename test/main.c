
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
#define TOPICROOT "TESTROOT"
#define TOPICTEST "TESTTEMP"

volatile bool subscribeDone = false;

#define TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)

void incomingPublish (const char* topic, 
                                size_t topic_length, 
                                const char* payload, 
                                size_t payload_length)
{
    printf("Incoming publish %.*s:%.*s\n", topic_length, topic, payload_length, payload);
}

void connectCallback(bool online)
{
    if (!online)
    {
        printf("Disconnected from MQTT broker\n");
    }
    else
    {
        printf("Connected to MQTT broker\n");
    }
}

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

    if (!mqttthing_init(&thing, WIFI_SSID, WIFI_PASSWORD, MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASSWD, TOPICROOT))
    {
        LogError(("Failed to initialize MQTTThing\n"));
        return;
    }
    mqttthing_connectLoop(&thing, connectCallback);

    vTaskDelay(pdTICKS_TO_MS(20000U));
   // char topic_buffer[30] = {0};

    TickType_t lastTimestamp = pdTICKS_TO_MS(xTaskGetTickCount());

    while (true)
    {
        runTimeStats();
        if (!subscribeDone)
        {
            char subscribeTopic[20] = {0};
            sprintf(subscribeTopic,"%s/#", TOPICTEST);
            printf("Subscribe to %s\n", subscribeTopic);
            mqttthing_subscribe(&thing, subscribeTopic, incomingPublish);
            subscribeDone = true;
        }
        else
        {
            TickType_t currentTime = pdTICKS_TO_MS(xTaskGetTickCount());

            if ((currentTime - lastTimestamp) > TEST_PUBLISH_FREQUENCY)
            {
                //sprintf(topic_buffer, "%s", "SENSOR/TEMPERATURE");
                printf("Publishing to %s, payload %s\n", TOPICTEST, "29");
                mqttthing_publish(&thing, TOPICTEST, "29");
                lastTimestamp = currentTime;
            }
        }
        vTaskDelay(pdTICKS_TO_MS(2000U));
    }
}

void vLaunch(void)
{
    TaskHandle_t task;

    xTaskCreate(main_task, "MainThread", 2048, NULL, TASK_PRIORITY, &task);

    /* Start the tasks and timer running. */
    vTaskStartScheduler();
}

int main(void)
{
    timer_hw->dbgpause = 0; // hack!
    stdio_init_all();

    sleep_ms(1000);

    /* Configure the hardware ready to run the demo. */

    const char *rtos_name;
    rtos_name = "FreeRTOS";
    printf("Starting %s on core 0:\n", rtos_name);
    sleep_ms(1000);

    vLaunch();

    return 0;
}