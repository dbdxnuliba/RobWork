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

#include "TNTIntegratorEuler.hpp"

#include <rwsim/dynamics/RigidBody.hpp>

using namespace rw::kinematics;
using namespace rw::math;
using namespace rwsim::dynamics;
using namespace rwsimlibs::tntphysics;

TNTIntegratorEuler::TNTIntegratorEuler():
	TNTIntegrator(NULL)
{
}

TNTIntegratorEuler::TNTIntegratorEuler(const TNTRigidBody* body):
	TNTIntegrator(body)
{
}

TNTIntegratorEuler::~TNTIntegratorEuler() {
}

const TNTIntegrator* TNTIntegratorEuler::makeIntegrator(const TNTRigidBody* body) const {
	return new TNTIntegratorEuler(body);
}

void TNTIntegratorEuler::integrate(const Wrench6D<> &netFT, double h, TNTRigidBody::RigidConfiguration &configuration) const {
	if (getBody() == NULL)
		RW_THROW("TNTEulerIntegrator (integrate): There is no body set for this integrator - please construct a new integrator for the specific body to use.");
	RW_THROW("TNTEulerIntegrator (integrate): Should not be used!");
	const rw::common::Ptr<const RigidBody> rwbody = getBody()->getRigidBody();
	const Transform3D<>& wTb = configuration.getWorldTcom();
	const Vector3D<>& R = wTb.P();
	const VelocityScrew6D<>& vel = configuration.getVelocity();
	const InertiaMatrix<>& inertia = wTb.R()*rwbody->getBodyInertia()*inverse(wTb.R());
	const InertiaMatrix<>& inertiaInv = wTb.R()*rwbody->getBodyInertiaInv()*inverse(wTb.R());
	const double massInv = rwbody->getMassInv();
	const Vector3D<> velAng = vel.angular().axis()*vel.angular().angle();
	const Vector3D<> angVel = velAng + inertiaInv*(-cross(velAng,inertia*velAng)+netFT.torque())*h;
	const Vector3D<> linVel = vel.linear() + h*massInv*netFT.force();
	configuration.setVelocity(VelocityScrew6D<>(linVel,EAA<>(angVel)));
	const Vector3D<> linPos = R+h*linVel;
	const Rotation3D<> angPos = EAA<>(h*angVel).toRotation3D()*wTb.R();
	configuration.setWorldTcom(Transform3D<>(linPos,angPos));
}

void TNTIntegratorEuler::positionUpdate(const Wrench6D<> &netFT, double h, TNTRigidBody::RigidConfiguration &configuration) const {
	if (getBody() == NULL)
		RW_THROW("TNTEulerIntegrator (positionUpdate): There is no body set for this integrator - please construct a new integrator for the specific body to use.");
	// Extract required info
	const rw::common::Ptr<const RigidBody> rwbody = getBody()->getRigidBody();
	const Transform3D<>& wTb = configuration.getWorldTcom();
	const Vector3D<>& R = wTb.P();
	const VelocityScrew6D<>& vel = configuration.getVelocity();
	const Vector3D<> velAng(vel[3],vel[4],vel[5]);// = vel.angular().axis()*vel.angular().angle();

	// Find new position and orientation
	const Vector3D<> linPos = R+h*vel.linear();
	const Rotation3D<> angPos = EAA<>(h*velAng).toRotation3D()*wTb.R();
	configuration.setWorldTcom(Transform3D<>(linPos,angPos));
}

