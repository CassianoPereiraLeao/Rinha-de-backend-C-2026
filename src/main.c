#include "common.h"
#include "http_parser.h"
#include "structs.h"

const uint32_t event_masks = EPOLLIN | EPOLLRDHUP;
const uint32_t event_err_masks = EPOLLERR | EPOLLHUP;

static Request* free_requests = NULL;

int create_and_bind_socket() {
    int socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(socketFd == -1) return -1;

    int option = 1;
    setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    char* UNIX_PATH = getenv("UNIX_SOCKET_PATH");
    if(!UNIX_PATH) {
        printf("Error Bind");
        return -1;
    }

    struct stat st;
    if(stat(UNIX_PATH, &st) == 0) {
        if(S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Erro: %s é um diretório, não um socket\n", UNIX_PATH);
            return -1;
        }
        unlink(UNIX_PATH);
    }

    size_t UNIX_PATH_LEN = strlen(UNIX_PATH);

    struct sockaddr_un serverAddr = { .sun_family = AF_UNIX };
    memcpy(serverAddr.sun_path, UNIX_PATH, UNIX_PATH_LEN);
    serverAddr.sun_path[UNIX_PATH_LEN] = '\0';

    if(bind(socketFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        close(socketFd);
        return -1;
    }

    chmod(UNIX_PATH, 0777);

    int flags = fcntl(socketFd, F_GETFL, 0);
    fcntl(socketFd, F_SETFL, flags | O_NONBLOCK);
    listen(socketFd, SOMAXCONN);
    return socketFd;
}

static bool requests(Request* request) {
    while(request->write_len == request->write_pos) {
        request->write_len = 0;
        request->write_pos = 0;

        if(request->read_len < 4) return true;
        char* end = NULL;
        for(size_t i = 0; i + 3 < request->read_len; i++) {
            if(request->buffer_read[i] == '\r' && request->buffer_read[i + 1] == '\n' &&
                request->buffer_read[i + 2] == '\r' && request->buffer_read[i + 3] == '\n') {
                end = request->buffer_read + i + 4;
                break;
            }
        }
        if(end == NULL) return true;

        const size_t len = (size_t)(end - request->buffer_read);
        size_t content_len = 0;
        const char* p = request->buffer_read;
        const char* header_end = end;
        while(p < header_end) {
            if(strncasecmp(p, "content-length:", 15) == 0) {
                p += 15;
                while(p < header_end && (*p == ' ' || *p == '\t')) p++;
                while(p < header_end && *p >= '0' && *p <= '9') {
                    content_len = content_len * 10 + (size_t)(*p - '0');
                    p++;
                }
                break;
            }
            while(p < header_end && *p != '\n') p++;
            if(p < header_end) p++;
        }
        if(request->read_len < len + content_len) return true;

        routes_manager(request, end, content_len);

        const size_t consumed = len + content_len;
        const size_t remaining = request->read_len - consumed;
        if(remaining > 0) 
            memmove(request->buffer_read, request->buffer_read + consumed, remaining);
        request->read_len = remaining;

        if(request->write_len > request->write_pos) return true;
    }
    return true;
}

static bool read_bytes(Request *request) {
    while(1) {
        if(request->read_len == BYTES_READ_CAPACITY) return false;

        const ssize_t bytes = read(request->fd,
                                    request->buffer_read + request->read_len,
                                    BYTES_READ_CAPACITY - request->read_len);
        if(bytes > 0) {
            request->read_len += bytes;
            continue;
        }
        if(bytes == 0) return false;
        if(errno == EAGAIN || errno == EWOULDBLOCK) break;
        if(errno == EINTR) continue;
        return false;
    }
    return requests(request);
};

static bool write_bytes(int epollFd, Request* request) {
    while(request->write_pos < request->write_len) {
        const ssize_t bytes = send(request->fd, 
                                    request->write_ptr + request->write_pos,
                                    request->write_len - request->write_pos,
                                    MSG_NOSIGNAL);
        if(bytes > 0) {
            request->write_pos += bytes;
            continue;
        }
        if(bytes < 0 && errno == EAGAIN) return true;
        if(bytes < 0 && errno == EINTR) continue;
        return false;
    }
    request->write_pos = 0;
    request->write_len = 0;
    request->write_ptr = NULL;
    return requests(request);
}

static bool update_events(int epollFd, Request* request) {
    uint32_t wanted = event_masks;
    if(request->write_pos < request->write_len)
        wanted |= EPOLLOUT;
    if(wanted == request->epoll_events) 
        return true;

    struct epoll_event event;
    event.data.ptr = request;
    event.events = wanted;
    request->epoll_events = wanted;
    return epoll_ctl(epollFd, EPOLL_CTL_MOD, request->fd, &event) == 0;
}

static Request* allocate_request(void) {
    Request* request = free_requests;
    if(request != NULL) {
        free_requests = request->next;
    } else {
        request = (Request*)malloc(sizeof(Request));
        if(request == NULL) return NULL;
    }
    request->fd = -1;
    request->read_len = 0;
    request->write_pos = 0;
    request->write_len = 0;
    request->write_ptr = NULL;
    request->epoll_events = event_masks;
    request->next = NULL;
    return request;
}

static void free_request(int fd, Request *request) {
    if(request == NULL) return;
    epoll_ctl(fd, EPOLL_CTL_DEL, request->fd, NULL);
    close(request->fd);
    request->fd = -1;
    request->read_len = 0;
    request->write_pos = 0;
    request->write_len = 0;
    request->write_ptr = NULL;
    request->next = free_requests;
    free_requests = request;
}

static void connection_accepter(int epollFd, int listenFd) {
    while(1) {
        const int socketFd = accept4(listenFd, NULL, NULL, SOCK_NONBLOCK);
        if(socketFd >= 0) {
            Request* request = allocate_request();
            if(request == NULL) {
                close(socketFd);
                continue;
            }
            request->fd = socketFd;
            request->epoll_events = event_masks;

            struct epoll_event event;
            memset(&event, 0, sizeof(event));
            event.events = event_masks;
            event.data.ptr = request;

            if(epoll_ctl(epollFd, EPOLL_CTL_ADD, socketFd, &event) != 0) {
                close(socketFd);
                request->fd = -1;
                request->next = free_requests;
                free_requests = request;
            }
            continue;
        }
        if(errno == EAGAIN || errno == EWOULDBLOCK) return;
        if(errno == EINTR) continue;
    }
}
static void* worker_thread(void* arg) {
    int listen = (intptr_t)arg;
    const int epollFd = epoll_create1(EPOLL_CLOEXEC);
    if(epollFd < 0) return NULL;

    struct epoll_event event, events[MAX_EVENTS];
    event.data.ptr = NULL;
    event.events = EPOLLIN;
    if(epoll_ctl(epollFd, EPOLL_CTL_ADD, listen, &event) == -1) {
        close(epollFd);
        return NULL;
    }

    while(1) {
        const int queue = epoll_wait(epollFd, events, MAX_EVENTS, -1);
        if(queue == -1) {
            if(errno == EINTR) continue;
            close(epollFd);
            return NULL;
        }

        for(int i = 0; i < queue; ++i) {
            if(events[i].data.ptr == NULL) {
                connection_accepter(epollFd, listen);
                continue;
            }

            Request* request = (Request*)events[i].data.ptr;
            bool connection = true;

            if((events[i].events& event_err_masks) != 0)
                connection = false;
            if(connection && (events[i].events & EPOLLIN))
                connection = read_bytes(request);
            if(connection && request->write_pos < request->write_len)
                connection = write_bytes(epollFd, request);
            else if(connection && (events[i].events & EPOLLOUT))
                connection = write_bytes(epollFd, request);
            if(connection)
                connection = update_events(epollFd, request);
            if(!connection)
                free_request(epollFd, request);
        }
    }
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);

    const char* refs_path = getenv("REFERENCES_BIN_PATH");
    const char* labels_path = getenv("LABELS_BIN_PATH");
    if (!refs_path   || refs_path[0]   == '\0') refs_path   = "resources/references.bin";
    if (!labels_path || labels_path[0] == '\0') labels_path = "resources/labels.bin";

    if (refs_load(refs_path, labels_path) != 0) {
        fprintf(stderr, "Falha ao carregar dataset\n");
        return 1;
    }

    int listenFd = create_and_bind_socket();
    if(listenFd == -1) exit(EXIT_FAILURE);

    worker_thread((void*)(intptr_t)listenFd);

    return 0;
}