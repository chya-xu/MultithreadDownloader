/*
 * @Author: xuqiaxin
 * @Mail: qiaxin.xu@foxmail.com
 * @Date: 2022-11-15 23:28:58
 * @Description: 多线程文件下载管理器
 */
#ifndef _MULTITHREAD_DOWNLOADER_H_
#include <string>
#include <functional>
#include <vector>
#include "httpdownloader.h"
using namespace std;

#define BLOCK_4K    4096
#define BYTE_SCALE  1024
#define PROGRESS_INTERVAL   3000 // 3000毫秒，用于控制进度条刷新时间
#define INT_DIVIDE(a, b)    ((int)((double)(a/b) + 0.5))

class DownloadManager {
public:
    DownloadManager(int thread_num = 5, int map_page_num = 256)
        : m_downloader(nullptr)
        , m_filesize(0)
        , m_thread_num(thread_num)
        , m_w_fd(-1)
        , m_map_page_num(map_page_num)
        , m_downloaded_sizes(thread_num, 0)
        , m_mems(thread_num, nullptr)
        , m_block_idxs(thread_num, 0)
        , m_remain_block_num(thread_num, 0)
        , m_current_block_size(thread_num, 0)
        , m_current_mem_pos(thread_num, 0) {};
    ~DownloadManager();

    /**
     * @description: 初始化下载管理器
     * @param {const DownloadInfo&} info 下载信息
     * @param {const string&} save_path 文件存放位置
     * @return {bool} 成功返回true， 失败返回false
     */
    bool Init(const DownloadInfo& info, const string& save_path);

    /**
     * @description: 执行下载
     * @return {bool} 成功返回true， 失败返回false
     */
    bool Download();

private:
    /**
     * @description: 接收数据并执行写入行为的回调函数
     * @param {const char*} data 接收的数据
     * @param {size_t} size 数据大小
     * @param {const int} thread_id 线程序号
     * @return {bool} 成功返回true， 失败返回false
     */
    bool WriteFileBulkCallback(const char* data, size_t size, const int thread_id);

    /**
     * @description: 映射文件到内存
     * @param {const int} thread_id 线程序号
     * @return {bool} 成功返回true， 失败返回false
     */
    bool MapToFile(const int thread_id);

    /**
     * @description: 解除内存映射
     * @return {bool} 成功返回true， 失败返回false
     */
    bool ReleaseMem();

    /**
     * @description: 创建空文件
     * @return {bool} 成功返回true， 失败返回false
     */
    bool CreateEmptyFile();

    /**
     * @description: 显示下载进度条
     * @return {bool} 下载成功返回true， 失败返回false
     */
    bool ShowProgress();

    Downloader* m_downloader; // 文件下载器
    string m_url; // 下载的文件链接
    string m_file_save_path; // 文件保存位置
    string m_filename; // 文件名
    file_size_t m_filesize; // 文件大小
    int m_thread_num; // 线程数量（包括主线程）
    int m_w_fd; // 打开的文件描述符
    int m_map_page_num; // 默认映射的页数
    std::vector<std::future<bool>> m_threads; // 线程future对象集合
    vector<file_size_t> m_downloaded_sizes; // 已下载的文件大小
    vector<char*> m_mems; // 各线程映射的内存地址
    vector<int> m_block_idxs; // 各线程当前映射的块位置
    vector<int> m_remain_block_num; // 未映射的块数
    vector<int> m_current_block_size; // 存放分配的块大小
    vector<size_t> m_current_mem_pos; // 当前映射内存使用的位置
};

#endif
