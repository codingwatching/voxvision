#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <voxrnd.h>
#include <stdlib.h>
#include "../modules.h"

static int get_position (lua_State *L)
{
    struct cameradata *camera = luaL_checkudata (L, 1, "voxrnd.camera");
    vox_dot position;
    camera->iface->get_position (camera->camera, position);
    WRITE_DOT (position);

    return 1;
}

static int set_position (lua_State *L)
{
    struct cameradata *camera = luaL_checkudata (L, 1, "voxrnd.camera");
    vox_dot position;
    READ_DOT (position, 2);
    camera->iface->set_position (camera->camera, position);

    return 0;
}

static int get_fov (lua_State *L)
{
    struct cameradata *camera = luaL_checkudata (L, 1, "voxrnd.camera");
    lua_pushnumber (L, camera->iface->get_fov (camera->camera));

    return 1;
}

static int set_fov (lua_State *L)
{
    struct cameradata *camera = luaL_checkudata (L, 1, "voxrnd.camera");
    float fov = luaL_checknumber (L, 2);
    camera->iface->set_fov (camera->camera, fov);

    return 0;
}

static int set_rot_angles (lua_State *L)
{
    struct cameradata *camera = luaL_checkudata (L, 1, "voxrnd.camera");
    vox_dot angles;
    READ_DOT (angles, 2);
    camera->iface->set_rot_angles (camera->camera, angles);

    return 0;
}

static int rotate_camera (lua_State *L)
{
    struct cameradata *camera = luaL_checkudata (L, 1, "voxrnd.camera");
    vox_dot delta;
    READ_DOT (delta, 2);
    camera->iface->rotate_camera (camera->camera, delta);

    return 0;
}

static int move_camera (lua_State *L)
{
    struct cameradata *camera = luaL_checkudata (L, 1, "voxrnd.camera");
    vox_dot delta;
    READ_DOT (delta, 2);
    camera->iface->move_camera (camera->camera, delta);

    return 0;
}

static int look_at (lua_State *L)
{
    struct cameradata *camera = luaL_checkudata (L, 1, "voxrnd.camera");
    vox_dot coord;
    READ_DOT (coord, 2);
    camera->iface->look_at (camera->camera, coord);

    return 0;
}

static int new_camera (lua_State *L)
{
    const char *name = luaL_checkstring (L, 1);
    struct vox_camera_interface *iface = vox_camera_methods (name);
    struct cameradata *data;
    int nret = 0;

    if (iface != NULL) {
        data = lua_newuserdata (L, sizeof (struct cameradata));
        data->camera = iface->construct_camera (NULL);
        data->iface = data->camera->iface;
        luaL_getmetatable (L, "voxrnd.camera");
        lua_setmetatable (L, -2);
        nret = 1;
    } else {
        lua_pushnil (L);
        lua_pushstring (L, "Cannot load camera");
        nret = 2;
    }

    return nret;
}

static int destroycamera (lua_State *L)
{
    struct cameradata *camera = luaL_checkudata (L, 1, "voxrnd.camera");
    camera->iface->destroy_camera (camera->camera);

    return 0;
}

static int printcamera (lua_State *L)
{
    lua_pushfstring (L, "<camera>");
    return 1;
}

static const struct luaL_Reg camera_methods [] = {
    {"__gc", destroycamera},
    {"__tostring", printcamera},
    {"get_position", get_position},
    {"set_position", set_position},
    {"get_fov", get_fov},
    {"set_fov", set_fov},
    {"set_rot_angles", set_rot_angles},
    {"rotate_camera", rotate_camera},
    {"move_camera", move_camera},
    {"look_at", look_at},
    {NULL, NULL}
};

static int new_cd (lua_State *L)
{
    struct vox_cd **data = lua_newuserdata (L, sizeof (struct vox_cd**));
    *data = vox_make_cd ();
    luaL_getmetatable (L, "voxrnd.cd");
    lua_setmetatable (L, -2);
    return 1;
}

static int printcd (lua_State *L)
{
    lua_pushfstring (L, "<collision detector>");
    return 1;
}

static int destroycd (lua_State *L)
{
    struct vox_cd **cd = luaL_checkudata (L, 1, "voxrnd.cd");
    free (*cd);

    return 0;
}

static int cd_attach_camera (lua_State *L)
{
    struct vox_cd **cd = luaL_checkudata (L, 1, "voxrnd.cd");
    struct cameradata *camera = luaL_checkudata (L, 2, "voxrnd.camera");
    float radius = luaL_checknumber (L, 3);

    vox_cd_attach_camera (*cd, camera->camera, radius);

    return 0;
}

static int cd_gravity (lua_State *L)
{
    struct vox_cd **cd = luaL_checkudata (L, 1, "voxrnd.cd");
    vox_dot gravity;
    READ_DOT (gravity, 2);

    vox_cd_gravity (*cd, gravity);

    return 0;
}

static const struct luaL_Reg cd_methods [] = {
    {"__gc", destroycd},
    {"__tostring", printcd},
    {"attach_camera", cd_attach_camera},
    {"gravity", cd_gravity},
    {NULL, NULL}
};

