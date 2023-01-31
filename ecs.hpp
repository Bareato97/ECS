#pragma once

#include <cinttypes>
#include <bitset>
#include <array>
#include <queue>
#include <assert.h>
#include <unordered_map>
#include <mutex>
#include <set>
#include <memory>
#include <algorithm>

using Entity = std::uint32_t;
const Entity MAX_ENTITIES = 1028;

using ComponentType = std::uint32_t;
const ComponentType MAX_COMPONENTS = 32;

using Signature = std::bitset<MAX_COMPONENTS>;

namespace ECS
{
	inline ComponentType GetComponentTypeID()
	{
		static ComponentType componentTypeID = 0u;
		return componentTypeID++;
	}

	template <typename CompType>
	inline ComponentType GetComponentTypeID() noexcept
	{
		static ComponentType componentTypeID = GetComponentTypeID();
		return componentTypeID;
	}

	class EntityManager
	{
		private:
			std::queue<Entity> freeEntities;
			std::array<Signature, MAX_ENTITIES> entitySignatures;
			uint32_t entityCount;

		public:
			EntityManager()
			{
				for (Entity index = 0; index < MAX_ENTITIES; ++index)
				{
					freeEntities.push(index);
				}
			}

			Entity CreateEntity()
			{
				assert(entityCount < MAX_ENTITIES && "Too many entities");

				Entity id = freeEntities.front();
				freeEntities.pop();
				entityCount++;

				return id;
			}

			void DestroyEntity(Entity entity)
			{
				assert(entityCount < MAX_ENTITIES && "Entity out of range");

				entitySignatures[entity].reset();

				freeEntities.push(entity);

				entityCount--;
			}

			void SetSignature(Entity entity, Signature signature)
			{

				assert(entity < MAX_ENTITIES && "Entity out of range");

				entitySignatures[entity] = signature;
			}

			Signature GetSignature(Entity entity)
			{
				assert(entity < MAX_ENTITIES && "Entity out of range");

				return entitySignatures[entity];
			}
	};

	class BaseComponentArray
	{
		public:
			virtual ~BaseComponentArray() = default;
			virtual void EntityDestroyed(Entity entity) = 0;
	};

	template <typename CompType>
	class ComponentArray : public BaseComponentArray
	{
		private:
			std::array<CompType, MAX_ENTITIES> components;
			std::unordered_map<Entity, size_t> entityToIndex;
			std::unordered_map<size_t, Entity> indexToEntity;

			ComponentType componentType;

			size_t arraySize;

		public:
			ComponentArray()
			{
				componentType = GetComponentTypeID<CompType>();
			}

			void InsertData(Entity entity, CompType component)
			{
				assert(entityToIndex.find(entity) == entityToIndex.end() && "Component added to same entity more than once.");

				size_t newIndex = arraySize;
				entityToIndex[entity] = newIndex;
				indexToEntity[newIndex] = entity;
				components[newIndex] = component;

				arraySize++;
			}

			void RemoveData(Entity entity)
			{
				assert(entityToIndex.find(entity) != entityToIndex.end() && "Entity does not have component to remove.");

				size_t removedEntity = entityToIndex[entity];
				size_t lastElement = arraySize - 1;
				components[removedEntity] = components[lastElement];

				// Update map to point to moved spot
				Entity lastElementEntity = indexToEntity[lastElement];
				indexToEntity[lastElementEntity] = removedEntity;
				indexToEntity[removedEntity] = lastElementEntity;

				entityToIndex.erase(entity);
				indexToEntity.erase(lastElement);

				arraySize--;
			}

			CompType &Get(Entity entity)
			{
				assert(entityToIndex.find(entity) != entityToIndex.end() && "Entity does not have component.");

				return components[entityToIndex[entity]];
			}

			void EntityDestroyed(Entity entity) override
			{
				if(entityToIndex.find(entity) != entityToIndex.end())
					RemoveData(entity);
			}
	};

