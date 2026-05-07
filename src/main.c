#include "common.h"

#define ERR_INIT_MUTEX 1
#define ERR_INIT_SOCKET 2
#define ERR_INIT_EPOLL 3
#define ERR_INIT_THREAD 4
#define ERR_SOCKET_BIND 5
#define ERR_SOCKET_LISTEN 6
#define ERR_SOCKET_ACCEPT 7
#define ERR_REALLOC 8

#define MAX_EVENTS 8
#define MAX_FILES 8
#define MAX_WORKERS 8
#define MAX_EPOLL_EVENTS 8
#define CHUNK 8

#define MAX_TABLE_SIZE 256
#define MAX_CLIENTS MAX_TABLE_SIZE / MAX_WORKERS

#define PORT 9999

#define MATCH_SUBFD 0x01
#define MATCH_PUBFD 0x10
#define MATCH_PATH 0x100
#define MATCH_UNLOADED 0x1000
#define MATCH_AVALIABLE 0x10000

#define ACT_WATCH 0x00
#define ACT_QUIT 0x01
#define ACT_NOTIFY 0x02
#define ACT_REPLY 0x03
#define ACT_STATUS 0x04

#define UNSET 0xFF

const int ActionSizes[5] = {
	1,
	0,
	2,
	0,
	0
};

const uint8_t OptionRange[5] = {
	0x01,
	0x01,
	0x05,
	0x05,
	0x05
};

int LastActionIndex = sizeof(ActionSizes) / sizeof(ActionSizes[0]) - 1;

int anyPubMagicValue = -2;
pthread_mutex_t lock;

struct Entry {
    int subFd;
    char *path;

    int pubFd;
    int *activeFiles;
};

struct SharedData {
    struct Entry *subTable;
    int subTableSize;
    pthread_mutex_t subTableLock;
};

struct PrivateData {
    int threadNum;
    pthread_t id;

    struct SharedData *sharedData;

    int epollFd;
    int counterTable[MAX_CLIENTS];

    struct epoll_event socketEvents[MAX_CLIENTS];
    struct epoll_event socketEventQueue[MAX_EVENTS];

    pthread_mutex_t socketEventsLock;
};

struct Message {
    uint8_t action;
    uint8_t options;
    uint8_t size;

    char **data;
    int dataLen;
};

struct SerialResult {
    int size;
    uint8_t reply;
};

void entry_factory(struct Entry *event) {
    event->subFd = 0;
    event->path = NULL;
    event->pubFd = 0;
    event->activeFiles = NULL;
}

void entry_reset(struct Entry *event) {
    if(event->path != NULL)
        free(event->path);

    entry_factory(event);
}

void private_data_factory(struct PrivateData *threadData) {
    threadData->threadNum = -1;
    threadData->epollFd = -1;
    threadData->sharedData = NULL;

    for(int i = 0; i < MAX_CLIENTS; i++) {
        threadData->socketEvents[i].data.fd = 0;
        threadData->counterTable[i] = -1;
    }
}

void init_data_table(int start, int end, struct Entry *event) {
    for(int i = start; i < end; i++)
        entry_factory(&event[i]);
}

int find_table_entry(struct Entry matchEvent, int tableSize, struct Entry *event, uint32_t matchMask, bool clear) {
    uint32_t matched = 0x000000;
    int matchedCounter = -1;

    for(int i = 0; i < tableSize; i++) {
        matched = 0x000000;
        if(matchEvent.subFd == event->subFd)
            matched |= MATCH_SUBFD;

        if(matchEvent.path == NULL)
            if(event[i].path == NULL)
                matched |= MATCH_PATH;
        else
            if(event[i].path != NULL && !strcmp(matchEvent.path, event[i].path))
                matched |= MATCH_PATH;

        if(matchEvent.pubFd == event[i].pubFd || matchEvent.pubFd == anyPubMagicValue)
            matched |= MATCH_PUBFD;

        if(event[i].activeFiles == NULL)
            matched |= MATCH_AVALIABLE;
        else
            if(*event[i].activeFiles < MAX_FILES)
                matched |= MATCH_UNLOADED;
        
        if((matched& matchMask) == matchMask) {
            matchedCounter++;

            if(clear) {
                entry_reset(&event[i]);
                continue;
            }

            entry_factory(&matchEvent);
            return i;
        }
    }

    entry_factory(&matchEvent);
    return matchedCounter;
}

