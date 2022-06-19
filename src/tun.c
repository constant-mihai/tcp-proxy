#include <sys/ioctl.h>
#include <errno.h>

#include <log/log.h>

#include "tun.h"

int tun_create(char *dev, int flags) {
    struct ifreq ifr;
    int fd, err;
    const char* errstr;

    if( (fd = open("/dev/net/tun", O_RDWR)) < 0 ) {
        errstr = strerror(errno);
        MR_LOG_ERR("error opening tun device");
        mr_log_error(errstr);
        MR_LOG_END();
        return fd;
    }

    memset(&ifr, 0, sizeof(ifr));

    // https://www.kernel.org/doc/Documentation/networking/tuntap.txt
    // Flags:
    // IFF_TUN   - TUN device (no Ethernet headers) 
    // IFF_TAP   - TAP device  
    //
    // IFF_NO_PI - Do not provide packet information

    // https://backreference.org/2010/03/26/tuntap-interface-tutorial/
    // Additionally, another flag IFF_NO_PI can be ORed with the base value.
    // IFF_NO_PI tells the kernel to not provide packet information.
    // The purpose of IFF_NO_PI is to tell the kernel that packets will be "pure"
    // IP packets, with no added bytes.
    // Otherwise (if IFF_NO_PI is unset), 4 extra bytes are added to the beginning of
    // the packet (2 flag bytes and 2 protocol bytes).
    // IFF_NO_PI need not match between interface creation and reconnection time.
    // Also note that when capturing traffic on the interface with Wireshark,
    // those 4 bytes are never shown.
    ifr.ifr_flags = flags;

    if (*dev) {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

    /* try to create the device */
    if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ) {
        errstr = strerror(errno);
        MR_LOG_ERR("error creating tun device");
        mr_log_error(errstr);
        MR_LOG_END();
        close(fd);
        return err;
    }

    strcpy(dev, ifr.ifr_name);

    return fd;
}

void tun_destroy(char *dev) {
    (void)dev;
    //TODO
}