static int l_scene_proxy (lua_State *L)
{
    struct vox_node **ndata = luaL_checkudata (L, 1, "voxtrees.vox_node");
    struct scene_proxydata *data = lua_newuserdata (L, sizeof (struct scene_proxydata));

    luaL_getmetatable (L, "voxrnd.scene_proxy");
    lua_setmetatable (L, -2);

    /*
     * Store reference to the tree in the proxy's metatable to prevent GC from
     * destroying it.
     */
    luaL_getmetatable (L, "voxrnd.scene_proxy");
    lua_pushvalue (L, 1);
    lua_setfield (L, -2, "__tree");
    lua_pop (L, 1);

    /*
     * Fill C-side internal fields.
     */
    data->tree = *ndata;
    data->scene_group = dispatch_group_create ();
    data->scene_sync_queue = dispatch_queue_create ("scene operations", 0);
    return 1;
}

static int l_scene_proxy_destroy (lua_State *L)
{
    struct scene_proxydata *data = luaL_checkudata (L, 1, "voxrnd.scene_proxy");
    dispatch_release (data->scene_sync_queue);
    dispatch_release (data->scene_group);

    return 0;
}

static int l_scene_proxy_tostring (lua_State *L)
{
    lua_pushfstring (L, "<async scene proxy %p>", lua_topointer (L, 1));
    return 1;
}

static int l_scene_proxy_insert (lua_State *L)
{
    struct vox_node **ndata;
    struct scene_proxydata *data = luaL_checkudata (L, 1, "voxrnd.scene_proxy");
    float x,y,z;
    READ_DOT_3 (2, x, y, z);

    lua_getfield (L, 1, "__tree");
    ndata = luaL_checkudata (L, -1, "voxtrees.vox_node");

    dispatch_group_notify (data->scene_group, data->scene_sync_queue, ^{
            vox_dot dot;
            vox_dot_set (dot, x, y, z);
            vox_insert_voxel (&(data->tree), dot);
            *ndata = data->tree;
            vox_context_set_scene (data->context, data->tree);
        });

    return 0;
}

static int l_scene_proxy_delete (lua_State *L)
{
    struct vox_node **ndata;
    struct scene_proxydata *data = luaL_checkudata (L, 1, "voxrnd.scene_proxy");
    float x,y,z;
    READ_DOT_3 (2, x, y, z);

    lua_getfield (L, 1, "__tree");
    ndata = luaL_checkudata (L, -1, "voxtrees.vox_node");

    dispatch_group_notify (data->scene_group, data->scene_sync_queue, ^{
            vox_dot dot;
            vox_dot_set (dot, x, y, z);
            vox_delete_voxel (&(data->tree), dot);
            *ndata = data->tree;
            vox_context_set_scene (data->context, data->tree);
        });

    return 0;
}

static int l_scene_proxy_rebuild (lua_State *L)
{
    struct vox_node **ndata;
    struct scene_proxydata *data = luaL_checkudata (L, 1, "voxrnd.scene_proxy");

    lua_getfield (L, 1, "__tree");
    ndata = luaL_checkudata (L, -1, "voxtrees.vox_node");

    dispatch_group_async (data->scene_group,
                          dispatch_get_global_queue
                          (DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
                              struct vox_node *new_tree = vox_rebuild_tree (data->tree);
                              dispatch_sync (data->scene_sync_queue, ^{
                                      vox_destroy_tree (data->tree);
                                      data->tree = new_tree;
                                      /* Update lua reference */
                                      *ndata = new_tree;
                                      vox_context_set_scene (data->context, new_tree);
                                  });
                          });
    return 0;
}

static const struct luaL_Reg scene_proxy_methods [] = {
    {"__tostring", l_scene_proxy_tostring},
    {"__gc", l_scene_proxy_destroy},
    {"rebuild", l_scene_proxy_rebuild},
    {"insert", l_scene_proxy_insert},
    {"delete", l_scene_proxy_delete},
    {NULL, NULL}
};

static const struct luaL_Reg voxrnd [] = {
    {"camera", new_camera},
    {"cd", new_cd},
    {"scene_proxy", l_scene_proxy},
    {NULL, NULL}
};

int luaopen_voxrnd (lua_State *L)
{
    luaL_newmetatable(L, "voxrnd.camera");
    lua_pushvalue (L, -1);
    lua_setfield (L, -2, "__index");
    luaL_setfuncs (L, camera_methods, 0);

    luaL_newmetatable(L, "voxrnd.cd");
    lua_pushvalue (L, -1);
    lua_setfield (L, -2, "__index");
    luaL_setfuncs (L, cd_methods, 0);

    luaL_newmetatable(L, "voxrnd.scene_proxy");
    lua_pushvalue (L, -1);
    lua_setfield (L, -2, "__index");
    luaL_setfuncs (L, scene_proxy_methods, 0);

    luaL_newlib (L, voxrnd);
    return 1;
}
