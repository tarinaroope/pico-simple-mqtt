#pragma once

#include <stdlib.h>


#ifndef MQTT_TOPIC_STATUS
#define MQTT_TOPIC_STATUS "STATUS"
#endif
#ifndef MQTT_TOPIC_STATUS_OFF
#define MQTT_TOPIC_STATUS_OFF "OFF"
#endif
#ifndef MQTT_TOPIC_STATUS_ON
#define MQTT_TOPIC_STATUS_ON "ON"
#endif
#ifndef MQTT_TOPIC_STATUS_KEEP
#define MQTT_TOPIC_STATUS_KEEP "KEEP"
#endif

    /***
     * Get length of the lifecycle topic, to allow dynamic creation of string
     * @param id = think ID
     * @return
     */
     size_t lenStatusTopic(const char *id);

    /***
     * generate the lifecycle topic for thing
     * @param topic - out to write the topic
     * @param id - id of the thing
     * @param name  = name of the lifecycle topic (ON, OFF, KEEP)
     */
     void genStatusTopic(char *topic, const char *id);


