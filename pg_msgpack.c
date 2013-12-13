#include "postgres.h"
#include "libpq/pqformat.h"
#include "utils/bytea.h"

#include "pg_msgpack.h"
#include "convert_from_msgpack.h"
#include "convert_to_msgpack.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(msgpack_in);
PG_FUNCTION_INFO_V1(msgpack_out);
PG_FUNCTION_INFO_V1(msgpack_recv);
PG_FUNCTION_INFO_V1(msgpack_send);

Datum
msgpack_in(PG_FUNCTION_ARGS)
{
	char	   		*json = PG_GETARG_CSTRING(0);
	bytea			*result;
	msgpack_sbuffer	*buf;

	if (json[0] == '\0')
		PG_RETURN_NULL();

	if (json[0] == '\\') {
		PG_RETURN_DATUM(DirectFunctionCall1(byteain, CStringGetDatum(json)));
	} else {
		buf = msgpack_sbuffer_new();
		json_string_to_msgpack(json, buf);

		result = (bytea *) palloc(buf->size + VARHDRSZ);
		SET_VARSIZE(result, buf->size + VARHDRSZ);
		memcpy(VARDATA(result), buf->data, buf->size);

		msgpack_sbuffer_free(buf);

		/* Internal representation is the same as bytea */
		PG_RETURN_BYTEA_P(result);
	}
}

Datum
msgpack_out(PG_FUNCTION_ARGS)
{
	bytea 				*data = PG_GETARG_BYTEA_P(0);
	char  				*out;
	msgpack_unpacked	msg;
	bool 				success;

	msgpack_unpacked_init(&msg);
	success = msgpack_unpack_next(&msg, VARDATA(data), VARSIZE(data) - VARHDRSZ, NULL);

	if (!success)
		PG_RETURN_NULL();

	out = msgpack_to_json_string(msg.data);

	msgpack_unpacked_destroy(&msg);

	PG_RETURN_CSTRING(out);
}

Datum
msgpack_recv(PG_FUNCTION_ARGS)
{
	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);
	bytea		*result;
	int			nbytes;

	nbytes = buf->len - buf->cursor;
	result = (bytea *) palloc(nbytes + VARHDRSZ);
	SET_VARSIZE(result, nbytes + VARHDRSZ);

	pq_copymsgbytes(buf, VARDATA(result), nbytes);

	PG_RETURN_BYTEA_P(result);
}

Datum
msgpack_send(PG_FUNCTION_ARGS)
{
	bytea	*data = PG_GETARG_BYTEA_P_COPY(0);

	PG_RETURN_BYTEA_P(data);
}
