//
// Created by 稻草人 on 2022/8/7.
//

#ifndef SPMC_SHMKV_STORE_H
#define SPMC_SHMKV_STORE_H

#include "data.h"
#include "page.h"
#include "spdlog/spdlog.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

inline bool cpu_set_affinity(int cpu_id) {
#if defined __linux__
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);
    return 0 == sched_setaffinity(0, sizeof(mask), &mask);
#else
    return false;
#endif
}


inline int key_compare(const void *a, const void *b) {
    using namespace ots::skv;

    // left key, right item-element
    auto cmp = (skv_cmp *) a;
    auto item = (skv_item *) b;
    auto key = cmp->key;
    const size_t key_len = key->length + 1;

    if (key_len > item->key_len) {
        // move right
        SPDLOG_TRACE("left:{}, right :{}, offset:{}, length:{},  ret:{}", key->data,
                     cmp->key_address + item->key_offset, item->key_offset, item->key_len, 1);
        return 1;
    } else if (key_len < item->key_len) {
        //move left
        SPDLOG_TRACE("left:{}, right :{}, offset:{}, length:{},  ret:{}", key->data,
                     cmp->key_address + item->key_offset, item->key_offset, item->key_len, -1);
        return -1;
    } else {
        //注意这里，比较大小的位置设置
        auto ret = memcmp(key->data, cmp->key_address + item->key_offset, key->length);
        SPDLOG_TRACE("left:{}, right :{}, offset:{}, length:{},  ret:{}", key->data,
                     cmp->key_address + item->key_offset, item->key_offset, item->key_len, ret);
        return ret;
    }
}


namespace ots::skv {
    constexpr int KeyAverageSize = 64;
    class Store {
    private:
        // shm
        skv_header *skv_header_ = nullptr;

        skv_item *writer_item_ = nullptr;
        skv_item *reader_item_ = nullptr;
        skv_item *reader_item_buffer_ = nullptr;
        size_t *writer_item_used_ = nullptr;
        size_t *reader_item_used_ = nullptr;

        char *writer_key_ = nullptr;
        char *reader_key_ = nullptr;
        size_t *writer_key_used_ = nullptr;
        size_t *reader_key_used_ = nullptr;

        char *writer_value_ = nullptr;
        char *reader_value_ = nullptr;
        size_t *writer_value_used_ = nullptr;
        size_t *reader_value_used_ = nullptr;

        size_t *item_max_used_ = nullptr;
        size_t *key_max_used_ = nullptr;
        size_t *value_max_used_ = nullptr;

        skv_address skv_address_{};
        size_t cache_used_{};
        Page *page_ = nullptr;
        Buffer *reader_buffer_ = nullptr;

    private:
        int ResetKey(skv_data &key) {
            // memory space size allow
            size_t key_need = *writer_key_used_ + key.length + 1;
            if (key_need <= *key_max_used_) {
                SPDLOG_DEBUG("No need reset_key.");
                return 0;
            }

            // if still bigger than key_max_used return false
            key_need = key.length + 1;
            for (size_t i = 0; i < *writer_item_used_; i++) {
                key_need += (writer_item_ + i)->key_len;
            }
            if (key_need >= *key_max_used_) {
                SPDLOG_ERROR("Key buffer memory size error, key_max_used: {}, need: {}", *writer_key_used_, key_need);
                return -1;
            }

            SPDLOG_DEBUG("ResetKey");
            // reset_key
            char *key_from_ = nullptr;
            char *key_to_ = nullptr;
            skv_item *item = nullptr;
            if (skv_header_->key_id == 0) {
                key_from_ = skv_address_.key_a_address;
                key_to_ = skv_address_.key_b_address;
            } else {
                key_from_ = skv_address_.key_b_address;
                key_to_ = skv_address_.key_a_address;
            }

            size_t key_used = 0;
            for (size_t i = 0; i < *writer_item_used_; i++) {
                item = writer_item_ + i;
                memcpy(key_to_ + key_used, key_from_ + item->key_offset, item->key_len);
                item->key_offset = key_used;
                key_used += item->key_len;
            }

            // change key id
            skv_header_->key_id = skv_header_->key_id == 0 ? 1 : 0;
            if (skv_header_->key_id == 0) {
                writer_key_ = skv_address_.key_a_address;
                writer_key_used_ = &skv_header_->key_a_used;
            } else {
                writer_key_ = skv_address_.key_b_address;
                writer_key_used_ = &skv_header_->key_b_used;
            }
            *writer_key_used_ = key_used;
            SPDLOG_DEBUG("Reset key buffer.");
            return 0;
        }

