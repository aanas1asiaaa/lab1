#include <windows.h>
#include <atomic>
#include <chrono>
#include <climits>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

void setupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

void section(const char* title) {
    std::cout << "\n======== " << title << " ========\n";
}

namespace task111 {
void worker() {
    std::cout << "Потік виконується. Його ідентифікатор: "
              << std::this_thread::get_id() << "\n";
}

void run() {
    section("1.1.1. Запуск і join");
    std::cout << "Головний потік: " << std::this_thread::get_id() << "\n";

    std::thread workerThread(worker);
    std::cout << "Ідентифікатор об'єкта std::thread: " << workerThread.get_id() << "\n";
    std::cout << "joinable до join: " << std::boolalpha << workerThread.joinable() << "\n";
    workerThread.join();
    std::cout << "joinable після join: " << std::boolalpha << workerThread.joinable() << "\n";
    std::cout << "Головний потік дочекався завершення і продовжує роботу.\n";
}
}

namespace task112 {
std::mutex printMutex;

void printLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(printMutex);
    std::cout << line << "\n";
}

void backgroundWork() {
    for (int step = 1; step <= 3; ++step) {
        printLine("Фоновий потік, крок " + std::to_string(step));
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
    printLine("Фоновий потік завершився.");
}

void run() {
    section("1.1.2. detach");
    std::thread background(backgroundWork);
    printLine(std::string("joinable до detach: ") + (background.joinable() ? "true" : "false"));
    background.detach();
    printLine(std::string("joinable після detach: ") + (background.joinable() ? "true" : "false"));
    printLine("Головний потік не викликає join і йде далі.");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    printLine("Головний потік завершується.");
}
}

namespace task113 {
void byValue(int number, std::string label) {
    number += 10;
    label += " (копія)";
    std::cout << "У потоці за значенням: " << number << ", «" << label << "»\n";
}

void byReference(int& number) {
    number += 10;
    std::cout << "У потоці за посиланням: " << number << "\n";
}

void run() {
    section("1.1.3. Аргументи потоку");
    int copied = 5;
    std::string label = "початковий напис";
    std::thread valueThread(byValue, copied, label);
    valueThread.join();
    std::cout << "Після потоку оригінали не змінилися: " << copied
              << ", «" << label << "»\n";

    int shared = 5;
    std::thread referenceThread(byReference, std::ref(shared));
    referenceThread.join();
    std::cout << "Після потоку оригінал за std::ref: " << shared << "\n";
}
}

namespace task114 {
std::mutex printMutex;

class Greeter {
public:
    explicit Greeter(int index) : index(index) {}

    void operator()() const {
        std::lock_guard<std::mutex> lock(printMutex);
        std::cout << "Функтор №" << index
                  << ", потік " << std::this_thread::get_id() << "\n";
    }

private:
    int index;
};

void run() {
    section("1.1.4. Функтор і лямбда");
    Greeter greeter(1);
    std::thread functorThread(greeter);

    int captured = 41;
    std::thread lambdaThread([captured]() {
        std::lock_guard<std::mutex> lock(printMutex);
        std::cout << "Лямбда: " << captured + 1
                  << ", потік " << std::this_thread::get_id() << "\n";
    });

    functorThread.join();
    lambdaThread.join();
}
}

namespace task115 {
std::mutex printMutex;

std::thread spawn(int index) {
    return std::thread([index]() {
        std::lock_guard<std::mutex> lock(printMutex);
        std::cout << "Потік №" << index
                  << ", id " << std::this_thread::get_id() << "\n";
    });
}

void run() {
    section("1.1.5. Передання володіння");
    std::thread original = spawn(0);
    std::cout << "До переміщення original.joinable: "
              << std::boolalpha << original.joinable() << "\n";
    std::thread moved = std::move(original);
    std::cout << "Після переміщення original.joinable: " << original.joinable() << "\n";
    std::cout << "Після переміщення moved.joinable: " << moved.joinable() << "\n";
    moved.join();

    std::vector<std::thread> pool;
    for (int index = 1; index <= 4; ++index) {
        pool.push_back(spawn(index));
    }
    for (std::thread& worker : pool) {
        worker.join();
    }
    std::cout << "Усі чотири потоки з вектора приєднано.\n";
}
}

