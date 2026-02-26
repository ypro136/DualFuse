#include <linked_list.h>
#include <liballoc.h>
#include <poll.h>
#include <syscalls.h>
#include <task.h>
#include <unixSocket.h>
#include <utility.h>

#include <lwip/sockets.h>

LLcontrol dsUnixSocket; // struct UnixSocket
Spinlock  LOCK_LL_UNIX_SOCKET;

// AF_UNIX socket implementation (still needs a lot of testing)

OpenFile *unixSocketAcceptCreate(UnixSocketPair *dir) {
  size_t sockFd = fsUserOpen(currentTask, "/dev/null", O_RDWR, 0);
  assert(!RET_IS_ERR(sockFd));

  OpenFile *sockNode = fsUserGetNode(currentTask, sockFd);
  assert(sockNode);

  sockNode->dir = dir;
  sockNode->handlers = &unixAcceptHandlers;

  return sockNode;
}

bool unixSocketAcceptDuplicate(OpenFile *original, OpenFile *orphan) {
  orphan->dir = original->dir;
  UnixSocketPair *pair = original->dir;
  spinlock_acquire(&pair->LOCK_PAIR);
  pair->serverFds++;
  spinlock_release(&pair->LOCK_PAIR);

  return true;
}

UnixSocketPair *unixSocketAllocatePair() {
  UnixSocketPair *pair = calloc(sizeof(UnixSocketPair), 1);
  pair->clientBuffSize = UNIX_SOCK_BUFF_DEFAULT;
  pair->serverBuffSize = UNIX_SOCK_BUFF_DEFAULT;
  CircularAllocate(&pair->serverBuff, pair->serverBuffSize);
  CircularAllocate(&pair->clientBuff, pair->clientBuffSize);
  return pair;
}

void unixSocketFreePair(UnixSocketPair *pair) {
  assert(pair->serverFds == 0 && pair->clientFds == 0);
  CircularFree(&pair->clientBuff);
  CircularFree(&pair->serverBuff);
  free(pair->filename);
  free(pair);
}

bool unixSocketAcceptClose(OpenFile *fd) {
  UnixSocketPair *pair = fd->dir;
  spinlock_acquire(&pair->LOCK_PAIR);
  pair->serverFds--;

  bool notify = pair->serverFds == 0;

  if (pair->serverFds == 0 && pair->clientFds == 0)
    unixSocketFreePair(pair);
  else
    spinlock_release(&pair->LOCK_PAIR);

  if (notify)
    pollInstanceRing((size_t)pair, EPOLLHUP);

  return true;
}

size_t unixSocketAcceptRecvfrom(OpenFile *fd, uint8_t *out, size_t limit,
                                int flags, sockaddr_linux *addr,
                                uint32_t *len) {
  // useless unless SOCK_DGRAM
  (void)addr;
  (void)len;

  UnixSocketPair *pair = fd->dir;
  if (!pair->clientFds && CircularReadPoll(&pair->serverBuff) == 0)
    return 0;
  while (true) {
    spinlock_acquire(&pair->LOCK_PAIR);
    if (!pair->clientFds && CircularReadPoll(&pair->serverBuff) == 0) {
      spinlock_release(&pair->LOCK_PAIR);
      return 0;
    } else if ((fd->flags & O_NONBLOCK || flags & MSG_DONTWAIT) &&
               CircularReadPoll(&pair->serverBuff) == 0) {
      spinlock_release(&pair->LOCK_PAIR);
      return ERR(EWOULDBLOCK);
    } else if (CircularReadPoll(&pair->serverBuff) > 0)
      break;
    spinlock_release(&pair->LOCK_PAIR);
  }

  // spinlock already acquired
  size_t toCopy = MIN(limit, CircularReadPoll(&pair->serverBuff));
  assert(CircularRead(&pair->serverBuff, out, toCopy) == toCopy);
  bool notify = CircularWritePoll(&pair->serverBuff) > UNIX_SOCK_POLL_EXTRA;
  spinlock_release(&pair->LOCK_PAIR);
  if (notify)
    pollInstanceRing((size_t)pair, EPOLLOUT);

  return toCopy;
}