        int ResetValue(skv_data &value) {
            // memory space size allow
            size_t value_need = *writer_value_used_ + value.length + 1;
            if (value_need <= *value_max_used_) {
                SPDLOG_DEBUG("No need reset_value.");
                return 0;
            }

            // if still bigger than value_max_used return false
            value_need = value.length + 1;
            for (size_t i = 0; i < *writer_item_used_; i++) {
                value_need += (writer_item_ + i)->value_len;
            }
            if (value_need >= *value_max_used_) {
                SPDLOG_ERROR("Value buffer memory size error, value_max_used: {}, need: {}", *writer_value_used_,
                             value_need);
                return -1;
            }

            SPDLOG_DEBUG("ResetValue");
            // reset_value
            char *value_from_ = nullptr;
            char *value_to_ = nullptr;
            skv_item *item = nullptr;
            if (skv_header_->value_id == 0) {
                value_from_ = skv_address_.value_a_address;
                value_to_ = skv_address_.value_b_address;
            } else {
                value_from_ = skv_address_.value_b_address;
                value_to_ = skv_address_.value_a_address;
            }

            size_t value_used = 0;
            for (size_t i = 0; i < *writer_item_used_; i++) {
                item = writer_item_ + i;
                memcpy(value_to_ + value_used, value_from_ + item->value_offset, item->value_len);
                item->value_offset = value_used;
                value_used += item->value_len;
            }

            // change value id
            skv_header_->value_id = skv_header_->value_id == 0 ? 1 : 0;
            if (skv_header_->value_id == 0) {
                writer_value_ = skv_address_.value_a_address;
                writer_value_used_ = &skv_header_->value_a_used;
            } else {
                writer_value_ = skv_address_.value_b_address;
                writer_value_used_ = &skv_header_->value_b_used;
            }
            *writer_value_used_ = value_used;

            SPDLOG_DEBUG("Reset value buffer.");
            return 0;
        }

        int AddItem(skv_data &key, skv_data &value) {
            if (*writer_item_used_ + 1 > *item_max_used_) {
                SPDLOG_ERROR("item_max_used_:{}, current_item_num:{}", *item_max_used_, *writer_item_used_);
                return -1;
            }

            if (ResetKey(key) != 0) {
                return -1;
            }
            if (ResetValue(value) != 0) {
                return -1;
            }

            // find the skv_address_ to insert new item
            skv_item *item = writer_item_;
            if (*writer_item_used_ > 0) {
                bool break_label = false;
                size_t i;
                for (i = 0; i < *writer_item_used_; i++) {
                    item = writer_item_ + i;
                    if (item->key_len > key.length + 1) {
                        break_label = true;
                        break;
                    } else if (item->key_len < key.length + 1) {
                    } else {
                        if (memcmp(writer_key_ + item->key_offset, key.data, key.length) > 0) {
                            break_label = true;
                            break;
                        }
                    }
                }

                //move bigger item to next item
                if (break_label) {
                    i--;
                    memcpy(writer_item_ + i + 1, writer_item_ + i, (*writer_item_used_ - i) * sizeof(skv_item));
                }
                // end place append
                else {
                    item += 1;
                }
            }
            item->key_len = key.length + 1;
            item->key_offset = *writer_key_used_;
            item->value_len = value.length + 1;
            item->value_offset = *writer_value_used_;

            memcpy(writer_key_ + item->key_offset, key.data, key.length);
            char *key_ = writer_key_ + item->key_offset + key.length;
            *key_ = '\0';

            memcpy(writer_value_ + item->value_offset, value.data, value.length);
            char *value_ = writer_value_ + item->value_offset + value.length;
            *value_ = '\0';

            *writer_item_used_ += 1;
            *writer_key_used_ += item->key_len;
            *writer_value_used_ += item->value_len;
            return 0;
        }

