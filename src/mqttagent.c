#include "mqttagent.h"

#include <stdlib.h>

#include "wifihelper.h"
//#include "topichelper.h"
#include "mqtttask.h"

/* MQTT Agent ports. */
#include "freertos_agent_message.h"
#include "freertos_command_pool.h"

/***
 * Callback on when new data is received
 * @param pMqttAgentContext
 * @param packetId
 * @param pxPublishInfo
 */
static void mqttagent_incomingPublishCallback(MQTTAgentContext_t *pMqttAgentContext,
                                        uint16_t packetId,
                                        MQTTPublishInfo_t *pxPublishInfo) 
{
    LogDebug(("MSG(%d,%d) %.*s:%.*s\n",
              pxPublishInfo->topicNameLength,
              pxPublishInfo->payloadLength,
              pxPublishInfo->topicNameLength,
              pxPublishInfo->pTopicName,
              pxPublishInfo->payloadLength,
              (char *)pxPublishInfo->pPayload));

    MQTTAgent* self = (MQTTAgent*) pMqttAgentContext->pIncomingCallbackContext;    
    if (self)
    {    
        self->pObserver->MQTTIncomingPublish(self->pObserver->context, 
                                            pxPublishInfo->pTopicName,
                                            pxPublishInfo->topicNameLength,
                                            pxPublishInfo->pPayload,
                                            pxPublishInfo->payloadLength);
    }
}

static void mqttagent_commandCallback(MQTTAgentCommandContext_t *pCmdCallbackContext,
                                         MQTTAgentReturnInfo_t *pReturnInfo)
{
    TaskHandle_t* taskHandle = (TaskHandle_t*) pCmdCallbackContext;
    xTaskNotify(*taskHandle, pReturnInfo->returnCode, eSetValueWithOverwrite);
}

/***
 * Set the connection state variable
 * @param s
 */
static void mqttagent_setConnState(MQTTAgent* self, MQTTState s) 
{
    self->xConnState = s;

    if (self->pObserver != NULL) {
        switch (self->xConnState) {
            case Offline: {
                if (self->pObserver->MQTTOffline)
                {
                    self->pObserver->MQTTOffline(self->pObserver->context);
                    if (self->xRecon) {
                        mqttagent_setConnState(self, MQTTRecon);
                    }
                }
                break;
            }
            case Online: {
                if (self->pObserver->MQTTOnline)
                {
                    self->pObserver->MQTTOnline(self->pObserver->context);
                }
                break;
            }
            default: {
                ;
            }
        }
    }
}

/***
 * Initialisation code
 * @return
 */
static MQTTStatus_t mqttagent_init(MQTTAgent* self) 
{
    tcptrans_init(&(self->xTcpTrans));
    MQTTFixedBuffer_t fixedBuffer = {.pBuffer = self->xNetworkBuffer, .size = MQTT_AGENT_NETWORK_BUFFER_SIZE};

    MQTTAgentMessageInterface_t messageInterface =
    {
            .pMsgCtx = NULL,
            .send = Agent_MessageSend,
            .recv = Agent_MessageReceive,
            .getCommand = Agent_GetCommand,
            .releaseCommand = Agent_ReleaseCommand
    };

    LogDebug(("Creating command queue."));
    self->xCommandQueue.queue = xQueueCreateStatic(MQTT_AGENT_COMMAND_QUEUE_LENGTH,
                                             sizeof(MQTTAgentCommand_t *),
                                             self->xStaticQueueStorageArea,
                                             &(self->xStaticQueueStructure));

    if (self->xCommandQueue.queue == NULL) {
        LogDebug(("MQTTAgent::mqttInit ERROR Queue not initialised"));
        return MQTTIllegalState;
    }
    messageInterface.pMsgCtx = &(self->xCommandQueue);

    /* Initialize the task pool. */
    Agent_InitializePool();

    // Fill in Transport interface
    self->xNetworkContext.mqttTask = NULL;
    self->xNetworkContext.tcpTransport = &(self->xTcpTrans);
    TransportInterface_t transport;
    transport.pNetworkContext = &(self->xNetworkContext);
    transport.send = tcptrans_staticSend;
    transport.recv = tcptrans_staticRead;
    transport.writev = NULL;

    /* Initialize MQTT library. */
    return MQTTAgent_Init(&(self->xGlobalMqttAgentContext),
                             &messageInterface,
                             &fixedBuffer,
                             &transport,
                             tcptrans_getCurrentTime,
                             mqttagent_incomingPublishCallback,
                             /* Context to pass into the callback. Passing the pointer to subscription array. */
                             self);
}

