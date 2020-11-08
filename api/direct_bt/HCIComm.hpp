/*
 * Author: Sven Gothel <sgothel@jausoft.com>
 * Copyright (c) 2020 Gothel Software e.K.
 * Copyright (c) 2020 ZAFENA AB
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef HCI_COMM_HPP_
#define HCI_COMM_HPP_

#include <cstring>
#include <string>
#include <cstdint>
#include <memory>
#include <mutex>

#include "BTTypes.hpp"
#include "BTIoctl.hpp"
#include "HCIIoctl.hpp"
#include "HCITypes.hpp"

/**
 * - - - - - - - - - - - - - - -
 *
 * Module HCIComm:
 *
 * - BT Core Spec v5.2: Vol 4, Part E Host Controller Interface (HCI)
 */
namespace direct_bt {

    /**
     * Read/Write HCI communication channel.
     */
    class HCIComm {
        public:
            const uint16_t dev_id;
            const uint16_t channel;

        private:
            static int hci_open_dev(const uint16_t dev_id, const uint16_t channel) noexcept;
            static int hci_close_dev(int dd) noexcept;

            std::recursive_mutex mtx_write;
            std::atomic<int> socket_descriptor; // the hci socket
            std::atomic<bool> interrupt_flag; // for forced disconnect
            std::atomic<pthread_t> tid_read;

        public:
            /** Constructing a newly opened HCI communication channel instance */
            HCIComm(const uint16_t dev_id, const uint16_t channel) noexcept;

            HCIComm(const HCIComm&) = delete;
            void operator=(const HCIComm&) = delete;

            /**
             * Releases this instance after issuing {@link #close()}.
             */
            ~HCIComm() noexcept { close(); }

            /** Closing the HCI channel, locking {@link #mutex_write()}. */
            void close() noexcept;

            bool isOpen() const noexcept { return 0 <= socket_descriptor; }

            /** Return this HCI socket descriptor, for multithreading access use {@link #dd()}. */
            inline int getSocketDescriptor() const noexcept { return socket_descriptor; }

            /** Return the recursive write mutex for multithreading access. */
            inline std::recursive_mutex & mutex_write() noexcept { return mtx_write; }

            /** Generic read w/ own timeoutMS, w/o locking suitable for a unique ringbuffer sink. */
            jau::snsize_t read(uint8_t* buffer, const jau::nsize_t capacity, const int32_t timeoutMS) noexcept;

            /** Generic write, locking {@link #mutex_write()}. */
            jau::snsize_t write(const uint8_t* buffer, const jau::nsize_t size) noexcept;

        private:
            static inline void setu32_bit(int nr, void *addr) noexcept
            {
                *((uint32_t *) addr + (nr >> 5)) |= (1 << (nr & 31));
            }

            static inline void clearu32_bit(int nr, void *addr) noexcept
            {
                *((uint32_t *) addr + (nr >> 5)) &= ~(1 << (nr & 31));
            }

            static inline int testu32_bit(int nr, void *addr) noexcept
            {
                return *((uint32_t *) addr + (nr >> 5)) & (1 << (nr & 31));
            }

        public:
            static inline void filter_clear(hci_ufilter *f) noexcept
            {
                bzero(f, sizeof(*f));
            }
            static inline void filter_set_ptype(int t, hci_ufilter *f) noexcept
            {
                setu32_bit((t == HCI_VENDOR_PKT) ? 0 : (t & HCI_FLT_TYPE_BITS), &f->type_mask);
            }
            static inline void filter_clear_ptype(int t, hci_ufilter *f) noexcept
            {
                clearu32_bit((t == HCI_VENDOR_PKT) ? 0 : (t & HCI_FLT_TYPE_BITS), &f->type_mask);
            }
            static inline int filter_test_ptype(int t, hci_ufilter *f) noexcept
            {
                return testu32_bit((t == HCI_VENDOR_PKT) ? 0 : (t & HCI_FLT_TYPE_BITS), &f->type_mask);
            }
            static inline void filter_all_ptypes(hci_ufilter *f) noexcept
            {
                memset((void *) &f->type_mask, 0xff, sizeof(f->type_mask));
            }
            static inline void filter_set_event(int e, hci_ufilter *f) noexcept
            {
                setu32_bit((e & HCI_FLT_EVENT_BITS), &f->event_mask);
            }
            static inline void filter_clear_event(int e, hci_ufilter *f) noexcept
            {
                clearu32_bit((e & HCI_FLT_EVENT_BITS), &f->event_mask);
            }
            static inline int filter_test_event(int e, hci_ufilter *f) noexcept
            {
                return testu32_bit((e & HCI_FLT_EVENT_BITS), &f->event_mask);
            }
            static inline void filter_all_events(hci_ufilter *f) noexcept
            {
                memset((void *) f->event_mask, 0xff, sizeof(f->event_mask));
            }
            static inline void filter_set_opcode(uint16_t opcode, hci_ufilter *f) noexcept
            {
                f->opcode = opcode;
            }
            static inline void filter_clear_opcode(hci_ufilter *f) noexcept
            {
                f->opcode = 0;
            }
            static inline int filter_test_opcode(uint16_t opcode, hci_ufilter *f) noexcept
            {
                return (f->opcode == opcode);
            }
    };

} // namespace direct_bt

#endif /* HCI_COMM_HPP_ */
