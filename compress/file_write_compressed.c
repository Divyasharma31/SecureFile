#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <openssl/sha.h>

/* ===== Pretty CLI Colors ===== */
#define GREEN   "\033[32m"
#define RED     "\033[31m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"
#define RESET   "\033[0m"

#define ALPHABET 256
#define MAX_TREE_NODES 512

typedef struct Node {
    unsigned char ch;
    int freq;
    int min_sym;              
    struct Node *left, *right;
} Node;

typedef struct Code {
    unsigned char ch;
    int len;                  
    unsigned char bits[32];   
} Code;

/* Min-heap for Huffman with deterministic order (freq, min_sym) */
typedef struct { int size; Node **arr; } Heap;

static void sha256_hex(const unsigned char *data, size_t n, char out[65]) {
    unsigned char h[SHA256_DIGEST_LENGTH];
    SHA256(data, n, h);
    for (int i=0;i<SHA256_DIGEST_LENGTH;i++) sprintf(out+2*i, "%02x", h[i]);
    out[64]='\0';
}

static int hash_lookup(const char *idx, const char *hex, long long *off, long long *sz){
    FILE *f=fopen(idx,"r"); if(!f) return 0;
    char hx[65]; long long o,s;
    while (fscanf(f,"%64s %lld %lld",hx,&o,&s)==3){
        if(strcmp(hx,hex)==0){ *off=o; *sz=s; fclose(f); return 1; }
    }
    fclose(f); return 0;
}

static void hash_append(const char *idx, const char *hex, long long off, long long sz){
    FILE *f=fopen(idx,"a"); if(!f){ perror("hash_index"); exit(1); }
    fprintf(f,"%s %lld %lld\n", hex, off, sz);
    fclose(f);
}

static Node* new_node(unsigned char ch,int freq,Node*l,Node*r){
    Node* n=(Node*)malloc(sizeof(Node));
    n->ch=ch; n->freq=freq; n->left=l; n->right=r;
    if(!l && !r) n->min_sym=ch;
    else {
        int lm=l?l->min_sym:255, rm=r?r->min_sym:255;
        n->min_sym = (lm<rm?lm:rm);
    }
    return n;
}

static void swap(Node**a,Node**b){ Node*t=*a; *a=*b; *b=t; }
static int less(Node*A,Node*B){ if(A->freq!=B->freq) return A->freq<B->freq; return A->min_sym<B->min_sym; }
static void heap_down(Heap*h,int i){
    for(;;){
        int l=2*i+1, r=2*i+2, s=i;
        if(l<h->size && less(h->arr[l],h->arr[s])) s=l;
        if(r<h->size && less(h->arr[r],h->arr[s])) s=r;
        if(s==i) break; swap(&h->arr[s],&h->arr[i]); i=s;
    }
}
static void heap_up(Heap*h,int i){
    while(i>0){ int p=(i-1)/2; if(less(h->arr[i],h->arr[p])){ swap(&h->arr[i],&h->arr[p]); i=p; } else break; }
}
static void hpush(Heap*h,Node*n){ h->arr[h->size++]=n; heap_up(h,h->size-1); }
static Node* hpop(Heap*h){ Node*top=h->arr[0]; h->arr[0]=h->arr[--h->size]; heap_down(h,0); return top; }

static Node* build_huffman(const int freq[ALPHABET]){
    int cnt=0; for(int i=0;i<ALPHABET;i++) if(freq[i]) cnt++;
    if(cnt==0) return NULL;
    Heap h; h.size=0; h.arr=(Node**)malloc(sizeof(Node*)*(2*cnt+8));
    for(int i=0;i<ALPHABET;i++) if(freq[i]) hpush(&h, new_node((unsigned char)i,freq[i],NULL,NULL));
    if(h.size==1){ Node*leaf=h.arr[0]; Node*root=new_node(0,leaf->freq,leaf,NULL); free(h.arr); return root; }
    while(h.size>1){
        Node*a=hpop(&h), *b=hpop(&h);
        Node*m=new_node(0,a->freq+b->freq,a,b);
        hpush(&h,m);
    }
    Node*root=hpop(&h); free(h.arr); return root;
}

static void emit_code(Node* n, unsigned path, int depth, Code table[ALPHABET], int *count){
    if(!n) return;
    if(!n->left && !n->right){
        Code *c = &table[n->ch];
        c->ch = n->ch;
        c->len = depth?depth:1; 
        memset(c->bits,0,sizeof(c->bits));
        for(int i=0;i<c->len;i++){
            int bit = (path >> (c->len-1-i)) & 1;
            c->bits[i>>3] |= bit << (7 - (i&7));
        }
        (*count)++;
        return;
    }
    emit_code(n->left,  (path<<1)|0, depth+1, table, count);
    emit_code(n->right, (path<<1)|1, depth+1, table, count);
}

static void free_tree(Node* r){ if(!r) return; free_tree(r->left); free_tree(r->right); free(r); }


typedef struct {
    char magic[4];
    uint64_t original_size;
    uint64_t code_count;
    uint64_t payload_bit_count;
} __attribute__((packed)) HCHeader;

