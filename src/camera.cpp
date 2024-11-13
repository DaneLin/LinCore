#include "camera.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

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
		if (e.key.keysym.sym == SDLK_w) {
			velocity.z = 0.f;
		}
		if (e.key.keysym.sym == SDLK_s) {
			velocity.z = 0.f;
		}
		if (e.key.keysym.sym == SDLK_a) {
			velocity.x = 0.f;
		}
		if (e.key.keysym.sym == SDLK_d) {
			velocity.x = 0.f;
		}
		if (e.key.keysym.sym == SDLK_e) {
			velocity.y = 0.f;
		}
		if (e.key.keysym.sym == SDLK_q) {
			velocity.y = 0.f;
		}
	}

	if (e.type == SDL_MOUSEMOTION) {
		yaw += e.motion.xrel * .001f;
		pitch -= e.motion.yrel * .001f;
	}
}

void Camera::update()
{
    glm::mat4 cameraRotation = get_rotation_matrix();
    position += glm::vec3(cameraRotation * glm::vec4(velocity * .1f, 0.f));
}