size_t unixSocketAcceptSendto(OpenFile *fd, uint8_t *in, size_t limit,
                              int flags, sockaddr_linux *addr, uint32_t len) {
  // useless unless SOCK_DGRAM
  (void)addr;
  (void)len;

  UnixSocketPair *pair = fd->dir;
  if (limit > pair->clientBuffSize) {
    printf("[socket] Warning! Truncating limit{%ld} to clientBuffSize{%ld}\n",
           limit, pair->clientBuffSize);
    limit = pair->clientBuffSize;
  }

  while (true) {
    spinlock_acquire(&pair->LOCK_PAIR);
    if (!pair->clientFds) {
      spinlock_release(&pair->LOCK_PAIR);
      atomicBitmapSet(&currentTask->sigPendingList, SIGPIPE);
      return ERR(EPIPE);
    } else if ((fd->flags & O_NONBLOCK || flags & MSG_DONTWAIT) &&
               CircularWritePoll(&pair->clientBuff) < limit) {
      spinlock_release(&pair->LOCK_PAIR);
      return ERR(EWOULDBLOCK);
    } else if (CircularWritePoll(&pair->clientBuff) >= limit)
      break;
    spinlock_release(&pair->LOCK_PAIR);
  }

  // spinlock already acquired
  assert(CircularWrite(&pair->clientBuff, in, limit) == limit);
  spinlock_release(&pair->LOCK_PAIR);
  pollInstanceRing((size_t)pair, EPOLLIN);

  return limit;
}

int unixSocketAcceptInternalPoll(OpenFile *fd, int events) {
  UnixSocketPair *pair = fd->dir;
  int             revents = 0;

  spinlock_acquire(&pair->LOCK_PAIR);
  if (!pair->clientFds)
    revents |= EPOLLHUP;

  if (events & EPOLLOUT && pair->clientFds &&
      CircularWritePoll(&pair->clientBuff) > 0)
    revents |= EPOLLOUT;

  if (events & EPOLLIN && CircularReadPoll(&pair->serverBuff) > 0)
    revents |= EPOLLIN;
  spinlock_release(&pair->LOCK_PAIR);

  return revents;
}

size_t unixSocketAcceptReportKey(OpenFile *fd) { return (size_t)fd->dir; }

size_t unixSocketAcceptGetpeername(OpenFile *fd, sockaddr_linux *addr,
                                   uint32_t *len) {
  UnixSocketPair *pair = fd->dir;
  if (!pair)
    return ERR(ENOTCONN);

  int actualLen = sizeof(addr->sa_family) + strlen(pair->filename);
  int toCopy = MIN(*len, actualLen);
  if (toCopy < sizeof(addr->sa_family)) // you're POOR!
    return ERR(EINVAL);
  addr->sa_family = AF_UNIX;
  memcpy(addr->sa_data, pair->filename, toCopy - sizeof(addr->sa_family));
  *len = toCopy;
  return 0;
}

size_t unixSocketOpen(void *taskPtr, int type, int protocol) {
  // rest are not supported yet, only SOCK_STREAM
  // todo: stuff like SOCK_SEQPACKET are still passing!
  if (!(type & 1)) {
    dbgSysStubf("unsupported type{%x}", type);
    return ERR(ENOSYS);
  }

  Task  *task = (Task *)taskPtr;
  size_t sockFd = fsUserOpen(task, "/dev/null", O_RDWR, 0);
  assert(!RET_IS_ERR(sockFd));

  OpenFile *sockNode = fsUserGetNode(task, sockFd);
  assert(sockNode);

  sockNode->handlers = &unixSocketHandlers;
  spinlock_acquire(&LOCK_LL_UNIX_SOCKET);
  UnixSocket *unixSocket =
      LinkedListAllocate(&dsUnixSocket, sizeof(UnixSocket));
  spinlock_release(&LOCK_LL_UNIX_SOCKET);
  sockNode->dir = unixSocket;

  unixSocket->timesOpened = 1;

  if (type | SOCK_CLOEXEC)
    sockNode->closeOnExec = true;
  // if (type | SOCK_NONBLOCK)
  //   sockNode->flags |= O_NONBLOCK;

  return sockFd;
}

