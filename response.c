#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

// GERADOR DE HEXADECIMAL PARA DIMINUIR LATÊNCIA DE RESPOSTA AO PEGAR A KEY DO JSON

uint32_t hash_fb(const char *s, int len) {
    uint32_t hash = 2166136261U;
    for (int i = 0; i < len; i++) {
        hash ^= (uint8_t)s[i];
        hash *= 16777619U;
    }
    return hash;
}

int main() {
    char *campos[] = {"transaction", "amount", "installments", "requested_at", "id",
    "customer", "avg_amount", "tx_count_24h", "known_merchants", "merchant", "mcc", "avg_amount",
    "terminal", "is_online", "card_present", "km_from_home", "last_transaction", "timestamp", "km_from_current"};
    for(int i = 0; i < 19; i++) {
        char new_char[32];
        for(int j = 0; j < strlen(campos[i]); j++) 
            new_char[j] = toupper(campos[i][j]);
        new_char[strlen(campos[i])] = '\0';
        printf("#define HASH_%s 0x%X\n", new_char, hash_fb(campos[i], strlen(campos[i])));
    }

    return 0;
}