        int ReplaceItem(skv_item *item, skv_data &key, skv_data &value) {
            if (ResetValue(value) != 0) {
                return -1;
            }
            item->value_len = value.length + 1;
            item->value_offset = *writer_value_used_;

            memcpy(writer_value_ + item->value_offset, value.data, value.length);
            char *value_ = writer_value_ + item->value_offset + value.length;
            *value_ = '\0';

            *writer_value_used_ += item->value_len;
            return 0;
        }

    public:
        Store() = default;

        ~Store() {
            delete page_;
            delete reader_buffer_;
        }

        int Init(const std::string &file_path,
                 const size_t &item_num,
                 const size_t &value_size,
                 const bool &writer,
                 const bool &init_header,
                 const bool &init_disk,
                 const int &cpu_id = 1) {

            // cpu亲和力
            if (cpu_id > 0) {
                if (cpu_set_affinity(cpu_id)) {
                    SPDLOG_INFO("Set cpu_id {} successfully.", cpu_id);
                } else {
                    SPDLOG_WARN("Set cpu_id {} failed.", cpu_id);
                }
            }

            if (init_disk) {
                Page::RemoveFile(file_path);
            }

            // 约束最大内存空间，在不同系统泛用
            {
                size_t header_size = sizeof(skv_header);
                size_t item_size = sizeof(skv_item);
                size_t k_size = KeyAverageSize;
                size_t v_size = value_size + 1;
                size_t shm_size = header_size + (item_size + k_size + v_size * 2) * item_num * 2;
                SPDLOG_DEBUG("skv size: {}.", shm_size);
                if (shm_size > GB) {
                    SPDLOG_ERROR("shm-kv size no more than {}, params is {}.", GB, shm_size);
                    return -1;
                }
                // 64*1024
                if (item_num > 65536) {
                    SPDLOG_ERROR("shm-kv count-num no more than {}, params is {}.", 65536, item_num);
                    return -2;
                }
                if (value_size > 16 * MB) {
                    SPDLOG_ERROR("shm-kv value-size no more than {}, params is {}.", 16 * MB, value_size);
                    return -3;
                }

                page_ = new Page(file_path, writer, shm_size);
                if (!page_->GetShm()) {
                    SPDLOG_ERROR("Shared memory error!");
                    return -4;
                }

                reader_buffer_ = new Buffer(sizeof(skv_item) * 64 * 1024);
                if (!reader_buffer_->GetShm()) {
                    SPDLOG_ERROR("Buffer error!");
                    return -5;
                }
            }

            skv_header_ = (skv_header *) page_->GetShmDataAddress();
            reader_item_buffer_ = (skv_item *) reader_buffer_->GetShmDataAddress();
            if (init_header) {
                skv_header_->item_id = 0;
                skv_header_->item_a_used = 0;
                skv_header_->item_b_used = 0;
                skv_header_->key_id = 0;
                skv_header_->key_a_used = 0;
                skv_header_->key_b_used = 0;
                skv_header_->value_id = 0;
                skv_header_->value_a_used = 0;
                skv_header_->value_b_used = 0;
                skv_header_->reader_id = 0;
                skv_header_->item_max_used = item_num;
                skv_header_->key_max_used = KeyAverageSize * item_num;
                skv_header_->value_max_used = (value_size + 1) * item_num * 2;
                memset((char *) skv_header_ + sizeof(skv_header), 0, sizeof(skv_item) * item_num * 2);
            }
            skv_address_.item_a_address = (skv_item *) ((char *) skv_header_ + sizeof(skv_header));
            skv_address_.item_b_address = (skv_item *) skv_address_.item_a_address + skv_header_->item_max_used;
            skv_address_.key_a_address = (char *) ((skv_item *) skv_address_.item_b_address + skv_header_->item_max_used);
            skv_address_.key_b_address = (char *) skv_address_.key_a_address + skv_header_->key_max_used;
            skv_address_.value_a_address = (char *) skv_address_.key_b_address + skv_header_->key_max_used;
            skv_address_.value_b_address = (char *) skv_address_.value_a_address + skv_header_->value_max_used;

            if (skv_header_->item_max_used != item_num) {
                if (writer) {
                    SPDLOG_ERROR("Writer Params Error. {}-{}", item_num, skv_header_->item_max_used);
                    return -5;
                } else {
                    SPDLOG_WARN("Reader Params Different. {}-{}", item_num, skv_header_->item_max_used);
                }
            }

            //cache
            item_max_used_ = &skv_header_->item_max_used;
            key_max_used_ = &skv_header_->key_max_used;
            value_max_used_ = &skv_header_->value_max_used;
            return 0;
        }

