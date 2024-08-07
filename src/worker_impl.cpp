#include <sosimple.hpp>
#include "worker_impl.hpp"

#include <set>
#include <iostream>

auto
sosimple::WorkerImpl::think_impl() -> std::optional<std::chrono::time_point<std::chrono::steady_clock>>
{
    auto zero = std::chrono::milliseconds(0);
    auto now = std::chrono::steady_clock::now();
    auto next = zero;
    bool nextSet{false};
    {
        std::unique_lock lock(mTaskMutex);
        for (auto& desc : mTasks) {
            if (desc.mPeriod == zero) {
                desc.mTask();
                desc.mStale = true;
            } else if (desc.mNextExec <= now) {
                if (!desc.mTask() || state != State::Running) {
                    desc.mStale = true;
                    continue;
                }
                if (desc.mPeriod > next) {
                    next = desc.mPeriod;
                    nextSet = true;
                }
            }
        }
        //remove stale tasks
        //mTasks.erase(std::remove_if(mTasks.begin(), mTasks.end(), [](const auto& task){ return task.mStale; }), mTasks.end());
        std::erase_if(mTasks, [](const auto& task){ return task.mStale; });
    }
    if (nextSet) {
        return now+next;
    } else {
        return {};
    }
}

auto
sosimple::WorkerImpl::loop_impl() -> void
{
    if (state != State::Ready)
        throw std::runtime_error("Worker is not ready to be restarted");
    state = State::Running;

    // in case of exception, properly clean up
    on_exit finally([&](){
        mTasks.clear();
        state = State::Ready;
    });

    std::unique_lock lock{mNotifyMutex};
    while (state == State::Running || !mTasks.empty()) {
        auto next = think_impl();
        if (next) {
            mNotifier.wait_until(lock, *next);
        } else {
            std::this_thread::yield(); //always at least yield, even if we run empty
        }
    }
}

auto
sosimple::WorkerImpl::get() -> WorkerImpl&
{
    static WorkerImpl instance{};
    return instance;
}

auto
sosimple::WorkerImpl::stop_impl() -> void
{
    if (state == State::Running)
    { state = State::Stopping; }
}

auto
sosimple::WorkerImpl::started_impl() -> bool
{
    return (state != State::Ready);
}

auto
sosimple::WorkerImpl::queue_impl(Task task) -> void
{
    queue_impl(std::move(task), std::chrono::milliseconds(0));
}

auto
sosimple::WorkerImpl::queue_impl(Task task, std::chrono::milliseconds interval) -> void
{
    std::unique_lock lock(mTaskMutex);
    mTasks.push_back({std::move(task), interval});
    mNotifier.notify_all();
}

// static <-> instance glue

/** process tasks once and return next process timepoint. This is mainly interesting if you dont want to block a thread. */
auto
sosimple::Worker::think() -> void
{ WorkerImpl::get().think_impl(); }

/**
 * thread main function that continuously processes tasks. use on main thread if you dont plan on doing anything else,
 * or with std::thread{sosimple::Worker::get().looper} otherwise.
 * This returns a lambda to avoid "member functions have to be called" issues
 */
auto
sosimple::Worker::looper() -> std::function<void()>
{ return []{ WorkerImpl::get().loop_impl(); }; };

auto
sosimple::Worker::stop() -> void
{ WorkerImpl::get().stop_impl(); }

auto
sosimple::Worker::isStarted() -> bool
{ return WorkerImpl::get().started_impl(); }

auto
sosimple::Worker::queue(Task task) -> void
{ WorkerImpl::get().queue_impl(task); }

auto
sosimple::Worker::queue(Task task, std::chrono::milliseconds interval) -> void
{ WorkerImpl::get().queue_impl(task, interval); }