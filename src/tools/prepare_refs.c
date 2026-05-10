#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <zlib.h>

#define REFS_COUNT 100000
#define DIMENTIONS 14
#define BUFFER_SIZE (1024 * 1024 * 16) // Nota para interessados (1024 * 1024) == 1 MB

static float dimentions[DIMENTIONS][REFS_COUNT];
static uint8_t labels[REFS_COUNT];

static char* g_buffer = NULL;
static size_t g_len = 0;

static int descompress_gz(const char* path) {
    gzFile f = gzopen(path, "rb");
    if(!f) {
        fprintf(stderr, "Arquivo falhou: %s\n", path);
        return -1;
    }

    g_buffer = malloc(BUFFER_SIZE);
    int bytes;
    while((bytes = gzread(f, g_buffer + g_len, BUFFER_SIZE - (int)g_len)) > 0)
        g_len += bytes;
    gzclose(f);
    if(bytes < 0) {
        fprintf(stderr, "Erro ao ler gz\n"); 
        return -1;
    }
    return 0;
}

static const char* skip_ws(const char* p, const char* end) {
    while(p < end && (*p==' '||*p=='\t'||*p=='\r'||*p=='\n')) p++;
    return p;
}

static const char* parse_float_value(const char* p, const char* end, float* out_value) {
    char buffer[30];
    int i = 0;
    while(p < end & i < 31 && (*p=='-'||(*p>='0'&&*p<='9')||*p=='.'||*p=='e'||*p=='E'||*p=='+'))
        buffer[i++] = *p++;
    buffer[i] = '\0';
    *out_value = strtof(buffer, NULL);
    return p;
}

static int parse_all(int expected) {
    const char* p = g_buffer;
    const char* end = g_buffer + g_len;

    p = skip_ws(p, end);
    if(p >= end || *p != '[') {
        fprintf(stderr, "Expected: '['\n");
        return -1;
    }
    p++;

    int index = 0;
    while(index < expected) {
        p = skip_ws(p, end);
        if(p >= end) break;
        if(*p == ']') break;
        if(*p == ',') { p++; continue; }
        if(*p != '{') { p++; continue; }
        p++;

        float vector[DIMENTIONS];
        int vec_ok = 0;
        int label_val = -1;

        for(int field = 0; field < 2; field++) {
            p = skip_ws(p, end);
            if(p >= end || *p != '"') break;
            p++;

            const char* key = p;
            while(p < end && *p != '"') p++;
            int key_len = (int)(p - key);
            p++;

            p = skip_ws(p, end);
            if(p >= end || *p != ":") break;
            p++;
            p = skip_ws(p, end);

            if(key_len == 6 && strncmp(key, "vector", 6) == 0) {
                if(p >= end || *p != '[') break;
                p++;

                for(int dimention = 0; dimention < DIMENTIONS; dimention++) {
                    p = skip_ws(p, end);
                    p = parse_float_value(p, end, &vector[dimention]);
                    p = skip_ws(p, end);
                    if(p < end && *p != ',') p++;
                }

                p = skip_ws(p, end);
                if(p < end && *p != ']') p++;
                vec_ok = 1;
            } else if(key_len == 5 && strncmp(key, "label", 5) == 0) {
                if(p >= end || *p != '"') break;
                p++;
                if(strncmp(p, "fraud", 5) == 0) label_val = 1;
                else label_val = 0;
                while(p < end && *p != '"') p++;
                p++;
            }

            p = skip_ws(p, end);
            if(p < end && *p != '"') p++;
        }

        p = skip_ws(p, end);
        if(p < end && *p == '}') p++;

        if(vec_ok && label_val >= 0) {
            for(int dimention = 0; dimention < DIMENTIONS; dimention++) 
                dimentions[dimention][index] = vector[dimention];
            labels[index] = (uint8_t)label_val;
            index++;
        }

        if(index % 10000 == 0)
            fprintf(stderr, "  parsed %d/%d\n", index, expected);
    }

    fprintf(stderr, "Total parseado: %d vetores\n", index);
    return index;
}

int main(int argc, char *argv[]) {
    if(argc < 4){
        fprintf(stderr, "Uso: %s <references.json.gz> <references.bin> <labels.bin>\n", argv[0]);
        return 1;
    }

    if(descompress_gz(argv[1]) != 0) return 1;

    const int count = parse_all(REFS_COUNT);
    if(count != REFS_COUNT)
        fprintf(stderr, "AVISO: esperava %d vetores, parseou %d\n", REFS_COUNT, count);

    FILE* f = fopen(argv[2], "wb");
    if(!f) {
        perror("fopen refs.bin"); 
        return 1;
    }

    fwrite(dimentions, sizeof(float), DIMENTIONS * REFS_COUNT, f);
    fclose(f);
    f = fopen(argv[3], "wb");
    if(!f) {
        perror("Fopen label.bin");
        return 1;
    }

    fwrite(labels, 1, REFS_COUNT, f);
    fclose(f);
    free(g_buffer);
    return 0;
}
