/*
 * $Id: lua.c,v 1.206.1.1 2013/04/12 18:48:47 roberto Exp $
 * Lua stand-alone interpreter
 * See Copyright Notice in lua.h
 */
#define lua_c
#include <nautilus/libccompat.h>
#include "lua/lua.h"

#include "lua/lauxlib.h"
#include "lua/lualib.h"
#include <nautilus/shell.h>

#ifdef NAUT_CONFIG_LOAD_LUA
#include <dev/lua_script.h>
#define lua_test 1
#endif

#if !defined(LUA_PROMPT)
#define LUA_PROMPT		"> "
#define LUA_PROMPT2		">> "
#endif

#if !defined(LUA_PROGNAME)
#define LUA_PROGNAME		"lua"
#endif

#if !defined(LUA_MAXINPUT)
#define LUA_MAXINPUT		512
#endif

#if !defined(LUA_INIT)
#define LUA_INIT		"LUA_INIT"
#endif

#define LUA_INITVERSION  \
	LUA_INIT "_" LUA_VERSION_MAJOR "_" LUA_VERSION_MINOR


#define lua_stdin_is_tty()	1  /* assume stdin is a tty */

/*
** lua_readline defines how to show a prompt and then read a line from
** the standard input.
** lua_saveline defines how to "save" a read line in a "history".
** lua_freeline defines how to free a line read by lua_readline.
*/
#if defined(LUA_USE_READLINE)

#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#define lua_readline(L,b,p)	((void)L, ((b)=readline(p)) != NULL)
#define lua_saveline(L,idx) \
        if (lua_rawlen(L,idx) > 0)  /* non-empty line? */ \
          add_history(lua_tostring(L, idx));  /* add it to history */
#define lua_freeline(L,b)	((void)L, free(b))

#elif !defined(lua_readline)

#define lua_readline(L,b,p) \
        ((void)L, fputs(p, stdout), fflush(stdout),  /* show prompt */ \
        fgets(b, LUA_MAXINPUT, stdin) != NULL)  /* get line */
#define lua_saveline(L,idx)	{ (void)L; (void)idx; }
#define lua_freeline(L,b)	{ (void)L; (void)b; }

#endif

static lua_State *globalL = NULL;
static const char *progname = LUA_PROGNAME;

static void 
lstop (lua_State *L, lua_Debug *ar) 
{
  (void)ar;  /* unused arg. */
  lua_sethook(L, NULL, 0, 0);
  luaL_error(L, "interrupted!");
}


static void 
laction (int i) 
{
  signal(i, SIG_DFL); 
  lua_sethook(globalL, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}


static void 
print_usage (const char *badoption) 
{
  luai_writestringerror("%s: ", progname);

  if (badoption[1] == 'e' || badoption[1] == 'l')
    luai_writestringerror("'%s' needs argument\n", badoption);
  else
    luai_writestringerror("unrecognized option '%s'\n", badoption);

  luai_writestringerror(
  "usage: %s [options] [script [args]]\n"
  "Available options are:\n"
  "  -e stat  execute string " LUA_QL("stat") "\n"
  "  -i       enter interactive mode after executing " LUA_QL("script") "\n"
  "  -l name  require library " LUA_QL("name") "\n"
  "  -v       show version information\n"
  "  -E       ignore environment variables\n"
  "  --       stop handling options\n"
  "  -        stop handling options and execute stdin\n"
  ,
  progname);
}


static void 
l_message (const char * pname, const char * msg) 
{
  if (pname) luai_writestringerror("%s: ", pname);
  luai_writestringerror("%s\n", msg);
}


static int 
report (lua_State * L, int status) 
{
  if (status != LUA_OK && !lua_isnil(L, -1)) {
    const char *msg = lua_tostring(L, -1);
    if (msg == NULL) msg = "(error object is not a string)";
    l_message(progname, msg);
    lua_pop(L, 1);
    /* force a complete garbage collection in case of errors */
    lua_gc(L, LUA_GCCOLLECT, 0);
  }
  return status;
}


/* the next function is called unprotected, so it must avoid errors */
static void 
finalreport (lua_State * L, int status) 
{
  if (status != LUA_OK) {
    const char *msg = (lua_type(L, -1) == LUA_TSTRING) ? lua_tostring(L, -1)
                                                       : NULL;
    if (msg == NULL) msg = "(error object is not a string)";
    l_message(progname, msg);
    lua_pop(L, 1);
  }
}


static int 
traceback (lua_State * L) 
{
  const char *msg = lua_tostring(L, 1);
  if (msg)
    luaL_traceback(L, L, msg, 1);
  else if (!lua_isnoneornil(L, 1)) {  /* is there an error object? */
    if (!luaL_callmeta(L, 1, "__tostring"))  /* try its 'tostring' metamethod */
      lua_pushliteral(L, "(no error message)");
  }
  return 1;
}


static int 
docall (lua_State * L, int narg, int nres) 
{
  int status;
  int base = lua_gettop(L) - narg;  /* function index */
  lua_pushcfunction(L, traceback);  /* push traceback function */
  lua_insert(L, base);  /* put it under chunk and args */
  globalL = L;  /* to be available to 'laction' */
  signal(SIGINT, laction);
  status = lua_pcall(L, narg, nres, base);
  signal(SIGINT, SIG_DFL);
  lua_remove(L, base);  /* remove traceback function */
  return status;
}


static void 
print_version (void) 
{
  luai_writestring(LUA_COPYRIGHT, strlen(LUA_COPYRIGHT));
  luai_writeline();
}


static int 
getargs (lua_State *L, char **argv, int n) 
{
    int narg;
    int i;
    int argc = 0;
    while (argv[argc]) argc++;  /* count total number of arguments */
    narg = argc - (n + 1);  /* number of arguments to the script */
    luaL_checkstack(L, narg + 3, "too many arguments to script");
    for (i=n+1; i < argc; i++)
        lua_pushstring(L, argv[i]);
    lua_createtable(L, narg, n + 1);
    for (i=0; i < argc; i++) {
        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, i - n);
    }
    return narg;
}


