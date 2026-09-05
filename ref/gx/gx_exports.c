typedef struct table_s {
	const char *name;
	void *pointer;
} table_t;

extern int GetRefAPI( int version, ref_interface_t *funcs,
	ref_api_t *engfuncs, ref_globals_t *globals );

table_t lib_ref_gx_exports[] = {
	{ "GetRefAPI", (void *)GetRefAPI },
	{ NULL, NULL }
};