int main(int argc, char** argv){
    if(argc!=4){
        fprintf(stderr, RED "Usage: %s <virtual_disk_meta> <storage_pool> <file_name>\n" RESET, argv[0]);
        return 1;
    }
    const char* meta=argv[1], *storage=argv[2], *input_path = argv[3];

    printf(BLUE "\n Starting compression\n" RESET);
    printf(CYAN "→ Input: %s\n" RESET, input_path);

    FILE* in=fopen(input_path,"rb"); 
    if(!in){ perror(RED "input" RESET); return 1; }
    fseek(in,0,SEEK_END); long long insz=ftell(in); rewind(in);
    unsigned char* data=(unsigned char*)malloc(insz?insz:1);
    if(!data){ fprintf(stderr, RED "OOM\n" RESET); fclose(in); return 1; }
    if(insz) fread(data,1,insz,in);
    fclose(in);

   
    char hex[65]; sha256_hex(data,(size_t)insz,hex);
    printf(GREEN "SHA256: %s\n" RESET, hex);

    long long dupe_off=0, dupe_sz=0;
    if (hash_lookup("hash_index.txt", hex, &dupe_off, &dupe_sz)){
        printf(YELLOW "Duplicate detected — reusing existing blob at offset %lld (%lld bytes)\n" RESET, dupe_off, dupe_sz);
        FILE* m=fopen(meta,"a"); if(!m){perror("meta"); free(data); return 1;}
        fprintf(m,"%s | Compressed Size: %lld bytes | Offset: %lld | Compressed: yes | Duplicate: yes | Hash: %s\n",
                input_path, dupe_sz, dupe_off, hex);
        fclose(m);
        printf(GREEN "Metadata updated. No rewrite needed.\n\n" RESET);
        free(data);
        return 0;
    }

    /* Build frequency & tree */
    int freq[ALPHABET]={0};
    for(long long i=0;i<insz;i++) freq[data[i]]++;
    Node* root = build_huffman(freq);

    /* Build code table */
    Code table[ALPHABET]; 
    for(int i=0;i<ALPHABET;i++){ table[i].ch=(unsigned char)i; table[i].len=0; memset(table[i].bits,0,sizeof(table[i].bits)); }
    int code_count=0;
    if (root) emit_code(root, 0, 0, table, &code_count);

    /* Compute payload bit count */
    uint64_t payload_bits=0;
    for(int i=0;i<ALPHABET;i++)
        if(freq[i] && table[i].len>0) payload_bits += (uint64_t)freq[i] * (uint64_t)table[i].len;

    if (code_count==1 && payload_bits==0 && insz>0){
        /* single-symbol file safety */
        int sym=0; while(sym<ALPHABET && !freq[sym]) sym++;
        table[sym].len=1; table[sym].bits[0]=0; 
        code_count=1; payload_bits = (uint64_t)insz * 1ULL;
    }

    /* Open storage and to write offset*/
    FILE* st=fopen(storage,"rb+"); 
    if(!st){ perror(RED " storage" RESET); free_tree(root); free(data); return 1; }
    fseek(st,0,SEEK_END); long long offset=ftell(st);

    /* Write header */
    HCHeader hdr; memcpy(hdr.magic,"HC1",4);
    hdr.original_size=(uint64_t)insz; 
    hdr.code_count=(uint64_t)code_count; 
    hdr.payload_bit_count=payload_bits;
    fwrite(&hdr,1,sizeof(hdr),st);

    /* Write codebook */
    for(int i=0;i<ALPHABET;i++){
        if(freq[i] && table[i].len>0){
            uint8_t sym=(uint8_t)i; uint16_t clen=(uint16_t)table[i].len;
            fwrite(&sym,1,1,st);
            fwrite(&clen,1,2,st);
            int cb=(clen+7)/8;
            fwrite(table[i].bits,1,cb,st);
        }
    }

    /* Write payload */
    unsigned char outb=0; int bitfill=0; long long payload_bytes=0;
    for(long long i=0;i<insz;i++){
        Code *c = &table[data[i]];
        for(int b=0;b<c->len;b++){
            int bit = (c->bits[b>>3] >> (7-(b&7))) & 1;
            outb = (outb<<1) | bit;
            bitfill++;
            if(bitfill==8){ fwrite(&outb,1,1,st); payload_bytes++; outb=0; bitfill=0; }
        }
    }
    if(bitfill>0){ outb <<= (8-bitfill); fwrite(&outb,1,1,st); payload_bytes++; }

    long long total_blob = (long long)sizeof(hdr);
    for(int i=0;i<ALPHABET;i++) if(freq[i] && table[i].len>0)
        total_blob += 1 + 2 + (long long)((table[i].len+7)/8);
    total_blob += payload_bytes;
    fclose(st);

    /* Index + metadata */
    hash_append("hash_index.txt", hex, offset, total_blob);
    FILE* m=fopen(meta,"a"); if(!m){perror("meta"); free_tree(root); free(data); return 1;}
    fprintf(m,"%s | Compressed Size: %lld bytes | Offset: %lld | Compressed: yes | Duplicate: no | Hash: %s\n",
            input_path, total_blob, offset, hex);
    fclose(m);

    printf(GREEN "Compression complete\n" RESET);
    printf(CYAN  "→ Wrote blob @ offset %lld (size %lld bytes)\n\n" RESET, offset, total_blob);

    free_tree(root);
    free(data);
    return 0;
}