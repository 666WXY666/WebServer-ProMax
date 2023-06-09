#include "../headers/httpresponse.h"

// 静态变量，文件类型和返回类型键值对
const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE =
    {
        {".html", "text/html"},
        {".xml", "text/xml"},
        {".xhtml", "application/xhtml+xml"},
        {".txt", "text/plain"},
        {".rtf", "application/rtf"},
        {".pdf", "application/pdf"},
        {".word", "application/msword"},
        {".png", "image/png"},
        {".gif", "image/gif"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".au", "audio/basic"},
        {".mpeg", "video/mpeg"},
        {".mpg", "video/mpeg"},
        {".avi", "video/x-msvideo"},
        {".gz", "application/x-gzip"},
        {".tar", "application/x-tar"},
        {".css", "text/css "},
        {".js", "text/javascript "},
};

// 静态变量，状态码和信息键值对
const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS =
    {
        {200, "OK"},
        {400, "Bad Request"},
        {403, "Forbidden"},
        {404, "Not Found"},
};

// 静态变量，错误码与页面对应关系
const std::unordered_map<int, std::string> HttpResponse::CODE_PATH =
    {
        {400, "/400.html"},
        {403, "/403.html"},
        {404, "/404.html"},
};

/*
 * 构造函数中初始化相关变量
 */
HttpResponse::HttpResponse() : code_(-1), path_(""), srcDir_(""), isKeepAlive_(false), mmFile_(nullptr)
{
    mmFileStat_ = {0};
}

/*
 * 析构函数释放资源
 */
HttpResponse::~HttpResponse()
{
    unmapFile();
}

/*
 * 根据参数初始化httpResponse中变量
 */
void HttpResponse::init(const std::string &srcDir, std::string &path, bool isKeepAlive, int code)
{
    assert(srcDir != "");
    // 如果mmFile_不为空，那么先取消对应的映射
    if (mmFile_)
    {
        unmapFile();
    }
    code_ = code;
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}

/*
 * 返回状态码信息
 */
int HttpResponse::code() const
{
    return code_;
}

/*
 * 返回文件内存映射地址
 */
char *HttpResponse::file()
{
    return mmFile_;
}

/*
 * 返回文件大小
 */
size_t HttpResponse::fileLen() const
{
    return mmFileStat_.st_size;
}

/*
 * 保存错误码为400，403，404的文件路径，将文件信息存入mmFileStat_变量中
 */
