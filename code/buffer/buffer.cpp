#include "buffer.h"

Buffer::Buffer(int initBufferSize) : readPos_(0), writePos_(0) { buffer_.resize(initBufferSize); }

/**
 * @description: 已经读取了多少字节的数据
 */
size_t Buffer::prependableBytes() const { return readPos_; }

/**
 * @description: 获取当前可以读取的数据大小
 */
size_t Buffer::readableBytes() const { return writePos_ - readPos_; }

/**
 * @description: 缓冲区还能写入多少字节数据
 */
size_t Buffer::writableBytes() const { return buffer_.size() - writePos_; }

/**
 * @description: 返回缓冲区的首地址
 */
char *Buffer::beginPtr() const { return const_cast<char *>(&(*buffer_.begin())); }

/**
 * @description: 清空缓冲区
 */
void Buffer::clearAll() {
    bzero(beginPtr(), buffer_.size());
    readPos_  = 0;
    writePos_ = 0;
}

/**
 * @description: 返回读指针地址
 */
char *Buffer::beginRead() const { return beginPtr() + readPos_; }

/**
 * @description: 返回写指针地址
 */
char *Buffer::beginWrite() const { return beginPtr() + writePos_; }

/**
 * @description: 读取len长的数据后，更新读指针
 * @param {size_t} len
 */
void Buffer::hasRead(size_t len) {
    assert(len <= readableBytes());
    readPos_ += len;
}

/**
 * @description: 写入len长数据后更新写指针位置
 * @param {size_t} len
 */
void Buffer::hasWritten(size_t len) { writePos_ += len; }

/**
 * @description: 更新读指针到end处
 * @param {char} *end
 */
void Buffer::retrieveUntil(const char *end) {
    assert(beginRead() <= end);
    hasRead(end - beginRead());
}

/**
 * @description: 取出所有数据
 * @return {string}
 */
std::string Buffer::retrieveAllToStr() {
    std::string str(beginRead(), readableBytes());
    clearAll();
    return str;
}

/**
 * @description: 往缓冲区添加数据，有3个重载
 * @param {char} *str
 * @param {size_t} len
 */
void Buffer::append(const char *str, size_t len) {
    assert(str);
    /*申请空间，或者压缩空间，确保能够写下len这么多的数据*/
    ensureWritable(len);
    /*将str到str + len这段区间的数据写入到BeginWrite()指向的地址*/
    std::copy(str, str + len, beginWrite());
    /*根据写入的数据大小，调用HasWritten函数向后移动writePos_指针*/
    hasWritten(len);
}

void Buffer::append(const std::string &str) {
    /*str.data()获取str的char指针*/
    append(str.data(), str.size());
}

void Buffer::append(const Buffer &buff) { append(buff.beginRead(), buff.readableBytes()); }

/**
 * @description: 保证足够空闲空间添加数据
 * @param {size_t} len
 */
void Buffer::ensureWritable(size_t len) {
    if (writableBytes() < len) {
        /*空间不足，调用MakeSpace_函数扩容*/
        extendSpace(len);
    }
    /*检查一下，保证写数据的空间是足够的*/
    assert(writableBytes() >= len);
}

/**
 * @description: 自增长扩容机制，要么申请更大内存，要么清理已经被读取的空间
 * @param {size_t} len
 */
void Buffer::extendSpace(size_t len) {
    if (writableBytes() + prependableBytes() < len) {
        buffer_.resize(writePos_ + len + 1);
    } else {
        /*获取当前还有多少数据再buffer中没有被读取*/
        size_t readable = readableBytes();
        /*将原有数据前移，覆盖掉读取过的数据空间*/
        std::copy(beginRead(), beginWrite(), beginPtr());
        readPos_  = 0;
        writePos_ = readable;
        /*确认指针移动是否正确*/
        assert(readable == readableBytes());
    }
}

/**
 * @description: 读取socket中的数据，并设置可能的错误号
 * @param {int} fd
 * @param {int} *saveErrno
 * @return 返回读取的字节数
 */
ssize_t Buffer::readFd(int fd, int *saveErrno) {
    /* 用来存储缓冲区可能装不完的那部分数据 */
    char buff[65535];

    /*计算还能写入多少数据*/
    size_t writable = writableBytes();

    /*分散读， 保证数据全部读完*/
    struct iovec iov[2];
    iov[0].iov_base = beginWrite();
    iov[0].iov_len  = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len  = sizeof(buff);

    ssize_t len = readv(fd, iov, 2);
    if (len < 0) {
        /*读取的数据长度小于0，那么肯定是发生错误了，将错误码返回*/
        *saveErrno = errno;
    } else if (static_cast<size_t>(len) <= writable) {
        /*读取的数据比缓冲区小，那么可以正常读取，直接将读缓冲区指针移动到对应位置*/
        hasWritten(len);
    } else {
        /*读取的数据比缓冲区大，首先将缓冲区的写入指针指到缓冲区尾部，然后再将超容的数据复制过去*/
        writePos_ = buffer_.size();
        append(buff, len - writable);
    }

    return len;
}

/**
 * @description: 往socket中写数据，并设置可能的错误号，未用到
 * @param {int} fd
 * @param {int} *saveErrno
 * @return 返回写了多少字节
 */
ssize_t Buffer::writeFd(int fd, int *saveErrno) {
    /*向socket中写入数据*/
    ssize_t len = write(fd, beginRead(), readableBytes());
    /*返回若是负数，表明出错了*/
    if (len < 0) {
        *saveErrno = errno;
        return len;
    }
    /*根据发送的数据大小移动readPos_*/
    hasRead(len);

    return len;
}
