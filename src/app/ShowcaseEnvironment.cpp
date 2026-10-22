#include "ShowcaseEnvironment.h"

#include "openrepublic/content/terrain_dds.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <sstream>

namespace openrepublic::preview
{
	using namespace openrepublic::content;

	namespace
	{
		constexpr double kFarHalfExtent = 7000.0;
		constexpr std::uint32_t kFarSamples = 769;
		constexpr float kPi = 3.14159265358979f;

		std::uint32_t HashInts(int x, int z, std::uint32_t seed)
		{
			std::uint32_t hash = static_cast<std::uint32_t>(x) * 0x8da6b343u ^
				static_cast<std::uint32_t>(z) * 0xd8163841u ^ seed * 0x9e3779b9u;
			hash ^= hash >> 13;
			hash *= 0x85ebca6bu;
			hash ^= hash >> 16;
			hash *= 0xc2b2ae35u;
			hash ^= hash >> 15;
			return hash;
		}

		float Hash01(int x, int z, std::uint32_t seed = 0)
		{
			return static_cast<float>(HashInts(x, z, seed) & 0x00ffffffu) / 16777215.0f;
		}

		float Smooth(float t)
		{
			return t * t * (3.0f - 2.0f * t);
		}

		float SmoothStep(float a, float b, float v)
		{
			if (a == b)
			{
				return v < a ? 0.0f : 1.0f;
			}
			return Smooth(std::clamp((v - a) / (b - a), 0.0f, 1.0f));
		}

		float ValueNoise(double x, double z, std::uint32_t seed = 0)
		{
			const int x0 = static_cast<int>(std::floor(x));
			const int z0 = static_cast<int>(std::floor(z));
			const float tx = Smooth(static_cast<float>(x - x0));
			const float tz = Smooth(static_cast<float>(z - z0));
			const float a = wi::math::Lerp(Hash01(x0, z0, seed), Hash01(x0 + 1, z0, seed), tx);
			const float b = wi::math::Lerp(Hash01(x0, z0 + 1, seed), Hash01(x0 + 1, z0 + 1, seed), tx);
			return wi::math::Lerp(a, b, tz);
		}

		float Fbm(double x, double z, int octaves, std::uint32_t seed = 0)
		{
			float sum = 0.0f;
			float amplitude = 0.5f;
			float norm = 0.0f;
			for (int i = 0; i < octaves; ++i)
			{
				sum += amplitude * ValueNoise(x, z, seed + static_cast<std::uint32_t>(i) * 101u);
				norm += amplitude;
				x *= 2.03;
				z *= 2.03;
				amplitude *= 0.5f;
			}
			return sum / norm;
		}

		XMFLOAT3 Mix(const XMFLOAT3& a, const XMFLOAT3& b, float t)
		{
			t = std::clamp(t, 0.0f, 1.0f);
			return XMFLOAT3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
		}

		// How forested a spot is, 0..1, shared by the terrain shading and the tree scatter.
		float ForestAmount(const ShowcaseTerrainData& data, double worldX, double worldZ)
		{
			const float forest = Fbm(worldX / 430.0 + 31.0, worldZ / 430.0 - 17.0, 4, 7);
			const double dist = std::hypot(worldX - data.centerX, worldZ - data.centerZ);
			const float awayFromCity = SmoothStep(static_cast<float>(data.nearHalfExtent) * 0.9f,
				static_cast<float>(data.nearHalfExtent) * 1.8f, static_cast<float>(dist));
			return SmoothStep(0.47f, 0.62f, forest) * (0.25f + 0.75f * awayFromCity);
		}

		// Height above the lake/sea level (or the lowest terrain) over a nominal 380 m mountain range.
		float RelativeElevation(const ShowcaseTerrainData& data, float height)
		{
			const float base = data.hasWater ? data.waterLevel : data.grid.minHeight;
			return std::clamp((height - base) / 380.0f, 0.0f, 1.0f);
		}

		std::uint32_t PackColor(const XMFLOAT3& color)
		{
			return wi::Color::fromFloat4(XMFLOAT4(
				std::clamp(color.x, 0.0f, 1.0f), std::clamp(color.y, 0.0f, 1.0f),
				std::clamp(color.z, 0.0f, 1.0f), 1.0f)).rgba;
		}

		// RGBA8 image with a full box-filtered mip chain, uploaded as an sRGB texture.
		wi::Resource MakeTexture(std::uint32_t width, std::uint32_t height, std::vector<std::uint8_t> base)
		{
			std::vector<std::vector<std::uint8_t>> levels;
			std::vector<std::pair<std::uint32_t, std::uint32_t>> sizes;
			levels.push_back(std::move(base));
			sizes.emplace_back(width, height);
			while (sizes.back().first > 1 && sizes.back().second > 1)
			{
				const auto [w, h] = sizes.back();
				const std::uint32_t nw = w / 2;
				const std::uint32_t nh = h / 2;
				std::vector<std::uint8_t> next(static_cast<std::size_t>(nw) * nh * 4);
				const auto& src = levels.back();
				for (std::uint32_t y = 0; y < nh; ++y)
				{
					for (std::uint32_t x = 0; x < nw; ++x)
					{
						for (int c = 0; c < 4; ++c)
						{
							const int sum = src[((2 * y) * w + 2 * x) * 4 + c] + src[((2 * y) * w + 2 * x + 1) * 4 + c] +
								src[((2 * y + 1) * w + 2 * x) * 4 + c] + src[((2 * y + 1) * w + 2 * x + 1) * 4 + c];
							next[(y * nw + x) * 4 + c] = static_cast<std::uint8_t>((sum + 2) / 4);
						}
					}
				}
				levels.push_back(std::move(next));
				sizes.emplace_back(nw, nh);
			}
			wi::graphics::TextureDesc desc;
			desc.type = wi::graphics::TextureDesc::Type::TEXTURE_2D;
			desc.width = width;
			desc.height = height;
			desc.mip_levels = static_cast<std::uint32_t>(levels.size());
			desc.format = wi::graphics::Format::R8G8B8A8_UNORM_SRGB;
			desc.bind_flags = wi::graphics::BindFlag::SHADER_RESOURCE;
			std::vector<wi::graphics::SubresourceData> data(levels.size());
			for (std::size_t i = 0; i < levels.size(); ++i)
			{
				data[i].data_ptr = levels[i].data();
				data[i].row_pitch = sizes[i].first * 4;
			}
			wi::graphics::Texture texture;
			wi::Resource resource;
			if (wi::graphics::GetDevice() != nullptr &&
				wi::graphics::GetDevice()->CreateTexture(&desc, data.data(), &texture))
			{
				resource.SetTexture(texture);
			}
			return resource;
		}

