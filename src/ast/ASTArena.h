#ifndef ASTARENA_H
#define ASTARENA_H

#include <vector>
#include <memory>
#include <functional>
#include <type_traits>
#include <utility>
#include <algorithm>

class ASTArena {
private:
    struct Chunk {
        size_t size;
        size_t used;
        char* data;
    };

    struct DtorOp {
        void (*dtor)(void*);
        void* ptr;
    };

    std::vector<Chunk> chunks;
    std::vector<DtorOp> destructors;
    size_t defaultChunkSize;

    void allocateChunk(size_t size) {
        chunks.push_back({size, 0, new char[size]});
    }

public:
    ASTArena(size_t chunkSize = 1024 * 1024) : defaultChunkSize(chunkSize) {
        allocateChunk(chunkSize);
    }

    ~ASTArena() {
        for (auto it = destructors.rbegin(); it != destructors.rend(); ++it) {
            it->dtor(it->ptr);
        }
        for (auto& chunk : chunks) {
            delete[] chunk.data;
        }
    }

    // Delete copy/move constructors
    ASTArena(const ASTArena&) = delete;
    ASTArena& operator=(const ASTArena&) = delete;

    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        size_t align = alignof(T);
        size_t size = sizeof(T);
        
        Chunk& current = chunks.back();
        size_t aligned_used = (current.used + align - 1) & ~(align - 1);
        
        if (aligned_used + size > current.size) {
            allocateChunk(std::max(defaultChunkSize, size));
            return allocate<T>(std::forward<Args>(args)...);
        }
        
        current.used = aligned_used + size;
        T* ptr = reinterpret_cast<T*>(current.data + aligned_used);
        new (ptr) T(std::forward<Args>(args)...);
        
        if constexpr (!std::is_trivially_destructible_v<T>) {
            destructors.push_back({ [](void* p) { static_cast<T*>(p)->~T(); }, ptr });
        }
        
        return ptr;
    }
};

#endif // ASTARENA_H
