/**
   @file geom.h
   @brief Basic geometry functions
**/

#ifndef __GEOM_H_
#define __GEOM_H_

#include "params.h"

/**
   \brief Take sum of two vectors.
**/
#ifdef SSE_INTRIN
#define vox_sum_vector(a,b,res) _mm_store_ps ((res), _mm_load_ps (a) + _mm_load_ps (b))
#else
void vox_sum_vector (const vox_dot a, const vox_dot b, vox_dot res);
#endif

/**
   \brief Calculate fast metric between two dots.

   This is a fast metric on R^3 calculacted as sum of absolute values of
   by-coordinate differences between two dots.
**/
float vox_abs_metric (const vox_dot dot1, const vox_dot dot2);

/**
   \brief Calculate metric between two dots.
   
   This is a square of standard Euclidian metric on R^3
**/
float vox_sqr_metric (const vox_dot dot1, const vox_dot dot2);


#ifdef VOXTREES_SOURCE
/**
   \brief Calculate a subspace index for the dot.
   
   For N-dimentional space we have 2^N ways to place the dot
   around the center of subdivision. Find which way is the case.
   
   \param dot1 the center of subdivision
   \param dot2 the dot we must calculate index for
   \return The subspace index in the range [0,2^N-1]
**/
int get_subspace_idx (const vox_dot dot1, const vox_dot dot2);

/**
   \brief Find intersection of a ray and an axis-aligned box.
   
   \param box a box to be checked
   \param origin a starting point of the ray
   \param dir the direction
   \param res where intersection is stored
   \return 1 if intersection is found, 0 otherwise
**/
int hit_box (const struct vox_box *box, const vox_dot origin, const vox_dot dir, vox_dot res);

/**
   \brief Find intersection of a ray and a plane.

   An intersection must be within a box.
   Plane must be axis-aligned.
   
   \param origin a starting point of the ray
   \param dir the direction
   \param planedot a dot on the plane
   \param planenum an axis number the plane is aligned with
   \param res where intersection is stored
   \param box a box
   \return 1 if intersection is found, 0 otherwise
**/
int hit_plane_within_box (const vox_dot origin, const vox_dot dir, const vox_dot planedot,
                          int planenum, vox_dot res, const struct vox_box *box);

/**
   \brief Find intersection of a box and a ball.
   
   The box and the ball are solid
**/
int box_ball_interp (const struct vox_box *box, const vox_dot center, float radius);
int dense_set_p (const struct vox_box *box, size_t n);
int voxel_in_box (const struct vox_box *box, const vox_dot dot);
void closest_vertex (const struct vox_box *box, const vox_dot dot, vox_dot res);
int divide_box (const struct vox_box *box, const vox_dot center, struct vox_box *res, int idx);
void get_dimensions (const struct vox_box *box, size_t dim[]);
int stripep (const struct vox_box *box, int *which);
#endif

#endif