/***
 * Connect to MQTT server
 * @return
 */
static MQTTStatus_t mqttagent_doConnect(MQTTAgent* self) 
{
    MQTTConnectInfo_t connectInfo;
    MQTTPublishInfo_t publishInfo;

    /* Many fields not used in this demo so start with everything at 0. */
    (void)memset((void *)&connectInfo, 0x00, sizeof(connectInfo));
    (void)memset((void *)&publishInfo, 0x00, sizeof(publishInfo));

    /* Start with a clean session i.e. direct the MQTT broker to discard any
     * previous session data. Also, establishing a connection with clean
     * session will ensure that the broker does not store any data when this
     * client gets disconnected. */
    connectInfo.cleanSession = true;

    /* The client identifier is used to uniquely identify this MQTT client to
     * the MQTT broker. In a production device the identifier can be something
     * unique, such as a device serial number. */
    connectInfo.pClientIdentifier = self->pId;
    connectInfo.clientIdentifierLength = (uint16_t)strlen(self->pId);
    connectInfo.pUserName = self->pUser;
    connectInfo.userNameLength = (uint16_t)strlen(self->pUser);
    connectInfo.pPassword = self->pPasswd;
    connectInfo.passwordLength = (uint16_t)strlen(self->pPasswd);

    publishInfo.qos = MQTTQoS1;
    if (self->pWillTopic)
    {
        publishInfo.pTopicName = self->pWillTopic;
        publishInfo.topicNameLength = strlen(self->pWillTopic);
        publishInfo.retain = true;
    }

    if (self->pWillPayload)
    {
        publishInfo.pPayload = self->pWillPayload;
        publishInfo.payloadLength = strlen(self->pWillPayload);
    }
    
    /* Set MQTT keep-alive period. It is the responsibility of the application
     * to ensure that the interval between Control Packets being sent does not
     * exceed the Keep Alive value.  In the absence of sending any other
     * Control Packets, the Client MUST send a PINGREQ Packet. */
    connectInfo.keepAliveSeconds = MQTTKEEPALIVETIME;

    bool sessionPresent = false;
    // Send MQTT CONNECT packet to broker. 
    MQTTStatus_t status =  MQTT_Connect(&(self->xGlobalMqttAgentContext.mqttContext),
                           &connectInfo,
                           &publishInfo,
                           30000U,
                           &sessionPresent);

    if (status != MQTTSuccess) 
    {
        LogError(("MQTTConnect error %d", status));
    }
    return status;
}

/***
 * Connect to MQTT hub
 * @return
 */
static bool mqttagent_TCPconn(MQTTAgent* self) {
    LogDebug(("TCP Connect...."));
    if (tcptrans_connect(&(self->xTcpTrans),self->pTarget, self->xPort)) {
        mqttagent_setConnState(self, TCPConned);
        LogDebug(("TCP Connected"));
        return true;
    } else {
        LogDebug(("TCP Connection failed"));
        mqttagent_setConnState(self, Offline);
    }
    return false;
}

/***
 * Set credentials
 * @param user - string pointer. Not copied so pointer must remain valid
 * @param passwd - string pointer. Not copied so pointer must remain valid
 * @param id - string pointer. Not copied so pointer must remain valid.
 * If not provide ID will be user
 *
 */

void mqttagent_setData(MQTTAgent* self, const char *user, const char *passwd, const char *id, const char *willTopic, const char* willPayload )
{
    self->pUser = user;
    self->pPasswd = passwd;

    if (id != NULL) {
        self->pId = id;
    } else {
        self->pId = self->pUser;
    }
    LogDebug(("MQTT Credentials Id=%s, usr=%s\n", self->pId, self->pUser));

    self->pTarget = NULL;
    self->xPort = 1883;
    self->xRecon = false;
    self->xAgentTaskHandle = NULL;
    self->xConnState = Offline;

    self->pWillTopic = pvPortMalloc(strlen(willTopic) + 1);
    self->pWillPayload = pvPortMalloc(strlen(willPayload) + 1);
    if (self->pWillTopic == NULL || self->pWillPayload == NULL)
    {
        LogError(("Failed to allocate memory for will topic and payload"));
        return;
    }

    strcpy(self->pWillTopic, willTopic);
    strcpy(self->pWillPayload, willPayload);
}

