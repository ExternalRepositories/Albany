//*****************************************************************//
//    Albany 2.0:  Copyright 2015 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Albany_PUMIMeshStruct.hpp"
#include <apfMDS.h>
#include <sstream>
#include <gmi_base.h>
#include <gmi_lookup.h>

namespace Albany {

struct Indices {
  Indices()
  {
  }
  Indices(int a, int b, int c):
    x(a),y(b),z(c)
  {
  }
  int x;
  int y;
  int z;
  int& operator[](int i)
  {
    switch (i) {
      case 0:
        return x;
      case 1:
        return y;
      case 2:
        return z;
      default:
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
                                   "i must be in {0,1,2}.");
        return x; // for the compiler
    }
  }
  Indices operator+(Indices oi)
  {
    return Indices(x + oi.x, y + oi.y, z + oi.z);
  }
  Indices operator*(int s)
  {
    return Indices(x * s, y * s, z * s);
  }
  static Indices unit(int d)
  {
    Indices i(0,0,0);
    i[d] = 1;
    return i;
  }
};

struct Grid {
  Grid(int nx, int ny, int nz):
    size(nx, ny, nz)
  {
    stride[0] = 1;
    for (int i = 0; i < 3; ++i)
      stride[i + 1] = stride[i] * size[i];
  }
  int total() {return stride[3];}
  Indices out(int i)
  {
    Indices is;
    for (int j = 0; j < 3; ++j)
      is[j] = (i % stride[j + 1]) / stride[j];
    return is;
  }
  int in(Indices is)
  {
    int i = 0;
    for (int j = 0; j < 3; ++j)
      i += is[j] * stride[j];
    return i;
  }
  Indices size;
  int stride[4];
};

struct BoxBuilder {
  Grid grid;
  Grid mgrid;
  int dim;
  double w[3];
  bool is_simplex;
  struct { int dim; int tag; } modelTable[27];
  int modelCounts[4];
  apf::Mesh2* m;
  std::vector<apf::MeshEntity*> v;
  BoxBuilder(int nx, int ny, int nz,
      double wx, double wy, double wz,
      bool is);
  void formModelTable();
  void addModelUse(gmi_base* gb, agm_bdry ab, Indices di);
  gmi_model* buildModel();
  int getModelIndex(int i, int d);
  Indices getModelIndices(Indices vi);
  apf::ModelEntity* getModelEntity(Indices mi);
  void buildCellVert(int i);
  apf::MeshEntity* getVert(Indices vi);
  void buildCellEdges(int i);
  void buildTriangles(apf::MeshEntity* fv[4], apf::ModelEntity* me);
  void buildFace(apf::MeshEntity* fv[4], apf::ModelEntity* me);
  void buildCellFaces(int i);
  void buildTets(apf::MeshEntity* rv[8], apf::ModelEntity* me);
  void buildHex(apf::MeshEntity* rv[8], apf::ModelEntity* me);
  void buildRegion(apf::MeshEntity* rv[8], apf::ModelEntity* me);
  void buildCellRegion(int i);
  void buildCell(int i, int d);
  void buildDimension(int d);
  void buildMeshAndModel();
  void buildSets(apf::StkModels& sets);
};

BoxBuilder::BoxBuilder(int nx, int ny, int nz,
      double wx, double wy, double wz,
      bool is):
  grid(nx + 1, ny + 1, nz + 1),
  mgrid(nx ? 3 : 1,ny ? 3 : 1, nz ? 3 : 1)
{
  for (dim = 0; dim < 3 && grid.size[dim] > 1; ++dim);
  w[0] = nx ? (wx / nx) : 0;
  w[1] = ny ? (wy / ny) : 0;
  w[2] = nz ? (wz / nz) : 0;
  is_simplex = is;
  formModelTable();
  gmi_model* gm = buildModel();
  m = apf::makeEmptyMdsMesh(gm, dim, false);
  v.resize(grid.total());
  buildMeshAndModel();
}

void BoxBuilder::formModelTable()
{
  int nd[4] = {0,0,0,0};
  for (int i = 0; i < mgrid.total(); ++i) {
    Indices mi = mgrid.out(i);
    int mdim = 0;
    for (int j = 0; j < 3; ++j)
      if (mi[j] == 1)
        ++mdim;
    modelTable[i].dim = mdim;
    modelTable[i].tag = nd[mdim]++;
  }
  for (int i = 0; i < 4; ++i)
    modelCounts[i] = nd[i];
}

int BoxBuilder::getModelIndex(int i, int d)
{
  if (i == grid.size[d] - 1)
    return 2;
  if (i == 0)
    return 0;
  return 1;
}

Indices BoxBuilder::getModelIndices(Indices vi)
{
  Indices mi;
  for (int i = 0; i < 3; ++i)
    mi[i] = getModelIndex(vi[i], i);
  return mi;
}

