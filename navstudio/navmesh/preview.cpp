// Offline polygon-mesh experiment. Never emits/install Omni-bot WAY files.
#include <Recast.h>
#include <RecastAlloc.h>
#include "../../src/game/surfaceflags.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <map>
#include <set>
#include <tuple>

static void require(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
struct Context : rcContext {
    void doLog(rcLogCategory, const char *message, int length) override {
        std::cerr.write(message, length); std::cerr << '\n';
    }
};

// Only one padded tile is alive at a time. Bound Recast allocations as well as
// cell count: stacked floors can otherwise exceed a simple XY memory estimate.
static constexpr size_t recastLimit = 256u * 1024u * 1024u;
static size_t recastLive=0, recastPeak=0;
struct alignas(std::max_align_t) Allocation { size_t size; };
static void *boundedAlloc(size_t bytes, rcAllocHint) {
    if(bytes>recastLimit || recastLive>recastLimit-bytes) return nullptr;
    auto *p=static_cast<Allocation *>(std::malloc(sizeof(Allocation)+bytes));
    if(!p) return nullptr;
    p->size=bytes;recastLive+=bytes;recastPeak=std::max(recastPeak,recastLive);
    return p+1;
}
static void boundedFree(void *p) {
    if(!p)return;
    auto *a=static_cast<Allocation *>(p)-1;
    recastLive-=a->size;std::free(a);
}
using PolyMesh=std::unique_ptr<rcPolyMesh,decltype(&rcFreePolyMesh)>;
using DetailMesh=std::unique_ptr<rcPolyMeshDetail,decltype(&rcFreePolyMeshDetail)>;
struct ContourProblem {
    int polygon,incident;
    std::vector<std::array<unsigned short,3>> vertices;
};
struct ContourAttempt {
    float error;int polygons,vertices;
    std::array<float,3> origin;
    std::vector<ContourProblem> problems;
};
struct ContourRefinement { int tx,tz;std::vector<ContourAttempt> attempts; };
// A simplified contour can collapse both sides of a thin, eroded corridor
// onto the same grid line. Rebuild its tile from the SAME compact field with
// a tighter error; never invent a link across the resulting zero-width chain.
static std::vector<ContourProblem> contourProblems(const rcPolyMesh &mesh) {
    std::vector<std::set<int>> incident(mesh.npolys);
    for(int i=0;i<mesh.npolys;i++) {
        const auto *p=&mesh.polys[i*mesh.nvp*2];
        for(int e=0;e<mesh.nvp&&p[e]!=RC_MESH_NULL_IDX;e++) {
            const int n=p[mesh.nvp+e];
            if(n==RC_MESH_NULL_IDX)continue;
            if(n&0x8000)incident[i].insert(-2-e); // Potential neighbor after tile stitching.
            else {require(n<mesh.npolys&&n!=i,"Invalid Recast polygon neighbor");incident[i].insert(n);incident[n].insert(i);}
        }
    }
    std::vector<ContourProblem> problems;
    for(int i=0;i<mesh.npolys;i++) {
        const auto *p=&mesh.polys[i*mesh.nvp*2];int count=0;
        while(count<mesh.nvp&&p[count]!=RC_MESH_NULL_IDX)count++;
        require(count>=3,"Invalid Recast polygon vertex count");
        long long area=0;
        for(int e=0;e<count;e++) {
            const auto *a=&mesh.verts[p[e]*3],*b=&mesh.verts[p[(e+1)%count]*3];
            area+=(long long)a[0]*b[2]-(long long)b[0]*a[2];
        }
        if(area!=0)continue;
        const auto *origin=&mesh.verts[p[0]*3];int second=1;
        while(second<count&&mesh.verts[p[second]*3]==origin[0]&&mesh.verts[p[second]*3+2]==origin[2])second++;
        if(second<count) {
            const auto *direction=&mesh.verts[p[second]*3];
            for(int e=0;e<count;e++) {
                const auto *v=&mesh.verts[p[e]*3];
                require((long long)(int(direction[0])-origin[0])*(int(v[2])-origin[2])-
                        (long long)(int(direction[2])-origin[2])*(int(v[0])-origin[0])==0,
                        "Non-collinear zero-area Recast polygon; no mesh written");
            }
        }
        if(incident[i].size()<=1)continue;
        ContourProblem problem{i,int(incident[i].size()),{}};
        for(int e=0;e<count;e++){const auto *v=&mesh.verts[p[e]*3];problem.vertices.push_back({v[0],v[1],v[2]});}
        problems.push_back(std::move(problem));
    }
    return problems;
}
static PolyMesh buildMesh(Context &ctx,const rcConfig &cfg,const std::vector<float> &vertices,
                          const std::vector<int> &triangles,const std::vector<unsigned char> &areas,
                          ContourRefinement *refinement=nullptr,DetailMesh *detailOut=nullptr) {
    std::unique_ptr<rcHeightfield,decltype(&rcFreeHeightField)> field(rcAllocHeightfield(),rcFreeHeightField);
    std::unique_ptr<rcCompactHeightfield,decltype(&rcFreeCompactHeightfield)> compact(rcAllocCompactHeightfield(),rcFreeCompactHeightfield);
    std::unique_ptr<rcContourSet,decltype(&rcFreeContourSet)> contours(rcAllocContourSet(),rcFreeContourSet);
    PolyMesh mesh(rcAllocPolyMesh(),rcFreePolyMesh);
    require(field&&compact&&contours&&mesh,"Recast allocation limit exceeded");
    require(rcCreateHeightfield(&ctx,*field,cfg.width,cfg.height,cfg.bmin,cfg.bmax,cfg.cs,cfg.ch),"Heightfield failed");
    require(rcRasterizeTriangles(&ctx,vertices.data(),int(vertices.size()/3),triangles.data(),
                                 areas.data(),int(areas.size()),*field,cfg.walkableClimb),"Rasterization failed");
    rcFilterLowHangingWalkableObstacles(&ctx,cfg.walkableClimb,*field);
    rcFilterLedgeSpans(&ctx,cfg.walkableHeight,cfg.walkableClimb,*field);
    rcFilterWalkableLowHeightSpans(&ctx,cfg.walkableHeight,*field);
    require(rcBuildCompactHeightfield(&ctx,cfg.walkableHeight,cfg.walkableClimb,*field,*compact),"Compact field failed");
    field.reset();
    require(rcErodeWalkableArea(&ctx,cfg.walkableRadius,*compact),"Erosion failed");
    require(rcBuildDistanceField(&ctx,*compact),"Distance field failed");
    require(rcBuildRegions(&ctx,*compact,cfg.borderSize,cfg.minRegionArea,cfg.mergeRegionArea),"Region build failed");
    const float errors[]={cfg.maxSimplificationError,0.9f,0.5f};
    for(size_t attempt=0;attempt<3;attempt++) {
        if(attempt) {
            // Release previous contour/mesh allocations before the bounded retry.
            contours.reset();mesh.reset();
            contours.reset(rcAllocContourSet());mesh.reset(rcAllocPolyMesh());
            require(contours&&mesh,"Recast allocation limit exceeded during contour retry");
        }
        require(rcBuildContours(&ctx,*compact,errors[attempt],cfg.maxEdgeLen,*contours),"Contours failed");
        require(rcBuildPolyMesh(&ctx,*contours,cfg.maxVertsPerPoly,*mesh),"Polygon mesh failed");
        require(mesh->nverts<65535&&mesh->npolys<32768,"Tile exceeds Recast 16-bit mesh capacity");
        auto problems=contourProblems(*mesh);
        const bool failed=!problems.empty();
        if(refinement&&(attempt||failed))refinement->attempts.push_back({errors[attempt],mesh->npolys,mesh->nverts,
            {mesh->bmin[0],mesh->bmin[1],mesh->bmin[2]},std::move(problems)});
        if(!failed)break;
        require(attempt<2,"Non-leaf zero-area contour remains after bounded precision retries; no mesh written");
    }
    if(detailOut&&mesh->npolys) {
        detailOut->reset(rcAllocPolyMeshDetail());
        require(bool(*detailOut),"Recast detail allocation limit exceeded");
        // Polygon corners alone cannot represent a curved or uneven floor.
        // Keep compact-field detail samples instead of interpolating a fan
        // over a whole simplified six-vertex polygon.
        require(rcBuildPolyMeshDetail(&ctx,*mesh,*compact,cfg.cs*4.f,cfg.ch,*detailOut->get()),
                "Polygon detail height mesh failed");
    }
    return mesh;
}

// JSON uses full-size integer indices, so aggregate tiles are not constrained
// by rcPolyMesh's 16-bit vertex/poly representation. Keep the consumer's 50k
// polygon limit explicit instead of producing a report it cannot load.
struct Vertex { double x,y,z; }; // integer grid X/Z; possibly interpolated Y.
struct HeightDomain { bool enabled=false;float minimum=0,maximum=0; };
// Bound vertical spans from triangle portions INSIDE the padded tile. Large
// sloped brushes outside the map must not dictate every tile's Y allocation.
static bool clippedHeightBounds(const float *a,const float *b,const float *c,const rcConfig &tile,
                                double &low,double &high,const HeightDomain *domain=nullptr) {
    std::array<Vertex,12> first{},second{};
    first[0]={a[0],a[1],a[2]};first[1]={b[0],b[1],b[2]};first[2]={c[0],c[1],c[2]};int count=3;
    for(int plane=0;plane<4;plane++) {
        const int axis=plane<2?0:2;const bool minimum=(plane%2)==0;
        const double boundary=minimum?tile.bmin[axis]:tile.bmax[axis];int next=0;
        auto distance=[&](const Vertex &v){return ((axis==0?v.x:v.z)-boundary)*(minimum?1:-1);};
        for(int i=0;i<count;i++) {
            const auto &p=first[i],&q=first[(i+1)%count];double dp=distance(p),dq=distance(q);
            if(dp>=0)second[next++]=p;
            if((dp>=0)!=(dq>=0)) {const double t=dp/(dp-dq);
                second[next++]={p.x+(q.x-p.x)*t,p.y+(q.y-p.y)*t,p.z+(q.z-p.z)*t};}
        }
        first=second;count=next;if(!count)return false;
    }
    double localLow=1000000,localHigh=-1000000;
    for(int i=0;i<count;i++){localLow=std::min(localLow,first[i].y);localHigh=std::max(localHigh,first[i].y);}
    if(domain&&domain->enabled) {
        localLow=std::max(localLow,double(domain->minimum));localHigh=std::min(localHigh,double(domain->maximum));
        if(localLow>localHigh)return false;
    }
    low=std::min(low,localLow);high=std::max(high,localHigh);
    return true;
}
struct Polygon { std::vector<int> vertices,neighbors,portals;std::vector<std::array<float,9>> detail; int region,tx,tz; };
struct Surface { std::vector<Vertex> vertices;std::vector<Polygon> polygons; };
struct DegenerateLeaf { int polygon,tx,tz;std::vector<int> vertices,neighbors; };
struct DegenerateCleanup { int inputPolygons=0;std::vector<int> remap;std::vector<DegenerateLeaf> removed; };
// Some Recast contours produce an exactly collinear terminal triangle. Such
// a zero-width leaf has no traversable area and cannot connect two regions.
// Do not use an epsilon: every positive-area polygon must survive unchanged.
static DegenerateCleanup removeDegenerateLeaves(Surface &surface) {
    DegenerateCleanup result;result.inputPolygons=int(surface.polygons.size());
    result.remap.assign(surface.polygons.size(),-1);
    std::vector<std::set<int>> incident(surface.polygons.size());
    for(size_t i=0;i<surface.polygons.size();i++) {
        const auto &p=surface.polygons[i];
        require(p.vertices.size()>=3&&p.neighbors.size()==p.vertices.size(),"Invalid polygon layout before degenerate cleanup");
        for(int n:p.neighbors)if(n>=0) {
            require(n<int(surface.polygons.size())&&n!=int(i),"Invalid neighbor before degenerate cleanup");
            incident[i].insert(n);incident[n].insert(int(i));
        }
    }
    std::vector<bool> remove(surface.polygons.size(),false);
    for(size_t i=0;i<surface.polygons.size();i++) {
        const auto &p=surface.polygons[i];double area=0;
        for(size_t e=0;e<p.vertices.size();e++) {
            const auto &a=surface.vertices[p.vertices[e]],&b=surface.vertices[p.vertices[(e+1)%p.vertices.size()]];
            area+=a.x*b.z-b.x*a.z;
        }
        if(area!=0)continue;
        const auto &origin=surface.vertices[p.vertices[0]];
        size_t second=1;
        while(second<p.vertices.size()&&surface.vertices[p.vertices[second]].x==origin.x&&
              surface.vertices[p.vertices[second]].z==origin.z)second++;
        bool collinear=true;
        if(second<p.vertices.size()) {
            const auto &direction=surface.vertices[p.vertices[second]];
            for(int v:p.vertices) {
                const auto &q=surface.vertices[v];
                if((direction.x-origin.x)*(q.z-origin.z)-(direction.z-origin.z)*(q.x-origin.x)!=0)collinear=false;
            }
        }
        if(!collinear||incident[i].size()>1) {
            std::ostringstream why;why<<"Non-leaf or non-collinear zero-area polygon "<<i
                <<" tile "<<p.tx<<','<<p.tz<<" incident_neighbors="<<incident[i].size()<<" grid_vertices=";
            for(int v:p.vertices){const auto &q=surface.vertices[v];why<<'['<<q.x<<','<<q.y<<','<<q.z<<']';}
            why<<"; no mesh written and no positive-area polygon removed";
            throw std::runtime_error(why.str());
        }
        remove[i]=true;
        result.removed.push_back({int(i),p.tx,p.tz,p.vertices,
                                  std::vector<int>(incident[i].begin(),incident[i].end())});
    }
    int count=0;
    for(size_t i=0;i<surface.polygons.size();i++)if(!remove[i])result.remap[i]=count++;
    std::vector<Polygon> kept;kept.reserve(count);
    for(size_t i=0;i<surface.polygons.size();i++)if(!remove[i]) {
        Polygon p=surface.polygons[i];
        for(int &n:p.neighbors)if(n>=0)n=result.remap[n];
        kept.push_back(std::move(p));
    }
    surface.polygons=std::move(kept);
    return result;
}
static void appendMesh(Surface &out,const rcPolyMesh &mesh,const rcConfig &global,int tx,int tz,const rcPolyMeshDetail *detail=nullptr) {
    const int vertexBase=int(out.vertices.size()),polyBase=int(out.polygons.size());
    require(out.vertices.size()+mesh.nverts<=500000&&out.polygons.size()+mesh.npolys<=50000,
            "Aggregate polygon/vertex review limit exceeded");
    const int ox=int(std::lround((mesh.bmin[0]-global.bmin[0])/global.cs));
    const int oy=int(std::lround((mesh.bmin[1]-global.bmin[1])/global.ch));
    const int oz=int(std::lround((mesh.bmin[2]-global.bmin[2])/global.cs));
    for(int i=0;i<mesh.nverts;i++) {
        const unsigned short *v=&mesh.verts[i*3];
        out.vertices.push_back({double(ox+v[0]),double(oy+v[1]),double(oz+v[2])});
    }
    for(int i=0;i<mesh.npolys;i++) {
        const unsigned short *p=&mesh.polys[i*mesh.nvp*2];
        Polygon poly;poly.region=mesh.regs[i];poly.tx=tx;poly.tz=tz;
        for(int j=0;j<mesh.nvp&&p[j]!=RC_MESH_NULL_IDX;j++) {
            const int n=p[mesh.nvp+j];
            poly.vertices.push_back(vertexBase+p[j]);
            poly.neighbors.push_back(n&0x8000?-1:polyBase+n);
            poly.portals.push_back(n!=RC_MESH_NULL_IDX&&(n&0x8000)?n&0xf:-1);
        }
        if(detail) {
            require(detail->nmeshes==mesh.npolys,"Recast detail polygon count mismatch");
            const unsigned int *sub=&detail->meshes[i*4];
            for(unsigned int triangle=0;triangle<sub[3];triangle++) {
                const unsigned char *indices=&detail->tris[(sub[2]+triangle)*4];
                std::array<float,9> points{};
                for(int corner=0;corner<3;corner++) {
                    require(indices[corner]<sub[1],"Recast detail vertex outside submesh");
                    const float *v=&detail->verts[(sub[0]+indices[corner])*3];
                    points[corner*3]=v[0];points[corner*3+1]=-v[2];points[corner*3+2]=v[1];
                }
                poly.detail.push_back(points);
            }
        }
        out.polygons.push_back(std::move(poly));
    }
}
struct Portal {
    int polygon,edge,side,axis,boundary,lo,hi;
    std::vector<int> cuts;
    struct Link { int lo,hi,polygon,portal; };
    std::vector<Link> links;
};
static double along(const Vertex &v,int axis) {return axis==0?v.z:v.x;}
static Vertex interpolate(const Surface &out,const Portal &portal,int coordinate) {
    const auto &p=out.polygons[portal.polygon];
    const auto &a=out.vertices[p.vertices[portal.edge]], &b=out.vertices[p.vertices[(portal.edge+1)%p.vertices.size()]];
    const double t=(coordinate-along(a,portal.axis))/(along(b,portal.axis)-along(a,portal.axis));
    return portal.axis==0?Vertex{double(portal.boundary),a.y+(b.y-a.y)*t,double(coordinate)}:
                          Vertex{double(coordinate),a.y+(b.y-a.y)*t,double(portal.boundary)};
}
struct SeamRejection { int polygon;bool reverse;Vertex a,b;std::vector<int> targets; };
struct StitchStats {
    int portals=0,pairedIntervals=0,linkedSegments=0,unmatchedInterior=0,ambiguous=0;long long comparisons=0;
    std::vector<SeamRejection> rejections;
};
static int uniqueNeighbor(const Portal &edge,int lo,int hi) {
    int neighbor=-1;
    for(const auto &link:edge.links)if(link.lo<=lo&&link.hi>=hi) {
        if(neighbor>=0&&neighbor!=link.polygon)return -2;
        neighbor=link.polygon;
    }
    return neighbor;
}
static StitchStats stitch(Surface &out,int tilesX,int tilesZ,int climb) {
    StitchStats stats;std::vector<Portal> edges;
    std::map<std::pair<int,int>,std::vector<int>> groups;
    std::vector<std::vector<int>> lookup(out.polygons.size());
    for(size_t i=0;i<out.polygons.size();i++) {
        const auto &p=out.polygons[i];lookup[i].assign(p.vertices.size(),-1);
        for(size_t e=0;e<p.vertices.size();e++)if(p.portals[e]>=0) {
            int side=p.portals[e],axis=(side==0||side==2)?0:2;
            const auto &a=out.vertices[p.vertices[e]],&b=out.vertices[p.vertices[(e+1)%p.vertices.size()]];
            require((axis==0?a.x==b.x:a.z==b.z)&&along(a,axis)!=along(b,axis),"Invalid tile portal");
            Portal portal{int(i),int(e),side,axis,int(axis==0?a.x:a.z),
                          int(std::min(along(a,axis),along(b,axis))),int(std::max(along(a,axis),along(b,axis))),{}, {}};
            portal.cuts={portal.lo,portal.hi};
            lookup[i][e]=int(edges.size());groups[{axis,portal.boundary}].push_back(int(edges.size()));
            edges.push_back(std::move(portal));
        }
    }
    stats.portals=int(edges.size());
    for(auto &item:groups) {
        auto &group=item.second;
        // Shared integer cut coordinates are also needed when a third surface
        // overlaps one side of a seam. This ensures an omitted ambiguous span
        // is omitted over exactly the same interval in both directions.
        std::vector<int> cuts;
        for(int id:group){cuts.push_back(edges[id].lo);cuts.push_back(edges[id].hi);}
        std::sort(cuts.begin(),cuts.end());cuts.erase(std::unique(cuts.begin(),cuts.end()),cuts.end());
        for(int id:group) {
            auto &e=edges[id];e.cuts.assign(std::lower_bound(cuts.begin(),cuts.end(),e.lo),
                                           std::upper_bound(cuts.begin(),cuts.end(),e.hi));
        }
        std::sort(group.begin(),group.end(),[&](int a,int b){return edges[a].lo<edges[b].lo;});
        for(size_t i=0;i<group.size();i++)for(size_t j=i+1;j<group.size();j++) {
            auto &a=edges[group[i]],&b=edges[group[j]];
            if(b.lo>=a.hi)break;
            require(++stats.comparisons<=2000000,"Tile portal matching work limit exceeded");
            if((a.side+2)%4!=b.side)continue;
            const auto &pa=out.polygons[a.polygon],&pb=out.polygons[b.polygon];
            const int dx=a.side==0?-1:(a.side==2?1:0), dz=a.side==3?-1:(a.side==1?1:0);
            if(pa.tx+dx!=pb.tx||pa.tz+dz!=pb.tz)continue;
            int lo=std::max(a.lo,b.lo),hi=std::min(a.hi,b.hi);if(lo>=hi)continue;
            // Both independently padded fields must expose an opposite portal.
            // Compare the whole linear overlap, not just nearest endpoints; a
            // stacked surface cannot become a proximity link through a wall.
            if(std::fabs(interpolate(out,a,lo).y-interpolate(out,b,lo).y)>climb||
               std::fabs(interpolate(out,a,hi).y-interpolate(out,b,hi).y)>climb)continue;
            a.links.push_back({lo,hi,b.polygon,group[j]});b.links.push_back({lo,hi,a.polygon,group[i]});stats.pairedIntervals++;
        }
    }
    // Split T-junctions on BOTH sides. One JSON neighbor belongs to precisely
    // one complete polygon side; silently attaching a short overlap to a long
    // unsplit side would let the corridor finder cross a blocked seam section.
    std::vector<Polygon> rebuilt;rebuilt.reserve(out.polygons.size());
    for(size_t i=0;i<out.polygons.size();i++) {
        const auto &source=out.polygons[i];Polygon p;p.region=source.region;p.tx=source.tx;p.tz=source.tz;p.detail=source.detail;
        for(size_t e=0;e<source.vertices.size();e++) {
            const int id=lookup[i][e];
            if(id<0){p.vertices.push_back(source.vertices[e]);p.neighbors.push_back(source.neighbors[e]);continue;}
            auto &edge=edges[id];auto &cuts=edge.cuts;
            std::sort(cuts.begin(),cuts.end());cuts.erase(std::unique(cuts.begin(),cuts.end()),cuts.end());
            const auto start=out.vertices[source.vertices[e]];
            if(along(start,edge.axis)>cuts.front())std::reverse(cuts.begin(),cuts.end());
            for(size_t c=0;c+1<cuts.size();c++) {
                const int lo=std::min(cuts[c],cuts[c+1]),hi=std::max(cuts[c],cuts[c+1]);
                int neighbor=uniqueNeighbor(edge,lo,hi);bool reverseAmbiguous=false;
                if(neighbor>=0)for(const auto &link:edge.links)if(link.lo<=lo&&link.hi>=hi)
                    if(uniqueNeighbor(edges[link.portal],lo,hi)!=int(i))reverseAmbiguous=true;
                if(neighbor==-2||reverseAmbiguous) {
                    require(stats.rejections.size()<25000,"Ambiguous seam diagnostic limit exceeded");
                    SeamRejection r{int(i),reverseAmbiguous,interpolate(out,edge,lo),interpolate(out,edge,hi),{}};
                    for(const auto &link:edge.links)if(link.lo<=lo&&link.hi>=hi)r.targets.push_back(link.polygon);
                    stats.rejections.push_back(std::move(r));stats.ambiguous++;neighbor=-1;
                }
                if(neighbor>=0)stats.linkedSegments++;
                else if((edge.side==0&&source.tx>0)||(edge.side==2&&source.tx+1<tilesX)||
                        (edge.side==3&&source.tz>0)||(edge.side==1&&source.tz+1<tilesZ))stats.unmatchedInterior++;
                if(c==0)p.vertices.push_back(source.vertices[e]);
                else {require(out.vertices.size()<500000,"Seam vertex limit exceeded");
                      p.vertices.push_back(int(out.vertices.size()));out.vertices.push_back(interpolate(out,edge,cuts[c]));}
                p.neighbors.push_back(neighbor);
            }
        }
        require(p.vertices.size()<=4096,"Seam subdivision limit exceeded");
        rebuilt.push_back(std::move(p));
    }
    out.polygons=std::move(rebuilt);
    // Reciprocal interval check catches segmentation errors before downstream
    // component/route analysis can mistake one-way seam adjacency for a path.
    for(size_t i=0;i<out.polygons.size();i++) {
        const auto &p=out.polygons[i];
        for(size_t e=0;e<p.vertices.size();e++)if(p.neighbors[e]>=0) {
            const auto &q=out.polygons[p.neighbors[e]];
            if(p.tx==q.tx&&p.tz==q.tz)continue;
            const auto &a=out.vertices[p.vertices[e]],&b=out.vertices[p.vertices[(e+1)%p.vertices.size()]];
            bool reciprocal=false;
            for(size_t f=0;f<q.vertices.size();f++)if(q.neighbors[f]==int(i)) {
                const auto &c=out.vertices[q.vertices[f]],&d=out.vertices[q.vertices[(f+1)%q.vertices.size()]];
                if(a.x==d.x&&a.z==d.z&&b.x==c.x&&b.z==c.z)reciprocal=true;
            }
            require(reciprocal,"Nonreciprocal tile seam; no mesh written");
        }
    }
    return stats;
}
int main(int argc, char **argv) try {
    require(argc >= 5 && argc <= 12,
            "Usage: tce_navmesh_preview collision.obj mesh.json standing_height cell_size [--tile-size 32..512] [--radius 14|16] [--height-domain MIN MAX]");
    require(std::filesystem::weakly_canonical(argv[1]) != std::filesystem::weakly_canonical(argv[2]),
            "Output must not overwrite collision input");
    const float height = std::stof(argv[3]), cell = std::stof(argv[4]);
    require(std::isfinite(height) && height >= 32 && height <= 128 &&
            std::isfinite(cell) && cell >= 2 && cell <= 16, "Invalid height/cell size");
    int requestedTile=0;float radius=16.f;bool tileSpecified=false,radiusSpecified=false;HeightDomain domain;
    for(int i=5;i<argc;) {
        const std::string option=argv[i++];require(i<argc,"Missing profile option value");
        const std::string value=argv[i++];size_t parsed=0;
        if(option=="--tile-size") {
            require(!tileSpecified,"Duplicate tile size");tileSpecified=true;
            requestedTile=std::stoi(value,&parsed);
            require(parsed==value.size()&&requestedTile>=32&&requestedTile<=512,"Invalid tile size");
        } else if(option=="--radius") {
            require(!radiusSpecified,"Duplicate radius");radiusSpecified=true;
            radius=std::stof(value,&parsed);
            require(parsed==value.size()&&(radius==14.f||radius==16.f),"Radius must be 14 or 16 TCE units");
        } else if(option=="--height-domain") {
            require(!domain.enabled,"Duplicate height domain");require(i<argc,"Missing height domain maximum");
            domain.minimum=std::stof(value,&parsed);require(parsed==value.size(),"Invalid height domain minimum");
            const std::string maximum=argv[i++];domain.maximum=std::stof(maximum,&parsed);
            require(parsed==maximum.size()&&std::isfinite(domain.minimum)&&std::isfinite(domain.maximum)&&
                    domain.minimum<domain.maximum&&std::fabs(domain.minimum)<1000000&&std::fabs(domain.maximum)<1000000,
                    "Invalid height domain bounds");domain.enabled=true;
        } else require(false,"Unknown profile option");
    }
    std::ifstream input(argv[1]); require(bool(input), "Cannot open collision OBJ");
    std::vector<float> vertices; std::vector<int> triangles;
    std::vector<unsigned int> surfaceFlags; unsigned int materialFlags=0;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream row(line); std::string kind; row >> kind;
        if (kind == "v") {
            float x,y,z; require(bool(row >> x >> y >> z), "Invalid OBJ vertex");
            require(std::isfinite(x) && std::isfinite(y) && std::isfinite(z) &&
                    std::fabs(x)<1000000 && std::fabs(y)<1000000 && std::fabs(z)<1000000,
                    "Non-finite/out-of-range vertex");
            vertices.insert(vertices.end(), {x,y,z});
        } else if (kind == "usemtl") {
            std::string name;row >> name;
            require(name.rfind("surfaceflags_",0)==0,"Unsupported collision material");
            materialFlags=static_cast<unsigned int>(std::stoul(name.substr(13)));
        } else if (kind == "f") {
            int a,b,c; std::string extra;
            require(bool(row >> a >> b >> c) && !(row >> extra), "Expected bare triangular OBJ face");
            require(a>0 && b>0 && c>0, "Invalid face index");
            triangles.insert(triangles.end(), {a-1,b-1,c-1});
            surfaceFlags.push_back(materialFlags);
        }
        require(vertices.size() <= 12000000 && triangles.size() <= 24000000, "Geometry limit exceeded");
    }
    require(!vertices.empty() && !triangles.empty(), "Empty geometry");
    for (int i:triangles) require(i < int(vertices.size()/3), "Face index out of range");
    rcAllocSetCustom(boundedAlloc,boundedFree);
    Context ctx; rcConfig cfg{};
    cfg.cs=cell; cfg.ch=2; cfg.walkableSlopeAngle=45;
    cfg.walkableHeight=int(std::ceil(height/cfg.ch));
    cfg.walkableClimb=9; // 18 TCE units; no jump links inferred.
    cfg.walkableRadius=int(std::ceil(radius/cfg.cs));
    cfg.maxEdgeLen=int(256/cfg.cs); cfg.maxSimplificationError=1.1f;
    // Keep small regions visible for review: disconnected does not mean irrelevant.
    cfg.minRegionArea=0; cfg.mergeRegionArea=400; cfg.maxVertsPerPoly=6;
    rcCalcBounds(vertices.data(), int(vertices.size()/3), cfg.bmin, cfg.bmax);
    const float geometryMinY=cfg.bmin[1],geometryMaxY=cfg.bmax[1];
    if(domain.enabled) {
        cfg.bmin[1]=std::max(cfg.bmin[1],domain.minimum);
        // A horizontal-only input still needs one raster cell for its surface.
        cfg.bmax[1]=std::min(cfg.bmax[1]+cfg.ch,domain.maximum);
        require(cfg.bmin[1]<cfg.bmax[1],"Empty height domain: no geometry intersects its interior");
    }
    rcCalcGridSize(cfg.bmin,cfg.bmax,cfg.cs,&cfg.width,&cfg.height);
    require(cfg.width>0 && cfg.height>0,"Empty grid");
    std::vector<unsigned char> areas(triangles.size()/3,RC_NULL_AREA);
    rcMarkWalkableTriangles(&ctx,cfg.walkableSlopeAngle,vertices.data(),int(vertices.size()/3),
                            triangles.data(),int(areas.size()),areas.data());
    // PM_WalkMove still walks on horizontal SURF_SLICK (air acceleration).
    // A blanket exclusion removed whole playable ice floors. Sloped slick
    // remains outside ordinary stable navigation until sliding is simulated.
    int flatSlickTriangles=0;
    for(size_t i=0;i<areas.size();i++) {
        if(surfaceFlags[i]&SURF_SKY){areas[i]=RC_NULL_AREA;continue;}
        if(surfaceFlags[i]&SURF_SLICK) {
            const float *a=&vertices[triangles[i*3]*3],*b=&vertices[triangles[i*3+1]*3],*c=&vertices[triangles[i*3+2]*3];
            double ux=b[0]-a[0],uy=b[1]-a[1],uz=b[2]-a[2],vx=c[0]-a[0],vy=c[1]-a[1],vz=c[2]-a[2];
            double nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
            double length=std::sqrt(nx*nx+ny*ny+nz*nz);
            if(length<=0||ny/length<.9999)areas[i]=RC_NULL_AREA;
            else if(areas[i]!=RC_NULL_AREA)flatSlickTriangles++;
        }
    }
    size_t domainBelow=0,domainAbove=0,domainCrossing=0;
    if(domain.enabled) {
        std::vector<int> selected;std::vector<unsigned char> selectedAreas;
        for(size_t i=0;i<areas.size();i++) {
            const float a=vertices[triangles[i*3]*3+1],b=vertices[triangles[i*3+1]*3+1],c=vertices[triangles[i*3+2]*3+1];
            const float low=std::min({a,b,c}),high=std::max({a,b,c});
            if(high<domain.minimum){domainBelow++;continue;}
            if(low>domain.maximum){domainAbove++;continue;}
            if(low<domain.minimum||high>domain.maximum)domainCrossing++;
            selected.insert(selected.end(),triangles.begin()+i*3,triangles.begin()+i*3+3);selectedAreas.push_back(areas[i]);
        }
        require(!selected.empty(),"Empty height domain: no triangle intersects it");
        // Original vertices, triangle normals and materials are unchanged. Recast
        // clips raster spans to its heightfield bounds; no horizontal cap is added.
        triangles.swap(selected);areas.swap(selectedAreas);
    }
    const bool tiled=requestedTile || (long long)cfg.width*cfg.height>8000000;
    const int tileCells=tiled?(requestedTile?requestedTile:512):0;
    const int tilesX=tiled?(cfg.width+tileCells-1)/tileCells:1,tilesZ=tiled?(cfg.height+tileCells-1)/tileCells:1;
    require((long long)tilesX*tilesZ<=4096,"Bounded tile-count limit exceeded");
    Surface surface;StitchStats seams;int builtTiles=0,emptyTiles=0,maxPaddedCells=0;
    std::vector<ContourRefinement> refinements;
    if(!tiled) {
        require((cfg.bmax[1]-cfg.bmin[1])/cfg.ch<RC_SPAN_MAX_HEIGHT,"Vertical span limit exceeded");
        ContourRefinement refinement{0,0,{}};
        DetailMesh detail(nullptr,rcFreePolyMeshDetail);
        auto mesh=buildMesh(ctx,cfg,vertices,triangles,areas,&refinement,&detail);
        if(!refinement.attempts.empty())refinements.push_back(std::move(refinement));
        appendMesh(surface,*mesh,cfg,0,0,detail.get());builtTiles=1;maxPaddedCells=cfg.width*cfg.height;
    } else {
        std::vector<std::array<float,4>> bounds;bounds.reserve(areas.size());
        for(size_t i=0;i<areas.size();i++) {
            const float *a=&vertices[triangles[i*3]*3],*b=&vertices[triangles[i*3+1]*3],*c=&vertices[triangles[i*3+2]*3];
            bounds.push_back({std::min({a[0],b[0],c[0]}),std::min({a[2],b[2],c[2]}),
                              std::max({a[0],b[0],c[0]}),std::max({a[2],b[2],c[2]})});
        }
        for(int z=0;z<tilesZ;z++)for(int x=0;x<tilesX;x++) {
            rcConfig tile=cfg;tile.tileSize=tileCells;tile.borderSize=cfg.walkableRadius+3;
            const int innerW=std::min(tileCells,cfg.width-x*tileCells),innerH=std::min(tileCells,cfg.height-z*tileCells);
            tile.width=innerW+tile.borderSize*2;tile.height=innerH+tile.borderSize*2;
            tile.bmin[0]=cfg.bmin[0]+(x*tileCells-tile.borderSize)*cfg.cs;
            tile.bmin[2]=cfg.bmin[2]+(z*tileCells-tile.borderSize)*cfg.cs;
            tile.bmax[0]=cfg.bmin[0]+(x*tileCells+innerW+tile.borderSize)*cfg.cs;
            tile.bmax[2]=cfg.bmin[2]+(z*tileCells+innerH+tile.borderSize)*cfg.cs;
            maxPaddedCells=std::max(maxPaddedCells,tile.width*tile.height);
            std::vector<int> selected;std::vector<unsigned char> selectedAreas;
            double low=1000000,high=-1000000;
            for(size_t i=0;i<bounds.size();i++) {
                const auto &b=bounds[i];
                if(b[2]<tile.bmin[0]||b[0]>tile.bmax[0]||b[3]<tile.bmin[2]||b[1]>tile.bmax[2])continue;
                if(!clippedHeightBounds(&vertices[triangles[i*3]*3],&vertices[triangles[i*3+1]*3],
                                        &vertices[triangles[i*3+2]*3],tile,low,high,domain.enabled?&domain:nullptr))continue;
                selected.insert(selected.end(),triangles.begin()+i*3,triangles.begin()+i*3+3);selectedAreas.push_back(areas[i]);
            }
            if(selected.empty()){emptyTiles++;continue;}
            tile.bmin[1]=cfg.bmin[1]+float(std::floor((low-cfg.bmin[1])/cfg.ch))*cfg.ch;
            tile.bmax[1]=cfg.bmin[1]+float(std::ceil((high-cfg.bmin[1])/cfg.ch)+1)*cfg.ch;
            if(domain.enabled) {
                tile.bmin[1]=std::max(tile.bmin[1],cfg.bmin[1]);tile.bmax[1]=std::min(tile.bmax[1],cfg.bmax[1]);
                if(tile.bmin[1]>=tile.bmax[1]){emptyTiles++;continue;}
            }
            require((tile.bmax[1]-tile.bmin[1])/cfg.ch<RC_SPAN_MAX_HEIGHT,"Padded tile vertical span limit exceeded");
            ContourRefinement refinement{x,z,{}};
            DetailMesh detail(nullptr,rcFreePolyMeshDetail);
            auto mesh=buildMesh(ctx,tile,vertices,selected,selectedAreas,&refinement,&detail);
            if(!refinement.attempts.empty())refinements.push_back(std::move(refinement));
            appendMesh(surface,*mesh,cfg,x,z,detail.get());builtTiles++;
            std::cout << "tile " << x+1 << '/' << tilesX << ',' << z+1 << '/' << tilesZ
                      << ": " << mesh->npolys << " polygons\n";
        }
        seams=stitch(surface,tilesX,tilesZ,cfg.walkableClimb);
    }
    require(recastLive==0,"A processed Recast tile was not released");
    const auto degenerate=removeDegenerateLeaves(surface);
    require(!domain.enabled||!surface.polygons.empty(),"Empty height domain: no traversable surface; no mesh written");
    std::ofstream out(argv[2]);require(bool(out),"Cannot write mesh report");
    out << std::setprecision(10);
    out << "{\"schema\":1,\"status\":\"geometry_candidate_not_engine_validated\",\"coordinate_system\":\"TCE_Z_up\",";
    out << "\"floor_height_model\":\"recast_compact_field_detail_triangles_v1\",\"detail_sample_distance\":"<<cfg.cs*4.f
        <<",\"detail_max_height_error\":"<<cfg.ch<<",\"flat_slick_triangles\":"<<flatSlickTriangles
        <<",\"slick_floor_normal_z_minimum\":0.9999,";
    if(domain.enabled)out << "\"height_domain\":{\"requested_minimum\":"<<domain.minimum<<",\"requested_maximum\":"<<domain.maximum
        <<",\"geometry_minimum\":"<<geometryMinY<<",\"geometry_maximum\":"<<geometryMaxY
        <<",\"raster_minimum\":"<<cfg.bmin[1]<<",\"raster_maximum\":"<<cfg.bmax[1]
        <<",\"excluded_triangles_below\":"<<domainBelow<<",\"excluded_triangles_above\":"<<domainAbove
        <<",\"crossing_triangles\":"<<domainCrossing<<",\"retained_triangles\":"<<areas.size()
        <<",\"rule\":\"original_triangles_intersected_by_raster_bounds_no_caps\",\"native_validation_required\":true},";
    out
        << "\"profile\":{\"height\":" << height << ",\"radius\":" << radius << ",\"step\":18,\"cell\":" << cell
        << ",\"cell_height\":2},\"grid\":[" << cfg.width << ',' << cfg.height << "],"
        << "\"tiling\":{\"enabled\":" << (tiled?"true":"false") << ",\"tile_cells\":" << tileCells
        << ",\"border_cells\":" << (tiled?cfg.walkableRadius+3:0) << ",\"tiles_x\":" << tilesX << ",\"tiles_z\":" << tilesZ
        << ",\"built_tiles\":" << builtTiles << ",\"empty_tiles\":" << emptyTiles << ",\"max_padded_cells\":" << maxPaddedCells
        << ",\"recast_peak_bytes\":" << recastPeak << ",\"recast_limit_bytes\":" << recastLimit
        << ",\"portal_edges\":" << seams.portals << ",\"paired_intervals\":" << seams.pairedIntervals
        << ",\"linked_directed_segments\":" << seams.linkedSegments << ",\"unmatched_interior_segments\":" << seams.unmatchedInterior
        << ",\"ambiguous_segments\":" << seams.ambiguous << ",\"portal_comparisons\":" << seams.comparisons
        << ",\"stitch_model\":\"opposite_padded_tile_portals_split_overlap_v1\",\"native_validation_required\":true,\"seam_rejections\":[";
    for(size_t i=0;i<seams.rejections.size();i++) {
        if(i)out<<',';const auto &r=seams.rejections[i];
        out<<"{\"reason\":\""<<(r.reverse?"opposite_portal_ambiguous":"multiple_opposite_portals")
           <<"\",\"polygon\":"<<degenerate.remap[r.polygon]<<",\"original_polygon\":"<<r.polygon<<",\"segment\":[";
        for(int k=0;k<2;k++){if(k)out<<',';const auto &v=k?r.b:r.a;
            out<<'['<<cfg.bmin[0]+v.x*cfg.cs<<','<<-(cfg.bmin[2]+v.z*cfg.cs)<<','<<cfg.bmin[1]+v.y*cfg.ch<<']';}
        out<<"],\"targets\":[";bool first=true;for(int target:r.targets)if(degenerate.remap[target]>=0){if(!first)out<<',';first=false;out<<degenerate.remap[target];}
        out<<"],\"original_targets\":[";for(size_t j=0;j<r.targets.size();j++){if(j)out<<',';out<<r.targets[j];}out<<"]}";
    }
    out << "]},\"degenerate_cleanup\":{\"policy\":\"exact_collinear_zero_area_incident_degree_at_most_one\",\"input_polygons\":"
        <<degenerate.inputPolygons<<",\"removed_count\":"<<degenerate.removed.size()
        <<",\"positive_area_polygons_retained\":"<<surface.polygons.size()<<",\"removed\":[";
    for(size_t i=0;i<degenerate.removed.size();i++) {
        const auto &r=degenerate.removed[i];if(i)out<<',';
        out<<"{\"original_polygon\":"<<r.polygon<<",\"tile\":["<<r.tx<<','<<r.tz<<"],\"signed_area\":0,\"original_neighbors\":[";
        for(size_t j=0;j<r.neighbors.size();j++){if(j)out<<',';out<<r.neighbors[j];}
        out<<"],\"vertices\":[";
        for(size_t j=0;j<r.vertices.size();j++){if(j)out<<',';const auto &v=surface.vertices[r.vertices[j]];
            out<<'['<<cfg.bmin[0]+v.x*cfg.cs<<','<<-(cfg.bmin[2]+v.z*cfg.cs)<<','<<cfg.bmin[1]+v.y*cfg.ch<<']';}
        out<<"]}";
    }
    out << "]},\"contour_refinement\":{\"policy\":\"retry_exact_collinear_nonleaf_zero_area_from_same_compact_field\","
        <<"\"errors\":[1.1,0.9,0.5],\"affected_tiles\":"<<refinements.size()<<",\"tiles\":[";
    for(size_t i=0;i<refinements.size();i++) {
        if(i)out<<',';const auto &r=refinements[i];out<<"{\"tile\":["<<r.tx<<','<<r.tz<<"],\"attempts\":[";
        for(size_t j=0;j<r.attempts.size();j++) {
            if(j)out<<',';const auto &a=r.attempts[j];
            out<<"{\"error\":"<<a.error<<",\"polygons\":"<<a.polygons<<",\"vertices\":"<<a.vertices
                <<",\"zero_area_nonleaf_count\":"<<a.problems.size()<<",\"problems\":[";
            for(size_t k=0;k<a.problems.size();k++) {
                if(k)out<<',';const auto &p=a.problems[k];
                out<<"{\"tile_polygon\":"<<p.polygon<<",\"incident_edges\":"<<p.incident<<",\"vertices\":[";
                for(size_t v=0;v<p.vertices.size();v++){if(v)out<<',';const auto &q=p.vertices[v];
                    out<<'['<<a.origin[0]+q[0]*cfg.cs<<','<<-(a.origin[2]+q[2]*cfg.cs)<<','<<a.origin[1]+q[1]*cfg.ch<<']';}
                out<<"]}";
            }
            out<<"]}";
        }
        out<<"]}";
    }
    out << "]},\"vertices\":[";
    for(size_t i=0;i<surface.vertices.size();i++) {
        const auto &v=surface.vertices[i];
        if(i)out<<',';
        out << '[' << cfg.bmin[0]+v.x*cfg.cs << ',' << -(cfg.bmin[2]+v.z*cfg.cs)
            << ',' << cfg.bmin[1]+v.y*cfg.ch << ']';
    }
    out << "],\"polygons\":[";
    for(size_t i=0;i<surface.polygons.size();i++) {
        if(i)out<<',';
        const auto &p=surface.polygons[i];
        out << "{\"region\":" << p.region << ",\"vertices\":[";
        for(size_t j=0;j<p.vertices.size();j++){if(j)out<<',';out<<p.vertices[j];}
        out << "],\"neighbors\":[";
        for(size_t j=0;j<p.neighbors.size();j++){if(j)out<<',';out<<p.neighbors[j];}
        out << "],\"detail_triangles\":[";
        for(size_t j=0;j<p.detail.size();j++) {
            if(j)out<<',';out<<'[';
            for(int corner=0;corner<3;corner++){if(corner)out<<',';out<<'[';
                for(int k=0;k<3;k++){if(k)out<<',';out<<p.detail[j][corner*3+k];}out<<']';}
            out<<']';
        }
        out << "]}";
    }
    out << "]}\n";out.flush();require(bool(out),"Mesh write failed");
    std::cout << surface.vertices.size() << " vertices, " << surface.polygons.size() << " polygons; candidate only\n";
    return 0;
} catch(const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
