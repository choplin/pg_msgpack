#ifndef __CONVERT_TO_MSGPACK__
#define ___CONVERT_TO_MSGPACK__

/* TODO */
/* stop to load stdbool.h. force to use postgresql's bool */
#define _STDBOOL_H 
#define __STDBOOL_H 
#include <msgpack.h>

void json_string_to_msgpack(const char *json_str, msgpack_sbuffer *buf);

#endif /* ___CONVERT_TO_MSGPACK__ */