		// Tileable value noise (period in lattice cells).
		float TileNoise(double x, double y, int period, std::uint32_t seed)
		{
			const int x0 = static_cast<int>(std::floor(x));
			const int y0 = static_cast<int>(std::floor(y));
			const float tx = Smooth(static_cast<float>(x - x0));
			const float ty = Smooth(static_cast<float>(y - y0));
			const auto wrap = [period](int v) { return ((v % period) + period) % period; };
			const float a = wi::math::Lerp(Hash01(wrap(x0), wrap(y0), seed), Hash01(wrap(x0 + 1), wrap(y0), seed), tx);
			const float b = wi::math::Lerp(Hash01(wrap(x0), wrap(y0 + 1), seed), Hash01(wrap(x0 + 1), wrap(y0 + 1), seed), tx);
			return wi::math::Lerp(a, b, ty);
		}

		float TileFbm(double u, double v, int basePeriod, int octaves, std::uint32_t seed)
		{
			float sum = 0.0f;
			float amplitude = 0.5f;
			float norm = 0.0f;
			int period = basePeriod;
			for (int i = 0; i < octaves; ++i)
			{
				sum += amplitude * TileNoise(u * period, v * period, period, seed + static_cast<std::uint32_t>(i) * 37u);
				norm += amplitude;
				period *= 2;
				amplitude *= 0.55f;
			}
			return sum / norm;
		}

		// A single tree-part mesh in tree-local metres.
		struct PartMesh
		{
			std::vector<XMFLOAT3> positions;
			std::vector<std::uint32_t> indices;
			std::vector<std::uint32_t> colors;
		};

		void AddRing(PartMesh& mesh, int segments, float y, float radius, const XMFLOAT3& color,
			float jitter, std::uint32_t seed, float cx = 0.0f, float cz = 0.0f)
		{
			for (int i = 0; i < segments; ++i)
			{
				const float angle = 2.0f * kPi * static_cast<float>(i) / segments;
				const float r = radius * (1.0f + jitter * (Hash01(i, static_cast<int>(y * 37.0f), seed) - 0.5f));
				mesh.positions.emplace_back(cx + std::cos(angle) * r, y, cz + std::sin(angle) * r);
				mesh.colors.push_back(PackColor(color));
			}
		}

		void BridgeRings(PartMesh& mesh, std::uint32_t lower, std::uint32_t upper, int segments)
		{
			for (int i = 0; i < segments; ++i)
			{
				const std::uint32_t a = lower + i;
				const std::uint32_t b = lower + (i + 1) % segments;
				const std::uint32_t c = upper + i;
				const std::uint32_t d = upper + (i + 1) % segments;
				mesh.indices.insert(mesh.indices.end(), { a, c, b, b, c, d });
			}
		}

		void CapFan(PartMesh& mesh, std::uint32_t ring, int segments, const XMFLOAT3& apex, const XMFLOAT3& color, bool up)
		{
			const std::uint32_t apexIndex = static_cast<std::uint32_t>(mesh.positions.size());
			mesh.positions.push_back(apex);
			mesh.colors.push_back(PackColor(color));
			for (int i = 0; i < segments; ++i)
			{
				const std::uint32_t a = ring + i;
				const std::uint32_t b = ring + (i + 1) % segments;
				if (up)
				{
					mesh.indices.insert(mesh.indices.end(), { a, apexIndex, b });
				}
				else
				{
					mesh.indices.insert(mesh.indices.end(), { a, b, apexIndex });
				}
			}
		}

		void AddTrunk(PartMesh& mesh, float height, float bottomRadius, float topRadius, int segments)
		{
			const XMFLOAT3 bark(0.20f, 0.13f, 0.08f);
			const std::uint32_t lower = static_cast<std::uint32_t>(mesh.positions.size());
			AddRing(mesh, segments, 0.0f, bottomRadius, bark, 0.0f, 1);
			const std::uint32_t upper = static_cast<std::uint32_t>(mesh.positions.size());
			AddRing(mesh, segments, height, topRadius, bark, 0.0f, 2);
			BridgeRings(mesh, lower, upper, segments);
		}

		// Stacked cones; `tiers` decreasing in radius.
		PartMesh MakeConifer(int tiers, int segments, float height, float baseRadius, const XMFLOAT3& dark,
			const XMFLOAT3& light, std::uint32_t seed)
		{
			PartMesh mesh;
			AddTrunk(mesh, height * 0.22f, baseRadius * 0.11f, baseRadius * 0.07f, 6);
			const float tierHeight = height * 0.78f / tiers;
			for (int t = 0; t < tiers; ++t)
			{
				const float fraction = static_cast<float>(t) / std::max(1, tiers - 1);
				const float baseY = height * 0.18f + t * tierHeight * 0.82f;
				const float radius = baseRadius * (1.0f - 0.62f * fraction);
				const XMFLOAT3 tint = Mix(dark, light, fraction * 0.8f);
				const std::uint32_t lower = static_cast<std::uint32_t>(mesh.positions.size());
				AddRing(mesh, segments, baseY, radius, Mix(tint, dark, 0.5f), 0.22f, seed + t);
				const std::uint32_t upper = static_cast<std::uint32_t>(mesh.positions.size());
				AddRing(mesh, segments, baseY + tierHeight * 0.42f, radius * 0.62f, tint, 0.15f, seed + 10 + t);
				BridgeRings(mesh, lower, upper, segments);
				CapFan(mesh, upper, segments, XMFLOAT3(0, baseY + tierHeight * 1.12f, 0), light, true);
				CapFan(mesh, lower, segments, XMFLOAT3(0, baseY - tierHeight * 0.08f, 0), dark, false);
			}
			return mesh;
		}

