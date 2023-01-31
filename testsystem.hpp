#pragma once

#include "ecs.hpp"
#include "testcomponent.hpp"

#include <iostream>

class TestSystem : public ECS::System
{
	public:
		TestSystem(ECS::ECS *ecs) : System(ecs){};
		void func()
		{
			for (auto &entity : managedEntities)
			{
				auto &testComponent = managingECS->GetComponent<test>(entity);

				if (testComponent.someValue < 100)
				{
					std::cout << testComponent.someValue++ << std::endl;
				}
			}
		}

		void printNumEntities()
		{
			std::cout << "Number of managed entities: " << managedEntities.size() << std::endl;
		}
};