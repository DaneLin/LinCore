#pragma once

namespace lincore
{
    class Timestep
    {
    public:
        Timestep(float time = 0.0f)
            : time_(time)
        {
        }

        operator float() const { return time_; }

        float GetSecond() const { return time_; }
        float GetMilliSeconds() const { return time_ * 1000.0f; }

    private:
        float time_;
    };
}