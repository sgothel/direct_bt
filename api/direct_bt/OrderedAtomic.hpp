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

#ifndef ORDERED_ATOMIC_HPP_
#define ORDERED_ATOMIC_HPP_

#include <atomic>
#include <memory>

namespace direct_bt {

#ifndef CXX_ALWAYS_INLINE
# define CXX_ALWAYS_INLINE inline __attribute__((__always_inline__))
#endif

/**
 * std::atomic<T> type with predefined fixed std::memory_order,
 * not allowing changing the memory model on usage and applying the set order to all operator.
 * <p>
 * See also:
 * <pre>
 * - Sequentially Consistent (SC) ordering or SC-DRF (data race free) <https://en.cppreference.com/w/cpp/atomic/memory_order#Sequentially-consistent_ordering>
 * - std::memory_order <https://en.cppreference.com/w/cpp/atomic/memory_order>
 * </pre>
 * </p>
 */
template <typename _Tp, std::memory_order _MO> struct ordered_atomic : private std::atomic<_Tp> {
  private:
    typedef std::atomic<_Tp> super;
    
  public:
    ordered_atomic() noexcept = default;
    ~ordered_atomic() noexcept = default;
    ordered_atomic(const ordered_atomic&) = delete;
    ordered_atomic& operator=(const ordered_atomic&) = delete;
    ordered_atomic& operator=(const ordered_atomic&) volatile = delete;

    constexpr ordered_atomic(_Tp __i) noexcept
    : super(__i)
    { }

    CXX_ALWAYS_INLINE
    operator _Tp() const noexcept
    { return super::load(_MO); }

    CXX_ALWAYS_INLINE
    operator _Tp() const volatile noexcept
    { return super::load(_MO); }

    CXX_ALWAYS_INLINE
    _Tp operator=(_Tp __i) noexcept
    { super::store(__i, _MO); return __i; }

    CXX_ALWAYS_INLINE
    _Tp operator=(_Tp __i) volatile noexcept
    { super::store(__i, _MO); return __i; }

    CXX_ALWAYS_INLINE
    _Tp operator++(int) noexcept // postfix ++
    { return super::fetch_add(1, _MO); }

    CXX_ALWAYS_INLINE
    _Tp operator++(int) volatile noexcept // postfix ++
    { return super::fetch_add(1, _MO); }

    CXX_ALWAYS_INLINE
    _Tp operator--(int) noexcept // postfix --
    { return super::fetch_sub(1, _MO); }

    CXX_ALWAYS_INLINE
    _Tp operator--(int) volatile noexcept // postfix --
    { return super::fetch_sub(1, _MO); }

#if 0 /* def _GLIBCXX_ATOMIC_BASE_H */

    // prefix ++, -- impossible w/o using GCC __atomic builtins and access to _M_i .. etc

    CXX_ALWAYS_INLINE
    _Tp operator++() noexcept // prefix ++
    { return __atomic_add_fetch(&_M_i, 1, int(_MO)); }

    CXX_ALWAYS_INLINE
    _Tp operator++() volatile noexcept // prefix ++
    { return __atomic_add_fetch(&_M_i, 1, int(_MO)); }

    CXX_ALWAYS_INLINE
    _Tp operator--() noexcept // prefix --
    { return __atomic_sub_fetch(&_M_i, 1, int(_MO)); }

    CXX_ALWAYS_INLINE
    _Tp operator--() volatile noexcept // prefix --
    { return __atomic_sub_fetch(&_M_i, 1, int(_MO)); }

#endif /* 0 _GLIBCXX_ATOMIC_BASE_H */

    CXX_ALWAYS_INLINE
    bool is_lock_free() const noexcept 
    { return super::is_lock_free(); }

    CXX_ALWAYS_INLINE
    bool is_lock_free() const volatile noexcept
    { return super::is_lock_free(); }

    static constexpr bool is_always_lock_free = super::is_always_lock_free;

    CXX_ALWAYS_INLINE
    void store(_Tp __i) noexcept
    { super::store(__i, _MO); }

    CXX_ALWAYS_INLINE
    void store(_Tp __i) volatile noexcept
    { super::store(__i, _MO); }

    CXX_ALWAYS_INLINE
    _Tp load() const noexcept
    { return super::load(_MO); }

    CXX_ALWAYS_INLINE
    _Tp load() const volatile noexcept
    { return super::load(_MO); }

    CXX_ALWAYS_INLINE
    _Tp exchange(_Tp __i) noexcept
    { return super::exchange(__i, _MO); }

    CXX_ALWAYS_INLINE
    _Tp exchange(_Tp __i) volatile noexcept
    { return super::exchange(__i, _MO); }

    CXX_ALWAYS_INLINE
    bool compare_exchange_weak(_Tp& __e, _Tp __i) noexcept
    { return super::compare_exchange_weak(__e, __i, _MO); }

    CXX_ALWAYS_INLINE
    bool compare_exchange_weak(_Tp& __e, _Tp __i) volatile noexcept
    { return super::compare_exchange_weak(__e, __i, _MO); }

    CXX_ALWAYS_INLINE
    bool compare_exchange_strong(_Tp& __e, _Tp __i) noexcept
    { return super::compare_exchange_strong(__e, __i, _MO); }

    CXX_ALWAYS_INLINE
    bool compare_exchange_strong(_Tp& __e, _Tp __i) volatile noexcept
    { return super::compare_exchange_strong(__e, __i, _MO); }

    CXX_ALWAYS_INLINE
    _Tp fetch_add(_Tp __i) noexcept
    { return super::fetch_add(__i, _MO); }

    CXX_ALWAYS_INLINE
    _Tp fetch_add(_Tp __i) volatile noexcept
    { return super::fetch_add(__i, _MO); }

    CXX_ALWAYS_INLINE
    _Tp fetch_sub(_Tp __i) noexcept
    { return super::fetch_sub(__i, _MO); }

    CXX_ALWAYS_INLINE
    _Tp fetch_sub(_Tp __i) volatile noexcept
    { return super::fetch_sub(__i, _MO); }

    CXX_ALWAYS_INLINE
    _Tp fetch_and(_Tp __i) noexcept
    { return super::fetch_and(__i, _MO); }

    CXX_ALWAYS_INLINE
    _Tp fetch_and(_Tp __i) volatile noexcept
    { return super::fetch_and(__i, _MO); }

    CXX_ALWAYS_INLINE
    _Tp fetch_or(_Tp __i) noexcept
    { return super::fetch_or(__i, _MO); }

    CXX_ALWAYS_INLINE
    _Tp fetch_or(_Tp __i) volatile noexcept
    { return super::fetch_or(__i, _MO); }

    CXX_ALWAYS_INLINE
    _Tp fetch_xor(_Tp __i) noexcept
    { return super::fetch_xor(__i, _MO); }

    CXX_ALWAYS_INLINE
    _Tp fetch_xor(_Tp __i) volatile noexcept
    { return super::fetch_xor(__i, _MO); }

  };

  /** SC atomic integral scalar integer. Memory-Model (MM) guaranteed sequential consistency (SC) between acquire (read) and release (write) */
  typedef ordered_atomic<int, std::memory_order::memory_order_seq_cst> sc_atomic_int;

  /** Relaxed non-SC atomic integral scalar integer. Memory-Model (MM) only guarantees the atomic value, _no_ sequential consistency (SC) between acquire (read) and release (write). */
  typedef ordered_atomic<int, std::memory_order::memory_order_relaxed> relaxed_atomic_int;

} /* namespace direct_bt */

#endif /* ORDERED_ATOMIC_HPP_ */
