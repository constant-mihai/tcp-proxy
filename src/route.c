#include <asm/types.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#include <log/log.h>
#include <utils/utils.h>

#include "route.h"

// https://www.infradead.org/~tgr/libnl/doc/core.html
// https://www.linuxjournal.com/article/8498
// https://olegkutkov.me/2019/03/24/getting-linux-routing-table-using-netlink/
// https://github.com/OpenVPN/openvpn/blob/63ba6b27ceb5972947175624a6a6f4a83cf67d8e/src/openvpn/networking_sitnl.c
// man rtnetlink
// man 7 rtnetlink
// man netlink
// man 7 netlink

typedef struct route_request {
    struct nlmsghdr nlh;  /* Netlink header */
    struct rtmsg rtm;     /* Payload - route message */
} route_request_t;

static int _route_open_netlink() {
    struct sockaddr_nl saddr;

    /* Open raw socket for the NETLINK_ROUTE protocol */
    int nl_sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

    if (fcntl(nl_sock,
              F_SETFD,
              fcntl(nl_sock, F_GETFD, 0) | O_NONBLOCK) == -1) {
        const char* strerr = strerror(errno);
        MR_LOG_ERR("error setting fd options");
        mr_log_error(strerr);
        MR_LOG_END();
        return -1;
    }

    if (nl_sock < 0) {
        const char* strerr = strerror(errno);
        MR_LOG_ERR("Failed to open netlink socket");
        mr_log_error(strerr);
        MR_LOG_END();
        return -1;
    }

    memset(&saddr, 0, sizeof(saddr));

    saddr.nl_family = AF_NETLINK;
    // nl_pid is the unicast address of netlink socket.  It's always 0 if the des‐
    // tination is in the kernel.  For a user-space process, nl_pid is usually the
    // PID of the process owning the destination socket.  However, nl_pid  identi‐
    // fies  a  netlink  socket, not a process.  If a process owns several netlink
    // sockets, then nl_pid can be equal to the process ID only for  at  most  one
    // socket.   There  are two ways to assign nl_pid to a netlink socket.  If the
    // application sets nl_pid before calling bind(2), then it is up to the appli‐
    // cation  to  make sure that nl_pid is unique.  If the application sets it to
    // 0, the kernel takes care of assigning it.  The kernel assigns  the  process
    // ID  to  the  first  netlink  socket  the process opens and assigns a unique
    // nl_pid to every netlink socket that the process subsequently creates.
    saddr.nl_pid = getpid();

    /* Bind current process to the netlink socket */
    if (bind(nl_sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        const char* strerr = strerror(errno);
        MR_LOG_ERR("Failed to bind to netlink socket");
        mr_log_error(strerr);
        MR_LOG_END();
        close(nl_sock);
        return -1;
    }

    return nl_sock;
}

static int _route_send_request(int nl_sock) {
    route_request_t req;
    struct sockaddr_nl nladdr;
    struct msghdr msg;
    struct iovec iov;

    // initialize the request buffer
    bzero(&req, sizeof(req));

    // set the NETLINK header
    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    //TODO : Although in theory a netlink message can be up to 4GiB in size.
    // The socket buffers are very likely not large enough to hold message of such sizes.
    // Therefore it is common to limit messages to one page size (PAGE_SIZE)
    // and use the multipart mechanism to split large pieces of data into several messages.
    // A multipart message has the flag NLM_F_MULTI set and the receiver is expected to
    // continue receiving and parsing until the special message type NLMSG_DONE is received.
    //
    // Multipart messages unlike fragmented ip packets must not be reassmbled even
    // though it is perfectly legal to do so if the protocols wishes to work this way.
    // Often multipart message are used to send lists or trees of objects were
    // each multipart message simply carries multiple objects allow for each message
    // to be parsed independently.
    req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    req.nlh.nlmsg_type = RTM_GETROUTE;
    //TODO: Netlink differs between requests, notifications, and replies.
    // Requests are messages which have the NLM_F_REQUEST flag set and are
    // meant to request an action from the receiver.
    // A request is typically sent from a userspace process to the kernel.
    // While not strictly enforced, requests should carry a sequence number 
    // incremented for each request sent.
    //req.nlh.nlmsg_seq = seq_num;

    // set the routing message header
    req.rtm.rtm_family = AF_INET;
    req.rtm.rtm_table = RT_TABLE_MAIN;

    // create the remote address to communicate
    bzero(&nladdr, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;

    // initialize & create the struct msghdr supplied
    // to the sendmsg() function
    bzero(&msg, sizeof(msg));
    msg.msg_name = (void *) &nladdr;
    msg.msg_namelen = sizeof(nladdr);

    // place the pointer & size of the RTNETLINK
    // message in the struct msghdr
    iov.iov_base = (void *) &req.nlh;
    iov.iov_len = req.nlh.nlmsg_len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1; // a list of requests can also be sent

    // send the RTNETLINK message to kernel
    if (sendmsg(nl_sock, &msg, 0) < 0) {
        const char* strerr = strerror(errno);
        MR_LOG_ERR("Failed to send request.");
        mr_log_error(strerr);
        MR_LOG_END();
        return -1;
    }

    return 0;
}

static int _route_alloc_read_buf(int fd, struct msghdr *msg) {
    struct iovec *iov = msg->msg_iov;
    char *buf;
    int len;

    iov->iov_base = NULL;
    iov->iov_len = 0;

    // get msg length
    while (1) {
        if ((len = recvmsg(fd, msg, MSG_PEEK | MSG_TRUNC)) < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                const char* strerr = strerror(errno);
                MR_LOG_ERR("error on recvmsg");
                mr_log_error(strerr);
                MR_LOG_END();
                return -1;
            }
        }
        break;
    }

    MALLOC(buf, len, char);

    iov->iov_base = (void*)buf;
    iov->iov_len = len;

    return len;
}

static int _route_receive(int fd, struct msghdr *msg) {
    int len;

    while (1) {
        if ((len = recvmsg(fd, msg, 0)) < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else {
                const char* strerr = strerror(errno);
                MR_LOG_ERR("error on recvmsg");
                mr_log_error(strerr);
                MR_LOG_END();

                return -1;
            }
        }
        break;
    }

    return len;
}

int _route_read_response(int nl_sock, struct msghdr *msg) {
    int len = 0;

    len = _route_receive(nl_sock, msg);
    if (len < 0) {
        MR_LOG_ERR("error on receiving route message");
        MR_LOG_END();
        return -1;
    }

    return len;
}

void _route_parse_nl_attr(struct rtmsg* r, int nlmsg_len, struct rtattr **attributes) {
    struct rtattr *rta = RTM_RTA(r);
    int table, len = nlmsg_len - NLMSG_LENGTH(sizeof(*r));

    table = r->rtm_table;
    (void) table; //TODO what is this used for?

    while (RTA_OK(rta, len))
    {
        attributes[rta->rta_type] = rta;
        rta = RTA_NEXT(rta, len);
    }
}

void _route_print_attributes(struct rtmsg* r, struct rtattr **attributes) {
    int if_idx = 0, n = 0;
    //TODO some of these buffers are too large.
    char buf[5012], dst[256], gw[256], if_name[IF_NAMESIZE], src[256];

    if (attributes[RTA_DST]) {
        inet_ntop(r->rtm_family,
                  RTA_DATA(attributes[RTA_DST]),
                  dst,
                  sizeof(dst));
        n += sprintf(buf+n, "%s/%u", dst, r->rtm_dst_len);
    } else if(r->rtm_dst_len) { //TODO: i don't get this part
        sprintf(buf, "0/%u ", r->rtm_dst_len);
    } else {
        n += sprintf(buf+n, "default ");
    }

    if (attributes[RTA_GATEWAY]) {
        inet_ntop(r->rtm_family,
                  RTA_DATA(attributes[RTA_GATEWAY]),
                  gw,
                  sizeof(gw));
        n += sprintf(buf+n, "via %s", gw);
    }

    if (attributes[RTA_OIF]) {
        if_idx = *(uint32_t*)RTA_DATA(attributes[RTA_OIF]);
        if_indextoname(if_idx, if_name);
        n += sprintf(buf+n, " dev %s", if_name);
    }

    //TODO: src is not printed, why?
    if (attributes[RTA_SRC]) {
        inet_ntop(r->rtm_family,
                  RTA_DATA(attributes[RTA_SRC]),
                  src,
                  sizeof(src));
        n += sprintf(buf+n, " src %s", src);
    }

    sprintf(buf+n, "\n");

    MR_LOG_INFO("Main routing table IPv4");
    mr_log_string("table", buf);
    MR_LOG_END();
}

void route_print(struct nlmsghdr *nh, int msglen) {
    struct rtattr* attributes[RTA_MAX+1];
    memset(attributes, 0, sizeof(struct rtattr *) * (RTA_MAX + 1));

    MR_LOG_INFO("Main routing table IPv4");
    MR_LOG_END();

    /* Iterate through all messages in buffer */
    while (NLMSG_OK(nh, msglen)) {
        if (nh->nlmsg_type == NLMSG_DONE) {
            return;
        }

        if (nh->nlmsg_flags & NLM_F_DUMP_INTR) {
            MR_LOG_ERR("Dump was interrupted\n");
            MR_LOG_END();
        }

        // TODO, not sure why this is here, can anyone else send to this socket?
        //struct sockaddr_nl *nladdr = (struct sockaddr_nl*) msg->msg_name;
        //if (nladdr->nl_pid != 0) { //TODO: why? when can it be 0?
        //    continue;
        //}

        // TODO
        // Netlink is not a reliable protocol.  It tries its best to deliver a message
        // to  its  destination(s), but may drop messages when an out-of-memory condi‐
        // tion or other error occurs.  For reliable transfer the sender  can  request
        // an acknowledgement from the receiver by setting the NLM_F_ACK flag.  An ac‐
        // knowledgment is an NLMSG_ERROR packet with the error field set to  0.   The
        // application  must  generate  acknowledgements for received messages itself.
        // The kernel tries to send an NLMSG_ERROR message for every failed packet.  A
        // user process should follow this convention too.
        if (nh->nlmsg_type == NLMSG_ERROR) {
            // TODO: Error messages should set the sequence number to the sequence
            // number of the request which caused the error.
            // TODO 2: the infradead.org article says the payload is the original
            // netlink message header.
            MR_LOG_ERR("netlink reported error; error handling not implemented");
            MR_LOG_END();
        }

        struct rtmsg* r = NLMSG_DATA(nh);
        _route_parse_nl_attr(r, nh->nlmsg_len, attributes);
        _route_print_attributes(r, attributes);

        nh = NLMSG_NEXT(nh, msglen);
    }
}

int route_get() {
    int nl_sock = 0, msglen = 0;
    struct sockaddr_nl nladdr;
    struct iovec iov;
    struct msghdr msg = {
        .msg_name = &nladdr,
        .msg_namelen = sizeof(nladdr),
        .msg_iov = &iov,
        .msg_iovlen = 1,
    };

    if ((nl_sock = _route_open_netlink()) < 0) {
        MR_LOG_ERR("error opening netlink socket");
        MR_LOG_END();
        return -1;
    }

    if (_route_send_request(nl_sock) != 0) {
        MR_LOG_ERR("send request failed");
        MR_LOG_END();
        return -1;
    }

    if ((msglen = _route_alloc_read_buf(nl_sock, &msg)) < 0) {
        MR_LOG_ERR("error on allocating buf for route message");
        MR_LOG_END();
        return -1;
    }

    if((msglen = _route_read_response(nl_sock, &msg)) < 0) {
        MR_LOG_ERR("read response failed");
        MR_LOG_END();
        free(msg.msg_iov->iov_base);
        return -1;
    }

    struct nlmsghdr *nh = (struct nlmsghdr *)msg.msg_iov->iov_base;
    //TODO, this is only for testing, should be removed
    route_print(nh, msglen);

    free(msg.msg_iov->iov_base);
    return 0;
}
