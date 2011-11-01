/********************************************************************\
*            Albany, Copyright (2010) Sandia Corporation             *
 *                                                                    *
 * Notice: This computer software was prepared by Sandia Corporation, *
 * hereinafter the Contractor, under Contract DE-AC04-94AL85000 with  *
 * the Department of Energy (DOE). All rights in the computer software*
 * are reserved by DOE on behalf of the United States Government and  *
 * the Contractor as provided in the Contract. You are authorized to  *
 * use this computer software for Governmental purposes but it is not *
 * to be released or distributed to the public. NEITHER THE GOVERNMENT*
 * NOR THE CONTRACTOR MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR      *
 * ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE. This notice    *
 * including this sentence must appear on any copies of this software.*
 *    Questions to Andy Salinger, agsalin@sandia.gov                  *
 \********************************************************************/
#include "Albany_StateManager.hpp"
#include "Albany_Utils.hpp"
#include "Teuchos_VerboseObject.hpp"
#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout_MDALayout.hpp"

Albany::StateManager::StateManager() :
  stateVarsAreAllocated(false),
  stateInfo(Teuchos::rcp(new StateInfoStruct)),
  time(0.0),
  timeOld(0.0)
{
}

Teuchos::RCP<Teuchos::ParameterList>
Albany::StateManager::registerStateVariable(const std::string &name, const Teuchos::RCP<PHX::DataLayout> &dl,
                                            const Teuchos::RCP<PHX::DataLayout> &dummy,
                                            const std::string &init_type,
                                            const bool registerOldState)
{
  return registerStateVariable(name, dl, dummy, init_type, registerOldState, name);
}

Teuchos::RCP<Teuchos::ParameterList>
Albany::StateManager::registerStateVariable(const std::string &stateName, const Teuchos::RCP<PHX::DataLayout> &dl,
                                            const Teuchos::RCP<PHX::DataLayout> &dummy,
                                            const std::string &init_type,
                                            const bool registerOldState,
                                            const std::string& fieldName)
{
  registerStateVariable(stateName, dl, init_type, registerOldState);

  // Create param list for SaveStateField evaluator 
  Teuchos::RCP<Teuchos::ParameterList> p = Teuchos::rcp(new Teuchos::ParameterList("Save or Load State " 
							  + stateName + " to/from field " + fieldName));
  p->set<const std::string>("State Name", stateName);
  p->set<const std::string>("Field Name", fieldName);
  p->set<const Teuchos::RCP<PHX::DataLayout> >("State Field Layout", dl);
  p->set<const Teuchos::RCP<PHX::DataLayout> >("Dummy Data Layout", dummy);
  return p;
}


void
Albany::StateManager::registerStateVariable(const std::string &stateName, 
					    const Teuchos::RCP<PHX::DataLayout> &dl,
                                            const std::string &init_type,
                                            const bool registerOldState,
					    const bool outputToExodus)

{
  TEUCHOS_TEST_FOR_EXCEPT(stateVarsAreAllocated);
  statesToStore[stateName] = dl;

  

  // Load into StateInfo
  (*stateInfo).push_back(Teuchos::rcp(new Albany::StateStruct(stateName)));
  Albany::StateStruct& stateRef = *stateInfo->back();
  stateRef.initType = init_type; 
  if ( dl->rank() > 1 )
    stateRef.entity = dl->name(1); //Tag, should be Node or QuadPoint
  else if ( dl->rank() == 1 )
    stateRef.entity = "ScalarValue";
  stateRef.output = outputToExodus;
  dl->dimensions(stateRef.dim); 

  // If space is needed for old state
  if (registerOldState) {
    stateRef.saveOldState = true; 

    std::string stateName_old = stateName + "_old";
    (*stateInfo).push_back(Teuchos::rcp(new Albany::StateStruct(stateName_old)));
    Albany::StateStruct& pstateRef = *stateInfo->back();
    pstateRef.initType = init_type; 
    if ( dl->rank() > 1 )
      pstateRef.entity = dl->name(1); //Tag, should be Node or QuadPoint
    else if ( dl->rank() == 1 )
      pstateRef.entity = "ScalarValue";
    pstateRef.output = false; 
    dl->dimensions(pstateRef.dim); 
  }
}

