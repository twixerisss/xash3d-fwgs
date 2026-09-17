extern table_t lib_filesystem_stdio_exports[];
extern table_t lib_hl_exports[]; //hl_dll
extern table_t lib_ref_gx_exports[]; //ref_gx
#if XASH_REF_GL_ENABLED
extern table_t lib_ref_gl_exports[];
#endif
#if XASH_REF_SOFT_ENABLED
extern table_t lib_ref_soft_exports[];
#endif
extern table_t lib_cl_dll_exports[]; //cl_dll
extern table_t lib_menu_exports[];

struct {const char *name;void *func;} libs[] = {
{ "filesystem_stdio", &lib_filesystem_stdio_exports },
{ "ref_gx", &lib_ref_gx_exports },
#if XASH_REF_GL_ENABLED
{ "ref_gl", &lib_ref_gl_exports },
#endif
#if XASH_REF_SOFT_ENABLED
{ "ref_soft", &lib_ref_soft_exports },
#endif
{ "server", &lib_hl_exports },
{ "client", &lib_cl_dll_exports },
{ "menu", &lib_menu_exports},
{ NULL, NULL } // Lib_Find walks until this; losing it means it walks off the end
};
