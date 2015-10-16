#ifndef  _LUACOMPAT_H_U8TPOMXK_
#define  _LUACOMPAT_H_U8TPOMXK_

#if defined (__cplusplus)
extern "C" {
#endif

#if LUA_VERSION_NUM < 502

# ifndef luaL_newlib
#  define luaL_newlib(L,l) (lua_newtable(L), luaL_register(L,NULL,l))
# endif

# ifndef lua_rawlen
#  define lua_rawlen(L, i) lua_objlen(L, i)
# endif

# ifndef luaL_checkversion
#  define luaL_checkversion(L) do {} while(0)
# endif

# ifndef lua_getuservalue
#  define lua_getuservalue(L, i) lua_getfenv(L, i)
# endif

# ifndef lua_setuservalue
#  define lua_setuservalue(L, i) lua_setfenv(L, i)
# endif

# ifndef luaL_tolstring
#  define luaL_tolstring(L, i, s) lua_tolstring(L, i, s)
# endif

# ifndef luaL_setfuncs
#  define luaL_setfuncs(L, l, n) _luaL_setfuncs(L, l, n)
# endif

static void _luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
	luaL_checkstack(L, nup, "too many upvalues");
	for (; l->name != NULL; l++) {  /* fill the table with given functions */
		int i;
		for (i = 0; i < nup; i++)  /* copy upvalues to the top */
			lua_pushvalue(L, -nup);
		lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
		lua_setfield(L, -(nup + 2), l->name);
	}
	lua_pop(L, nup);  /* remove upvalues */
}
#endif // end of LUA_VERSION_NUM < 502

#if defined (__cplusplus)
}	/*end of extern "C"*/
#endif

#endif /* end of include guard:  _LUACOMPAT_H_U8TPOMXK_ */

