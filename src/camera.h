
#include <vk_types.h>
#include <SDL_events.h>

class Camera {
public:
	glm::vec3 velocity_;
	glm::vec3 position_;

	float pitch_{ 0.f };

	float yaw_{ 0.f };

	float speed_factor_{ 1.f };

	glm::mat4 GetViewMatrix();

	glm::mat4 GetRotationMatrix();

	void ProcessSdlEvent(SDL_Event& e);

	void Update();
};
