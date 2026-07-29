#include <poll.h>


struct pollfd {
    int fd;         // the socket descriptor
    short events;   // bitmap of events we're interested in
    short revents;  // on return, bitmap of events that occurred
};