MQTTState mqttagent_getConnectionState(MQTTAgent* self)
{
    return self->xConnState;
}

/***
 * Connect to mqtt server
 * @param target - hostname or ip address, Not copied so pointer must remain valid
 * @param port - port number
 * @param ssl - unused
 * @return
 */
bool mqttagent_connect(MQTTAgent* self, const char *target, uint16_t port, bool recon) {
    self->pTarget = target;
    self->xPort = port;
    self->xRecon = recon;
    mqttagent_setConnState(self, TCPReq);
    LogDebug(("TCP Requested\n"));
    return true;
}

/***
 * Run loop for the task
 */
static void mqttagent_run(MQTTAgent* self) 
{
    LogDebug(("MQTTAgent run\n"));

    MQTTStatus_t status;

    for (;;) 
    {
        switch (self->xConnState) {
            case Offline: {
                break;
            }
            case TCPReq: {
                if (wifi_isJoined()) {
                    mqttagent_TCPconn(self);
                } else {
                    LogInfo(("Network offline, awaiting reconnect"));
                    vTaskDelay(MQTT_RECON_DELAY);
                }
                break;
            }
            case TCPConned: {
                LogDebug(("Attempting MQTT conn\n"));
                status = mqttagent_doConnect(self);
                if (status == MQTTSuccess) {
                    mqttagent_setConnState(self, MQTTReq);
                    LogDebug(("MQTTconn ok\n"));
                } else {
                    mqttagent_setConnState(self, Offline);
                    LogDebug(("MQTTConn failed\n"));
                }
                break;
            }
            case MQTTReq: {
                mqttagent_setConnState(self, MQTTConned);
                break;
            }
            case MQTTConned: {
                mqttagent_setConnState(self, Online);
                break;
            }
            case Online: {
                LogDebug(("Starting CMD loop\n"));

                status = MQTTAgent_CommandLoop(&self->xGlobalMqttAgentContext);

                // The function returns on either receiving a terminate command,
                // undergoing network disconnection OR encountering an error.
                if ((status == MQTTSuccess) && (self->xGlobalMqttAgentContext.mqttContext.connectStatus == MQTTNotConnected)) {
                    // A terminate command was processed and MQTT connection was closed.
                    // Need to close socket connection.
                    // Platform_DisconnectNetwork( mqttAgentContext.mqttContext.transportInterface.pNetworkContext );
                    LogDebug(("MQTT Closed\n"));
                    tcptrans_close(&(self->xTcpTrans));
                    mqttagent_setConnState(self, Offline);
                } else if (status == MQTTSuccess) {
                    // Terminate command was processed but MQTT connection was not
                    // closed. Thus, need to close both MQTT and socket connections.
                    LogDebug(("Terminated processed\n"));
                    status = MQTT_Disconnect(&(self->xGlobalMqttAgentContext.mqttContext));
                    // assert( status == MQTTSuccess );
                    // Platform_DisconnectNetwork( mqttAgentContext.mqttContext.transportInterface.pNetworkContext );
                    tcptrans_close(&(self->xTcpTrans));
                    mqttagent_setConnState(self, Offline);
                } else {
                    // Handle error.
                    LogDebug(("Command Loop error %d\n", status));
                    mqttagent_setConnState(self, Offline);
                }
                break;
            }
            case MQTTRecon: {
                if (wifi_isJoined()) {
                    tcptrans_close(&(self->xTcpTrans));
                }
                vTaskDelay(MQTT_RECON_DELAY);
                mqttagent_setConnState(self, TCPReq);
                break;
            }
            default: {
            }
        };

        taskYIELD();
        // vTaskDelay(1);
    }

    LogError(("RUN STOPPED\n"));
}

/***
 * Internal function used by FreeRTOS to run the task
 * @param pvParameters
 */