        int Get(skv_data &key, skv_data &value) {
            const uint8_t reader_id = skv_header_->reader_id;
            if (reader_id == 0) {
                reader_item_ = skv_address_.item_a_address;
                reader_item_used_ = &skv_header_->item_a_used;
                reader_key_ = skv_address_.key_a_address;
                reader_key_used_ = &skv_header_->key_a_used;
                reader_value_ = skv_address_.value_a_address;
                reader_value_used_ = &skv_header_->value_a_used;
            } else if (reader_id == 1) {
                reader_item_ = skv_address_.item_a_address;
                reader_item_used_ = &skv_header_->item_a_used;
                reader_key_ = skv_address_.key_a_address;
                reader_key_used_ = &skv_header_->key_a_used;
                reader_value_ = skv_address_.value_b_address;
                reader_value_used_ = &skv_header_->value_b_used;
            } else if (reader_id == 2) {
                reader_item_ = skv_address_.item_a_address;
                reader_item_used_ = &skv_header_->item_a_used;
                reader_key_ = skv_address_.key_b_address;
                reader_key_used_ = &skv_header_->key_b_used;
                reader_value_ = skv_address_.value_a_address;
                reader_value_used_ = &skv_header_->value_a_used;
            } else if (reader_id == 3) {
                reader_item_ = skv_address_.item_a_address;
                reader_item_used_ = &skv_header_->item_a_used;
                reader_key_ = skv_address_.key_b_address;
                reader_key_used_ = &skv_header_->key_b_used;
                reader_value_ = skv_address_.value_b_address;
                reader_value_used_ = &skv_header_->value_b_used;
            } else if (reader_id == 4) {
                reader_item_ = skv_address_.item_b_address;
                reader_item_used_ = &skv_header_->item_b_used;
                reader_key_ = skv_address_.key_a_address;
                reader_key_used_ = &skv_header_->key_a_used;
                reader_value_ = skv_address_.value_a_address;
                reader_value_used_ = &skv_header_->value_a_used;
            } else if (reader_id == 5) {
                reader_item_ = skv_address_.item_b_address;
                reader_item_used_ = &skv_header_->item_b_used;
                reader_key_ = skv_address_.key_a_address;
                reader_key_used_ = &skv_header_->key_a_used;
                reader_value_ = skv_address_.value_b_address;
                reader_value_used_ = &skv_header_->value_b_used;
            } else if (reader_id == 6) {
                reader_item_ = skv_address_.item_b_address;
                reader_item_used_ = &skv_header_->item_b_used;
                reader_key_ = skv_address_.key_b_address;
                reader_key_used_ = &skv_header_->key_b_used;
                reader_value_ = skv_address_.value_a_address;
                reader_value_used_ = &skv_header_->value_a_used;
            } else if (reader_id == 7) {
                reader_item_ = skv_address_.item_b_address;
                reader_item_used_ = &skv_header_->item_b_used;
                reader_key_ = skv_address_.key_b_address;
                reader_key_used_ = &skv_header_->key_b_used;
                reader_value_ = skv_address_.value_b_address;
                reader_value_used_ = &skv_header_->value_b_used;
            } else {
                SPDLOG_ERROR("reader_id:{}", reader_id);
                return -1;
            }

            SPDLOG_DEBUG("reader_id:{}, reader_item_used:{}", reader_id, *reader_item_used_);

            // fix-bug, create snapshot
            cache_used_ = *reader_item_used_;
            memcpy(reader_item_buffer_, reader_item_, cache_used_ * sizeof(skv_item));

            skv_cmp cmp_{};// wrapper
            cmp_.key_address = reader_key_;
            cmp_.key = &key;
            auto b_ptr = bsearch(&cmp_, reader_item_buffer_, cache_used_, sizeof(skv_item), key_compare);
            SPDLOG_DEBUG("b-search b_ptr:{}", b_ptr);

            if (b_ptr == nullptr) {
                return -1;
            }

            auto *item = (skv_item *) b_ptr;
            SPDLOG_DEBUG("get, key_offset:{}, key_len:{}, value_offset:{}, value_len:{}",
                         item->key_offset, item->key_len, item->value_offset, item->value_len);
            value.length = item->value_len;
            value.data = reader_value_ + item->value_offset;
            return 0;
        }

