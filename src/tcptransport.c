#include "core_mqtt_config.h"
#include "tcptransport.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#define DEBUG_LINE 25

/***
 * Print the buffer in hex and plain text for debugging
 */
static void tcptrans_debugPrintBuffer(const char *title, const void *pBuffer, size_t bytes) {
    size_t count = 0;
    size_t lineEnd = 0;
    const uint8_t *pBuf = (uint8_t *)pBuffer;

    printf("DEBUG: %s of size %d\n", title, bytes);

    while (count < bytes) {
        lineEnd = count + DEBUG_LINE;
        if (lineEnd > bytes) {
            lineEnd = bytes;
        }

        // Print HEX DUMP
        for (size_t i = count; i < lineEnd; i++) {
            if (pBuf[i] <= 0x0F) {
                printf("0%X ", pBuf[i]);
            } else {
                printf("%X ", pBuf[i]);
            }
        }

        // Pad for short lines
        size_t pad = (DEBUG_LINE - (lineEnd - count)) * 3;
        for (size_t i = 0; i < pad; i++) {
            printf(" ");
        }

        // Print Plain Text
        for (size_t i = count; i < lineEnd; i++) {
            if ((pBuf[i] >= 0x20) && (pBuf[i] <= 0x7e)) {
                printf("%c", pBuf[i]);
            } else {
                printf(".");
            }
        }

        printf("\n");

        count = lineEnd;
    }
}

/***
 * Called when DNS is returned
 * @param name - server name
 * @param ipaddr - ip address of server
 * @param callback_arg - this TCPtransport object
 */
static void tcptrans_dnsFound(TCPTransport* self, const char *name, const ip_addr_t *ipaddr, void *callback_arg) 
{
    memcpy(&(self->xHost), ipaddr, sizeof(self->xHost));

    LogInfo(("DNS Found %s copied to xHost %s\n", ipaddr_ntoa(ipaddr), ipaddr_ntoa(&(self->xHost))));
    xSemaphoreGiveFromISR(self->xHostDNSFound, NULL);
}

/***
 * Call back function for the DNS lookup
 * @param name - server name
 * @param ipaddr - resulting IP address
 * @param callback_arg - poiter to TCPTransport object
 */
static void tcptrans_dnsCB(const char *name, const ip_addr_t *ipaddr, void *callback_arg) 
{
    TCPTransport *self = (TCPTransport *)callback_arg;
    tcptrans_dnsFound(self, name, ipaddr, callback_arg);
}

/***
 * Connect to socket previously stored ip address and port number
 * @return true if socket openned
 */
static bool tcptrans_doConnect(TCPTransport* self) 
{
    struct sockaddr_in serv_addr;

    self->xSock = socket(AF_INET, SOCK_STREAM, 0);
    if (self->xSock < 0) {
        LogError(("ERROR opening socket\n"));
        return false;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(self->xPort);
    memcpy(&serv_addr.sin_addr.s_addr, &(self->xHost), sizeof(self->xHost));

    int res = connect(self->xSock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (res < 0) {
        char *s = ipaddr_ntoa(&(self->xHost));
        LogError(("ERROR connecting %d to %s port %d\n", res, s, self->xPort));
        return false;
    }

    int nonblock = 1;
    ioctlsocket(self->xSock, FIONBIO, &nonblock);

    LogInfo(("Connect success\n"));
    return true;
}





/***
 * Constructor
 */
void tcptrans_init(TCPTransport* self) 
{
    self->xHostDNSFound = xSemaphoreCreateBinary();
    self->xSock = 0;
    self->xPort = 80;

}

/***
 * Required by CoreMQTT returns time in ms
 * @return
 */
uint32_t tcptrans_getCurrentTime() 
{
    return to_ms_since_boot(get_absolute_time());
}

/***
 * Send bytes through socket
 * @param pNetworkContext - Network context object from MQTT
 * @param pBuffer - Buffer to send from
 * @param bytesToSend - number of bytes to send
 * @return number of bytes sent
 */
static int32_t tcptrans_send(TCPTransport* self, const void *pBuffer, size_t bytesToSend) {
    uint32_t dataOut;

    // debugPrintBuffer("TCPTransport::transSend", pBuffer, bytesToSend);

    dataOut = write(self->xSock, (uint8_t *)pBuffer, bytesToSend);
    if (dataOut != bytesToSend) {
        LogError(("Send failed %d\n", dataOut));
    }
    return dataOut;
}

/***
 * Send
 * @param pNetworkContext
 * @param pBuffer
 * @param bytesToRecv
 * @return
 */
static int32_t tcptrans_read(TCPTransport* self, void *pBuffer, size_t bytesToRecv) {
    int32_t dataIn = 0;

    dataIn = read(self->xSock, (uint8_t *)pBuffer, bytesToRecv);

    if (dataIn < 0) {
        if (errno == 0) {
            dataIn = 0;
        }
    }

    // printf("Read(%d)=%d\n", bytesToRecv, dataIn);

    // printf("transRead(%d)=%d\n", bytesToRecv, dataIn);
    return dataIn;
}

/***
 * Static function to send data through socket from buffer
 * @param pNetworkContext - Used to locate the TCPTransport object to use
 * @param pBuffer - Buffer of data to send
 * @param bytesToSend - number of bytes to send
 * @return number of bytes sent
 */
int32_t tcptrans_staticSend(NetworkContext_t *pNetworkContext, const void *pBuffer, size_t bytesToSend) {
    TCPTransport *t = (TCPTransport *)pNetworkContext->tcpTransport;
    return tcptrans_send(t, pBuffer, bytesToSend);
}

/***
 * Read data from network socket. Non blocking returns 0 if no data
 * @param pNetworkContext - Used to locate the TCPTransport object to use
 * @param pBuffer - Buffer to read into
 * @param bytesToRecv - Maximum number of bytes to read
 * @return number of bytes read. May be 0 as non blocking
 * Negative number indicates error
 */
int32_t tcptrans_staticRead(NetworkContext_t *pNetworkContext, void *pBuffer, size_t bytesToRecv) {
    TCPTransport *t = (TCPTransport *)pNetworkContext->tcpTransport;
    return tcptrans_read(t, pBuffer, bytesToRecv);
}

/***
 * Connect to remote TCP Socket
 * @param host - Host address
 * @param port - Port number
 * @return true on success
 */
bool tcptrans_connect(TCPTransport* self, const char *host, uint16_t port) {
    err_t res = dns_gethostbyname(host, &(self->xHost), tcptrans_dnsCB, self);

    strcpy(self->xHostName, host);
    self->xPort = port;

    if (xSemaphoreTake(self->xHostDNSFound, TCP_TRANSPORT_WAIT) != pdTRUE) {
        LogError(("DNS Timeout on Connect: %s, %d", host, res));
        // return false;
    }

    return tcptrans_doConnect(self);
}

/***
 * Get status of the socket
 * @return int <0 is error
 */
int tcptrans_status(TCPTransport* self) 
{
    int error = 0;
    socklen_t len = sizeof(error);
    int retval = getsockopt(self->xSock, SOL_SOCKET, SO_ERROR, &error, &len);
    return error;
}

/***
 * Close the socket
 * @return true on success
 */
bool tcptrans_close(TCPTransport* self) 
{
    closesocket(self->xSock);
    return true;
}

