#include <uxr/client/transport.h>

#include <rmw_microxrcedds_c/config.h>
#include <cstddef>

bool udp_transport_open(struct uxrCustomTransport* transport);
bool udp_transport_close(struct uxrCustomTransport* transport);
size_t udp_transport_write(struct uxrCustomTransport* transport, const uint8_t* buf, size_t len, uint8_t* err);
size_t udp_transport_read(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err);
