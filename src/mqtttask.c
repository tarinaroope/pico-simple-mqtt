#include "FreeRTOS.h"
#include "mqtttask.h"
#include <limits.h>
#include <core_mqtt_config.h>

static void mqtttask_commandCallback(MQTTAgentCommandContext_t *pCmdCallbackContext,
                                         MQTTAgentReturnInfo_t *pReturnInfo)
{
    TaskHandle_t* taskHandle = (TaskHandle_t*) pCmdCallbackContext;
    // No return info checking at this point - just blindly go forward.
    printf("Callback called %d\n", taskHandle);
    xTaskNotifyGive(*taskHandle);
}

static void mqtttask_vTaskPing(void *pvParameters) 
{
    MQTTTaskParameters *params = (MQTTTaskParameters *)pvParameters;

    MQTTAgentCommandInfo_t commandInfo = {0};
    commandInfo.cmdCompleteCallback = mqtttask_commandCallback;
    TaskHandle_t currentTaskHandle = xTaskGetCurrentTaskHandle();
    commandInfo.pCmdCompleteCallbackContext = (MQTTAgentCommandContext_t *) &currentTaskHandle;    
    commandInfo.blockTimeMs = 500;

     MQTTAgentContext_t* agentContext = params->pMqttAgentContext;

    // Publish and wait for the callback to be called. The callback will notify this task to continue.
    LogDebug(("Sending ping...\n"));

    MQTTStatus_t status = MQTTAgent_Ping(agentContext, &commandInfo);
    if (status != MQTTSuccess) 
    {
        LogError(("Failed to send ping, error %d\n", status));
    }

    // Parent task can now proceed. Pass the status.
    xTaskNotify(params->parentTask, (uint32_t) status, eSetValueWithOverwrite);
 
    // Wait until callback notifies this task to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    vTaskDelete(NULL);
}

static void mqtttask_vTaskSubscribe(void *pvParameters) 
{
    MQTTTaskParameters *params = (MQTTTaskParameters *)pvParameters;

    // Create space for the paramaters
    char *topic = pvPortMalloc(params->topic_length);
    memcpy(topic, params->topic, params->topic_length);

    MQTTAgentCommandInfo_t commandInfo = {0};
    commandInfo.cmdCompleteCallback = mqtttask_commandCallback;
    TaskHandle_t currentTaskHandle = xTaskGetCurrentTaskHandle();
    commandInfo.pCmdCompleteCallbackContext = (MQTTAgentCommandContext_t *) &currentTaskHandle;    
    commandInfo.blockTimeMs = 500;

    MQTTSubscribeInfo_t subscribeInfo = {0};

    // Fill the information for topic filters to subscribe to.
    subscribeInfo.qos = MQTTQoS1;
    subscribeInfo.pTopicFilter = topic;
    subscribeInfo.topicFilterLength = params->topic_length;

    MQTTAgentSubscribeArgs_t subscribeArgs = {0};
    subscribeArgs.pSubscribeInfo = &subscribeInfo;
    subscribeArgs.numSubscriptions = 1U;
    
    MQTTAgentContext_t* agentContext = params->pMqttAgentContext;

    // Publish and wait for the callback to be called. The callback will notify this task to continue.
    LogDebug(("Subscribing to topic %.*s\n", subscribeInfo.topicFilterLength, subscribeInfo.pTopicFilter));
    MQTTStatus_t status = MQTTAgent_Subscribe(agentContext, &subscribeArgs, &commandInfo);
    if (status != MQTTSuccess) 
    {
        LogError(("Failed to subscribe to topic, error %d\n", status));
    }
       
    // Parent task can now proceed. Pass the status.
    xTaskNotify(params->parentTask, (uint32_t) status, eSetValueWithOverwrite);

    // Wait until callback notifies this task to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    vPortFree((void *)topic);

    vTaskDelete(NULL);
}

