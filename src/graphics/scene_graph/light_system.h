#pragma once

#include <vector>
#include <memory>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace lincore::scene
{
    // 光源类型枚举
    enum class LightType : uint32_t
    {
        Directional = 0,    // 平行光
        Point = 1,          // 点光源
        Spot = 2,           // 聚光灯
        Area = 3,           // 面光源
    };

    // 压缩后的光源结构
    struct alignas(16) LightData
    {
        glm::vec4 color_intensity;     // rgb: 颜色, a: 强度
        glm::vec4 position_range;      // xyz: 位置/方向, w: 范围/未使用(平行光)
        glm::vec4 direction_angles;    // xyz: 方向(聚光灯)/面光源尺寸(xy)+pad, w: 内角外角(聚光灯,xy分量)
        glm::vec4 attenuation;         // xyz: 常数项/线性项/二次项, w: 类型(低30位)和启用标志(最高位)

        // 构造函数
        LightData() : 
            color_intensity(1.0f, 1.0f, 1.0f, 1.0f),
            position_range(0.0f, 0.0f, 0.0f, 10.0f),
            direction_angles(0.0f, -1.0f, 0.0f, 0.0f),
            attenuation(1.0f, 0.09f, 0.032f, BitwiseFloat(static_cast<uint32_t>(LightType::Point) | (1u << 31))) {}

        // 辅助函数
        void SetType(LightType t) { 
            uint32_t current = FloatBitwise(attenuation.w);
            attenuation.w = BitwiseFloat((current & 0x80000000u) | static_cast<uint32_t>(t));
        }
        
        void SetEnabled(bool enabled) {
            uint32_t current = FloatBitwise(attenuation.w);
            attenuation.w = BitwiseFloat((current & 0x7FFFFFFFu) | (enabled ? 0x80000000u : 0u));
        }
        
        LightType GetType() const { 
            return static_cast<LightType>(FloatBitwise(attenuation.w) & 0x7FFFFFFFu);
        }
        
        bool IsEnabled() const { 
            return (FloatBitwise(attenuation.w) & 0x80000000u) != 0;
        }

        // 设置颜色和强度
        void SetColor(const glm::vec3& c) { color_intensity = glm::vec4(c, color_intensity.w); }
        void SetIntensity(float i) { color_intensity.w = i; }
        
        // 设置位置/方向
        void SetPosition(const glm::vec3& p) { position_range = glm::vec4(p, position_range.w); }
        void SetRange(float r) { position_range.w = r; }
        
        // 设置聚光灯参数
        void SetSpotDirection(const glm::vec3& d) { direction_angles = glm::vec4(d, direction_angles.w); }
        void SetSpotAngles(float inner, float outer) { 
            // 将两个角度打包到一个float中：
            // 使用高16位存储inner角度，低16位存储outer角度
            uint32_t inner_bits = static_cast<uint32_t>(inner * 65535.0f) & 0xFFFF;
            uint32_t outer_bits = static_cast<uint32_t>(outer * 65535.0f) & 0xFFFF;
            uint32_t packed = (inner_bits << 16) | outer_bits;
            direction_angles.w = *reinterpret_cast<float*>(&packed);
        }
        
        // 设置面光源尺寸
        void SetAreaSize(const glm::vec2& size) { 
            direction_angles.x = size.x;
            direction_angles.y = size.y;
        }

        // 设置衰减参数
        void SetAttenuation(float constant, float linear, float quadratic) {
            attenuation = glm::vec4(constant, linear, quadratic, 0.0f);
        }

        // 获取参数
        glm::vec3 GetColor() const { return glm::vec3(color_intensity); }
        float GetIntensity() const { return color_intensity.w; }
        glm::vec3 GetPosition() const { return glm::vec3(position_range); }
        float GetRange() const { return position_range.w; }
        glm::vec3 GetDirection() const { return glm::vec3(direction_angles); }
        glm::vec2 GetSpotAngles() const { 
            // 从float中解包两个角度
            uint32_t packed = *reinterpret_cast<const uint32_t*>(&direction_angles.w);
            float inner = static_cast<float>(packed >> 16) / 65535.0f;
            float outer = static_cast<float>(packed & 0xFFFF) / 65535.0f;
            return glm::vec2(inner, outer);
        }
        glm::vec2 GetAreaSize() const { 
            return glm::vec2(direction_angles.x, direction_angles.y);
        }

        // 获取衰减参数
        glm::vec3 GetAttenuation() const {
            return glm::vec3(attenuation);
        }

    private:
        // 辅助函数：在float和uint32_t之间安全转换
        static float BitwiseFloat(uint32_t v) {
            return *reinterpret_cast<float*>(&v);
        }
        
        static uint32_t FloatBitwise(float v) {
            return *reinterpret_cast<const uint32_t*>(&v);
        }
    };

    // 光源系统类
    class LightSystem
    {
    public:
        // 添加新光源，返回光源索引
        uint32_t AddLight(const LightData& light);
        
        // 更新现有光源
        void UpdateLight(uint32_t index, const LightData& light);
        
        // 移除光源
        void RemoveLight(uint32_t index);
        
        // 获取光源数据
        const LightData& GetLight(uint32_t index) const;
        
        // 获取所有光源
        const std::vector<LightData>& GetAllLights() const { return lights_; }
        
        // 获取特定类型的所有光源
        std::vector<uint32_t> GetLightsByType(LightType type) const;
        
        // 清除所有光源
        void Clear();
        
        // 获取光源数量
        size_t GetLightCount() const { return lights_.size(); }
        
        // 获取启用的光源数量
        size_t GetEnabledLightCount() const;
        
        // 获取GPU缓冲区数据
        const void* GetGPUData() const { return lights_.data(); }
        size_t GetGPUDataSize() const { return lights_.size() * sizeof(LightData); }

    private:
        std::vector<LightData> lights_;
    };
} 