        int Set(skv_data &key, skv_data &value) {
            // create item snapshot
            // current_id:0, writer_item B, reader_item a
            // current_id:1, writer_item A, reader_item b
            uint8_t writer_item_id;
            if (skv_header_->item_id == 0) {
                writer_item_ = skv_address_.item_b_address;
                reader_item_ = skv_address_.item_a_address;
                writer_item_used_ = &skv_header_->item_b_used;
                reader_item_used_ = &skv_header_->item_a_used;
                writer_item_id = 1;
            } else {
                writer_item_ = skv_address_.item_a_address;
                reader_item_ = skv_address_.item_b_address;
                writer_item_used_ = &skv_header_->item_a_used;
                reader_item_used_ = &skv_header_->item_b_used;
                writer_item_id = 0;
            }

            *writer_item_used_ = *reader_item_used_;
            memcpy(writer_item_, reader_item_, sizeof(skv_item) * (*writer_item_used_));
            SPDLOG_DEBUG("item_id:{}, writer_item_id:{}, writer_item_used:{}", skv_header_->item_id, writer_item_id,
                         *writer_item_used_);

            // key value pointer
            if (skv_header_->key_id == 0) {
                writer_key_ = skv_address_.key_a_address;
                writer_key_used_ = &skv_header_->key_a_used;
            } else {
                writer_key_ = skv_address_.key_b_address;
                writer_key_used_ = &skv_header_->key_b_used;
            }
            if (skv_header_->value_id == 0) {
                writer_value_ = skv_address_.value_a_address;
                writer_value_used_ = &skv_header_->value_a_used;
            } else {
                writer_value_ = skv_address_.value_b_address;
                writer_value_used_ = &skv_header_->value_b_used;
            }

            // b-search if exit replace else append
            skv_cmp cmp_{};// wrapper
            cmp_.key_address = writer_key_;
            cmp_.key = &key;
            auto b_ptr = bsearch(&cmp_, writer_item_, *writer_item_used_, sizeof(skv_item), key_compare);
            SPDLOG_DEBUG("b-search b_ptr:{}", b_ptr);

            int ret;
            if (b_ptr == nullptr) {
                SPDLOG_DEBUG("Append, key:{}, value:{}", key.data, value.data);
                ret = AddItem(key, value);
            } else {
                SPDLOG_DEBUG("Replace, key:{}, value:{}", key.data, value.data);
                ret = ReplaceItem((skv_item *) b_ptr, key, value);
            }

            if (ret == 0) {
                // change reader-item
                skv_header_->item_id = writer_item_id;
                if (skv_header_->item_id == 0 && skv_header_->key_id == 0 && skv_header_->value_id == 0) {
                    skv_header_->reader_id = 0;
                } else if (skv_header_->item_id == 0 && skv_header_->key_id == 0 && skv_header_->value_id == 1) {
                    skv_header_->reader_id = 1;
                } else if (skv_header_->item_id == 0 && skv_header_->key_id == 1 && skv_header_->value_id == 0) {
                    skv_header_->reader_id = 2;
                } else if (skv_header_->item_id == 0 && skv_header_->key_id == 1 && skv_header_->value_id == 1) {
                    skv_header_->reader_id = 3;
                } else if (skv_header_->item_id == 1 && skv_header_->key_id == 0 && skv_header_->value_id == 0) {
                    skv_header_->reader_id = 4;
                } else if (skv_header_->item_id == 1 && skv_header_->key_id == 0 && skv_header_->value_id == 1) {
                    skv_header_->reader_id = 5;
                } else if (skv_header_->item_id == 1 && skv_header_->key_id == 1 && skv_header_->value_id == 0) {
                    skv_header_->reader_id = 6;
                } else {
                    skv_header_->reader_id = 7;
                }
            }
            return ret;
        }

