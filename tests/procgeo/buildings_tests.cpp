#include "openrepublic/procgeo/buildings.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace openrepublic::procgeo;
namespace { void Require(bool ok,const char* what){if(!ok){std::cerr<<"FAIL: "<<what<<'\n';std::exit(1);}} }
int main()
{
	const char* names[]={"coal_mine","power_plant","bakery","farm","storage","housing","depot","border_post","market"};
	for(const char* name:names)
	{
		Mesh mesh=MakeBuilding(name,16,8,42);
		Require(mesh.Valid()&&!mesh.indices.empty(),"valid categorized building mesh");
		Vec3 lo,hi;Require(mesh.Bounds(lo,hi),"bounds available");
		if(!(lo.x>=-8.01f&&hi.x<=8.01f&&lo.z>=-4.01f&&hi.z<=4.01f)){std::cerr<<"footprint failure: "<<name<<" bounds "<<lo.x<<","<<hi.x<<","<<lo.z<<","<<hi.z<<'\n';Require(false,"model stays within footprint");}
		Require(lo.y>=-0.001f&&hi.y>1,"ground and roof bounds");
	}
	const auto a=MakeBuilding("housing",12,8,7,false,1,true);
	const auto b=MakeBuilding("housing",12,8,7,false,1,true);
	Require(a.indices==b.indices&&a.positions.size()==b.positions.size(),"deterministic housing template");
	const auto site=MakeBuilding("market",12,8,7,true,.25f);
	const auto complete=MakeBuilding("market",12,8,7,true,1.0f);
	Vec3 slo,shi,clo,chi;site.Bounds(slo,shi);complete.Bounds(clo,chi);
	Require(site.Valid()&&complete.Valid()&&shi.y<chi.y,"site scaffold grows with snapshot progress");
	std::cout<<"procgeo buildings: category, footprint, determinism and construction bounds verified\n";
}
