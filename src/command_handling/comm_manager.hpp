/**
 * @file comm_manager.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <cstdint>
#include <array>
#include "estp.hpp"
#include "dispatcher.hpp"

namespace command_addapter {

template <uint16_t command_capacity, uint16_t rx_buffer_size = 1024>
class CommManager {
public:
    using Frame = m_estp::EstpFrame<>;

    CommManager() {};
    ~CommManager() = default;

    /**
     * @brief Initialize the communication manager.
     * @example comm_manager.init();
     * How to subscribe: comm_manager.subscribe<CommManager, &CommManager::handleLedCommand>(0x01, this);
     */
    void init() {}

    /**
     * @brief Process incoming data bytes.
     * @param data Pointer to the incoming data buffer.
     * @param len Length of the incoming data buffer.
     * @param response_code Reference to a response code variable to indicate processing result.
     * @example comm_manager.processIncomingData(data_buffer, buffer_length, response_code);
     */
    void processIncomingData(const uint8_t* data, const uint16_t len, uint8_t & response_code)
    {
        if (len < 1)
        {
            return; // Invalid data
        }

        if (data[0] == m_estp::Estp::SOF)
        {
            // Start of Frame detected
            // flush the buffer
            rx_index_ = 0;
            rx_buffer_.fill(0);
            // printf("Start of Frame detected\n");
        }

        for (uint16_t i = 0; i < len; i++)
        {
            // printf("Processing byte: %02X\n", data[i]);
            uint8_t byte = data[i];
            if (byte == m_estp::Estp::EOFRM)
            {
                // End of Frame detected
                // printf("End of Frame detected. Total bytes received: %u\n", rx_index_);
                if ((rx_index_ + 1) >= Frame::kMinRawFrameSize)
                {
                    // Process the received frame
                    Frame frame = m_estp::Estp::deserialize(rx_buffer_.data(), rx_index_);
                    uint16_t command_id = frame.getFrameId();
                    printf("Dispatchering command ID: 0x%04X\n", command_id);
                    const uint16_t data_length = frame.getLength();
                    printf("Data length: %u\n", data_length);
                    printf("Data: ");
                    for (uint16_t j = 0; j < data_length; j++)
                    {
                        printf("%02X ", frame.getData()[j]);
                    }
                    printf("\n");
                    dispatcher_.dispatch(command_id, frame.getData(), data_length, response_code);
                    rx_buffer_.fill(0);
                    if (response_code != 0)
                    {
                        printf("Command processing resulted in response code: %u\n", response_code);
                    }
                }
                rx_index_ = 0; // Reset for next frame
            }
            else
            {
                // printf("Storing byte: %02X at index %u\n", byte, rx_index_);
                // Store byte in buffer
                if (rx_index_ < rx_buffer_.size())
                {
                    rx_buffer_[rx_index_++] = byte;
                    // printf("Buffer index now: %u\n", rx_index_);
                }
                else
                {
                    // Buffer overflow, handle error as needed
                    rx_index_ = 0; // Reset buffer
                    rx_buffer_.fill(0);
                    // printf("Buffer overflow detected. Buffer reset.\n");
                }
            }
        }
    }

    /**
     * @brief Subscribe a command handler to a specific command ID.
     * @tparam T The class type of the handler.
     * @tparam Handler The member function pointer to the handler method.
     * @param id The command ID to subscribe to.
     * @param instance Pointer to the instance of the handler class.
     * @example comm_manager.subscribe<CommManager, &CommManager::handleLedCommand>(0x01, this);
     */
    template <typename T, void (T::*Handler)(const uint8_t*, const uint16_t, uint8_t &)>
    void subscribe(const uint16_t id, T* instance) noexcept
    {
        DispatcherBase& basedisp = dispatcher_;
        basedisp.subscribe<T, Handler>(id, instance);
    }

    /**
     * @brief Get a reference to the internal dispatcher.
     * @return Reference to the dispatcher.
     */
    Dispatcher<command_capacity>& getDispatcher()
    {
        return dispatcher_;
    }

private:
    Dispatcher<command_capacity> dispatcher_;
    std::array<uint8_t, rx_buffer_size> rx_buffer_{};
    uint16_t rx_index_{0};
    // Internal state and methods for communication management
    
};

} // namespace command_adapter