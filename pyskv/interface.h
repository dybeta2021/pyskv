//
// Created by 稻草人 on 2022/8/7.
//

#ifndef PYSHMKV_INTERFACE_H
#define PYSHMKV_INTERFACE_H


#include "logger.h"
#include "process_mutex.h"
#include "spdlog/spdlog.h"
#include "store.h"
#include <mutex>
#include <pybind11/pybind11.h>
#include <thread>

namespace py = pybind11;

class PySKV {
private:
    CProcessMutex *ptr_process_mutex = nullptr;
    std::mutex ptr_thread_lock;
    ots::skv::Store *store_ = nullptr;

// private:
//     static void remove_store(const std::string &path) {
//         //todo:添加判断文件是否存在
//         if (remove(const_cast<char *>(path.c_str())) == 0)
//             SPDLOG_INFO("Removed {} succeeded.", const_cast<char *>(path.c_str()));
//         else
//             SPDLOG_WARN("Removed {} failed.", const_cast<char *>(path.c_str()));
//     }

public:
    PySKV(const std::string &path,
          const size_t &count,
          const size_t &value_size,
          const bool &write_mode,
          const bool &init_header,
          const bool &init_disk,
          const std::string &log_path,
          const std::string &log_level,
          const bool &thread_lock = false,
          const bool &process_lock = false,
          const std::string &process_mutex = "process_mutex") {
        create_logger(log_path, log_level, false, false, false, 1, 1);

        // if (init_disk) {
        //     remove_store(path);
        // }

        store_ = new ots::skv::Store;
        if (store_->Init(path, count, value_size, write_mode, init_disk, init_disk, 1) != 0) {
            SPDLOG_ERROR("Store Init Failed.");
            exit(-1);
        }

        if (process_lock & thread_lock) {
            SPDLOG_DEBUG("error config, process_lock true, thread_lock true!");
            exit(-2);
        }

        if (process_lock) {
            ptr_process_mutex = new CProcessMutex(process_mutex.c_str());
        }
    }

    ~PySKV() {
        if (store_ != nullptr) {
            store_->Close();
            delete store_;
        }
    }

    bool Set(const std::string &key,
             const char *value,
             const size_t &value_len,
             const bool process_lock,
             const bool thread_lock) {
        if (store_ == nullptr) {
            SPDLOG_ERROR("no store.");
            return true;
        }

        ots::skv::skv_data skv_key{}, skv_value{};
        skv_key.length = key.length();
        skv_key.data = (char *) key.c_str();
        skv_value.length = value_len;
        skv_value.data = (char *) value;

        if (process_lock) {
            if (ptr_process_mutex == nullptr) {
                SPDLOG_ERROR("ptr_process_mutex null.");
                return true;
            }

            bool lock = ptr_process_mutex->Lock();
            if (!lock) {
                SPDLOG_ERROR("ptr_process_mutex lock error.");
                return true;
            }

            bool ret = store_->Set(skv_key, skv_value) != 0;
            lock = ptr_process_mutex->UnLock();
            return ret;
        } else if (thread_lock) {
            ptr_thread_lock.lock();
            bool ret = store_->Set(skv_key, skv_value) != 0;
            ptr_thread_lock.unlock();
            return ret;
        } else {
            if (store_->Set(skv_key, skv_value) != 0) {
                return true;
            }
            return false;
        }
    };

    bool Del(const std::string &key,
             const bool process_lock,
             const bool thread_lock) {
        if (store_ == nullptr) {
            SPDLOG_ERROR("no store.");
            return true;
        }

        ots::skv::skv_data skv_key{};
        skv_key.length = key.length();
        skv_key.data = (char *) key.c_str();

        if (process_lock) {
            if (ptr_process_mutex == nullptr) {
                SPDLOG_ERROR("ptr_process_mutex null.");
                return true;
            }

            bool lock = ptr_process_mutex->Lock();
            if (!lock) {
                SPDLOG_ERROR("ptr_process_mutex lock error.");
                return true;
            }

            bool ret = store_->Del(skv_key) != 0;
            lock = ptr_process_mutex->UnLock();
            return ret;
        } else if (thread_lock) {
            ptr_thread_lock.lock();
            bool ret = store_->Del(skv_key) != 0;
            ptr_thread_lock.unlock();
            return ret;
        } else {
            if (store_->Del(skv_key) != 0) {
                return true;
            }
            return false;
        }
    };

    py::tuple Get(const std::string &key) {
        if (store_ == nullptr) {
            SPDLOG_ERROR("no store.");
            return py::make_tuple(true, "null");
        }

        ots::skv::skv_data skv_key{}, skv_value{};
        skv_key.length = key.length();
        skv_key.data = (char *) key.c_str();
        bool ret = store_->Get(skv_key, skv_value);
        if (ret) {
            return py::make_tuple(true, "null");
        }

        auto str = py::bytes(skv_value.data, skv_value.length);
        return py::make_tuple(false, str);
    };

    py::list GetCurrentAllKeys() {
        auto all_keys = store_->GetCurrentAllKeys();
        py::list out;
        for (const auto &item: all_keys) {
            auto str = py::bytes(item);
            out.append(str);
        }
        return out;
    }

public:
    void ShowHeader() {
        store_->ShowHeader();
    }

    void ShowCurrentKey() {
        store_->ShowCurrentKey();
    }

    void ShowAllKey() {
        store_->ShowAllKey();
    }
};

#endif//PYSHMKV_INTERFACE_H
