#include "logger.h"
#include "store.h"
#include <string>

struct TickData {
    char symbol[64];
    double price;
    int volume;
};


int main() {
    create_logger("test.log", "trace", false, false, false);

    ots::skv::Store store;
    store.Init("test.store", 32, sizeof(TickData), true, true, true);

    TickData *tick_ptr_ = nullptr;
    TickData tick_1{};
    tick_ptr_ = &tick_1;
    for (int j = 0; j < 20; j++) {
        SPDLOG_INFO("th, j:{}", j);
        for (int i = 0; i < 5; i++) {
            std::string key = "test" + std::to_string(i);

            ots::skv::skv_data skv_key{}, skv_value{};
            skv_key.data = tick_ptr_->symbol;
            skv_key.length = key.length();
            skv_value.data = (char *) tick_ptr_;
            skv_value.length = sizeof(TickData);
            tick_1.volume = j;
            strcpy(tick_1.symbol, key.c_str());

            ots::skv::skv_data out_data{};
            SPDLOG_INFO("j:{}, i:{}, key:{}, set:{}", j, i, key, store.Set(skv_key, skv_value));
            SPDLOG_DEBUG("get, i:{}, key:{}, set:{}", i, key, store.Get(skv_key, out_data));
            if (((TickData *) out_data.data)->volume != ((TickData *) skv_value.data)->volume) {
                SPDLOG_ERROR("set, i:{}, key:{}, vol:{}", i, key, ((TickData *) skv_value.data)->volume);
                SPDLOG_ERROR("get, i:{}, key:{}, vol:{}", i, key, ((TickData *) out_data.data)->volume);
                store.ShowAllKey();
                break;
            }
        }

        store.ShowAllKey();
        SPDLOG_INFO("\n");
    }

    SPDLOG_INFO("Show");
    store.ShowHeader();
    store.ShowAllKey();
    SPDLOG_INFO("\n");

    SPDLOG_INFO("Reset");
    store.ResetValue();
    store.ShowAllKey();
    SPDLOG_INFO("\n");


    return 0;
}
