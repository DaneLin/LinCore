#pragma once

#include <SDL_events.h>

class Camera {
public:

	glm::vec3 velocity_{ 0.f };
	glm::vec3 position_{ 0.f };

	float pitch_{ 0.f };

	float yaw_{ 0.f };

	float speed_factor_{ 0.1f };

	float fov_{ 45.f };

	float aspect_ratio_{ 1.f };

	float near_clip_{ 0.1f };

	float far_clip_{ 100.f };

	void Init(glm::vec3 position, float fov, float aspect_ratio, float near_clip, float far_clip);

	Camera& SetAspectRatio(float aspect_ratio);

	Camera& SetFov(float fov);

	Camera& SetNearClip(float near_clip);

	Camera& SetFarClip(float far_clip);

	glm::mat4 GetViewMatrix();

	glm::mat4 GetRotationMatrix();

	glm::mat4 GetProjectionMatrix();

	void ProcessSdlEvent(SDL_Event& e);

	void Update();
};
