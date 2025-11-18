#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

/* ===== CLI Colors ===== */
#define GREEN   "\033[32m"
#define RED     "\033[31m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"
#define RESET   "\033[0m"

typedef struct Node {
    unsigned char ch;
    struct Node *left, *right;
} Node;

typedef struct {
    char magic[4];
    uint64_t original_size;
    uint64_t code_count;
    uint64_t payload_bit_count;
} __attribute__((packed)) HCHeader;

static void trim(char *s){
    if(!s) return;
    char *p=s; while(*p && isspace((unsigned char)*p)) p++;
    if(p!=s) memmove(s,p,strlen(p)+1);
    size_t n=strlen(s);
    while(n>0 && isspace((unsigned char)s[n-1])) n--;
    s[n]='\0';
}

static int parse_meta(const char* meta, const char* name, long long* size, long long* off){
    FILE* f=fopen(meta,"r"); if(!f){ perror("meta"); return 0; }
    char line[1024]; int ok=0;
    while(fgets(line,sizeof(line),f)){
        trim(line);
        char nm[512]; long long s,o;
        if (sscanf(line,"%511[^|]| Compressed Size: %lld bytes | Offset: %lld", nm, &s, &o)==3){
            size_t L=strlen(nm); while(L && (nm[L-1]==' '||nm[L-1]=='\t')) nm[--L]='\0';
            if(strcmp(nm,name)==0){ *size=s; *off=o; ok=1; break; }
        }
    }
    fclose(f); return ok;
}

static Node* new_leaf(unsigned char ch){ Node* n=(Node*)malloc(sizeof(Node)); n->ch=ch; n->left=n->right=NULL; return n; }
static Node* new_int(){ Node* n=(Node*)malloc(sizeof(Node)); n->ch=0; n->left=n->right=NULL; return n; }

static void insert_code(Node* root, unsigned char ch, const unsigned char *bits, int len){
    Node* cur=root;
    for(int i=0;i<len;i++){
        int bit = (bits[i>>3] >> (7-(i&7))) & 1;
        if(bit==0){ if(!cur->left)  cur->left  = new_int(); cur=cur->left; }
        else       { if(!cur->right) cur->right = new_int(); cur=cur->right; }
    }
    cur->ch=ch; cur->left=NULL; cur->right=NULL;
}

static void free_tree(Node* r){ if(!r) return; free_tree(r->left); free_tree(r->right); free(r); }

int main(int argc, char** argv){
    if(argc!=4){
        fprintf(stderr, RED "Usage: %s <virtual_disk_meta> <storage_pool> <file_name>\n" RESET, argv[0]);
        return 1;
    }
    const char* meta=argv[1], *storage=argv[2], *name=argv[3];

    printf(BLUE "\n Starting decompression\n" RESET);
    printf(CYAN "→ Looking up '%s' in metadata\n" RESET, name);

    long long blob_size=0, offset=0;
    if(!parse_meta(meta,name,&blob_size,&offset)){
        fprintf(stderr, RED " File '%s' not found in metadata.\n" RESET, name);
        return 1;
    }

    FILE* st=fopen(storage,"rb");
    if(!st){ perror(RED " storage" RESET); return 1; }
    if(fseek(st, offset, SEEK_SET)!=0){ perror(RED " fseek" RESET); fclose(st); return 1; }

    /* Read header */
    HCHeader hdr;
    if (fread(&hdr,1,sizeof(hdr),st)!=sizeof(hdr)){ fprintf(stderr, RED "Failed to read header\n" RESET); fclose(st); return 1; }
    if (memcmp(hdr.magic,"HC1",3)!=0){ fprintf(stderr, RED "Bad magic. Not an HC1 blob.\n" RESET); fclose(st); return 1; }

    printf(GREEN " Header OK  " RESET "(orig=%llu, codes=%llu, bits=%llu)\n",
           (unsigned long long)hdr.original_size,
           (unsigned long long)hdr.code_count,
           (unsigned long long)hdr.payload_bit_count);

    Node* root = new_int();
    for(uint64_t i=0;i<hdr.code_count;i++){
        uint8_t sym; uint16_t clen;
        if(fread(&sym,1,1,st)!=1){ fprintf(stderr, RED "Codebook read error (sym)\n" RESET); free_tree(root); fclose(st); return 1; }
        if(fread(&clen,1,2,st)!=2){ fprintf(stderr, RED "Codebook read error (len)\n" RESET); free_tree(root); fclose(st); return 1; }
        int cb=(clen+7)/8;
        if(cb>32){ fprintf(stderr, RED " Code too long\n" RESET); free_tree(root); fclose(st); return 1; }
        unsigned char tmp[32]; memset(tmp,0,sizeof(tmp));
        if((int)fread(tmp,1,cb,st)!=cb){ fprintf(stderr, RED " Codebook read error (bits)\n" RESET); free_tree(root); fclose(st); return 1; }
        insert_code(root, sym, tmp, (int)clen);
    }

    /* Read payload */
    uint64_t pbytes = (hdr.payload_bit_count + 7) / 8;
    unsigned char *payload=(unsigned char*)malloc((size_t)pbytes);
    if(!payload){ fprintf(stderr, RED " OOM\n" RESET); free_tree(root); fclose(st); return 1; }
    if (fread(payload,1,(size_t)pbytes,st)!=pbytes){ fprintf(stderr, RED " Payload read error\n" RESET); free(payload); free_tree(root); fclose(st); return 1; }
    fclose(st);

    /* Decode */
    char outname[600]; snprintf(outname,sizeof(outname),"recovered_%s", name);
    FILE* out=fopen(outname,"wb"); if(!out){ perror(RED " output" RESET); free(payload); free_tree(root); return 1; }

    Node* cur=root; uint64_t written=0;
    for(uint64_t i=0;i<hdr.payload_bit_count && written<hdr.original_size;i++){
        int bit = (payload[i>>3] >> (7 - (i & 7))) & 1;
        cur = bit ? cur->right : cur->left;
        if(cur && !cur->left && !cur->right){
            fputc(cur->ch,out);
            written++;
            cur=root;
        }
    }
    fclose(out);

    free(payload);
    free_tree(root);

    printf(GREEN "Decompression complete\n" RESET);
    printf(CYAN  "→ Wrote %s (%llu bytes)\n\n" RESET, outname, (unsigned long long)written);
    return 0;
}