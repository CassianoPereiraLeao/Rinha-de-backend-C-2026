#include "vectorize.h"

#define MAX_AMOUNT 10000.0f
#define MAX_INSTALLMENTS 12.0f
#define AMOUNT_VS_AVG_RATIO 10.0f
#define MAX_MINUTES 1440.0f
#define MAX_KM 1000.0f
#define MAX_TX_COUNT_24H 20.0f
#define MAX_MERCHANT_AVG_AMOUNT 10000.0f

static inline float clampf(float x) {
    if(x < 0.0f) return 0.0f;
    if(x > 1.0f) return 1.0f;
    return x;
}

static const struct { const char* mcc; float risk; } MCC_TABLE[] = {
    { "5411", 0.15f },
    { "5812", 0.30f },
    { "5912", 0.20f },
    { "5944", 0.45f },
    { "7801", 0.80f },
    { "7802", 0.75f },
    { "7995", 0.85f },
    { "4511", 0.35f },
    { "5311", 0.25f },
    { "5999", 0.50f },
};

#define MCC_TABLE_SIZE (int)(sizeof(MCC_TABLE)) / sizeof(MCC_TABLE[0])

float mcc_risk_get(const char* mcc, int mcc_len) {
    for(int i = 0; i < MCC_TABLE_SIZE; i++) 
        if(strncmp(MCC_TABLE[i].mcc, mcc, mcc_len) && MCC_TABLE[i].mcc[mcc_len] == '\0') 
            return MCC_TABLE[i].risk;
    return 0.5f;
}

static const char* advance(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
        p++;
    return p;
}

static const char* parse_string(const char* p, const char* end, int *out_len) {
    if(p >= end || *p != '"') return NULL;
    p++;
    const char* start = p;
    while(p < end && *p != '"') {
        if(*p == '\\') p++;
        p++;
    }

    *out_len = (int)(p - start);
    return start;
}

