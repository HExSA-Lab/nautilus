.align 4096
.global __LUA_SCRIPT_START 
__LUA_SCRIPT_START:
.incbin "lua_script.txt"
.global __LUA_SCRIPT_END 
__LUA_SCRIPT_END:
