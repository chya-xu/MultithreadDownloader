/*
 * @Author: xuqiaxin
 * @Mail: qiaxin.xu@foxmail.com
 * @Date: 2022-11-15 20:05:56
 * @Description: file content
 */
#ifndef _DOWNLOADERS_H_
#include <string>
#include <functional>
using namespace std;

#define file_size_t unsigned long long

typedef function<bool(const char*, size_t)> DataDealCallback;

// 下载器类型
enum DownloaderType {
    HTTP
};


// 下载相关信息
struct DownloadInfo {
    DownloadInfo(DownloaderType type, string url): type(type), url(url) {}
    DownloaderType type; // 使用的下载器类型
    string url; // 文件下载链接
};


// 下载器接口类
class Downloader {
public:
    explicit Downloader() = default;

    /**
     * @description: 判断下载网站是否支持断点续传
     * @return {bool}
     */
    virtual bool IsRangeAvailable() = 0;

    /**
     * @description: 初始化下载器
     * @param {const string&} url 下载的url
     * @return {bool} 成功返回true， 失败返回false
     */
    virtual bool Init(const string& url) = 0;

    /**
     * @description: 下载文件
     * @param {const file_size_t} start_pos 下载起始字节
     * @param {const file_size_t end_pos 下载结束字节
     * @param {DataDealCallback} call 管理器提供的回调函数
     * @return {bool} 成功返回true， 失败返回false
     */
    virtual bool Download(const file_size_t start_pos, const file_size_t end_pos, DataDealCallback call) = 0;

    /**
     * @description: 获取文件大小，单位字节
     * @return {file_size_t} 返回字节数
     */
    virtual file_size_t GetFileSize() = 0;

    virtual ~Downloader() {};
};

#endif