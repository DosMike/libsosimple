#if !defined SOSIMPLE_WORKER_HPP
#define SOSIMPLE_WORKER_HPP

#include "sosimple/exports.hpp"

#include "sosimple/platforms.hpp"
#include <functional>
#include <chrono>
#include <optional>
#include <thread>

namespace sosimple {

class SOSIMPLE_API Worker {
public:
    /** tasks can return false to stop re-scheduling. returning true on a task that is not periodic does nothing. */
    using Task = std::function<bool()>;

public:
    virtual ~Worker() = default;

    // disable copy
    Worker(const Worker&) = delete;
    Worker(Worker&&) = default;
    Worker& operator=(const Worker&) = delete;
    Worker& operator=(Worker&&) = default;

protected:
    /** hide constructor for singleton */
    Worker(){}

public:

    /** process tasks once and return next process timepoint. This is mainly interesting if you dont want to block a thread. */
    static auto
    think() -> void;

    /**
     * thread main function that continuously processes tasks. use on main thread if you dont plan on doing anything else,
     * or with std::thread{sosimple::Worker::get().looper} otherwise.
     * This returns a lambda to avoid "member functions have to be called" issues
     */
    static auto
    looper() -> std::function<void()>;

    static auto
    stop() -> void;

    static auto
    isStarted() -> bool;

    static auto
    make_thread() -> std::thread
    { auto t = std::thread{looper()}; while(!isStarted()) std::this_thread::yield(); return t; }

    static auto
    queue(Task task) -> void;

    static auto
    queue(Task task, std::chrono::milliseconds interval) -> void;

};

}

#endif