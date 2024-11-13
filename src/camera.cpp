#include "camera.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

constexpr float MAX_MOVING_SPEED = 10.f;
constexpr float MIN_MOVING_SPEED = 0.1f;

glm::mat4 Camera::get_view_matrix()
{
	glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
	glm::mat4 cameraRotation = get_rotation_matrix();
	return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::get_rotation_matrix()
{
	glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{ 1.f, 0.f, 0.f });
	glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{ 0.f, -1.f, 0.f });

	return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

void Camera::process_sdl_event(SDL_Event& e)
{
	if (e.type == SDL_KEYDOWN) {
		if (e.key.keysym.sym == SDLK_w) {
			velocity.z = -1.f;
		}
		if (e.key.keysym.sym == SDLK_s) {
			velocity.z = 1.f;
		}
		if (e.key.keysym.sym == SDLK_a) {
			velocity.x = -1.f;
		}
		if (e.key.keysym.sym == SDLK_d) {
			velocity.x = 1.f;
		}
		if (e.key.keysym.sym == SDLK_e) {
			velocity.y = 1.f;
		}
		if (e.key.keysym.sym == SDLK_q) {
			velocity.y = -1.f;
		}
	}
	if (e.type == SDL_KEYUP) {
		if (e.key.keysym.sym == SDLK_w || e.key.keysym.sym == SDLK_s) {
			velocity.z = 0.f;
		}
		if (e.key.keysym.sym == SDLK_a || e.key.keysym.sym == SDLK_d) {
			velocity.x = 0.f;
		}
		if (e.key.keysym.sym == SDLK_e || e.key.keysym.sym == SDLK_q) {
			velocity.y = 0.f;
		}
	}

	if (e.type == SDL_MOUSEMOTION) {
		
		// 检查右键是否按下
		if (e.motion.state & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
			// 只有右键按下时才更新视角
			yaw += e.motion.xrel * 0.001f;
			pitch -= e.motion.yrel * 0.001f;
		}
	}

	// 监听鼠标滚轮事件来调整速度
	if (e.type == SDL_MOUSEWHEEL) {
		if (e.wheel.y > 0) { // 滚轮向上滚动，增加速度
			speedFactor = std::min(MAX_MOVING_SPEED, speedFactor + 0.1f); // 确保速度不会超过 MAX_MOVING_SPEED
		}
		else if (e.wheel.y < 0) { // 滚轮向下滚动，减少速度
			speedFactor = std::max(MIN_MOVING_SPEED, speedFactor - 0.1f); // 确保速度不会低于 MIN_MOVING_SPEED

		}
	}
}

void Camera::update()
{
    glm::mat4 cameraRotation = get_rotation_matrix();
    position += glm::vec3(cameraRotation * glm::vec4(velocity * speedFactor, 0.f));
}