apf::ModelEntity* BoxBuilder::getModelEntity(Indices mi)
{
  int mj = mgrid.in(mi);
  int mdim = modelTable[mj].dim;
  int mtag = modelTable[mj].tag;
  return m->findModelEntity(mdim, mtag);
}

void BoxBuilder::addModelUse(gmi_base* gb, agm_bdry ab, Indices di)
{
  int ddim = modelTable[mgrid.in(di)].dim;
  int dtag = modelTable[mgrid.in(di)].tag;
  agm_ent ad = gmi_look_up(gb->lookup, agm_type_from_dim(ddim), dtag);
  agm_add_use(gb->topo, ab, ad);
}

gmi_model* BoxBuilder::buildModel()
{
  /* plain malloc because gmi_destroy calls plain free */
  gmi_base* gb = (gmi_base*) malloc(sizeof(*gb));
  gb->model.ops = &gmi_base_ops;
  gmi_base_init(gb);
  agm_ent_type at;
  agm_ent ae;
  agm_bdry ab;
  Indices di;
  for (int i = 0; i < mgrid.total(); ++i) {
    int mdim = modelTable[i].dim;
    int mtag = modelTable[i].tag;
    at = agm_type_from_dim(mdim);
    ae = agm_add_ent(gb->topo, at);
    gmi_set_lookup(gb->lookup, ae, mtag);
  }
  for (int i = 0; i < 4; ++i) {
    at = agm_type_from_dim(i);
    gmi_freeze_lookup(gb->lookup, at);
    gb->model.n[i] = agm_ent_count(gb->topo, at);
    assert(gb->model.n[i] == modelCounts[i]);
  }
  for (int i = 0; i < mgrid.total(); ++i) {
    int mdim = modelTable[i].dim;
    int mtag = modelTable[i].tag;
    if (mdim == 0)
      continue;
    at = agm_type_from_dim(mdim);
    ae = gmi_look_up(gb->lookup, at, mtag);
    Indices mi = mgrid.out(i);
    ab = agm_add_bdry(gb->topo, ae);
    for (int j = 0; j < 3; ++j)
      if (mi[j] == 1) {
        di = mi;
        di[j] = 0;
        addModelUse(gb, ab, di);
        di[j] = 2;
        addModelUse(gb, ab, di);
      }
  }
  gmi_write_dmg(&gb->model, "adobe.dmg");
  return &gb->model;
}

void BoxBuilder::buildCellVert(int i)
{
  Indices vi = grid.out(i);
  v[i] = m->createVert(getModelEntity(getModelIndices(vi)));
  apf::Vector3 pt(w[0] * vi.x, w[1] * vi.y, w[2] * vi.z);
  m->setPoint(v[i], 0, pt);
}

apf::MeshEntity* BoxBuilder::getVert(Indices vi)
{
  return v.at(grid.in(vi));
}

void BoxBuilder::buildCellEdges(int i)
{
  Indices vi = grid.out(i);
  Indices mi = getModelIndices(vi);
  apf::MeshEntity* ev[2];
  ev[0] = getVert(vi);
  for (int j = 0; j < 3; ++j) {
    if (mi[j] == 2)
      continue;
    ev[1] = getVert(vi + Indices::unit(j));
    Indices emi = mi;
    emi[j] = 1;
    apf::ModelEntity* me = getModelEntity(emi);
    apf::buildElement(m, me, apf::Mesh::EDGE, ev);
  }
}

void BoxBuilder::buildTriangles(apf::MeshEntity* fv[4], apf::ModelEntity* me)
{
  apf::MeshEntity* tv[3];
  tv[0] = fv[0]; tv[1] = fv[1]; tv[2] = fv[2];
  apf::buildElement(m, me, apf::Mesh::TRIANGLE, tv);
  tv[0] = fv[2]; tv[1] = fv[3]; tv[2] = fv[0];
  apf::buildElement(m, me, apf::Mesh::TRIANGLE, tv);
}

void BoxBuilder::buildFace(apf::MeshEntity* fv[4], apf::ModelEntity* me)
{
  if (is_simplex)
    buildTriangles(fv, me);
  else
    apf::buildElement(m, me, apf::Mesh::QUAD, fv);
}

void BoxBuilder::buildCellFaces(int i)
{
  Indices vi = grid.out(i);
  Indices mi = getModelIndices(vi);
  apf::MeshEntity* fv[4];
  fv[0] = getVert(vi);
  for (int jx = 0; jx < 3; ++jx) {
    int jy = (jx + 1) % 3;
    if (mi[jx] == 2 || mi[jy] == 2)
      continue;
    fv[1] = getVert(vi + Indices::unit(jx));
    fv[2] = getVert(vi + Indices::unit(jx) + Indices::unit(jy));
    fv[3] = getVert(vi + Indices::unit(jy));
    Indices emi = mi;
    emi[jx] = 1;
    emi[jy] = 1;
    apf::ModelEntity* me = getModelEntity(emi);
    buildFace(fv, me);
  }
}