		// Lumpy ellipsoid blob.
		void AddBlob(PartMesh& mesh, const XMFLOAT3& center, const XMFLOAT3& radii, int rings, int segments,
			const XMFLOAT3& low, const XMFLOAT3& high, std::uint32_t seed)
		{
			std::vector<std::uint32_t> ringStart;
			for (int r = 1; r < rings; ++r)
			{
				const float v = kPi * static_cast<float>(r) / rings;
				const float y = std::cos(v);
				const float s = std::sin(v);
				ringStart.push_back(static_cast<std::uint32_t>(mesh.positions.size()));
				for (int i = 0; i < segments; ++i)
				{
					const float angle = 2.0f * kPi * static_cast<float>(i) / segments;
					const float bump = 1.0f + 0.28f * (Hash01(i, r, seed) - 0.5f);
					mesh.positions.emplace_back(center.x + std::cos(angle) * s * radii.x * bump,
						center.y + y * radii.y * bump, center.z + std::sin(angle) * s * radii.z * bump);
					const float shade = std::clamp(0.5f + 0.5f * y + 0.18f * (Hash01(i, r, seed + 5) - 0.5f), 0.0f, 1.0f);
					mesh.colors.push_back(PackColor(Mix(low, high, shade)));
				}
			}
			for (std::size_t r = 0; r + 1 < ringStart.size(); ++r)
			{
				BridgeRings(mesh, ringStart[r + 1], ringStart[r], segments);
			}
			CapFan(mesh, ringStart.front(), segments, XMFLOAT3(center.x, center.y + radii.y, center.z), high, true);
			CapFan(mesh, ringStart.back(), segments, XMFLOAT3(center.x, center.y - radii.y, center.z), low, false);
		}

		PartMesh MakeBroadleaf(float trunkHeight, float crownRadius, int blobs, int rings, int segments,
			const XMFLOAT3& low, const XMFLOAT3& high, std::uint32_t seed)
		{
			PartMesh mesh;
			AddTrunk(mesh, trunkHeight * 1.15f, crownRadius * 0.13f, crownRadius * 0.08f, 6);
			for (int b = 0; b < blobs; ++b)
			{
				const float angle = 2.0f * kPi * Hash01(b, 3, seed);
				const float spread = b == 0 ? 0.0f : crownRadius * (0.35f + 0.35f * Hash01(b, 6, seed));
				const XMFLOAT3 center(std::cos(angle) * spread,
					trunkHeight + crownRadius * (0.35f + 0.65f * Hash01(b, 4, seed)),
					std::sin(angle) * spread);
				const float radius = crownRadius * (b == 0 ? 0.78f : 0.52f + 0.12f * Hash01(b, 8, seed));
				AddBlob(mesh, center, XMFLOAT3(radius, radius * 0.82f, radius), rings, segments, low, high, seed + b);
			}
			return mesh;
		}

		wi::ecs::Entity CreatePartEntity(wi::scene::Scene& scene, const char* name, PartMesh& part,
			float roughness)
		{
			const wi::ecs::Entity entity = scene.Entity_CreateMeshFromData(name, part.indices.size(),
				part.indices.data(), part.positions.size(), part.positions.data());
			wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(entity);
			wi::scene::MaterialComponent* material = scene.materials.GetComponent(entity);
			if (mesh == nullptr || material == nullptr)
			{
				return wi::ecs::INVALID_ENTITY;
			}
			mesh->vertex_colors = part.colors;
			mesh->CreateRenderData();
			material->SetUseVertexColors(true);
			material->SetBaseColor(XMFLOAT4(1, 1, 1, 1));
			material->SetRoughness(roughness);
			material->SetReflectance(0.02f);
			material->CreateRenderData(true);
			if (wi::scene::ObjectComponent* own = scene.objects.GetComponent(entity))
			{
				own->SetRenderable(false);
			}
			return entity;
		}

		struct Mask
		{
			double minX = 0.0;
			double minZ = 0.0;
			double cell = 4.0;
			int size = 0;
			std::vector<std::uint8_t> blocked;

			void Init(double centerX, double centerZ, double halfExtent, double cellSize)
			{
				cell = cellSize;
				minX = centerX - halfExtent;
				minZ = centerZ - halfExtent;
				size = static_cast<int>(std::ceil(2.0 * halfExtent / cellSize));
				blocked.assign(static_cast<std::size_t>(size) * size, 0);
			}
			bool Blocked(double x, double z) const
			{
				const int ix = static_cast<int>(std::floor((x - minX) / cell));
				const int iz = static_cast<int>(std::floor((z - minZ) / cell));
				return ix >= 0 && iz >= 0 && ix < size && iz < size && blocked[static_cast<std::size_t>(iz) * size + ix] != 0;
			}
			void Disc(double x, double z, double radius)
			{
				const int r = static_cast<int>(std::ceil(radius / cell));
				const int cx = static_cast<int>(std::floor((x - minX) / cell));
				const int cz = static_cast<int>(std::floor((z - minZ) / cell));
				for (int dz = -r; dz <= r; ++dz)
				{
					for (int dx = -r; dx <= r; ++dx)
					{
						const int ix = cx + dx;
						const int iz = cz + dz;
						if (ix < 0 || iz < 0 || ix >= size || iz >= size || dx * dx + dz * dz > r * r)
						{
							continue;
						}
						blocked[static_cast<std::size_t>(iz) * size + ix] = 1;
					}
				}
			}
			void Segment(double ax, double az, double bx, double bz, double radius)
			{
				const double length = std::hypot(bx - ax, bz - az);
				const int steps = std::max(1, static_cast<int>(std::ceil(length / (cell * 0.75))));
				for (int s = 0; s <= steps; ++s)
				{
					const double t = static_cast<double>(s) / steps;
					Disc(ax + (bx - ax) * t, az + (bz - az) * t, radius);
				}
			}
		};
	}

	bool PrepareShowcaseTerrain(const MapPlacementRequest& request, double nearHalfExtent,
		ShowcaseTerrainData& data, std::string& error)
	{
		const TerrainTransform transform = MakeMapTerrainTransform(request.metadata);
		const PlacedBuilding& center = request.placement;
		const double minX = std::max(transform.originX, center.X() - kFarHalfExtent);
		const double maxX = std::min(transform.originX + transform.sizeX, center.X() + kFarHalfExtent);
		const double minZ = std::max(transform.originZ, center.Z() - kFarHalfExtent);
		const double maxZ = std::min(transform.originZ + transform.sizeZ, center.Z() + kFarHalfExtent);
		if (!BuildTerrainRegionGrid(request.heightmap, transform, minX, minZ, maxX - minX, maxZ - minZ,
			kFarSamples, kFarSamples, data.grid, &error))
		{
			return false;
		}
		data.centerX = center.X();
		data.centerZ = center.Z();
		data.nearHalfExtent = nearHalfExtent;

		// Lake level: a low percentile of the visible landscape, kept clearly below the town.
		std::vector<float> sorted(data.grid.heights);
		const std::size_t index = sorted.size() / 9;
		std::nth_element(sorted.begin(), sorted.begin() + index, sorted.end());
		const float low = sorted[index];
		// Presentation choice (INFERRED, not read from the map): where the terrain dips well below
		// zero (sunken basins), treat zero as the water level; otherwise use the low percentile.
		float level = std::min(low, static_cast<float>(center.Y()) - 6.0f);
		if (data.grid.minHeight < -5.0f)
		{
			level = std::min(0.0f, static_cast<float>(center.Y()) - 4.0f);
		}
		data.waterLevel = level;
		data.hasWater = level > data.grid.minHeight + 0.5f;
		return true;
	}