static int 
dofile (lua_State * L, const char *name) 
{
  int status = luaL_loadfile(L, name);
  if (status == LUA_OK) status = docall(L, 0, 0);
  return report(L, status);
}


static int 
dostring (lua_State * L, const char * s, const char * name) 
{
    int status = luaL_loadbuffer(L, s, strlen(s), name);
    if (status == LUA_OK) status = docall(L, 0, 0);
    return report(L, status);
}


static int 
dolibrary (lua_State * L, const char * name) 
{
    int status;
    lua_getglobal(L, "require");
    lua_pushstring(L, name);
    status = docall(L, 1, 1);  /* call 'require(name)' */
    if (status == LUA_OK)
        lua_setglobal(L, name);  /* global[name] = require return */
    return report(L, status);
}


static const char *
get_prompt (lua_State *L, int firstline) 
{
    const char *p;
    lua_getglobal(L, firstline ? "_PROMPT" : "_PROMPT2");
    p = lua_tostring(L, -1);
    if (p == NULL) p = (firstline ? LUA_PROMPT : LUA_PROMPT2);
    return p;
}

/* mark in error messages for incomplete statements */
#define EOFMARK		"<eof>"
#define marklen		(sizeof(EOFMARK)/sizeof(char) - 1)

static int 
incomplete (lua_State *L, int status) 
{
    if (status == LUA_ERRSYNTAX) {
        size_t lmsg;
        const char *msg = lua_tolstring(L, -1, &lmsg);
        if (lmsg >= marklen && strcmp(msg + lmsg - marklen, EOFMARK) == 0) {
            lua_pop(L, 1);
            return 1;
        }
    }

    return 0;
}


static int 
pushline (lua_State * L, int firstline) 
{
  char buffer[LUA_MAXINPUT];
  char *b = buffer;
  const char *prmt = get_prompt(L, firstline);
  size_t l;
  char buf[80]; // for gets
  
  nk_vc_printf("\n %s%s ","LUA",prmt); 
  int readstatus = nk_vc_gets(b,80,1);

  // int readstatus =1;
  lua_pop(L, 1);  /* remove result from 'get_prompt' */

  if (readstatus == -1) // Changed from 0 to -1
      return 0;  /* no input */

  l = strlen(b);

  if (l > 0 && b[l-1] == '\n')  /* line ends with newline? */
      b[l-1] = '\0';  /* remove it */

  if (firstline && b[0] == '=')  /* first line starts with `=' ? */
      lua_pushfstring(L, "return %s", b+1);  /* change it to `return' */
  else
      lua_pushstring(L, b);

  lua_freeline(L, b);

  return 1;
}


