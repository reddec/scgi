//
// Created by Red Dec on 11.04.15.
//

#ifndef AUTH_PATTERNS_H
#define AUTH_PATTERNS_H

#include <mutex>
#include <queue>
#include <condition_variable>

namespace scgi {
    namespace patterns {

        /**
         * Concurrent blocking queue
         */
        template<class T>
        class BlockingQueue {

        public:

            /**
             * Push data to queue and notify threads
             */
            inline void push(const T &var) {
                std::unique_lock<std::mutex> lock(mutex);
                queue_.push(var);
                monitor.notify_one();
            }

            /**
             * Get (and pop) one element from queue. If queue is empty, threads will be waiting until queue is closed or
             * data pushed.
             * Returns false if queue is closed
             */
            inline bool pop(T &var) {
                while (true) {
                    std::unique_lock<std::mutex> lock(mutex);
                    if (finalized) break;
                    if (!queue_.empty()) {
                        var = queue_.front();
                        queue_.pop();
                        return true;
                    }
                    monitor.wait(lock);
                }
                return false;
            }

            /**
             * Queue state
             */
            inline bool is_finished() const {
                return finalized;
            }

            /**
             * Close queue and release all waiting threads
             */
            inline void kill() {
                std::unique_lock<std::mutex> lock(mutex);
                finalized = true;
                monitor.notify_all();
            }

            /**
             * Close queue
             */
            ~BlockingQueue() {
                kill();
            }

        private:
            std::queue<T> queue_;
            volatile bool finalized = false;
            std::condition_variable monitor;
            std::mutex mutex;
        };
    }
}
#endif //AUTH_PATTERNS_H
