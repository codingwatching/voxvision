#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <dlfcn.h>
#include <assert.h>

#include <modules.h>
#include "engine.h"

static int engine_panic (lua_State *L)
{
    struct vox_engine *engine;

    fprintf (stderr,
             "%s\n"
             "Unhandeled error, quiting. Doublecheck your scripts\n",
             lua_tostring (L, -1));

    lua_getfield (L, LUA_REGISTRYINDEX, "voxengine");
    engine = lua_topointer (L, -1);

    vox_destroy_engine (engine);
    exit(0);
}

static void usage()
{
    fprintf (stderr, "Usase: <program> [-w width] [-h height] -s script [rest]\n");
}

static void load_module (lua_State *L, const char *modname)
{
    char path[MAXPATHLEN];
    char init[MAXPATHLEN];

    strcpy (path, LUA_MODULE_PATH);
    strcat (path, modname);
    strcat (path, ".so");

    void *handle = dlopen (path, RTLD_LAZY | RTLD_NODELETE);
    if (handle == NULL) luaL_error (L, "Cannot open lua module %s", path);

    strcpy (init, "luaopen_");
    strcat (init, modname);

    void *init_func = dlsym (handle, init);
    if (init_func == NULL) luaL_error (L, "Cannot cannot find initfunction %s", init);

    luaL_requiref (L, modname, init_func, 0);
    // Copy table in our environment
    lua_setfield (L, -2, modname);
}

static void set_safe_environment (lua_State *L)
{
    // Function is on top of the stack
    lua_getglobal (L, "voxvision");
    // FIXME: _ENV is upvalue #1
    lua_setupvalue (L, -2, 1);
}

static void prepare_safe_environment (lua_State *L)
{
    // Our environment is on top of the stack
    lua_getglobal (L, "print");
    lua_setfield (L, -2, "print");

    lua_getglobal (L, "ipairs");
    lua_setfield (L, -2, "ipairs");

    lua_getglobal (L, "pairs");
    lua_setfield (L, -2, "pairs");

    if (luaL_loadstring (L, "return require \"math\"") || lua_pcall (L, 0, 1, 0))
        luaL_error (L, "Cannot load math module");

    lua_getfield (L, -1, "sin");
    lua_setfield (L, -3, "sin");

    lua_getfield (L, -1, "cos");
    lua_setfield (L, -3, "cos");

    lua_pop (L, 1);
}

static void initialize_lua (struct vox_engine *engine)
{
    lua_State *L = luaL_newstate ();
    engine->L = L;
    int res;

    luaL_openlibs (L);
    // Put engine in the registry and set panic handler
    lua_pushlightuserdata (L, engine);
    lua_setfield (L, LUA_REGISTRYINDEX, "voxengine");
    lua_atpanic (L, engine_panic);

    // Environment is on top of the stack
    lua_newtable (L);
    // Copy environment in global variable
    lua_pushvalue (L, -1);
    lua_setglobal (L, "voxvision");

    load_module (L, "voxtrees");
    load_module (L, "voxrnd");
    load_module (L, "voxsdl");

    // Also add some safe functions
    prepare_safe_environment (L);
    lua_pop (L, 1);

    if (luaL_loadfile (L, engine->script))
        luaL_error (L, "Error loading script %s: %s", engine->script,
                    lua_tostring (L, -1));
    set_safe_environment (L);

    if ((res = lua_pcall (L, 0, 0, 0)))
        luaL_error (L,
                    "Error executing script %s\n"
                    "%s",
                    engine->script,
                    lua_tostring (L, -1));
}

static void execute_tick (struct vox_engine *engine)
{
    lua_State *L = engine->L;

    lua_getglobal (L, "voxvision");
    lua_getfield (L, -1, "tick");

    if (lua_isfunction (L, -1))
    {
        set_safe_environment (L);

        // Copy the tree and the camera
        lua_pushvalue (L, 1);
        lua_pushvalue (L, 2);
        lua_pushnumber (L, SDL_GetTicks());
        lua_pushnumber (L, engine->fps_info.frame_time);
        if (lua_pcall (L, 4, 0, 0))
            luaL_error (L, "error executing tick function: %s", lua_tostring (L, -1));
        lua_pop (L, 1);
    }
    else if (!lua_isnil (L, -1))
        luaL_error (L, "tick is not a function: %s", lua_tostring (L, -1));
    else lua_pop (L, 2);
}