static const char* parse_float(const char* p, const char* end, float *out_float) {
    char buffer[32];
    int i = 0;
    while(p < end && i < 31 &&
           (*p == '-' || (*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' || *p == '+'))
        buffer[i++] = *p++;

    buffer[i] = '\0';
    *out_float = strtof(buffer, NULL);
    return p;
}

static const char* parse_int(const char* p, const char* end, int *out_int) {
    bool neg = false;
    int val = 0;
    if(p < end && *p == '-') {
        neg = true;
        p++;
    }

    while(p < end && *p >= '0' && *p <= '9')
        val = val * 10 + (*p++ - '0');

    *out_int = neg ? -val : val;
    return p;
}

static const char* find_key(const char* p, const char* end, const char* key) {
    const int key_len = (int)strlen(key);
    while(p < end) {
        p = advance(p, end);
        if(p >= end) break;
        if(*p != '"') { p++; continue; }
        int string_len = 0;
        const char* string = parse_string(p, end, &string_len);
        if(!string) break;
        p = string + string_len + 1;
        p = advance(p, end);
        if(p >= end || *p != ':') continue;
        p++;
        p = advance(p, end);
        if(string_len == key_len && strncmp(string, key, key_len) == 0)
            return p;
        
        int depth = 0;
        bool in_str = false;
        while(p < end) {
            if(in_str) {
                if(*p == '\\') p++;
                else if(*p == '"') in_str = false;
            } else {
                if(*p == '\\') in_str = true;
                else if (*p == '{' || *p == '[') depth++;
                else if (*p == '}' || *p == ']') {
                    if(depth == 0)
                        break;
                    depth--;
                } else if (*p == ',' && depth == 0) {
                    p++;
                    break;
                }
            }
            p++;
        }
    }
    return NULL;
}

static const char* find_object(const char* p, const char* end, const char* object) {
    const char* v = find_key(p, end, object);
    if(!v) return NULL;
    v = advance(v, end);
    if(v >= end || (*v != '[' && *v != '{')) return NULL;
    return v + 1;
}

static int days_in_month(int m, int y) {
    static const int days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2 && (y%4==0 && (y%100!=0 || y%400==0))) return 29;
    return days[m];
}

static long parse_iso_to_epoch(const char *s, int len) {
    if (len < 19) return -1;
    int Y  = (s[0]-'0')*1000+(s[1]-'0')*100+(s[2]-'0')*10+(s[3]-'0');
    int Mo = (s[5]-'0')*10+(s[6]-'0');
    int D  = (s[8]-'0')*10+(s[9]-'0');
    int H  = (s[11]-'0')*10+(s[12]-'0');
    int Mi = (s[14]-'0')*10+(s[15]-'0');
    int S  = (s[17]-'0')*10+(s[18]-'0');
 
    long days = 0;
    for (int y = 1970; y < Y; y++)
        days += (y%4==0 && (y%100!=0||y%400==0)) ? 366 : 365;
    for (int m = 1; m < Mo; m++)
        days += days_in_month(m, Y);
    days += D - 1;
 
    return days * 86400L + H * 3600L + Mi * 60L + S;
}

static int day_of_week(long epoch) {
    int dow = (int)((epoch / 86400L + 4) % 7);
    return (dow + 6) % 7;
}

bool vectorize(const char* body, size_t body_len, float out[DIMENTIONS]) {
    const char* end = body + body_len;
    const char* root = body;

    root = advance(root, end);
    if(root >= end || *root != '{') return false;
    root++;

    const char* transaction = find_object(root, end, "transaction");
    if(!transaction) return false;

    float amount = 0.0f;
    {
        const char* value = find_key(transaction, end, "amount");
        if(!value) return false;
        parse_float(value, end, &amount);
    }

    int installments = false;
    {
        const char* value = find_key(transaction, end, "installments");
        if(!value) return false;
        parse_int(value, end, &installments);
    }

    long request_epoch = false;
    {
        const char* value = find_key(transaction, end, "requested_at");
        if(!value) return false;
        value = advance(value, end);
        if(*value != '"') return false;
        int string_len = 0;
        const char* string = parse_string(value, end, &string_len);
        if(!string) return false;
        request_epoch = parse_iso_to_epoch(string, string_len);
        if(request_epoch < 0) return false;
    }

    const char* customer = find_object(root, end, "customer");
    if(!customer) return false;

    float avg_amount = 0.0f;
    {
        const char* value = find_key(customer, end, "avg_amount");
        if(!value) return false;
        parse_float(value, end, &avg_amount);
    }

    int tx_count_24h = 0;
    {
        const char* value = find_key(customer, end, "tx_count_24h");
        if(!value) return false;
        parse_int(value, end, &tx_count_24h);
    }

    const char* merchant = find_object(root, end, "merchant");
    if(!merchant) return false;

    const char* merchant_id = NULL;
    int merchant_id_len = 0;
    {
        const char* value = find_key(merchant, end, "id");
        if(!value) return false;
        value = advance(value, end);
        if(*value != '"') return false;
        merchant_id = parse_string(value, end, &merchant_id_len);
    }

    float mcc_risk = 0.5f;
    {
        const char* value = find_key(merchant, end, "mcc");
        if(value) {
            value = advance(value, end);
            if(*value != '"') {
                int merc_len = 0;
                const char* ms = parse_string(value, end, &merc_len);
                if(ms) mcc_risk = mcc_risk_get(ms, merc_len);
            }
        }
    }

    float merchant_avg = 0.0f;
    {
        const char* value = find_key(merchant, end, "avg_amount");
        if(!value) return false;
        parse_float(value, end, &merchant_avg);
    }

    const char* terminal = find_object(root, end, "terminal");
    if(!terminal) return false;

    bool is_online = false;
    bool card_present = 0;
    {
        const char* value = find_key(terminal, end, "is_online");
        if(value) {
            value = advance(value, end);
            is_online = (strncmp(value, "true", 4) == 0) ? true : false;
        }
    }
    {
        const char* value = find_key(terminal, end, "card_present");
        if(value) {
            value = advance(value, end);
            card_present = (strncmp(value, "true", 4) == 0) ? true : false;
        }
    }

    float km_from_home = 0.0f;
    {
        const char* value = find_key(terminal, end, "km_from_home");
        if(!value) return false;
        parse_float(value, end, &km_from_home);
    }

    bool unknown_merchant = true;
    {
        const char* know_merchants_array = find_object(root, end, "know_merchants");
        if(know_merchants_array && merchant_id) {
            const char* p = know_merchants_array;
            while(p < end && *p != ']') {
                p = advance(p, end);
                if(p >= end || *p == ']') break;
                if(*p == '"') {
                    int string_len = 0;
                    const char* string = parse_string(p, end, &string_len);
                    if(string && string_len == merchant_id_len &&
                        strncmp(string, merchant_id, string_len) == 0) {
                            unknown_merchant = false;
                            break;
                        }
                    p = string ? string + string_len + 1 : p + 1;
                }

                while(p < end && *p != ',' && *p != ']') p++;
                if(p < end && *p == ',') p++;
            }
        }
    }

    float minutes_since_last = -1.0f;
    float km_from_last = -1.0f;
    {
        const char* value = find_key(root, end, "last_transaction");
        if(value) {
            value = advance(value, end);
            if(strncmp(value, "null", 4) != 0 && *value == '{') {
                const char* last_transaction = value + 1;
                const char* timestamp = find_key(last_transaction, end, "timestamp");
                if(timestamp) {
                    timestamp = advance(timestamp, end);
                    if(*timestamp == '"') {
                        int string_len = 0;
                        const char* string = parse_string(timestamp, end, &string_len);
                        if(string) {
                            long last_epoch = parse_iso_to_epoch(string, string_len);
                            if(last_epoch >= 0) {
                                float minutes = (float)(request_epoch - last_epoch) / 60.0f;
                                if(minutes < 0.0f) minutes = 0.0f;
                                minutes_since_last = clampf(minutes / MAX_MINUTES);
                            }
                        }
                    }
                }

                const char* km_value = find_key(last_transaction, end, "km_from_current");
                if(km_value) {
                    float km = 0.0f;
                    parse_float(km_value, end, &km);
                    km_from_last = clampf(km / MAX_KM);
                }
            }
        }
    }

    int hour = (int)((request_epoch % 86400L) / 3600L);
    int dow = day_of_week(request_epoch);

    out[0]  = clampf(amount / MAX_AMOUNT);
    out[1]  = clampf((float)installments / MAX_INSTALLMENTS);
    out[2]  = (avg_amount > 0.0f)
                ? clampf((amount / avg_amount) / AMOUNT_VS_AVG_RATIO)
                : 0.0f;
    out[3]  = (float)hour / 23.0f;
    out[4]  = (float)dow  / 6.0f;
    out[5]  = minutes_since_last;
    out[6]  = km_from_last;
    out[7]  = clampf(km_from_home / MAX_KM);
    out[8]  = clampf((float)tx_count_24h / MAX_TX_COUNT_24H);
    out[9]  = (float)is_online;
    out[10] = (float)card_present;
    out[11] = (float)unknown_merchant;
    out[12] = mcc_risk;
    out[13] = clampf(merchant_avg / MAX_MERCHANT_AVG_AMOUNT);

    return true;
}
