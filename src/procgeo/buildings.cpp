#include "openrepublic/procgeo/buildings.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace openrepublic::procgeo
{
namespace
{
	using namespace std::literals;
	Color3 Mix(Color3 a, Color3 b, float t) { return {a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t}; }
	std::string Lower(std::string_view name) { std::string out(name); for (char& c:out)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return out; }
	bool Has(std::string_view n, std::initializer_list<std::string_view> words) { for(auto w:words) if(n.find(w)!=n.npos)return true; return false; }
	void Box(Mesh& m,float x,float y,float z,float hx,float hy,float hz,Color3 c){AddBox(m,{x,y,z},{hx,hy,hz},c);}
	void Shed(Mesh& m,float w,float d,float h,Color3 wall,Color3 roof,bool doors)
	{
		Box(m,0,h*.42f,0,w*.46f,h*.42f,d*.46f,wall);
		Box(m,0,h*.89f,0,w*.49f,h*.055f,d*.49f,roof);
		if(doors) for(float x:{-w*.23f,w*.23f}) Box(m,x,h*.30f,d*.466f,w*.12f,h*.26f,.025f,{.19f,.22f,.22f});
	}
}

Mesh MakeBuilding(std::string_view catalogName,float width,float depth,std::uint32_t seed,bool site,float progress,bool dark)
{
	Mesh m; if(!(width>0&&depth>0))return m;
	const std::string n=Lower(catalogName);
	const std::array<Color3,6> palettes{{{.63f,.48f,.35f},{.48f,.58f,.61f},{.69f,.63f,.43f},{.57f,.45f,.55f},{.43f,.57f,.44f},{.66f,.54f,.39f}}};
	const Color3 base=palettes[seed%palettes.size()]; const float variation=float((seed>>4)%9)*.012f;
	const Color3 wall=Mix(base,{.82f,.78f,.68f},variation); const Color3 roof{.25f+variation,.28f+variation,.29f+variation};
	const float w=std::max(1.2f,width*.92f),d=std::max(1.2f,depth*.92f),h=std::max(1.8f,std::min(w,d)*.72f);
	if(site)
	{
		const float top=h*std::clamp(progress,.08f,1.0f);
		// A low partial shell and open corner scaffold replace the former height cube.
		Box(m,0,top*.46f,0,w*.42f,top*.46f,d*.42f,{.48f,.43f,.31f});
		const Color3 steel{.78f,.45f,.12f};
		for(float x:{-w*.47f,w*.47f}) for(float z:{-d*.47f,d*.47f}) Box(m,x,top*.5f,z,.055f,top*.5f,.055f,steel);
		for(float y=top*.3f;y<top;y+=std::max(.45f,h*.25f)){Box(m,0,y,-d*.47f,w*.47f,.035f,.035f,steel);Box(m,-w*.47f,y,0,.035f,.035f,d*.47f,steel);}
		return m;
	}
	if(Has(n,{"mine","quarry","gravel","coal_mine"sv}))
	{
		Shed(m,w*.58f,d*.48f,h*.55f,wall,roof,true);
		Box(m,0,h*.90f,0,w*.20f,h*.035f,d*.20f,{.72f,.34f,.12f});
		for(float x:{-w*.15f,w*.15f}){Box(m,x,h*.73f,0,.07f,h*.18f,.07f,{.22f,.24f,.24f});Box(m,x,h*.92f,0,.08f,.07f,.08f,{.22f,.24f,.24f});}
		for(int i=0;i<5;++i)Box(m,(i-2)*w*.11f,h*.12f,-d*.47f+(i%2)*.16f,w*.08f,.11f,.10f,{.27f,.29f,.27f});
	}
	else if(Has(n,{"power","heating","boiler","plant"}))
	{
		Shed(m,w*.72f,d*.70f,h*.72f,{.56f,.53f,.45f},roof,true);
		AddCylinder(m,-w*.28f,-d*.25f,h*.45f,h*1.85f,std::min(w,d)*.10f,10,{.58f,.30f,.19f});
		AddCylinder(m,w*.27f,d*.24f,h*.12f,h*.70f,std::min(w,d)*.18f,12,{.55f,.62f,.61f});
	}
	else if(Has(n,{"mill","bakery","factory","sawmill","brickworks"}))
	{
		Shed(m,w*.78f,d*.70f,h*.62f,wall,roof,true);
		const int teeth=std::max(2,std::min(6,static_cast<int>(w/3.0f)));
		for(int i=0;i<teeth;++i){float x=-w*.38f+(i+.5f)*w*.76f/teeth; Box(m,x,h*.83f,0,w*.76f/teeth*.45f,h*.08f,d*.36f,{.48f,.56f,.56f}); Box(m,x+w*.76f/teeth*.35f,h*.92f,0,w*.08f,h*.08f,d*.36f,roof);}
		AddCylinder(m,w*.29f,-d*.27f,h*.55f,h*1.05f,w*.13f,12,{.66f,.59f,.43f});
	}
	else if(Has(n,{"farm","kolkhoz","sovkhoz"}))
	{
		Shed(m,w*.43f,d*.38f,h*.65f,{.58f,.31f,.19f},roof,true);
		for(int row=0;row<3;++row)for(int col=0;col<4;++col)Box(m,-w*.39f+col*w*.24f,.035f,-d*.40f+row*d*.27f,w*.075f,.035f,d*.09f,{.28f,.42f,.19f});
	}
	else if(Has(n,{"storage","warehouse","granary"})) Shed(m,w,d,h*.72f,wall,roof,true);
	else if(Has(n,{"housing","residential","apartment","house","dormitory"}))
	{
		const int floors=std::clamp(static_cast<int>(h/1.5f),2,8); const float floorH=h/floors;
		Box(m,0,h*.5f,0,w*.44f,h*.5f,d*.44f,wall);
		for(int f=0;f<floors;++f)for(int c=0;c<std::max(2,static_cast<int>(w/1.2f));++c){float x=-w*.34f+c*(w*.68f/std::max(1,static_cast<int>(w/1.2f)-1));Color3 glass=dark?Color3{1,.62f,.22f}:Color3{.16f,.24f,.28f}; Box(m,x,(f+.54f)*floorH,d*.45f,.12f,.16f,.025f,glass);}
	}
	else if(Has(n,{"depot","garage","bus_station","vehicle_station"})) Shed(m,w,d*.78f,h*.55f,wall,roof,true);
	else if(Has(n,{"border","customs","checkpoint"}))
	{
		Shed(m,w*.38f,d*.42f,h*.62f,wall,roof,false); Box(m,0,h*.16f,d*.30f,w*.48f,.055f,.055f,{.92f,.78f,.18f});
	}
	else if(Has(n,{"market","shop","store"}))
	{
		Shed(m,w,d,h*.52f,wall,roof,true); Box(m,0,h*.67f,d*.43f,w*.42f,.10f,.10f,{.72f,.19f,.13f});
	}
	else Shed(m,w,d,h,wall,roof,true);
	return m;
}
}
