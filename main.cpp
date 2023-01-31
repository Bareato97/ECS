#include "ecs.hpp"
#include "testsystem.hpp"
#include "testcomponent.hpp"

int main()
{
	ECS::ECS ecs;

	ecs.Init();

	ecs.RegisterComponent<test>();

	uint32_t player = ecs.CreateEntity();
	ecs.AddComponent(
		player,
		test{10}
	);

	TestSystem testSystem(&ecs);
	testSystem.RegisterComponentToSystem<test>();

	testSystem.RegisterEntity(player, ecs.GetEntitySignature(player));

	testSystem.printNumEntities();

	while(1)
	{
		testSystem.func();
	}

	return 0;
}