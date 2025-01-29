#include "topichelper.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/***
 * Get length of the lifecycle topic, to allow dynamic creation of string
 * @param id
 * @return
 */
size_t lenStatusTopic(const char *id) {
    size_t res = 0;
    res = strlen(MQTT_TOPIC_STATUS) +
          strlen(id) +
          4;
    return res;
}

/***
 * generate the lifecycle topic for thing
 * @param topic - out to write the topic
 * @param id - id of the thing
 */
void genStatusTopic(char *topic, const char *id) {
    sprintf(topic, "%s/%s/%s", id, MQTT_TOPIC_STATUS);
}

