#ifndef __CONVERT_FROM_MSGPACK__
#define __CONVERT_FROM_MSGPACK__

#include <msgpack.h>

#include "postgres.h"

/* Convert msgpack_object to string */
char * msgpack_to_json_string(msgpack_object o);

/* Convert msgpack_object to bytea */
bytea * msgpack_to_bytea(msgpack_object o);

#endif /* __CONVERT_FROM_MSGPACK__ */