        int Del(skv_data &key) {
            // create item snapshot
            // current_id:0, writer_item B, reader_item a
            // current_id:1, writer_item A, reader_item b
            uint8_t writer_item_id;
            if (skv_header_->item_id == 0) {
                writer_item_ = skv_address_.item_b_address;
                reader_item_ = skv_address_.item_a_address;
                writer_item_used_ = &skv_header_->item_b_used;
                reader_item_used_ = &skv_header_->item_a_used;
                writer_item_id = 1;
            } else {
                writer_item_ = skv_address_.item_a_address;
                reader_item_ = skv_address_.item_b_address;
                writer_item_used_ = &skv_header_->item_a_used;
                reader_item_used_ = &skv_header_->item_b_used;
                writer_item_id = 0;
            }
            *writer_item_used_ = *reader_item_used_;
            memcpy(writer_item_, reader_item_, sizeof(skv_item) * (*writer_item_used_));
            SPDLOG_DEBUG("item_id:{}, writer_item_id:{}, writer_item_used:{}", skv_header_->item_id, writer_item_id,
                         *writer_item_used_);

            // key value pointer
            if (skv_header_->key_id == 0) {
                writer_key_ = skv_address_.key_a_address;
                writer_key_used_ = &skv_header_->key_a_used;
            } else {
                writer_key_ = skv_address_.key_b_address;
                writer_key_used_ = &skv_header_->key_b_used;
            }
            if (skv_header_->value_id == 0) {
                writer_value_ = skv_address_.value_a_address;
                writer_value_used_ = &skv_header_->value_a_used;
            } else {
                writer_value_ = skv_address_.value_b_address;
                writer_value_used_ = &skv_header_->value_b_used;
            }

            // b-search if exit replace else append
            skv_cmp cmp_{};// wrapper
            cmp_.key_address = writer_key_;
            cmp_.key = &key;
            auto b_ptr = bsearch(&cmp_, writer_item_, *writer_item_used_, sizeof(skv_item), key_compare);
            SPDLOG_DEBUG("b-search b_ptr:{}", b_ptr);

            if (b_ptr == nullptr) {
                SPDLOG_DEBUG("Not found key: {}-{}", key.length, key.data);
                return -1;
            } else {
                SPDLOG_DEBUG("Delete, key:{}, value:{}", key.length, key.data);
                auto *item = (skv_item *) b_ptr;
                // not the last item
                if (item < writer_item_ + *writer_item_used_ - 1) {
                    memcpy(item, item + 1, sizeof(skv_item) * (*writer_item_used_ + 1 - (item - writer_item_)));
                }
                // last item,just sub used_num
                *writer_item_used_ -= 1;

                skv_header_->item_id = writer_item_id;
                if (skv_header_->item_id == 0 && skv_header_->key_id == 0 && skv_header_->value_id == 0) {
                    skv_header_->reader_id = 0;
                } else if (skv_header_->item_id == 0 && skv_header_->key_id == 0 && skv_header_->value_id == 1) {
                    skv_header_->reader_id = 1;
                } else if (skv_header_->item_id == 0 && skv_header_->key_id == 1 && skv_header_->value_id == 0) {
                    skv_header_->reader_id = 2;
                } else if (skv_header_->item_id == 0 && skv_header_->key_id == 1 && skv_header_->value_id == 1) {
                    skv_header_->reader_id = 3;
                } else if (skv_header_->item_id == 1 && skv_header_->key_id == 0 && skv_header_->value_id == 0) {
                    skv_header_->reader_id = 4;
                } else if (skv_header_->item_id == 1 && skv_header_->key_id == 0 && skv_header_->value_id == 1) {
                    skv_header_->reader_id = 5;
                } else if (skv_header_->item_id == 1 && skv_header_->key_id == 1 && skv_header_->value_id == 0) {
                    skv_header_->reader_id = 6;
                } else {
                    skv_header_->reader_id = 7;
                }

                return 0;
            }
        }