void HttpResponse::errorHtml_()
{
    // 若返回码为400，403，404其中之一，则将对应的文件路径与信息读取出来，并将文件信息保存mmFileStat_
    if (CODE_PATH.count(code_) == 1)
    {
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}

/*
 * 解除内存映射
 */
void HttpResponse::unmapFile()
{
    if (mmFile_)
    {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

/*
 * 获取文件对应的返回类型
 */
std::string HttpResponse::getFileType_()
{
    // 根据后缀名，判断文件类型
    std::string::size_type idx = path_.find_last_of('.');
    if (idx == std::string::npos)
    {
        // 没有后缀名，那就设置文件类型为 text/plain
        return "text/plain";
    }
    // 分割得到后缀名
    std::string suffix = path_.substr(idx);
    // 根据后缀名拿到SUFFIX_TYPE中的对应值
    if (SUFFIX_TYPE.count(suffix) == 1)
    {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    // 不符合上面条件就返回text/plain
    return "text/plain";
}

/*
 * 打开文件失败，组装返回信息，写入到发送缓冲区中
 */
void HttpResponse::errorContent(Buffer &buff, std::string message)
{
    std::string body;
    std::string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if (CODE_STATUS.count(code_) == 1)
    {
        status = CODE_STATUS.find(code_)->second;
    }
    else
    {
        status = "Bad Request";
    }
    body += std::to_string(code_) + " : " + status + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>WebServer</em></body></html>";

    // 这里先把Content-length长度写入到响应头中，后面再append上面定义的html响应体body
    // 这里有两组\r\n，第一个代表当前行的结束，第二个代表响应头后面与响应体隔开的空行
    buff.append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
    buff.append(body);
}

/*
 * 将返回信息中的 状态行 添加到写缓冲区中
 * 状态行示例：HTTP/1.1 200 OK
 * CODE_STATUS中含有200，400，403，404这四个状态
 */
void HttpResponse::addStateLine_(Buffer &buff)
{
    std::string status; // 状态码的说明，比如OK等
    if (CODE_STATUS.count(code_) == 1)
    {
        // 根据状态码获取对应的字符串，将其赋值给status变量
        status = CODE_STATUS.find(code_)->second;
    }
    else
    {
        // 其余CODE_STATUS中不存在的状态码统一以400作为状态码，表示请求报文存在语法错误
        code_ = 400;
        // 根据状态码获取对应的字符串，将其赋值给status变量
        status = CODE_STATUS.find(400)->second;
    }
    // 拼接字符串获取状态行，并将信息加入到写缓冲区中
    buff.append("HTTP/1.1 " + std::to_string(code_) + " " + status + "\r\n");
}

/*
 * 将返回信息中的 响应头 添加到写缓冲区中
 * 其实返回头中是没有 Connection 与 keep-alive 这两个字段的
 */
void HttpResponse::addHeader_(Buffer &buff)
{
    // 组装信息，将Connection信息送入写缓冲区中
    buff.append("Connection: ");
    if (isKeepAlive_)
    {
        buff.append("keep-alive\r\n");
        buff.append("keep-alive: max=6, timeout=120\r\n");
    }
    else
    {
        buff.append("close\r\n");
    }
    // 继续组装信息，将Content-type信息输送写缓冲区中
    buff.append("Content-type: " + getFileType_() + "\r\n");
}

/*
 * 将返回信息中的 响应体（服务器上的文件） 映射到内存中
 * 采用的是将文件映射到内存中一块区域这种方式，比较快节省资源
 * 此函数负责将文件映射到内存中
 */
void HttpResponse::addContent_(Buffer &buff)
{
    // 根据文件名以只读方式打开文件，得到资源文件的文件描述符
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if (srcFd < 0)
    {
        // 若打开文件失败，向客户端发送指定错误信息的html页面
        errorContent(buff, "File Not Found!");
        return;
    }

    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    // note: 将文件映射到内存提高文件的访问速度
    // MAP_PRIVATE 建立一个写入时拷贝的私有映射，MAP_PRIVATE被该进程私有，不会共享
    int *mmRet = (int *)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    // 若映射文件失败，向客户端发送指定错误信息的html页面
    if (*mmRet == -1)
    {
        errorContent(buff, "File Not Found!");
        return;
    }
    // 将映射的地址赋值给mmFile_变量
    mmFile_ = (char *)mmRet;
    // 映射成功后就可以关闭文件描述符了
    int ret = close(srcFd);

    // 继续向响应头添加Content-length信息并加入发送缓存中，返回内容的长度信息
    // 注意这里是往响应头添加字段，因为响应体文件映射在内存，没有在buff中，后面通过聚集写传输到Socket
    // 这个字段也只能在该函数里添加，而不能在addHeader_()里，因为调用addHeader_()时还不知道文件大小
    // 这里有两组\r\n，第一个代表当前行的结束，第二个代表响应头后面与响应体隔开的空行
    buff.append("Content-length: " + std::to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

/*
 * 拼装返回的头部以及需要发送的文件
 */
void HttpResponse::makeResponse(Buffer &buff)
{
    // 判断请求的资源文件
    // 如果服务器上无法找到请求的资源或者是目录（在请求中已经将连接的默认文件补充完整，如果还是目录说明错误）
    // note: stat用来将参数file_name所指的文件状态, 复制到参数mmFileStat_所指的结构中。若执行失败，即返回值为-1
    if (stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode))
    {
        // 返回404 NOT FOUND错误
        code_ = 404;
    }
    // 请求的资源没有读取权限，访问被服务器拒绝，置状态码code_为403
    else if (!(mmFileStat_.st_mode & S_IROTH))
    {
        // 返回403 Forbidden错误
        code_ = 403;
    }
    // code默认是-1，将状态码置为200，表示成功
    // 但是init其实会将code设为200或400等，所以感觉这一个分支没啥作用
    else if (code_ == -1)
    {
        code_ = 200;
    }
    // 若状态码码为400，403，404其中之一，则将文件路径与信息读取到path_与mmFileStat_变量中
    errorHtml_();
    // 根据状态码将返回信息中的状态行添加到写缓冲区中
    addStateLine_(buff);
    // 将返回信息中的 响应头 添加到写缓冲区中
    addHeader_(buff);
    // 将返回信息中的 响应体（服务器上的文件） 映射到内存中
    addContent_(buff);
}
