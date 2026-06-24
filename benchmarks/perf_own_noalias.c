/* Magnitude of the own->noalias win: a hot loop that writes through a uniquely-
 * owned pointer (dst) while reading another (src). Without noalias the optimizer
 * must assume dst may alias src and reload/store dst every iteration; with
 * noalias (== own's unique-ownership guarantee) it register-promotes dst.
 * `restrict` is the C spelling of LLVM `noalias`. */
#include <stdio.h>
#include <time.h>
#include <stdint.h>
static double now_s(void){ return (double)clock()/(double)CLOCKS_PER_SEC; }

__attribute__((noinline))
int64_t accum_plain(int64_t *dst, const int64_t *src, int64_t n) {
    for (int64_t i = 0; i < n; i++) dst[0] += src[i & 1023];
    return dst[0];
}
__attribute__((noinline))
int64_t accum_noalias(int64_t * restrict dst, const int64_t * restrict src, int64_t n) {
    for (int64_t i = 0; i < n; i++) dst[0] += src[i & 1023];
    return dst[0];
}

int main(void){
    static int64_t src[1024];
    for (int i=0;i<1024;i++) src[i] = (i%7)-3;
    const int64_t N = 4000000000LL;
    int64_t d1=0, d2=0;
    double t0=now_s(); int64_t r1=accum_plain(&d1, src, N); double t1=now_s();
    double t2=now_s(); int64_t r2=accum_noalias(&d2, src, N); double t3=now_s();
    double plain=t1-t0, na=t3-t2;
    printf("plain=%.3fs  noalias=%.3fs  speedup=%.2fx  (r1=%lld r2=%lld)\n",
           plain, na, plain/na, (long long)r1, (long long)r2);
    return 0;
}
