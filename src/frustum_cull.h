#include <glm/matrix.hpp>

class Frustum
{
public:
	Frustum() {}

	// m = ProjectionMatrix * ViewMatrix
	Frustum(glm::mat4 m);

	// http://iquilezles.org/www/articles/frustumcorrect/frustumcorrect.htm
	bool IsBoxVisible(const glm::vec3 &minp, const glm::vec3 &maxp) const;

	bool IsSphereVisible(const glm::vec3 &center, float radius) const
	{
		float epsilon = 0.01f; // 增加较大的误差容限来处理浮点精度问题

		// 增加一定冗余，确保包围球包含所有边界
		float adjusted_radius = radius * 1.1f;

		// Check if the sphere is completely outside of any of the planes
		for (int i = 0; i < kCount; ++i)
		{
			float distance = glm::dot(glm::vec3(planes_[i]), center) + planes_[i].w;

			// 远平面符号在 Reversed-Z 中是反的，确保符号判断一致
			if (distance < -adjusted_radius - epsilon)
			{
				return false;
			}
		}
		return true;
	}

private:
	enum Planes
	{
		kLeft = 0,
		kRight,
		kBottom,
		kTop,
		kNear,
		kFar,
		kCount,
		kCombinations = kCount * (kCount - 1) / 2
	};

	template <Planes i, Planes j>
	struct ij2k
	{
		enum
		{
			k = i * (9 - i) / 2 + j - 1
		};
	};

	template <Planes a, Planes b, Planes c>
	glm::vec3 intersection(const glm::vec3 *crosses) const;

	glm::vec4 planes_[kCount];
	glm::vec3 points_[8];
};

inline Frustum::Frustum(glm::mat4 m)
{
	// Adapt to Reversed-Z, transpose the matrix first
	m = glm::transpose(m);

	// In Reversed-Z, the meaning of kNear and kFar changes.
	planes_[kLeft] = m[3] + m[0];
	planes_[kRight] = m[3] - m[0];
	planes_[kBottom] = m[3] - m[1]; // Adjusted to match Y-axis inversion
	planes_[kTop] = m[3] + m[1];	// Adjusted to match Y-axis inversion
	planes_[kNear] = m[3] - m[2];	// kNear plane is the farthest in Reversed-Z
	planes_[kFar] = m[3] + m[2];	// kFar plane is closest in Reversed-Z

	// Normalize the planes
	for (int i = 0; i < kCount; ++i)
	{
		planes_[i] /= glm::length(glm::vec3(planes_[i]));
	}

	// Calculate intersection points (used for more precise box culling if needed)
	glm::vec3 crosses[kCombinations] = {
		glm::cross(glm::vec3(planes_[kLeft]), glm::vec3(planes_[kRight])),
		glm::cross(glm::vec3(planes_[kLeft]), glm::vec3(planes_[kBottom])),
		glm::cross(glm::vec3(planes_[kLeft]), glm::vec3(planes_[kTop])),
		glm::cross(glm::vec3(planes_[kLeft]), glm::vec3(planes_[kNear])),
		glm::cross(glm::vec3(planes_[kLeft]), glm::vec3(planes_[kFar])),
		glm::cross(glm::vec3(planes_[kRight]), glm::vec3(planes_[kBottom])),
		glm::cross(glm::vec3(planes_[kRight]), glm::vec3(planes_[kTop])),
		glm::cross(glm::vec3(planes_[kRight]), glm::vec3(planes_[kNear])),
		glm::cross(glm::vec3(planes_[kRight]), glm::vec3(planes_[kFar])),
		glm::cross(glm::vec3(planes_[kBottom]), glm::vec3(planes_[kTop])),
		glm::cross(glm::vec3(planes_[kBottom]), glm::vec3(planes_[kNear])),
		glm::cross(glm::vec3(planes_[kBottom]), glm::vec3(planes_[kFar])),
		glm::cross(glm::vec3(planes_[kTop]), glm::vec3(planes_[kNear])),
		glm::cross(glm::vec3(planes_[kTop]), glm::vec3(planes_[kFar])),
		glm::cross(glm::vec3(planes_[kNear]), glm::vec3(planes_[kFar]))};

	points_[0] = intersection<kLeft, kBottom, kNear>(crosses);
	points_[1] = intersection<kLeft, kTop, kNear>(crosses);
	points_[2] = intersection<kRight, kBottom, kNear>(crosses);
	points_[3] = intersection<kRight, kTop, kNear>(crosses);
	points_[4] = intersection<kLeft, kBottom, kFar>(crosses);
	points_[5] = intersection<kLeft, kTop, kFar>(crosses);
	points_[6] = intersection<kRight, kBottom, kFar>(crosses);
	points_[7] = intersection<kRight, kTop, kFar>(crosses);
}