namespace task116 {
class ThreadGuard {
public:
    explicit ThreadGuard(std::thread& thread) : thread(thread) {}

    ~ThreadGuard() {
        if (thread.joinable()) {
            thread.join();
        }
    }

    ThreadGuard(const ThreadGuard&) = delete;
    ThreadGuard& operator=(const ThreadGuard&) = delete;

private:
    std::thread& thread;
};

void worker() {
    std::cout << "Потік допрацював до кінця.\n";
}

void run() {
    section("1.1.6. ThreadGuard");
    try {
        std::thread workerThread(worker);
        ThreadGuard guard(workerThread);
        throw std::runtime_error("збій у головному потоці");
    } catch (const std::exception& error) {
        std::cout << "Перехоплено: " << error.what() << "\n";
        std::cout << "Деструктор ThreadGuard уже викликав join.\n";
    }
}
}

namespace task117 {
constexpr long long kCount = 80000000;

long long sumRange(long long first, long long last) {
    long long sum = 0;
    for (long long value = first; value < last; ++value) {
        sum += value & 255;
    }
    return sum;
}

long long parallelSum(long long count, unsigned threadCount) {
    std::vector<long long> partial(threadCount, 0);
    std::vector<std::thread> workers;
    workers.reserve(threadCount - 1);

    const long long block = count / static_cast<long long>(threadCount);
    for (unsigned index = 0; index + 1 < threadCount; ++index) {
        const long long first = static_cast<long long>(index) * block;
        const long long last = first + block;
        workers.emplace_back([&partial, index, first, last]() {
            partial[index] = sumRange(first, last);
        });
    }

    const long long tailFirst = static_cast<long long>(threadCount - 1) * block;
    partial[threadCount - 1] = sumRange(tailFirst, count);

    for (std::thread& worker : workers) {
        worker.join();
    }

    long long total = 0;
    for (long long part : partial) {
        total += part;
    }
    return total;
}

void run() {
    section("1.1.7. hardware_concurrency");
    unsigned hardware = std::thread::hardware_concurrency();
    std::cout << "hardware_concurrency: " << hardware << "\n";
    if (hardware == 0) {
        hardware = 2;
    }

    const auto sequentialStart = std::chrono::steady_clock::now();
    const long long sequential = sumRange(0, kCount);
    const double sequentialMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - sequentialStart).count();

    const auto parallelStart = std::chrono::steady_clock::now();
    const long long parallel = parallelSum(kCount, hardware);
    const double parallelMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - parallelStart).count();

    std::cout.setf(std::ios::fixed);
    std::cout.precision(1);
    std::cout << "Елементів: " << kCount << "\n";
    std::cout << "Послідовна сума: " << sequential << " за " << sequentialMs << " мс\n";
    std::cout << "Паралельна сума (" << hardware << " потоків): " << parallel
              << " за " << parallelMs << " мс\n";
    std::cout << "Суми збігаються: " << std::boolalpha << (sequential == parallel) << "\n";
}
}

namespace task118 {
constexpr int kIterations = 20000;

void addPlain(int& counter) {
    for (int step = 0; step < kIterations; ++step) {
        const int local = counter;
        std::this_thread::yield();
        counter = local + 1;
    }
}

void addGuarded(int& counter, std::mutex& mutex) {
    for (int step = 0; step < kIterations; ++step) {
        std::lock_guard<std::mutex> lock(mutex);
        const int local = counter;
        counter = local + 1;
    }
}

void run() {
    section("1.1.8. М'ютекс і стан гонки");
    const int expected = kIterations * 2;
    std::cout << "Очікувана сума двох потоків: " << expected << "\n";

    int plain = 0;
    std::thread firstPlain(addPlain, std::ref(plain));
    std::thread secondPlain(addPlain, std::ref(plain));
    firstPlain.join();
    secondPlain.join();
    std::cout << "Без м'ютекса: " << plain << "\n";

    int guarded = 0;
    std::mutex mutex;
    std::thread firstGuarded(addGuarded, std::ref(guarded), std::ref(mutex));
    std::thread secondGuarded(addGuarded, std::ref(guarded), std::ref(mutex));
    firstGuarded.join();
    secondGuarded.join();
    std::cout << "Під lock_guard: " << guarded << "\n";
}
}