        void Close() {
            page_->DetachShm();
        }

        std::vector<std::string> GetCurrentAllKeys() {
            std::vector<std::string> out;

            if (skv_header_->item_id == 0) {
                SPDLOG_INFO("item-a");
                for (int i = 0; i < skv_header_->item_a_used; i++) {
                    auto *item = (skv_item *) skv_address_.item_a_address + i;
                    auto *key_buffer =
                            skv_header_->key_id == 0 ? skv_address_.key_a_address : skv_address_.key_b_address;
                    char tmp[128] = {0};
                    memcpy(&tmp, key_buffer + item->key_offset, item->key_len);
                    std::string key_str(tmp);
                    out.push_back(key_str);
                }
            } else {
                SPDLOG_INFO("item-b");
                for (int i = 0; i < skv_header_->item_b_used; i++) {
                    auto *item = (skv_item *) skv_address_.item_b_address + i;
                    auto *key_buffer =
                            skv_header_->key_id == 0 ? skv_address_.key_a_address : skv_address_.key_b_address;
                    char tmp[128] = {0};
                    memcpy(&tmp, key_buffer + item->key_offset, item->key_len);
                    std::string key_str(tmp);
                    out.push_back(key_str);
                }
            }
            return out;
        }

    public:
        void ResetValue() {
            SPDLOG_INFO("ResetValue");
            ShowHeader();
            ShowAllKey();
            // reset_value
            char *value_from_ = nullptr;
            char *value_to_ = nullptr;
            skv_item *item = nullptr;
            if (skv_header_->value_id == 0) {
                value_from_ = skv_address_.value_a_address;
                value_to_ = skv_address_.value_b_address;
            } else {
                value_from_ = skv_address_.value_b_address;
                value_to_ = skv_address_.value_a_address;
            }

            *writer_value_used_ = 0;
            for (size_t i = 0; i < *writer_item_used_; i++) {
                item = writer_item_ + i;
                memcpy(value_to_ + *writer_value_used_, value_from_ + item->value_offset, item->value_len);
                item->value_offset = *writer_value_used_;
                *writer_value_used_ += item->value_len;
            }

            // change id
            skv_header_->value_id = skv_header_->value_id == 0 ? 1 : 0;
            // change value id
            skv_header_->value_id = skv_header_->value_id == 0 ? 1 : 0;
            if (skv_header_->value_id == 0) {
                writer_value_ = skv_address_.value_a_address;
                writer_value_used_ = &skv_header_->value_a_used;
            } else {
                writer_value_ = skv_address_.value_b_address;
                writer_value_used_ = &skv_header_->value_b_used;
            }
            SPDLOG_INFO("Reset value buffer.");
            ShowHeader();
            ShowAllKey();
        }