char *unixSocketAddrSafe(sockaddr_linux *addr, size_t len) {
  if (addr->sa_family != AF_UNIX)
    return (void *)ERR(EAFNOSUPPORT);

  size_t addrLen = len - sizeof(addr->sa_family);
  if (addrLen <= 0)
    return (void *)ERR(EINVAL);

  char *unsafe = calloc(addrLen + 1, 1);     // ENSURE there's a null char
  bool  abstract = addr->sa_data[0] == '\0'; // todo: not all sockets!
  int   skip = abstract ? 1 : 0;
  memcpy(unsafe, &addr->sa_data[skip], addrLen - skip);
  spinlock_acquire(&currentTask->infoFs->LOCK_FS);
  char *safe = fsSanitize(currentTask->infoFs->cwd, unsafe);
  spinlock_release(&currentTask->infoFs->LOCK_FS);
  free(unsafe);

  return safe;
}

typedef struct {
  size_t len;
  char  *filename;
} UnlinkNotifyCb;
bool unixSocketUnlinkNotifyCb(void *data, void *ctx) {
  UnixSocket     *browse = (UnixSocket *)data;
  UnlinkNotifyCb *p = (UnlinkNotifyCb *)ctx;
  spinlock_acquire(&browse->LOCK_SOCK);
  if (browse->bindAddr && strlen(browse->bindAddr) == p->len &&
      memcmp(browse->bindAddr, p->filename, p->len) == 0)
    return true;
  spinlock_release(&browse->LOCK_SOCK);
  return false;
}

bool unixSocketUnlinkNotify(char *filename) {
     UnlinkNotifyCb args = {.len = strlen(filename), .filename = filename};
  spinlock_acquire(&LOCK_LL_UNIX_SOCKET);
  UnixSocket *browse = (UnixSocket *)LinkedListSearch(&dsUnixSocket, unixSocketUnlinkNotifyCb, &args);
  spinlock_release(&LOCK_LL_UNIX_SOCKET);

  if (browse) {
    // spinlock already here!
    browse->bindAddr = 0;
    spinlock_release(&browse->LOCK_SOCK);
  }

  return !!browse;
}

typedef struct {
  char  *safe;
  size_t safeLen;
} SocketBindArgs;
bool unixSocketBindCb(void *data, void *ctx) {
  UnixSocket     *browse = data;
  SocketBindArgs *p = ctx;

  spinlock_acquire(&browse->LOCK_SOCK);
  if (browse->bindAddr && strlen(browse->bindAddr) == p->safeLen &&
      memcmp(p->safe, browse->bindAddr, p->safeLen) == 0)
    return true;
  spinlock_release(&browse->LOCK_SOCK);
  return false;
}