	class ComponentManager
	{
		private:
			std::unordered_map<ComponentType, std::shared_ptr<BaseComponentArray>> componentArrays;

			template<typename CompType>
			std::shared_ptr<ComponentArray<CompType>> GetComponentArray()
			{
				ComponentType componentID = GetComponentTypeID<CompType>();

				return std::static_pointer_cast<ComponentArray<CompType>>(componentArrays[componentID]);
			}

		public:
			template<typename CompType>
			void RegisterComponent()
			{
				componentArrays.insert({GetComponentTypeID<CompType>(), std::make_shared<ComponentArray<CompType>>()});
			}

			template<typename Comptype>
			ComponentType GetComponentType()
			{
				ComponentType componentID = GetComponentTypeID<Comptype>();

				return componentID;
			}

			template <typename Comptype>
			void AddComponent(Entity entity, Comptype component)
			{
				GetComponentArray<Comptype>()->InsertData(entity, component);
			}

			template <typename Comptype>
			void RemoveComponent(Entity entity, Comptype component)
			{
				GetComponentArray<Comptype>()->RemoveData(entity, component);
			}

			template <typename Comptype>
			Comptype &GetComponent(Entity entity)
			{
				return GetComponentArray<Comptype>()->Get(entity);
			}

			void EntityDestroyed(Entity entity)
			{
				for(auto const& pair : componentArrays)
				{
					auto const& component = pair.second;

					component->EntityDestroyed(entity);
				}
			}
	};

	class ECS
	{
		private:
			std::unique_ptr<ComponentManager> componentManager;
			std::unique_ptr<EntityManager> entityManager;

		public:
			void Init()
			{
				componentManager = std::make_unique<ComponentManager>();
				entityManager = std::make_unique<EntityManager>();
			}
			Entity CreateEntity()
			{
				return entityManager->CreateEntity();
			}

			void DestroyEntity(Entity entity)
			{
				entityManager->DestroyEntity(entity);
				componentManager->EntityDestroyed(entity);
			}

			template<typename CompType>
			void RegisterComponent()
			{
				componentManager->RegisterComponent<CompType>();
			}

			template <typename CompType>
			void AddComponent(Entity entity, CompType component)
			{
				componentManager->AddComponent<CompType>(entity, component);

				auto signature = entityManager->GetSignature(entity);
				signature.set(componentManager->GetComponentType<CompType>(), true);
				entityManager->SetSignature(entity, signature);
			}

			template <typename CompType>
			void RemoveComponent(Entity entity)
			{
				componentManager->RemoveComponent<CompType>();

				auto signature = entityManager->GetSignature(entity);
				signature.set(componentManager->GetComponentType<CompType>(), false);
				entityManager->SetSignature(entity, signature);
			}

			template <typename CompType>
			CompType &GetComponent(Entity entity)
			{
				return componentManager->GetComponent<CompType>(entity);
			}

			template <typename CompType>
			ComponentType GetComponentType()
			{
				return componentManager->GetComponentType<CompType>();
			}

			Signature GetEntitySignature(Entity entity)
			{
				return entityManager->GetSignature(entity);
			}
	};

	class System
	{
		protected:
			std::vector<Entity> managedEntities;
			Signature systemSignature;
			ECS *managingECS;

		public:
			System(ECS *ecs)
			{
				managingECS = ecs;
			}
			template <typename ComponentSignature>
			void RegisterComponentToSystem()
			{
				ComponentType compSignature = GetComponentTypeID<ComponentSignature>();

				systemSignature.set(compSignature, true);
			}

			void RegisterEntity(Entity entity, Signature entitySignature)
			{
				assert(entitySignature == systemSignature && "Entity signature does not match system signature");

				managedEntities.emplace_back(entity);
			}

			void RemoveEntity(Entity entity)
			{
				std::remove(managedEntities.begin(), managedEntities.end(), entity);
			}
	};
}