Teuchos::RCP<Albany::StateInfoStruct>
Albany::StateManager::getStateInfoStruct()
{
  return stateInfo;
}

void
Albany::StateManager::setStateArrays(const Teuchos::RCP<Albany::AbstractDiscretization>& disc_)
{
  TEUCHOS_TEST_FOR_EXCEPT(stateVarsAreAllocated);
  stateVarsAreAllocated = true;
  Teuchos::RCP<Teuchos::FancyOStream> out(Teuchos::VerboseObjectBase::getDefaultOStream());

  disc = disc_;

  // Get states from STK mesh 
  Albany::StateArrays& sa = disc->getStateArrays();

  int numWorksets = sa.size();

  // For each workset, loop over registered states

  for (unsigned int i=0; i<stateInfo->size(); i++) {
    const std::string stateName = (*stateInfo)[i]->name;
    const std::string init_type = (*stateInfo)[i]->initType;
  
    *out << "StateManager: initializing state:  " << stateName;
    if (init_type == "zero")
          *out << " with initialization type 'zero'" << std::endl;
    else if (init_type == "identity")
          *out << " with initialization type 'identity'" << std::endl;

    for (int ws = 0; ws < numWorksets; ws++)
    {
      std::vector<int> dims;
      sa[ws][stateName].dimensions(dims);
      int size = dims.size();

      if (init_type == "zero")
      {
        switch (size) {
	case 1:
	  sa[ws][stateName](0) = 0.0;
	  break;
	case 2:
	  for (int cell = 0; cell < dims[0]; ++cell)
	    for (int qp = 0; qp < dims[1]; ++qp)
	      sa[ws][stateName](cell, qp) = 0.0;
	  break;
	case 3:
	  for (int cell = 0; cell < dims[0]; ++cell)
	    for (int qp = 0; qp < dims[1]; ++qp)
	      for (int i = 0; i < dims[2]; ++i)
		sa[ws][stateName](cell, qp, i) = 0.0;
	  break;
	case 4:
	  for (int cell = 0; cell < dims[0]; ++cell)
	    for (int qp = 0; qp < dims[1]; ++qp)
	      for (int i = 0; i < dims[2]; ++i)
		for (int j = 0; j < dims[3]; ++j)
		  sa[ws][stateName](cell, qp, i, j) = 0.0;
	  break;
	default:
	  TEUCHOS_TEST_FOR_EXCEPTION(size<2||size>4, std::logic_error,
				     "Something is wrong during zero state variable initialization: " << size);
        }

      }
      else if (init_type == "identity")
      {
        // we assume operating on the last two indices is correct
        TEUCHOS_TEST_FOR_EXCEPTION(size != 4, std::logic_error,
				   "Something is wrong during identity state variable initialization: " << size);
        TEUCHOS_TEST_FOR_EXCEPT( ! (dims[2] == dims[3]) );

        for (int cell = 0; cell < dims[0]; ++cell)
          for (int qp = 0; qp < dims[1]; ++qp)
            for (int i = 0; i < dims[2]; ++i)
              for (int j = 0; j < dims[3]; ++j)
                if (i==j) sa[ws][stateName](cell, qp, i, i) = 1.0;
                else      sa[ws][stateName](cell, qp, i, j) = 0.0;
      }
    }
  }
  *out << std::endl;
}