static int 
loadline (lua_State *L) 
{
    int status;
    lua_settop(L, 0);

    if (!pushline(L, 1)) {
        return -1;  /* no input */
    }

    for (;;) {  /* repeat until gets a complete line */
        size_t l;
        const char *line = lua_tolstring(L, 1, &l);
        status = luaL_loadbuffer(L, line, l, "=stdin");

        if (!incomplete(L, status)) break;  /* cannot try to add lines? */

        if (!pushline(L, 0))  /* no more input? */
            return -1;

        lua_pushliteral(L, "\n");  /* add a new line... */
        lua_insert(L, -2);  /* ...between the two lines */
        lua_concat(L, 3);  /* join them */
    }

    lua_saveline(L, 1);
    lua_remove(L, 1);  /* remove line */

    return status;
}


static void 
dotty (lua_State *L) 
{
    int status;
    const char *oldprogname = progname;
    progname = NULL;
    
    while ((status = loadline(L)) != -1) {

        if (status == LUA_OK) status = docall(L, 0, LUA_MULTRET);

        report(L, status);

        if (status == LUA_OK && lua_gettop(L) > 0) {  /* any result to print? */
            luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
            lua_getglobal(L, "print");
            lua_insert(L, 1);

            if (lua_pcall(L, lua_gettop(L)-1, 0, 0) != LUA_OK)
                l_message(progname, lua_pushfstring(L,
                            "error calling " LUA_QL("print") " (%s)",
                            lua_tostring(L, -1)));
        }
    }

    lua_settop(L, 0);  /* clear stack */
    luai_writeline();
    progname = oldprogname;
}


static char *
trimwhitespace (char * str)
{
    char * end;

    while (isspace((unsigned char)*str)) str++;
    
    if (!*str)  // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;

    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    *(end+1) = 0;

    return str;
}


static int 
handle_script (lua_State *L, char **argv, int n) 
{
    int status;
    char* buffname;

#ifdef NAUT_CONFIG_LOAD_LUA 
    buffname = read_lua_script();
    buffname = trimwhitespace(buffname);
#endif

    status = dostring(L,buffname,"=" LUA_INIT);

    if (status != LUA_OK)
        lua_pop(L, 1);

    return report(L, status);
}


/* check that argument has no extra characters at the end */
#define noextrachars(x)		{if ((x)[2] != '\0') return -1;}


/* indices of various argument indicators in array args */
#define has_i		0	/* -i */
#define has_v		1	/* -v */
#define has_e		2	/* -e */
#define has_E		3	/* -E */

#define num_has		4	/* number of 'has_*' */


static int 
collectargs (char **argv, int *args) 
{
  int i;
  
  for (i = 1; argv[i] != NULL; i++) {

    if (argv[i][0] != '-')  /* not an option? */
        return i;
    switch (argv[i][1]) {  /* option */
      case '-':
        noextrachars(argv[i]);
        return (argv[i+1] != NULL ? i+1 : 0);
      case '\0':
        return i;
      case 'E':
        args[has_E] = 1;
        break;
      case 'i':
        noextrachars(argv[i]);
        args[has_i] = 1;  /* go through */
      case 'v':
        noextrachars(argv[i]);
        args[has_v] = 1;
        break;
      case 'e':
        args[has_e] = 1;  /* go through */
      case 'l':  /* both options need an argument */
        if (argv[i][2] == '\0') {  /* no concatenated argument? */
          i++;  /* try next 'argv' */
          if (argv[i] == NULL || argv[i][0] == '-')
            return -(i - 1);  /* no next argument or it is another option */
        }
        break;
      default:  /* invalid option; return its index... */
        return -i;  /* ...as a negative value */
    }
  }
  return 0;
}


static int 
runargs (lua_State * L, char ** argv, int n) 
{
  int i;

  for (i = 1; i < n; i++) {
    lua_assert(argv[i][0] == '-');
    switch (argv[i][1]) {  /* option */
      case 'e': {
        const char *chunk = argv[i] + 2;
        if (*chunk == '\0') chunk = argv[++i];
        lua_assert(chunk != NULL);
        if (dostring(L, chunk, "=(command line)") != LUA_OK)
          return 0;
        break;
      }
      case 'l': {
        const char *filename = argv[i] + 2;
        if (*filename == '\0') filename = argv[++i];
        lua_assert(filename != NULL);
        if (dolibrary(L, filename) != LUA_OK)
          return 0;  /* stop if file fails */
        break;
      }
      default: break;
    }
  }
  return 1;
}


