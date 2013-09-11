/**
   @file search.h
   @brief Algorithms for searching in tree

   Searching for intersection with a ray and a tree and (maybe) other
**/

#include <stdlib.h>
#include <string.h>

#include "tree.h"
#include "geom.h"
#include "search.h"

void gen_subspaces (struct tagged_coord subspaces[], int n)
{
    // Turn plane numbers into subspace indices by simply XORing
    // old subspace index with plane number
    int i;

    for (i=1; i<n; i++)  subspaces[i].tag = (1 << subspaces[i].tag) ^ subspaces[i-1].tag;
}

int compare_tagged (float *origin, struct tagged_coord *c1, struct tagged_coord *c2)
{
    float *dot1 = c1->coord;
    float *dot2 = c2->coord;

    return calc_abs_metric (origin, dot1) - calc_abs_metric (origin, dot2);
}

// Maybe flollowing deserves a bit more explanation

/**
   \brief Find intersection of a tree and a ray.
   
   \param tree a tree
   \param origin starting point of the ray
   \param dir direction of the ray
   \param res where result is stored
   \param depth an initial depth of recursion. Better to specify 1
   \param lod Level of Detail. depth = 1, lod = 0 means no level of detail
   \return 1 if intersection was found, 0 otherwise
**/
int ray_tree_intersection (struct node *tree, const float *origin, const float *dir, float *res, int depth, int lod)
{
    float inter[N];
    float tmp[N];
    int interp, i;

    if (!(FULLP (tree))) return 0;
    if (!(hit_box (tree->bb_min, tree->bb_max, origin, dir, inter))) return 0;
    if (depth == lod) // Desired level of details reached -> intersection found
    {
        memcpy (res, inter, sizeof (float)*N);
        return 1;
    }
    
    if (LEAFP(tree))
    {
        // If passed argument is a tree leaf, do O(tree->dots_num) search for intersections
        // with voxels stored in the leaf and return closest one (also O(tree->dots_num))
        int count = 0;
        float intersect[MAX_DOTS][N];
        leaf_data leaf = tree->data.leaf;
        for (i=0; i<leaf.dots_num; i++)
        {
            sum_vector (leaf.dots[i], voxel, tmp);
            interp = hit_box (leaf.dots[i], tmp, origin, dir, inter);
            if (interp)
            {
                memcpy (intersect[count], inter, sizeof(float)*N);
                count++;
            }
        }
        if (count)
        {
            memcpy (res, closest_in_set (intersect, count, origin, calc_abs_metric), sizeof(float)*N);
            return 1;
        }
        else return 0;
    }
    // ELSE

    inner_data inner = tree->data.inner;
    struct tagged_coord plane_inter[N+1];
    int plane_counter = 1;
    // Init tagged_coord structure with "entry point" in the node, so to say
    // The tag is a subspace index of entry point
    plane_inter[0].tag = get_subspace_idx (inner.center, inter);
    memcpy (plane_inter[0].coord, inter, sizeof(float)*N);

    // Now, search for intersections of the ray and all N axis-aligned planes
    // for our N-dimentional space.
    // If such an intersection is inside the node (it means, inside its bounding box),
    // and it to plane_inter and mark with number of plane where intersection is occured.
    for (i=0; i<N; i++)
    {
        interp = hit_plane (origin, dir, inner.center, i, plane_inter[plane_counter].coord);
        plane_inter[plane_counter].tag = i;
        if (interp && (dot_betweenp (tree->bb_min, tree->bb_max, plane_inter[plane_counter].coord))) plane_counter++;
    }

    // We want closest intersection to be found, so sort intersections with planes by proximity to origin
    qsort_r (plane_inter+1, plane_counter-1, sizeof(struct tagged_coord), origin, compare_tagged);
    // Convert plane numbers to subspace indices
    gen_subspaces (plane_inter, plane_counter);

    // For each intersection call ray_tree_intersection recursively, using child node specified by subspace index
    // in tagged structure. If intersection is found, return.
    // Note, what we specify an entry point to that child as a new ray origin
    for (i=0; i<plane_counter; i++)
    {
        interp = ray_tree_intersection (inner.children[plane_inter[i].tag], plane_inter[i].coord, dir, res, depth+1, lod);
        if (interp) return 1;
    }
    
    return 0;
}