static void mqttagent_vTask(void *pvParameters) {
    MQTTAgent *task = (MQTTAgent *)pvParameters;
    mqttagent_run(task);
}

/***
 *  create the vtask, will get picked up by scheduler
 *
 *  */
void mqttagent_start(MQTTAgent* self, UBaseType_t priority) 
{
    if (mqttagent_init(self) == MQTTSuccess) 
    {
        xTaskCreate(
            mqttagent_vTask,
            "MQTTAgent",
            2512,
            (void *)self,
            priority,
            &(self->xAgentTaskHandle));
    }
}

/***
 * Returns the id of the client
 * @return
 */
const char * mqttagent_getId(MQTTAgent* self) {
    return self->pId;
}

/***
 * Close connection
 */
void mqttagent_close(MQTTAgent* self) 
{
    tcptrans_close(&(self->xTcpTrans));
    self->xRecon = false;
    mqttagent_setConnState(self, Offline);
}

/***
 * Set a single observer to get call back on state changes
 * @param obs
 */
void mqttagent_setObserver(MQTTAgent* self, MQTTAgentObserver *obs) {
    self->pObserver = obs;
}

/***
 * Stop task
 * @return
 */
void mqttagent_stop(MQTTAgent* self) 
{
    if (self->xConnState != Offline) {
        tcptrans_close(&(self->xTcpTrans));
        mqttagent_setConnState(self, Offline);
    }
    if (self->xAgentTaskHandle != NULL) {
        vTaskDelete(self->xAgentTaskHandle);
        self->xAgentTaskHandle = NULL;
    }
    if (self->pWillTopic != NULL) 
    {
        vPortFree(self->pWillTopic);
        self->pWillTopic = NULL;
    }
    if (self->pWillPayload != NULL) 
    {
        vPortFree(self->pWillPayload);
        self->pWillPayload = NULL;
    }
}

/***
 * Get the FreeRTOS task being used
 * @return
 */
TaskHandle_t mqttagent_getTask(MQTTAgent* self) 
{
    return self->xAgentTaskHandle;
}

/***
 * Get high water for stack
 * @return close to zero means overflow risk
 */
static unsigned int mqttagent_getStackHighWater(MQTTAgent* self) 
{
    if (self->xAgentTaskHandle != NULL)
        return uxTaskGetStackHighWaterMark(self->xAgentTaskHandle);
    else
        return 0;
}

bool mqttagent_mqttPublish(MQTTAgent* self, const char *topic, const char *payload) 
{   
    /*
    MQTTPublishInfo_t publishInfo = {0};
    // Fill the information for publish operation.
    publishInfo.pTopicName = topic;
    publishInfo.topicNameLength = (uint16_t)strlen(topic);
    publishInfo.pPayload = payload;
    publishInfo.payloadLength = (uint16_t)strlen(payload);
    
    return (mqtttask_publish(&self->xGlobalMqttAgentContext, &publishInfo) == MQTTSuccess);
    */

   // Create space for the paramaters
    
    MQTTAgentCommandInfo_t commandInfo = {0};
    commandInfo.cmdCompleteCallback = mqttagent_commandCallback;
    TaskHandle_t currentTaskHandle = xTaskGetCurrentTaskHandle();
    commandInfo.pCmdCompleteCallbackContext = (MQTTAgentCommandContext_t *) &currentTaskHandle;    
    commandInfo.blockTimeMs = 500;

    MQTTPublishInfo_t publishInfo = {0};
    publishInfo.qos = MQTTQoS1;
    publishInfo.pTopicName = topic;
    publishInfo.topicNameLength = (uint16_t)strlen(topic);
    publishInfo.pPayload = payload;
    publishInfo.payloadLength = (uint16_t)strlen(payload);


    // Publish and wait for the callback to be called. The callback will notify this task to continue.
  //  LogDebug(("Publishing topic %.*s\n", publishInfo.topicNameLength, publishInfo.pTopicName));
   // LogDebug(("Publishing payload %.*s\n", publishInfo.payloadLength, publishInfo.pPayload));
    
    MQTTStatus_t status = MQTTAgent_Publish(&self->xGlobalMqttAgentContext, &publishInfo, &commandInfo);
    if (status != MQTTSuccess) 
    {
        LogError(("Failed to publish message, error %d\n", status));
        return false;
    }
    // Parent task can now proceed. Pass the status.
   // xTaskNotify(params->parentTask, (uint32_t) status, eSetValueWithOverwrite);

    // Wait until callback notifies this task to continue
    uint32_t ulNotificationValue;
    xTaskNotifyWait(0, 0, &ulNotificationValue, portMAX_DELAY);
    //ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    return ( (MQTTStatus_t)ulNotificationValue == MQTTSuccess);
    
  //  vPortFree((void *)topic);
  //  vPortFree((void *)payload);
      // Parent task can now proceed. Pass the status.
   // xTaskNotify(params->parentTask, (uint32_t) status, eSetValueWithOverwrite);

  //  vTaskDelay(pdTICKS_TO_MS(1000U));
  //  printf("delete task\n");
}

