#include "udp_transport.h"

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <stdio.h>
#include "platform.h"

#ifdef RMW_UXRCE_TRANSPORT_CUSTOM

static int sock_fd = -1;
struct sockaddr_in agent_addr_sender;

bool udp_transport_open(struct uxrCustomTransport* transport) {
    sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd < 0) {
        return false;
    }

    struct sockaddr_in agent_addr;
    memset(&agent_addr, 0, sizeof(agent_addr));
    agent_addr.sin_family = AF_INET;
    agent_addr.sin_port = htons(ROS_UDP_PORT);
    agent_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // inet_aton(ROS_AGENT_IP, &agent_addr.sin_addr);

    // Reinitialize agent_addr as in open()
    memset(&agent_addr_sender, 0, sizeof(agent_addr_sender));
    agent_addr_sender.sin_family = AF_INET;
    agent_addr_sender.sin_port = htons(ROS_UDP_PORT);
    inet_aton(ROS_AGENT_IP, &agent_addr_sender.sin_addr);

    // Bind socket (optional, for receive)
    if (bind(sock_fd, (struct sockaddr*)&agent_addr, sizeof(agent_addr)) < 0) {
        close(sock_fd);
        return false;
    }

    return true;
}

bool udp_transport_close(struct uxrCustomTransport* transport) {
    if (sock_fd >= 0) {
        close(sock_fd);
        sock_fd = -1;
    }
    return true;
}

size_t udp_transport_write(struct uxrCustomTransport* transport, const uint8_t* buf, size_t len, uint8_t* err) {
    if (sock_fd < 0) return 0;

    int sent = sendto(sock_fd, buf, len, 0, (struct sockaddr*)&agent_addr_sender, sizeof(agent_addr_sender));
    if (sent < 0) {
        *err = 1;
        return 0;
    }
    *err = 0;
    return sent;
}

size_t udp_transport_read(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err) {
    if (sock_fd < 0) return 0;

    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int received = recv(sock_fd, buf, len, 0);
    if (received < 0) {
        *err = 1;
        return 0;
    }
    *err = 0;
    return received;
}
#endif
