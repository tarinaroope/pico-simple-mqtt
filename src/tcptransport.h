#pragma once

#define TCP_TRANSPORT_WAIT 10000

//#include "MQTTConfig.h"
#include "core_mqtt.h"
// #include "core_mqtt_agent.h"

//extern "C" {
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include "lwip/dns.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"

//}

typedef struct
{
    // Socket number
    int xSock;

    // Port to connect to
    uint16_t xPort;

    // Remote server IP to connect to
    ip_addr_t xHost;

    // Remote server name to connect to
    char xHostName[80];

    // Semaphore used to wait on DNS responses
    SemaphoreHandle_t xHostDNSFound; 
} TCPTransport;

    void tcptrans_init(TCPTransport* self);

    /***
     * Connect to remote TCP Socket
     * @param host - Host address
     * @param port - Port number
     * @return true on success
     */
    bool tcptrans_connect(TCPTransport* self, const char *host, uint16_t port);

    /***
     * Get status of the socket
     * @return int <0 is error
     */
    int tcptrans_status(TCPTransport* self);

    /***
     * Close the socket
     * @return true on success
     */
    bool tcptrans_close(TCPTransport* self);

    /***
     * returns current time, as time in ms since boot
     * Required for MQTT Agent library
     * @return
     */
    uint32_t tcptrans_getCurrentTime();

    /***
     * Static function to send data through socket from buffer
     * @param pNetworkContext - Used to locate the TCPTransport object to use
     * @param pBuffer - Buffer of data to send
     * @param bytesToSend - number of bytes to send
     * @return number of bytes sent
     */
    int32_t tcptrans_staticSend(NetworkContext_t *pNetworkContext, const void *pBuffer, size_t bytesToSend);

    /***
     * Read data from network socket. Non blocking returns 0 if no data
     * @param pNetworkContext - Used to locate the TCPTransport object to use
     * @param pBuffer - Buffer to read into
     * @param bytesToRecv - Maximum number of bytes to read
     * @return number of bytes read. May be 0 as non blocking
     * Negative number indicates error
     */
    int32_t tcptrans_staticRead(NetworkContext_t *pNetworkContext, void *pBuffer, size_t bytesToRecv);