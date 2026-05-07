#include "common.h"
#include "http_parser.h"
#include "arena.h"
#include "structs.h"

int create_and_bind_socket() {
    int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if(socketFd == -1) return -1;

    int option = 1;
    setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    setsockopt(socketFd, SOL_SOCKET, SO_REUSEPORT, &option, sizeof(option));

    struct sockaddr_in serverAddr;

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if(bind(socketFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        close(socketFd);
        return -1;
    }

    int flags = fcntl(socketFd, F_GETFL, 0);
    fcntl(socketFd, F_SETFL, flags | O_NONBLOCK);

    listen(socketFd, SOMAXCONN);
    printf("Servidor escutando na porta %d...\n", PORT);
    return socketFd;
}

void* worker_thread(void *arg) {
    int listen = *(int*)arg;
    printf("Thread iniciada monitorando FD: %d\n", listen);
    int epoll = epoll_create1(0);
    struct epoll_event event, events[MAX_EVENTS];
    struct Request request;

    if(epoll == -1) return NULL;

    event.data.fd = listen;
    event.events = EPOLLIN | EPOLLET;
    if(epoll_ctl(epoll, EPOLL_CTL_ADD, listen, &event) == -1) return NULL;

    request.arena = init_arena(1024 * 1024 * 10);

    while(true) {
        int poll = epoll_wait(epoll, events, MAX_EVENTS, -1);
        for(int i = 0; i < poll; i++) {
            if(events[i].data.fd == listen) {
                while(true) {
                    struct sockaddr sock;
                    socklen_t sock_len = sizeof(sock);
                    int sockFd = accept4(listen, &sock, &sock_len, SOCK_NONBLOCK);
                    if(sockFd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        perror("Erro no accept4");
                        break;
                    }

                    printf("Nova conexão aceita! FD: %d\n", sockFd);
                    event.data.fd = sockFd;
                    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
                    epoll_ctl(epoll, EPOLL_CTL_ADD, sockFd, &event);
                }
            }
            else if(events[i].events& EPOLLIN) {
                arena_memory_reset(request.arena);
                size_t bytes = recv(events[i].data.fd, request.arena->arena_buffer, request.arena->arena_size, 0);

                if(bytes > 0) {
                    request.method = http_method(request.arena->arena_buffer);
                    printf("Estou aqui: %d", request.method);
                    routes_manager(request, events[i].data.fd);
                }
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
                close(events[i].data.fd);
        }
    }
}

int main(int argc, char *argv[]) {
    int listenFd = create_and_bind_socket();
    if(listenFd == -1) exit(EXIT_FAILURE);

    pthread_t workers[2];
    for(int i = 0; i < 2; i++) 
        pthread_create(&workers[i], NULL, worker_thread, &listenFd);

    for(int i = 0; i < 2; i++)
        pthread_join(workers[i], NULL);
}