void mqtttask_vTaskPublish(void *pvParameters) 
{
    
    MQTTTaskParameters *params = (MQTTTaskParameters *)pvParameters;

    // Create space for the paramaters
    char *topic = pvPortMalloc(params->topic_length);
    char *payload = pvPortMalloc(params->payload_length);
    memcpy(topic, params->topic, params->topic_length);
    memcpy(payload, params->payload, params->payload_length);

    MQTTAgentCommandInfo_t commandInfo = {0};
    commandInfo.cmdCompleteCallback = mqtttask_commandCallback;
    TaskHandle_t currentTaskHandle = xTaskGetCurrentTaskHandle();
    commandInfo.pCmdCompleteCallbackContext = (MQTTAgentCommandContext_t *) &currentTaskHandle;    
    commandInfo.blockTimeMs = 500;

    MQTTPublishInfo_t publishInfo = {0};
    publishInfo.qos = MQTTQoS1;
    publishInfo.pTopicName = topic;
    publishInfo.topicNameLength = params->topic_length;
    publishInfo.pPayload = payload;
    publishInfo.payloadLength = params->payload_length;

    MQTTAgentContext_t* agentContext = params->pMqttAgentContext;

    // Publish and wait for the callback to be called. The callback will notify this task to continue.
    LogDebug(("Publishing topic %.*s\n", publishInfo.topicNameLength, publishInfo.pTopicName));
    LogDebug(("Publishing payload %.*s\n", publishInfo.payloadLength, publishInfo.pPayload));
    
    MQTTStatus_t status = MQTTAgent_Publish(agentContext, &publishInfo, &commandInfo);
    if (status != MQTTSuccess) 
    {
        LogError(("Failed to publish message, error %d\n", status));
    }
    // Parent task can now proceed. Pass the status.
    xTaskNotify(params->parentTask, (uint32_t) status, eSetValueWithOverwrite);

    // Wait until callback notifies this task to continue
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
   
    vPortFree((void *)topic);
    vPortFree((void *)payload);

    vTaskDelete(NULL);
}

/***
 *  create the vtask, will get picked up by scheduler
 *
 *  */
MQTTStatus_t mqtttask_publish(MQTTAgentContext_t* mqttAgentContext, MQTTPublishInfo_t* publishInfo) 
{ 
    MQTTTaskParameters params = {0};
    params.topic = publishInfo->pTopicName;
    params.topic_length = publishInfo->topicNameLength;
    params.payload = publishInfo->pPayload;
    params.payload_length = publishInfo->payloadLength; 
    params.parentTask = xTaskGetCurrentTaskHandle();
    params.pMqttAgentContext = mqttAgentContext;
    
    xTaskCreate(
        mqtttask_vTaskPublish,
        "MQTTTaskPublish",
        1024,
        (void *) &params,
        (tskIDLE_PRIORITY + 1UL),
        NULL);
    // Todo return error handling
    
    // Wait until publish task has copied the parameters and this task gets notified
    uint32_t ulNotificationValue;
    xTaskNotifyWait(0x00, ULONG_MAX, &ulNotificationValue, portMAX_DELAY);

    return (MQTTStatus_t) ulNotificationValue;
}

MQTTStatus_t mqtttask_subscribe(MQTTAgentContext_t* mqttAgentContext, MQTTSubscribeInfo_t* subscribeInfo) 
{ 
    MQTTTaskParameters params = {0};
    params.topic = subscribeInfo->pTopicFilter;
    params.topic_length = subscribeInfo->topicFilterLength;

    params.parentTask = xTaskGetCurrentTaskHandle();
    params.pMqttAgentContext = mqttAgentContext;
    
    xTaskCreate(
        mqtttask_vTaskSubscribe,
        "MQTTTaskSubscribe",
        1024,
        (void *) &params,
        (tskIDLE_PRIORITY + 1UL),
        NULL);
    
    // Wait until publish task has copied the parameters and this task gets notified
    uint32_t ulNotificationValue;
    xTaskNotifyWait(0x00, ULONG_MAX, &ulNotificationValue, portMAX_DELAY);
    return (MQTTStatus_t) ulNotificationValue;
}

MQTTStatus_t mqtttask_ping(MQTTAgentContext_t* mqttAgentContext) 
{ 
    MQTTTaskParameters params = {0};
    
    params.parentTask = xTaskGetCurrentTaskHandle();
    params.pMqttAgentContext = mqttAgentContext;
    
    xTaskCreate(
        mqtttask_vTaskPing,
        "MQTTTaskPing",
        512,
        (void *) &params,
        (tskIDLE_PRIORITY + 1UL),
        NULL);
    
    // Wait until ping task has copied the parameters and this task gets notified
    uint32_t ulNotificationValue;
    xTaskNotifyWait(0x00, ULONG_MAX, &ulNotificationValue, portMAX_DELAY);
    return (MQTTStatus_t) ulNotificationValue;
}
