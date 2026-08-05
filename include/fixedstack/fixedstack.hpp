#include <cstddef>
#include <cassert>

template<typename T, size_t N>
struct FixedStack {
    T data[N];
    size_t position = 0;

    void push(const T& v) {
        assert(position < N);
        data[position++] = v;
    }

    T pop() {
        assert(position > 0);
        return data[--position];
    }

    T& top(size_t spot = 1) {
        assert(spot > 0);
        assert(spot <= position);
        return data[position - spot];
    }

    bool empty() const { return position == 0; }
    size_t size() const { return position; }
};