#include <atomic>
#include <cstring>

class LockFreeQueue {
public:
    LockFreeQueue(int capacity, int length)
        : m_capacity(capacity)
        , m_length(length)
        , m_head(0)
        , m_tail(0)
        , m_count(0) {
        m_data = new char[capacity * length];
    }

    ~LockFreeQueue() {
        delete[] m_data;
    }

    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    bool push(const char* data, int length) {
        if (length > m_length) {
            return false;
        }

        int tail = m_tail.load(std::memory_order_relaxed);
        int head = m_head.load(std::memory_order_acquire);
        int count = m_count.load(std::memory_order_acquire);

        if (count >= m_capacity) {
            return false; // queue is full
        }

        if ((tail + 1) % m_capacity == head) {
            // queue is full, discard new frame
            m_head.store((head + 1) % m_capacity, std::memory_order_release);
            m_count.fetch_sub(1, std::memory_order_release);
        }

        std::memcpy(m_data + tail * m_length, data, length);
        m_tail.store((tail + 1) % m_capacity, std::memory_order_release);
        m_count.fetch_add(1, std::memory_order_release);
        return true;
    }


    bool pop(char* data, int& length) {
        int head = m_head.load(std::memory_order_relaxed);
        int tail = m_tail.load(std::memory_order_acquire);
        int count = m_count.load(std::memory_order_acquire);

        if (count == 0 || head == tail) {
            return false; // queue is empty
        }

        std::memcpy(data, m_data + head * m_length, m_length);
        length = m_length;
        m_head.store((head + 1) % m_capacity, std::memory_order_release);
        m_count.fetch_sub(1, std::memory_order_release);
        return true;
    }

    const int capacity() const {
        return m_capacity;
    }

    const int length() const {
        return m_length;
    }

private:
    const int m_capacity;
    const int m_length; // length of each frame in bytes
    char* m_data;
    std::atomic<int> m_head;
    std::atomic<int> m_tail;
    std::atomic<int> m_count;
};
