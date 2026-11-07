#pragma once

#include "Core/Types.hpp"
#include "Core/UUID.hpp"

#include <memory>
#include <string>

namespace RL
{

    struct AssetBase
    {
        UUID        uuid;
        std::string virtualPath;
        std::string absolutePath;
        bool        isLoaded = false;

        virtual ~AssetBase() = default;
    };

    template <typename AssetT>
    class AssetHandle
    {
    public:
        AssetHandle() = default;
        explicit AssetHandle(std::shared_ptr<AssetT> ptr) : m_ptr(std::move(ptr)) {}

        [[nodiscard]] AssetT*       Get()       noexcept { return m_ptr.get(); }
        [[nodiscard]] const AssetT* Get() const noexcept { return m_ptr.get(); }

        AssetT*       operator->()       noexcept { return m_ptr.get(); }
        const AssetT* operator->() const noexcept { return m_ptr.get(); }
        AssetT&       operator*()        noexcept { return *m_ptr; }
        const AssetT& operator*()  const noexcept { return *m_ptr; }

        [[nodiscard]] explicit operator bool() const noexcept { return m_ptr && m_ptr->isLoaded; }
        [[nodiscard]] bool IsValid() const noexcept { return static_cast<bool>(*this); }

        [[nodiscard]] const UUID& GetUUID() const noexcept
        {
            static UUID s_nil;
            return m_ptr ? m_ptr->uuid : s_nil;
        }

        [[nodiscard]] std::string_view GetPath() const noexcept
        {
            return m_ptr ? std::string_view(m_ptr->virtualPath) : std::string_view{};
        }

        void Reset() noexcept { m_ptr.reset(); }

        bool operator==(const AssetHandle&) const noexcept = default;

    private:
        std::shared_ptr<AssetT> m_ptr;
    };

}
