#include "Scene/SceneSerializer.hpp"
#include "Scene/Components.hpp"
#include "Graphics/DX12/GLTFLoader.hpp"
#include "Graphics/DX12/Mesh.hpp"
#include "Core/Logger.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <string_view>
#include <charconv>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace RL
{
    using namespace DirectX;

    static constexpr u32 BINARY_MAGIC = 0x534C5221;
    static constexpr u32 BINARY_VERSION = 1;

    enum ComponentMask : u32
    {
        Mask_Tag             = 1u << 0,
        Mask_Transform       = 1u << 1,
        Mask_Mesh            = 1u << 2,
        Mask_Light           = 1u << 3,
        Mask_Material        = 1u << 4,
        Mask_Camera          = 1u << 5,
        Mask_RigidBody       = 1u << 6,
        Mask_BoxCollider     = 1u << 7,
        Mask_SphereCollider  = 1u << 8,
        Mask_ParticleEmitter = 1u << 9,
        Mask_SkinnedMesh     = 1u << 10,
        Mask_AudioSource     = 1u << 11,
        Mask_AudioListener   = 1u << 12,
        Mask_Script          = 1u << 13,
        Mask_QuadRender      = 1u << 14,
        Mask_Relationship    = 1u << 15
    };

    enum class JsonType { Null, Bool, Number, String, Array, Object };

    struct JsonValue
    {
        JsonType Type = JsonType::Null;
        bool BoolVal = false;
        double NumVal = 0.0;
        std::string StrVal;
        std::vector<JsonValue> ArrVal;
        std::vector<std::pair<std::string, JsonValue>> ObjVal;

        [[nodiscard]] bool IsObject() const noexcept { return Type == JsonType::Object; }
        [[nodiscard]] bool IsArray() const noexcept { return Type == JsonType::Array; }
        [[nodiscard]] bool IsString() const noexcept { return Type == JsonType::String; }
        [[nodiscard]] bool IsNumber() const noexcept { return Type == JsonType::Number; }
        [[nodiscard]] bool IsBool() const noexcept { return Type == JsonType::Bool; }

        [[nodiscard]] const JsonValue* Find(std::string_view key) const noexcept
        {
            if (Type != JsonType::Object) return nullptr;
            for (const auto& kv : ObjVal)
            {
                if (kv.first == key) return &kv.second;
            }
            return nullptr;
        }

        [[nodiscard]] float AsFloat(float def = 0.0f) const noexcept
        {
            return (Type == JsonType::Number) ? static_cast<float>(NumVal) : def;
        }

        [[nodiscard]] int AsInt(int def = 0) const noexcept
        {
            return (Type == JsonType::Number) ? static_cast<int>(NumVal) : def;
        }

        [[nodiscard]] u32 AsU32(u32 def = 0) const noexcept
        {
            return (Type == JsonType::Number) ? static_cast<u32>(NumVal) : def;
        }

        [[nodiscard]] bool AsBool(bool def = false) const noexcept
        {
            return (Type == JsonType::Bool) ? BoolVal : def;
        }

        [[nodiscard]] std::string AsString(const std::string& def = "") const
        {
            return (Type == JsonType::String) ? StrVal : def;
        }
    };

    class JsonParser
    {
    public:
        explicit JsonParser(std::string_view src) : m_src(src) {}

        bool Parse(JsonValue& root)
        {
            SkipWhitespace();
            if (m_pos >= m_src.size()) return false;
            return ParseValue(root);
        }

    private:
        void SkipWhitespace()
        {
            while (m_pos < m_src.size())
            {
                const char c = m_src[m_pos];
                if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
                {
                    m_pos++;
                }
                else
                {
                    break;
                }
            }
        }

        bool Match(char expected)
        {
            SkipWhitespace();
            if (m_pos < m_src.size() && m_src[m_pos] == expected)
            {
                m_pos++;
                return true;
            }
            return false;
        }

        bool ParseValue(JsonValue& val)
        {
            SkipWhitespace();
            if (m_pos >= m_src.size()) return false;
            const char c = m_src[m_pos];
            if (c == '{') return ParseObject(val);
            if (c == '[') return ParseArray(val);
            if (c == '"') return ParseString(val.StrVal) ? (val.Type = JsonType::String, true) : false;
            if (c == 't' || c == 'f') return ParseBool(val);
            if (c == 'n') return ParseNull(val);
            if (c == '-' || (c >= '0' && c <= '9')) return ParseNumber(val);
            return false;
        }

        bool ParseObject(JsonValue& val)
        {
            if (!Match('{')) return false;
            val.Type = JsonType::Object;
            val.ObjVal.clear();
            SkipWhitespace();
            if (Match('}')) return true;

            while (true)
            {
                SkipWhitespace();
                if (m_pos >= m_src.size() || m_src[m_pos] != '"') return false;
                std::string key;
                if (!ParseString(key)) return false;
                if (!Match(':')) return false;
                JsonValue child;
                if (!ParseValue(child)) return false;
                val.ObjVal.emplace_back(std::move(key), std::move(child));

                SkipWhitespace();
                if (Match('}')) return true;
                if (!Match(',')) return false;
            }
        }

        bool ParseArray(JsonValue& val)
        {
            if (!Match('[')) return false;
            val.Type = JsonType::Array;
            val.ArrVal.clear();
            SkipWhitespace();
            if (Match(']')) return true;

            while (true)
            {
                JsonValue child;
                if (!ParseValue(child)) return false;
                val.ArrVal.push_back(std::move(child));

                SkipWhitespace();
                if (Match(']')) return true;
                if (!Match(',')) return false;
            }
        }

        bool ParseString(std::string& out)
        {
            if (m_pos >= m_src.size() || m_src[m_pos] != '"') return false;
            m_pos++;
            out.clear();
            while (m_pos < m_src.size())
            {
                const char c = m_src[m_pos++];
                if (c == '"') return true;
                if (c == '\\')
                {
                    if (m_pos >= m_src.size()) return false;
                    const char esc = m_src[m_pos++];
                    switch (esc)
                    {
                        case '"': out.push_back('"'); break;
                        case '\\': out.push_back('\\'); break;
                        case '/': out.push_back('/'); break;
                        case 'b': out.push_back('\b'); break;
                        case 'f': out.push_back('\f'); break;
                        case 'n': out.push_back('\n'); break;
                        case 'r': out.push_back('\r'); break;
                        case 't': out.push_back('\t'); break;
                        default: out.push_back(esc); break;
                    }
                }
                else
                {
                    out.push_back(c);
                }
            }
            return false;
        }

        bool ParseBool(JsonValue& val)
        {
            val.Type = JsonType::Bool;
            if (m_src.substr(m_pos, 4) == "true")
            {
                val.BoolVal = true;
                m_pos += 4;
                return true;
            }
            if (m_src.substr(m_pos, 5) == "false")
            {
                val.BoolVal = false;
                m_pos += 5;
                return true;
            }
            return false;
        }

        bool ParseNull(JsonValue& val)
        {
            if (m_src.substr(m_pos, 4) == "null")
            {
                val.Type = JsonType::Null;
                m_pos += 4;
                return true;
            }
            return false;
        }

        bool ParseNumber(JsonValue& val)
        {
            val.Type = JsonType::Number;
            const size_t start = m_pos;
            if (m_pos < m_src.size() && (m_src[m_pos] == '-' || m_src[m_pos] == '+')) m_pos++;
            while (m_pos < m_src.size() && ((m_src[m_pos] >= '0' && m_src[m_pos] <= '9') || m_src[m_pos] == '.' || m_src[m_pos] == 'e' || m_src[m_pos] == 'E' || m_src[m_pos] == '+' || m_src[m_pos] == '-'))
            {
                m_pos++;
            }
            const std::string numStr(m_src.substr(start, m_pos - start));
            char* endPtr = nullptr;
            val.NumVal = std::strtod(numStr.c_str(), &endPtr);
            return endPtr != numStr.c_str();
        }

        std::string_view m_src;
        size_t m_pos = 0;
    };

    static void WriteEscapeString(std::ostream& out, std::string_view s)
    {
        out << '"';
        for (char c : s)
        {
            switch (c)
            {
                case '"': out << "\\\""; break;
                case '\\': out << "\\\\"; break;
                case '\b': out << "\\b"; break;
                case '\f': out << "\\f"; break;
                case '\n': out << "\\n"; break;
                case '\r': out << "\\r"; break;
                case '\t': out << "\\t"; break;
                default: out << c; break;
            }
        }
        out << '"';
    }

    static void WriteIndent(std::ostream& out, int indent)
    {
        for (int i = 0; i < indent; ++i)
        {
            out << "  ";
        }
    }

    static void ResolveMesh(ID3D12Device* device, MeshComponent& mc)
    {
        if (mc.MeshRef) return;
        if (!mc.ModelPath.empty() && device)
        {
            std::error_code ec;
            std::filesystem::path currentDir = std::filesystem::current_path(ec);
            std::filesystem::path resolved = std::filesystem::weakly_canonical(currentDir / mc.ModelPath, ec);
            auto rel = resolved.lexically_relative(currentDir);
            if (!ec && !rel.empty() && !rel.string().starts_with("..") && std::filesystem::exists(resolved))
            {
                auto model = GLTFLoader::LoadFromFile(device, resolved.string());
                if (model && !model->Meshes.empty())
                {
                    mc.MeshRef = model->Meshes[0];
                    return;
                }
            }
        }

        if (device)
        {
            if (mc.MeshName == "Cube")
            {
                if (auto m = Mesh::CreateCube(device)) mc.MeshRef = *m;
            }
            else if (mc.MeshName == "Sphere")
            {
                if (auto m = Mesh::CreateSphere(device)) mc.MeshRef = *m;
            }
            else if (mc.MeshName == "Plane")
            {
                if (auto m = Mesh::CreatePlane(device)) mc.MeshRef = *m;
            }
        }
    }

    static DirectX::XMFLOAT3 ReadVec3(const JsonValue* arr, DirectX::XMFLOAT3 def = { 0.0f, 0.0f, 0.0f })
    {
        if (!arr || !arr->IsArray() || arr->ArrVal.size() < 3) return def;
        return DirectX::XMFLOAT3(arr->ArrVal[0].AsFloat(), arr->ArrVal[1].AsFloat(), arr->ArrVal[2].AsFloat());
    }

    static DirectX::XMFLOAT4 ReadVec4(const JsonValue* arr, DirectX::XMFLOAT4 def = { 0.0f, 0.0f, 0.0f, 0.0f })
    {
        if (!arr || !arr->IsArray() || arr->ArrVal.size() < 4) return def;
        return DirectX::XMFLOAT4(arr->ArrVal[0].AsFloat(), arr->ArrVal[1].AsFloat(), arr->ArrVal[2].AsFloat(), arr->ArrVal[3].AsFloat());
    }

    template<typename T>
    static void WriteBinary(std::ostream& out, const T& val)
    {
        out.write(reinterpret_cast<const char*>(&val), sizeof(T));
    }

    template<typename T>
    static bool ReadBinary(std::istream& in, T& val)
    {
        in.read(reinterpret_cast<char*>(&val), sizeof(T));
        return in.good();
    }

    static void WriteBinaryString(std::ostream& out, const std::string& str)
    {
        const u32 len = static_cast<u32>(str.size());
        WriteBinary(out, len);
        if (len > 0)
        {
            out.write(str.data(), len);
        }
    }

    static bool ReadBinaryString(std::istream& in, std::string& str)
    {
        u32 len = 0;
        if (!ReadBinary(in, len)) return false;
        str.resize(len);
        if (len > 0)
        {
            in.read(str.data(), len);
        }
        return in.good();
    }

    bool SceneSerializer::SerializeJson(const Scene& scene, const std::filesystem::path& filepath)
    {
        std::error_code ec;
        if (filepath.has_parent_path())
        {
            std::filesystem::create_directories(filepath.parent_path(), ec);
        }

        std::ofstream out(filepath);
        if (!out.is_open()) return false;

        out << "{\n";
        out << "  \"version\": \"7.1\",\n";
        out << "  \"entities\": [\n";

        const auto& alive = scene.GetAliveEntities();
        bool firstEntity = true;

        for (const Entity ent : alive)
        {
            if (!scene.IsAlive(ent)) continue;

            if (!firstEntity) out << ",\n";
            firstEntity = false;

            out << "    {\n";

            UUID id = UUID::Nil();
            if (scene.HasComponent<IDComponent>(ent))
            {
                id = scene.GetComponent<IDComponent>(ent).ID;
            }
            if (id.IsNil())
            {
                id = UUID::Generate();
            }

            out << "      \"id\": ";
            WriteEscapeString(out, id.ToString());

            if (scene.HasComponent<RelationshipComponent>(ent))
            {
                const auto& rel = scene.GetComponent<RelationshipComponent>(ent);
                if (rel.Parent.IsValid() && scene.IsValid(rel.Parent) && scene.HasComponent<IDComponent>(rel.Parent))
                {
                    const auto parentId = scene.GetComponent<IDComponent>(rel.Parent).ID;
                    out << ",\n      \"ParentUUID\": ";
                    WriteEscapeString(out, parentId.ToString());
                }
            }

            if (scene.HasComponent<TagComponent>(ent))
            {
                const auto& c = scene.GetComponent<TagComponent>(ent);
                out << ",\n      \"tag\": { \"name\": ";
                WriteEscapeString(out, c.Tag);
                out << " }";
            }

            if (scene.HasComponent<TransformComponent>(ent))
            {
                const auto& t = scene.GetComponent<TransformComponent>(ent);
                out << ",\n      \"transform\": {\n";
                out << "        \"position\": [" << t.Position.x << ", " << t.Position.y << ", " << t.Position.z << "],\n";
                out << "        \"rotation\": [" << t.Rotation.x << ", " << t.Rotation.y << ", " << t.Rotation.z << "],\n";
                out << "        \"scale\": [" << t.Scale.x << ", " << t.Scale.y << ", " << t.Scale.z << "]\n";
                out << "      }";
            }

            if (scene.HasComponent<MeshComponent>(ent))
            {
                const auto& m = scene.GetComponent<MeshComponent>(ent);
                out << ",\n      \"mesh\": {\n";
                out << "        \"name\": "; WriteEscapeString(out, m.MeshName); out << ",\n";
                out << "        \"model_path\": "; WriteEscapeString(out, m.ModelPath); out << ",\n";
                out << "        \"color\": [" << m.Color.x << ", " << m.Color.y << ", " << m.Color.z << ", " << m.Color.w << "],\n";
                out << "        \"visible\": " << (m.IsVisible ? "true" : "false") << ",\n";
                out << "        \"wireframe\": " << (m.Wireframe ? "true" : "false") << "\n";
                out << "      }";
            }

            if (scene.HasComponent<LightComponent>(ent))
            {
                const auto& l = scene.GetComponent<LightComponent>(ent);
                out << ",\n      \"light\": {\n";
                out << "        \"type\": " << static_cast<int>(l.Type) << ",\n";
                out << "        \"color\": [" << l.Color.x << ", " << l.Color.y << ", " << l.Color.z << "],\n";
                out << "        \"intensity\": " << l.Intensity << ",\n";
                out << "        \"range\": " << l.Range << ",\n";
                out << "        \"spot_angle\": " << l.SpotAngle << ",\n";
                out << "        \"cast_shadows\": " << (l.CastShadows ? "true" : "false") << "\n";
                out << "      }";
            }

            if (scene.HasComponent<MaterialComponent>(ent))
            {
                const auto& mat = scene.GetComponent<MaterialComponent>(ent);
                out << ",\n      \"material\": {\n";
                out << "        \"base_color\": [" << mat.BaseColor.x << ", " << mat.BaseColor.y << ", " << mat.BaseColor.z << ", " << mat.BaseColor.w << "],\n";
                out << "        \"metallic\": " << mat.Metallic << ",\n";
                out << "        \"roughness\": " << mat.Roughness << ",\n";
                out << "        \"ao\": " << mat.AO << ",\n";
                out << "        \"normal_strength\": " << mat.NormalStrength << ",\n";
                out << "        \"emission_strength\": " << mat.EmissionStrength << ",\n";
                out << "        \"emissive_color\": [" << mat.EmissiveColor.x << ", " << mat.EmissiveColor.y << ", " << mat.EmissiveColor.z << "],\n";
                out << "        \"emissive_intensity\": " << mat.EmissiveIntensity << ",\n";
                out << "        \"texture_path\": "; WriteEscapeString(out, mat.TexturePath); out << "\n";
                out << "      }";
            }

            if (scene.HasComponent<CameraComponent>(ent))
            {
                const auto& cam = scene.GetComponent<CameraComponent>(ent);
                out << ",\n      \"camera\": {\n";
                out << "        \"fov\": " << cam.Fov << ",\n";
                out << "        \"near_z\": " << cam.NearZ << ",\n";
                out << "        \"far_z\": " << cam.FarZ << ",\n";
                out << "        \"is_orthographic\": " << (cam.IsOrthographic ? "true" : "false") << ",\n";
                out << "        \"ortho_size\": " << cam.OrthoSize << ",\n";
                out << "        \"is_primary\": " << (cam.IsPrimary ? "true" : "false") << "\n";
                out << "      }";
            }

            if (scene.HasComponent<RigidBodyComponent>(ent))
            {
                const auto& rb = scene.GetComponent<RigidBodyComponent>(ent);
                out << ",\n      \"rigidbody\": {\n";
                out << "        \"type\": " << static_cast<int>(rb.Type) << ",\n";
                out << "        \"mass\": " << rb.Mass << ",\n";
                out << "        \"linear_velocity\": [" << rb.LinearVelocity.x << ", " << rb.LinearVelocity.y << ", " << rb.LinearVelocity.z << "],\n";
                out << "        \"angular_velocity\": [" << rb.AngularVelocity.x << ", " << rb.AngularVelocity.y << ", " << rb.AngularVelocity.z << "],\n";
                out << "        \"gravity_scale\": " << rb.GravityScale << ",\n";
                out << "        \"restitution\": " << rb.Restitution << ",\n";
                out << "        \"friction\": " << rb.Friction << ",\n";
                out << "        \"linear_damping\": " << rb.LinearDamping << ",\n";
                out << "        \"angular_damping\": " << rb.AngularDamping << ",\n";
                out << "        \"use_gravity\": " << (rb.UseGravity ? "true" : "false") << "\n";
                out << "      }";
            }

            if (scene.HasComponent<BoxColliderComponent>(ent))
            {
                const auto& bc = scene.GetComponent<BoxColliderComponent>(ent);
                out << ",\n      \"box_collider\": {\n";
                out << "        \"half_size\": [" << bc.HalfSize.x << ", " << bc.HalfSize.y << ", " << bc.HalfSize.z << "],\n";
                out << "        \"offset\": [" << bc.Offset.x << ", " << bc.Offset.y << ", " << bc.Offset.z << "]\n";
                out << "      }";
            }

            if (scene.HasComponent<SphereColliderComponent>(ent))
            {
                const auto& sc = scene.GetComponent<SphereColliderComponent>(ent);
                out << ",\n      \"sphere_collider\": {\n";
                out << "        \"radius\": " << sc.Radius << ",\n";
                out << "        \"offset\": [" << sc.Offset.x << ", " << sc.Offset.y << ", " << sc.Offset.z << "]\n";
                out << "      }";
            }

            if (scene.HasComponent<ParticleEmitterComponent>(ent))
            {
                const auto& pe = scene.GetComponent<ParticleEmitterComponent>(ent);
                out << ",\n      \"particle_emitter\": {\n";
                out << "        \"max_particles\": " << pe.MaxParticles << ",\n";
                out << "        \"emission_rate\": " << pe.EmissionRate << ",\n";
                out << "        \"lifetime\": " << pe.Lifetime << ",\n";
                out << "        \"speed\": " << pe.Speed << ",\n";
                out << "        \"cone_angle\": " << pe.ConeAngle << ",\n";
                out << "        \"start_size\": " << pe.StartSize << ",\n";
                out << "        \"end_size\": " << pe.EndSize << ",\n";
                out << "        \"start_color\": [" << pe.StartColor.x << ", " << pe.StartColor.y << ", " << pe.StartColor.z << ", " << pe.StartColor.w << "],\n";
                out << "        \"end_color\": [" << pe.EndColor.x << ", " << pe.EndColor.y << ", " << pe.EndColor.z << ", " << pe.EndColor.w << "],\n";
                out << "        \"initial_velocity\": [" << pe.InitialVelocity.x << ", " << pe.InitialVelocity.y << ", " << pe.InitialVelocity.z << "],\n";
                out << "        \"velocity_spread\": [" << pe.VelocitySpread.x << ", " << pe.VelocitySpread.y << ", " << pe.VelocitySpread.z << "],\n";
                out << "        \"gravity\": [" << pe.Gravity.x << ", " << pe.Gravity.y << ", " << pe.Gravity.z << "],\n";
                out << "        \"floor_bounce\": " << (pe.FloorBounce ? "true" : "false") << ",\n";
                out << "        \"looping\": " << (pe.Looping ? "true" : "false") << ",\n";
                out << "        \"is_active\": " << (pe.IsActive ? "true" : "false") << "\n";
                out << "      }";
            }

            if (scene.HasComponent<SkinnedMeshComponent>(ent))
            {
                const auto& sk = scene.GetComponent<SkinnedMeshComponent>(ent);
                out << ",\n      \"skinned_mesh\": {\n";
                out << "        \"model_path\": "; WriteEscapeString(out, sk.ModelPath); out << ",\n";
                out << "        \"skeleton_name\": "; WriteEscapeString(out, sk.SkeletonName); out << ",\n";
                out << "        \"current_animation\": "; WriteEscapeString(out, sk.CurrentAnimation); out << ",\n";
                out << "        \"animation_speed\": " << sk.AnimationSpeed << ",\n";
                out << "        \"is_playing\": " << (sk.IsPlaying ? "true" : "false") << ",\n";
                out << "        \"is_visible\": " << (sk.IsVisible ? "true" : "false") << "\n";
                out << "      }";
            }

            if (scene.HasComponent<AudioSourceComponent>(ent))
            {
                const auto& a = scene.GetComponent<AudioSourceComponent>(ent);
                out << ",\n      \"audio_source\": {\n";
                out << "        \"sound_path\": "; WriteEscapeString(out, a.SoundPath); out << ",\n";
                out << "        \"volume\": " << a.Volume << ",\n";
                out << "        \"pitch\": " << a.Pitch << ",\n";
                out << "        \"min_distance\": " << a.MinDistance << ",\n";
                out << "        \"max_distance\": " << a.MaxDistance << ",\n";
                out << "        \"is_3d\": " << (a.Is3D ? "true" : "false") << ",\n";
                out << "        \"loop\": " << (a.Loop ? "true" : "false") << ",\n";
                out << "        \"play_on_start\": " << (a.PlayOnStart ? "true" : "false") << "\n";
                out << "      }";
            }

            if (scene.HasComponent<AudioListenerComponent>(ent))
            {
                const auto& al = scene.GetComponent<AudioListenerComponent>(ent);
                out << ",\n      \"audio_listener\": {\n";
                out << "        \"is_active\": " << (al.IsActive ? "true" : "false") << "\n";
                out << "      }";
            }

            if (scene.HasComponent<ScriptComponent>(ent))
            {
                const auto& s = scene.GetComponent<ScriptComponent>(ent);
                out << ",\n      \"script\": {\n";
                out << "        \"script_name\": "; WriteEscapeString(out, s.ScriptName); out << ",\n";
                out << "        \"script_path\": "; WriteEscapeString(out, s.ScriptPath); out << ",\n";
                out << "        \"enabled\": " << (s.Enabled ? "true" : "false") << "\n";
                out << "      }";
            }

            if (scene.HasComponent<QuadRenderComponent>(ent))
            {
                const auto& q = scene.GetComponent<QuadRenderComponent>(ent);
                out << ",\n      \"quad_render\": {\n";
                out << "        \"color\": [" << q.Color.x << ", " << q.Color.y << ", " << q.Color.z << ", " << q.Color.w << "],\n";
                out << "        \"visible\": " << (q.IsVisible ? "true" : "false") << ",\n";
                out << "        \"texture_index\": " << q.TextureIndex << ",\n";
                out << "        \"texture_path\": "; WriteEscapeString(out, q.TexturePath); out << "\n";
                out << "      }";
            }

            out << "\n    }";
        }

        out << "\n  ]\n}\n";
        return true;
    }

    bool SceneSerializer::DeserializeJson(Scene& scene, ID3D12Device* device, const std::filesystem::path& filepath)
    {
        if (!std::filesystem::exists(filepath)) return false;

        std::ifstream file(filepath);
        if (!file.is_open()) return false;

        std::stringstream buffer;
        buffer << file.rdbuf();
        const std::string content = buffer.str();

        JsonParser parser(content);
        JsonValue root;
        if (!parser.Parse(root) || !root.IsObject()) return false;

        const auto* entitiesNode = root.Find("entities");
        if (!entitiesNode || !entitiesNode->IsArray()) return false;

        scene.Clear();

        std::vector<std::pair<Entity, UUID>> pendingParents;
        std::unordered_map<UUID, Entity> uuidToEntity;

        for (const auto& entNode : entitiesNode->ArrVal)
        {
            if (!entNode.IsObject()) continue;

            UUID id = UUID::Generate();
            if (const auto* idNode = entNode.Find("id"); idNode && idNode->IsString())
            {
                UUID parsed = UUID::FromString(idNode->StrVal);
                if (!parsed.IsNil()) id = parsed;
            }

            std::string tag = "Entity";
            if (const auto* tagObj = entNode.Find("tag"); tagObj && tagObj->IsObject())
            {
                if (const auto* nameNode = tagObj->Find("name"); nameNode && nameNode->IsString())
                {
                    tag = nameNode->StrVal;
                }
            }

            Entity entity = scene.CreateEntityWithUUID(id, tag);
            uuidToEntity[id] = entity;

            UUID parentId = UUID::Nil();
            if (const auto* pNode = entNode.Find("ParentUUID"); pNode && pNode->IsString())
            {
                parentId = UUID::FromString(pNode->StrVal);
            }
            else if (const auto* pNode = entNode.Find("parent_id"); pNode && pNode->IsString())
            {
                parentId = UUID::FromString(pNode->StrVal);
            }
            if (!parentId.IsNil())
            {
                pendingParents.emplace_back(entity, parentId);
            }

            if (const auto* trObj = entNode.Find("transform"); trObj && trObj->IsObject())
            {
                auto& t = scene.GetComponent<TransformComponent>(entity);
                t.Position = ReadVec3(trObj->Find("position"), t.Position);
                t.Rotation = ReadVec3(trObj->Find("rotation"), t.Rotation);
                t.Scale = ReadVec3(trObj->Find("scale"), t.Scale);
            }

            if (const auto* mObj = entNode.Find("mesh"); mObj && mObj->IsObject())
            {
                std::string meshName = "Cube";
                if (const auto* n = mObj->Find("name"); n && n->IsString()) meshName = n->StrVal;

                auto& mc = scene.AddComponent<MeshComponent>(entity, meshName);
                if (const auto* p = mObj->Find("model_path"); p && p->IsString()) mc.ModelPath = p->StrVal;
                mc.Color = ReadVec4(mObj->Find("color"), mc.Color);
                if (const auto* v = mObj->Find("visible"); v && v->IsBool()) mc.IsVisible = v->BoolVal;
                if (const auto* w = mObj->Find("wireframe"); w && w->IsBool()) mc.Wireframe = w->BoolVal;

                ResolveMesh(device, mc);
            }

            if (const auto* lObj = entNode.Find("light"); lObj && lObj->IsObject())
            {
                LightType lt = LightType::Point;
                if (const auto* t = lObj->Find("type"); t && t->IsNumber()) lt = static_cast<LightType>(t->AsInt());
                XMFLOAT3 col = ReadVec3(lObj->Find("color"), { 1.0f, 1.0f, 1.0f });
                float intensity = 1.0f;
                if (const auto* it = lObj->Find("intensity"); it && it->IsNumber()) intensity = it->AsFloat();
                float range = 10.0f;
                if (const auto* rg = lObj->Find("range"); rg && rg->IsNumber()) range = rg->AsFloat();

                auto& lc = scene.AddComponent<LightComponent>(entity, lt, col, intensity, range);
                if (const auto* sa = lObj->Find("spot_angle"); sa && sa->IsNumber()) lc.SpotAngle = sa->AsFloat();
                if (const auto* cs = lObj->Find("cast_shadows"); cs && cs->IsBool()) lc.CastShadows = cs->BoolVal;
            }

            if (const auto* matObj = entNode.Find("material"); matObj && matObj->IsObject())
            {
                auto& mat = scene.AddComponent<MaterialComponent>(entity);
                mat.BaseColor = ReadVec4(matObj->Find("base_color"), mat.BaseColor);
                mat.AlbedoColor = mat.BaseColor;
                if (const auto* m = matObj->Find("metallic"); m && m->IsNumber()) mat.Metallic = m->AsFloat();
                if (const auto* r = matObj->Find("roughness"); r && r->IsNumber()) mat.Roughness = r->AsFloat();
                if (const auto* a = matObj->Find("ao"); a && a->IsNumber()) mat.AO = a->AsFloat();
                if (const auto* ns = matObj->Find("normal_strength"); ns && ns->IsNumber()) mat.NormalStrength = ns->AsFloat();
                if (const auto* es = matObj->Find("emission_strength"); es && es->IsNumber()) mat.EmissionStrength = es->AsFloat();
                mat.EmissiveColor = ReadVec3(matObj->Find("emissive_color"), mat.EmissiveColor);
                if (const auto* ei = matObj->Find("emissive_intensity"); ei && ei->IsNumber()) mat.EmissiveIntensity = ei->AsFloat();
                if (const auto* tp = matObj->Find("texture_path"); tp && tp->IsString()) mat.TexturePath = tp->StrVal;
            }

            if (const auto* camObj = entNode.Find("camera"); camObj && camObj->IsObject())
            {
                auto& cam = scene.AddComponent<CameraComponent>(entity);
                if (const auto* f = camObj->Find("fov"); f && f->IsNumber()) cam.Fov = f->AsFloat();
                if (const auto* nz = camObj->Find("near_z"); nz && nz->IsNumber()) cam.NearZ = nz->AsFloat();
                if (const auto* fz = camObj->Find("far_z"); fz && fz->IsNumber()) cam.FarZ = fz->AsFloat();
                if (const auto* o = camObj->Find("is_orthographic"); o && o->IsBool()) cam.IsOrthographic = o->BoolVal;
                if (const auto* os = camObj->Find("ortho_size"); os && os->IsNumber()) cam.OrthoSize = os->AsFloat();
                if (const auto* ip = camObj->Find("is_primary"); ip && ip->IsBool()) cam.IsPrimary = ip->BoolVal;
            }

            if (const auto* rbObj = entNode.Find("rigidbody"); rbObj && rbObj->IsObject())
            {
                auto& rb = scene.AddComponent<RigidBodyComponent>(entity);
                if (const auto* t = rbObj->Find("type"); t && t->IsNumber()) rb.Type = static_cast<BodyType>(t->AsInt());
                if (const auto* m = rbObj->Find("mass"); m && m->IsNumber()) rb.Mass = (std::max)(0.001f, m->AsFloat());
                rb.LinearVelocity = ReadVec3(rbObj->Find("linear_velocity"), rb.LinearVelocity);
                rb.AngularVelocity = ReadVec3(rbObj->Find("angular_velocity"), rb.AngularVelocity);
                if (const auto* gs = rbObj->Find("gravity_scale"); gs && gs->IsNumber()) rb.GravityScale = gs->AsFloat();
                if (const auto* res = rbObj->Find("restitution"); res && res->IsNumber()) rb.Restitution = std::clamp(res->AsFloat(), 0.0f, 1.0f);
                if (const auto* fr = rbObj->Find("friction"); fr && fr->IsNumber()) rb.Friction = std::clamp(fr->AsFloat(), 0.0f, 1.0f);
                if (const auto* ld = rbObj->Find("linear_damping"); ld && ld->IsNumber()) rb.LinearDamping = (std::max)(0.0f, ld->AsFloat());
                if (const auto* ad = rbObj->Find("angular_damping"); ad && ad->IsNumber()) rb.AngularDamping = (std::max)(0.0f, ad->AsFloat());
                if (const auto* ug = rbObj->Find("use_gravity"); ug && ug->IsBool()) rb.UseGravity = ug->BoolVal;
            }

            if (const auto* bcObj = entNode.Find("box_collider"); bcObj && bcObj->IsObject())
            {
                auto& bc = scene.AddComponent<BoxColliderComponent>(entity);
                bc.HalfSize = ReadVec3(bcObj->Find("half_size"), bc.HalfSize);
                bc.Offset = ReadVec3(bcObj->Find("offset"), bc.Offset);
            }

            if (const auto* scObj = entNode.Find("sphere_collider"); scObj && scObj->IsObject())
            {
                auto& sc = scene.AddComponent<SphereColliderComponent>(entity);
                if (const auto* r = scObj->Find("radius"); r && r->IsNumber()) sc.Radius = (std::max)(0.001f, r->AsFloat());
                sc.Offset = ReadVec3(scObj->Find("offset"), sc.Offset);
            }

            if (const auto* peObj = entNode.Find("particle_emitter"); peObj && peObj->IsObject())
            {
                auto& pe = scene.AddComponent<ParticleEmitterComponent>(entity);
                if (const auto* mp = peObj->Find("max_particles"); mp && mp->IsNumber()) pe.MaxParticles = mp->AsU32();
                if (const auto* er = peObj->Find("emission_rate"); er && er->IsNumber()) pe.EmissionRate = std::clamp(er->AsFloat(), 0.0f, 50000.0f);
                if (const auto* lt = peObj->Find("lifetime"); lt && lt->IsNumber()) pe.Lifetime = (std::max)(0.01f, lt->AsFloat());
                if (const auto* sp = peObj->Find("speed"); sp && sp->IsNumber()) pe.Speed = sp->AsFloat();
                if (const auto* ca = peObj->Find("cone_angle"); ca && ca->IsNumber()) pe.ConeAngle = ca->AsFloat();
                if (const auto* ss = peObj->Find("start_size"); ss && ss->IsNumber()) pe.StartSize = (std::max)(0.001f, ss->AsFloat());
                if (const auto* es = peObj->Find("end_size"); es && es->IsNumber()) pe.EndSize = (std::max)(0.001f, es->AsFloat());
                pe.StartColor = ReadVec4(peObj->Find("start_color"), pe.StartColor);
                pe.EndColor = ReadVec4(peObj->Find("end_color"), pe.EndColor);
                pe.InitialVelocity = ReadVec3(peObj->Find("initial_velocity"), pe.InitialVelocity);
                pe.VelocitySpread = ReadVec3(peObj->Find("velocity_spread"), pe.VelocitySpread);
                pe.Gravity = ReadVec3(peObj->Find("gravity"), pe.Gravity);
                if (const auto* fb = peObj->Find("floor_bounce"); fb && fb->IsBool()) pe.FloorBounce = fb->BoolVal;
                if (const auto* lp = peObj->Find("looping"); lp && lp->IsBool()) pe.Looping = lp->BoolVal;
                if (const auto* ia = peObj->Find("is_active"); ia && ia->IsBool()) pe.IsActive = ia->BoolVal;
            }

            if (const auto* skObj = entNode.Find("skinned_mesh"); skObj && skObj->IsObject())
            {
                auto& sk = scene.AddComponent<SkinnedMeshComponent>(entity);
                if (const auto* mp = skObj->Find("model_path"); mp && mp->IsString()) sk.ModelPath = mp->StrVal;
                if (const auto* sn = skObj->Find("skeleton_name"); sn && sn->IsString()) sk.SkeletonName = sn->StrVal;
                if (const auto* ca = skObj->Find("current_animation"); ca && ca->IsString()) sk.CurrentAnimation = ca->StrVal;
                if (const auto* as = skObj->Find("animation_speed"); as && as->IsNumber()) sk.AnimationSpeed = as->AsFloat();
                if (const auto* ip = skObj->Find("is_playing"); ip && ip->IsBool()) sk.IsPlaying = ip->BoolVal;
                if (const auto* iv = skObj->Find("is_visible"); iv && iv->IsBool()) sk.IsVisible = iv->BoolVal;
            }

            if (const auto* asObj = entNode.Find("audio_source"); asObj && asObj->IsObject())
            {
                auto& a = scene.AddComponent<AudioSourceComponent>(entity);
                if (const auto* sp = asObj->Find("sound_path"); sp && sp->IsString()) a.SoundPath = sp->StrVal;
                if (const auto* v = asObj->Find("volume"); v && v->IsNumber()) a.Volume = v->AsFloat();
                if (const auto* p = asObj->Find("pitch"); p && p->IsNumber()) a.Pitch = p->AsFloat();
                if (const auto* mn = asObj->Find("min_distance"); mn && mn->IsNumber()) a.MinDistance = mn->AsFloat();
                if (const auto* mx = asObj->Find("max_distance"); mx && mx->IsNumber()) a.MaxDistance = mx->AsFloat();
                if (const auto* i3 = asObj->Find("is_3d"); i3 && i3->IsBool()) a.Is3D = i3->BoolVal;
                if (const auto* lp = asObj->Find("loop"); lp && lp->IsBool()) a.Loop = lp->BoolVal;
                if (const auto* ps = asObj->Find("play_on_start"); ps && ps->IsBool()) a.PlayOnStart = ps->BoolVal;
            }

            if (const auto* alObj = entNode.Find("audio_listener"); alObj && alObj->IsObject())
            {
                auto& al = scene.AddComponent<AudioListenerComponent>(entity);
                if (const auto* ia = alObj->Find("is_active"); ia && ia->IsBool()) al.IsActive = ia->BoolVal;
            }

            if (const auto* scObj = entNode.Find("script"); scObj && scObj->IsObject())
            {
                auto& s = scene.AddComponent<ScriptComponent>(entity);
                if (const auto* sn = scObj->Find("script_name"); sn && sn->IsString()) s.ScriptName = sn->StrVal;
                if (const auto* sp = scObj->Find("script_path"); sp && sp->IsString()) s.ScriptPath = sp->StrVal;
                if (const auto* en = scObj->Find("enabled"); en && en->IsBool()) s.Enabled = en->BoolVal;
            }

            if (const auto* qObj = entNode.Find("quad_render"); qObj && qObj->IsObject())
            {
                auto& q = scene.HasComponent<QuadRenderComponent>(entity)
                    ? scene.GetComponent<QuadRenderComponent>(entity)
                    : scene.AddComponent<QuadRenderComponent>(entity);
                q.Color = ReadVec4(qObj->Find("color"), q.Color);
                if (const auto* v = qObj->Find("visible"); v && v->IsBool()) q.IsVisible = v->BoolVal;
                if (const auto* ti = qObj->Find("texture_index"); ti && ti->IsNumber()) q.TextureIndex = static_cast<u32>(ti->NumVal);
                if (const auto* tp = qObj->Find("texture_path"); tp && tp->IsString()) q.TexturePath = tp->StrVal;
            }
        }

        for (const auto& [child, parentUUID] : pendingParents)
        {
            if (auto it = uuidToEntity.find(parentUUID); it != uuidToEntity.end())
            {
                scene.SetParent(child, it->second);
            }
        }

        return true;
    }

    bool SceneSerializer::SerializeBinary(const Scene& scene, const std::filesystem::path& filepath)
    {
        std::error_code ec;
        if (filepath.has_parent_path())
        {
            std::filesystem::create_directories(filepath.parent_path(), ec);
        }

        std::ofstream out(filepath, std::ios::binary);
        if (!out.is_open()) return false;

        WriteBinary(out, BINARY_MAGIC);
        WriteBinary(out, BINARY_VERSION);

        const auto& alive = scene.GetAliveEntities();
        u32 validCount = 0;
        for (const Entity ent : alive)
        {
            if (scene.IsAlive(ent)) validCount++;
        }
        WriteBinary(out, validCount);

        for (const Entity ent : alive)
        {
            if (!scene.IsAlive(ent)) continue;

            UUID id = UUID::Nil();
            if (scene.HasComponent<IDComponent>(ent))
            {
                id = scene.GetComponent<IDComponent>(ent).ID;
            }
            if (id.IsNil())
            {
                id = UUID::Generate();
            }

            WriteBinary(out, id.GetHigh());
            WriteBinary(out, id.GetLow());

            u32 mask = 0;
            if (scene.HasComponent<TagComponent>(ent)) mask |= Mask_Tag;
            if (scene.HasComponent<TransformComponent>(ent)) mask |= Mask_Transform;
            if (scene.HasComponent<MeshComponent>(ent)) mask |= Mask_Mesh;
            if (scene.HasComponent<LightComponent>(ent)) mask |= Mask_Light;
            if (scene.HasComponent<MaterialComponent>(ent)) mask |= Mask_Material;
            if (scene.HasComponent<CameraComponent>(ent)) mask |= Mask_Camera;
            if (scene.HasComponent<RigidBodyComponent>(ent)) mask |= Mask_RigidBody;
            if (scene.HasComponent<BoxColliderComponent>(ent)) mask |= Mask_BoxCollider;
            if (scene.HasComponent<SphereColliderComponent>(ent)) mask |= Mask_SphereCollider;
            if (scene.HasComponent<ParticleEmitterComponent>(ent)) mask |= Mask_ParticleEmitter;
            if (scene.HasComponent<SkinnedMeshComponent>(ent)) mask |= Mask_SkinnedMesh;
            if (scene.HasComponent<AudioSourceComponent>(ent)) mask |= Mask_AudioSource;
            if (scene.HasComponent<AudioListenerComponent>(ent)) mask |= Mask_AudioListener;
            if (scene.HasComponent<ScriptComponent>(ent)) mask |= Mask_Script;
            if (scene.HasComponent<QuadRenderComponent>(ent)) mask |= Mask_QuadRender;
            if (scene.HasComponent<RelationshipComponent>(ent))
            {
                const auto& rel = scene.GetComponent<RelationshipComponent>(ent);
                if (rel.Parent.IsValid() && scene.IsValid(rel.Parent) && scene.HasComponent<IDComponent>(rel.Parent))
                {
                    mask |= Mask_Relationship;
                }
            }

            WriteBinary(out, mask);

            if (mask & Mask_Tag)
            {
                const auto& c = scene.GetComponent<TagComponent>(ent);
                WriteBinaryString(out, c.Tag);
            }

            if (mask & Mask_Transform)
            {
                const auto& t = scene.GetComponent<TransformComponent>(ent);
                WriteBinary(out, t.Position);
                WriteBinary(out, t.Rotation);
                WriteBinary(out, t.Scale);
            }

            if (mask & Mask_Mesh)
            {
                const auto& m = scene.GetComponent<MeshComponent>(ent);
                WriteBinaryString(out, m.MeshName);
                WriteBinaryString(out, m.ModelPath);
                WriteBinary(out, m.Color);
                const u8 flags = (m.IsVisible ? 1u : 0u) | (m.Wireframe ? 2u : 0u);
                WriteBinary(out, flags);
            }

            if (mask & Mask_Light)
            {
                const auto& l = scene.GetComponent<LightComponent>(ent);
                const u8 t = static_cast<u8>(l.Type);
                WriteBinary(out, t);
                WriteBinary(out, l.Color);
                WriteBinary(out, l.Intensity);
                WriteBinary(out, l.Range);
                WriteBinary(out, l.SpotAngle);
                const u8 cs = l.CastShadows ? 1u : 0u;
                WriteBinary(out, cs);
            }

            if (mask & Mask_Material)
            {
                const auto& mat = scene.GetComponent<MaterialComponent>(ent);
                WriteBinary(out, mat.BaseColor);
                WriteBinary(out, mat.Metallic);
                WriteBinary(out, mat.Roughness);
                WriteBinary(out, mat.NormalStrength);
                WriteBinary(out, mat.EmissionStrength);
                WriteBinary(out, mat.EmissiveColor);
                WriteBinary(out, mat.EmissiveIntensity);
                WriteBinaryString(out, mat.TexturePath);
            }

            if (mask & Mask_Camera)
            {
                const auto& cam = scene.GetComponent<CameraComponent>(ent);
                WriteBinary(out, cam.Fov);
                WriteBinary(out, cam.NearZ);
                WriteBinary(out, cam.FarZ);
                const u8 cf = (cam.IsOrthographic ? 1u : 0u) | (cam.IsPrimary ? 2u : 0u);
                WriteBinary(out, cf);
                WriteBinary(out, cam.OrthoSize);
            }

            if (mask & Mask_RigidBody)
            {
                const auto& rb = scene.GetComponent<RigidBodyComponent>(ent);
                const u8 bt = static_cast<u8>(rb.Type);
                WriteBinary(out, bt);
                WriteBinary(out, rb.Mass);
                WriteBinary(out, rb.LinearVelocity);
                WriteBinary(out, rb.AngularVelocity);
                WriteBinary(out, rb.GravityScale);
                WriteBinary(out, rb.Restitution);
                WriteBinary(out, rb.Friction);
                WriteBinary(out, rb.LinearDamping);
                WriteBinary(out, rb.AngularDamping);
                const u8 ug = rb.UseGravity ? 1u : 0u;
                WriteBinary(out, ug);
            }

            if (mask & Mask_BoxCollider)
            {
                const auto& bc = scene.GetComponent<BoxColliderComponent>(ent);
                WriteBinary(out, bc.HalfSize);
                WriteBinary(out, bc.Offset);
            }

            if (mask & Mask_SphereCollider)
            {
                const auto& sc = scene.GetComponent<SphereColliderComponent>(ent);
                WriteBinary(out, sc.Radius);
                WriteBinary(out, sc.Offset);
            }

            if (mask & Mask_ParticleEmitter)
            {
                const auto& pe = scene.GetComponent<ParticleEmitterComponent>(ent);
                WriteBinary(out, pe.MaxParticles);
                WriteBinary(out, pe.EmissionRate);
                WriteBinary(out, pe.Lifetime);
                WriteBinary(out, pe.Speed);
                WriteBinary(out, pe.ConeAngle);
                WriteBinary(out, pe.StartSize);
                WriteBinary(out, pe.EndSize);
                WriteBinary(out, pe.StartColor);
                WriteBinary(out, pe.EndColor);
                WriteBinary(out, pe.InitialVelocity);
                WriteBinary(out, pe.VelocitySpread);
                WriteBinary(out, pe.Gravity);
                const u8 pf = (pe.FloorBounce ? 1u : 0u) | (pe.Looping ? 2u : 0u) | (pe.IsActive ? 4u : 0u);
                WriteBinary(out, pf);
            }

            if (mask & Mask_SkinnedMesh)
            {
                const auto& sk = scene.GetComponent<SkinnedMeshComponent>(ent);
                WriteBinaryString(out, sk.ModelPath);
                WriteBinaryString(out, sk.SkeletonName);
                WriteBinaryString(out, sk.CurrentAnimation);
                WriteBinary(out, sk.AnimationSpeed);
                const u8 sf = (sk.IsPlaying ? 1u : 0u) | (sk.IsVisible ? 2u : 0u);
                WriteBinary(out, sf);
            }

            if (mask & Mask_AudioSource)
            {
                const auto& a = scene.GetComponent<AudioSourceComponent>(ent);
                WriteBinaryString(out, a.SoundPath);
                WriteBinary(out, a.Volume);
                WriteBinary(out, a.Pitch);
                WriteBinary(out, a.MinDistance);
                WriteBinary(out, a.MaxDistance);
                const u8 af = (a.Is3D ? 1u : 0u) | (a.Loop ? 2u : 0u) | (a.PlayOnStart ? 4u : 0u);
                WriteBinary(out, af);
            }

            if (mask & Mask_AudioListener)
            {
                const auto& al = scene.GetComponent<AudioListenerComponent>(ent);
                const u8 lf = al.IsActive ? 1u : 0u;
                WriteBinary(out, lf);
            }

            if (mask & Mask_Script)
            {
                const auto& s = scene.GetComponent<ScriptComponent>(ent);
                WriteBinaryString(out, s.ScriptName);
                WriteBinaryString(out, s.ScriptPath);
                const u8 ef = s.Enabled ? 1u : 0u;
                WriteBinary(out, ef);
            }

            if (mask & Mask_QuadRender)
            {
                const auto& q = scene.GetComponent<QuadRenderComponent>(ent);
                WriteBinary(out, q.Color);
                const u8 qf = q.IsVisible ? 1u : 0u;
                WriteBinary(out, qf);
                WriteBinary(out, q.TextureIndex);
                WriteBinaryString(out, q.TexturePath);
            }

            if (mask & Mask_Relationship)
            {
                const auto& rel = scene.GetComponent<RelationshipComponent>(ent);
                const auto parentId = scene.GetComponent<IDComponent>(rel.Parent).ID;
                WriteBinary(out, parentId.GetHigh());
                WriteBinary(out, parentId.GetLow());
            }
        }

        return true;
    }

    bool SceneSerializer::DeserializeBinary(Scene& scene, ID3D12Device* device, const std::filesystem::path& filepath)
    {
        if (!std::filesystem::exists(filepath)) return false;

        std::ifstream in(filepath, std::ios::binary);
        if (!in.is_open()) return false;

        u32 magic = 0;
        u32 version = 0;
        u32 count = 0;

        if (!ReadBinary(in, magic) || magic != BINARY_MAGIC) return false;
        if (!ReadBinary(in, version) || version != BINARY_VERSION) return false;
        if (!ReadBinary(in, count)) return false;

        scene.Clear();

        std::vector<std::pair<Entity, UUID>> pendingParents;
        std::unordered_map<UUID, Entity> uuidToEntity;

        for (u32 i = 0; i < count; ++i)
        {
            u64 hi = 0;
            u64 lo = 0;
            u32 mask = 0;

            if (!ReadBinary(in, hi)) break;
            if (!ReadBinary(in, lo)) break;
            if (!ReadBinary(in, mask)) break;

            const UUID id{ hi, lo };
            Entity entity = scene.CreateEntityWithUUID(id, "Entity");
            uuidToEntity[id] = entity;

            if (mask & Mask_Tag)
            {
                std::string tag;
                ReadBinaryString(in, tag);
                scene.GetComponent<TagComponent>(entity).Tag = std::move(tag);
            }

            if (mask & Mask_Transform)
            {
                auto& t = scene.GetComponent<TransformComponent>(entity);
                ReadBinary(in, t.Position);
                ReadBinary(in, t.Rotation);
                ReadBinary(in, t.Scale);
            }

            if (mask & Mask_Mesh)
            {
                std::string meshName;
                std::string modelPath;
                DirectX::XMFLOAT4 color;
                u8 flags = 0;

                ReadBinaryString(in, meshName);
                ReadBinaryString(in, modelPath);
                ReadBinary(in, color);
                ReadBinary(in, flags);

                auto& mc = scene.AddComponent<MeshComponent>(entity, meshName);
                mc.ModelPath = std::move(modelPath);
                mc.Color = color;
                mc.IsVisible = (flags & 1u) != 0;
                mc.Wireframe = (flags & 2u) != 0;

                ResolveMesh(device, mc);
            }

            if (mask & Mask_Light)
            {
                u8 lt = 0;
                DirectX::XMFLOAT3 color;
                float intensity = 1.0f;
                float range = 10.0f;
                float spotAngle = 45.0f;
                u8 cs = 0;

                ReadBinary(in, lt);
                ReadBinary(in, color);
                ReadBinary(in, intensity);
                ReadBinary(in, range);
                ReadBinary(in, spotAngle);
                ReadBinary(in, cs);

                auto& lc = scene.AddComponent<LightComponent>(entity, static_cast<LightType>(lt), color, intensity, range);
                lc.SpotAngle = spotAngle;
                lc.CastShadows = (cs != 0);
            }

            if (mask & Mask_Material)
            {
                auto& mat = scene.AddComponent<MaterialComponent>(entity);
                ReadBinary(in, mat.BaseColor);
                mat.AlbedoColor = mat.BaseColor;
                ReadBinary(in, mat.Metallic);
                ReadBinary(in, mat.Roughness);
                ReadBinary(in, mat.NormalStrength);
                ReadBinary(in, mat.EmissionStrength);
                ReadBinary(in, mat.EmissiveColor);
                ReadBinary(in, mat.EmissiveIntensity);
                ReadBinaryString(in, mat.TexturePath);
            }

            if (mask & Mask_Camera)
            {
                auto& cam = scene.AddComponent<CameraComponent>(entity);
                u8 cf = 0;
                ReadBinary(in, cam.Fov);
                ReadBinary(in, cam.NearZ);
                ReadBinary(in, cam.FarZ);
                ReadBinary(in, cf);
                ReadBinary(in, cam.OrthoSize);
                cam.IsOrthographic = (cf & 1u) != 0;
                cam.IsPrimary = (cf & 2u) != 0;
            }

            if (mask & Mask_RigidBody)
            {
                auto& rb = scene.AddComponent<RigidBodyComponent>(entity);
                u8 bt = 0;
                u8 ug = 0;
                ReadBinary(in, bt);
                ReadBinary(in, rb.Mass);
                ReadBinary(in, rb.LinearVelocity);
                ReadBinary(in, rb.AngularVelocity);
                ReadBinary(in, rb.GravityScale);
                ReadBinary(in, rb.Restitution);
                ReadBinary(in, rb.Friction);
                ReadBinary(in, rb.LinearDamping);
                ReadBinary(in, rb.AngularDamping);
                ReadBinary(in, ug);
                rb.Type = static_cast<BodyType>(bt);
                rb.UseGravity = (ug != 0);
            }

            if (mask & Mask_BoxCollider)
            {
                auto& bc = scene.AddComponent<BoxColliderComponent>(entity);
                ReadBinary(in, bc.HalfSize);
                ReadBinary(in, bc.Offset);
            }

            if (mask & Mask_SphereCollider)
            {
                auto& sc = scene.AddComponent<SphereColliderComponent>(entity);
                ReadBinary(in, sc.Radius);
                ReadBinary(in, sc.Offset);
            }

            if (mask & Mask_ParticleEmitter)
            {
                auto& pe = scene.AddComponent<ParticleEmitterComponent>(entity);
                u8 pf = 0;
                ReadBinary(in, pe.MaxParticles);
                ReadBinary(in, pe.EmissionRate);
                ReadBinary(in, pe.Lifetime);
                ReadBinary(in, pe.Speed);
                ReadBinary(in, pe.ConeAngle);
                ReadBinary(in, pe.StartSize);
                ReadBinary(in, pe.EndSize);
                ReadBinary(in, pe.StartColor);
                ReadBinary(in, pe.EndColor);
                ReadBinary(in, pe.InitialVelocity);
                ReadBinary(in, pe.VelocitySpread);
                ReadBinary(in, pe.Gravity);
                ReadBinary(in, pf);
                pe.FloorBounce = (pf & 1u) != 0;
                pe.Looping = (pf & 2u) != 0;
                pe.IsActive = (pf & 4u) != 0;
            }

            if (mask & Mask_SkinnedMesh)
            {
                auto& sk = scene.AddComponent<SkinnedMeshComponent>(entity);
                u8 sf = 0;
                ReadBinaryString(in, sk.ModelPath);
                ReadBinaryString(in, sk.SkeletonName);
                ReadBinaryString(in, sk.CurrentAnimation);
                ReadBinary(in, sk.AnimationSpeed);
                ReadBinary(in, sf);
                sk.IsPlaying = (sf & 1u) != 0;
                sk.IsVisible = (sf & 2u) != 0;
            }

            if (mask & Mask_AudioSource)
            {
                auto& a = scene.AddComponent<AudioSourceComponent>(entity);
                u8 af = 0;
                ReadBinaryString(in, a.SoundPath);
                ReadBinary(in, a.Volume);
                ReadBinary(in, a.Pitch);
                ReadBinary(in, a.MinDistance);
                ReadBinary(in, a.MaxDistance);
                ReadBinary(in, af);
                a.Is3D = (af & 1u) != 0;
                a.Loop = (af & 2u) != 0;
                a.PlayOnStart = (af & 4u) != 0;
            }

            if (mask & Mask_AudioListener)
            {
                auto& al = scene.AddComponent<AudioListenerComponent>(entity);
                u8 lf = 0;
                ReadBinary(in, lf);
                al.IsActive = (lf != 0);
            }

            if (mask & Mask_Script)
            {
                auto& s = scene.AddComponent<ScriptComponent>(entity);
                u8 ef = 0;
                ReadBinaryString(in, s.ScriptName);
                ReadBinaryString(in, s.ScriptPath);
                ReadBinary(in, ef);
                s.Enabled = (ef != 0);
            }

            if (mask & Mask_QuadRender)
            {
                auto& q = scene.HasComponent<QuadRenderComponent>(entity)
                    ? scene.GetComponent<QuadRenderComponent>(entity)
                    : scene.AddComponent<QuadRenderComponent>(entity);
                u8 qf = 0;
                ReadBinary(in, q.Color);
                ReadBinary(in, qf);
                ReadBinary(in, q.TextureIndex);
                ReadBinaryString(in, q.TexturePath);
                q.IsVisible = (qf != 0);
            }

            if (mask & Mask_Relationship)
            {
                u64 pHi = 0;
                u64 pLo = 0;
                ReadBinary(in, pHi);
                ReadBinary(in, pLo);
                const UUID parentId{ pHi, pLo };
                if (!parentId.IsNil())
                {
                    pendingParents.emplace_back(entity, parentId);
                }
            }
        }

        for (const auto& [child, parentUUID] : pendingParents)
        {
            if (auto it = uuidToEntity.find(parentUUID); it != uuidToEntity.end())
            {
                scene.SetParent(child, it->second);
            }
        }

        return true;
    }

    static bool DeserializeLegacy(Scene& scene, ID3D12Device* device, const std::filesystem::path& path)
    {
        std::ifstream in(path);
        if (!in.is_open()) return false;

        scene.Clear();

        std::string line;
        Entity currentEntity{};

        auto Split = [](const std::string& str, char delim) {
            std::vector<std::string> tokens;
            std::stringstream ss(str);
            std::string token;
            while (std::getline(ss, token, delim))
            {
                tokens.push_back(token);
            }
            return tokens;
        };

        while (std::getline(in, line))
        {
            if (line.empty() || line[0] == '#') continue;

            if (line == "[Entity]")
            {
                currentEntity = scene.CreateEntity("Entity");
                continue;
            }

            const size_t eqPos = line.find('=');
            if (eqPos == std::string::npos || !currentEntity.IsValid()) continue;

            const std::string key = line.substr(0, eqPos);
            const std::string val = line.substr(eqPos + 1);
            auto tokens = Split(val, ',');

            try
            {
                if (key == "Tag")
                {
                    if (scene.HasComponent<TagComponent>(currentEntity))
                    {
                        scene.GetComponent<TagComponent>(currentEntity).Tag = val;
                    }
                }
                else if (key == "Transform" && tokens.size() >= 9)
                {
                    auto& t = scene.GetComponent<TransformComponent>(currentEntity);
                    t.Position = { std::stof(tokens[0]), std::stof(tokens[1]), std::stof(tokens[2]) };
                    t.Rotation = { std::stof(tokens[3]), std::stof(tokens[4]), std::stof(tokens[5]) };
                    t.Scale = { std::stof(tokens[6]), std::stof(tokens[7]), std::stof(tokens[8]) };
                }
                else if (key == "Mesh" && tokens.size() >= 7)
                {
                    auto& m = scene.AddComponent<MeshComponent>(currentEntity, tokens[0]);
                    m.ModelPath = tokens[1];
                    m.Color = { std::stof(tokens[2]), std::stof(tokens[3]), std::stof(tokens[4]), std::stof(tokens[5]) };
                    m.Wireframe = (tokens[6] == "1");
                    ResolveMesh(device, m);
                }
                else if (key == "Light" && tokens.size() >= 6)
                {
                    LightType lt = static_cast<LightType>(std::stoi(tokens[0]));
                    XMFLOAT3 col{ std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) };
                    float intensity = std::stof(tokens[4]);
                    float range = std::stof(tokens[5]);
                    scene.AddComponent<LightComponent>(currentEntity, lt, col, intensity, range);
                }
                else if (key == "Material" && tokens.size() >= 8)
                {
                    auto& mat = scene.AddComponent<MaterialComponent>(currentEntity);
                    mat.Roughness = std::stof(tokens[0]);
                    mat.Metallic = std::stof(tokens[1]);
                    mat.NormalStrength = std::stof(tokens[2]);
                    mat.EmissionStrength = std::stof(tokens[3]);
                    mat.BaseColor = { std::stof(tokens[4]), std::stof(tokens[5]), std::stof(tokens[6]), std::stof(tokens[7]) };
                    mat.AlbedoColor = mat.BaseColor;
                }
                else if (key == "RigidBody" && tokens.size() >= 8)
                {
                    auto& rb = scene.AddComponent<RigidBodyComponent>(currentEntity);
                    rb.Type = static_cast<BodyType>(std::stoi(tokens[0]));
                    rb.Mass = (std::max)(0.001f, std::stof(tokens[1]));
                    rb.Restitution = std::clamp(std::stof(tokens[2]), 0.0f, 1.0f);
                    rb.Friction = std::clamp(std::stof(tokens[3]), 0.0f, 1.0f);
                    rb.LinearDamping = (std::max)(0.0f, std::stof(tokens[4]));
                    rb.AngularDamping = (std::max)(0.0f, std::stof(tokens[5]));
                    rb.UseGravity = (tokens[6] == "1");
                    rb.GravityScale = std::stof(tokens[7]);
                }
                else if (key == "BoxCollider" && tokens.size() >= 6)
                {
                    auto& bc = scene.AddComponent<BoxColliderComponent>(currentEntity);
                    bc.HalfSize = { std::abs(std::stof(tokens[0])), std::abs(std::stof(tokens[1])), std::abs(std::stof(tokens[2])) };
                    bc.Offset = { std::stof(tokens[3]), std::stof(tokens[4]), std::stof(tokens[5]) };
                }
                else if (key == "SphereCollider" && tokens.size() >= 4)
                {
                    auto& sc = scene.AddComponent<SphereColliderComponent>(currentEntity);
                    sc.Radius = (std::max)(0.001f, std::abs(std::stof(tokens[0])));
                    sc.Offset = { std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]) };
                }
                else if (key == "ParticleEmitter" && tokens.size() >= 16)
                {
                    auto& pe = scene.AddComponent<ParticleEmitterComponent>(currentEntity);
                    pe.EmissionRate = std::clamp(std::stof(tokens[0]), 0.0f, 50000.0f);
                    pe.Lifetime = (std::max)(0.01f, std::stof(tokens[1]));
                    pe.Speed = std::stof(tokens[2]);
                    pe.ConeAngle = std::stof(tokens[3]);
                    pe.StartColor = { std::stof(tokens[4]), std::stof(tokens[5]), std::stof(tokens[6]), std::stof(tokens[7]) };
                    pe.EndColor = { std::stof(tokens[8]), std::stof(tokens[9]), std::stof(tokens[10]), std::stof(tokens[11]) };
                    pe.StartSize = (std::max)(0.001f, std::stof(tokens[12]));
                    pe.EndSize = (std::max)(0.001f, std::stof(tokens[13]));
                    pe.FloorBounce = (tokens[14] == "1");
                    pe.Looping = (tokens[15] == "1");
                }
                else if (key == "Script" && tokens.size() >= 2)
                {
                    auto& sc = scene.AddComponent<ScriptComponent>(currentEntity);
                    sc.ScriptName = tokens[0];
                    sc.Enabled = (tokens[1] == "1");
                }
            }
            catch (const std::exception&)
            {
                continue;
            }
        }

        return true;
    }

    bool SceneSerializer::Save(const Scene& scene, const std::filesystem::path& filepath)
    {
        if (filepath.extension() == ".bin" || filepath.extension() == ".rlbin")
        {
            return SerializeBinary(scene, filepath);
        }
        return SerializeJson(scene, filepath);
    }

    bool SceneSerializer::Load(Scene& scene, ID3D12Device* device, const std::filesystem::path& filepath)
    {
        if (!std::filesystem::exists(filepath)) return false;

        {
            std::ifstream file(filepath, std::ios::binary);
            if (file.is_open())
            {
                u32 magic = 0;
                file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
                if (magic == BINARY_MAGIC)
                {
                    return DeserializeBinary(scene, device, filepath);
                }
            }
        }

        {
            std::ifstream textFile(filepath);
            if (textFile.is_open())
            {
                std::string firstLine;
                std::getline(textFile, firstLine);
                if (firstLine.find("[RLengineScene]") != std::string::npos)
                {
                    return DeserializeLegacy(scene, device, filepath);
                }
            }
        }

        return DeserializeJson(scene, device, filepath);
    }
}
