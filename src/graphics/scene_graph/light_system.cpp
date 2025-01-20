#include "light_system.h"
#include <algorithm>

namespace lincore::scene
{
    uint32_t LightSystem::AddLight(const LightData& light)
    {
        lights_.push_back(light);
        return static_cast<uint32_t>(lights_.size() - 1);
    }

    void LightSystem::UpdateLight(uint32_t index, const LightData& light)
    {
        if (index < lights_.size())
        {
            lights_[index] = light;
        }
    }

    void LightSystem::RemoveLight(uint32_t index)
    {
        if (index < lights_.size())
        {
            lights_.erase(lights_.begin() + index);
        }
    }

    const LightData& LightSystem::GetLight(uint32_t index) const
    {
        return lights_[index];
    }

    std::vector<uint32_t> LightSystem::GetLightsByType(LightType type) const
    {
        std::vector<uint32_t> result;
        for (size_t i = 0; i < lights_.size(); ++i)
        {
            if (lights_[i].type == type && lights_[i].enabled)
            {
                result.push_back(static_cast<uint32_t>(i));
            }
        }
        return result;
    }

    void LightSystem::Clear()
    {
        lights_.clear();
    }

    size_t LightSystem::GetEnabledLightCount() const
    {
        return std::count_if(lights_.begin(), lights_.end(),
            [](const LightData& light) { return light.enabled != 0; });
    }
} 