void BoxBuilder::buildTets(apf::MeshEntity* rv[8], apf::ModelEntity* me)
{
  static int const tet_verts[6][4] = {
  {0,1,2,6},
  {0,2,3,6},
  {0,3,7,6},
  {0,7,4,6},
  {0,4,5,6},
  {0,5,1,6}};
  for (int i = 0; i < 6; ++i) {
    apf::MeshEntity* tv[4];
    for (int j = 0; j < 4; ++j)
      tv[j] = rv[tet_verts[i][j]];
    apf::buildElement(m, me, apf::Mesh::TET, tv);
  }
}

void BoxBuilder::buildHex(apf::MeshEntity* rv[8], apf::ModelEntity* me)
{
  apf::buildElement(m, me, apf::Mesh::HEX, rv);
}

void BoxBuilder::buildRegion(apf::MeshEntity* rv[8], apf::ModelEntity* me)
{
  if (is_simplex)
    buildTets(rv, me);
  else
    buildHex(rv, me);
}

void BoxBuilder::buildCellRegion(int i)
{
  Indices vi = grid.out(i);
  Indices mi = getModelIndices(vi);
  if (mi[0] == 2 || mi[1] == 2 || mi[2] == 2)
    return;
  apf::MeshEntity* rv[8];
  rv[0] = getVert(Indices(vi.x + 0, vi.y + 0, vi.z + 0));
  rv[1] = getVert(Indices(vi.x + 1, vi.y + 0, vi.z + 0));
  rv[2] = getVert(Indices(vi.x + 1, vi.y + 1, vi.z + 0));
  rv[3] = getVert(Indices(vi.x + 0, vi.y + 1, vi.z + 0));
  rv[4] = getVert(Indices(vi.x + 0, vi.y + 0, vi.z + 1));
  rv[5] = getVert(Indices(vi.x + 1, vi.y + 0, vi.z + 1));
  rv[6] = getVert(Indices(vi.x + 1, vi.y + 1, vi.z + 1));
  rv[7] = getVert(Indices(vi.x + 0, vi.y + 1, vi.z + 1));
  apf::ModelEntity* me = getModelEntity(Indices(1,1,1));
  buildRegion(rv, me);
}

void BoxBuilder::buildCell(int i, int d)
{
  switch (d) {
    case 0:
      buildCellVert(i);
      return;
    case 1:
      buildCellEdges(i);
      return;
    case 2:
      buildCellFaces(i);
      return;
    case 3:
      buildCellRegion(i);
      return;
  };
}

void BoxBuilder::buildDimension(int d)
{
  for (int i = 0; i < grid.total(); ++i)
    buildCell(i, d);
}

void BoxBuilder::buildMeshAndModel()
{
  for (int d = 0; d <= dim; ++d)
    buildDimension(d);
  m->acceptChanges();
}

void BoxBuilder::buildSets(apf::StkModels& sets)
{ /* arrange these however you expect
     the NodeSets and SideSets to be ordered */
  Indices const faceTable[6] = {
    Indices(0,1,1),
    Indices(2,1,1),
    Indices(1,0,1),
    Indices(1,2,1),
    Indices(1,1,0),
    Indices(1,1,2),
  };
  int dims[2] = {0, dim - 1};
  char const* names[2] = {"NodeSet", "SideSet"};
  for (int i = 0; i < 2; ++i) {
    sets.models[dims[i]].resize(6);
    for (int j = 0; j < 6; ++j) {
      Indices mi = faceTable[j];
      for (int k = dim; k < 3; ++k)
        mi[k] = 0;
      int mj = mgrid.in(mi);
      apf::StkModel* set = new apf::StkModel();
      int mdim = modelTable[mj].dim;
      int mtag = modelTable[mj].tag;
      apf::ModelEntity* me = m->findModelEntity(mdim, mtag);
      set->ents.push_back(me);
      std::stringstream ss;
      ss << names[i] << j;
      set->stkName = ss.str();
      sets.models[dims[i]][j] = set;
    }
  }
  sets.models[dim].push_back(new apf::StkModel());
  sets.models[dim][0]->ents.push_back(m->findModelEntity(dim, 0));
  sets.models[dim][0]->stkName = "Block0";
}

void PUMIMeshStruct::buildBoxMesh(
    int nex, int ney, int nez,
    double wx, double wy, double wz, bool is)
{
  BoxBuilder bb(nex, ney, nez, wx, wy, wz, is);
  mesh = bb.m;
  bb.buildSets(sets);
}

}