size_t unixSocketBind(OpenFile *fd, sockaddr_linux *addr, size_t len) {
  UnixSocket *sock = fd->dir;
  if (sock->bindAddr)
    return ERR(EINVAL);

  // sanitize the filename
  char *safe = unixSocketAddrSafe(addr, len);
  if (RET_IS_ERR((size_t)(safe)))
    return (size_t)safe;
  dbgSysExtraf("safe{%s}", safe);

  // check if it already exists
  struct stat statTarg = {0};
  bool        ret = fsStatByFilename(currentTask, safe, &statTarg);
  if (ret) {
    free(safe);
    if (!(statTarg.st_mode & S_IFSOCK))
      return ERR(ENOTSOCK);
    else
      return ERR(EADDRINUSE);
  }

  // make sure there are no duplicates
  SocketBindArgs args = {.safe = safe, .safeLen = strlen(safe)};
  spinlock_acquire(&LOCK_LL_UNIX_SOCKET);
  UnixSocket *browse = LinkedListSearch(&dsUnixSocket, unixSocketBindCb, &args);
  spinlock_release(&LOCK_LL_UNIX_SOCKET);

  // found a duplicate!
  if (browse) {
    spinlock_release(&browse->LOCK_SOCK);
    free(safe);
    return ERR(EADDRINUSE);
  }

  sock->bindAddr = safe;
  return 0;
}

size_t unixSocketListen(OpenFile *fd, int backlog) {
  if (backlog == 0) // newer kernel behavior
    backlog = 1;
  if (backlog < 0)
    backlog = 128;

  // maybe do a typical array here
  UnixSocket *sock = fd->dir;
  spinlock_acquire(&sock->LOCK_SOCK);
  sock->connMax = backlog;
  sock->backlog = calloc(sock->connMax * sizeof(UnixSocketPair *), 1);
  spinlock_release(&sock->LOCK_SOCK);
  return 0;
}

size_t unixSocketAccept(OpenFile *fd, sockaddr_linux *addr, uint32_t *len) {
  UnixSocket *sock = fd->dir;
  if (addr && len && *len > 0) {
    dbgSysStubf("todo addr");
  }

  while (true) {
    spinlock_acquire(&sock->LOCK_SOCK);
    if (sock->connCurr > 0)
      break;
    if (fd->flags & O_NONBLOCK) {
      sock->acceptWouldBlock = true;
      spinlock_release(&sock->LOCK_SOCK);
      return ERR(EWOULDBLOCK);
    } else
      sock->acceptWouldBlock = false;
    spinlock_release(&sock->LOCK_SOCK);
    hand_control();
  }

  // now pick the first thing! (sock spinlock already engaged)
  UnixSocketPair *pair = sock->backlog[0];
  spinlock_acquire(&pair->LOCK_PAIR);
  assert(pair->serverFds == 0);
  pair->serverFds++;
  pair->established = true;
  pair->filename = strdup(sock->bindAddr);
  spinlock_release(&pair->LOCK_PAIR);

  OpenFile *acceptFd = unixSocketAcceptCreate(pair);
  sock->backlog[0] = 0; // just in case
  memmove(sock->backlog, &sock->backlog[1],
          (sock->connMax - 1) * sizeof(UnixSocketPair *));
  sock->connCurr--;
  spinlock_release(&sock->LOCK_SOCK);
  return acceptFd->id;
}

typedef struct {
  UnixSocket *sock;
  char       *safe;
  size_t      safeLen;
} SocketConnectCbArgs;
bool unixSocketConnectCb(void *data, void *ctx) {
  UnixSocket          *parent = data;
  SocketConnectCbArgs *args = ctx;

  if (parent == args->sock)
    return false;

  spinlock_acquire(&parent->LOCK_SOCK);
  if (parent->bindAddr && strlen(parent->bindAddr) == args->safeLen &&
      memcmp(args->safe, parent->bindAddr, args->safeLen) == 0)
    return true;
  spinlock_release(&parent->LOCK_SOCK);

  return false;
}