namespace task119 {
std::mutex printMutex;
std::mutex mutexA;
std::mutex mutexB;
std::atomic<int> armed{0};

void printLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(printMutex);
    std::cout << line << "\n";
}

void lockInOrder(const char* name, std::mutex& first, std::mutex& second) {
    first.lock();
    armed.fetch_add(1);
    while (armed.load() < 2) {
        std::this_thread::yield();
    }
    if (!second.try_lock()) {
        printLine(std::string(name) + " не отримав другий м'ютекс: взаємоблокування.");
        first.unlock();
        return;
    }
    second.unlock();
    first.unlock();
}

void lockBoth(const char* name) {
    std::lock(mutexA, mutexB);
    std::unique_lock<std::mutex> lockA(mutexA, std::adopt_lock);
    std::unique_lock<std::mutex> lockB(mutexB, std::adopt_lock);
    printLine(std::string(name) + " тримає A і B через std::lock.");
}

void run() {
    section("1.1.9. Взаємоблокування");
    armed.store(0);
    std::cout << "Порядок A->B проти B->A:\n";
    std::thread first(lockInOrder, "Потік 1", std::ref(mutexA), std::ref(mutexB));
    std::thread second(lockInOrder, "Потік 2", std::ref(mutexB), std::ref(mutexA));
    first.join();
    second.join();

    std::cout << "Безпечний порядок через std::lock:\n";
    std::thread third(lockBoth, "Потік 3");
    std::thread fourth(lockBoth, "Потік 4");
    third.join();
    fourth.join();
}
}

namespace task1110 {
class HierarchicalMutex {
public:
    explicit HierarchicalMutex(unsigned long level) : level(level) {}

    void lock() {
        if (thisThreadLevel <= level) {
            throw std::logic_error("порушено ієрархію м'ютексів");
        }
        internal.lock();
        previousLevel = thisThreadLevel;
        thisThreadLevel = level;
    }

    void unlock() {
        thisThreadLevel = previousLevel;
        internal.unlock();
    }

private:
    std::mutex internal;
    unsigned long level;
    unsigned long previousLevel = 0;
    static thread_local unsigned long thisThreadLevel;
};

thread_local unsigned long HierarchicalMutex::thisThreadLevel = ULONG_MAX;

std::recursive_mutex recursive;

void nested(int depth) {
    std::lock_guard<std::recursive_mutex> lock(recursive);
    std::cout << "Рекурсивний м'ютекс, глибина " << depth << "\n";
    if (depth < 3) {
        nested(depth + 1);
    }
}

void run() {
    section("1.1.10. Інші типи захоплення");

    std::mutex firstMutex;
    std::mutex secondMutex;
    std::unique_lock<std::mutex> first(firstMutex, std::defer_lock);
    std::unique_lock<std::mutex> second(secondMutex, std::defer_lock);
    std::lock(first, second);
    std::cout << "unique_lock: обидва м'ютекси захоплено разом.\n";
    first.unlock();
    std::cout << "unique_lock: перший м'ютекс відпущено раніше.\n";

    nested(1);

    HierarchicalMutex high(10000);
    HierarchicalMutex low(1000);
    {
        std::lock_guard<HierarchicalMutex> highLock(high);
        std::lock_guard<HierarchicalMutex> lowLock(low);
        std::cout << "Ієрархія: спочатку вищий рівень, потім нижчий.\n";
    }
    try {
        std::lock_guard<HierarchicalMutex> lowLock(low);
        std::lock_guard<HierarchicalMutex> highLock(high);
    } catch (const std::logic_error& error) {
        std::cout << "Ієрархія відхилила зворотний порядок: " << error.what() << "\n";
    }
}
}

int main() {
    setupConsole();
    std::cout << "Лабораторна робота №1. Управління потоками та розділення даних.\n";
    task111::run();
    task112::run();
    task113::run();
    task114::run();
    task115::run();
    task116::run();
    task117::run();
    task118::run();
    task119::run();
    task1110::run();
    std::cout << "\nУсі пункти 1.1.1–1.1.10 виконано.\n";
    return 0;
}