bool mqttagent_mqttSubscribe(MQTTAgent* self, const char *topic) 
{
    MQTTAgentCommandInfo_t commandInfo = {0};
    commandInfo.cmdCompleteCallback = mqttagent_commandCallback;
    TaskHandle_t currentTaskHandle = xTaskGetCurrentTaskHandle();
    commandInfo.pCmdCompleteCallbackContext = (MQTTAgentCommandContext_t *) &currentTaskHandle;    
    commandInfo.blockTimeMs = 500;

    MQTTSubscribeInfo_t subscribeInfo = {0};

    // Fill the information for topic filters to subscribe to.
    subscribeInfo.qos = MQTTQoS1;
    subscribeInfo.pTopicFilter = topic;
    subscribeInfo.topicFilterLength = (uint16_t) strlen(topic);

    MQTTAgentSubscribeArgs_t subscribeArgs = {0};
    subscribeArgs.pSubscribeInfo = &subscribeInfo;
    subscribeArgs.numSubscriptions = 1U;
    
    // Publish and wait for the callback to be called. The callback will notify this task to continue.
    LogDebug(("Subscribing to topic %.*s\n", subscribeInfo.topicFilterLength, subscribeInfo.pTopicFilter));
    MQTTStatus_t status = MQTTAgent_Subscribe(&self->xGlobalMqttAgentContext, &subscribeArgs, &commandInfo);
    if (status != MQTTSuccess) 
    {
        LogError(("Failed to subscribe to topic, error %d\n", status));
    }
       
    uint32_t ulNotificationValue;
    xTaskNotifyWait(0, 0, &ulNotificationValue, portMAX_DELAY);
    return ( (MQTTStatus_t)ulNotificationValue == MQTTSuccess);
    
/*
    MQTTSubscribeInfo_t subscribeInfo = {0};
    // Fill the information for topic filters to subscribe to.
    subscribeInfo.qos = MQTTQoS1;
    subscribeInfo.pTopicFilter = topic;
    subscribeInfo.topicFilterLength = strlen(topic);
    
    return (mqtttask_subscribe(&self->xGlobalMqttAgentContext, &subscribeInfo) == MQTTSuccess);
    */
}

bool mqttagent_mqttPing(MQTTAgent* self)
{
    MQTTAgentCommandInfo_t commandInfo = {0};
    commandInfo.cmdCompleteCallback = mqttagent_commandCallback;
    TaskHandle_t currentTaskHandle = xTaskGetCurrentTaskHandle();
    commandInfo.pCmdCompleteCallbackContext = (MQTTAgentCommandContext_t *) &currentTaskHandle;    
    commandInfo.blockTimeMs = 500;

    // Publish and wait for the callback to be called. The callback will notify this task to continue.
    LogDebug(("Sending ping...\n"));

    MQTTStatus_t status = MQTTAgent_Ping(&self->xGlobalMqttAgentContext, &commandInfo);
    if (status != MQTTSuccess) 
    {
        LogError(("Failed to send ping, error %d\n", status));
    }
    uint32_t ulNotificationValue;
    xTaskNotifyWait(0, 0, &ulNotificationValue, portMAX_DELAY);
    return ( (MQTTStatus_t)ulNotificationValue == MQTTSuccess);
    
    //return ((mqttagent_ping(&self->xGlobalMqttAgentContext)) == MQTTSuccess);
}
