#ifndef MY_WEBSERVER_BUFFER_H
#define MY_WEBSERVER_BUFFER_H

#include <assert.h>
#include <sys/uio.h> /*readv*/
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <string>
#include <vector>

class Buffer {
private:
    std::vector<char>        buffer_;    // vector实现的自增长缓冲区
    std::atomic<std::size_t> readPos_;   // 读指针所在的下标
    std::atomic<std::size_t> writePos_;  // 写指针所在的下标

    char       *beginPtr_();
    const char *beginPtr_() const;

    void makeSpace_(size_t len);

public:
    Buffer(int initBufferSize = 1024);
    ~Buffer() = default;

    void clearAll();

    size_t writableBytes() const;
    size_t readableBytes() const;
    size_t prependableBytes() const;

    char       *beginRead();
    const char *beginRead() const;

    char       *beginWrite();
    const char *beginWrite() const;

    void        retrieve(size_t len);
    void        retrieveUntil(const char *end);
    std::string retrieveAllToStr();

    void ensureWritable(size_t len);

    void hasWritten(size_t len);

    void append(const char *str, size_t len);
    void append(const std::string &str);
    void append(const Buffer &buff);

    ssize_t readFd(int fd, int *saveErrno);
    ssize_t writeFd(int fd, int *saveErrno);
};

#endif  //MY_WEBSERVER_BUFFER_H
