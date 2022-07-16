/*
 * @Description  : 自定义缓冲区类
 * @Date         : 2022-07-16 01:14:05
 * @LastEditTime : 2022-07-16 19:56:35
 */
#ifndef BUFFER_H
#define BUFFER_H

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

    char *beginPtr() const;

    void extendSpace(size_t len);
    void ensureWritable(size_t len);

public:
    Buffer(int initBufferSize = 1024);
    ~Buffer() = default;

    void clearAll();

    size_t writableBytes() const;
    size_t readableBytes() const;
    size_t prependableBytes() const;

    char *beginRead() const;
    char *beginWrite() const;

    void hasRead(size_t len);
    void hasWritten(size_t len);

    void        retrieveUntil(const char *end);
    std::string retrieveAllToStr();

    void append(const char *str, size_t len);
    void append(const std::string &str);
    void append(const Buffer &buff);

    ssize_t readFd(int fd, int *saveErrno);
    ssize_t writeFd(int fd, int *saveErrno);
};

#endif  //BUFFER_H
