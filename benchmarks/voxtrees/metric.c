#include <stdio.h>
#include <voxtrees.h>
#include <gettime.h>

#define N 300000000

int main()
{
    vox_dot dot1, dot2;
    float acc = 0;
    double time;
    int i;

    time = gettime();
    for (i=0; i<N; i++)
    {
#ifdef SSE_INTRIN
        _mm_store_ps (dot1, _mm_set_ps (0, i,   i+1, i+2));
        _mm_store_ps (dot2, _mm_set_ps (0, i+1, i+2, i+3));
#else
        dot1[0] = i;   dot1[1] = i+1; dot1[2] = i+2;
        dot2[0] = i+1; dot2[1] = i+2; dot2[2] = i+3;
#endif
        acc += vox_sqr_metric (dot1, dot2);
    }
    time = gettime () - time;
    printf ("vox_sqr_metric = %f, time = %f\n", acc, time);

    acc = 0;
    time = gettime();
    for (i=0; i<N; i++)
    {
#ifdef SSE_INTRIN
        _mm_store_ps (dot1, _mm_set_ps (0, i,   i+1, i+2));
        _mm_store_ps (dot2, _mm_set_ps (0, i+1, i+2, i+3));
#else
        dot1[0] = i;   dot1[1] = i+1; dot1[2] = i+2;
        dot2[0] = i+1; dot2[1] = i+2; dot2[2] = i+3;
#endif
        acc += vox_abs_metric (dot1, dot2);
    }
    time = gettime () - time;
    printf ("vox_abs_metric = %f, time = %f\n", acc, time);
    printf ("Metric functions were called %i times\n", N);

    return 0;
}