size_t unixSocketConnect(OpenFile *fd, sockaddr_linux *addr, uint32_t len) {
  UnixSocket *sock = fd->dir;
  if (sock->connMax != 0) // already ran listen()
    return ERR(ECONNREFUSED);

  if (sock->pair) // already ran connect()
    return ERR(EISCONN);

  char *safe = unixSocketAddrSafe(addr, len);
  if (RET_IS_ERR((size_t)(safe)))
    return (size_t)safe;
  SocketConnectCbArgs args = {
      .sock = sock, .safe = safe, .safeLen = strlen(safe)};
  dbgSysExtraf("safe{%s}", safe);

  // find object
  spinlock_acquire(&LOCK_LL_UNIX_SOCKET);
  UnixSocket *parent =
      LinkedListSearch(&dsUnixSocket, unixSocketConnectCb, &args);
  spinlock_release(&LOCK_LL_UNIX_SOCKET);
  free(safe); // no longer needed

  // todo: actual filesystem contact
  if (!parent)
    return ERR(ENOENT);

  // nonblock edge case, check man page
  if (parent->acceptWouldBlock && fd->flags & O_NONBLOCK) {
    spinlock_release(&parent->LOCK_SOCK);
    return ERR(EINPROGRESS); // use select, poll, or epoll
  }

  // listen() hasn't yet ran
  if (!parent->connMax) {
    spinlock_release(&parent->LOCK_SOCK);
    return ERR(ECONNREFUSED);
  }

  // todo!
  assert(!(fd->flags & O_NONBLOCK));
  // spinlock_acquire(&parent->LOCK_SOCK);
  if (parent->connCurr >= parent->connMax) {
    spinlock_release(&parent->LOCK_SOCK);
    return ERR(ECONNREFUSED); // no slot
  }
  UnixSocketPair *pair = unixSocketAllocatePair();
  sock->pair = pair;
  pair->clientFds = 1;
  parent->backlog[parent->connCurr++] = pair;
  spinlock_release(&parent->LOCK_SOCK);
  pollInstanceRing((size_t)parent, EPOLLIN);
  pollInstanceRing((size_t)pair, EPOLLIN);

  // todo!
  assert(!(fd->flags & O_NONBLOCK));
  while (true) {
    spinlock_acquire(&pair->LOCK_PAIR);
    if (pair->established)
      break;
    spinlock_release(&pair->LOCK_PAIR);
    // wait for parent to accept this thing and have it's own fd on the side
    hand_control();
  }
  spinlock_release(&pair->LOCK_PAIR);

  return 0;
}

bool unixSocketDuplicate(OpenFile *original, OpenFile *orphan) {
  orphan->dir = original->dir;
  UnixSocket *unixSocket = original->dir;
  spinlock_acquire(&unixSocket->LOCK_SOCK);
  unixSocket->timesOpened++;
  if (unixSocket->pair) {
    spinlock_acquire(&unixSocket->pair->LOCK_PAIR);
    unixSocket->pair->clientFds++;
    spinlock_release(&unixSocket->pair->LOCK_PAIR);
  }
  spinlock_release(&unixSocket->LOCK_SOCK);

  return true;
}

bool unixSocketClose(OpenFile *fd) {
  UnixSocket *unixSocket = fd->dir;
  spinlock_acquire(&unixSocket->LOCK_SOCK);
  unixSocket->timesOpened--;
  size_t keyToNotify = 0;
  if (unixSocket->pair) {
    spinlock_acquire(&unixSocket->pair->LOCK_PAIR);
    unixSocket->pair->clientFds--;
    if (unixSocket->pair->clientFds == 0)
      keyToNotify = (size_t)unixSocket->pair;
    if (!unixSocket->pair->clientFds && !unixSocket->pair->serverFds)
      unixSocketFreePair(unixSocket->pair);
    else
      spinlock_release(&unixSocket->pair->LOCK_PAIR);
  }
  if (unixSocket->timesOpened == 0) {
    // destroy it
    spinlock_acquire(&LOCK_LL_UNIX_SOCKET);
    assert(LinkedListRemove(&dsUnixSocket, sizeof(UnixSocket), unixSocket));
    spinlock_release(&LOCK_LL_UNIX_SOCKET);
    goto skip_release;
  }
  spinlock_release(&unixSocket->LOCK_SOCK);
skip_release:
  if (keyToNotify)
    pollInstanceRing(keyToNotify, EPOLLHUP);
  return true;
}