void TNTIntegratorEuler::velocityUpdate(const Wrench6D<> &netFTcur, const Wrench6D<> &netFTnext, double h, const TNTRigidBody::RigidConfiguration &configuration0, TNTRigidBody::RigidConfiguration &configurationH) const {
	if (getBody() == NULL)
		RW_THROW("TNTEulerIntegrator (velocityUpdate): There is no body set for this integrator - please construct a new integrator for the specific body to use.");
	// Extract required info
	const rw::common::Ptr<const RigidBody> rwbody = getBody()->getRigidBody();
	const Transform3D<>& wTb = configuration0.getWorldTcom();
	const VelocityScrew6D<>& vel = configuration0.getVelocity();
	const InertiaMatrix<>& inertia = wTb.R()*rwbody->getBodyInertia()*inverse(wTb.R());
	const InertiaMatrix<>& inertiaInv = wTb.R()*rwbody->getBodyInertiaInv()*inverse(wTb.R());
	const double massInv = rwbody->getMassInv();
	const Vector3D<> velAng(vel[3],vel[4],vel[5]);// = vel.angular().axis()*vel.angular().angle();

	// Find new velocity
	const Vector3D<> angVel = velAng + inertiaInv*(-cross(velAng,inertia*velAng)+netFTnext.torque())*h;
	const Vector3D<> linVel = vel.linear() + h*massInv*netFTnext.force();
	configurationH.setVelocity(VelocityScrew6D<>(linVel,EAA<>(angVel)));
}

Eigen::Matrix<double, 6, 1> TNTIntegratorEuler::eqPointVelIndependent(const Vector3D<> point, double h, const TNTRigidBody::RigidConfiguration &configuration0, const TNTRigidBody::RigidConfiguration &configurationH, const Vector3D<>& Ftot0, const Vector3D<>& Ntot0, const Vector3D<>& FextH, const Vector3D<>& NextH) const {
	if (getBody() == NULL)
		RW_THROW("TNTEulerIntegrator (eqPointVelIndependent): There is no body set for this integrator - please construct a new integrator for the specific body to use.");
	const rw::common::Ptr<const RigidBody> rwbody = getBody()->getRigidBody();
	const Transform3D<>& wTb = configurationH.getWorldTcom();
	const Vector3D<>& R = wTb.P();
	const VelocityScrew6D<>& vel = configurationH.getVelocity();
	const InertiaMatrix<>& inertia = wTb.R()*rwbody->getBodyInertia()*inverse(wTb.R());
	const InertiaMatrix<>& inertiaInv = wTb.R()*rwbody->getBodyInertiaInv()*inverse(wTb.R());
	const double massInv = rwbody->getMassInv();
	const Vector3D<> velAng = vel.angular().axis()*vel.angular().angle();
	const Vector3D<> angVel = velAng+inertiaInv*(-cross(velAng,inertia*velAng)+NextH)*h;
	const Vector3D<> linVel = vel.linear() + h*massInv*FextH + cross(angVel,point-R);
	Eigen::Matrix<double, 6, 1> a;
	a << linVel.e(), angVel.e();
	return a;
}

Eigen::Matrix<double, 6, 6> TNTIntegratorEuler::eqPointVelConstraintWrenchFactor(const Vector3D<> point, double h, const Vector3D<> constraintPos, const TNTRigidBody::RigidConfiguration &configuration, const TNTRigidBody::RigidConfiguration &configurationGuess) const {
	if (getBody() == NULL)
		RW_THROW("TNTEulerIntegrator (eqPointVelConstraintWrenchFactor): There is no body set for this integrator - please construct a new integrator for the specific body to use.");
	const rw::common::Ptr<const RigidBody> rwbody = getBody()->getRigidBody();
	const Transform3D<>& wTb = configuration.getWorldTcom();
	const Vector3D<>& R = wTb.P();
	const Eigen::Matrix3d inertiaInv = (wTb.R()*rwbody->getBodyInertiaInv()*inverse(wTb.R())).e();
	const double massInv = rwbody->getMassInv();
	const Eigen::Matrix3d BLinTorque = -h*Math::skew(point-R)*inertiaInv;
	const Eigen::Matrix3d BLinForce = BLinTorque*Math::skew(constraintPos-R)+h*massInv*Rotation3D<>::identity().e();
	const Eigen::Matrix3d BAngTorque = h*inertiaInv;
	const Eigen::Matrix3d BAngForce = BAngTorque*Math::skew(constraintPos-R);
	Eigen::Matrix<double, 6, 6> B;
	B <<	BLinForce, BLinTorque,
			BAngForce, BAngTorque;
	return B;
}

bool TNTIntegratorEuler::eqIsApproximation() const {
	return false;
}

const TNTIntegrator* TNTIntegratorEuler::getDiscontinuityIntegrator() const {
	return this;
}