static void execute_init (struct vox_engine *engine)
{
    lua_State *L = engine->L;

    lua_getglobal (L, "voxvision");
    lua_getfield (L, -1, "init");

    if (!lua_isfunction (L, -1))
        luaL_error (L, "init is not a function: %s", lua_tostring (L, -1));

    set_safe_environment (L);

    if (lua_pcall (L, 0, 2, 0))
        luaL_error (L, "Error executing init function: %s", lua_tostring (L, -1));

    struct nodedata *ndata = luaL_checkudata (L, -2, "voxtrees.vox_node");
    struct cameradata *cdata = luaL_checkudata (L, -1, "voxrnd.camera");
    lua_remove (L, 1);

    engine->camera = cdata->camera;
    engine->tree = ndata->node;
}

struct vox_engine* vox_create_engine (int *argc, char **argv[])
{
    struct vox_engine *engine;
    int ch, width = 800, height = 600, fps = 30;
    const char *script = NULL;

    while ((ch = getopt (*argc, *argv, "w:h:s:f:")) != -1)
    {
        switch (ch)
        {
        case 'w':
            width = strtol (optarg, NULL, 10);
            break;
        case 'h':
            height = strtol (optarg, NULL, 10);
            break;
        case 's':
            script = optarg;
            break;
        case 'f':
            fps = strtol (optarg, NULL, 10);
            break;
        case '?':
        default:
            usage();
            return NULL;
        }
    }

    *argc -= optind;
    *argv += optind;

    if (script == NULL)
    {
        usage ();
        return NULL;
    }

    engine = malloc (sizeof (struct vox_engine));
    memset (engine, 0, sizeof (struct vox_engine));
    engine->width = width;
    engine->height = height;
    engine->script = script;

    initialize_lua (engine);
    assert (lua_gettop (engine->L) == 0);
    execute_init (engine);
    assert (lua_gettop (engine->L) == 2);

    // Init SDL
    if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        fprintf (stderr, "Cannot init SDL: %s\n", SDL_GetError());
        goto bad;
    }

    if (SDL_CreateWindowAndRenderer (engine->width, engine->height,
                                     0,
                                     &engine->window, &engine->renderer))
    {
        fprintf (stderr, "Cannot create a window: %s\n",
                 SDL_GetError());
        goto bad;
    }

    engine->texture = SDL_CreateTexture (engine->renderer, SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         engine->width, engine->height);
    if (engine->texture == NULL)
    {
        fprintf (stderr, "Cannot create texture: %s\n",
                 SDL_GetError());
        goto bad;
    }

    int bpp;
    Uint32 Rmask, Gmask, Bmask, Amask;
    SDL_PixelFormatEnumToMasks (SDL_PIXELFORMAT_ARGB8888, &bpp, &Rmask, &Gmask, &Bmask, &Amask);
    engine->surface = SDL_CreateRGBSurface (0, engine->width, engine->height,
                                            bpp, Rmask, Gmask, Bmask, Amask);
    if (engine->surface == NULL)
    {
        fprintf (stderr, "Cannot create surface: %s\n",
                 SDL_GetError());
        goto bad;
    }

    engine->ctx = vox_make_renderer_context (engine->surface, engine->tree, engine->camera);
    engine->fps_controller = vox_make_fps_controller (fps);

    return engine;

bad:
    vox_destroy_engine (engine);
    return NULL;
}

void vox_engine_tick (struct vox_engine *engine)
{
    vox_render (engine->ctx);
    SDL_UpdateTexture (engine->texture, NULL, engine->surface->pixels, engine->surface->pitch);
    SDL_FillRect (engine->surface, NULL, 0);
    SDL_RenderCopy (engine->renderer, engine->texture, NULL, NULL);
    SDL_RenderPresent (engine->renderer);
    engine->fps_info = engine->fps_controller();

    engine->event_occured = SDL_PollEvent (&(engine->event));
    execute_tick (engine);
    assert (lua_gettop (engine->L) == 2);
}

void vox_destroy_engine (struct vox_engine *engine)
{
    if (engine->L != NULL) lua_close (engine->L);
    if (engine->surface != NULL) SDL_FreeSurface(engine->surface);
    if (engine->texture != NULL) SDL_DestroyTexture (engine->texture);
    if (engine->renderer != NULL) SDL_DestroyRenderer (engine->renderer);
    if (engine->window != NULL) SDL_DestroyWindow (engine->window);
    if (engine->fps_controller != NULL) vox_destroy_fps_controller (engine->fps_controller);
    if (engine->ctx != NULL) free (engine->ctx);
    if (SDL_WasInit(0)) SDL_Quit();
    free (engine);
}
