#include <stdint.h>

class GameObject {
    public:
        uint64_t id;

    private:

};

template<typename T = GameObject>
struct ObjectHandle {
    uint64_t id;
};
