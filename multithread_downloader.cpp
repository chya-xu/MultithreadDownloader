/*
 * @Author: xuqiaxin
 * @Mail: qiaxin.xu@foxmail.com
 * @Date: 2022-11-15 23:57:39
 * @Description: 多线程文件下载管理器实现
 */

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <future>
#include <iostream>
#include <getopt.h>
#include <math.h>
#include "multithread_downloader.h"
#include "version.h"

 /**
  * @description: 获取文件下载器
  * @param {DownloaderType} type 下载器类型
  * @return {Downloader*} 下载器对象
  */
Downloader* GetDownloader(DownloaderType type) {
    if (type == HTTP) {
        return new HttpDownloader();
    }
    return nullptr;
}

/**
 * @description: 转换字节大小
 * @param {int} size 原大小
 * @param {string&} unit 转换后单位
 * @return {int} 转换后大小
 */
int ConvertSize(int size, string& unit) {
    vector<string> all_units = {"KB", "MB", "GB", "TB"};
    unit = "B";
    for (auto &one_unit : all_units) {
        if (size > BYTE_SCALE) {
            size = (double)size / BYTE_SCALE;
            unit = one_unit;
        }
        else {            
            return size;
        }
    }
    return size;
}


/**
 * @description: 解除内存映射
 * @return {bool} 成功返回true， 失败返回false
 */
bool DownloadManager::ReleaseMem() {
    bool flag = true;
    for (auto& mem : m_mems) {
        if (mem != nullptr && -1 == munmap(mem, BLOCK_4K)) {
            perror("unmap failed");
            flag = false;
        }
    }
    return flag;
}

DownloadManager::~DownloadManager() {
    if (m_downloader) {
        delete m_downloader;
    }
    if (m_w_fd != -1) {
        close(m_w_fd);
        m_w_fd = -1;
    }
    ReleaseMem();
}

/**
 * @description: 创建空文件
 * @return {bool} 成功返回true， 失败返回false
 */
bool DownloadManager::CreateEmptyFile() {
    // To do 添加递归创建文件夹逻辑

    string file_full_name = m_file_save_path + "/" + m_filename;
    m_w_fd = open(file_full_name.c_str(), O_CREAT | O_RDWR | O_TRUNC, 00777);
    if (!m_w_fd) {
        printf("create file(%s) failed\n", file_full_name.c_str());
        return false;
    }
    if (m_filesize > 0 && -1 == lseek(m_w_fd, m_filesize - 1, SEEK_SET)) {
        perror("lseek error:");
        return false;
    }
    write(m_w_fd, "", 1);

    return true;
}

/**
 * @description: 初始化下载管理器
 * @param {const DownloadInfo&} info 下载信息
 * @param {const string&} save_path 文件存放位置
 * @return {bool} 成功返回true， 失败返回false
 */
bool DownloadManager::Init(const DownloadInfo& info, const string& save_path) {
    m_downloader = GetDownloader(info.type);
    if (!m_downloader) {
        return false;
    }
    if (!m_downloader->Init(info.url)) {
        printf("downloader init error");
        return false;
    }

    m_filename = info.url.substr(info.url.find_last_of('/') + 1);
    printf("filename is %s\n", m_filename.c_str());
    m_url = info.url;
    m_file_save_path = save_path;

    // 服务器不支持多线程下载，自动转换为单线程
    if (!m_downloader->IsRangeAvailable()) {
        printf("multi-thread downloading is not supported, adjust to single-thread\n");
        m_thread_num = 1;
    }

    m_filesize = m_downloader->GetFileSize();
    printf("file size: %lu\n", m_filesize);

    // 创建对应大小空文件
    return CreateEmptyFile();
}

/**
 * @description: 执行下载
 * @return {bool} 成功返回true， 失败返回false
 */
