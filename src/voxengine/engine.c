#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdlib.h>
#include <stdio.h>
#include <sys/param.h>
#include <dlfcn.h>
#include <assert.h>

#include "modules.h"
#include "engine.h"

static int engine_key;
static int quit_key;

static int engine_panic (lua_State *L)
{
    struct vox_engine **data;

    fprintf (stderr,
             "%s\n"
             "Unhandeled error, quiting. Doublecheck your scripts\n",
             lua_tostring (L, -1));

    lua_pushlightuserdata (L, &engine_key);
    lua_gettable (L, LUA_REGISTRYINDEX);
    data = lua_touserdata (L, -1);

    vox_destroy_engine (*data);
    exit(0);
}

// Load module, but do not add returned table to environment
static void load_module_restricted (lua_State *L, const char *modname)
{
    char path[MAXPATHLEN];
    char init[MAXPATHLEN];

    if (snprintf (path, MAXPATHLEN, "%s%s.so", VOX_MODULE_PATH, modname) >= MAXPATHLEN)
        luaL_error (L, "Module path name is too long");

    void *handle = dlopen (path, RTLD_LAZY | RTLD_NODELETE);
    if (handle == NULL) luaL_error (L, "Cannot open lua module %s", path);

    if (snprintf (init, MAXPATHLEN, "luaopen_%s", modname) >= MAXPATHLEN)
        luaL_error (L, "Module path name is too long");

    void *init_func = dlsym (handle, init);
    if (init_func == NULL) luaL_error (L, "Cannot cannot find init function %s", init);

    luaL_requiref (L, modname, init_func, 0);
}

static void load_module (lua_State *L, const char *modname)
{
    load_module_restricted (L, modname);
    // Copy table in our environment
    lua_setfield (L, -2, modname);
}

static void load_lua_module (lua_State *L, const char *modname)
{
    char path[MAXPATHLEN];

    if (snprintf (path, MAXPATHLEN, "%s%s.lua", VOX_MODULE_PATH, modname) >= MAXPATHLEN)
        luaL_error (L, "Module path name is too long");

    if (luaL_loadfile (L, path) || lua_pcall (L, 0, 1, 0))
        luaL_error (L, "Cannot load module %s: %s", modname, lua_tostring (L, -1));

    // Copy table in our environment
    lua_setfield (L, -2, modname);
}

static void set_safe_environment (lua_State *L)
{
    // Function to be executed is on top of the stack
    lua_getglobal (L, "voxvision");
    // FIXME: _ENV is upvalue #1
    lua_setupvalue (L, -2, 1);
}

static void prepare_safe_environment (lua_State *L)
{
    // Our environment is on top of the stack
#include <safe_environment.h>
}

static int l_get_geometry (lua_State *L)
{
    lua_pushlightuserdata (L, &engine_key);
    lua_gettable (L, LUA_REGISTRYINDEX);
    struct vox_engine **data = lua_touserdata (L, -1);
    struct vox_engine *engine = *data;

    lua_pushinteger (L, engine->width);
    lua_pushinteger (L, engine->height);

    return 2;
}

static int l_request_quit (lua_State *L)
{
    lua_pushlightuserdata (L, &quit_key);
    lua_pushboolean (L, 1);
    lua_settable (L, LUA_REGISTRYINDEX);
    return 0;
}

static void initialize_lua (struct vox_engine *engine)
{
    lua_State *L = luaL_newstate ();
    engine->L = L;

    luaL_openlibs (L);
    // Put engine in the registry and set panic handler
    lua_pushlightuserdata (L, &engine_key);
    struct vox_engine **data = lua_newuserdata (L, sizeof (struct vox_engine*));
    *data = engine;
    lua_settable (L, LUA_REGISTRYINDEX);
    lua_atpanic (L, engine_panic);

    // Place our safe environment is on top of the stack
    lua_newtable (L);
    // Copy environment in global variable
    lua_pushvalue (L, -1);
    lua_setglobal (L, "voxvision");

    // Load C modules
    load_module (L, "voxtrees");
    load_module (L, "voxrnd");

    // Also add some safe functions
    prepare_safe_environment (L);

    // Add some core functions
    lua_pushcfunction (L, l_request_quit);
    lua_setfield (L, -2, "request_quit");
    lua_pushcfunction (L, l_get_geometry);
    lua_setfield (L, -2, "get_geometry");

    // And load lua modules
    load_lua_module (L, "voxutils");

    // Remove the environment from the stack, it is in "voxvision" global now
    lua_pop (L, 1);
}

