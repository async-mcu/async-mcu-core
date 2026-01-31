#pragma once

/**
 * @file Semaphore.h
 * @brief Defines the async::Semaphore class for managing resource access in asynchronous environments.
 */
namespace async {

    /**
     * @brief A simple counting semaphore for asynchronous resource management.
     *
     * The Semaphore class provides a mechanism to control access to a shared resource
     * by multiple tasks in an asynchronous environment. It supports acquiring and releasing
     * the semaphore, as well as querying the available count and lock status.
     */
    class Semaphore {
        private:
            int count;                ///< Current available count.
            const int maxCount;       ///< Maximum count allowed.
            volatile bool lock = false; ///< Indicates if the semaphore is currently locked.

        public:
            /**
             * @brief Construct a new Semaphore object.
             * @param initialCount Initial count value.
             * @param maximumCount Maximum count value.
             */
            Semaphore(int initialCount, int maximumCount);

            /**
             * @brief Try to acquire the semaphore.
             * @return true if acquired successfully, false otherwise.
             */
            bool tryAcquire();

            /**
             * @brief Check if the semaphore is currently locked.
             * @return true if locked, false otherwise.
             */
            bool isLock() const;

            /**
             * @brief Release the semaphore.
             *
             * Unlocks the semaphore and increments the count if below maxCount.
             */
            void release();

            /**
             * @brief Get the number of available resources.
             * @return Current available count.
             */
            int available() const;
    };
}