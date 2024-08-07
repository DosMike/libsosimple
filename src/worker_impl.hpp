#if !defined SOSIMPLE_WORKER_IMPL_HPP
#define SOSIMPLE_WORKER_IMPL_HPP

#include <sosimple.hpp>
#include <thread>
#include <atomic>
#include <vector>
#include <functional>
#include <chrono>
#include <mutex>
#include <condition_variable>

namespace sosimple {

class WorkerImpl : public Worker {
public:
    /** tasks can return false to stop re-scheduling. returning true on a task that is not periodic does nothing. */
    using Task = std::function<bool()>;
private:
    enum class State {
        Ready, //can be started
        Running, //is executing tasks
        Stopping, //does not allow new tasks
    };
    std::atomic<State> state{State::Ready};
    struct TaskDescriptor {
        Task mTask;
        std::chrono::milliseconds mPeriod;
        std::chrono::time_point<std::chrono::steady_clock> mNextExec;
        bool mStale{false};

        /** There are some oddities with lazy loading library symbols. Make sure your task function is resolved by the loader before passing it in! */
        inline
        TaskDescriptor(Task task, std::chrono::milliseconds interval)
        : mTask{task}, mPeriod{interval}, mNextExec{std::chrono::steady_clock::now()} {}

        //make sortable
        inline auto
        operator<(const TaskDescriptor& other) -> bool
        {
            if (this->mPeriod == std::chrono::milliseconds(0) || other.mPeriod == std::chrono::milliseconds(0))
                return this->mPeriod < other.mPeriod; //whatever has no period goes first
            return this->mNextExec < other.mNextExec;
        }

        friend class WorkerImpl;
    };
    std::mutex mTaskMutex;
    std::vector<TaskDescriptor> mTasks;
    std::mutex mNotifyMutex;
    std::condition_variable mNotifier; //notify about new tasks

public:
    /// hide constructor for singleton
    WorkerImpl(){}

    ~WorkerImpl()
    override = default;

    /// process tasks once and return next process timepoint
    auto
    think_impl() -> std::optional<std::chrono::time_point<std::chrono::steady_clock>>;

    /// thread main function that continuously processes tasks.
    /// use on main thread if you dont plan on doing anything else,
    /// or with std::thread{sosimple::Worker::get().looper} otherwise.
    auto
    loop_impl() -> void;

    /// stop the worker, will finish pending tasks. main purpose is to signal
    /// the working thread to stop. after being stopped, you can restart the
    /// worker by re-entering looper()
    auto
    stop_impl() -> void;

    auto
    started_impl() -> bool;

    auto
    queue_impl(Task task) -> void;

    auto
    queue_impl(Task task, std::chrono::milliseconds interval) -> void;

    /** get the worker singleton. it's a singleton so other components can cueue more easily */
    static auto
    get() -> WorkerImpl&;
};

}

#endif