void message_factory(struct Message *msg) {
    msg->action = UNSET;
    msg->options = UNSET;
    msg->size = 0;

    msg->data = NULL;
    msg->dataLen = 0;
}

void message_reset(struct Message *msg) {
    if(msg->data == NULL) {
        message_factory(msg);
        return;
    }

    for(int i = 0; i < msg->dataLen; i++)
        if(msg->data[i] != NULL)
            free(msg->data[i]);

    free(msg->data);
    message_factory(msg);
}

void serialize_result_factory(struct SerialResult *result) {
    result->size = -1;
    result->reply = UNSET;
}

void deserialize(uint8_t buffer[255], struct Message *msg, struct SerialResult *result) {
    int dataLen = -1;
    int dataOffset = 3;
    int *dataSizes = NULL;
    int dataIndex = 0;
    int dataSize = 0;

    serialize_result_factory(result);
    message_reset(msg);

    if(buffer[0] <= LastActionIndex) {
        msg->action = buffer[0];
        dataLen = ActionSizes[msg->action];
    } else {
        result->reply = 0x02;
        return;
    }

    if(buffer[1] > OptionRange[msg->action]) {
        result->reply = 0x03;
        return;
    }

    msg->options = buffer[1];
    if(dataLen == 0) {
        if(buffer[2] != 0) result->reply = 0x02;
        else result->reply = 0x00;
        return;
    }

    msg->size = buffer[2];
    dataSizes = (int*)malloc(sizeof(int)*dataLen);

    for(int i = 3; i < 255; i++) {
        if(i < msg->size + 3) {
            free(dataSizes);
            result->reply = 0x01;
            return;
        }

        if(dataIndex == dataLen) {
            if(dataSize == 1) {
                free(dataSizes);
                result->reply = 0x01;
                return;
            }
            break;
        }

        if(buffer[i] < ' ' || buffer[i] > '~') {
            free(dataSizes);
            result->reply = 0x05;
            return;
        }

        dataSize++;
    }

    msg->data = (char**)malloc(sizeof(char*)*dataLen);
    msg->dataLen = dataLen;

    for(int i = 0; i < dataLen; i++) {
        msg->data[i] = (char*)malloc(sizeof(char) * dataSizes[i]);

        for(int j = 0; j < dataSizes[i]; j++) 
            msg->data[i][j] = buffer[j * dataOffset];

        dataOffset += dataSizes[i];
    }

    result->reply = 0x00;
    result->size = 3 + msg->size;
    free(dataSizes);
}

