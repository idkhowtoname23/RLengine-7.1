#pragma once

#include "Core/Types.hpp"
#include "Core/Logger.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Components.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace RL::Editor
{

    class ICommand
    {
    public:
        virtual ~ICommand() = default;

        virtual void Execute() = 0;
        virtual void Undo()    = 0;

        virtual bool TryMerge([[maybe_unused]] const ICommand& other) { return false; }

        [[nodiscard]] virtual std::string_view GetName() const noexcept = 0;
    };

    class CommandHistory
    {
    public:
        static constexpr usize kMaxHistory = 256;

        static void Execute(std::unique_ptr<ICommand> cmd)
        {
            cmd->Execute();

            if (!s_undoStack.empty() && s_undoStack.back()->TryMerge(*cmd))
            {

                return;
            }

            s_undoStack.push_back(std::move(cmd));
            s_redoStack.clear();

            if (s_undoStack.size() > kMaxHistory)
            {
                s_undoStack.erase(s_undoStack.begin());
            }

            RL_LOG_TRACE("CommandHistory: executed '{}' (undo stack: {})",
                s_undoStack.back()->GetName(), s_undoStack.size());
        }

        static void Undo()
        {
            if (s_undoStack.empty()) return;

            auto cmd = std::move(s_undoStack.back());
            s_undoStack.pop_back();
            cmd->Undo();
            s_redoStack.push_back(std::move(cmd));

            RL_LOG_TRACE("CommandHistory: undo '{}' (undo stack: {})",
                s_redoStack.back()->GetName(), s_undoStack.size());
        }

        static void Redo()
        {
            if (s_redoStack.empty()) return;

            auto cmd = std::move(s_redoStack.back());
            s_redoStack.pop_back();
            cmd->Execute();
            s_undoStack.push_back(std::move(cmd));

            RL_LOG_TRACE("CommandHistory: redo '{}' (undo stack: {})",
                s_undoStack.back()->GetName(), s_undoStack.size());
        }

        static void Clear()
        {
            s_undoStack.clear();
            s_redoStack.clear();
        }

        [[nodiscard]] static bool CanUndo() noexcept { return !s_undoStack.empty(); }
        [[nodiscard]] static bool CanRedo() noexcept { return !s_redoStack.empty(); }
        [[nodiscard]] static usize GetUndoCount() noexcept { return s_undoStack.size(); }
        [[nodiscard]] static usize GetRedoCount() noexcept { return s_redoStack.size(); }

        [[nodiscard]] static std::string_view GetUndoName() noexcept
        {
            return s_undoStack.empty() ? "" : s_undoStack.back()->GetName();
        }
        [[nodiscard]] static std::string_view GetRedoName() noexcept
        {
            return s_redoStack.empty() ? "" : s_redoStack.back()->GetName();
        }

    private:
        inline static std::vector<std::unique_ptr<ICommand>> s_undoStack;
        inline static std::vector<std::unique_ptr<ICommand>> s_redoStack;
    };

    class MoveCommand : public ICommand
    {
    public:
        MoveCommand(Scene* scene, Entity ent,
                    DirectX::XMFLOAT3 oldPos, DirectX::XMFLOAT3 newPos)
            : m_scene(scene), m_entity(ent), m_oldPos(oldPos), m_newPos(newPos)
        {}

        void Execute() override { SetPos(m_newPos); }
        void Undo()    override { SetPos(m_oldPos); }

        bool TryMerge(const ICommand& other) override
        {
            if (const auto* mc = dynamic_cast<const MoveCommand*>(&other))
            {
                if (mc->m_entity == m_entity)
                {
                    m_newPos = mc->m_newPos;
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] std::string_view GetName() const noexcept override { return "Move Entity"; }

    private:
        void SetPos(const DirectX::XMFLOAT3& pos)
        {
            if (m_scene && m_scene->IsAlive(m_entity) &&
                m_scene->HasComponent<TransformComponent>(m_entity))
            {
                m_scene->GetComponent<TransformComponent>(m_entity).Position = pos;
            }
        }

        Scene*               m_scene;
        Entity               m_entity;
        DirectX::XMFLOAT3    m_oldPos, m_newPos;
    };

    class TransformCommand : public ICommand
    {
    public:
        TransformCommand(Scene* scene, Entity ent,
                         const TransformComponent& oldTrans, const TransformComponent& newTrans)
            : m_scene(scene), m_entity(ent), m_oldTrans(oldTrans), m_newTrans(newTrans)
        {}

        void Execute() override { SetTransform(m_newTrans); }
        void Undo()    override { SetTransform(m_oldTrans); }

        bool TryMerge(const ICommand& other) override
        {
            if (const auto* tc = dynamic_cast<const TransformCommand*>(&other))
            {
                if (tc->m_entity == m_entity)
                {
                    m_newTrans = tc->m_newTrans;
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] std::string_view GetName() const noexcept override { return "Transform Entity"; }

    private:
        void SetTransform(const TransformComponent& t)
        {
            if (m_scene && m_scene->IsAlive(m_entity) &&
                m_scene->HasComponent<TransformComponent>(m_entity))
            {
                m_scene->GetComponent<TransformComponent>(m_entity) = t;
            }
        }

        Scene*             m_scene;
        Entity             m_entity;
        TransformComponent m_oldTrans;
        TransformComponent m_newTrans;
    };

    class DeleteEntityCommand : public ICommand
    {
    public:
        DeleteEntityCommand(Scene* scene, Entity ent)
            : m_scene(scene), m_entity(ent)
        {

            if (scene->HasComponent<TagComponent>(ent))
                m_savedTag = scene->GetComponent<TagComponent>(ent).Tag;
            if (scene->HasComponent<TransformComponent>(ent))
                m_savedTransform = scene->GetComponent<TransformComponent>(ent);
        }

        void Execute() override
        {
            if (m_scene->IsAlive(m_entity))
                m_scene->DestroyEntity(m_entity);
        }

        void Undo() override
        {

            m_entity = m_scene->CreateEntity(m_savedTag);
            if (m_savedTransform.has_value())
            {
                m_scene->AddComponent<TransformComponent>(m_entity, *m_savedTransform);
            }
        }

        [[nodiscard]] std::string_view GetName() const noexcept override { return "Delete Entity"; }

    private:
        Scene*                              m_scene;
        Entity                             m_entity;
        std::string                        m_savedTag;
        std::optional<TransformComponent>  m_savedTransform;
    };

    class PropertyChangeCommand : public ICommand
    {
    public:
        using Setter = std::function<void()>;

        PropertyChangeCommand(std::string_view name, Setter undoSetter, Setter redoSetter)
            : m_name(name), m_undoSetter(std::move(undoSetter)), m_redoSetter(std::move(redoSetter))
        {}

        void Execute() override { m_redoSetter(); }
        void Undo()    override { m_undoSetter(); }

        [[nodiscard]] std::string_view GetName() const noexcept override { return m_name; }

    private:
        std::string m_name;
        Setter      m_undoSetter;
        Setter      m_redoSetter;
    };

}
