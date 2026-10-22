#include "openrepublic/procgeo/props.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace openrepublic::procgeo
{
	namespace
	{
		constexpr float kPi = 3.14159265358979323846f;
		constexpr Color3 wood{ 0.30f, 0.19f, 0.10f };
		constexpr Color3 darkSteel{ 0.19f, 0.22f, 0.21f };
		constexpr Color3 concrete{ 0.43f, 0.44f, 0.41f };

		Vec3 Add(Vec3 a, Vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
		Vec3 Sub(Vec3 a, Vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
		Vec3 Mul(Vec3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
		float Dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
		Vec3 Cross(Vec3 a, Vec3 b) { return { a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x }; }
		float Length(Vec3 a) { return std::sqrt(Dot(a, a)); }
		Vec3 Normalize(Vec3 a) { const float l = Length(a); return l > 1e-6f ? Mul(a, 1.0f/l) : Vec3{0,1,0}; }
		Vec3 Lerp(Vec3 a, Vec3 b, float t) { return Add(Mul(a, 1-t), Mul(b, t)); }

		void AddBeam(Mesh& out, Vec3 a, Vec3 b, float width, Color3 color)
		{
			const Vec3 axis = Normalize(Sub(b, a));
			Vec3 u = Cross(axis, { 0, 0, 1 });
			if (Length(u) < 0.01f) u = Cross(axis, { 1, 0, 0 });
			u = Normalize(u);
			const Vec3 v = Normalize(Cross(u, axis));
			Mesh box;
			AddBox(box, { 0, 0, 0 }, { width, Length(Sub(b, a))*0.5f, width }, color);
			// AddBox's long local axis is Y. Map it onto the segment and preserve its face winding.
			for (Vec3& p : box.positions)
			{
				p = Add(Lerp(a, b, 0.5f), Add(Mul(u, p.x), Add(Mul(axis, p.y), Mul(v, p.z))));
			}
			out.Append(box);
		}

		void AddTube(Mesh& out, const std::vector<Vec3>& path, float radius, int sides, Color3 color)
		{
			if (path.size() < 2) return;
			sides = std::max(3, sides);
			const std::uint32_t base = static_cast<std::uint32_t>(out.positions.size());
			for (std::size_t i = 0; i < path.size(); ++i)
			{
				const Vec3 tangent = Normalize(Sub(path[std::min(i+1,path.size()-1)], path[i == 0 ? 0 : i-1]));
				Vec3 u = Normalize(Cross(tangent, { 0, 0, 1 }));
				if (Length(u) < 0.01f) u = Normalize(Cross(tangent, { 1, 0, 0 }));
				const Vec3 v = Normalize(Cross(tangent, u));
				for (int s = 0; s < sides; ++s)
				{
					const float angle = 2*kPi*s/sides;
					out.positions.push_back(Add(path[i], Mul(Add(Mul(u, std::cos(angle)), Mul(v, std::sin(angle))), radius)));
					out.colors.push_back(color);
				}
			}
			for (std::size_t i = 0; i+1 < path.size(); ++i)
				for (int s = 0; s < sides; ++s)
				{
					const std::uint32_t a = base + static_cast<std::uint32_t>(i*sides+s);
					const std::uint32_t b = base + static_cast<std::uint32_t>(i*sides+(s+1)%sides);
					const std::uint32_t c = a + sides, d = b + sides;
					out.indices.insert(out.indices.end(), { a, b, c, b, d, c });
				}
		}

		std::uint32_t Next(std::uint32_t& s)
		{
			if (s == 0) s = 0x9e3779b9u;
			s ^= s << 13; s ^= s >> 17; s ^= s << 5;
			return s;
		}
		float Random01(std::uint32_t& s) { return (Next(s) >> 8) * (1.0f / 16777215.0f); }
		Color3 Mix(Color3 a, Color3 b, float t) { return { a.r+(b.r-a.r)*t, a.g+(b.g-a.g)*t, a.b+(b.b-a.b)*t }; }
	}

	Mesh MakeStreetLamp()
	{
		Mesh m; AddCylinder(m, 0, 0, 0, 7.75f, 0.11f, 10, darkSteel);
		AddTube(m, {{0,7.2f,0},{0,7.9f,0},{0.25f,8.35f,0},{0.85f,8.5f,0},{1.25f,8.5f,0}}, 0.055f, 6, darkSteel);
		AddBox(m, {1.25f,8.43f,0}, {0.38f,0.11f,0.23f}, {0.82f,0.75f,0.53f});
		return m;
	}

	Mesh MakeWoodenUtilityPole()
	{
		Mesh m; AddCylinder(m,0,0,0,9.8f,0.19f,8,wood);
		AddBox(m,{0,8.9f,0},{2.2f,0.12f,0.14f},wood);
		AddBox(m,{0,8.25f,0},{1.45f,0.10f,0.13f},wood);
		for (float x : {-1.8f,-0.9f,0.0f,0.9f,1.8f})
		{
			AddCylinder(m,x,0,9.02f,9.45f,0.11f,6,{0.68f,0.72f,0.67f});
			AddCylinder(m,x,0,8.36f,8.73f,0.10f,6,{0.68f,0.72f,0.67f});
		}
		return m;
	}

	Mesh MakeSteelLatticePylon()
	{
		Mesh m; const Color3 steel{0.31f,0.34f,0.32f};
		// Four tapered legs, 30 m tall, joined by 3 braced bays.
		const auto leg = [](float y) { const float r=2.6f*(1.0f-y/30.0f)+0.22f; return r; };
		for (int i=0;i<4;++i)
		{
			const float x=(i&1)?1.0f:-1.0f, z=(i&2)?1.0f:-1.0f;
			AddCylinder(m,x*leg(0),z*leg(0),0,0.35f,0.15f,6,steel);
			AddBeam(m,{x*leg(0),0.35f,z*leg(0)},{x*leg(30),30,z*leg(30)},0.10f,steel);
		}
		for (int bay=0;bay<3;++bay)
		{
			const float y0=bay*7.5f+0.5f,y1=(bay+1)*7.5f;
			const float r0=leg(y0),r1=leg(y1);
			for (int side=0;side<4;++side)
			{
				const bool xside=side<2; const float sign=(side%2)?1.0f:-1.0f;
				if (xside) AddBeam(m,{sign*r0,y0,-r0},{sign*r1,y1,r1},0.07f,steel);
				else AddBeam(m,{-r0,y0,sign*r0},{r1,y1,sign*r1},0.07f,steel);
			}
			for (int side=0;side<4;++side)
			{
				const float s=(side&1)?1.0f:-1.0f;
				if (side<2) AddBeam(m,{s*r0,y0,-r0},{-s*r0,y0,r0},0.065f,steel);
				else AddBeam(m,{-r0,y0,s*r0},{r0,y0,-s*r0},0.065f,steel);
			}
		}
		for (float y : {18.0f,23.0f,28.0f})
		{
			const float span = y==28.0f ? 4.0f : (y==23.0f ? 6.5f : 8.0f);
			AddBeam(m,{-span,y,0},{span,y,0},0.11f,steel);
			AddBeam(m,{-span*0.65f,y-0.35f,0},{span*0.65f,y-0.35f,0},0.08f,steel);
		}
		return m;
	}

	Mesh MakeCable(Vec3 a, Vec3 b, float sag, int segments, float thickness)
	{
		Mesh m; segments=std::max(2,segments); sag=std::max(0.0f,sag); thickness=std::max(0.001f,thickness);
		std::vector<Vec3> path; path.reserve(static_cast<std::size_t>(segments)+1);
		for (int i=0;i<=segments;++i)
		{
			const float t=static_cast<float>(i)/segments;
			Vec3 p=Lerp(a,b,t); p.y -= 4.0f*sag*t*(1.0f-t); path.push_back(p);
		}
		AddTube(m,path,thickness*0.5f,4,{0.14f,0.15f,0.14f}); return m;
	}

	Mesh MakeRailSegment(float length)
	{
		Mesh m; length=std::max(0.6f,length); const float half=length*0.5f;
		const Color3 ballast{0.34f,0.30f,0.25f};
		const std::uint32_t base=static_cast<std::uint32_t>(m.positions.size());
		m.positions.insert(m.positions.end(),{{-1.15f,0,-half},{1.15f,0,-half},{1.15f,0,half},{-1.15f,0,half},
			{-0.95f,0.20f,-half},{0.95f,0.20f,-half},{0.95f,0.20f,half},{-0.95f,0.20f,half}});
		m.colors.insert(m.colors.end(),8,ballast);
		const std::uint32_t faces[]={0,2,1,0,3,2, 4,6,5,4,7,6,
			0,4,5,0,5,1, 3,2,6,3,6,7, 0,3,7,0,7,4, 1,5,6,1,6,2};
		for(std::uint32_t i : faces) m.indices.push_back(base+i);
		const int sleepers=std::max(2,static_cast<int>(std::floor(length/0.6f))+1);
		for(int i=0;i<sleepers;++i)
		{
			const float z=-half+length*(i+0.5f)/sleepers;
			AddBox(m,{0,0.24f,z},{0.95f,0.12f,0.10f},{0.26f,0.17f,0.10f});
		}
		for(float x : {-0.76f,0.76f})
		{
			AddBox(m,{x,0.34f,0},{0.075f,0.11f,half},{0.48f,0.49f,0.47f});
			AddBox(m,{x,0.46f,0},{0.11f,0.035f,half},{0.58f,0.59f,0.57f});
		}
		return m;
	}

	Mesh MakeRailCrossbuck()
	{
		Mesh m; AddCylinder(m,0,0,0,3.2f,0.09f,8,wood);
		const Color3 white{0.88f,0.86f,0.78f}, red{0.72f,0.12f,0.08f};
		for(float y : {2.2f,2.65f})
		{
			AddBox(m,{0,y,0},{0.85f,0.10f,0.075f},white);
			for(float x : {-0.55f,0.0f,0.55f}) AddBox(m,{x,y,0.078f},{0.12f,0.105f,0.012f},red);
		}
		return m;
	}

	Mesh MakeConcreteFence()
	{
		Mesh m; AddBox(m,{0,1.0f,0},{2.0f,1.0f,0.12f},concrete);
		AddBox(m,{-1.72f,1.0f,0},{0.28f,1.0f,0.22f},{0.36f,0.37f,0.35f});
		AddBox(m,{1.72f,1.0f,0},{0.28f,1.0f,0.22f},{0.36f,0.37f,0.35f});
		return m;
	}

	Mesh MakeWoodenFence()
	{
		Mesh m; for(float x : {-1.8f,0.0f,1.8f}) AddBox(m,{x,0.95f,0},{0.08f,0.95f,0.09f},wood);
		AddBox(m,{0,0.48f,0},{1.9f,0.09f,0.07f},{0.38f,0.25f,0.12f});
		AddBox(m,{0,1.35f,0},{1.9f,0.09f,0.07f},{0.38f,0.25f,0.12f});
		for(int i=0;i<9;++i) AddBox(m,{-1.6f+i*0.4f,0.94f,0},{0.045f,0.78f,0.06f},{0.42f,0.28f,0.13f});
		return m;
	}

	Mesh MakeShrub(std::uint32_t seed)
	{
		Mesh m; std::uint32_t rng=seed;
		const std::array<Color3,4> palette{{{0.07f,0.20f,0.06f},{0.13f,0.30f,0.07f},{0.21f,0.36f,0.09f},{0.52f,0.42f,0.10f}}};
		const float rx=0.65f+Random01(rng)*0.4f, rz=0.58f+Random01(rng)*0.4f, h=0.8f+Random01(rng)*0.4f;
		constexpr int rings=4,sides=10; const std::uint32_t top=static_cast<std::uint32_t>(m.positions.size());
		m.positions.push_back({0,h,0}); m.colors.push_back(palette[1]);
		for(int r=1;r<=rings;++r)
		{
			const float phi=(kPi*0.5f)*r/rings;
			for(int s=0;s<sides;++s)
			{
				const float a=2*kPi*s/sides, jitter=0.88f+Random01(rng)*0.24f;
				m.positions.push_back({rx*std::sin(phi)*std::cos(a)*jitter,h*std::cos(phi),rz*std::sin(phi)*std::sin(a)*jitter});
				const float autumn=Random01(rng); m.colors.push_back(Mix(palette[0],palette[autumn>0.93f?3:1+(Next(rng)%2)],Random01(rng)*0.65f));
			}
		}
		const std::uint32_t bottom=static_cast<std::uint32_t>(m.positions.size()); m.positions.push_back({0,0,0}); m.colors.push_back(palette[0]);
		for(int s=0;s<sides;++s) m.indices.insert(m.indices.end(),{top,1u+static_cast<std::uint32_t>((s+1)%sides),1u+static_cast<std::uint32_t>(s)});
		for(int r=0;r<rings-1;++r) for(int s=0;s<sides;++s)
		{
			const std::uint32_t a=1+r*sides+s,b=1+r*sides+(s+1)%sides,c=a+sides,d=b+sides;
			m.indices.insert(m.indices.end(),{a,c,b,b,c,d});
		}
		const std::uint32_t last=1+(rings-1)*sides;
		for(int s=0;s<sides;++s) m.indices.insert(m.indices.end(),{last+static_cast<std::uint32_t>(s),last+static_cast<std::uint32_t>((s+1)%sides),bottom});
		return m;
	}

	Mesh MakeGrassTuft(std::uint32_t seed)
	{
		Mesh m; std::uint32_t rng=seed;
		for(int i=0;i<7;++i)
		{
			const float a=2*kPi*Random01(rng), w=0.035f+Random01(rng)*0.025f, h=0.35f+Random01(rng)*0.55f;
			const float x=std::cos(a)*0.12f,z=std::sin(a)*0.12f, dx=-std::sin(a)*w,dz=std::cos(a)*w;
			const std::uint32_t b=static_cast<std::uint32_t>(m.positions.size());
			m.positions.insert(m.positions.end(),{{x-dx,0,z-dz},{x+dx,0,z+dz},{x+std::cos(a)*0.06f,h,z+std::sin(a)*0.06f}});
			const Color3 c=Mix({0.08f,0.20f,0.04f},{0.35f,0.40f,0.08f},Random01(rng)); m.colors.insert(m.colors.end(),3,c);
			m.indices.insert(m.indices.end(),{b,b+1,b+2,b+2,b+1,b});
		}
		return m;
	}

	Mesh MakeBrickChimney()
	{
		Mesh m; AddCylinder(m,0,0,0,20,2.0f,16,{0.58f,0.20f,0.13f});
		AddCylinder(m,0,0,20,40,1.65f,16,{0.91f,0.87f,0.77f});
		AddCylinder(m,0,0,40,60,1.3f,16,{0.58f,0.20f,0.13f});
		AddCylinder(m,0,0,59.6f,60,1.55f,16,{0.38f,0.35f,0.31f}); return m;
	}

	Mesh MakeCoolingTower()
	{
		Mesh m; constexpr int around=24, rings=12; const std::uint32_t base=0;
		for(int j=0;j<=rings;++j)
		{
			const float t=static_cast<float>(j)/rings, y=40*t;
			const float radius=9.0f-2.6f*std::sin(kPi*t);
			for(int i=0;i<around;++i)
			{
				const float a=2*kPi*i/around; m.positions.push_back({radius*std::cos(a),y,radius*std::sin(a)});
				m.colors.push_back({0.67f+0.08f*std::sin(kPi*t),0.68f+0.07f*std::sin(kPi*t),0.64f+0.08f*std::sin(kPi*t)});
			}
		}
		for(int j=0;j<rings;++j) for(int i=0;i<around;++i)
		{
			const std::uint32_t a=base+j*around+i,b=base+j*around+(i+1)%around,c=a+around,d=b+around;
			m.indices.insert(m.indices.end(),{a,b,c,b,d,c});
		}
		return m;
	}

	const std::vector<PropInfo>& AvailableProps()
	{
		static const std::vector<PropInfo> props{
			{"street_lamp",8.5f},{"wooden_utility_pole",10.0f},{"steel_lattice_pylon",30.0f},
			{"cable",0.0f},{"rail_segment",0.7f},{"rail_crossbuck",3.2f},
			{"concrete_fence",2.2f},{"wooden_fence",1.9f},{"shrub",1.2f},{"grass_tuft",0.9f},
			{"brick_chimney",60.0f},{"cooling_tower",40.0f}};
		return props;
	}
}
