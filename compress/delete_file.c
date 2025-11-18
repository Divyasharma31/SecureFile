#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

void trim(char *s) {
    if (!s) return;
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    s[n] = '\0';
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <virtual_disk_meta> <file_name>\n", argv[0]);
        return 1;
    }

    const char *metaFile = argv[1];
    const char *targetFile = argv[2];

    FILE *meta = fopen(metaFile, "r");
    if (!meta) {
        perror("Error opening metadata file");
        return 1;
    }

    FILE *tempMeta = fopen("temp_meta.txt", "w");
    if (!tempMeta) {
        perror("Error creating temp metadata");
        fclose(meta);
        return 1;
    }

    char line[512];
    int found = 0;
    char targetHash[128] = {0};

    while (fgets(line, sizeof(line), meta)) {
        trim(line);
        if (strstr(line, targetFile)) {
            found = 1;
            char hash[128];
            if (sscanf(line, "%*[^|]| Compressed Size: %*d bytes | Offset: %*d | Compressed: yes | Hash: %s", hash) == 1) {
                strcpy(targetHash, hash);
            }
            printf("Deleting metadata entry for '%s'\n", targetFile);
            continue; 
        }
        fprintf(tempMeta, "%s\n", line);
    }

    fclose(meta);
    fclose(tempMeta);

    if (!found) {
        printf("File '%s' not found in metadata.\n", targetFile);
        remove("temp_meta.txt");
        return 1;
    }
    remove(metaFile);
    rename("temp_meta.txt", metaFile);


    FILE *hashFile = fopen("hash_index.txt", "r");
    if (!hashFile) {
        perror("Error opening hash_index.txt");
        return 1;
    }

    FILE *tempHash = fopen("temp_hash.txt", "w");
    if (!tempHash) {
        perror("Error creating temp hash index");
        fclose(hashFile);
        return 1;
    }

    while (fgets(line, sizeof(line), hashFile)) {
        char hash[128];
        long long offset, size;
        int refCount;

        if (sscanf(line, "%64s %lld %lld %d", hash, &offset, &size, &refCount) == 4) {
            if (strcmp(hash, targetHash) == 0) {
                refCount--;
                if (refCount <= 0) {
                    printf("Ref count = 0 → block free (offset %lld, size %lld)\n", offset, size);
                    continue;
                } else {
                    printf("Decremented ref count for hash %s → %d\n", hash, refCount);
                }
            }
            fprintf(tempHash, "%s %lld %lld %d\n", hash, offset, size, refCount);
        } else {
            fprintf(tempHash, "%s", line); 
        }
    }

    fclose(hashFile);
    fclose(tempHash);
    remove("hash_index.txt");
    rename("temp_hash.txt", "hash_index.txt");

    printf("File '%s' deleted successfully.\n", targetFile);
    return 0;
}