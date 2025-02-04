#pragma once

#include "FreeRTOS.h"
#include "tcptransport.h"
#include "core_mqtt_agent.h"
#include "core_mqtt_config.h"
#include "freertos_agent_message.h"
#include "freertos_command_pool.h"

#ifndef MQTT_AGENT_NETWORK_BUFFER_SIZE
#define MQTT_AGENT_NETWORK_BUFFER_SIZE 512
#endif

#ifndef MAXSUBS
#define MAXSUBS 12
#endif

#ifndef MQTTKEEPALIVETIME
#define MQTTKEEPALIVETIME 60
#endif

#ifndef MQTT_RECON_DELAY
#define MQTT_RECON_DELAY 10
#endif


typedef struct
{
    void (*MQTTOffline)(void* context);
    void (*MQTTOnline)(void* context);
    void (*MQTTIncomingPublish)(void* context, const char* topic, size_t topic_length, const char* payload, size_t payload_length);
    void* context;
} MQTTAgentObserver;

// Enumerator used to control the state machine at centre of agent
typedef enum  
{ 
    Offline,
    TCPReq,
    TCPConned,
    MQTTReq,
    MQTTConned,
    MQTTRecon,
    Online 
} MQTTState;

typedef struct 
{
    NetworkContext_t xNetworkContext;
    TCPTransport xTcpTrans;

    // MQTT Server and credentials
    const char *pUser;
    const char *pPasswd;
    // Device name used in mqtt topics 
    const char *pId;
    const char *pTarget;
    char xMacStr[14];
    uint16_t xPort;
    bool xRecon;

    char* pWillTopic;
    char* pWillPayload;

    // Buffers and queues
    uint8_t xNetworkBuffer[MQTT_AGENT_NETWORK_BUFFER_SIZE];
    uint8_t xStaticQueueStorageArea[MQTT_AGENT_COMMAND_QUEUE_LENGTH * sizeof(MQTTAgentCommand_t *)];
    StaticQueue_t xStaticQueueStructure;
    MQTTAgentMessageContext_t xCommandQueue;
    MQTTAgentContext_t xGlobalMqttAgentContext;
    TaskHandle_t xAgentTaskHandle;
    
    // State machine state
    MQTTState xConnState;

    // Single Observer
    MQTTAgentObserver* pObserver;
} MQTTAgent;

    /***
     * Set Data
     * @param user - string pointer. Not copied so pointer must remain valid
     * @param passwd - string pointer. Not copied so pointer must remain valid
     * @param id - string pointer. Not copied so pointer must remain valid.
     */
    void mqttagent_setData(MQTTAgent* self, const char *user, const char *passwd, const char *id, const char *willTopic, const char* willPayload );

    /***
     * Connect to mqtt server - Actual connection is done in the state machine so task must be running
     * @param target - hostname or ip address, Not copied so pointer must remain valid
     * @param port - port number
     * @param ssl - unused
     * @return
     */
    bool mqttagent_connect(MQTTAgent* self, const char *target, uint16_t port, bool recon);

    /***
     * Start the task running
     * @param priority - priority to run within FreeRTOS
     */
    void mqttagent_start(MQTTAgent* self, UBaseType_t priority);

    /***
     * Stop task
     * @return
     */
    void mqttagent_stop(MQTTAgent* self);

    /***
     * Returns the id of the MQTT client
     * @return
     */
    const char *mqttagent_getId(MQTTAgent* self);

    /***
     * Close connection
     */
    void mqttagent_close(MQTTAgent* self);

    /***
     * Set a single observer to get call back on state changes
     * @param obs
     */
    void mqttagent_setObserver(MQTTAgent* self, MQTTAgentObserver *obs);
    
    /***
     * Get the FreeRTOS task being used
     * @return
     */
    TaskHandle_t mqttagent_getTask(MQTTAgent* self);

    MQTTState mqttagent_getConnectionState(MQTTAgent* self);

    bool mqttagent_mqttPublish(MQTTAgent* self, const char *topic, const char *payload);

    bool mqttagent_mqttSubscribe(MQTTAgent* self, const char *topic);

    bool mqttagent_mqttPing(MQTTAgent* self);
