/********************************************************************************
 * Copyright 2014 The Robotics Group, The Maersk Mc-Kinney Moller Institute,
 * Faculty of Engineering, University of Southern Denmark
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ********************************************************************************/

#ifndef RWSIMLIBS_TNTPHYSICS_TNTFRICTIONMODEL_HPP_
#define RWSIMLIBS_TNTPHYSICS_TNTFRICTIONMODEL_HPP_

/**
 * @file TNTFrictionModel.hpp
 *
 * \copydoc rwsimlibs::tntphysics::TNTFrictionModel
 */

#include <rw/common/ExtensionPoint.hpp>

// Forward declarations
namespace rw { namespace kinematics { class State; } }

namespace rwsimlibs {
namespace tntphysics {

// Forward declarations
class TNTContact;
class TNTIslandState;

//! @addtogroup rwsimlibs_tntphysics

//! @{
/**
 * @brief A friction model calculates friction coefficients based on the relative velocities.
 */
class TNTFrictionModel {
public:
    //! @brief The possible friction values.
    struct Values {
    	//! @brief Default constructor - no friction used.
    	Values(): enableTangent(false), tangent(0), tangentAbsolute(0), enableAngular(false), angular(0), angularAbsolute(0) {};
    	//! @brief Solve for tangential friction.
    	bool enableTangent;
    	//! @brief Friction coefficient - apply friction proportional to normal force.
    	double tangent;
    	//! @brief Apply friction independent of normal force.
    	double tangentAbsolute;
    	//! @brief Solve for angular friction.
    	bool enableAngular;
    	//! @brief Angular friction coefficient - apply friction proportional to normal force.
    	double angular;
    	//! @brief Apply angular friction independent of normal force.
    	double angularAbsolute;
    };

    //! @brief Constructor.
    TNTFrictionModel() {};

    //! @brief Destructor.
	virtual ~TNTFrictionModel() {};

	/**
	 * @brief Get a model with the given properties.
	 * @param map [in] a map of properties with the required parameters for the model.
	 * @return a friction model with the given properties (can return NULL if properties are not as expected).
	 */
	virtual const TNTFrictionModel* withProperties(const rw::common::PropertyMap &map) const = 0;

	/**
	 * @brief Get the friction coefficients.
	 * @param contact [in] the contact to find coefficient for.
	 * @param tntstate [in] the current state of the system.
	 * @param rwstate [in] the current state of the system.
	 * @return the Values.
	 */
	virtual Values getRestitution(const TNTContact& contact, const TNTIslandState& tntstate, const rw::kinematics::State& rwstate) const = 0;

	/**
	 * @addtogroup extensionpoints
	 * @extensionpoint{rwsimlibs::tntphysics::TNTFrictionModel::Factory,rwsimlibs::tntphysics::TNTFrictionModel,rwsimlibs.tntphysics.TNTFrictionModel}
	 */

	/**
	 * @brief A factory for a TNTFrictionModel. This factory also defines an ExtensionPoint.
	 *
	 * By default the factory provides the following TNTFrictionModel types:
	 *  - None - TNTFrictionModelNone
	 *  - Coulomb - TNTFrictionModelCoulomb
	 */
	class Factory: public rw::common::ExtensionPoint<TNTFrictionModel> {
	public:
		/**
		 * @brief Get the available models.
		 * @return a vector of identifiers for models.
		 */
		static std::vector<std::string> getModels();

		/**
		 * @brief Check if model is available.
		 * @param model [in] the name of the model.
		 * @return true if available, false otherwise.
		 */
		static bool hasModel(const std::string& model);

		/**
		 * @brief Create a new model.
		 * @param model [in] the name of the model.
		 * @param properties [in] a map of properties with the required parameters for the given model.
		 * @return a pointer to a new TNTFrictionModel owned by the caller (can return NULL if properties are not as expected).
		 */
		static const TNTFrictionModel* makeModel(const std::string& model, const rw::common::PropertyMap &properties);

	private:
		Factory();
	};
};
//! @}
} /* namespace tntphysics */
} /* namespace rwsimlibs */
#endif /* RWSIMLIBS_TNTPHYSICS_TNTFRICTIONMODEL_HPP_ */