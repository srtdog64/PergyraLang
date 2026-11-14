#include "self_host_public_diagnostic_wire_owner.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char kSelfHostPublicDiagnosticWirePrefix[] =
    "pgy.selfhost.public-diagnostic.v1";

static bool
driver_self_host_public_diagnostic_wire_admit(
    const unsigned char *payload,
    size_t payload_length,
    const unsigned char **json_payload,
    size_t *json_length)
{
    size_t prefix_length = sizeof(kSelfHostPublicDiagnosticWirePrefix) - 1;
    const unsigned char *json;
    size_t length;

    if (json_payload == NULL || json_length == NULL)
        return false;
    *json_payload = NULL;
    *json_length = 0;
    if (payload == NULL || payload_length <= prefix_length + 1
        || memcmp(payload, kSelfHostPublicDiagnosticWirePrefix,
                  prefix_length) != 0)
        return false;
    if (payload[prefix_length] == '\r'
        && payload[prefix_length + 1] == '\n') {
        json = payload + prefix_length + 2;
        length = payload_length - prefix_length - 2;
    } else if (payload[prefix_length] == '\n') {
        json = payload + prefix_length + 1;
        length = payload_length - prefix_length - 1;
    } else {
        return false;
    }
    if (memchr(json, '\0', length) != NULL || json[0] != '[')
        return false;
    while (length > 0 &&
           (json[length - 1] == '\n' || json[length - 1] == '\r'))
        length--;
    if (length < 4 || json[1] != '{' || json[length - 2] != '}'
        || json[length - 1] != ']')
        return false;
    *json_payload = json;
    *json_length = length;
    return true;
}

int
driver_self_host_public_diagnostic_wire_relay(
    const unsigned char *payload,
    size_t payload_length)
{
    const unsigned char *json_payload = NULL;
    size_t json_length = 0;

    if (!driver_self_host_public_diagnostic_wire_admit(
            payload, payload_length, &json_payload, &json_length))
        return 0;
    if (fwrite(json_payload, 1, json_length, stderr) != json_length
        || fputc('\n', stderr) == EOF || fflush(stderr) != 0)
        return -1;
    return 1;
}