bool DownloadManager::Download() {
    if (m_filesize == 0) {
        return true;
    }

    // 计算文件大小可以分成的块数(每块4k)
    int num_of_4k_block = (int)ceil((double)m_filesize / BLOCK_4K);
    if (num_of_4k_block < m_thread_num) {
        m_thread_num = num_of_4k_block;
        printf("due to small file size, auto adjust thread num to %d\n", num_of_4k_block);
    }

    // 创建线程，分配下载的文件片段
    int base_blk_num = num_of_4k_block / m_thread_num;
    int remain_blk_num = num_of_4k_block - (base_blk_num * m_thread_num);
    file_size_t last_pos = 0;
    int last_blk = 0;


    // 分配各线程要处理的数据位置
    for (int i = 1; i < m_thread_num; i++) {
        int block_num = base_blk_num;
        if (remain_blk_num) {
            block_num++;
            remain_blk_num--;
        }
        m_block_idxs[i] = last_blk;

        // 创建回调函数，记录线程序号
        DataDealCallback callback = [this, i](const char* data, size_t size)->bool {
            return WriteFileBulkCallback(data, size, i);
        };
        m_threads.emplace_back(std::async(std::launch::async, &Downloader::Download, m_downloader, last_pos,
            last_pos + block_num * BLOCK_4K - 1, callback));
        last_pos += block_num * BLOCK_4K;
        last_blk += block_num;
        m_remain_block_num[i] = block_num;
    }

    m_block_idxs[0] = last_blk;
    m_threads.emplace_back(std::async(std::launch::async, &Downloader::Download, m_downloader, last_pos, m_filesize - 1,
        [this](const char* data, size_t size)->bool {
            return WriteFileBulkCallback(data, size, 0);
        }));
    m_remain_block_num[0] = num_of_4k_block - last_blk;

    // 显示进度条
    if (!ShowProgress()) {
        return false;
    }

    // 刷新磁盘
    if (!ReleaseMem()) {
        return false;
    }

    string bar(100, '=');
    printf("[%-100s][%3d%%]\r\n", bar.c_str(), 100);
    return true;
}

/**
 * @description: 显示下载进度条
 * @return {bool} 下载成功返回true， 失败返回false
 */
bool DownloadManager::ShowProgress() {

    file_size_t total_size = 0;
    file_size_t last_size = 0;
    int undone_thread_num = m_thread_num; // 未执行完线程数
    int wait_time = PROGRESS_INTERVAL / undone_thread_num;
    auto last_time = chrono::system_clock::now();
    int speed = 0;
    string speed_size = "KB";

    while (undone_thread_num != 0) {
        // 计算已下载文件大小, 鉴于进度条只是粗略估算，此处不加锁
        total_size = 0;
        for (auto& size : m_downloaded_sizes) {
            total_size += size;
        }

        // 显示进度条
        int progress = (int)(total_size * 100 / m_filesize);
        string bar(progress, '=');
        int diff_time = chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - last_time).count();
        if (diff_time > 1) {
            file_size_t diff_size = total_size - last_size;
            speed = ConvertSize((double)diff_size / diff_time, speed_size);
        }
        printf("[%-100s][%3d%%][%3d%s/s]\r", bar.c_str(), progress, speed, speed_size.c_str());
        fflush(stdout);

        // 检测线程是否执行完毕
        std::vector<std::future<bool>>::iterator it = m_threads.begin();
        for (; it != m_threads.end(); ) {
            std::future_status status = it->wait_for(chrono::milliseconds(wait_time));
            if (status != std::future_status::ready) {
                ++it;
                continue;
            }

            // 有线程执行失败，直接返回
            if (!it->get()) {
                return false;
            }

            // 执行成功，则从集合中移除
            it = m_threads.erase(it);
            if (--undone_thread_num) {
                wait_time = PROGRESS_INTERVAL / undone_thread_num;
            }
        }
    }
    return true;
}

/**
 * @description: 接收数据并执行写入行为的回调函数
 * @param {const char*} data 接收的数据
 * @param {size_t} size 数据大小
 * @param {const int} thread_id 线程序号
 * @return {bool} 成功返回true， 失败返回false
 */