	float ShowcaseLandHeight(const ShowcaseTerrainData& data, double worldX, double worldZ, float rawHeight)
	{
		if (!data.hasWater || rawHeight <= data.waterLevel + 0.5f)
		{
			return rawHeight;
		}
		const double dist = std::hypot(worldX - data.centerX, worldZ - data.centerZ);
		// Gentle rolling relief away from the (flat) town so the plain does not read as a billiard table.
		rawHeight += 9.0f * (Fbm(worldX / 210.0 + 5.0, worldZ / 210.0 - 8.0, 3, 41) - 0.5f) *
			SmoothStep(static_cast<float>(data.nearHalfExtent) * 1.1f, static_cast<float>(data.nearHalfExtent) * 2.4f, static_cast<float>(dist)) *
			SmoothStep(data.waterLevel + 2.0f, data.waterLevel + 14.0f, rawHeight);
		const float boost = 1.0f + 1.5f * SmoothStep(900.0f, 3200.0f, static_cast<float>(dist));
		float height = data.waterLevel + (rawHeight - data.waterLevel) * boost;
		const float ridge = 1.0f - std::fabs(2.0f * Fbm(worldX / 2300.0 + 9.0, worldZ / 2300.0 - 4.0, 5, 19) - 1.0f);
		const float range = Fbm(worldX / 3600.0 + 3.0, worldZ / 3600.0 + 11.0, 2, 23);
		const float landward = SmoothStep(data.waterLevel + 3.0f, data.waterLevel + 30.0f, rawHeight);
		height += 360.0f * SmoothStep(1500.0f, 6500.0f, static_cast<float>(dist)) * ridge * ridge *
			SmoothStep(0.25f, 0.5f, range) * landward;
		return height;
	}

	namespace
	{
		bool RealGroundAlbedoValid();
	}

	XMFLOAT3 ShadeShowcaseTerrain(const ShowcaseTerrainData& data, double worldX, double worldZ,
		float height, float slope)
	{
		const float elevation = RelativeElevation(data, height);
		const float broad = Fbm(worldX / 260.0, worldZ / 260.0, 3, 3);
		const float fine = ValueNoise(worldX / 23.0, worldZ / 23.0, 9);
		const float forest = ForestAmount(data, worldX, worldZ);

		const XMFLOAT3 lush(0.12f, 0.22f, 0.065f);
		const XMFLOAT3 meadow(0.22f, 0.30f, 0.09f);
		const XMFLOAT3 dry(0.34f, 0.31f, 0.14f);
		const XMFLOAT3 forestFloor(0.055f, 0.105f, 0.045f);
		const XMFLOAT3 rock(0.30f, 0.28f, 0.25f);
		const XMFLOAT3 snow(0.78f, 0.80f, 0.84f);
		const XMFLOAT3 sand(0.40f, 0.35f, 0.24f);

		XMFLOAT3 color = Mix(lush, meadow, SmoothStep(0.3f, 0.7f, broad));
		color = Mix(color, dry, std::clamp((broad - 0.55f) * 2.0f + elevation * 0.3f, 0.0f, 0.55f));
		color = Mix(color, forestFloor, forest * 0.9f);
		const float rockMask = std::max(SmoothStep(0.42f, 0.85f, slope), SmoothStep(0.30f, 0.55f, elevation + (fine - 0.5f) * 0.1f));
		color = Mix(color, rock, rockMask);
		color = Mix(color, snow, SmoothStep(0.72f, 0.86f, elevation + (fine - 0.5f) * 0.05f) * (1.0f - SmoothStep(0.5f, 0.9f, slope)));
		if (data.hasWater)
		{
			color = Mix(color, sand, 1.0f - SmoothStep(data.waterLevel + 0.3f, data.waterLevel + 2.6f, height));
		}
		// The real WRSR ground tile is much darker on average than the procedural one.
		const float detail = (0.9f + fine * 0.2f) * (RealGroundAlbedoValid() ? 3.2f : 1.45f);
		return XMFLOAT3(color.x * detail, color.y * detail, color.z * detail);
	}

