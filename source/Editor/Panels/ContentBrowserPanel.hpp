#pragma once

#include "Editor/UI/Panel.hpp"
#include "Platform/FileSystem/VFS.hpp"
#include "Core/Types.hpp"

#include <imgui.h>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace RL::Editor
{
    enum class AssetFilterType : u8
    {
        All = 0, Textures, Meshes, Scripts, Audio, Shaders
    };

    class ContentBrowserPanel : public Panel
    {
    public:
        ContentBrowserPanel()
            : Panel("Content Browser##CB")
        {

            SetRoot("assets://");
        }

        void SetRoot(std::string_view virtualRoot)
        {
            m_virtualRoot  = virtualRoot;
            m_currentPath  = m_virtualRoot;
            RefreshDirectory();
        }

        void OnRender() override
        {

            RenderToolbar();

            ImGui::Separator();

            const f32 leftWidth = 180.0f;

            if (ImGui::BeginChild("##cb_left", { leftWidth, 0 }, true))
            {
                RenderDirectoryTree(m_virtualRoot);
            }
            ImGui::EndChild();

            ImGui::SameLine();

            if (ImGui::BeginChild("##cb_right", { 0, 0 }, false))
            {
                RenderFileGrid();
            }
            ImGui::EndChild();
        }

    private:

        void RenderToolbar()
        {

            if (ImGui::ArrowButton("##back", ImGuiDir_Left))
            {
                NavigateBack();
            }
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.5f, 0.05f, 1.0f));
            ImGui::Text("%s", m_currentPath.c_str());
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::SetNextItemWidth(160.0f);
            ImGui::InputTextWithHint("##cb_search", "Search...", m_searchBuf, sizeof(m_searchBuf));

            ImGui::SameLine();

            const char* filterNames[] = { "All", "Textures", "Meshes", "Scripts", "Audio", "Shaders" };
            int filterIdx = static_cast<int>(m_filter);
            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::Combo("##cb_filter", &filterIdx, filterNames, IM_ARRAYSIZE(filterNames)))
            {
                m_filter = static_cast<AssetFilterType>(filterIdx);
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::SliderFloat("##thumb_sz", &m_thumbnailSize, 48.0f, 128.0f, "%.0f");
        }

        void RenderDirectoryTree(const std::string& virtualPath)
        {
            const auto entries = VFS::ListDirectory(virtualPath, false);
            for (const auto& entry : entries)
            {
                if (!std::filesystem::is_directory(entry)) continue;

                const std::string dirName = entry.filename().string();
                const bool open = ImGui::TreeNodeEx(dirName.c_str(),
                    ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth);

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                {
                    m_currentPath = virtualPath + dirName + "/";
                    RefreshDirectory();
                }

                if (open)
                {
                    RenderDirectoryTree(virtualPath + dirName + "/");
                    ImGui::TreePop();
                }
            }
        }

        void RenderFileGrid()
        {
            const f32 cellSize  = m_thumbnailSize + 16.0f;
            const f32 panelWidth= ImGui::GetContentRegionAvail().x;
            const i32 cols      = std::max(1, static_cast<i32>(panelWidth / cellSize));

            const std::string filter(m_searchBuf);

            i32 col = 0;
            for (const auto& entry : m_currentEntries)
            {
                const std::string filename = entry.path().filename().string();
                const bool isDir = std::filesystem::is_directory(entry.path());

                if (!filter.empty() && filename.find(filter) == std::string::npos)
                    continue;

                if (!isDir && !PassesTypeFilter(entry.path().extension().string()))
                    continue;

                if (col > 0) ImGui::SameLine();
                if (col >= cols) { col = 0; }

                RenderFileCell(entry, filename, isDir);

                ++col;
                if (col >= cols) col = 0;
            }

            if (m_currentEntries.empty())
            {
                ImGui::Spacing();
                ImGui::TextDisabled("  (empty folder)");
            }
        }

        void RenderFileCell(const std::filesystem::directory_entry& entry,
                            const std::string& name, bool isDir)
        {
            const std::string cellId = "##cb_" + name;

            ImGui::BeginGroup();

            ImU32 iconColor = IM_COL32(120, 120, 120, 255);
            const char* iconLabel = "?";

            if (isDir)
            {
                iconColor = IM_COL32(230, 190, 60, 255);
                iconLabel = "DIR";
            }
            else
            {
                const std::string ext = entry.path().extension().string();
                if (ext == ".png" || ext == ".jpg" || ext == ".tga" || ext == ".hdr")
                { iconColor = IM_COL32(100, 180, 240, 255); iconLabel = "TEX"; }
                else if (ext == ".gltf" || ext == ".glb" || ext == ".obj")
                { iconColor = IM_COL32(90, 200, 90, 255); iconLabel = "MSH"; }
                else if (ext == ".lua")
                { iconColor = IM_COL32(200, 140, 60, 255); iconLabel = "LUA"; }
                else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3")
                { iconColor = IM_COL32(180, 100, 200, 255); iconLabel = "SND"; }
                else if (ext == ".hlsl")
                { iconColor = IM_COL32(240, 100, 100, 255); iconLabel = "SHD"; }
            }

            const ImVec2 thumbSize = { m_thumbnailSize, m_thumbnailSize };
            const ImVec2 cursor    = ImGui::GetCursorScreenPos();
            ImDrawList* dl         = ImGui::GetWindowDrawList();

            const bool isSelected = (m_selected == name);
            if (isSelected)
            {
                dl->AddRectFilled(
                    { cursor.x - 4, cursor.y - 4 },
                    { cursor.x + thumbSize.x + 4, cursor.y + thumbSize.y + 4 + 18 },
                    IM_COL32(91, 49, 5, 180), 4.0f);
            }

            dl->AddRectFilled(cursor,
                { cursor.x + thumbSize.x, cursor.y + thumbSize.y },
                IM_COL32(30, 30, 30, 255), 4.0f);

            dl->AddRectFilled(cursor,
                { cursor.x + thumbSize.x, cursor.y + thumbSize.y },
                iconColor & 0x00FFFFFF | 0x28000000, 4.0f);

            const ImVec2 textPos{
                cursor.x + thumbSize.x * 0.5f - 12,
                cursor.y + thumbSize.y * 0.5f - 7
            };
            dl->AddText(textPos, iconColor, iconLabel);

            ImGui::Dummy(thumbSize);

            if (ImGui::IsItemHovered())
            {
                dl->AddRect(cursor,
                    { cursor.x + thumbSize.x, cursor.y + thumbSize.y },
                    IM_COL32(230, 126, 13, 200), 4.0f, 0, 2.0f);
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                m_selected = name;
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                if (isDir)
                {
                    m_pathHistory.push_back(m_currentPath);
                    m_currentPath = m_currentPath + name + "/";
                    RefreshDirectory();
                }
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("%s", entry.path().string().c_str());
                if (!isDir)
                {
                    const auto size = std::filesystem::file_size(entry.path());
                    ImGui::TextDisabled("%.1f KB", size / 1024.0f);
                }
                ImGui::EndTooltip();
            }

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                const std::string fullVirtual = m_currentPath + name;
                ImGui::SetDragDropPayload("ASSET_PATH", fullVirtual.c_str(),
                    fullVirtual.size() + 1);
                ImGui::Text("  %s  %s", iconLabel, name.c_str());
                ImGui::EndDragDropSource();
            }

            ImGui::PushTextWrapPos(cursor.x + thumbSize.x);
            ImGui::TextDisabled("%s", name.size() > 14 ? (name.substr(0, 13) + "…").c_str() : name.c_str());
            ImGui::PopTextWrapPos();

            ImGui::EndGroup();
        }

        bool PassesTypeFilter(const std::string& ext) const
        {
            switch (m_filter)
            {
                case AssetFilterType::All:      return true;
                case AssetFilterType::Textures: return ext==".png"||ext==".jpg"||ext==".tga"||ext==".hdr";
                case AssetFilterType::Meshes:   return ext==".gltf"||ext==".glb"||ext==".obj";
                case AssetFilterType::Scripts:  return ext==".lua";
                case AssetFilterType::Audio:    return ext==".wav"||ext==".ogg"||ext==".mp3";
                case AssetFilterType::Shaders:  return ext==".hlsl"||ext==".hlsli";
            }
            return true;
        }

        void RefreshDirectory()
        {
            m_currentEntries.clear();
            const auto entries = VFS::ListDirectory(m_currentPath, false);
            for (const auto& p : entries)
            {
                m_currentEntries.emplace_back(p);
            }

            std::sort(m_currentEntries.begin(), m_currentEntries.end(),
                [](const auto& a, const auto& b) {
                    if (std::filesystem::is_directory(a.path()) !=
                        std::filesystem::is_directory(b.path()))
                        return std::filesystem::is_directory(a.path());
                    return a.path().filename() < b.path().filename();
                });
        }

        void NavigateBack()
        {
            if (!m_pathHistory.empty())
            {
                m_currentPath = m_pathHistory.back();
                m_pathHistory.pop_back();
                RefreshDirectory();
            }
        }

        std::string m_virtualRoot   = "assets://";
        std::string m_currentPath   = "assets://";
        std::string m_selected;
        AssetFilterType m_filter    = AssetFilterType::All;
        f32         m_thumbnailSize = 72.0f;

        std::vector<std::filesystem::directory_entry> m_currentEntries;
        std::vector<std::string>                       m_pathHistory;

        char m_searchBuf[128] = {};
    };

}
