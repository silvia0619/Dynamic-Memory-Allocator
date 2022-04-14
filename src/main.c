#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    size_t sz_u = 200, sz_v = 300, sz_w = 200, sz_x = 500, sz_y = 200;
    /* void *u = */ sf_malloc(sz_u);
    void *v = sf_malloc(sz_v);
    void *w = sf_malloc(sz_w);
    void *x = sf_malloc(sz_x);
    /* void *y = */ sf_malloc(sz_y);

    sf_free(v);
    sf_free(x);
    sf_free(w);
    // sf_free(sf_malloc(200));
    return EXIT_SUCCESS;
}