// http://iquilezles.org/www/articles/frustumcorrect/frustumcorrect.htm
inline bool Frustum::IsBoxVisible(const glm::vec3 &minp, const glm::vec3 &maxp) const
{
	// check box outside/inside of frustum
	for (int i = 0; i < kCount; i++)
	{
		if ((glm::dot(planes_[i], glm::vec4(minp.x, minp.y, minp.z, 1.0f)) < 0.0) &&
			(glm::dot(planes_[i], glm::vec4(maxp.x, minp.y, minp.z, 1.0f)) < 0.0) &&
			(glm::dot(planes_[i], glm::vec4(minp.x, maxp.y, minp.z, 1.0f)) < 0.0) &&
			(glm::dot(planes_[i], glm::vec4(maxp.x, maxp.y, minp.z, 1.0f)) < 0.0) &&
			(glm::dot(planes_[i], glm::vec4(minp.x, minp.y, maxp.z, 1.0f)) < 0.0) &&
			(glm::dot(planes_[i], glm::vec4(maxp.x, minp.y, maxp.z, 1.0f)) < 0.0) &&
			(glm::dot(planes_[i], glm::vec4(minp.x, maxp.y, maxp.z, 1.0f)) < 0.0) &&
			(glm::dot(planes_[i], glm::vec4(maxp.x, maxp.y, maxp.z, 1.0f)) < 0.0))
		{
			return false;
		}
	}

	// check frustum outside/inside box
	int out;
	out = 0;
	for (int i = 0; i < 8; i++)
		out += ((points_[i].x > maxp.x) ? 1 : 0);
	if (out == 8)
		return false;
	out = 0;
	for (int i = 0; i < 8; i++)
		out += ((points_[i].x < minp.x) ? 1 : 0);
	if (out == 8)
		return false;
	out = 0;
	for (int i = 0; i < 8; i++)
		out += ((points_[i].y > maxp.y) ? 1 : 0);
	if (out == 8)
		return false;
	out = 0;
	for (int i = 0; i < 8; i++)
		out += ((points_[i].y < minp.y) ? 1 : 0);
	if (out == 8)
		return false;
	out = 0;
	for (int i = 0; i < 8; i++)
		out += ((points_[i].z > maxp.z) ? 1 : 0);
	if (out == 8)
		return false;
	out = 0;
	for (int i = 0; i < 8; i++)
		out += ((points_[i].z < minp.z) ? 1 : 0);
	if (out == 8)
		return false;

	return true;
}

template <Frustum::Planes a, Frustum::Planes b, Frustum::Planes c>
inline glm::vec3 Frustum::intersection(const glm::vec3 *crosses) const
{
	float D = glm::dot(glm::vec3(planes_[a]), crosses[ij2k<b, c>::k]);
	glm::vec3 res = glm::mat3(crosses[ij2k<b, c>::k], -crosses[ij2k<a, c>::k], crosses[ij2k<a, b>::k]) *
					glm::vec3(planes_[a].w, planes_[b].w, planes_[c].w);
	return res * (-1.0f / D);
}