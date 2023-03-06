/*
 * @Author: xuqiaxin
 * @Mail: qiaxin.xu@foxmail.com
 * @Date: 2022-11-15 23:43:09
 * @Description: http下载器
 */
#ifndef HttpDownloader

#include <curl/curl.h>
#include "downloaders.h"

class HttpDownloader: public Downloader {
public:
    explicit HttpDownloader();

    /**
     * @description: 获取文件大小，单位字节
     * @return {file_size_t} 返回字节数
     */
    file_size_t GetFileSize();

    /**
     * @description: 判断下载网站是否支持断点续传
     * @return {bool}
     */
    bool IsRangeAvailable();

    /**
     * @description: 初始化下载器
     * @param {const string&} url 下载的url
     * @return {bool} 成功返回true， 失败返回false
     */
    bool Init(const std::string& url);

    /**
     * @description: 下载文件
     * @param {const file_size_t} start_pos 下载起始字节
     * @param {const file_size_t} end_pos 下载结束字节
     * @param {DataDealCallback} call 管理器提供的回调函数
     * @return {bool} 成功返回true， 失败返回false
     */
    bool Download(const file_size_t start_pos, const file_size_t end_pos, DataDealCallback call);

    ~HttpDownloader();

private:
    /**
     * @description: 获取文件信息
     * @return {bool} 成功返回true， 失败返回false
     */    
    bool GetFileInfo();

    /**
     * @description: 接收的文件内容的处理回调函数
     * @param {void*} data 传入的数据
     * @param {size_t} size 
     * @param {size_t} nmemb
     * @param {void*} stream
     * @return {size_t} 成功返回实际处理的字节数， 失败返回 0
     */    
    static size_t ReadDataCallback(void* data, size_t size, size_t nmemb, void* stream);
    
    string m_url;
    double m_filesize;
    bool m_range_supported;
};

#endif