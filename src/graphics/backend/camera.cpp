﻿#include "camera.h"
// external
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include "imgui.h"

constexpr float MAX_MOVING_SPEED = 10.f;
constexpr float MIN_MOVING_SPEED = 0.01f;

void Camera::Init(glm::vec3 position, float fov, float aspect_ratio, float near_clip, float far_clip)
{
	position_ = position;
	fov_ = fov;
	aspect_ratio_ = aspect_ratio;
	near_clip_ = near_clip;
	far_clip_ = far_clip;
}

Camera &Camera::SetAspectRatio(float aspect_ratio)
{
	aspect_ratio_ = aspect_ratio;
	return *this;
}

Camera &Camera::SetFov(float fov)
{
	fov_ = fov;
	return *this;
}

Camera &Camera::SetNearClip(float near_clip)
{
	near_clip_ = near_clip;
	return *this;
}

Camera &Camera::SetFarClip(float far_clip)
{
	far_clip_ = far_clip;
	return *this;
}

glm::mat4 Camera::GetViewMatrix()
{
	glm::mat4 camera_translation = glm::translate(glm::mat4(1.f), position_);
	glm::mat4 camera_rotation = GetRotationMatrix();
	return glm::inverse(camera_translation * camera_rotation);
}

glm::mat4 Camera::GetRotationMatrix()
{
	glm::quat pitch_rotation = glm::angleAxis(pitch_, glm::vec3{1.f, 0.f, 0.f});
	glm::quat yaw_rotation = glm::angleAxis(yaw_, glm::vec3{0.f, -1.f, 0.f});

	return glm::toMat4(yaw_rotation) * glm::toMat4(pitch_rotation);
}

glm::mat4 Camera::GetProjectionMatrix()
{
	return glm::perspective(glm::radians(fov_), aspect_ratio_, near_clip_, far_clip_);
}

void Camera::ProcessSdlEvent(SDL_Event &e)
{
	// 获取 ImGui IO 状态
	ImGuiIO &io = ImGui::GetIO();

	// 如果 ImGui 需要捕获键盘输入或鼠标输入，则不进行相机的事件处理
	if (io.WantCaptureMouse || io.WantCaptureKeyboard)
	{
		return; // 如果 ImGui 正在捕获输入，直接返回，忽略相机控制
	}

	if (e.type == SDL_KEYDOWN)
	{
		if (e.key.keysym.sym == SDLK_w)
		{
			velocity_.z = -1.f;
		}
		if (e.key.keysym.sym == SDLK_s)
		{
			velocity_.z = 1.f;
		}
		if (e.key.keysym.sym == SDLK_a)
		{
			velocity_.x = -1.f;
		}
		if (e.key.keysym.sym == SDLK_d)
		{
			velocity_.x = 1.f;
		}
		if (e.key.keysym.sym == SDLK_e)
		{
			velocity_.y = 1.f;
		}
		if (e.key.keysym.sym == SDLK_q)
		{
			velocity_.y = -1.f;
		}
	}
	if (e.type == SDL_KEYUP)
	{
		if (e.key.keysym.sym == SDLK_w || e.key.keysym.sym == SDLK_s)
		{
			velocity_.z = 0.f;
		}
		if (e.key.keysym.sym == SDLK_a || e.key.keysym.sym == SDLK_d)
		{
			velocity_.x = 0.f;
		}
		if (e.key.keysym.sym == SDLK_e || e.key.keysym.sym == SDLK_q)
		{
			velocity_.y = 0.f;
		}
	}

	if (e.type == SDL_MOUSEMOTION)
	{

		// 检查右键是否按下
		if (e.motion.state & SDL_BUTTON(SDL_BUTTON_RIGHT))
		{
			// 只有右键按下时才更新视角
			yaw_ += e.motion.xrel * 0.001f;
			pitch_ -= e.motion.yrel * 0.001f;
		}
	}

	// 监听鼠标滚轮事件来调整速度
	if (e.type == SDL_MOUSEWHEEL)
	{
		if (e.wheel.y > 0)
		{																	   // 滚轮向上滚动，增加速度
			speed_factor_ = std::min(MAX_MOVING_SPEED, speed_factor_ + 0.01f); // 确保速度不会超过 MAX_MOVING_SPEED
		}
		else if (e.wheel.y < 0)
		{																	   // 滚轮向下滚动，减少速度
			speed_factor_ = std::max(MIN_MOVING_SPEED, speed_factor_ - 0.01f); // 确保速度不会低于 MIN_MOVING_SPEED
		}
	}
}

void Camera::Update()
{
	glm::mat4 camera_rotation = GetRotationMatrix();
	position_ += glm::vec3(camera_rotation * glm::vec4(velocity_ * speed_factor_, 0.f));
}