	const wi::Resource& ShowcaseGrassTexture()
	{
		static wi::Resource resource = []()
		{
			constexpr std::uint32_t kSize = 512;
			std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kSize) * kSize * 4);
			for (std::uint32_t y = 0; y < kSize; ++y)
			{
				for (std::uint32_t x = 0; x < kSize; ++x)
				{
					const double u = static_cast<double>(x) / kSize;
					const double v = static_cast<double>(y) / kSize;
					const float large = TileFbm(u, v, 4, 3, 11);
					const float grain = TileFbm(u, v, 32, 3, 29);
					const float blades = TileNoise(u * 128.0, v * 128.0, 128, 53);
					const float value = std::clamp(0.70f + 0.16f * (large - 0.5f) + 0.30f * (grain - 0.5f) + 0.12f * (blades - 0.5f), 0.0f, 1.0f);
					const float warm = 0.5f + 0.5f * (TileFbm(u, v, 6, 2, 71) - 0.5f);
					const std::size_t i = (static_cast<std::size_t>(y) * kSize + x) * 4;
					pixels[i + 0] = static_cast<std::uint8_t>(std::clamp((value * (0.96f + 0.12f * warm)) * 255.0f * 1.18f, 0.0f, 255.0f));
					pixels[i + 1] = static_cast<std::uint8_t>(std::clamp(value * 255.0f * 1.18f, 0.0f, 255.0f));
					pixels[i + 2] = static_cast<std::uint8_t>(std::clamp((value * (0.86f + 0.1f * warm)) * 255.0f * 1.18f, 0.0f, 255.0f));
					pixels[i + 3] = 255;
				}
			}
			return MakeTexture(kSize, kSize, std::move(pixels));
		}();
		return resource;
	}

	const wi::Resource& ShowcaseAsphaltTexture()
	{
		// 5 m wide road: u across, v along (one repeat = 16 m). Edge lines and a dashed centre line.
		static wi::Resource resource = []()
		{
			constexpr std::uint32_t kWidth = 128;
			constexpr std::uint32_t kHeight = 512;
			std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kWidth) * kHeight * 4);
			for (std::uint32_t y = 0; y < kHeight; ++y)
			{
				for (std::uint32_t x = 0; x < kWidth; ++x)
				{
					const double u = static_cast<double>(x) / kWidth;
					const double v = static_cast<double>(y) / kHeight;
					const float speckle = Hash01(static_cast<int>(x), static_cast<int>(y), 5);
					const float patch = ValueNoise(u * 6.0, v * 24.0, 17);
					float value = 0.20f + 0.05f * (patch - 0.5f) + 0.07f * (speckle - 0.5f);
					const float wear = 1.0f - std::fabs(u - 0.5f) * 2.0f; // darker tyre-free edges, lighter wheel paths
					value += 0.02f * (wear - 0.5f);
					XMFLOAT3 color(value, value * 1.01f, value * 1.03f);
					const bool edgeLine = (u > 0.055 && u < 0.085) || (u > 0.915 && u < 0.945);
					const bool centreDash = (u > 0.488 && u < 0.512) && (std::fmod(v, 0.5) < 0.22);
					if (edgeLine || centreDash)
					{
						const float paint = 0.62f + 0.18f * (speckle - 0.5f);
						color = XMFLOAT3(paint, paint * 0.98f, paint * 0.9f);
					}
					const std::size_t i = (static_cast<std::size_t>(y) * kWidth + x) * 4;
					pixels[i + 0] = static_cast<std::uint8_t>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f);
					pixels[i + 1] = static_cast<std::uint8_t>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f);
					pixels[i + 2] = static_cast<std::uint8_t>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f);
					pixels[i + 3] = 255;
				}
			}
			return MakeTexture(kWidth, kHeight, std::move(pixels));
		}();
		return resource;
	}

	namespace
	{
		wi::Resource& RealGroundAlbedo() { static wi::Resource r; return r; }
		bool RealGroundAlbedoValid() { return RealGroundAlbedo().IsValid(); }
		wi::Resource& RealGroundNormal() { static wi::Resource r; return r; }
	}

	void SetShowcaseRealGroundTexture(const wi::Resource& albedo, const wi::Resource& normal)
	{
		RealGroundAlbedo() = albedo;
		RealGroundNormal() = normal;
	}

	const wi::Resource& ShowcaseSoftCircleTexture()
	{
		static wi::Resource resource = []()
		{
			constexpr std::uint32_t kSize = 64;
			std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kSize) * kSize * 4);
			for (std::uint32_t y = 0; y < kSize; ++y)
			{
				for (std::uint32_t x = 0; x < kSize; ++x)
				{
					const float dx = (x + 0.5f) / kSize * 2.0f - 1.0f;
					const float dy = (y + 0.5f) / kSize * 2.0f - 1.0f;
					const float r = std::sqrt(dx * dx + dy * dy);
					const float falloff = std::clamp(1.0f - r, 0.0f, 1.0f);
					const float alpha = falloff * falloff * (3.0f - 2.0f * falloff);
					const float noise = 0.85f + 0.3f * Hash01(static_cast<int>(x / 4), static_cast<int>(y / 4), 61);
					const std::size_t i = (static_cast<std::size_t>(y) * kSize + x) * 4;
					pixels[i + 0] = pixels[i + 1] = pixels[i + 2] = 255;
					pixels[i + 3] = static_cast<std::uint8_t>(std::clamp(alpha * noise, 0.0f, 1.0f) * 255.0f);
				}
			}
			return MakeTexture(kSize, kSize, std::move(pixels));
		}();
		return resource;
	}

	void ApplyShowcaseGrass(wi::scene::MaterialComponent& material)
	{
		const bool real = RealGroundAlbedo().IsValid();
		material.textures[wi::scene::MaterialComponent::BASECOLORMAP].resource = real ? RealGroundAlbedo() : ShowcaseGrassTexture();
		material.textures[wi::scene::MaterialComponent::BASECOLORMAP].uvset = 0;
		if (real && RealGroundNormal().IsValid())
		{
			material.textures[wi::scene::MaterialComponent::NORMALMAP].resource = RealGroundNormal();
			material.textures[wi::scene::MaterialComponent::NORMALMAP].uvset = 0;
		}
	}

	bool BuildShowcaseWorld(wi::scene::Scene& scene, const MapPlacementRequest& request,
		const ShowcaseTerrainData& terrain, double regionRadius, const std::vector<RegionInstance>& instances,
		const RealTreeSet* realTrees, ShowcaseEnvironmentInfo& info, std::string& error)
	{
		const TerrainTransform transform = MakeMapTerrainTransform(request.metadata);
		const PlacedBuilding& center = request.placement;
		const TerrainPreviewGrid& grid = terrain.grid;
		const std::uint32_t samples = kFarSamples;
		const bool useReal = realTrees != nullptr && realTrees->Any();

		// ---- Far landscape: the real heightmap out to several kilometres.
		const double nearHalf = terrain.nearHalfExtent;
		const double nearMinX = std::max(transform.originX, center.X() - nearHalf);
		const double nearMaxX = std::min(transform.originX + transform.sizeX, center.X() + nearHalf);
		const double nearMinZ = std::max(transform.originZ, center.Z() - nearHalf);
		const double nearMaxZ = std::min(transform.originZ + transform.sizeZ, center.Z() + nearHalf);
		const auto heightAt = [&](std::uint32_t x, std::uint32_t z)
		{
			return grid.heights[static_cast<std::size_t>(z) * samples + x];
		};
		std::vector<float> boosted(static_cast<std::size_t>(samples) * samples);
		for (std::uint32_t z = 0; z < samples; ++z)
		{
			for (std::uint32_t x = 0; x < samples; ++x)
			{
				boosted[static_cast<std::size_t>(z) * samples + x] = ShowcaseLandHeight(
					terrain, grid.worldXCoordinates[x], grid.worldZCoordinates[z], heightAt(x, z));
			}
		}
		const auto boostedAt = [&](std::uint32_t x, std::uint32_t z)
		{
			return boosted[static_cast<std::size_t>(z) * samples + x];
		};
		std::vector<XMFLOAT3> positions;
		std::vector<XMFLOAT2> uvs;
		std::vector<std::uint32_t> colors;
		positions.reserve(static_cast<std::size_t>(samples) * samples);
		uvs.reserve(positions.capacity());
		colors.reserve(positions.capacity());
		for (std::uint32_t z = 0; z < samples; ++z)
		{
			for (std::uint32_t x = 0; x < samples; ++x)
			{
				const double wx = grid.worldXCoordinates[x];
				const double wz = grid.worldZCoordinates[z];
				float height = boostedAt(x, z);
				const std::uint32_t left = x == 0 ? x : x - 1;
				const std::uint32_t right = std::min(x + 1, samples - 1);
				const std::uint32_t down = z == 0 ? z : z - 1;
				const std::uint32_t up = std::min(z + 1, samples - 1);
				const double dx = grid.worldXCoordinates[right] - grid.worldXCoordinates[left];
				const double dz = grid.worldZCoordinates[up] - grid.worldZCoordinates[down];
				const float slopeX = dx > 0.0 ? static_cast<float>((boostedAt(right, z) - boostedAt(left, z)) / dx) : 0.0f;
				const float slopeZ = dz > 0.0 ? static_cast<float>((boostedAt(x, up) - boostedAt(x, down)) / dz) : 0.0f;
				const float slope = std::sqrt(slopeX * slopeX + slopeZ * slopeZ);
				colors.push_back(PackColor(ShadeShowcaseTerrain(terrain, wx, wz, height, slope)));
				// Sink the coarse mesh under the detailed near mesh, fading in from its border.
				const double inside = std::min({ wx - nearMinX, nearMaxX - wx, wz - nearMinZ, nearMaxZ - wz });
				if (inside > 0.0)
				{
					height -= 2.5f * SmoothStep(0.0f, 70.0f, static_cast<float>(inside));
				}
				positions.emplace_back(static_cast<float>(wx - center.X()), height, static_cast<float>(wz - center.Z()));
				uvs.emplace_back(static_cast<float>(wx / 12.0), static_cast<float>(wz / 12.0));
			}
		}
		std::vector<std::uint32_t> indices;
		indices.reserve(static_cast<std::size_t>(samples - 1) * (samples - 1) * 6);
		for (std::uint32_t z = 0; z + 1 < samples; ++z)
		{
			for (std::uint32_t x = 0; x + 1 < samples; ++x)
			{
				const std::uint32_t a = z * samples + x;
				const std::uint32_t b = a + 1;
				const std::uint32_t c = a + samples;
				const std::uint32_t d = c + 1;
				indices.insert(indices.end(), { a, b, c, b, d, c });
			}
		}
		const wi::ecs::Entity farEntity = scene.Entity_CreateMeshFromData("OpenRepublic showcase far landscape",
			indices.size(), indices.data(), positions.size(), positions.data());
		wi::scene::MeshComponent* farMesh = scene.meshes.GetComponent(farEntity);
		wi::scene::MaterialComponent* farMaterial = scene.materials.GetComponent(farEntity);
		if (farMesh == nullptr || farMaterial == nullptr)
		{
			error = "Wicked Engine did not create the far landscape components";
			return false;
		}
		farMesh->vertex_colors = std::move(colors);
		farMesh->vertex_uvset_0 = std::move(uvs);
		farMesh->CreateRenderData();
		farMaterial->SetUseVertexColors(true);
		farMaterial->SetBaseColor(XMFLOAT4(1, 1, 1, 1));
		farMaterial->SetRoughness(0.96f);
		farMaterial->SetReflectance(0.02f);
		ApplyShowcaseGrass(*farMaterial);
		farMaterial->CreateRenderData(true);
		if (wi::scene::ObjectComponent* object = scene.objects.GetComponent(farEntity))
		{
			object->SetCastShadow(false);
		}

		// ---- Trees (procedural OpenRepublic assets, not WRSR vegetation).
		struct Species
		{
			wi::ecs::Entity mesh = wi::ecs::INVALID_ENTITY;
			float minScale = 0.8f;
			float maxScale = 1.3f;
		};
		PartMesh spruceHigh = MakeConifer(5, 9, 17.0f, 3.1f, XMFLOAT3(0.035f, 0.10f, 0.045f), XMFLOAT3(0.10f, 0.23f, 0.09f), 3);
		PartMesh pineHigh = MakeConifer(4, 8, 22.0f, 2.5f, XMFLOAT3(0.06f, 0.12f, 0.05f), XMFLOAT3(0.16f, 0.26f, 0.09f), 8);
		PartMesh oakHigh = MakeBroadleaf(4.2f, 5.6f, 9, 5, 9, XMFLOAT3(0.045f, 0.11f, 0.03f), XMFLOAT3(0.15f, 0.27f, 0.07f), 5);
		PartMesh birchHigh = MakeBroadleaf(5.0f, 3.9f, 6, 5, 8, XMFLOAT3(0.09f, 0.18f, 0.045f), XMFLOAT3(0.24f, 0.36f, 0.10f), 9);
		PartMesh poplarHigh;
		{
			AddTrunk(poplarHigh, 3.0f, 0.22f, 0.15f, 6);
			AddBlob(poplarHigh, XMFLOAT3(0, 11.0f, 0), XMFLOAT3(2.1f, 9.0f, 2.1f), 7, 9,
				XMFLOAT3(0.05f, 0.12f, 0.035f), XMFLOAT3(0.16f, 0.28f, 0.075f), 21);
		}
		PartMesh spruceLow = MakeConifer(2, 6, 17.0f, 3.0f, XMFLOAT3(0.035f, 0.10f, 0.045f), XMFLOAT3(0.10f, 0.22f, 0.09f), 13);
		PartMesh broadLow = MakeBroadleaf(4.0f, 5.2f, 2, 4, 7, XMFLOAT3(0.045f, 0.11f, 0.03f), XMFLOAT3(0.14f, 0.26f, 0.07f), 15);

		Species spruce{ CreatePartEntity(scene, "showcase spruce", spruceHigh, 0.92f), 0.8f, 1.45f };
		Species pine{ CreatePartEntity(scene, "showcase pine", pineHigh, 0.92f), 0.8f, 1.3f };
		Species oak{ CreatePartEntity(scene, "showcase oak", oakHigh, 0.9f), 0.8f, 1.3f };
		Species birch{ CreatePartEntity(scene, "showcase birch", birchHigh, 0.9f), 0.85f, 1.3f };
		Species poplar{ CreatePartEntity(scene, "showcase poplar", poplarHigh, 0.9f), 0.85f, 1.25f };
		Species spruceFar{ CreatePartEntity(scene, "showcase spruce far", spruceLow, 0.95f), 0.9f, 1.6f };
		PartMesh shrubHigh;
		AddBlob(shrubHigh, XMFLOAT3(0, 0.55f, 0), XMFLOAT3(1.0f, 0.7f, 1.0f), 4, 7, XMFLOAT3(0.05f, 0.10f, 0.03f), XMFLOAT3(0.18f, 0.28f, 0.07f), 31);
		AddBlob(shrubHigh, XMFLOAT3(0.7f, 0.45f, 0.3f), XMFLOAT3(0.7f, 0.5f, 0.7f), 4, 7, XMFLOAT3(0.06f, 0.11f, 0.03f), XMFLOAT3(0.22f, 0.30f, 0.08f), 33);
		Species shrub{ CreatePartEntity(scene, "showcase shrub", shrubHigh, 0.95f), 0.7f, 1.6f };
		Species broadFar{ CreatePartEntity(scene, "showcase broadleaf far", broadLow, 0.95f), 0.9f, 1.4f };

		struct RealPick
		{
			wi::ecs::Entity mesh = wi::ecs::INVALID_ENTITY;
			float unit = 1.0f;
		};
		const auto realMesh = [&](const std::vector<wi::ecs::Entity>& list, std::uint32_t hash, float targetHeight)
		{
			RealPick pick;
			if (list.empty())
			{
				return pick;
			}
			pick.mesh = list[hash % list.size()];
			const wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(pick.mesh);
			const float height = mesh != nullptr ? mesh->aabb.getHalfWidth().y * 2.0f : 0.0f;
			if (height < 0.05f)
			{
				return RealPick{};
			}
			pick.unit = targetHeight / height;
			return pick;
		};

		Mask mask;
		const double maskHalf = regionRadius * 1.5;
		mask.Init(center.X(), center.Z(), maskHalf, 4.0);
		for (const RegionInstance& instance : instances)
		{
			mask.Disc(instance.x, instance.z, 17.0);
		}
		if (request.hasRoads)
		{
			const auto& nodes = request.roads.nodes;
			for (std::size_t i = 0; i < nodes.size(); ++i)
			{
				if (std::fabs(nodes[i].x - center.X()) > maskHalf * 1.2 || std::fabs(nodes[i].z - center.Z()) > maskHalf * 1.2)
				{
					continue;
				}
				for (const std::uint32_t j : nodes[i].neighbors)
				{
					if (j > i && j < nodes.size())
					{
						mask.Segment(nodes[i].x, nodes[i].z, nodes[j].x, nodes[j].z, 7.5);
					}
				}
			}
		}

		const auto heightNear = [&](double wx, double wz)
		{
			double h = 0.0;
			SampleTerrainHeight(request.heightmap, transform,
				std::min(std::max(wx, transform.originX), transform.originX + transform.sizeX),
				std::min(std::max(wz, transform.originZ), transform.originZ + transform.sizeZ), h, nullptr);
			return static_cast<double>(ShowcaseLandHeight(terrain, wx, wz, static_cast<float>(h)));
		};
		const auto slopeAt = [&](double wx, double wz)
		{
			const double e = 3.0;
			const double sx = (heightNear(wx + e, wz) - heightNear(wx - e, wz)) / (2.0 * e);
			const double sz = (heightNear(wx, wz + e) - heightNear(wx, wz - e)) / (2.0 * e);
			return static_cast<float>(std::hypot(sx, sz));
		};

		constexpr std::size_t kNearCap = 16000;
		constexpr std::size_t kFarCap = 18000;
		struct Candidate
		{
			double x, z;
			bool farRing;
		};
		std::vector<Candidate> candidates;
		const auto scan = [&](double radiusMin, double radiusMax, double spacing, bool farRing)
		{
			const int half = static_cast<int>(std::ceil(radiusMax / spacing));
			for (int gz = -half; gz <= half; ++gz)
			{
				for (int gx = -half; gx <= half; ++gx)
				{
					const double jx = (Hash01(gx, gz, 41) - 0.5) * spacing * 0.9;
					const double jz = (Hash01(gx, gz, 43) - 0.5) * spacing * 0.9;
					const double wx = center.X() + gx * spacing + jx;
					const double wz = center.Z() + gz * spacing + jz;
					const double dist = std::hypot(wx - center.X(), wz - center.Z());
					if (dist < radiusMin || dist >= radiusMax)
					{
						continue;
					}
					if (wx < transform.originX || wz < transform.originZ ||
						wx > transform.originX + transform.sizeX || wz > transform.originZ + transform.sizeZ)
					{
						continue;
					}
					if (mask.Blocked(wx, wz))
					{
						continue;
					}
					const float forest = ForestAmount(terrain, wx, wz);
					const bool urban = dist < regionRadius * 1.25;
					const float density = urban ? 0.30f + 0.55f * forest : 0.08f + 0.92f * forest;
					if (Hash01(gx, gz, 47) > density)
					{
						continue;
					}
					candidates.push_back({ wx, wz, farRing });
				}
			}
		};
		scan(0.0, 1400.0, 8.5, false);
		// Undergrowth: dense small shrubs around the town and along the nearest forest edges.
		struct ShrubSpot { double x, z; };
		std::vector<ShrubSpot> shrubSpots;
		{
			const double spacing = 4.5;
			const int half = static_cast<int>(std::ceil(520.0 / spacing));
			for (int gz = -half; gz <= half; ++gz)
			{
				for (int gx = -half; gx <= half; ++gx)
				{
					const double wx = center.X() + gx * spacing + (Hash01(gx, gz, 91) - 0.5) * spacing;
					const double wz = center.Z() + gz * spacing + (Hash01(gx, gz, 93) - 0.5) * spacing;
					if (std::hypot(wx - center.X(), wz - center.Z()) > 520.0 || mask.Blocked(wx, wz))
					{
						continue;
					}
					const float patch = Fbm(wx / 38.0, wz / 38.0, 3, 97);
					if (Hash01(gx, gz, 95) < SmoothStep(0.48f, 0.68f, patch) * 0.8f)
					{
						shrubSpots.push_back({ wx, wz });
					}
				}
			}
		}
		scan(1400.0, 4200.0, 22.0, true);

		std::size_t nearCount = 0;
		std::size_t farCount = 0;
		std::size_t shrubCount = 0;
		for (const ShrubSpot& spot : shrubSpots)
		{
			const double h = heightNear(spot.x, spot.z);
			if (shrub.mesh == wi::ecs::INVALID_ENTITY || (terrain.hasWater && h < terrain.waterLevel + 1.0))
			{
				continue;
			}
			const int hx = static_cast<int>(std::floor(spot.x * 0.53));
			const int hz = static_cast<int>(std::floor(spot.z * 0.53));
			const wi::ecs::Entity object = scene.Entity_CreateObject("");
			wi::scene::ObjectComponent* component = scene.objects.GetComponent(object);
			component->meshID = shrub.mesh;
			component->SetCastShadow(false);
			const float tint = 0.8f + 0.5f * Hash01(hx, hz, 101);
			component->color = XMFLOAT4(tint, tint, tint * 0.9f, 1.0f);
			float scale = shrub.minScale + (shrub.maxScale - shrub.minScale) * Hash01(hx, hz, 103);
			if (useReal)
			{
				const RealPick real = realMesh(realTrees->shrubs, HashInts(hx, hz, 109), 1.7f);
				if (real.mesh != wi::ecs::INVALID_ENTITY)
				{
					component->meshID = real.mesh;
					component->color = XMFLOAT4(1, 1, 1, 1);
					scale = real.unit * (0.8f + 0.7f * Hash01(hx, hz, 103));
				}
			}
			XMFLOAT4X4 matrix;
			XMStoreFloat4x4(&matrix, XMMatrixScaling(scale, scale, scale) * XMMatrixRotationY(6.28f * Hash01(hx, hz, 107)) *
				XMMatrixTranslation(static_cast<float>(spot.x - center.X()), static_cast<float>(h) - 0.1f, static_cast<float>(spot.z - center.Z())));
			scene.transforms.GetComponent(object)->MatrixTransform(matrix);
			++shrubCount;
		}
		for (const Candidate& candidate : candidates)
		{
			const int hx = static_cast<int>(std::floor(candidate.x * 0.37));
			const int hz = static_cast<int>(std::floor(candidate.z * 0.37));
			const double h = heightNear(candidate.x, candidate.z);
			if (terrain.hasWater && h < terrain.waterLevel + 1.2)
			{
				continue;
			}
			if (RelativeElevation(terrain, static_cast<float>(h)) > 0.58f || slopeAt(candidate.x, candidate.z) > 0.62f)
			{
				continue;
			}
			if (!candidate.farRing && nearCount >= kNearCap)
			{
				continue;
			}
			if (candidate.farRing && farCount >= kFarCap)
			{
				continue;
			}
			const float pick = Hash01(hx, hz, 53);
			const float elevation = RelativeElevation(terrain, static_cast<float>(h));
			const float forest = ForestAmount(terrain, candidate.x, candidate.z);
			const double dist = std::hypot(candidate.x - center.X(), candidate.z - center.Z());
			Species* species = nullptr;
			if (candidate.farRing)
			{
				species = (pick < 0.28f + 0.5f * elevation) ? &spruceFar : &broadFar;
			}
			else if (dist < regionRadius * 1.25 && pick < 0.05f)
			{
				species = &poplar;
			}
			else if (pick < 0.30f + 0.5f * std::max(elevation, forest))
			{
				species = Hash01(hx, hz, 59) < 0.55f ? &spruce : &pine;
			}
			else
			{
				species = Hash01(hx, hz, 61) < 0.5f ? &oak : &birch;
			}
			if (species->mesh == wi::ecs::INVALID_ENTITY)
			{
				continue;
			}
			float scale = species->minScale + (species->maxScale - species->minScale) * Hash01(hx, hz, 67);
			const float yaw = 2.0f * kPi * Hash01(hx, hz, 71);
			wi::ecs::Entity meshToUse = species->mesh;
			bool usedReal = false;
			if (useReal)
			{
				const bool conifer = species == &spruce || species == &pine || species == &spruceFar;
				const bool poplarKind = species == &poplar;
				const RealPick real = poplarKind
					? realMesh(realTrees->poplars, HashInts(hx, hz, 113), 22.0f)
					: (conifer ? realMesh(realTrees->conifers, HashInts(hx, hz, 113), 15.0f)
						: realMesh(realTrees->broadleaf, HashInts(hx, hz, 113), 13.0f));
				if (real.mesh != wi::ecs::INVALID_ENTITY)
				{
					meshToUse = real.mesh;
					scale = real.unit * (0.75f + 0.55f * Hash01(hx, hz, 67));
					usedReal = true;
				}
			}
			const wi::ecs::Entity object = scene.Entity_CreateObject("");
			wi::scene::ObjectComponent* component = scene.objects.GetComponent(object);
			component->meshID = meshToUse;
			if (candidate.farRing)
			{
				component->SetCastShadow(false);
			}
			const float tint = usedReal ? 1.0f : 0.82f + 0.36f * Hash01(hx, hz, 73);
			component->color = XMFLOAT4(tint, tint * (0.95f + 0.1f * Hash01(hx, hz, 79)), tint * 0.92f, 1.0f);
			const XMMATRIX matrix = XMMatrixScaling(scale, scale * (usedReal ? 1.0f : 0.9f + 0.2f * Hash01(hx, hz, 83)), scale) *
				XMMatrixRotationY(yaw) *
				XMMatrixTranslation(static_cast<float>(candidate.x - center.X()), static_cast<float>(h) - 0.25f,
					static_cast<float>(candidate.z - center.Z()));
			XMFLOAT4X4 transformMatrix;
			XMStoreFloat4x4(&transformMatrix, matrix);
			scene.transforms.GetComponent(object)->MatrixTransform(transformMatrix);
			(candidate.farRing ? farCount : nearCount)++;
		}

		info.hasWater = terrain.hasWater;
		info.waterLevel = terrain.waterLevel;
		info.terrainMinimum = grid.minHeight;
		info.terrainMaximum = grid.maxHeight;
		info.nearTrees = nearCount;
		info.farTrees = farCount;
		std::ostringstream summary;
		summary << "Showcase environment (OpenRepublic procedural presentation over the verified height data): far landscape "
			<< samples << 'x' << samples << " samples out to ~" << kFarHalfExtent << " m, height range "
			<< grid.minHeight << ".." << grid.maxHeight << " m, "
			<< (terrain.hasWater ? "lake level " : "no lake (level ") << terrain.waterLevel << " m"
			<< (terrain.hasWater ? "" : ")") << ", " << nearCount << " detailed and " << farCount << " distant trees, " << shrubCount << " shrubs.";
		info.summary = summary.str();
		return true;
	}
}
