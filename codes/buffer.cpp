#include "../headers/buffer.h"

/*
 * 构造函数
 */
Buffer::Buffer(int initBufferSize) : buffer_(initBufferSize), readPos_(0), writePos_(0)
{
}

/*
 * 返回缓冲区的首地址指针，因为buffer是vector，
 * 所以通过buffer_.begin()取得头部位置的迭代器，再*取值，再取地址得到真实的数据起始地址
 */
char *Buffer::beginPtr_()
{
    return &*buffer_.begin();
}

/*
 * 上面函数的const重载
 */
const char *Buffer::beginPtr_() const
{
    return &*buffer_.begin();
}

/*
 * 获取当前可以读取的数据大小
 */
size_t Buffer::readableBytes() const
{
    // 这里writePos_总是>=readPos_，因为每次都有扩容检查，扩容检查把位置重置了
    // 并且如果writePos_和readPos_重叠，代表无数据可读
    return writePos_ - readPos_;
}

/*
 * 计算缓冲区还能写入多少字节数据
 */
size_t Buffer::writableBytes() const
{
    return buffer_.size() - writePos_;
}

/*
 * 返回当前readPos_所在的位置
 */
size_t Buffer::prependableBytes() const
{
    return readPos_;
}

/*
 * 返回当前readPos_所在字符地址的指针
 */
const char *Buffer::peek() const
{
    return beginPtr_() + readPos_;
}

/*
 * 移动readPos_指针，表示这一段已经被读取了
 */
void Buffer::retrieve(size_t len)
{
    assert(len <= readableBytes());
    readPos_ += len;
}

/*
 * 移动readPos_指针，移动大小为readPos_所在字符地址的指针到end这一段
 */
void Buffer::retrieveUntil(const char *end)
{
    assert(peek() <= end);
    retrieve(end - peek());
}

/*
 * 回收所有空间，将读写指针还原，buffer清空
 */
void Buffer::retrieveAll()
{
    // 因为buffer是vertor，所以需要&buffer_[0]获取数据的起始地址
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}

/*
 * 以string类型返回被回收的数据，同时回收所有空间
 */
std::string Buffer::retrieveAllToStr()
{
    std::string str(peek(), readableBytes());
    retrieveAll();

    return str;
}

/*
 * 计算缓冲区中能够开始写数据的位置
 */
char *Buffer::beginWrite()
{
    return beginPtr_() + writePos_;
}

/*
 * const重载版
 * 计算缓冲区中能够开始写数据的位置
 */
const char *Buffer::beginWriteConst() const
{
    return beginPtr_() + writePos_;
}

/*
 * 写入了len这么多的数据，那么就要移动writePos_位置
 */
void Buffer::hasWritten(size_t len)
{
    writePos_ += len;
}

/*
 * 使用标准库的std函数将指定大小数据复制到BeginWrite()指定的缓冲区中
 */
void Buffer::append(const char *str, size_t len)
{
    assert(str);
    // 申请空间，或者压缩空间，确保能够写下len这么多的数据
    ensureWritable(len);
    // 将str到str + len这段区间的数据写入到BeginWrite()指向的地址
    std::copy(str, str + len, beginWrite());
    // 根据写入的数据大小，调用HasWritten函数向后移动writePos_指针
    hasWritten(len);
}

/*
 * 对append(const char* str, size_t len)函数的包装
 * 使得string类也能使用该函数
 */
void Buffer::append(const std::string &str)
{
    // str.data()获取str中的数据的char指针
    append(str.data(), str.length());
}

/*
 * 对append(const char* str, size_t len)函数的包装
 * 将data指针转型为char指针，再调用append(const char* str, size_t len)
 */
void Buffer::append(const void *data, size_t len)
{
    assert(data);
    // 转型为char*后执行希望的操作
    append(static_cast<const char *>(data), len);
}

/*
 * 将传入的buff对象中的未读取数据写入本buff中
 */
void Buffer::append(const Buffer &buff)
{
    // buff.Peek()指示该buff中第一个未被读取的数据位置，buff.ReadableBytes()表示需要写入多少数据
    append(buff.peek(), buff.readableBytes());
}

/*
 * 该保证写数据的空间是足够的
 * 首先比较空闲空间与需要写入数据的大小，若空闲空间不够，则使用MakeSpace_函数扩容
 */
