/**
 * @file Function.h
 * @brief Implementation of a type-erased function wrapper similar to std::function
 */

// https://stackoverflow.com/a/14741161/1261153

#pragma once

/**
 * @namespace function_private
 * @brief Internal implementation details for the Function template
 */
namespace function_private {
    /**
     * @brief Abstract base class for type-erased function storage
     * @tparam Result Return type of the function
     * @tparam Args Argument types of the function
     */
    template <typename Result, typename... Args>
    struct abstract_function {
        /**
         * @brief Function call operator (pure virtual)
         * @param args Arguments to pass to the stored function
         * @return Result of the function call
         */
        virtual Result operator()(Args... args) = 0;
        
        /**
         * @brief Clone the function object (pure virtual)
         * @return Pointer to a new copy of the function object
         */
        virtual abstract_function *clone() const = 0;
        
        /**
         * @brief Virtual destructor
         */
        virtual ~abstract_function() = default;
    };

    /**
     * @brief Concrete implementation of abstract_function for a specific functor type
     * @tparam Func Type of the callable object
     * @tparam Result Return type of the function
     * @tparam Args Argument types of the function
     */
    template <typename Func, typename Result, typename... Args>
    class concrete_function : public abstract_function<Result, Args...> {
        Func f; ///< The stored callable object

    public:
        /**
         * @brief Construct from a callable object
         * @param x The callable object to store
         */
        concrete_function(const Func &x) : f(x) {}
        
        /**
         * @brief Invoke the stored function
         * @param args Arguments to pass to the function
         * @return Result of the function call
         */
        Result operator()(Args... args) override { return f(args...); }
        
        /**
         * @brief Create a copy of this function wrapper
         * @return Pointer to a new concrete_function instance
         */
        concrete_function *clone() const override { return new concrete_function{f}; }
    };

    /**
     * @brief Helper to normalize function types
     * @tparam Func Type to normalize
     * 
     * @details Converts function signature types to function pointer types
     */
    template <typename Func>
    struct func_filter {
        typedef Func type; ///< Original type by default
    };
    
    /**
     * @brief Specialization for function signature types
     * @tparam Result Return type of the function
     * @tparam Args Argument types of the function
     */
    template <typename Result, typename... Args>
    struct func_filter<Result(Args...)> {
        typedef Result (*type)(Args...); ///< Function pointer type
    };

}  // namespace function_private

/**
 * @brief Forward declaration of Function template
 * @tparam signature Function signature (e.g., void(int, float))
 */
template <typename signature>
class Function;

/**
 * @brief Type-erased function wrapper similar to std::function
 * @tparam Result Return type of the function
 * @tparam Args Argument types of the function
 * 
 * @details This class provides a generic function wrapper that can store, copy, and invoke
 * any callable target (functions, lambda expressions, bind expressions, or other function objects).
 * It uses type erasure to handle different callable types through a common interface.
 */
template <typename Result, typename... Args>
class Function<Result(Args...)> {
    function_private::abstract_function<Result, Args...> *f; ///< Pointer to the stored function

public:
    /**
     * @brief Construct an empty Function object
     */
    Function() : f(nullptr) {}
    
    /**
     * @brief Construct from any callable object
     * @tparam Func Type of the callable object
     * @param x The callable object to store
     */
    template <typename Func>
    Function(const Func &x)
        : f(new function_private::concrete_function<
              typename function_private::func_filter<Func>::type, Result,
              Args...>(x)) {}
              
    /**
     * @brief Copy constructor
     * @param rhs Function object to copy
     */
    Function(const Function &rhs) : f(rhs.f ? rhs.f->clone() : nullptr) {}
    
    /**
     * @brief Copy assignment operator
     * @param rhs Function object to copy
     * @return Reference to this object
     */
    Function &operator=(const Function &rhs) {
        if ((&rhs != this) && (rhs.f)) {
            auto *temp = rhs.f->clone();
            delete f;
            f = temp;
        }
        return *this;
    }
    
    /**
     * @brief Assign a new callable object
     * @tparam Func Type of the callable object
     * @param x The callable object to store
     * @return Reference to this object
     */
    template <typename Func>
    Function &operator=(const Func &x) {
        auto *temp = new function_private::concrete_function<
            typename function_private::func_filter<Func>::type, Result, Args...>(x);
        delete f;
        f = temp;
        return *this;
    }
    
    /**
     * @brief Invoke the stored function
     * @param args Arguments to pass to the function
     * @return Result of the function call
     * 
     * @note Returns a default-constructed Result if no function is stored
     */
    Result operator()(Args... args) {
        if (f)
            return (*f)(args...);
        else
            return Result();
    }
    
    /**
     * @brief Destructor
     */
    ~Function() { delete f; }
};