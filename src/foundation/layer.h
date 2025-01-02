#pragma once

#include <string>
#include <memory>
#include <vector>

namespace lincore
{
    class CommandBuffer;
    class Event;
    class Layer
    {
    public:
        Layer(const std::string &name = "layer") : debug_name_(name) {}

        virtual ~Layer() = default;
        virtual void OnAttach() {}
        virtual void OnDetach() {}
        virtual void OnUpdate(float dt) {}
        virtual void OnRender(CommandBuffer *cmd) {}
        virtual void OnImGuiRender() {}
        virtual void OnEvent(Event &event) {}
        inline const std::string &GetName() const { return debug_name_; }

    private:
        std::string debug_name_;
    };

    class LayerStack
    {
    public:
        LayerStack() = default;
        ~LayerStack();
        void PushLayer(Layer* layer);
        void PopLayer(Layer* layer);
        void PushOverlay(Layer* overlay);
        void PopOverlay(Layer* overlay);
       
		std::vector<Layer*>::iterator begin() { return layers_.begin(); }
		std::vector<Layer*>::iterator end() { return layers_.end(); };
		std::vector<Layer*>::reverse_iterator rbegin() { return layers_.rbegin(); }
		std::vector<Layer*>::reverse_iterator rend() { return layers_.rend(); }

    private:
        std::vector<Layer*> layers_;
        unsigned int layer_insert_index_ = 0;
    };
}