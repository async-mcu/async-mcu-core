#pragma once

/**
 * @file
 * @brief Definition of the Tick interface for periodic processing
 */

/**
 * @class async::Tick
 * @brief Abstract base class for objects requiring periodic processing
 * 
 * @details The Tick class defines an interface for objects that need to perform
 * regular updates or processing. It serves as the foundation for various timing
 * and scheduling components in the async framework.
 * 
 * Derived classes must implement the core tick() method and can optionally
 * override the control methods (start, pause, resume, cancel) to implement
 * specific behavior.
 * 
 * @note The actual tick frequency and timing depends on the scheduler implementation
 * 
 * @example Creating a custom tick-based component
 * @code
 * class MyComponent : public async::Tick {
 * public:
 *     bool tick() override {
 *         // Perform periodic updates
 *         updateSensors();
 *         processData();
 *         return true; // Continue ticking
 *     }
 *     
 *     bool start() override {
 *         // Custom initialization
 *         setupHardware();
 *     }
 * };
 * 
 * // Usage:
 * MyComponent comp;
 * executor.add(&comp);
 * @endcode
 */
namespace async { 
    class Tick {
    public:
        /**
         * @brief Process a single tick
         * @return bool True to continue receiving ticks, false to unsubscribe
         * 
         * @details This pure virtual method must be implemented by derived classes
         * to perform their periodic processing. The method is called at intervals
         * determined by the scheduler.
         * 
         * @note Returning false indicates the tickable should be removed from
         * the scheduler's update list.
         */
        virtual bool tick() = 0;

        /**
         * @brief Start the tickable object
         * @return bool True if successful, false otherwise
         * 
         * @note Default implementation simply returns true
         */
        virtual bool start() { return true; };

        /**
         * @brief Pause the tickable object
         * @return bool True if successful, false otherwise
         * 
         * @note Default implementation simply returns true
         * @note The object will stop receiving tick() calls while paused
         */
        virtual bool pause() { return true; };

        /**
         * @brief Resume a paused tickable object
         * @return bool True if successful, false otherwise
         * 
         * @note Default implementation simply returns true
         */
        virtual bool resume() { return true; };

        /**
         * @brief Cancel and terminate the tickable object
         * @return bool True if successful, false otherwise
         * 
         * @note Default implementation simply returns true
         * @note After cancellation, the object should be removed from scheduling
         */
        virtual bool cancel() { return true; };

        /**
         * @brief Virtual destructor
         */
        virtual ~Tick() = default;
    };
}