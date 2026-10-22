#include "ShowcaseProps.h"

#include "ContentPreview.h"
#include "RoadPaths.h"
#include "openrepublic/content/linear_network.h"
#include "openrepublic/content/terrain_dds.h"
#include "openrepublic/procgeo/props.h"
#include "openrepublic/procgeo/vehicles.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>

namespace openrepublic::preview
{
	using namespace openrepublic::content;
	namespace pg = openrepublic::procgeo;

	namespace
	{
		// Rotation applied to the install's vehicle meshes so their front follows the road direction.
		constexpr double kRealVehicleYawOffset = 0.0;

		float Hash01(int x, int z, std::uint32_t seed)
		{
			std::uint32_t hash = static_cast<std::uint32_t>(x) * 0x8da6b343u ^
				static_cast<std::uint32_t>(z) * 0xd8163841u ^ seed * 0x9e3779b9u;
			hash ^= hash >> 13;
			hash *= 0x85ebca6bu;
			hash ^= hash >> 16;
			hash *= 0xc2b2ae35u;
			hash ^= hash >> 15;
			return static_cast<float>(hash & 0x00ffffffu) / 16777215.0f;
		}

		std::uint32_t PackLinear(const pg::Color3& c)
		{
			return wi::Color::fromFloat4(XMFLOAT4(std::clamp(c.r, 0.0f, 1.0f), std::clamp(c.g, 0.0f, 1.0f),
				std::clamp(c.b, 0.0f, 1.0f), 1.0f)).rgba;
		}

		struct Placement
		{
			double x = 0.0;
			double z = 0.0;
			double yaw = 0.0;
			double pitch = 0.0;
			double y = 0.0;
		};

		void AddObject(wi::scene::Scene& scene, wi::ecs::Entity mesh, const Placement& p, const MapPlacementRequest& request,
			double centerX, double centerZ, float scale, bool castShadow, XMFLOAT4 tint = XMFLOAT4(1, 1, 1, 1))
		{
			const wi::ecs::Entity object = scene.Entity_CreateObject("");
			wi::scene::ObjectComponent* component = scene.objects.GetComponent(object);
			component->meshID = mesh;
			component->SetCastShadow(castShadow);
			component->color = tint;
			XMFLOAT4X4 matrix;
			XMStoreFloat4x4(&matrix, XMMatrixScaling(scale, scale, scale) *
				XMMatrixRotationRollPitchYaw(static_cast<float>(-p.pitch), static_cast<float>(p.yaw), 0.0f) *
				XMMatrixTranslation(static_cast<float>(p.x - centerX), static_cast<float>(p.y), static_cast<float>(p.z - centerZ)));
			scene.transforms.GetComponent(object)->MatrixTransform(matrix);
			(void)request;
		}
	}

	wi::ecs::Entity CreateProceduralMeshEntity(wi::scene::Scene& scene, const char* name, const pg::Mesh& source,
		float roughness, float metalness)
	{
		if (!source.Valid() || source.positions.empty())
		{
			return wi::ecs::INVALID_ENTITY;
		}
		std::vector<XMFLOAT3> positions;
		positions.reserve(source.positions.size());
		for (const pg::Vec3& p : source.positions)
		{
			positions.emplace_back(p.x, p.y, p.z);
		}
		const wi::ecs::Entity entity = scene.Entity_CreateMeshFromData(name, source.indices.size(),
			source.indices.data(), positions.size(), positions.data());
		wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(entity);
		wi::scene::MaterialComponent* material = scene.materials.GetComponent(entity);
		if (mesh == nullptr || material == nullptr)
		{
			return wi::ecs::INVALID_ENTITY;
		}
		mesh->vertex_colors.reserve(source.colors.size());
		for (const pg::Color3& c : source.colors)
		{
			mesh->vertex_colors.push_back(PackLinear(c));
		}
		mesh->CreateRenderData();
		material->SetUseVertexColors(true);
		material->SetBaseColor(XMFLOAT4(1, 1, 1, 1));
		material->SetRoughness(roughness);
		material->SetMetalness(metalness);
		material->SetReflectance(0.3f);
		material->SetDoubleSided(true);
		material->CreateRenderData(true);
		if (wi::scene::ObjectComponent* own = scene.objects.GetComponent(entity))
		{
			own->SetRenderable(false);
		}
		return entity;
	}

