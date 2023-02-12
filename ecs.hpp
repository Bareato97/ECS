/**
 * @file ecs.hpp
 * @author Isaac K.
 * 
 * This is my attempt at an entity component system
 * Deals with Entities, Components, Systems, which are all managed by a 'Coordinator' Coordinator acts as an interface to the entity, component and system managers
 * The ECS is essentially a datatable that systems use for referencing which components bound together, as well as to put components in packed arrays
 * No 'archtypes' are included in this scope
 * TODO: implement proper initial memory allocation
 * 
 * 
 */

#pragma once

#include <cinttypes>
#include <bitset>
#include <array>
#include <queue>
#include <assert.h>
#include <unordered_map>
#include <mutex>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <Bits.h>
/**
 * Entity is defined as just a unsigned integer
 * This is utilised by systems to link components together
 * Max_ENTITIES to be utilised in future expansion for memory management
 */
using Entity = std::uint32_t;
const Entity MAX_ENTITIES = 1028;

/**
 * Component types identified by a unique id
 * MAX_COMPONENTS used for memory/array management
 */
using ComponentType = std::uint32_t;
const ComponentType MAX_COMPONENTS = 32;

/**
 * Signature is the combinations of components and entity might 'have', an entity with a unique set of components will have a unique mask
 */
using Signature = std::bitset<MAX_COMPONENTS>;

namespace ECS
{
	/**
	 * @brief CRTP to generate unique componentids for each call with a unique template
	 * 			calling this function with the same template will return the same component id
	 * @return ComponentType 
	 */
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

	/**
	 * EntityManager maintains the list of current entity indicies, as well as an array of their signatures with the entity id being an index to the array
	 * Entities aren't bunched together
	 */
	class EntityManager
	{
		private:
			std::queue<Entity> freeEntities;
			std::array<Signature, MAX_ENTITIES> entitySignatures;
			uint32_t entityCount; // number of current entities used in the game state

