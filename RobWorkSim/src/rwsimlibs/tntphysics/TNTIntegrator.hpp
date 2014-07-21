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

#ifndef RWSIMLIBS_TNTPHYSICS_TNTINTEGRATOR_HPP_
#define RWSIMLIBS_TNTPHYSICS_TNTINTEGRATOR_HPP_

/**
 * @file TNTIntegrator.hpp
 *
 * \copydoc rwsimlibs::tntphysics::TNTIntegrator
 */

#include <rw/common/ExtensionPoint.hpp>
#include <rw/math/Wrench6D.hpp>

#include "TNTRigidBody.hpp"

#include <list>

// Forward declarations
namespace rw { namespace kinematics { class State; } }

namespace rwsimlibs {
namespace tntphysics {

// Forward declarations
class TNTConstraint;

//! @addtogroup rwsimlibs_tntphysics

//! @{
/**
 * @brief Interface for different motion integrators for rigid bodies.
 *
 * The integrators are responsible for doing the actual integration, but also
 * for providing a linear model relating the applied forces and torques to the motion
 * of a point on the object.
 */
class TNTIntegrator {
public:
	//! @brief Default constructor for empty integrator.
	TNTIntegrator();

	//! @brief Destructor
	virtual ~TNTIntegrator();

	/**
	 * @brief Get the body that this integrator is associated to.
	 * @return a pointer to the rigid body.
	 */
	const TNTRigidBody* getBody() const;

	/**
	 * @brief Create a new integrator for the given body.
	 * @param body [in] the body to create the integrator for.
	 * @return a pointer to a new TNTIntegrator - the pointer is owned by the caller.
	 */
	virtual const TNTIntegrator* makeIntegrator(const TNTRigidBody* body) const = 0;

	/**
	 * @brief Reset the integrator to a given position and velocity.
	 * @param state [in] the state to reset the integrator to.
	 */
	virtual TNTRigidBody::RigidConfiguration* getConfiguration(const rw::kinematics::State &state) const;

	/**
	 * @brief Integrate the motion with the given constraints.
	 * @param constraints [in] a list of the constraints acting on the body.
	 * @param gravity [in] the gravity in world coordinates.
	 * @param stepsize [in] the size of the step to integrate.
	 * @param configuration [in/out] the configuration to update.
	 * @param state [in/out] the state with the current constraint forces, position and velocities.
	 * @param rwstate [in] the current state.
	 */
	virtual void integrate(const std::list<const TNTConstraint*> &constraints, const rw::math::Vector3D<>& gravity, double stepsize, TNTRigidBody::RigidConfiguration &configuration, TNTIslandState &state, const rw::kinematics::State &rwstate) const;

	/**
	 * @brief Integrate the motion with the given net force and torque acting on the object.
	 * @param netFT [in] the net force and torque acting in the center of mass of the body, given in world frame.
	 * @param stepsize [in] the time to integrate.
	 * @param configuration [in/out] the configuration to update.
	 */
	virtual void integrate(const rw::math::Wrench6D<> &netFT, double stepsize, TNTRigidBody::RigidConfiguration &configuration) const = 0;

	/**
	 * @brief Get the socalled independent motion of the point, which is not related to a constraint wrench.
	 * @param point [in] the point to find contribution for.
	 * @param stepsize [in] the size of the step to solve for.
	 * @param configuration [in] the current configuration.
	 * @param configurationGuess [in] for iterative linearization.
	 * @param Fext [in] an extra external force not related to the constraints or gravity.
	 * @param Next [in] an extra external torque not related to the constraints.
	 * @return a vector of size 6.
	 */
	virtual Eigen::Matrix<double, 6, 1> eqPointVelIndependent(const rw::math::Vector3D<> point, double stepsize, const TNTRigidBody::RigidConfiguration &configuration, const TNTRigidBody::RigidConfiguration &configurationGuess, const rw::math::Vector3D<>& Fext, const rw::math::Vector3D<>& Next) const = 0;

	/**
	 * @brief Get the dependent motion of the point, which is dependent on the applied constraint wrench.
	 * @param point [in] get the contribution for the constraint in this point.
	 * @param stepsize [in] the size of the step to solve for.
	 * @param constraintPos [in] get the contribution of this other constraint to the main constraint.
	 * @param configuration [in] the current configuration.
	 * @param configurationGuess [in] for iterative linearization.
	 * @return a matrix block of size 6 times 6.
	 */
	virtual Eigen::Matrix<double, 6, 6> eqPointVelConstraintWrenchFactor(const rw::math::Vector3D<> point, double stepsize, const rw::math::Vector3D<> constraintPos, const TNTRigidBody::RigidConfiguration &configuration, const TNTRigidBody::RigidConfiguration &configurationGuess) const = 0;

	/**
	 * @brief Check if the linear model is an approximation.
	 *
	 * If the model is an approximation to a non-linear model,
	 * the simulator should know this and solve iteratively.
	 *
	 * @return true is approximation, false otherwise
	 */
	virtual bool eqIsApproximation() const = 0;


	/**
	 * @addtogroup extensionpoints
	 * @extensionpoint{rwsimlibs::tntphysics::TNTIntegrator::Factory,rwsimlibs::tntphysics::TNTIntegrator,rwsimlibs.tntphysics.TNTIntegrator}
	 */

	/**
	 * @brief A factory for a TNTIntegrator. This factory also defines an
	 * extension point for TNTIntegrator.
	 *
	 * By default the factory provides the following TNTIntegrator types:
	 *  - Euler - TNTEulerIntegrator
	 */
    class Factory: public rw::common::ExtensionPoint<TNTIntegrator> {
    public:
    	/**
    	 * @brief Get the available integrator types.
    	 * @return a vector of identifiers for integrators.
    	 */
    	static std::vector<std::string> getIntegrators();

    	/**
    	 * @brief Check if integrator type is available.
    	 * @param integratorType [in] the name of the integrator.
    	 * @return true if available, false otherwise.
    	 */
    	static bool hasIntegrator(const std::string& integratorType);

    	/**
    	 * @brief Create a new integrator.
    	 * @param integratorType [in] the name of the integrator.
    	 * @param body [in] the body to create the integrator for.
    	 * @return a pointer to a new TNTIntegrator - the pointer is owned by the caller.
    	 */
    	static const TNTIntegrator* makeIntegrator(const std::string& integratorType, const TNTRigidBody* body);

    private:
        Factory();
    };

protected:
    /**
     * @brief Create a integrator associated to a given body.
     * @param body [in] the body to associate to.
     */
	TNTIntegrator(const TNTRigidBody* body);

private:
	const TNTRigidBody* const _body;
};
//! @}
} /* namespace tntphysics */
} /* namespace rwsimlibs */
#endif /* RWSIMLIBS_TNTPHYSICS_TNTINTEGRATOR_HPP_ */