	bool BuildShowcaseProps(wi::scene::Scene& scene, const MapPlacementRequest& request,
		const ShowcaseTerrainData& terrain, double regionRadius, const std::filesystem::path& contentRoot,
		ShowcasePropsInfo& info, std::string& error)
	{
		const TerrainTransform transform = MakeMapTerrainTransform(request.metadata);
		const double cx = request.placement.X();
		const double cz = request.placement.Z();
		const auto heightAt = [&](double wx, double wz)
		{
			double h = 0.0;
			SampleTerrainHeight(request.heightmap, transform,
				std::min(std::max(wx, transform.originX), transform.originX + transform.sizeX),
				std::min(std::max(wz, transform.originZ), transform.originZ + transform.sizeZ), h, nullptr);
			return static_cast<double>(ShowcaseLandHeight(terrain, wx, wz, static_cast<float>(h)));
		};
		// Ground pose for an object of given length oriented along `yaw` (pitch from the slope).
		const auto pose = [&](double x, double z, double yaw, double length, double lift)
		{
			Placement p;
			p.x = x;
			p.z = z;
			p.yaw = yaw;
			const double fx = std::sin(yaw) * length * 0.5;
			const double fz = std::cos(yaw) * length * 0.5;
			const double front = heightAt(x + fx, z + fz);
			const double back = heightAt(x - fx, z - fz);
			p.y = (front + back) * 0.5 + lift;
			p.pitch = length > 0.5 ? std::atan2(front - back, length) : 0.0;
			return p;
		};

		// ---- Meshes (created once, instanced many times).
		struct VehicleType
		{
			pg::VehicleKind kind;
			std::uint32_t seed;
			wi::ecs::Entity mesh;
			float length;
		};
		std::vector<VehicleType> vehicleTypes;
		const std::pair<pg::VehicleKind, float> weights[] = {
			{ pg::VehicleKind::CargoTruck, 0.26f }, { pg::VehicleKind::Tanker, 0.10f }, { pg::VehicleKind::BoxVan, 0.16f },
			{ pg::VehicleKind::Car, 0.36f }, { pg::VehicleKind::Bus, 0.12f } };
		std::vector<float> weightTable;
		for (const auto& [kind, weight] : weights)
		{
			for (std::uint32_t variant = 0; variant < 4; ++variant)
			{
				VehicleType type{ kind, variant * 7u + 3u, wi::ecs::INVALID_ENTITY, pg::VehicleLength(kind) };
				type.mesh = CreateProceduralMeshEntity(scene, "showcase vehicle", pg::MakeVehicle(kind, type.seed), 0.45f, 0.1f);
				vehicleTypes.push_back(type);
				weightTable.push_back(weight);
			}
		}
		// Authentic vehicles from a local install (vehicles/<name>/main.nmf + material.mtl); never committed.
		struct RealVehicle
		{
			wi::ecs::Entity mesh = wi::ecs::INVALID_ENTITY;
			float length = 5.0f;
		};
		std::vector<RealVehicle> realCars, realTrucks, realBuses;
		if (!contentRoot.empty())
		{
			const auto loadVehicle = [&](const std::filesystem::path& folder) -> RealVehicle
			{
				RealVehicle vehicle;
				std::error_code fileError;
				const std::filesystem::path model = folder / "main.nmf";
				const std::filesystem::path material = folder / "material.mtl";
				if (!std::filesystem::is_regular_file(model, fileError) || !std::filesystem::is_regular_file(material, fileError))
				{
					return vehicle;
				}
				wi::ecs::Entity meshEntity = wi::ecs::INVALID_ENTITY;
				XMFLOAT3 ignoredTarget;
				float ignoredDistance = 0.0f;
				std::string d, a, e;
				if (!CreateNmfStaticPreview(scene, model, meshEntity, ignoredTarget, ignoredDistance, false, false,
					{ contentRoot }, material, d, a, e, nullptr, true, false))
				{
					return vehicle;
				}
				if (wi::scene::ObjectComponent* own = scene.objects.GetComponent(meshEntity))
				{
					own->SetRenderable(false);
				}
				if (const wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(meshEntity))
				{
					vehicle.length = std::max(3.0f, mesh->aabb.getHalfWidth().z * 2.0f);
					for (const auto& subset : mesh->subsets)
					{
						if (wi::scene::MaterialComponent* m = scene.materials.GetComponent(subset.materialID))
						{
							m->SetRoughness(0.5f);
							m->SetReflectance(0.35f);
							m->CreateRenderData(true);
						}
					}
				}
				vehicle.mesh = meshEntity;
				return vehicle;
			};
			const std::filesystem::path vehicleRoot = contentRoot / "vehicles";
			std::error_code listError;
			std::vector<std::filesystem::path> folders;
			for (const auto& entry : std::filesystem::directory_iterator(vehicleRoot, listError))
			{
				if (entry.is_directory())
				{
					folders.push_back(entry.path());
				}
			}
			std::sort(folders.begin(), folders.end());
			const auto take = [&](const char* prefix, std::vector<RealVehicle>& into, std::size_t limit)
			{
				std::size_t taken = 0;
				for (const auto& folder : folders)
				{
					if (taken >= limit)
					{
						break;
					}
					const std::string name = folder.filename().string();
					if (name.rfind(prefix, 0) != 0 || name.find("trail") != std::string::npos || name.find("train") != std::string::npos)
					{
						continue;
					}
					// Spread the pick over the alphabetical list so the variants differ.
					if (Hash01(static_cast<int>(name.size()), static_cast<int>(name.back()), 211) > 0.45f)
					{
						continue;
					}
					const RealVehicle loaded = loadVehicle(folder);
					if (loaded.mesh != wi::ecs::INVALID_ENTITY)
					{
						into.push_back(loaded);
						++taken;
					}
				}
			};
			take("personal_", realCars, 8);
			take("open_", realTrucks, 5);
			take("covered_", realTrucks, 5);
			take("oil_", realTrucks, 3);
			take("bus_", realBuses, 3);
		}
		info.realVehicleTypes = realCars.size() + realTrucks.size() + realBuses.size();
		const wi::ecs::Entity lampMesh = CreateProceduralMeshEntity(scene, "showcase street lamp", pg::MakeStreetLamp(), 0.6f, 0.4f);
		const wi::ecs::Entity poleMesh = CreateProceduralMeshEntity(scene, "showcase utility pole", pg::MakeWoodenUtilityPole(), 0.9f);
		const pg::Mesh pylonSource = pg::MakeSteelLatticePylon();
		const wi::ecs::Entity pylonMesh = CreateProceduralMeshEntity(scene, "showcase pylon", pylonSource, 0.6f, 0.5f);
		const wi::ecs::Entity railMesh = CreateProceduralMeshEntity(scene, "showcase rail", pg::MakeRailSegment(12.0f), 0.7f, 0.3f);
		const wi::ecs::Entity crossbuckMesh = CreateProceduralMeshEntity(scene, "showcase crossbuck", pg::MakeRailCrossbuck(), 0.6f);
		const wi::ecs::Entity chimneyMesh = CreateProceduralMeshEntity(scene, "showcase chimney", pg::MakeBrickChimney(), 0.9f);
		const wi::ecs::Entity towerMesh = CreateProceduralMeshEntity(scene, "showcase cooling tower", pg::MakeCoolingTower(), 0.9f);
		if (lampMesh == wi::ecs::INVALID_ENTITY || poleMesh == wi::ecs::INVALID_ENTITY)
		{
			error = "procedural prop meshes were rejected by the scene";
			return false;
		}
		pg::Vec3 pylonLo, pylonHi;
		pylonSource.Bounds(pylonLo, pylonHi);

		pg::Mesh wires; // every cable in one mesh, positions in centre-relative world metres
		const auto addWire = [&](double ax, double ay, double az, double bx, double by, double bz, float sag, float thickness)
		{
			const int segments = std::max(4, static_cast<int>(std::hypot(bx - ax, bz - az) / 6.0));
			wires.Append(pg::MakeCable({ static_cast<float>(ax - cx), static_cast<float>(ay), static_cast<float>(az - cz) },
				{ static_cast<float>(bx - cx), static_cast<float>(by), static_cast<float>(bz - cz) }, sag, segments, thickness));
		};

		// ---- Roads: vehicles, poles, lamps (following decoded curves where available).
		std::size_t vehicles = 0;
		std::size_t poles = 0;
		std::size_t lamps = 0;
		const double limit = regionRadius * 1.1;
		if (request.hasRoads)
		{
			const auto& nodes = request.roads.nodes;
			const RoadPathIndex roadPaths(request.roads);
			float weightSum = 0.0f;
			for (float w : weightTable) { weightSum += w; }
			for (std::size_t i = 0; i < nodes.size(); ++i)
			{
				for (const std::uint32_t j : nodes[i].neighbors)
				{
					if (j <= i || j >= nodes.size())
					{
						continue;
					}
					const double ax = nodes[i].x, az = nodes[i].z, bx = nodes[j].x, bz = nodes[j].z;
					const bool inside = (std::fabs(ax - cx) < limit && std::fabs(az - cz) < limit) ||
						(std::fabs(bx - cx) < limit && std::fabs(bz - cz) < limit);
					if (!inside)
					{
						continue;
					}
					const std::vector<RoadSample> samples = RoadPathIndex::Resample(
						roadPaths.Path(static_cast<std::uint32_t>(i), j), 3.0);
					if (samples.size() < 2)
					{
						continue;
					}
					const double length = samples.back().distance;
					if (length < 14.0)
					{
						continue;
					}
					// Position/direction at arc length d (samples are ~3 m apart).
					const auto at = [&](double d)
					{
						const std::size_t index = std::min(samples.size() - 1,
							static_cast<std::size_t>(std::max(0.0, d) / length * (samples.size() - 1) + 0.5));
						return samples[index];
					};
					const int seedA = static_cast<int>(i), seedB = static_cast<int>(j);
					// Vehicles: roughly one per 55 m per segment, in the lane of its travel direction.
					const int count = static_cast<int>(length / 55.0 + Hash01(seedA, seedB, 3));
					for (int k = 0; k < count && vehicles < 260; ++k)
					{
						const double t = length * (0.12 + 0.76 * Hash01(seedA * 31 + k, seedB, 5));
						const RoadSample s = at(t);
						const bool forward = Hash01(seedA + k, seedB, 7) < 0.5f;
						const double lane = forward ? -1.35 : 1.35;
						const double x = s.x + s.dz * lane;
						const double z = s.z - s.dx * lane;
						float pick = Hash01(seedA + k, seedB * 3, 11) * weightSum;
						std::size_t chosen = 0;
						for (std::size_t w = 0; w < weightTable.size(); ++w)
						{
							pick -= weightTable[w];
							if (pick <= 0.0f) { chosen = w; break; }
						}
						const VehicleType& type = vehicleTypes[chosen];
						wi::ecs::Entity vehicleMesh = type.mesh;
						float vehicleLength = type.length;
						double yawOffset = 0.0;
						{
							// Prefer the install's own vehicles by procedural category when available.
							const std::vector<RealVehicle>* pool = nullptr;
							switch (type.kind)
							{
							case pg::VehicleKind::Car: pool = &realCars; break;
							case pg::VehicleKind::Bus: pool = &realBuses; break;
							case pg::VehicleKind::CargoTruck: case pg::VehicleKind::Tanker: case pg::VehicleKind::BoxVan: case pg::VehicleKind::Trailer:
								pool = &realTrucks; break;
							}
							if (pool != nullptr && !pool->empty())
							{
								const RealVehicle& real = (*pool)[static_cast<std::size_t>(Hash01(seedA + k, seedB, 17) * pool->size()) % pool->size()];
								vehicleMesh = real.mesh;
								vehicleLength = real.length;
								yawOffset = kRealVehicleYawOffset;
							}
						}
						if (vehicleMesh == wi::ecs::INVALID_ENTITY)
						{
							continue;
						}
						const double yaw = std::atan2(s.dx, s.dz);
						const Placement p = pose(x, z, (forward ? yaw : yaw + 3.14159265) + yawOffset, vehicleLength, 0.3);
						AddObject(scene, vehicleMesh, p, request, cx, cz, 1.0f, true);
						++vehicles;
					}
					// Utility poles every 38 m on the left side with wires between consecutive poles,
					// street lamps every 50 m on the right side inside the town.
					if (length > 60.0)
					{
						Placement previous;
						bool havePrevious = false;
						double previousTop = 0.0;
						for (double t = 10.0; t < length - 8.0 && poles < 160; t += 38.0)
						{
							const RoadSample s = at(t);
							const double side = 5.2;
							const double x = s.x + s.dz * side;
							const double z = s.z - s.dx * side;
							Placement p = pose(x, z, std::atan2(s.dx, s.dz), 0.0, -0.1);
							AddObject(scene, poleMesh, p, request, cx, cz, 1.0f, false);
							++poles;
							const double top = p.y + 9.2;
							if (havePrevious)
							{
								for (int line = 0; line < 3; ++line)
								{
									const double across = (line - 1) * 0.95;
									addWire(previous.x + s.dz * across, previousTop - line * 0.05, previous.z - s.dx * across,
										p.x + s.dz * across, top - line * 0.05, p.z - s.dx * across, 0.55f, 0.04f);
								}
							}
							previous = p;
							previousTop = top;
							havePrevious = true;
						}
						const double distanceFromCenter = std::hypot((ax + bx) * 0.5 - cx, (az + bz) * 0.5 - cz);
						if (distanceFromCenter < regionRadius * 0.95)
						{
							for (double t = 18.0; t < length - 10.0 && lamps < 140; t += 50.0)
							{
								const RoadSample s = at(t);
								const double side = -4.6;
								const double x = s.x + s.dz * side;
								const double z = s.z - s.dx * side;
								AddObject(scene, lampMesh, pose(x, z, std::atan2(s.dx, s.dz) + 3.14159265 * 0.5, 0.0, -0.05), request, cx, cz, 1.0f, false);
								++lamps;
							}
						}
					}
				}
			}
		}

		// ---- Real power supports and railway from the package. Rail uses the
		// candidate ordered XYZ segment points when present, with node chords as fallback.
		std::size_t realPylons = 0;
		std::size_t realRail = 0;
		const double realRange = 3600.0;
		const auto loadNetwork = [&](const char* fileName, openrepublic::content::LinearNetwork& network)
		{
			std::ifstream stream(request.mapDirectory / fileName, std::ios::binary);
			if (!stream)
			{
				return false;
			}
			std::ostringstream buffer;
			buffer << stream.rdbuf();
			std::string bytes = buffer.str();
			std::string networkError;
			return openrepublic::content::ExtractLinearNetwork(bytes, network, &networkError) && !network.nodes.empty();
		};
		const auto inRange = [&](double x, double z)
		{
			return std::hypot(x - cx, z - cz) < realRange;
		};
		const auto stringPowerLine = [&](const openrepublic::content::LinearNetwork& network, wi::ecs::Entity supportMesh,
			double supportHeight, double wireSpacing, int wires, float sag, std::size_t cap)
		{
			std::size_t placed = 0;
			for (std::size_t i = 0; i < network.nodes.size() && placed < cap; ++i)
			{
				const auto& node = network.nodes[i];
				if (!inRange(node.x, node.z))
				{
					continue;
				}
				const double h = heightAt(node.x, node.z);
				double yaw = 0.0;
				for (const std::uint32_t j : network.nodes[i].links[0])
				{
					if (j < network.nodes.size())
					{
						yaw = std::atan2(network.nodes[j].x - node.x, network.nodes[j].z - node.z);
						break;
					}
				}
				Placement support;
				support.x = node.x; support.z = node.z; support.y = h - 0.3; support.yaw = yaw; support.pitch = 0.0;
				AddObject(scene, supportMesh, support, request, cx, cz, 1.0f, true);
				++placed;
				for (const std::uint32_t j : network.nodes[i].links[0])
				{
					if (j <= i || j >= network.nodes.size())
					{
						continue;
					}
					const auto& other = network.nodes[j];
					const double hb = heightAt(other.x, other.z);
					const double dxw = other.x - node.x, dzw = other.z - node.z;
					const double len = std::hypot(dxw, dzw);
					if (len < 5.0 || len > 1200.0)
					{
						continue;
					}
					const double nx = -dzw / len, nz = dxw / len;
					for (int w = 0; w < wires; ++w)
					{
						const double lateral = (w - (wires - 1) * 0.5) * wireSpacing;
						addWire(node.x + nx * lateral, h + supportHeight - (w % 2) * 1.2, node.z + nz * lateral,
							other.x + nx * lateral, hb + supportHeight - (w % 2) * 1.2, other.z + nz * lateral, sag, 0.08f);
					}
				}
			}
			return placed;
		};
		{
			openrepublic::content::LinearNetwork high;
			if (pylonMesh != wi::ecs::INVALID_ENTITY && loadNetwork("electro_high.bin", high))
			{
				realPylons += stringPowerLine(high, pylonMesh, static_cast<double>(pylonHi.y) * 0.85, 4.2, 6, 6.0f, 400);
			}
			openrepublic::content::LinearNetwork low;
			if (loadNetwork("electro_low.bin", low))
			{
				realPylons += stringPowerLine(low, poleMesh, 9.0, 0.9, 3, 1.2f, 600);
			}
			// Disabled: the links between rail nodes cross the town's buildings in the sample save, so the
			// neighbour interpretation (links[0]) or the straight-span assumption is not validated yet.
			// Re-enable after the OVERLAYS worker establishes the rail segment layout.
			constexpr bool kUseRealRail = true;
			for (const char* fileName : { "rail.bin", "rail_secondary.bin" })
			{
				if (!kUseRealRail)
				{
					break;
				}
				openrepublic::content::LinearNetwork rail;
				if (railMesh == wi::ecs::INVALID_ENTITY || !loadNetwork(fileName, rail))
				{
					continue;
				}
				for (const auto& segment : rail.segments)
				{
					if (realRail >= 6000 || segment.nodeA >= rail.nodes.size() || segment.nodeB >= rail.nodes.size())
					{
						break;
					}
					const auto chordFallback = [&]()
					{
						const auto& a = rail.nodes[segment.nodeA];
						const auto& b = rail.nodes[segment.nodeB];
						const double dx = b.x - a.x, dz = b.z - a.z;
						const double length = std::hypot(dx, dz);
						if (length < 1.0 || length > 3000.0 || (!inRange(a.x, a.z) && !inRange(b.x, b.z))) return;
						const double yaw = std::atan2(dx, dz);
						for (double t = 6.0; t < length && realRail < 6000; t += 12.0)
						{
							const double x = a.x + dx * t / length;
							const double z = a.z + dz * t / length;
							const double h = heightAt(x, z);
							if (terrain.hasWater && h < terrain.waterLevel + 0.8) continue;
							AddObject(scene, railMesh, pose(x, z, yaw, 12.0, 0.15), request, cx, cz, 1.0f, false);
							++realRail;
						}
					};
					if (segment.points.size() < 2)
					{
						chordFallback();
						continue;
					}
					double pathLength = 0.0;
					for (std::size_t p = 1; p < segment.points.size(); ++p)
					{
						const auto& a = segment.points[p - 1];
						const auto& b = segment.points[p];
						pathLength += std::sqrt(std::pow(static_cast<double>(b.x) - a.x, 2.0) +
							std::pow(static_cast<double>(b.y) - a.y, 2.0) +
							std::pow(static_cast<double>(b.z) - a.z, 2.0));
					}
					if (!(pathLength > 1.0) || pathLength > 10000.0 ||
						std::none_of(segment.points.begin(), segment.points.end(), [&](const auto& p) { return inRange(p.x, p.z); }))
					{
						continue;
					}
					const std::size_t sampleCount = std::max<std::size_t>(2, static_cast<std::size_t>(std::ceil(pathLength / 12.0)) + 1);
					const auto samples = SampleRoadPolyline(segment.points, sampleCount);
					for (std::size_t p = 1; p < samples.size() && realRail < 6000; ++p)
					{
						const auto& a = samples[p - 1];
						const auto& b = samples[p];
						const double dx = static_cast<double>(b.x) - a.x;
						const double dz = static_cast<double>(b.z) - a.z;
						const double horizontalLength = std::hypot(dx, dz);
						if (horizontalLength < 0.1 || !inRange((a.x + b.x) * 0.5, (a.z + b.z) * 0.5)) continue;
						const double x = (a.x + b.x) * 0.5;
						const double z = (a.z + b.z) * 0.5;
						const double y = (a.y + b.y) * 0.5 + 0.15;
						if (terrain.hasWater && heightAt(x, z) < terrain.waterLevel + 0.8) continue;
						Placement placement;
						placement.x = x; placement.y = y; placement.z = z;
						placement.yaw = std::atan2(dx, dz);
						placement.pitch = std::atan2(static_cast<double>(b.y) - a.y, horizontalLength);
						AddObject(scene, railMesh, placement, request, cx, cz, 1.0f, false);
						++realRail;
					}
				}
			}
		}

		// ---- Fallback pylon line crossing the landscape on the far side of the town (only without real data).
		std::size_t pylons = 0;
		if (pylonMesh != wi::ecs::INVALID_ENTITY && realPylons == 0)
		{
			const double angle = 0.9;
			const double ux = std::cos(angle), uz = std::sin(angle);
			const double px = -uz, pz = ux; // offset perpendicular to the line direction
			const double offset = regionRadius * 1.15;
			double prevX = 0.0, prevZ = 0.0, prevY = 0.0;
			bool havePrev = false;
			const double width = static_cast<double>(pylonHi.x - pylonLo.x) * 0.45;
			for (int n = -7; n <= 7; ++n)
			{
				const double x = cx + px * offset + ux * n * 260.0;
				const double z = cz + pz * offset + uz * n * 260.0;
				if (x < transform.originX || z < transform.originZ || x > transform.originX + transform.sizeX || z > transform.originZ + transform.sizeZ)
				{
					havePrev = false;
					continue;
				}
				const double h = heightAt(x, z);
				if (terrain.hasWater && h < terrain.waterLevel + 1.5)
				{
					havePrev = false;
					continue;
				}
				Placement p;
				p.x = x; p.z = z; p.y = h - 0.3; p.yaw = angle; p.pitch = 0.0;
				AddObject(scene, pylonMesh, p, request, cx, cz, 1.0f, true);
				++pylons;
				if (havePrev)
				{
					for (int arm = 0; arm < 3; ++arm)
					{
						const double top = (0.93 - 0.13 * arm) * static_cast<double>(pylonHi.y);
						for (int side = -1; side <= 1; side += 2)
						{
							const double lateral = side * width * (1.0 - 0.18 * arm);
							addWire(prevX + (-uz) * lateral, prevY + top, prevZ + ux * lateral,
								x + (-uz) * lateral, p.y + top, z + ux * lateral, 7.5f, 0.07f);
						}
					}
				}
				prevX = x; prevZ = z; prevY = p.y; havePrev = true;
			}
		}

		// ---- Railway: a long gentle line tangent to the town, piece by piece along the terrain.
		std::size_t railPieces = realRail;
		if (railMesh != wi::ecs::INVALID_ENTITY && realRail == 0)
		{
			const double angle = -0.55;
			const double ux = std::cos(angle), uz = std::sin(angle);
			const double px = -uz, pz = ux;
			const double offset = -regionRadius * 1.02;
			for (int n = -70; n <= 70; ++n)
			{
				const double x = cx + px * offset + ux * n * 12.0 + std::sin(n * 0.05) * 18.0 * px;
				const double z = cz + pz * offset + uz * n * 12.0 + std::sin(n * 0.05) * 18.0 * pz;
				if (x < transform.originX || z < transform.originZ || x > transform.originX + transform.sizeX || z > transform.originZ + transform.sizeZ)
				{
					continue;
				}
				const double h = heightAt(x, z);
				if (terrain.hasWater && h < terrain.waterLevel + 1.0)
				{
					continue;
				}
				AddObject(scene, railMesh, pose(x, z, std::atan2(ux, uz), 12.0, 0.15), request, cx, cz, 1.0f, false);
				++railPieces;
			}
		}

		// ---- Industrial skyline: chimney and cooling tower beyond the town edge.
		const auto industrial = [&](wi::ecs::Entity mesh, double dx, double dz, double smokeHeight)
		{
			if (mesh == wi::ecs::INVALID_ENTITY)
			{
				return;
			}
			const double x = cx + dx, z = cz + dz;
			const double h = heightAt(x, z);
			if (terrain.hasWater && h < terrain.waterLevel + 2.0)
			{
				return;
			}
			Placement p;
			p.x = x; p.z = z; p.y = h - 0.3; p.yaw = 0.0; p.pitch = 0.0;
			AddObject(scene, mesh, p, request, cx, cz, 1.0f, true);
			if (smokeHeight > 0.0)
			{
				// Slowly rising, drifting grey plume from the top of the stack.
				const wi::ecs::Entity emitterEntity = scene.Entity_CreateEmitter("showcase smoke",
					XMFLOAT3(static_cast<float>(x - cx), static_cast<float>(h + smokeHeight), static_cast<float>(z - cz)));
				if (wi::EmittedParticleSystem* emitter = scene.emitters.GetComponent(emitterEntity))
				{
					emitter->count = 22.0f;
					emitter->size = 5.0f;
					emitter->life = 26.0f;
					emitter->random_life = 0.4f;
					emitter->normal_factor = 0.0f;
					emitter->random_factor = 0.5f;
					emitter->velocity = XMFLOAT3(2.2f, 3.2f, 0.8f);
					emitter->scaleX = 2.6f;
					emitter->mass = -0.02f;
					emitter->drag = 0.99f;
					emitter->rotation = 1.0f;
				}
				if (wi::scene::MaterialComponent* material = scene.materials.GetComponent(emitterEntity))
				{
					material->SetBaseColor(XMFLOAT4(0.55f, 0.53f, 0.50f, 0.6f));
					material->userBlendMode = wi::enums::BLENDMODE_ALPHA;
					material->textures[wi::scene::MaterialComponent::BASECOLORMAP].resource = ShowcaseSoftCircleTexture();
					material->textures[wi::scene::MaterialComponent::BASECOLORMAP].uvset = 0;
				}
			}
		};
		industrial(chimneyMesh, regionRadius * 1.6, -regionRadius * 0.5, 60.0);
		industrial(towerMesh, regionRadius * 1.9, -regionRadius * 0.15, 40.0);
		industrial(chimneyMesh, regionRadius * 1.75, regionRadius * 0.1, 60.0);
		(void)crossbuckMesh;

		// ---- Wires: one static mesh, lit and shadowed like the rest.
		if (!wires.indices.empty())
		{
			const wi::ecs::Entity wireEntity = CreateProceduralMeshEntity(scene, "showcase wires", wires, 0.5f, 0.6f);
			if (wireEntity != wi::ecs::INVALID_ENTITY)
			{
				if (wi::scene::ObjectComponent* object = scene.objects.GetComponent(wireEntity))
				{
					object->SetRenderable(true);
					object->SetCastShadow(false);
				}
			}
		}

		info.vehicles = vehicles;
		info.poles = poles;
		info.lamps = lamps;
		info.pylons = pylons + realPylons;
		info.railPieces = railPieces;
		std::ostringstream summary;
		summary << "Showcase props (OpenRepublic procedural meshes" << (info.realVehicleTypes > 0 ? std::string(" plus ") + std::to_string(info.realVehicleTypes) + " local-install vehicle types" : std::string()) << "): " << vehicles << " vehicles on roads, " << poles
			<< " utility poles with wires, " << lamps << " street lamps, " << pylons << " pylons, " << railPieces
			<< " railway pieces, industrial skyline.";
		info.summary = summary.str();
		return true;
	}
}