        void ShowHeader() {
            SPDLOG_INFO("skv_header-item_id:{}", skv_header_->item_id);
            SPDLOG_INFO("skv_header-key_id:{}", skv_header_->key_id);
            SPDLOG_INFO("skv_header-value_id:{}", skv_header_->value_id);
            SPDLOG_INFO("skv_header-item_a_used:{}", skv_header_->item_a_used);
            SPDLOG_INFO("skv_header-item_b_used:{}", skv_header_->item_b_used);
            SPDLOG_INFO("skv_header-key_a_used:{}", skv_header_->key_a_used);
            SPDLOG_INFO("skv_header-key_b_used:{}", skv_header_->key_b_used);
            SPDLOG_INFO("skv_header-value_a_used:{}", skv_header_->value_a_used);
            SPDLOG_INFO("skv_header-value_b_used:{}", skv_header_->value_b_used);
            SPDLOG_INFO("skv_header-item_max_used_:{}", skv_header_->item_max_used);
            SPDLOG_INFO("skv_header: {}", (void *) skv_header_);
            SPDLOG_INFO("skv_header-item_a_address: {}", (void *) skv_address_.item_a_address);
            SPDLOG_INFO("skv_header-item_b_address: {}", (void *) skv_address_.item_b_address);
            SPDLOG_INFO("skv_header-key_a_address: {}", (void *) skv_address_.key_a_address);
            SPDLOG_INFO("skv_header-key_b_address: {}", (void *) skv_address_.key_b_address);
            SPDLOG_INFO("skv_header-value_a_address: {}", (void *) skv_address_.value_a_address);
            SPDLOG_INFO("skv_header-value_b_address: {}", (void *) skv_address_.value_b_address);
        }

        void ShowCurrentKey() {
            if (skv_header_->item_id == 0) {
                SPDLOG_INFO("item-a");
                for (int i = 0; i < skv_header_->item_a_used; i++) {
                    auto *item = (skv_item *) skv_address_.item_a_address + i;
                    auto *key_buffer =
                            skv_header_->key_id == 0 ? skv_address_.key_a_address : skv_address_.key_b_address;
                    char tmp[128] = {0};
                    memcpy(&tmp, key_buffer + item->key_offset, item->key_len);
                    SPDLOG_INFO("item:{}, key:{}, key_offset:{}, key_len:{}, value_offset:{}, value_len:{}",
                                i, tmp, item->key_offset, item->key_len, item->value_offset, item->value_len);
                }
            } else {
                SPDLOG_INFO("item-b");
                for (int i = 0; i < skv_header_->item_b_used; i++) {
                    auto *item = (skv_item *) skv_address_.item_b_address + i;
                    auto *key_buffer =
                            skv_header_->key_id == 0 ? skv_address_.key_a_address : skv_address_.key_b_address;
                    char tmp[128] = {0};
                    memcpy(&tmp, key_buffer + item->key_offset, item->key_len);
                    SPDLOG_INFO("item:{}, key:{}, key_offset:{}, key_len:{}, value_offset:{}, value_len:{}",
                                i, tmp, item->key_offset, item->key_len, item->value_offset, item->value_len);
                }
            }
        }

        void ShowAllKey() {
            SPDLOG_INFO("item_id:{}, key_id:{}, value_id:{}", skv_header_->item_id, skv_header_->key_id, skv_header_->value_id);
            SPDLOG_INFO("item-a");
            for (int i = 0; i < skv_header_->item_a_used; i++) {
                auto *item = (skv_item *) skv_address_.item_a_address + i;
                auto *key_buffer = skv_header_->key_id == 0 ? skv_address_.key_a_address : skv_address_.key_b_address;
                char tmp[128] = {0};
                memcpy(&tmp, key_buffer + item->key_offset, item->key_len);
                SPDLOG_INFO("item:{}, key:{}, key_offset:{}, key_len:{}, value_offset:{}, value_len:{}",
                            i, tmp, item->key_offset, item->key_len, item->value_offset, item->value_len);
            }
            SPDLOG_INFO("item-b");
            for (int i = 0; i < skv_header_->item_b_used; i++) {
                auto *item = (skv_item *) skv_address_.item_b_address + i;
                auto *key_buffer = skv_header_->key_id == 0 ? skv_address_.key_a_address : skv_address_.key_b_address;
                char tmp[128] = {0};
                memcpy(&tmp, key_buffer + item->key_offset, item->key_len);
                SPDLOG_INFO("item:{}, key:{}, key_offset:{}, key_len:{}, value_offset:{}, value_len:{}",
                            i, tmp, item->key_offset, item->key_len, item->value_offset, item->value_len);
            }
        }
    };


}// namespace ots::skv

#endif//SPMC_SHMKV_STORE_H
