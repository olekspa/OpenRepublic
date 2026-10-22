#include "openrepublic/procgeo/props.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace openrepublic::procgeo;

namespace
{
	void Require(bool condition, const std::string& message)
	{
		if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
	}
	void Check(const char* name, const Mesh& mesh, float minHeight, float maxHeight, std::size_t maxTriangles)
	{
		Require(mesh.Valid(), std::string(name) + " mesh validity");
		Require(!mesh.indices.empty() && mesh.indices.size()/3 <= maxTriangles, std::string(name) + " triangle count");
		Vec3 lo,hi; Require(mesh.Bounds(lo,hi), std::string(name) + " bounds exist");
		Require(lo.y >= minHeight && hi.y <= maxHeight, std::string(name) + " height bounds");
		Require(std::fabs(lo.y) < 0.02f, std::string(name) + " ground contact");
	}
	bool Same(const Mesh& a,const Mesh& b)
	{
		if(a.positions.size()!=b.positions.size()||a.colors.size()!=b.colors.size()||a.indices!=b.indices) return false;
		for(std::size_t i=0;i<a.positions.size();++i)
		{
			const auto& x=a.positions[i]; const auto& y=b.positions[i];
			const auto& c=a.colors[i]; const auto& d=b.colors[i];
			if(x.x!=y.x||x.y!=y.y||x.z!=y.z||c.r!=d.r||c.g!=d.g||c.b!=d.b) return false;
		}
		return true;
	}
}

int main()
{
	Check("street lamp",MakeStreetLamp(),-0.01f,8.7f,250);
	Check("wood pole",MakeWoodenUtilityPole(),-0.01f,10.0f,300);
	Check("pylon",MakeSteelLatticePylon(),-0.01f,30.1f,700);
	Check("rail",MakeRailSegment(12.0f),-0.01f,0.6f,1000);
	Check("crossbuck",MakeRailCrossbuck(),-0.01f,3.3f,250);
	Check("concrete fence",MakeConcreteFence(),-0.01f,2.0f,100);
	Check("wood fence",MakeWoodenFence(),-0.01f,2.0f,200);
	Check("shrub",MakeShrub(12345),-0.01f,1.5f,100);
	Check("grass",MakeGrassTuft(12345),-0.01f,1.0f,30);
	Check("chimney",MakeBrickChimney(),-0.01f,60.1f,500);
	Check("cooling tower",MakeCoolingTower(),-0.01f,40.1f,700);
	const Mesh cable=MakeCable({0,10,0},{12,10,0},1.0f,16,0.08f);
	Require(cable.Valid() && cable.indices.size()/3<=128,"cable validity/count");
	Vec3 lo,hi; cable.Bounds(lo,hi); Require(std::fabs(lo.y-8.96f)<0.03f && std::fabs(hi.y-10.04f)<0.03f,"cable sag bounds");
	const Mesh cable2=MakeCable({0,10,0},{12,10,0},1.0f,16,0.08f); Require(Same(cable,cable2),"cable deterministic");
	const Mesh shrub=MakeShrub(9876), shrubAgain=MakeShrub(9876), shrubOther=MakeShrub(9877);
	Require(Same(shrub,shrubAgain),"shrub deterministic"); Require(!Same(shrub,shrubOther),"shrub seed variation");
	const Mesh grass=MakeGrassTuft(7), grassAgain=MakeGrassTuft(7), grassOther=MakeGrassTuft(8);
	Require(Same(grass,grassAgain),"grass deterministic"); Require(!Same(grass,grassOther),"grass seed variation");
	const auto& props=AvailableProps(); Require(props.size()>=12,"prop catalogue populated");
	std::cout << "procgeo props: " << props.size() << " catalog entries verified\n";
	return 0;
}
