#include <string.h>
#include "camera.h"
#include "modules.h"

void vox_use_camera_methods (struct vox_camera *camera, const struct vox_camera_interface *iface)
{
    size_t nfields = sizeof (struct vox_camera_interface)/sizeof(void*);
    size_t i;
    void* const * src = (void * const *)iface;
    void **dst = (void**)camera->iface;

    for (i=0; i<nfields; i++)
    {
        if (src[i] != NULL) dst[i] = src[i];
    }
}

static void dummy_vector (const struct vox_camera *camera, vox_dot ray, ...)
{
    vox_dot_set (ray, 0, 0, 0);
}

static void dummy_void (struct vox_camera *camera, ...)
{
}

static float dummy_fl (struct vox_camera *camera, ...)
{
    return 0;
}

static void* dummy_ptr (struct vox_camera *camera, ...)
{
    return NULL;
}

static void destroy_camera (struct vox_camera *camera)
{
    free (camera->iface);
    free (camera);
}

struct vox_camera_interface dummy_methods = {
    .screen2world = (void*)dummy_vector,
    .rotate_camera = (void*)dummy_void,
    .move_camera = (void*)dummy_void,
    .look_at = (void*)dummy_void,
    .set_rot_angles = (void*)dummy_void,
    .get_position = (void*)dummy_vector,
    .set_position = (void*)dummy_void,
    .get_fov = (void*)dummy_fl,
    .set_fov = (void*)dummy_void,
    .set_window_size = (void*)dummy_void,

    .construct_camera = (void*)dummy_ptr,
    .destroy_camera = destroy_camera
};

void vox_init_camera (struct vox_camera *camera)
{
    camera->iface = malloc (sizeof (struct vox_camera_interface));
    memcpy (camera->iface, &dummy_methods, sizeof (struct vox_camera_interface));
}

struct vox_camera_interface* vox_camera_methods (const char *name)
{
    struct vox_camera_interface* iface =
        (struct vox_camera_interface*)vox_load_module (name, CAMERA_MODULE);
    return iface;
}
