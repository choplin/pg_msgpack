CREATE EXTENSION pg_msgpack;

-- conversion
SELECT '[null, true, false, 10, 5.500000, {"a":"b"}]'::msgpack;

-- operator
SELECT '{"a":"b"}'::msgpack -> 'a';
SELECT '[1,2,3]'::msgpack -> 1;