size_t unixSocketRecvfrom(OpenFile *fd, uint8_t *out, size_t limit, int flags,
                          sockaddr_linux *addr, uint32_t *len) {
  // useless unless SOCK_DGRAM
  (void)addr;
  (void)len;

  UnixSocket     *socket = fd->dir;
  UnixSocketPair *pair = socket->pair;
  if (!pair)
    return ERR(ENOTCONN);
  if (!pair->serverFds && CircularReadPoll(&pair->clientBuff) == 0)
    return 0;
  while (true) {
    spinlock_acquire(&pair->LOCK_PAIR);
    if (!pair->serverFds && CircularReadPoll(&pair->clientBuff) == 0) {
      spinlock_release(&pair->LOCK_PAIR);
      return 0;
    } else if ((fd->flags & O_NONBLOCK || flags & MSG_DONTWAIT) &&
               CircularReadPoll(&pair->clientBuff) == 0) {
      spinlock_release(&pair->LOCK_PAIR);
      return ERR(EWOULDBLOCK);
    } else if (CircularReadPoll(&pair->clientBuff) > 0)
      break;
    spinlock_release(&pair->LOCK_PAIR);
  }

  // spinlock already acquired
  size_t toCopy = MIN(limit, CircularReadPoll(&pair->clientBuff));
  assert(CircularRead(&pair->clientBuff, out, toCopy) == toCopy);
  spinlock_release(&pair->LOCK_PAIR);
  pollInstanceRing((size_t)pair, EPOLLOUT);

  return toCopy;
}

size_t unixSocketSendto(OpenFile *fd, uint8_t *in, size_t limit, int flags,
                        sockaddr_linux *addr, uint32_t len) {
  // useless unless SOCK_DGRAM
  (void)addr;
  (void)len;

  UnixSocket     *socket = fd->dir;
  UnixSocketPair *pair = socket->pair;
  if (!pair)
    return ERR(ENOTCONN);
  if (limit > pair->serverBuffSize) {
    printf("[socket] Warning! Truncating limit{%ld} to serverBuffSize{%ld}\n",
           limit, pair->serverBuffSize);
    limit = pair->serverBuffSize;
  }

  while (true) {
    spinlock_acquire(&pair->LOCK_PAIR);
    if (!pair->serverFds) {
      spinlock_release(&pair->LOCK_PAIR);
      atomicBitmapSet(&currentTask->sigPendingList, SIGPIPE);
      return ERR(EPIPE);
    } else if ((fd->flags & O_NONBLOCK || flags & MSG_DONTWAIT) &&
               CircularWritePoll(&pair->serverBuff) < limit) {
      spinlock_release(&pair->LOCK_PAIR);
      return ERR(EWOULDBLOCK);
    } else if (CircularWritePoll(&pair->serverBuff) >= limit)
      break;
    spinlock_release(&pair->LOCK_PAIR);
  }

  // spinlock already acquired
  assert(CircularWrite(&pair->serverBuff, in, limit) == limit);
  spinlock_release(&pair->LOCK_PAIR);
  pollInstanceRing((size_t)pair, EPOLLIN);

  return limit;
}

size_t unixSocketGetpeername(OpenFile *fd, sockaddr_linux *addr,
                             uint32_t *len) {
  UnixSocket     *socket = fd->dir;
  UnixSocketPair *pair = socket->pair;
  if (!pair)
    return ERR(ENOTCONN);

  int actualLen = sizeof(addr->sa_family) + strlen(pair->filename);
  int toCopy = MIN(*len, actualLen);
  if (toCopy < sizeof(addr->sa_family)) // you're POOR!
    return ERR(EINVAL);
  addr->sa_family = AF_UNIX;
  memcpy(addr->sa_data, pair->filename, toCopy - sizeof(addr->sa_family));
  *len = toCopy;
  return 0;
}

