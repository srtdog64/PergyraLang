#include <stdio.h>
#include <stdlib.h>
static int N, twoN;
static void process_perm(int *perm, long long *hist, int *nxt, int *visited){
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
static void heapsub(int m,int base,int *perm,long long *hist,int *nxt,int *visited){
    if(m==1){ process_perm(perm,hist,nxt,visited); return; }
    int i;
    for(i=0;i<m-1;i++){
        heapsub(m-1,base,perm,hist,nxt,visited);
        if((m-(m/2)*2)==0){int t=perm[base+i];perm[base+i]=perm[base+m-1];perm[base+m-1]=t;}
        if((m-(m/2)*2)==1){int t=perm[base];perm[base]=perm[base+m-1];perm[base+m-1]=t;}
    }
    heapsub(m-1,base,perm,hist,nxt,visited);
}
static long long compute_prefix(int first){
    int perm[20], nxt[40], visited[40]; long long hist[41];
    perm[0]=first; int idx=1,v;
    for(v=1;v<=N;v++) if(v!=first) perm[idx++]=v;
    int c; for(c=0;c<twoN+1;c++) hist[c]=0;
    heapsub(N-1,1,perm,hist,nxt,visited);
    long long cnt=0; for(c=0;c<twoN+1;c++) cnt+=hist[c];
    return cnt;
}
int main(int argc,char**argv){
    N=argc>1?atoi(argv[1]):10; twoN=2*N;
    long long total=0; int first;
    #pragma omp parallel for reduction(+:total) schedule(dynamic)
    for(first=1;first<=N;first++) total += compute_prefix(first);
    printf("total=%lld\n",total);
    return 0;
}