bool DownloadManager::WriteFileBulkCallback(const char* data, size_t size, const int thread_id) {
    // 若未映射内存，则进行映射
    if (m_mems[thread_id] == nullptr && !MapToFile(thread_id)) {
        return false;
    }

    size_t remain_data_size = size; // 未写入数据大小
    size_t remain_mem_size = m_current_block_size[thread_id] - m_current_mem_pos[thread_id]; // 可写内存剩余大小
    size_t data_offset = 0; // 数据偏移量

    // 未写入数据量大于内存大小时，先写部分数据再重新映射空间
    while (remain_data_size > remain_mem_size) {
        memcpy(m_mems[thread_id] + m_current_mem_pos[thread_id], data + data_offset, remain_mem_size);
        remain_data_size -= remain_mem_size;
        data_offset += remain_mem_size;
        // 重新映射内存
        if (!MapToFile(thread_id)) {
            return false;
        }

        // 更新剩余空间值
        remain_mem_size = m_current_block_size[thread_id] - m_current_mem_pos[thread_id];
    }
    memcpy(m_mems[thread_id] + m_current_mem_pos[thread_id], data + data_offset, remain_data_size);
    m_current_mem_pos[thread_id] += remain_data_size;
    m_downloaded_sizes[thread_id] += size;
    return true;
}

/**
 * @description: 映射文件到内存
 * @param {const int} thread_id 线程序号
 * @return {bool} 成功返回true， 失败返回false
 */
bool DownloadManager::MapToFile(const int thread_id) {
    // 如果原来的地址有映射，要先刷盘
    if (m_mems[thread_id] != nullptr) {
        if (-1 == munmap(m_mems[thread_id], m_current_block_size[thread_id])) {
            perror("unmap failed:");
            printf("unmap failed, thread id is %d\n", thread_id);
            return false;
        }
    }
    int to_map_block_num = m_remain_block_num[thread_id] > m_map_page_num ? m_map_page_num : m_remain_block_num[thread_id];
    if (to_map_block_num <= 0) {
        printf("map failed, remain block num is 0, thread id is %d\n", thread_id);
        return false;
    }
    m_mems[thread_id] = (char*)mmap(0, BLOCK_4K * to_map_block_num, PROT_WRITE, MAP_SHARED, m_w_fd,
        m_block_idxs[thread_id] * BLOCK_4K);
    if (m_mems[thread_id] == MAP_FAILED) {
        m_mems[thread_id] = nullptr;
        perror("map failed:");
        printf("map failed, block_idx is %d, remain_block_num is, to_map_block_num is %d\n", m_block_idxs[thread_id],
            m_remain_block_num[thread_id], to_map_block_num);
        return false;
    }
    m_current_mem_pos[thread_id] = 0;
    m_block_idxs[thread_id] += to_map_block_num;
    m_remain_block_num[thread_id] -= to_map_block_num;
    m_current_block_size[thread_id] = to_map_block_num * BLOCK_4K;
    return true;
}


int main(int argc, char* argv[]) {
    int ch;
    string url;
    string path;
    int thread_num = 5;
    int map_page_num = 256;

    while ((ch = getopt(argc, argv, "t:u:d:p:hv")) != EOF) {
        switch (ch) {
        case 'u':
        {
            url.assign(optarg);
            break;
        }
        case 'd':
        {
            path.assign(optarg);
            cout << "filepath is  " << optarg << endl;
            break;
        }
        case 'h':
        {
            cout << "Usage:" << endl;
            cout << "-u * set URL" << endl;
            cout << "-d * set file path to save result" << endl;
            cout << "-h show this help" << endl;
            cout << "-t set thread num, default = 5" << endl;
            cout << "-s set map_page_num, default = 256" << endl;
            cout << "e.g. ./multithread_downloader -u "
                "http://mirrors.163.com/centos-vault/6.2/isos/x86_64/CentOS-6.2-x86_64-netinstall.iso -d /root/"
                << endl;
            return 0;
        }
        case 't':
        {
            thread_num = atoi(optarg);
            break;
        }
        case 'p':
        {
            map_page_num = atoi(optarg);
            break;
        }
        case 'v':
        {
            printf("version: %d.%d\n", MULTITHREAD_DOWNLOADER_VERSION_MAJOR, MULTITHREAD_DOWNLOADER_VERSION_MINOR);
            break;
        }
        default:
        {
            cout << "undefined option: -" << ch << endl;
        }
        }
    }
    if (url.empty() || path.empty()) {
        cout << "please insert url by -u, and output path by -d!!" << endl;
    }
    DownloadManager app(thread_num, map_page_num);
    DownloadInfo info(HTTP, url);
    if (!app.Init(info, path)) {
        cout << "error occur, please try again" << endl;
        return -1;
    }
    if (!app.Download()) {
        cout << "download failed, please try again" << endl;
        return -1;
    }
    return 0;
}
