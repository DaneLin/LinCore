#include <vk_engine.h>

int main(int argc, char *argv[])
{
	lincore::VulkanEngine engine;

	engine.Init();

	engine.Run();

	engine.CleanUp();

	return 0;
}