void* hq_thread(void *threadData) {
    struct PrivateData *data = (struct PrivateData*)threadData;
    uint8_t socketBuffer[255];

    int clientId = -1;
    int bufferReaded = -1;
    int eventsReady = -1;
    int watchIndex = -1;
    int subIndex = -1;

    struct Message readMsg, sendMsg;
    struct SerialResult result;
    struct Entry matchEntry;
    struct SharedData *sharedData = data->sharedData;

    message_factory(&readMsg);
    message_factory(&sendMsg);
    serialize_result_factory(&result);
    entry_factory(&matchEntry);

    while(true) {
        eventsReady = epoll_wait(data->epollFd, data->socketEventQueue, MAX_EPOLL_EVENTS, -1);

        if(eventsReady == -1)
            pthread_exit(NULL);

        for(int i = 0; i < eventsReady; i++) {
            message_reset(&sendMsg);
            message_reset(&readMsg);

            subIndex = -1;
            watchIndex = -1;
            clientId = -1;

            pthread_mutex_lock(&data->socketEventsLock);
            for(int j = 0; j < MAX_CLIENTS; j++)
                if(data->socketEvents[j].data.fd == data->socketEventQueue[i].data.fd) {
                    clientId = j;
                    break;
                }
            if(clientId == -1) {
                pthread_mutex_unlock(&data->socketEventsLock);
                break;
            }

            bufferReaded = read(data->socketEventQueue[i].data.fd, socketBuffer, sizeof(socketBuffer));
            if(bufferReaded == 0 || bufferReaded == -1) {
                subIndex = data->socketEventQueue[i].data.fd;
                epoll_ctl(data->epollFd, EPOLL_CTL_DEL, data->socketEvents[clientId].data.fd, &data->socketEvents[clientId]);

                close(data->socketEvents[clientId].data.fd);
                data->socketEventQueue[clientId].data.fd = 0;

                bzero(&data->socketEvents[clientId], sizeof(struct epoll_event));
                pthread_mutex_unlock(&data->socketEventsLock);

                continue;
            }

            pthread_mutex_unlock(&data->socketEventsLock);

            deserialize(socketBuffer, &readMsg, &result);
            if(result.reply != 0x00)
                continue;

            switch (readMsg.action)
            {   
            case 0x04:
                if(readMsg.options == 0x05)
                    if(data->counterTable[clientId] != -1)
                        continue;

                pthread_mutex_lock(&sharedData->subTableLock);
                matchEntry.path = NULL;
                matchEntry.pubFd = 0;
                watchIndex = find_table_entry(matchEntry, sharedData->subTableSize, sharedData->subTable, MATCH_PUBFD | MATCH_PATH, false);

                if(watchIndex == -1) {
                    pthread_mutex_unlock(&sharedData->subTableLock);
                    continue;
                }
                data->counterTable[clientId] = 0;

                break;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int clientId = -1;
    int threadId = -1;

    int socketFd = -1;
    int clientFd = -1;

    struct sockaddr_in serverAddress, clientAddress;
    socklen_t addressSize = sizeof(struct sockaddr_in);

    struct SharedData sharedData;
    struct PrivateData threadWorkTable[MAX_CLIENTS];

    sharedData.subTable = (struct Entry*)malloc(sizeof(struct Entry) * CHUNK);
    sharedData.subTableSize = CHUNK;
    init_data_table(0, CHUNK, sharedData.subTable);

    if(pthread_mutex_init(&lock, NULL) || pthread_mutex_init(&sharedData.subTableLock, NULL))
        exit(ERR_INIT_MUTEX);

    if((socketFd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        exit(ERR_INIT_SOCKET);

    bzero(&serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(PORT);

    if(bind(socketFd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1)
        exit(ERR_SOCKET_BIND);

    for(int i = 0; i < MAX_WORKERS; i++) {
        private_data_factory(&threadWorkTable[i]);
        threadWorkTable[i].epollFd = epoll_create1(0);

        if(threadWorkTable[i].epollFd == -1)
            exit(ERR_INIT_EPOLL);

        threadWorkTable[i].sharedData = &sharedData;
        threadWorkTable[i].threadNum = i;

        if(pthread_mutex_init(&threadWorkTable[i].socketEventsLock, NULL) != 0)
            exit(ERR_INIT_MUTEX);

        if(pthread_create(&threadWorkTable[i].id, NULL, hq_thread, &threadWorkTable[i]) != 0)
            exit(ERR_INIT_THREAD);
    }

    if(listen(socketFd, 1))
        exit(ERR_SOCKET_LISTEN);

    while(true) {
        if((clientFd = accept(socketFd, (struct sockaddr*)& clientAddress, (socklen_t*)& addressSize)) == -1)
            exit(ERR_SOCKET_ACCEPT);

        threadId = -1;
        clientId = -1;

        for(int i = 0; i < MAX_WORKERS; i++) {
            pthread_mutex_lock(&threadWorkTable[i].socketEventsLock);
            for(int j = 0; j < MAX_CLIENTS; j++) {
                if(threadWorkTable[i].socketEvents[j].data.fd == 0) {
                    clientId = j;
                    threadId = j;
                    break;
                }
            }

            if(clientId != -1)
                break;
            pthread_mutex_unlock(&threadWorkTable[i].socketEventsLock);
        }

        if(threadId == -1) {
            close(clientFd);
            continue;
        }

        threadWorkTable[threadId].socketEvents[clientId].events = EPOLLIN;
        threadWorkTable[threadId].socketEvents[clientId].data.fd = clientFd;

        if(epoll_ctl(threadWorkTable[threadId].epollFd, EPOLL_CTL_ADD, clientFd, &threadWorkTable[threadId].socketEvents[clientId]) != 0) {
            pthread_mutex_unlock(&threadWorkTable[threadId].socketEventsLock);
            close(clientFd);
            continue;
        }

        pthread_mutex_unlock(&threadWorkTable[threadId].socketEventsLock);
    }
    return 0;
}
