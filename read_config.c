#include"read_config.h"

#include<unistd.h>

#include<assert.h>
#include<stdlib.h>
#include<string.h>

#include<lua.h>
#include<lauxlib.h>
#include<lualib.h>

// gcc ./inotify_rsync.c `pkg-config --cflags --libs glib-2.0 lua5.4`

struct config *
read_config (char * path)
{
  struct config * tobackup;

  lua_State * Lua = luaL_newstate ();
  luaL_openlibs (Lua);

  if (luaL_loadfile(Lua, path)||lua_pcall(Lua, 0, 0, 0))
  {
    fprintf (stderr, "failed loading file\n");
	_exit (1);
  }

  lua_getglobal (Lua, "paths");  // 1
  lua_len (Lua, -1);  // 2
  int npath = lua_tointeger (Lua, -1);
  tobackup = malloc (sizeof (struct src_dst) * npath + sizeof (struct config));
  tobackup->count = npath;
  for (int i = 1; i <= npath; i++)
  {  // 2
    lua_pushinteger (Lua, i);  // 3
    lua_gettable (Lua, -3);
    lua_pushstring (Lua, "src");  // 4
    lua_gettable (Lua, -2);
    const char * src = lua_tostring (Lua, -1);
	assert (src != 0);
    tobackup->v[i-1].src = malloc (strlen (src) + 1);
    strcpy ((void*)tobackup->v[i-1].src, src);
    lua_pushstring (Lua, "dst");  // 5
    lua_gettable (Lua, -3);
    const char * dst = lua_tostring (Lua, -1);
	assert (dst != 0);
    tobackup->v[i-1].dst = malloc (strlen (dst) + 1);
    strcpy ((void*)tobackup->v[i-1].dst, dst);
    lua_pop (Lua, 3);  // 2
  }
  lua_pop (Lua, 2);  // 0

  lua_close (Lua);

  return tobackup;
}