size_t unixSocketRecvmsg(OpenFile *fd, struct msghdr_linux *msg, int flags) {
  if (msg->msg_name || msg->msg_namelen > 0) {
    dbgSysStubf("todo optional addr");
    return ERR(ENOSYS);
  }
  msg->msg_controllen = 0;
  msg->msg_flags = 0;
  size_t cnt = 0;
  bool   noblock = flags & MSG_DONTWAIT;
  for (int i = 0; i < msg->msg_iovlen; i++) {
    struct iovec *curr =
        (struct iovec *)((size_t)msg->msg_iov + i * sizeof(struct iovec));
    if (cnt > 0 && fd->handlers->internalPoll) {
      // check syscalls_fs.c for why this is necessary
      if (!(fd->handlers->internalPoll(fd, EPOLLIN) & EPOLLIN))
        return cnt;
    }
    size_t singleCnt = unixSocketRecvfrom(fd, curr->iov_base, curr->iov_len,
                                          noblock ? MSG_DONTWAIT : 0, 0, 0);
    if (RET_IS_ERR(singleCnt))
      return singleCnt;

    cnt += singleCnt;
  }

  return cnt;
}

size_t unixSocketSendmsg(OpenFile *fd, struct msghdr_linux *msg, int flags) {
  if (msg->msg_name || msg->msg_namelen > 0) {
    dbgSysStubf("todo optional addr");
    return ERR(ENOSYS);
  }
  assert(!msg->msg_controllen && !msg->msg_flags);
  size_t cnt = 0;
  bool   noblock = flags & MSG_DONTWAIT;
  for (int i = 0; i < msg->msg_iovlen; i++) {
    struct iovec *curr =
        (struct iovec *)((size_t)msg->msg_iov + i * sizeof(struct iovec));
    if (cnt > 0 && fd->handlers->internalPoll) {
      // check syscalls_fs.c for why this is necessary
      if (!(fd->handlers->internalPoll(fd, EPOLLOUT) & EPOLLOUT))
        return cnt;
    }
    size_t singleCnt = unixSocketSendto(fd, curr->iov_base, curr->iov_len,
                                        noblock ? MSG_DONTWAIT : 0, 0, 0);
    if (RET_IS_ERR(singleCnt))
      return singleCnt;

    cnt += singleCnt;
  }

  return cnt;
} // wooo

size_t unixSocketAcceptRecvmsg(OpenFile *fd, struct msghdr_linux *msg,
                               int flags) {
  if (msg->msg_name || msg->msg_namelen > 0) {
    dbgSysStubf("todo optional addr");
    return ERR(ENOSYS);
  }
  msg->msg_controllen = 0;
  msg->msg_flags = 0;
  size_t cnt = 0;
  bool   noblock = flags & MSG_DONTWAIT;
  for (int i = 0; i < msg->msg_iovlen; i++) {
    struct iovec *curr =
        (struct iovec *)((size_t)msg->msg_iov + i * sizeof(struct iovec));
    if (cnt > 0 && fd->handlers->internalPoll) {
      // check syscalls_fs.c for why this is necessary
      if (!(fd->handlers->internalPoll(fd, EPOLLIN) & EPOLLIN))
        return cnt;
    }
    size_t singleCnt = unixSocketAcceptRecvfrom(
        fd, curr->iov_base, curr->iov_len, noblock ? MSG_DONTWAIT : 0, 0, 0);
    if (RET_IS_ERR(singleCnt))
      return singleCnt;

    cnt += singleCnt;
  }

  return cnt;
}

