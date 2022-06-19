#include "tun_test.h"
#include <assert.h>

void test_tun() {
    char tun_name[IFNAMSIZ];
    tun_name[0] = '\0';
    int fd = tun_create(tun_name, IFF_TUN | IFF_NO_PI);
    assert(fd > 0);
}