void
Albany::StateManager::
importStateData(Albany::StateArrays& statesToCopyFrom)
{
  TEUCHOS_TEST_FOR_EXCEPT(!stateVarsAreAllocated);

  // Get states from STK mesh 
  Albany::StateArrays& sa = getStateArrays();
  int numWorksets = sa.size();

  TEUCHOS_TEST_FOR_EXCEPT((unsigned int)numWorksets != statesToCopyFrom.size());

  Teuchos::RCP<Teuchos::FancyOStream> out(Teuchos::VerboseObjectBase::getDefaultOStream());
  *out << std::endl;

  for (unsigned int i=0; i<stateInfo->size(); i++) {
    const std::string stateName = (*stateInfo)[i]->name;

    //check if state exists in statesToCopyFrom (check first workset only)
    if( statesToCopyFrom[0].find(stateName) == statesToCopyFrom[0].end() ) {
      //*out << "StateManager: state " << stateName << " not present, so not filled" << std::endl;
      continue;
    }

    *out << "StateManager: filling state:  " << stateName << std::endl;
    for (int ws = 0; ws < numWorksets; ws++)
    {
      std::vector<int> dims;
      sa[ws][stateName].dimensions(dims);
      int size = dims.size();

      switch (size) {
      case 1:
	sa[ws][stateName](0) = statesToCopyFrom[ws][stateName](0);
	break;
      case 2:
	for (int cell = 0; cell < dims[0]; ++cell)
	  for (int qp = 0; qp < dims[1]; ++qp)
	    sa[ws][stateName](cell, qp) = statesToCopyFrom[ws][stateName](cell, qp);
	break;
      case 3:
	for (int cell = 0; cell < dims[0]; ++cell)
	  for (int qp = 0; qp < dims[1]; ++qp)
	    for (int i = 0; i < dims[2]; ++i)
	      sa[ws][stateName](cell, qp, i) = statesToCopyFrom[ws][stateName](cell, qp, i);
	break;
      case 4:
	for (int cell = 0; cell < dims[0]; ++cell)
	  for (int qp = 0; qp < dims[1]; ++qp)
	    for (int i = 0; i < dims[2]; ++i)
	      for (int j = 0; j < dims[3]; ++j)
		sa[ws][stateName](cell, qp, i, j) = statesToCopyFrom[ws][stateName](cell, qp, i, j);
	break;
      default:
	TEUCHOS_TEST_FOR_EXCEPTION(size<2||size>4, std::logic_error,
				   "Something is wrong during zero state variable fill: " << size);
      }
    }
  }

  *out << std::endl;
}

Albany::StateArray&
Albany::StateManager::getStateArray(const int ws) const
{
  TEUCHOS_TEST_FOR_EXCEPT(!stateVarsAreAllocated);
  return disc->getStateArrays()[ws];
}

Albany::StateArrays&
Albany::StateManager::getStateArrays() const
{
  TEUCHOS_TEST_FOR_EXCEPT(!stateVarsAreAllocated);
  return disc->getStateArrays();
}

void
Albany::StateManager::updateStates()
{
  // Swap boolean that defines old and new (in terms of state1 and 2) in accessors
  TEUCHOS_TEST_FOR_EXCEPT(!stateVarsAreAllocated);

  // Get states from STK mesh 
  Albany::StateArrays& sa = disc->getStateArrays();
  int numWorksets = sa.size();

  // For each workset, loop over registered states

  for (unsigned int i=0; i<stateInfo->size(); i++) {
    if ((*stateInfo)[i]->saveOldState) {
      const std::string stateName = (*stateInfo)[i]->name;
      const std::string stateName_old = stateName + "_old";
  
      for (int ws = 0; ws < numWorksets; ws++)
        for (int j = 0; j < sa[ws][stateName].size(); j++)
          sa[ws][stateName_old][j] = sa[ws][stateName][j];
    }
  }
}

Teuchos::RCP<Albany::EigendataStruct> 
Albany::StateManager::getEigenData()
{
  return eigenData;
}

void 
Albany::StateManager::setEigenData(const Teuchos::RCP<Albany::EigendataStruct>& eigdata)
{
  eigenData = eigdata;
}