int unixSocketInternalPoll(OpenFile *fd, int events) {
  UnixSocket *socket = fd->dir;
  int         revents = 0;

  if (socket->connMax > 0) {
    // listen()
    spinlock_acquire(&socket->LOCK_SOCK);
    // todo: fix below
    if (socket->connCurr < socket->connMax) // can connect()
      revents |= (events & EPOLLOUT) ? EPOLLOUT : 0;
    if (socket->connCurr > 0) // can accept()
      revents |= (events & EPOLLIN) ? EPOLLIN : 0;
    spinlock_release(&socket->LOCK_SOCK);
  } else if (socket->pair) {
    // connect()
    UnixSocketPair *pair = socket->pair;
    spinlock_acquire(&pair->LOCK_PAIR);
    if (!pair->serverFds)
      revents |= EPOLLHUP;

    if (events & EPOLLOUT && pair->serverFds &&
        CircularWritePoll(&pair->serverBuff) > UNIX_SOCK_POLL_EXTRA)
      revents |= EPOLLOUT;

    if (events & EPOLLIN && CircularReadPoll(&pair->clientBuff) > 0)
      revents |= EPOLLIN;
    spinlock_release(&pair->LOCK_PAIR);
  } else
    revents |= EPOLLHUP;

  return revents;
}

size_t unixSocketReportKey(OpenFile *fd) {
  UnixSocket *socket = fd->dir;
  return socket->pair ? (size_t)socket->pair : (size_t)socket;
}

size_t unixSocketPair(int type, int protocol, int *sv) {
  size_t sock1 = unixSocketOpen(currentTask, type, protocol);
  if (RET_IS_ERR(sock1))
    return sock1;

  OpenFile *sock1Fd = fsUserGetNode(currentTask, sock1);
  assert(sock1Fd);

  UnixSocketPair *pair = unixSocketAllocatePair();
  pair->clientFds = 1;
  pair->serverFds = 1;

  UnixSocket *sock = sock1Fd->dir;
  sock->pair = pair;

  OpenFile *sock2Fd = unixSocketAcceptCreate(pair);

  // finish it off
  sv[0] = sock1Fd->id;
  sv[1] = sock2Fd->id;

  dbgSysExtraf("fds{%d, %d}", sv[0], sv[1]);
  return 0;
}

VfsHandlers unixSocketHandlers = {
    .read = NULL,
    .write = NULL,
    .seek = NULL,
    .ioctl = NULL,
    .stat = NULL,
    .mmap = NULL,
    .getdents64 = NULL,
    .getFilesize = NULL,
    .poll = NULL,
    .fcntl = NULL,
    .bind = unixSocketBind,
    .listen = unixSocketListen,
    .accept = unixSocketAccept,
    .connect = unixSocketConnect,
    .recvfrom = unixSocketRecvfrom,
    .sendto = unixSocketSendto,
    .recvmsg = unixSocketRecvmsg,
    .sendmsg = unixSocketSendmsg,
    .getsockname = NULL,
    .getsockopts = NULL,
    .getpeername = unixSocketGetpeername,
    .internalPoll = unixSocketInternalPoll,
    .reportKey = unixSocketReportKey,
    .addWatchlist = NULL,
    .duplicate = unixSocketDuplicate,
    .open = NULL,
    .close = unixSocketClose
};

VfsHandlers unixAcceptHandlers = {
    .read = NULL,
    .write = NULL,
    .seek = NULL,
    .ioctl = NULL,
    .stat = NULL,
    .mmap = NULL,
    .getdents64 = NULL,
    .getFilesize = NULL,
    .poll = NULL,
    .fcntl = NULL,
    .bind = NULL,
    .listen = NULL,
    .accept = NULL,
    .connect = NULL,
    .recvfrom = unixSocketAcceptRecvfrom,
    .sendto = unixSocketAcceptSendto,
    .recvmsg = unixSocketAcceptRecvmsg,
    .sendmsg = NULL,
    .getsockname = NULL,
    .getsockopts = NULL,
    .getpeername = unixSocketAcceptGetpeername,
    .internalPoll = unixSocketAcceptInternalPoll,
    .reportKey = unixSocketAcceptReportKey,
    .addWatchlist = NULL,
    .duplicate = unixSocketAcceptDuplicate,
    .open = NULL,
    .close = unixSocketAcceptClose
};