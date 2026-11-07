#pragma warning(disable: 4996)
#pragma once

#include "Core/Types.hpp"
#include "Core/Logger.hpp"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

namespace RL::Memory
{

    class FrameAllocator
    {
    public:
        explicit FrameAllocator(usize capacityBytes)
            : m_buffer(std::make_unique<byte[]>(capacityBytes))
            , m_capacity(capacityBytes)
            , m_offset(0)
        {}

        [[nodiscard]] void* Allocate(usize sizeBytes, usize alignment = alignof(std::max_align_t))
        {
            const usize alignedOffset = AlignUp(m_offset, alignment);
            if (alignedOffset > m_capacity || sizeBytes > m_capacity - alignedOffset)
            {
                RL_LOG_ERROR("FrameAllocator: out of memory! Requested {}B, available {}B",
                    sizeBytes, (alignedOffset <= m_capacity) ? (m_capacity - alignedOffset) : 0);
                return nullptr;
            }
            void* ptr = m_buffer.get() + alignedOffset;
            m_offset  = alignedOffset + sizeBytes;
            return ptr;
        }

        template <typename T, typename... Args>
        [[nodiscard]] T* New(Args&&... args)
        {
            void* mem = Allocate(sizeof(T), alignof(T));
            if (!mem) return nullptr;
            return std::construct_at(static_cast<T*>(mem), std::forward<Args>(args)...);
        }

        void Reset() noexcept { m_offset = 0; }

        [[nodiscard]] usize GetUsed()     const noexcept { return m_offset;               }
        [[nodiscard]] usize GetCapacity() const noexcept { return m_capacity;              }
        [[nodiscard]] usize GetFree()     const noexcept { return m_capacity - m_offset;   }

    private:
        static usize AlignUp(usize value, usize align) noexcept
        {
            if (align == 0 || (align & (align - 1)) != 0) align = alignof(std::max_align_t);
            return (value + align - 1) & ~(align - 1);
        }

        std::unique_ptr<byte[]> m_buffer;
        usize                   m_capacity;
        usize                   m_offset;
    };

    template <typename T>
    class PoolAllocator
    {
    public:
        explicit PoolAllocator(usize capacity)
            : m_capacity(capacity)
            , m_allocatedSlots(capacity, false)
        {
            m_storage  = std::make_unique<Storage[]>(capacity);
            m_freeList.reserve(capacity);
            for (usize i = capacity; i-- > 0; )
            {
                m_freeList.push_back(i);
            }
        }

        template <typename... Args>
        [[nodiscard]] T* Allocate(Args&&... args)
        {
            if (m_freeList.empty())
            {
                RL_LOG_ERROR("PoolAllocator<{}>: pool exhausted (capacity={})",
                    typeid(T).name(), m_capacity);
                return nullptr;
            }
            const usize idx = m_freeList.back();
            m_freeList.pop_back();
            m_allocatedSlots[idx] = true;
            ++m_allocated;
            return std::construct_at(reinterpret_cast<T*>(&m_storage[idx]), std::forward<Args>(args)...);
        }

        void Free(T* ptr)
        {
            if (!ptr) return;
            const auto* storageStart = reinterpret_cast<const byte*>(m_storage.get());
            const auto* storageEnd = storageStart + (m_capacity * sizeof(Storage));
            const auto* ptrByte = reinterpret_cast<const byte*>(ptr);
            if (ptrByte < storageStart || ptrByte >= storageEnd)
            {
                RL_LOG_ERROR("PoolAllocator: pointer outside pool storage in Free");
                return;
            }
            const usize byteOffset = static_cast<usize>(ptrByte - storageStart);
            if (byteOffset % sizeof(Storage) != 0)
            {
                RL_LOG_ERROR("PoolAllocator: unaligned pointer in Free");
                return;
            }
            const usize idx = byteOffset / sizeof(Storage);
            if (!m_allocatedSlots[idx])
            {
                RL_LOG_ERROR("PoolAllocator: double free detected on element {}", idx);
                return;
            }
            m_allocatedSlots[idx] = false;
            std::destroy_at(ptr);
            m_freeList.push_back(idx);
            --m_allocated;
        }

        [[nodiscard]] usize GetAllocated() const noexcept { return m_allocated; }
        [[nodiscard]] usize GetCapacity()  const noexcept { return m_capacity;  }
        [[nodiscard]] usize GetFree()      const noexcept { return m_freeList.size(); }

    private:

        using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;

        std::unique_ptr<Storage[]> m_storage;
        std::vector<usize>         m_freeList;
        std::vector<bool>          m_allocatedSlots;
        usize                      m_capacity;
        usize                      m_allocated = 0;
    };

    class StackAllocator
    {
    public:
        using Marker = usize;

        explicit StackAllocator(usize capacityBytes)
            : m_buffer(std::make_unique<byte[]>(capacityBytes))
            , m_capacity(capacityBytes)
            , m_offset(0)
        {}

        [[nodiscard]] void* Allocate(usize sizeBytes, usize alignment = alignof(std::max_align_t))
        {
            const usize alignedOffset = AlignUp(m_offset, alignment);
            if (alignedOffset > m_capacity || sizeBytes > m_capacity - alignedOffset)
            {
                RL_LOG_ERROR("StackAllocator: out of memory! Requested {}B", sizeBytes);
                return nullptr;
            }
            void* ptr = m_buffer.get() + alignedOffset;
            m_offset  = alignedOffset + sizeBytes;
            return ptr;
        }

        template <typename T, typename... Args>
        [[nodiscard]] T* New(Args&&... args)
        {
            void* mem = Allocate(sizeof(T), alignof(T));
            if (!mem) return nullptr;
            return std::construct_at(static_cast<T*>(mem), std::forward<Args>(args)...);
        }

        [[nodiscard]] Marker GetMarker() const noexcept { return m_offset; }

        void FreeToMarker(Marker marker) noexcept
        {
            if (marker <= m_offset)
            {
                m_offset = marker;
            }
        }

        void Reset() noexcept { m_offset = 0; }

        [[nodiscard]] usize GetUsed()     const noexcept { return m_offset;             }
        [[nodiscard]] usize GetCapacity() const noexcept { return m_capacity;            }

    private:
        static usize AlignUp(usize value, usize align) noexcept
        {
            if (align == 0 || (align & (align - 1)) != 0) align = alignof(std::max_align_t);
            return (value + align - 1) & ~(align - 1);
        }

        std::unique_ptr<byte[]> m_buffer;
        usize                   m_capacity;
        usize                   m_offset;
    };

}
