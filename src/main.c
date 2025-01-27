
#include <stdio.h>

#include "FreeRTOS.h"

#include "wifihelper.h"
#include "lwip/dns.h"
#include "lwip/ip4_addr.h"
#include "pico/cyw43_arch.h"

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

// LED PAD defintions
#define PULSE_LED 0

#define TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)

bool isOnline = false;

 void MQTTOfflineCallback()
 {
    printf("Offline callback called\n");
    isOnline = false;
 }
void MQTTOnlineCallback()
{
    printf("Online callback called\n");
    isOnline = true;
}
  //  void (*MQTTSend)();
  //  void (*MQTTRecv)();

void runTimeStats() {
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

    if (pxTaskStatusArray != NULL) {
        /* Generate raw status information about each task. */
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray,
                                           uxArraySize,
                                           &ulTotalRunTime);

        /* For each populated position in the pxTaskStatusArray array,
        format the raw data as human readable ASCII data. */
        for (x = 0; x < uxArraySize; x++) {
            printf("Task: %d \t cPri:%d \t bPri:%d \t hw:%d \t%s\n",
                   pxTaskStatusArray[x].xTaskNumber,
                   pxTaskStatusArray[x].uxCurrentPriority,
                   pxTaskStatusArray[x].uxBasePriority,
                   pxTaskStatusArray[x].usStackHighWaterMark,
                   pxTaskStatusArray[x].pcTaskName);
        }

        /* The array is no longer needed, free the memory it consumes. */
        vPortFree(pxTaskStatusArray);
    } else {
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

void main_task(void *params) {
    printf("Main task started\n");

    if (wifi_init()) {
        printf("Wifi Controller Initialised\n");
    } else {
        printf("Failed to initialise controller\n");
        return;
    }

    printf("Connecting to WiFi... %s \n", WIFI_SSID);

    if (wifi_join(WIFI_SSID, WIFI_PASSWORD)) {
        printf("Connect to Wifi\n");
    } else {
        printf("Failed to connect to Wifi \n");
    }

    // Print MAC Address
    char macStr[20];
    wifi_getMACAddressStr(macStr);
    printf("MAC ADDRESS: %s\n", macStr);

    // Print IP Address
    char ipStr[20];
    wifi_getIPAddressStr(ipStr);
    printf("IP ADDRESS: %s\n", ipStr);

    // Setup for MQTT Connection
    char mqttTarget[] = MQTT_HOST;
    int mqttPort = MQTT_PORT;
    char mqttClient[] = MQTT_CLIENT;
    char mqttUser[] = MQTT_USER;
    char mqttPwd[] = MQTT_PASSWD;

    MQTTAgent mqttAgent;
    mqttagent_init_types(&mqttAgent);
    MQTTAgentObserver mqttObs;
    mqttObs.MQTTOffline = MQTTOfflineCallback;
    mqttObs.MQTTOnline = MQTTOnlineCallback;

    mqttagent_setObserver(&mqttAgent, &mqttObs);
    mqttagent_credentials(&mqttAgent, mqttUser, mqttPwd, mqttClient);

    printf("Connecting to: %s(%d)\n", mqttTarget, mqttPort);
    printf("Client id: %.4s...\n", mqttagent_getId(&mqttAgent));
    printf("User id: %.4s...\n", mqttUser);

    mqttagent_connect(&mqttAgent, mqttTarget, mqttPort, true);
    mqttagent_start(&mqttAgent, TASK_PRIORITY);

    while (true) {
        // runTimeStats();

        vTaskDelay(3000);
        if (isOnline == true)
        {
            //mqttAgent.mqttPublish(NULL,NULL);
            isOnline = false;
        }
        if (!wifi_isJoined()) {
            printf("AP Link is down\n");

            if (wifi_join(WIFI_SSID, WIFI_PASSWORD)) {
                printf("Connect to Wifi\n");
            } else {
                printf("Failed to connect to Wifi \n");
            }
        }
    }
}

void vLaunch(void) {
    TaskHandle_t task;

    xTaskCreate(main_task, "MainThread", 2048, NULL, TASK_PRIORITY, &task);

    /* Start the tasks and timer running. */
    vTaskStartScheduler();
}

int main(void) 
{
    timer_hw->dbgpause = 0; 
    stdio_init_all();
    sleep_ms(2000);
    printf("GO\n");

    /* Configure the hardware ready to run the demo. */
    const char *rtos_name;
    rtos_name = "FreeRTOS";
    printf("Starting %s on core 0:\n", rtos_name);
        sleep_ms(1000);

    vLaunch();

    return 0;
}