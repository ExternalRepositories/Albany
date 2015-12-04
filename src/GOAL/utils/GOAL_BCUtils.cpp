//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "GOAL_BCUtils.hpp"
#include "Albany_Application.hpp"
#include "Albany_GOALDiscretization.hpp"
#include "GOAL_MechanicsProblem.hpp"

#include "RTC_FunctionRTC.hh"

using Teuchos::RCP;
using Teuchos::ArrayRCP;
using Teuchos::ParameterList;

namespace GOAL {

class BCManager
{
  public:
    BCManager(
        const double time,
        Albany::Application const& application);
    const double t;
    Albany::Application const& app;
    RCP<const Tpetra_Vector> sol;
    RCP<Tpetra_Vector> res;
    RCP<Tpetra_Vector> qoi;
    RCP<Tpetra_CrsMatrix> jac;
    RCP<Tpetra_CrsMatrix> jacT;
    void run();
  private:
    bool isAdjoint;
    RCP<Albany::GOALDiscretization> disc;
    RCP<Albany::GOALMeshStruct> meshStruct;
    RCP<Albany::GOALMechanicsProblem> problem;
    Albany::GOALNodeSets ns;
    ParameterList bcParams;
    void applyBC(ParameterList const& p);
    void modifyPrimalSystem(double v, int offset, std::string set);
    void modifyAdjointSystem(double v, int offset, std::string set);
};

BCManager::BCManager(
    const double time,
    Albany::Application const& application) :
  t(time),
  app(application)
{
  RCP<const ParameterList> pl = app.getProblemPL();
  bcParams = pl->sublist("Hierarchic Boundary Conditions");
  RCP<Albany::AbstractDiscretization> ad = app.getDiscretization();
  this->disc = Teuchos::rcp_dynamic_cast<Albany::GOALDiscretization>(ad);
  this->meshStruct = disc->getGOALMeshStruct();
  RCP<Albany::AbstractProblem> ap = app.getProblem();
  this->problem = Teuchos::rcp_dynamic_cast<Albany::GOALMechanicsProblem>(ap);
  this->ns = disc->getGOALNodeSets();
}

void BCManager::run()
{
  if (jacT == Teuchos::null) isAdjoint = false;
  typedef ParameterList::ConstIterator ParamIter;
  for (ParamIter i = bcParams.begin(); i != bcParams.end(); ++i)
  {
    std::string const& name = bcParams.name(i);
    Teuchos::ParameterEntry const& entry = bcParams.entry(i);
    assert(entry.isList());
    applyBC(Teuchos::getValue<ParameterList>(entry));
  }
}

static RCP<ParameterList> getValidBCParameters()
{
  RCP<ParameterList> p = rcp(new ParameterList("Valid Hierarchic BC Params"));
  p->set<std::string>("DOF", "", "Degree of freedom to which BC is applied");
  p->set<std::string>("Value", "", "Value of the BC as function of t");
  p->set<std::string>("Node Set", "", "Node Set to apply the BC to");
  return p;
}

static double parseExpression(std::string const& val, const double t)
{
  bool success;
  PG_RuntimeCompiler::Function f;
  f.addVar("double", "t");
  f.addVar("double", "value");
  success = f.addBody(val); assert(success);
  success = f.varValueFill(0, t); assert(success);
  success = f.varValueFill(1, 0); assert(success);
  success = f.execute(); assert(success);
  return f.getValueOfVar("value");
}

void BCManager::applyBC(ParameterList const& p)
{
  // validate parameters
  RCP<ParameterList> vp = getValidBCParameters();
  p.validateParameters(*vp,0);

  // get the input parameters
  std::string val = p.get<std::string>("Value");
  std::string set = p.get<std::string>("Node Set");
  std::string dof = p.get<std::string>("DOF");

  // if this is the adjoint problem, set the value to 0
  // otherwise parse the expression from the input file to get the value
  double v = 0.0;
  if (!isAdjoint)
    v = parseExpression(val, t);

  // does this node set actually exist?
  assert(ns.count(set) == 1);

  // does this dof actually exist?
  int offset = problem->getOffset(dof);

  if (!isAdjoint)
    modifyPrimalSystem(v, offset, set);
  else
    modifyAdjointSystem(v, offset, set);
}

void BCManager::modifyPrimalSystem(double v, int offset, std::string set)
{
  // should we fill in BC info?
  bool fillRes = (res != Teuchos::null);
  bool fillJac = (jac != Teuchos::null);
  if ((!fillRes) && (!fillJac)) return;

  // get views of the solution and residual vectors
  ArrayRCP<const ST> x_const_view;
  ArrayRCP<ST> f_nonconst_view;
  if (fillRes) {
    x_const_view = sol->get1dView();
    f_nonconst_view = res->get1dViewNonConst();
  }

  // set up some data for replacing jacobian values
  Teuchos::Array<LO> index(1);
  Teuchos::Array<ST> value(1);
  value[0] = 1.0;
  size_t numEntries;
  Teuchos::Array<ST> matrixEntries;
  Teuchos::Array<LO> matrixIndices;

  // loop over all of the nodes in this node set
  std::vector<Albany::GOALNode> nodes = ns[set];
  for (int i=0; i < nodes.size(); ++i)
  {
    Albany::GOALNode node = nodes[i];
    int lunk = disc->getDOF(node.lid, offset);

    // if the node is higher order, we set the value of the DBC to be 0
    // note: this assumes that bcs are either constant or linear in space
    // anything else would require a linear solve to find coefficients v
    if (node.higherOrder)
      v = 0.0;

    // modify the residual if necessary
    if (fillRes)
      f_nonconst_view[lunk] = x_const_view[lunk] - v;

    // modify the jacobian if necessary
    if (fillJac)
    {
      index[0] = lunk;
      numEntries = jac->getNumEntriesInLocalRow(lunk);
      matrixEntries.resize(numEntries);
      matrixIndices.resize(numEntries);
      jac->getLocalRowCopy(lunk, matrixIndices(), matrixEntries(), numEntries);
      for (int i=0; i < numEntries; ++i)
        matrixEntries[i] = 0.0;
      jac->replaceLocalValues(lunk, matrixIndices(), matrixEntries());
      jac->replaceLocalValues(lunk, index(), value());
    }
  }
}

void BCManager::modifyAdjointSystem(double v, int offset, std::string set)
{

  // get views of the qoi vector
  ArrayRCP<ST> qoi_nonconst_view = qoi->get1dViewNonConst();

  // set up some data for replacing jacobian values
  Teuchos::Array<LO> index(1);
  Teuchos::Array<ST> value(1);
  value[0] = 1.0;
  size_t numEntries;
  Teuchos::Array<ST> matrixEntries;
  Teuchos::Array<LO> matrixIndices;

  // loop over all of the nodes in this node set
  std::vector<Albany::GOALNode> nodes = ns[set];
  for (int i=0; i < nodes.size(); ++i)
  {
    Albany::GOALNode node = nodes[i];
    int lunk = disc->getDOF(node.lid, offset);

    // modify the qoi vector
    qoi_nonconst_view[lunk] = 0.0;

    // modify the transpose of the jacobian
    index[0] = lunk;
    numEntries = jacT->getNumEntriesInLocalRow(lunk);
    matrixEntries.resize(numEntries);
    matrixIndices.resize(numEntries);
    jacT->getLocalRowCopy(lunk, matrixIndices(), matrixEntries(), numEntries);
    for (int i=0; i < numEntries; ++i)
      matrixEntries[i] = 0.0;
    jacT->replaceLocalValues(lunk, matrixIndices(), matrixEntries());
    jacT->replaceLocalValues(lunk, index(), value());
  }
}

void computeHierarchicBCs(
    const double time,
    Albany::Application const& app,
    Teuchos::RCP<const Tpetra_Vector> const& sol,
    Teuchos::RCP<Tpetra_Vector> const& res,
    Teuchos::RCP<Tpetra_CrsMatrix> const& jac)
{
  RCP<const ParameterList> pl = app.getProblemPL();
  std::string name = pl->get<std::string>("Name");
  if (name.find("GOAL") != 0)
    return;
  BCManager bcm(time, app);
  bcm.sol = sol;
  bcm.res = res;
  bcm.jac = jac;
  bcm.run();
}

void computeAdjointHierarchicBCs(
    const double time,
    Albany::Application const& app,
    Teuchos::RCP<Tpetra_Vector> const& qoi,
    Teuchos::RCP<Tpetra_CrsMatrix> const& jacT)
{
  RCP<const ParameterList> pl = app.getProblemPL();
  std::string name = pl->get<std::string>("Name");
  if (name.find("GOAL") != 0)
    return;
  BCManager bcm(time, app);
  bcm.qoi = qoi;
  bcm.jacT = jacT;
  bcm.run();
}

}
