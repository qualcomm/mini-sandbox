#include "connect_with_timeout.h"

int connect_with_timeout(int sockfd, struct sockaddr *addr, socklen_t addrlen, int timeout_sec) {

    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    int result = connect(sockfd, addr, addrlen);
    if (result == 0) {

        fcntl(sockfd, F_SETFL, flags); 
        return 0;
    } else if (errno != EINPROGRESS) {

        return -1;
    }
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(sockfd, &writefds);

    struct timeval tv;
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;

    result = select(sockfd + 1, NULL, &writefds, NULL, &tv);
    if (result <= 0) {
        return -1;
    }

    int so_error;
    socklen_t len = sizeof(so_error);
    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);
    if (so_error != 0) {
        errno = so_error;
        return -1;
    }

    fcntl(sockfd, F_SETFL, flags);
    return 0;
}

