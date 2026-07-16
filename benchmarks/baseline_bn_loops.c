#include <stdio.h>
#include <stdlib.h>
static long long hist[41];
static int perm[20], nxt[40], visited[40];
static int N, twoN;
static void process_perm(void){
    int limit=1,b; for(b=0;b<N;b++) limit*=2;
    int mask;
    for(mask=0; mask<limit; mask++){
        int m=mask,i;
        for(i=0;i<N;i++){
            int bit=m-(m/2)*2, v=perm[i];
            if(bit==0){ nxt[i]=v-1; nxt[N+i]=N+v-1; }
            if(bit==1){ nxt[i]=N+v-1; nxt[N+i]=v-1; }
            m/=2;
        }
        int r; for(r=0;r<twoN;r++) visited[r]=0;
        int cycles=0,s;
        for(s=0;s<twoN;s++){
            if(!visited[s]){ cycles++; int cur=s; while(!visited[cur]){visited[cur]=1;cur=nxt[cur];} }
        }
        hist[cycles]++;
    }
}
static void heap(int k){
    if(k==1){ process_perm(); return; }
    int i;
    for(i=0;i<k-1;i++){
        heap(k-1);
        if((k-(k/2)*2)==0){int t=perm[i];perm[i]=perm[k-1];perm[k-1]=t;}
        if((k-(k/2)*2)==1){int t=perm[0];perm[0]=perm[k-1];perm[k-1]=t;}
    }
    heap(k-1);
}
int main(int argc,char**argv){
    N = argc>1?atoi(argv[1]):10; twoN=2*N;
    int i; for(i=0;i<N;i++)perm[i]=i+1; for(i=0;i<twoN+1;i++)hist[i]=0;
    heap(N);
    long long total=0; int c; for(c=0;c<twoN+1;c++) total+=hist[c];
    printf("total=%lld\n",total);
    return 0;
}
