#include "math_utils.h"

namespace math_utils
{
    float Lerp(float a, float b, float f)
    {
        return a + f * (b - a);
    }
}