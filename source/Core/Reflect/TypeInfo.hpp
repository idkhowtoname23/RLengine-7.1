#pragma once

#include "Core/Types.hpp"
#include "Core/StringID.hpp"

#include <any>
#include <functional>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace RL::Reflect
{

    enum class FieldKind : u8
    {
        Bool,
        Int32, Int64,
        Uint32, Uint64,
        Float32, Float64,
        String,
        Vec2, Vec3, Vec4,
        Color,
        Enum,
        Asset,
        Unknown
    };

    struct FieldInfo
    {
        std::string_view name;
        FieldKind        kind    = FieldKind::Unknown;
        usize            offset  = 0;
        usize            size    = 0;
        bool             hidden  = false;

        std::function<std::any(const void*)>       getter;
        std::function<void(void*, const std::any&)> setter;
    };

    struct TypeInfo
    {
        std::string_view          name;
        usize                     size    = 0;
        std::vector<FieldInfo>    fields;
        std::type_index           typeIdx = typeid(void);

        [[nodiscard]] const FieldInfo* FindField(std::string_view fieldName) const noexcept
        {
            for (const auto& f : fields)
                if (f.name == fieldName) return &f;
            return nullptr;
        }
    };

    class TypeRegistry
    {
    public:
        static void Register(std::type_index tid, TypeInfo info)
        {
            s_registry[tid] = std::move(info);
        }

        [[nodiscard]] static const TypeInfo* Find(std::type_index tid)
        {
            auto it = s_registry.find(tid);
            return (it != s_registry.end()) ? &it->second : nullptr;
        }

        template <typename T>
        [[nodiscard]] static const TypeInfo* Find()
        {
            return Find(typeid(T));
        }

        [[nodiscard]] static const std::unordered_map<std::type_index, TypeInfo>& GetAll()
        {
            return s_registry;
        }

    private:
        inline static std::unordered_map<std::type_index, TypeInfo> s_registry;
    };

    namespace Internal
    {
        template <typename T, typename FieldT>
        FieldInfo MakeField(std::string_view name, FieldT T::* memberPtr)
        {
            FieldInfo f;
            f.name   = name;
            f.offset = reinterpret_cast<usize>(&(reinterpret_cast<T const volatile*>(0)->*memberPtr));
            f.size   = sizeof(FieldT);

            if      constexpr (std::is_same_v<FieldT, bool>)         f.kind = FieldKind::Bool;
            else if constexpr (std::is_same_v<FieldT, i32>)          f.kind = FieldKind::Int32;
            else if constexpr (std::is_same_v<FieldT, i64>)          f.kind = FieldKind::Int64;
            else if constexpr (std::is_same_v<FieldT, u32>)          f.kind = FieldKind::Uint32;
            else if constexpr (std::is_same_v<FieldT, u64>)          f.kind = FieldKind::Uint64;
            else if constexpr (std::is_same_v<FieldT, f32>)          f.kind = FieldKind::Float32;
            else if constexpr (std::is_same_v<FieldT, f64>)          f.kind = FieldKind::Float64;
            else if constexpr (std::is_same_v<FieldT, std::string>)  f.kind = FieldKind::String;
            else                                                        f.kind = FieldKind::Unknown;

            f.getter = [memberPtr](const void* obj) -> std::any {
                return static_cast<const T*>(obj)->*memberPtr;
            };
            f.setter = [memberPtr](void* obj, const std::any& val) {
                static_cast<T*>(obj)->*memberPtr = std::any_cast<FieldT>(val);
            };
            return f;
        }

        template <typename T>
        struct AutoRegistrar
        {
            explicit AutoRegistrar(TypeInfo info)
            {
                TypeRegistry::Register(typeid(T), std::move(info));
            }
        };
    }

}

#define RL_REFLECT_FIELD(Type, member) \
    ::RL::Reflect::Internal::MakeField(#member, &Type::member)

#define RL_REFLECT(Type, ...)                                                       \
    inline static ::RL::Reflect::Internal::AutoRegistrar<Type> s_rlReflect_##Type{ \
        ::RL::Reflect::TypeInfo{                                                     \
            .name    = #Type,                                                        \
            .size    = sizeof(Type),                                                 \
            .fields  = { RL_REFLECT_FIELD(Type, __VA_ARGS__) },                     \
            .typeIdx = typeid(Type)                                                  \
        }                                                                            \
    };

#define RL_REFLECT_FIELDS_1(T,a)          RL_REFLECT_FIELD(T,a)
#define RL_REFLECT_FIELDS_2(T,a,b)        RL_REFLECT_FIELD(T,a), RL_REFLECT_FIELD(T,b)
#define RL_REFLECT_FIELDS_3(T,a,b,c)      RL_REFLECT_FIELD(T,a), RL_REFLECT_FIELD(T,b), RL_REFLECT_FIELD(T,c)
#define RL_REFLECT_FIELDS_4(T,a,b,c,d)    RL_REFLECT_FIELD(T,a), RL_REFLECT_FIELD(T,b), RL_REFLECT_FIELD(T,c), RL_REFLECT_FIELD(T,d)
#define RL_REFLECT_FIELDS_5(T,a,b,c,d,e)  RL_REFLECT_FIELD(T,a), RL_REFLECT_FIELD(T,b), RL_REFLECT_FIELD(T,c), RL_REFLECT_FIELD(T,d), RL_REFLECT_FIELD(T,e)
#define RL_REFLECT_FIELDS_6(T,a,b,c,d,e,f) RL_REFLECT_FIELD(T,a), RL_REFLECT_FIELD(T,b), RL_REFLECT_FIELD(T,c), RL_REFLECT_FIELD(T,d), RL_REFLECT_FIELD(T,e), RL_REFLECT_FIELD(T,f)
#define RL_REFLECT_FIELDS_7(T,a,b,c,d,e,f,g) RL_REFLECT_FIELD(T,a), RL_REFLECT_FIELD(T,b), RL_REFLECT_FIELD(T,c), RL_REFLECT_FIELD(T,d), RL_REFLECT_FIELD(T,e), RL_REFLECT_FIELD(T,f), RL_REFLECT_FIELD(T,g)
#define RL_REFLECT_FIELDS_8(T,a,b,c,d,e,f,g,h) RL_REFLECT_FIELD(T,a), RL_REFLECT_FIELD(T,b), RL_REFLECT_FIELD(T,c), RL_REFLECT_FIELD(T,d), RL_REFLECT_FIELD(T,e), RL_REFLECT_FIELD(T,f), RL_REFLECT_FIELD(T,g), RL_REFLECT_FIELD(T,h)

#define RL_GET_REFLECT_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,NAME,...) NAME

#define RL_REFLECT(Type, ...)                                                                     \
    inline static ::RL::Reflect::Internal::AutoRegistrar<Type> s_rlReflect_##Type{               \
        ::RL::Reflect::TypeInfo{                                                                   \
            .name    = #Type,                                                                      \
            .size    = sizeof(Type),                                                               \
            .fields  = { RL_GET_REFLECT_MACRO(__VA_ARGS__,                                        \
                            RL_REFLECT_FIELDS_8, RL_REFLECT_FIELDS_7, RL_REFLECT_FIELDS_6,        \
                            RL_REFLECT_FIELDS_5, RL_REFLECT_FIELDS_4, RL_REFLECT_FIELDS_3,        \
                            RL_REFLECT_FIELDS_2, RL_REFLECT_FIELDS_1)(Type, __VA_ARGS__) },       \
            .typeIdx = typeid(Type)                                                                \
        }                                                                                          \
    };