void Buffer::ensureWritable(size_t len)
{
    if (writableBytes() < len)
    {
        // 空间不足，调用MakeSpace_函数扩容
        makeSpace_(len);
    }
    // 检查一下，保证写数据的空间是足够的
    assert(writableBytes() >= len);
}

/*
 * 扩展缓存空间的函数
 * 解释：假设[____***************______]，总长度25，已写15，空闲10
 *              |              |
 *          readPos_=4    writePos_=19
 * 前面_代表已经在缓冲区中被读取的的数据，*代表写入了缓冲区但还未被读取的数据，_代表缓冲区中剩余的空间
 * 本函数就是确认前面的_与后面的_所占的空间是否足够放入最新所需的数据
 * 如果不够，那么直接将缓冲区resize成 writePos_ + len + 1 这么大
 * 如果足够，那就将 * 所代表的数据向前移动 [*******__________]，再将writePos_移动到对应的位置，readPos_置0即可
 */
void Buffer::makeSpace_(size_t len)
{
    // 后面可写的+前面已读的
    // 在上例中writableBytes()=25-19=6，prependableBytes()=4
    // 空闲=6+4=10
    // 如果可写的数据大小+当前读取的数据大小<所申请的空间长度
    // 那么直接申请一块 writePos_ + len + 1 大小的空间
    if (writableBytes() + prependableBytes() < len)
    {
        // 假设len=10，那么就直接申请19+10+1=30的空间，后面就空闲出11的空间
        // [____***************___________]，总长度30，已写15，空闲15
        //      |              |
        //  readPos_=4    writePos_=19
        buffer_.resize(writePos_ + len + 1);
    }
    // 长度够，直接移动
    else
    {
        // 获取当前可以读取的数据大小,也就是还有多少数据再buffer中没有被读取
        size_t readable = readableBytes(); // 15
        // 移动[readPos_,writePos_]到[0,readable]，即[4,19]移动到[0,15]
        std::copy(beginPtr_() + readPos_, beginPtr_() + writePos_, beginPtr_());
        // 因为读取过的数据直接被覆盖掉了，所以将readPos_置0即可
        readPos_ = 0;
        // 直接将写数据指针移动到指定位置，这里不加readPos_是一样的
        writePos_ = readPos_ + readable;
        // 此时状态为
        // [***************__________]，总长度25，已写15，空闲10
        //  |              |
        // readPos_=0  writePos_=15
        // 确认指针移动是否正确
        assert(readable == readableBytes());
    }
}

/*
 * 读取socket的fd中的数据到buffer中
 */
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    char buff[65535]; // 64KB少1B
    // 这里指定两个缓冲区，其实可以更多
    struct iovec iov[2];
    // 计算还能写入多少数据
    const size_t writable = writableBytes();

    // note: 分散读，保证数据全部读完，先读到0中，超过的读到临时1中
    //       readv 总是先填满一个缓冲区，然后再填写下一个
    // iov[0]：Buffer内置的数组_buffer，默认大小是1024
    // iov[1]：buff临时数组，大小为65535
    iov[0].iov_base = beginPtr_() + writePos_;
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);
    // len为读到的数据长度
    const ssize_t len = readv(fd, iov, 2);
    // 读取的数据长度小于0，那么肯定是发生错误了，将错误码返回
    if (len < 0)
    {
        *saveErrno = errno;
    }
    // 读取的数据比缓冲区小，那么可以正常读取，直接将读缓冲区指针移动到对应位置
    else if (static_cast<size_t>(len) <= writable)
    {
        hasWritten(len);
    }
    // 读取的数据比缓冲区大，首先将读取指针指到缓冲区尾部
    // 然后再调用append方法申请多出来的这一部分空间，并将iov[1]临时数组的数据复制过去
    else
    {
        // 缓冲区已满，移动writePos_指针到最后
        writePos_ = buffer_.size();
        // len - writable为剩下的在iov[1]临时数组的数据长度
        // append函数会自动扩容
        append(buff, len - writable);
    }

    return len;
}

/*
 * 将buffer的数据写入socket的fd中，即向socket中发送数据
 */
ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
    // 获取当前可以读取多少数据
    size_t readSize = readableBytes();
    // 向socket中写入数据
    ssize_t len = write(fd, peek(), readSize);
    // 返回若是负数，表明出错了
    if (len < 0)
    {
        *saveErrno = errno;
        return len;
    }
    // 根据发送的数据大小移动readPos_
    readPos_ += len;

    return len;
}
