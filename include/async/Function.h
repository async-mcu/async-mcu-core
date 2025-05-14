#pragma once

// Базовый абстрактный класс для стирания типа
template<typename Result, typename... Args>
struct AbstractFunction {
    virtual Result operator()(Args... args) const = 0;
    virtual AbstractFunction* clone() const = 0;
    virtual ~AbstractFunction() {}
};

// Конкретная реализация для хранения функциональных объектов
template<typename Func, typename Result, typename... Args>
struct ConcreteFunction : AbstractFunction<Result, Args...> {
    Func f;
    
    ConcreteFunction(const Func& func) : f(func) {}
    ConcreteFunction(Func&& func) : f(static_cast<Func&&>(func)) {}
    
    Result operator()(Args... args) const override {
        return f(args...);
    }
    
    AbstractFunction<Result, Args...>* clone() const override {
        return new ConcreteFunction(f);
    }
};

// Основной класс Function
template<typename>
class Function;

template<typename Result, typename... Args>
class Function<Result(Args...)> {
    AbstractFunction<Result, Args...>* f;

public:
    Function() : f(0) {}
    
    Function(decltype(nullptr)) : f(0) {}  // Аналог std::nullptr_t
    
    template<typename Func>
    Function(const Func& func) : f(new ConcreteFunction<Func, Result, Args...>(func)) {}
    
    Function(const Function& other) : f(other.f ? other.f->clone() : 0) {}
    
    Function(Function&& other) : f(other.f) {
        other.f = 0;
    }
    
    ~Function() {
        delete f;
    }
    
    Result operator()(Args... args) const {
        if (!f) {
            // Простая обработка ошибки вместо исключения
            return Result();
        }
        return (*f)(args...);
    }
    
    Function& operator=(const Function& other) {
        if (this != &other) {
            delete f;
            f = other.f ? other.f->clone() : 0;
        }
        return *this;
    }
    
    Function& operator=(Function&& other) {
        if (this != &other) {
            delete f;
            f = other.f;
            other.f = 0;
        }
        return *this;
    }
    
    Function& operator=(decltype(nullptr)) {
        delete f;
        f = 0;
        return *this;
    }
    
    explicit operator bool() const {
        return f != 0;
    }
};