static void execute_init (struct vox_engine *engine)
{
    lua_State *L = engine->L;

    /*
     * FIXME:
     * Drop the old table from execution of the previous script.
     * GC must free all resources.
     */
    if (engine->script_executed)
    {
        lua_pop (L, 1);
        lua_gc (L, LUA_GCCOLLECT, 0);
    }

    lua_getglobal (L, "voxvision");
    lua_getfield (L, -1, "init");
    lua_remove (L, 1);

    if (!lua_isfunction (L, 1))
        luaL_error (L, "init is not a function: %s", lua_tostring (L, -1));

    set_safe_environment (L);

    if (lua_pcall (L, 0, 1, 0))
        luaL_error (L, "Error executing init function: %s", lua_tostring (L, -1));

    lua_getfield (L, 1, "tree");
    lua_getfield (L, 1, "camera");
    lua_getfield (L, 1, "cd");

    /*
     * "tree" field can contain the tree itself, or a thread-safe proxy. The
     * latter can be used to perform insert, delete and rebuild calls in the
     * tick() function in a thread-safe manner.
     */
    struct scene_proxydata *scene = luaL_testudata (L, 2, SCENE_PROXY_META);
    struct vox_node **ndata;
    if (scene != NULL) {
        engine->tree = scene->tree;
        engine->rendering_queue = scene->scene_sync_queue;
        scene->context = engine->ctx;
    } else {
        ndata = luaL_checkudata (L, 2, TREE_META);
        engine->tree = *ndata;
    }
    struct cameradata *cdata = luaL_checkudata (L, 3, CAMERA_META);
    struct vox_cd **cd = NULL;
    if (!lua_isnil (L, 4)) cd = luaL_checkudata (L, 4, CD_META);

    engine->camera = cdata->camera;
    if (cd != NULL) engine->cd = *cd;

    // Remove tree, camera and cd from the stack
    lua_pop (L, 3);

    vox_context_set_scene (engine->ctx, engine->tree);
    vox_context_set_camera (engine->ctx, engine->camera);
    if (engine->cd != NULL) vox_cd_attach_context (engine->cd, engine->ctx);
    engine->script_executed = 1;
}

void vox_engine_load_script (struct vox_engine *engine, const char *script)
{
    lua_State *L = engine->L;

    if (luaL_loadfile (L, script))
        luaL_error (L, "Error loading script %s: %s", script,
                    lua_tostring (L, -1));
    set_safe_environment (L);

    if (lua_pcall (L, 0, 0, 0))
        luaL_error (L,
                    "Error executing script %s\n"
                    "%s",
                    script,
                    lua_tostring (L, -1));

    execute_init (engine);

    // Check that we have only the world table on the stack
    assert (lua_gettop (engine->L) == 1);
}

static void execute_tick (struct vox_engine *engine)
{
    lua_State *L = engine->L;

    lua_getglobal (L, "voxvision");
    lua_getfield (L, -1, "tick");

    if (!lua_isnil (L, -1))
    {
        // Assume that tick is a function
        set_safe_environment (L);

        // Copy the "world" table
        lua_pushvalue (L, 1);
        lua_pushnumber (L, SDL_GetTicks());
        if (lua_pcall (L, 2, 0, 0))
            luaL_error (L, "error executing tick function: %s", lua_tostring (L, -1));
        lua_pop (L, 1);
    }
    else
    {
        lua_pop (L, 2);
        // Do very basic SDL event handling here
        SDL_Event event;

        if (SDL_PollEvent (&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                l_request_quit (L);
                break;
            default:
                ;
            }
        }
    }
}

struct vox_engine* vox_create_engine (int width, int height)
{
    struct vox_engine *engine;
 
    engine = malloc (sizeof (struct vox_engine));
    memset (engine, 0, sizeof (struct vox_engine));
    engine->width = width;
    engine->height = height;

    initialize_lua (engine);
    assert (lua_gettop (engine->L) == 0);

    // Init SDL
    if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        fprintf (stderr, "Cannot init SDL: %s\n", SDL_GetError());
        goto bad;
    }

    engine->ctx = vox_make_context_and_window (engine->width, engine->height);
    if (engine->ctx == NULL)
    {
        fprintf (stderr, "Cannot create the context: %s\n", SDL_GetError());
        goto bad;
    }

    return engine;

bad:
    vox_destroy_engine (engine);
    return NULL;
}

int vox_engine_quit_requested (struct vox_engine *engine)
{
    lua_State *L = engine->L;
    int quit;

    lua_pushlightuserdata (L, &quit_key);
    lua_gettable (L, LUA_REGISTRYINDEX);
    quit = lua_toboolean (L, -1);
    lua_pop (L, 1);

    return quit;
}

int vox_engine_tick (struct vox_engine *engine)
{
    if (!engine->script_executed) return 0;

    /*
     * If we have a queue associated with the tree (this is so if we use
     * thread-safe scene proxy in the world table), enqueue the rendering
     * operation to that queue and wait for its completion. This will assure us
     * that the tree is in consistent state while the rendering is performed.
     */
    if (engine->rendering_queue != NULL) {
        dispatch_sync (engine->rendering_queue, ^{
                vox_render (engine->ctx);
            });
    }
    else vox_render (engine->ctx);
    vox_redraw (engine->ctx);

    execute_tick (engine);
    if (engine->cd != NULL) vox_cd_collide (engine->cd);
    assert (lua_gettop (engine->L) == 1);
    return 1;
}

void vox_destroy_engine (struct vox_engine *engine)
{
    if (engine->L != NULL) lua_close (engine->L);
    if (engine->ctx != NULL) vox_destroy_context (engine->ctx);
    if (SDL_WasInit(0)) SDL_Quit();
    free (engine);
}
