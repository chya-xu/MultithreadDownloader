/*
 * @Author: xuqiaxin
 * @Mail: qiaxin.xu@foxmail.com
 * @Date: 2022-11-15 23:43:34
 * @Description: http下载器
 */
#include "string.h"
#include "httpdownloader.h"
#include <curl/curl.h>
using namespace std;

#define TCP_KEEPIDLE 120L
#define TCP_KEEPINTVL 60L

HttpDownloader::HttpDownloader(): m_filesize(0), m_range_supported(true) {

}

HttpDownloader::~HttpDownloader() {

}

/**
 * @description: 初始化下载器
 * @param {const string&} url 下载的url
 * @return {bool} 成功返回true， 失败返回false
 */
bool HttpDownloader::Init(const std::string& url) {
    m_url = url;
    return GetFileInfo();
}

/**
 * @description: 接收的文件内容的处理回调函数
 * @param {void*} data 传入的数据
 * @param {size_t} size
 * @param {size_t} nmemb
 * @param {void*} stream
 * @return {size_t} 成功返回实际处理的字节数， 失败返回 0
 */
size_t HttpDownloader::ReadDataCallback(void* data, size_t size, size_t nmemb, void* stream) {
    size_t total_size = size * nmemb;
    DataDealCallback* call = (DataDealCallback*)stream;
    if (!(*call)((char*)data, total_size)) {
        return 0;
    }
    return total_size;
}

/**
 * @description: 下载文件
 * @param {const file_size_t} start_pos 下载起始字节
 * @param {const file_size_t} end_pos 下载结束字节
 * @param {DataDealCallback} call 管理器提供的回调函数
 * @return {bool} 成功返回true， 失败返回false
 */
bool HttpDownloader::Download(const file_size_t start_pos, const file_size_t end_pos, DataDealCallback call) {
    string range = to_string(start_pos) + "-" + to_string(end_pos);

    CURL* curl_handle = curl_easy_init();
    if (!curl_handle) {
        return false;
    }
    // 设置参数
    curl_easy_setopt(curl_handle, CURLOPT_URL, m_url.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_RANGE, range.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_TCP_KEEPIDLE, TCP_KEEPIDLE);
    curl_easy_setopt(curl_handle, CURLOPT_TCP_KEEPINTVL, TCP_KEEPINTVL);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &call);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, &HttpDownloader::ReadDataCallback);
    // 运行
    CURLcode res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK) {
        printf("curl_easy_perform failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl_handle);
        return false;
    }

#if DEBUG
    double download_size = 0;
    curl_easy_getinfo(curl_handle, CURLINFO_SIZE_DOWNLOAD, &download_size);
    printf("current download size is %f\n", download_size);
#endif

    curl_easy_cleanup(curl_handle);
    return true;
}

/**
 * @description: 获取文件信息
 * @return {bool} 成功返回true， 失败返回false
 */
bool HttpDownloader::GetFileInfo() {
    CURL* curl_handle = curl_easy_init();
    if (!curl_handle) {
        return false;
    }

    // 设置参数
    curl_easy_setopt(curl_handle, CURLOPT_URL, m_url.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_RANGE, "0-");

    // 运行
    CURLcode res = curl_easy_perform(curl_handle);
    if (res == CURLE_RANGE_ERROR) {
        m_range_supported = false;
    }
    else if (res != CURLE_OK) {
        printf("curl_easy_perform failed: %s\n", curl_easy_strerror(res));
        goto end;
    }

    // 获取文件大小
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &m_filesize);
    if (CURLE_OK != res) {
        printf("cannot get filesize: %s\n", curl_easy_strerror(res));
        goto end;
    }

    curl_easy_cleanup(curl_handle);
    return true;
end:
    curl_easy_cleanup(curl_handle);
    return false;
}

/**
 * @description: 获取文件大小，单位字节
 * @return {file_size_t} 返回字节数
 */
file_size_t HttpDownloader::GetFileSize() {
    return (file_size_t)m_filesize;
}

/**
 * @description: 判断下载网站是否支持断点续传
 * @return {bool}
 */
bool HttpDownloader::IsRangeAvailable() {
    return m_range_supported;
}
