#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include "tree.h"
#include "datareader.h"

struct vox_node* vox_read_raw_data (const char *filename, unsigned int dim[],
                                    unsigned int samplesize, unsigned int threshold,
                                    const char **error)
{
    struct vox_node *tree = NULL;
    int fd = open (filename, O_RDONLY);
    if (fd == -1) {*error = strerror (errno); goto done;}
    vox_dot *array;

#define BUFSIZE 4096
    unsigned char buf[BUFSIZE];
    ssize_t end = 0;
    ssize_t pos = 0;

    int i,j,k,l,n = dim[0]*dim[1]*dim[2];
    int counter = 0;
    unsigned int value;

    struct stat sb;
    if (fstat (fd, &sb) == -1) {*error = strerror (errno); goto closefd;}
    if (n*samplesize != sb.st_size) {*error = "Wrong size of dataset"; goto closefd;}

    array = aligned_alloc (16, n*sizeof(vox_dot));
    if (array == NULL) {*error = strerror (errno); goto freemem;}
    
    for (i=0; i<dim[0]; i++)
    {
        for (j=0; j<dim[1]; j++)
        {
            for (k=0; k<dim[2]; k++)
            {
                value = 0;
                for (l=0; l<samplesize; l++)
                {
                    if (pos==end)
                    {
                        ssize_t res = read (fd, buf, BUFSIZE);
                        if (res <= 0) {*error = strerror (errno); goto freemem;}
                        end = res;
                        pos = 0;
                    }
                    value |= buf[pos] << (8*l);
                    pos++;
                }
                if (value > threshold)
                {
                    array[counter][0] = i*vox_voxel[0];
                    array[counter][1] = j*vox_voxel[1];
                    array[counter][2] = k*vox_voxel[2];
                    counter++;
                }
            }
        }
    }
    tree = vox_make_tree (array, counter);

freemem:
    free (array);
closefd:
    close (fd);
done:
    return tree;
}