static int 
handle_luainit (lua_State * L) 
{
  const char *name = "=" LUA_INITVERSION;
  const char *init = getenv(name + 1);

  if (!init) {
    name = "=" LUA_INIT;
    init = getenv(name + 1);  /* try alternative name */
  }

  if (!init) return LUA_OK;
  else if (init[0] == '@')
    return dofile(L, init+1);
  else
    return dostring(L, init, name);
}


static int 
pmain (lua_State * L) 
{
    int argc     = (int)lua_tointeger(L, 1);
    char ** argv = (char **)lua_touserdata(L, 2);
    int script;
    int args[num_has];

    args[has_i] = args[has_v] = args[has_e] = args[has_E] = 0;

    if (argv[0] && argv[0][0]) {
        progname = argv[0];
    }

    if (argc > 1) { 
        script = collectargs(argv, args);
    } else {
        script = 0; 
    }

    if (script < 0) {  /* invalid arg? */
        print_usage(argv[-script]);
        return 0;
    }

    if (args[has_v]) {
        print_version();
    }

    if (args[has_E]) {  /* option '-E'? */
        lua_pushboolean(L, 1);  /* signal for libraries to ignore env. vars. */
        lua_setfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");
    }

    luaL_checkversion(L);
    lua_gc(L, LUA_GCSTOP, 0); /* stop collector during initialization */
    luaL_openlibs(L); 

    lua_gc(L, LUA_GCRESTART, 0);

    if (!args[has_E] && handle_luainit(L) != LUA_OK) {
        return 0;  
    }

    /* execute arguments -e and -l */
    if (!runargs(L, argv, (script > 0) ? script : argc)) return 0;

    /* execute main script (if there is one) */
    if (script && handle_script(L, argv, script) != LUA_OK) return 0;

    if (args[has_i]) {  /* -i option? */
        dotty(L);
    } else if (script == 0 && !args[has_e] && !args[has_v]) {  /* no arguments? */
        if (lua_stdin_is_tty()) {
            print_version();
            dotty(L);
        } else {
            dofile(L, NULL);  /* executes stdin as a file */
        }
    } 

    lua_pushboolean(L, 1);  /* signal no errors */

    return 1;
}


int 
lua_main (int argc, char ** argv) 
{
    int status, result;
    lua_State * L = luaL_newstate();

    if (!L) {
        l_message(argv[0], "cannot create state: not enough memory");
        return -1;
    }

    lua_pushcfunction(L, &pmain);
    lua_pushinteger(L, argc);
    lua_pushlightuserdata(L, argv);

    status = lua_pcall(L, 2, 1, 0);
    result = lua_toboolean(L, -1);  

    finalreport(L, status);

    lua_close(L);

    return (result && status == LUA_OK) ? 0 : -1;
}


#define LUA_MAX_CMD_TOKENS 128
#define LUA_MAX_TOKEN_LEN 64

static int 
handle_lua_cmd (char * buf, void * priv)
{
    char * argv[LUA_MAX_CMD_TOKENS];
    char * tmp  = NULL;
    int count   = 0;
    int st, i;

    tmp = strtok(buf, " ");

    while (tmp) {

        argv[count] = malloc(LUA_MAX_TOKEN_LEN);
        memset(argv[count], 0, LUA_MAX_TOKEN_LEN);

        if (count == 0) {
            snprintf(argv[count], LUA_MAX_TOKEN_LEN, "./%s", tmp);
        } else {
            strncpy(argv[count], tmp, LUA_MAX_TOKEN_LEN);
        }

        tmp = strtok(NULL, " "); 
        count++;
    }
        
    st = lua_main(count, argv);

    for (i = 0; i < count; i++) {
        free(argv[i]);
    }

    return 0;
}

static struct shell_cmd_impl lua_impl = {
    .cmd      = "lua",
    .help_str = "lua",
    .handler  = handle_lua_cmd,
};
nk_register_shell_cmd(lua_impl);
