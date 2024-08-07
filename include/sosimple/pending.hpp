#if !defined SOSIMPLE_PENDING_HPP
#define SOSIMPLE_PENDING_HPP

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <utility>

#include "sosimple/exports.hpp"

namespace sosimple {

/** a value wrapper that is conceptially similar to optional, but you can wait for a value without having to
 * create and manage the condition_variable yourself.
 * The value type T has to be moveable and default constructible (for the "empty" case).
 * To manipulate the value you have to retrieve() swap it out of the pending, resetting the pending to "no value"
 */
template<class T>
class SOSIMPLE_API pending {
    T mValue{};
    std::atomic_bool mHasValue{false};
    mutable std::mutex mValueMutex;
    mutable std::mutex mWaitMutex;
    mutable std::condition_variable mWaiter;

public:
    pending() {}
    ~pending() { mWaiter.notify_all(); /*unblock waiting threads*/ }

    /** move the value out of this pending instance
     * @returns value
     * @throws if has no value
     */
    auto retrieve() -> T
    {
        std::unique_lock lock(mValueMutex);
        if (!mHasValue) throw std::runtime_error("pending has no value");
        T outvalue{};
        std::swap(outvalue, mValue);
        mHasValue = false;
        return std::move(outvalue);
    }

    /**
     * @return true if has value
     */
    operator bool() const
    {
        std::unique_lock lock(mValueMutex);
        return mHasValue;
    }

    /**
     * @return a const reference to the contained value
     * @throws if has no value
     */
    auto operator->() const -> const T&
    {
        std::unique_lock lock(mValueMutex);
        if (!mHasValue) throw std::runtime_error("pending has no value");
        return mValue;
    }

    /**
     * sets a value to this pending and notifies threads waiting for a value
     * @param value the new value
     * @return pending<T>
     * @throws if has no value
     */
    auto operator=(T&& value) -> pending<T>&
    {
        std::unique_lock lock(mValueMutex);
        std::swap(mValue, value);
        mHasValue = true;
        mWaiter.notify_all();
        return *this;
    }

    /**
     * block until this pending has a value
     */
    auto wait() -> void
    {
        std::unique_lock lock(mWaitMutex);
        mWaiter.wait(lock);
    }
    /**
     * block until this pending has a value
     * @return cv_status indicating a timeout
     */
    template<typename Rep, typename Period>
    auto wait_for(std::chrono::duration<Rep,Period> time) -> std::cv_status
    {
        std::unique_lock lock(mWaitMutex);
        return mWaiter.wait_for<Rep,Period>(lock, time);
    }
    /**
     * block until this pending has a value
     * @return cv_status indicating a timeout
     */
    template<typename Rep, typename Period>
    auto wait_until(std::chrono::duration<Rep,Period> time) -> std::cv_status
    {
        std::unique_lock lock(mWaitMutex);
        return mWaiter.wait_until<Rep,Period>(lock, time);
    }

};

}

#endif