		public:
			EntityManager()
			{
				// Create a queue with all potentially valid entity ids from 0 to MAX_ENTITIES - 1
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
	/**
	 * BaseComponentArray is used to abstract what component arrays are managed by the component manager, as a component array may have different
	 * 
	 */
	class BaseComponentArray
	{
		public:
			virtual ~BaseComponentArray() = default;
			virtual void EntityDestroyed(Entity entity) = 0;
	};

	/**
	 * Component array holds an of all components of a type, it is also responsible for the association of an entity to this component type
	 * It deals with the addition/removal of components from an entity
	 * @tparam CompType 
	 */
	template <typename CompType>
	class ComponentArray : public BaseComponentArray
	{
		private:
			std::array<CompType, MAX_ENTITIES> components;
			std::unordered_map<Entity, size_t> entityToIndex; // maps the entity to an array index value, the entityid is static but the index may change
			std::unordered_map<size_t, Entity> indexToEntity; 

			ComponentType componentType;

			size_t arraySize;

		public:
			ComponentArray()
			{
				componentType = GetComponentTypeID<CompType>();
			}

			// Associates a component with an entity, and creates the two way relation between them
			void InsertData(Entity entity, CompType component)
			{
				assert(entityToIndex.find(entity) == entityToIndex.end() && "Component added to same entity more than once.");

				size_t newIndex = arraySize;
				entityToIndex[entity] = newIndex;
				indexToEntity[newIndex] = entity;
				components[newIndex] = component;

				arraySize++;
			}

			/**
			 * Removes the component by entity value, and moves the last component in the list to the empty position to keep it tightly packed
			 * Entity to index and indextoentity are updated to reflect the shuffle
			 * @param entity 
			 */
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

			// returns the component associated with an entity
			CompType &Get(Entity entity)
			{
				assert(entityToIndex.find(entity) != entityToIndex.end() && "Entity does not have component.");

				return components[entityToIndex[entity]];
			}

			// wrapper function for RemoveData to be called when an entity is removed by other means, this should be called by the coordinator
			void EntityDestroyed(Entity entity) override
			{
				if(entityToIndex.find(entity) != entityToIndex.end())
					RemoveData(entity);
			}
	};

	/**
	 * The ComponentManager is responsible for mainting a collection of all component arrays
	 * It applies wrappers to the adding or removing component function 
	 */
	class ComponentManager
	{
		private:
			std::unordered_map<ComponentType, std::shared_ptr<BaseComponentArray>> componentArrays;

			// Returns a pointer to a specific component
			template<typename CompType>
			std::shared_ptr<ComponentArray<CompType>> GetComponentArray()
			{
				ComponentType componentID = GetComponentTypeID<CompType>();

				return std::static_pointer_cast<ComponentArray<CompType>>(componentArrays[componentID]);
			}

		public:
			// Initializes a new component array
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
			// wrapper function for adding a component to an entity. Component data is created in the arguments
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

			// calls entity destroyed for each component array
			void EntityDestroyed(Entity entity)
			{
				for(auto const& pair : componentArrays)
				{
					auto const& component = pair.second;

					component->EntityDestroyed(entity);
				}
			}
	};

	/**
	 * System is a base class for specialised systems,
	 * TODO: Implement transient systems for short lived entities
	 */
	class System
	{
		protected:
			std::vector<Entity> managedEntities; // list of entities managed by a system
			Signature systemSignature;			 // unique signature used to identify what components the system requires

		public:
			template <typename ComponentSignature>
			void RegisterComponentToSystem()
			{
				ComponentType compSignature = GetComponentTypeID<ComponentSignature>();

				systemSignature.set(compSignature, true);
			}
			/** registers an entity to be actioned by the system, should be expanded so that it can action an entity as long as it has all the required components
			 *	
			 */
			void RegisterEntityToSystem(Entity entity, Signature entitySignature)
			{
				assert(entitySignature == systemSignature && "Entity signature does not match system signature");

				managedEntities.emplace_back(entity);
			}

			void CheckEntity(Entity entity, Signature entitySignature)
			{
				if(entitySignature != systemSignature)
				{
					RemoveEntity(entity);
				}
			}

			void RemoveEntity(Entity entity)
			{
				std::remove(managedEntities.begin(), managedEntities.end(), entity);
			}

			virtual void Update(){};
	};
	/**
	 * System manager contains a list of all systems (should later be implemented as all active systems, maybe contain seperate lists?)
	 * This is used for iterating over systems
	 */
	class SystemManager
	{
		private:
			std::unordered_set<std::shared_ptr<System>> managedSystems;

		public:
			template<typename Sys>
			std::shared_ptr<Sys> RegisterSystem()
			{
				auto system = std::make_shared<Sys>();
				managedSystems.insert(system);
				return system;
			}

			template <typename Sys>
			void RegisterEntityToSystem(Entity entity, Signature entitySignature)
			{
				auto system = std::make_shared<Sys>();

				if (managedSystems.find(system) != managedSystems.end())
				{
					system->RegisterEntityToSystem(entity, entitySignature);
				}			
			}

			void RemoveEntity(System *system, Entity entity)
			{
				system->RemoveEntity(entity);
			}
			// Not to be used currently, should be a better management option for deleting and creating systems
			template <typename Sys>
			void RemoveSystem()
			{
				auto system = std::make_shared<Sys>();

				if (managedSystems.find(system) != managedSystems.end()){
					managedSystems.erase(system);
				}
			}

			void EntityDestroyed(Entity entity)
			{
				for(auto const& sys : managedSystems)
				{				
					auto const& system = sys;

					system->RemoveEntity(entity);
				}
			}

			void EvaluateEntity(Entity entity, Signature entitySignature){
				for (auto const &sys : managedSystems)
				{
					auto const &system = sys;

					system->CheckEntity(entity, entitySignature);
				}
			}

			void Update()
			{
				for (auto const &sys : managedSystems)
				{
					auto const &system = sys;

					system->Update();
				}
			}
	};

	/**
	 * ECS is the coordinator for the system, it contains a component and entity manager
	 * It mostly delegates tasks for these system, and acts as the interface for the main loop to interact
	 * with these containers
	 */
	class ECS
	{
		private:
			std::unique_ptr<ComponentManager> componentManager;
			std::unique_ptr<EntityManager> entityManager;
			std::unique_ptr<SystemManager> systemManager;

		public:
			void Init()
			{
				componentManager = std::make_unique<ComponentManager>();
				entityManager = std::make_unique<EntityManager>();
				systemManager = std::make_unique<SystemManager>();
			}
			Entity CreateEntity()
			{
				return entityManager->CreateEntity();
			}

			void DestroyEntity(Entity entity)
			{
				entityManager->DestroyEntity(entity);
				componentManager->EntityDestroyed(entity);
				systemManager->EntityDestroyed(entity);
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

}
