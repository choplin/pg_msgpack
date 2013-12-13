CREATE FUNCTION msgpack_in(cstring) RETURNS msgpack AS
'MODULE_PATHNAME'
LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION msgpack_out(msgpack) RETURNS cstring AS
'MODULE_PATHNAME'
LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION msgpack_recv(internal) RETURNS msgpack AS
'MODULE_PATHNAME'
LANGUAGE c IMMUTABLE STRICT;
CREATE FUNCTION msgpack_send(msgpack) RETURNS bytea AS
'MODULE_PATHNAME'
LANGUAGE c IMMUTABLE STRICT;

CREATE TYPE msgpack (
	INPUT = msgpack_in,
	OUTPUT = msgpack_out,
	RECEIVE = msgpack_recv,
	SEND = msgpack_send
);

CREATE CAST (msgpack AS json) WITH INOUT;
CREATE CAST (json AS msgpack) WITH INOUT;

CREATE CAST (msgpack AS bytea) WITHOUT FUNCTION AS ASSIGNMENT;
CREATE CAST (bytea AS msgpack) WITHOUT FUNCTION AS ASSIGNMENT;

CREATE FUNCTION msgpack_object_field(msgpack, text) RETURNS msgpack AS
'MODULE_PATHNAME'
LANGUAGE c IMMUTABLE STRICT;

CREATE OPERATOR -> (
	LEFTARG = msgpack,
	RIGHTARG = text,
	PROCEDURE = msgpack_object_field
);

CREATE FUNCTION msgpack_array_element(msgpack, integer) RETURNS msgpack AS
'MODULE_PATHNAME'
LANGUAGE c IMMUTABLE STRICT;

CREATE OPERATOR -> (
	LEFTARG = msgpack,
	RIGHTARG = integer,
	PROCEDURE = msgpack_array_element
);
