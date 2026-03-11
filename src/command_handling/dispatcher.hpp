/**
 * @file dispatcher.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <cstdint>

namespace command_addapter {

// The signature for our "Bridge" functions
using CommandFunc = void (*)(void* context, const uint8_t* data, const uint16_t len, uint8_t & response_code);

class DispatcherBase {
protected:
    struct Entry {
        uint16_t id;
        CommandFunc func;
        void* context;
    };

    Entry * _table;
    uint16_t _count { 0 };
    uint16_t _capacity;

public:
    DispatcherBase(Entry* table, uint16_t capacity) noexcept
        : _table{table}, _capacity{capacity} {}
    ~DispatcherBase() = default;

    template <typename T, void (T::*Handler)(const uint8_t*, const uint16_t, uint8_t &)>
    void subscribe(uint16_t id, T* instance) noexcept
    {
        if (_count < _capacity)
        {
            auto bridge = [](void* ctx, const uint8_t* data, const uint16_t len, uint8_t & response_code)
            {
                (static_cast<T*>(ctx)->*Handler)(data, len, response_code);
            };
            _table[_count++] = {id, bridge, instance};
        }
    }
    // example: dispatcher.subscribe<Scoreboard, &Scoreboard::updateScore>(0x01, &myScoreboard);

    void dispatch(uint16_t id, const uint8_t* data, const uint16_t len, uint8_t & response_code) noexcept
    {
        if (_count == 0) return;

        // Sentinel slot is always at t_capacity index
        _table[_count].id = id;

        uint16_t i = 0;
        while (_table[i].id != id)
        {
            ++i;
        }

        if (i < _count)
        {
            _table[i].func(_table[i].context, data, len, response_code);
        }
    }
};

/**
 * @brief Fixed-capacity dispatcher.
 * This class provides a dispatcher with a compile-time defined capacity.
 */
template <uint16_t t_capacity>
class Dispatcher : public DispatcherBase {
private:
    Entry _storage[t_capacity + 1];
public:
    Dispatcher() : DispatcherBase(_storage, t_capacity) {}
};

} // namespace command_addapter