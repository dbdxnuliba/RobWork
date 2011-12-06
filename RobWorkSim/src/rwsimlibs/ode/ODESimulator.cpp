/********************************************************************************
 * Copyright 2009 The Robotics Group, The Maersk Mc-Kinney Moller Institute,
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

#include "ODESimulator.hpp"

#include <ode/ode.h>

#include <rw/kinematics/Kinematics.hpp>
#include <rw/math/Transform3D.hpp>
#include <rw/math/Vector3D.hpp>
#include <rw/math/Quaternion.hpp>

#include <rw/models/JointDevice.hpp>
#include <rw/models/Joint.hpp>
#include <rw/models/RevoluteJoint.hpp>
#include <rw/models/PrismaticJoint.hpp>

#include <rw/geometry/TriangleUtil.hpp>

#include <rwsim/dynamics/KinematicDevice.hpp>
#include <rwsim/dynamics/RigidDevice.hpp>
#include <rwsim/dynamics/FixedBody.hpp>
#include <rwsim/dynamics/KinematicBody.hpp>
#include <rwsim/dynamics/RigidBody.hpp>
#include <rwsim/dynamics/DynamicUtil.hpp>

#include <boost/foreach.hpp>

#include <rw/kinematics/FramePairMap.hpp>

#include <rw/models/DependentRevoluteJoint.hpp>
#include <rw/models/DependentPrismaticJoint.hpp>
#include <rw/common/TimerUtil.hpp>
#include <rw/geometry/Sphere.hpp>
#include <rw/geometry/Plane.hpp>
#include <rw/geometry/Cylinder.hpp>

#include "ODEKinematicDevice.hpp"
#include "ODEVelocityDevice.hpp"
#include "ODEDebugRender.hpp"
#include "ODEUtil.hpp"
#include "ODESuctionCupDevice.hpp"

#include <rwsim/dynamics/OBRManifold.hpp>

#include <rw/proximity/Proximity.hpp>
#include <rw/proximity/BasicFilterStrategy.hpp>

#include <rwsim/dynamics/ContactPoint.hpp>
#include <rwsim/dynamics/ContactCluster.hpp>
#include <rwsim/dynamics/SuctionCup.hpp>
#include <rwsim/simulator/PhysicsEngineFactory.hpp>
#include <rw/common/Log.hpp>

#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp> // !
#include <boost/lambda/construct.hpp>
#include <boost/function.hpp>

#include <fstream>
#include <iostream>

using namespace rwsim::dynamics;
using namespace rwsim::simulator;
using namespace rwsim::sensor;
using namespace rwsim;

using namespace rw::kinematics;
using namespace rw::geometry;
using namespace rw::models;
using namespace rw::math;
using namespace rw::proximity;
using namespace rw::common;

using namespace rwlibs::simulation;
using namespace rwlibs::proximitystrategies;

#define INITIAL_MAX_CONTACTS 1000

//#define RW_DEBUGS( str ) std::cout << str  << std::endl;
#define RW_DEBUGS( str )
/*
#define TIMING( str, func ) \
    { long start = rw::common::TimerUtil::currentTimeMs(); \
    func; \
     long end = rw::common::TimerUtil::currentTimeMs(); \
    std::cout << str <<":" << (end-start) <<"ms"<<std::endl;  }
*/
#define TIMING( str, func ) {func;}

namespace {

    // register the ODEPhysicsEngine with the factory
    struct InitStruct {
        InitStruct(){
            //std::cout << "************************** INITIALIZE ODE *********************" << std::endl;

            //rwsim::simulator::PhysicsEngineFactory::makePhysicsEngineFunctor odephysics =
            //        boost::lambda::bind( boost::lambda::new_ptr<rwsim::simulator::ODESimulator>(), boost::lambda::_1);

            //rwsim::simulator::PhysicsEngineFactory::addPhysicsEngine("ODE DSfdsfiudif", odephysics);
        }
    };

    //InitStruct initializeStaticStuff;




   Vector3D<> toVector3D(const dReal *v){
        return Vector3D<>(v[0],v[1],v[2]);
    }


	void setODEBodyMass(dMass *m, double mass, Vector3D<> c, InertiaMatrix<> I){
		dReal i11 = I(0,0);
		dReal i22 = I(1,1);
		dReal i33 = I(2,2);
		dReal i12 = I(0,1);
		dReal i13 = I(0,2);
		dReal i23 = I(1,2);
		dMassSetParameters(m, mass,
                           c(0), c(1), c(2),
						   i11, i22, i33,
						   i12, i13, i23);
	}

	std::string printArray(const dReal* arr, int n){
	    std::stringstream str;
	    str << "(";
	    for(int i=0; i<n-1; i++){
	        str << arr[i]<<",";
	    }
	    str << arr[n-1]<<")";
	    return str.str();
	}

	void printMassInfo(const dMass& dmass, const Frame& frame ){
	    std::cout  << "----- Mass properties for frame: " << frame.getName() << std::endl;
	    std::cout  << "- Mass    : " << dmass.mass << std::endl;
	    std::cout  << "- Center  : " << printArray(&(dmass.c[0]), 3) << std::endl;
	    std::cout  << "- Inertia : " << printArray(&dmass.I[0], 3) << std::endl;
	    std::cout  << "-           " << printArray(&dmass.I[3], 3) <<  std::endl;
	    std::cout  << "-           " << printArray(&dmass.I[6], 3) << std::endl;
	    std::cout  << "----------------------------------------------------" << std::endl;
	}

	rw::common::Cache<GeometryData*, ODESimulator::TriMeshData > _cache;

//	ODESimulator::TriMeshData* buildTriMesh(dTriMeshDataID triMeshDataId, const std::vector<Frame*>& frames,
//			rw::kinematics::Frame* parent, const rw::kinematics::State &state, bool invert){

	ODESimulator::TriMeshData::Ptr buildTriMesh(GeometryData::Ptr gdata, const State &state, bool invert = false){
		// check if the geometry is allready in cache
		if( _cache.isInCache(gdata.get()) ){
			ODESimulator::TriMeshData::Ptr tridata = _cache.get(gdata.get());
			return tridata;
		}

        //bool ownedData = false;
        RW_DEBUGS("indexed stuff");
        IndexedTriMesh<float>::Ptr imesh;

		// if not in cache then we need to create a TriMeshData geom,
		// but only if the geomdata is a trianglemesh
		if( !dynamic_cast<TriMesh*>(gdata.get()) ){
		    TriMesh::Ptr mesh = gdata->getTriMesh();
		    imesh = TriangleUtil::toIndexedTriMesh<IndexedTriMeshN0<float> >(*mesh,0.00001);
		    //ownedData = true;
		} else if( !dynamic_cast< IndexedTriMesh<float>* >(gdata.get()) ){
			// convert the trimesh to an indexed trimesh
			RW_DEBUGS("to indexed tri mesh");
			imesh = TriangleUtil::toIndexedTriMesh<IndexedTriMeshN0<float> >(*((TriMesh*)gdata.get()),0.00001);
			//ownedData = true;
		} else {
			imesh = static_cast< IndexedTriMesh<float>* >(gdata.get());
		}
		RW_DEBUGS("done casting");
		int nrOfVerts = imesh->getVertices().size();
		RW_DEBUGS("nr tris");
		int nrOfTris = imesh->getSize();

		// std::cout  << "- NR of faces: " << nrOfTris << std::endl;
		// std::cout  << "- NR of verts: " << nrOfVerts << std::endl;
		RW_DEBUGS("new ode trimesh");
		ODESimulator::TriMeshData *data =
			new ODESimulator::TriMeshData(nrOfTris*3, nrOfVerts*3);
		RW_DEBUGS("trimeshid");
		dTriMeshDataID triMeshDataId = dGeomTriMeshDataCreate();

		//const float myScale = 1.02;

		data->triMeshID = triMeshDataId;
		int vertIdx = 0;
		RW_DEBUGS("for each vertice");
		BOOST_FOREACH(const Vector3D<float>& v, imesh->getVertices()){
			data->vertices[vertIdx+0] = v(0);
			data->vertices[vertIdx+1] = v(1);
			data->vertices[vertIdx+2] = v(2);
			vertIdx+=3;
		}
		RW_DEBUGS("for each triangle");
		int indiIdx = 0;
		//BOOST_FOREACH(, imesh->getTriangles()){
		for(size_t i=0;i<imesh->getSize();i++){
			const IndexedTriangle<uint32_t> tri = imesh->getIndexedTriangle(i);
			if(invert){
				data->indices[indiIdx+0] = tri.getVertexIdx(2);
				data->indices[indiIdx+1] = tri.getVertexIdx(1);
				data->indices[indiIdx+2] = tri.getVertexIdx(0);
			} else {
				data->indices[indiIdx+0] = tri.getVertexIdx(0);
				data->indices[indiIdx+1] = tri.getVertexIdx(1);
				data->indices[indiIdx+2] = tri.getVertexIdx(2);
			}
			//if(data->indices[indiIdx+0]>=(size_t)nrOfVerts)
			//	std::cout << indiIdx+0 << " " << data->indices[indiIdx+0] << "<" << nrOfVerts << std::endl;
			RW_ASSERT( data->indices[indiIdx+0]< (size_t)nrOfVerts );
			//if(data->indices[indiIdx+1]>=(size_t)nrOfVerts)
			//	std::cout << data->indices[indiIdx+1] << "<" << nrOfVerts << std::endl;
			RW_ASSERT( data->indices[indiIdx+1]<(size_t)nrOfVerts );
			//if(data->indices[indiIdx+2]>=(size_t)nrOfVerts)
			//	std::cout << data->indices[indiIdx+2] << "<" << nrOfVerts << std::endl;
			RW_ASSERT( data->indices[indiIdx+2]<(size_t)nrOfVerts );

			indiIdx+=3;
		}
		RW_DEBUGS("build ode trimesh");
		dGeomTriMeshDataBuildSingle(triMeshDataId,
				&data->vertices[0], 3*sizeof(float), nrOfVerts,
				(dTriIndex*)&data->indices[0], nrOfTris*3, 3*sizeof(dTriIndex));

		RW_DEBUGS("DONE tristuff");
		// write all data to the disc
		/*
		std::ofstream fstr;
		std::stringstream sstr;
		sstr << "test_data_" << nrOfVerts << ".h";
		fstr.open(sstr.str().c_str());
		if(!fstr.is_open())
		    RW_THROW("fstr not open!");
		fstr << "const int VertexCount = " << nrOfVerts << "\n"
			 << "const int IndexCount = " << nrOfTris << " * 3\n"
			 << "\n\n"
			 << "float Vertices[VertexCount * 3] = {";
		for(int i=0;i<nrOfVerts-1;i++){
			fstr << data->vertices[i*3+0] << ","
				 << data->vertices[i*3+1] << ","
				 << data->vertices[i*3+2] << ",\n";
		}
		fstr << data->vertices[(nrOfVerts-1)*3+0] << ","
			 << data->vertices[(nrOfVerts-1)*3+1] << ","
			 << data->vertices[(nrOfVerts-1)*3+2] << "\n };";

		fstr << "\n\ndTriIndex Indices[IndexCount/3][3] = { \n";
		for(int i=0;i<nrOfTris-1;i++){
			fstr << "{" << data->indices[i*3+0] << ","
				 << data->indices[i*3+1] << ","
				 << data->indices[i*3+2] << "},\n";
		}
		fstr << "{" << data->indices[(nrOfTris-1)*3+0] << ","
			 << data->indices[(nrOfTris-1)*3+1] << ","
			 << data->indices[(nrOfTris-1)*3+2] << "}\n };";

		fstr.close();
*/
		//triMeshDatas.push_back(boost::shared_ptr<ODESimulator::TriMeshData>(data) );

//		if( ownedData )
//			delete imesh;

		return data;
	}

	std::vector<ODESimulator::TriGeomData*> buildTriGeom(std::vector<Geometry::Ptr> geoms, const State &state, dSpaceID spaceid, bool invert = false){
        RW_DEBUGS( "----- BEGIN buildTriGeom --------" );
		RW_DEBUGS( "Nr of geoms: " << geoms.size() );
        std::vector<ODESimulator::TriGeomData*> triGeomDatas;
        for(size_t i=0; i<geoms.size(); i++){
            GeometryData::Ptr rwgdata = geoms[i]->getGeometryData();
            Transform3D<> transform = geoms[i]->getTransform();
            RW_DEBUGS(" TRANSFORM: " << geoms[i]->getTransform());

            ODESimulator::TriMeshData::Ptr triMeshData = buildTriMesh(rwgdata,state,invert);
            if(triMeshData==NULL){
            	continue;
            }
            dGeomID geoId;
            bool isTriMesh=false;
            if( Sphere* sphere_rw = dynamic_cast<Sphere*>(rwgdata.get()) ){
                geoId = dCreateSphere(spaceid, (dReal)sphere_rw->getRadius());
            } else if( Cylinder* cyl_rw = dynamic_cast<Cylinder*>(rwgdata.get()) ){
                geoId = dCreateCylinder(spaceid, (dReal)cyl_rw->getRadius(), (dReal)cyl_rw->getHeight());
            } else if( Plane* plane_rw = dynamic_cast<Plane*>(rwgdata.get()) ){
                Vector3D<> n = plane_rw->normal();
                geoId = dCreatePlane(spaceid, (dReal)n[0], (dReal)n[1], (dReal)n[2], (dReal)plane_rw->d());
            } else {
                geoId = dCreateTriMesh(spaceid, triMeshData->triMeshID, NULL, NULL, NULL);
                isTriMesh = true;
            }

    	    dGeomSetData(geoId, triMeshData->triMeshID);
    	    ODESimulator::TriGeomData *gdata = new ODESimulator::TriGeomData(triMeshData);
    	    gdata->isGeomTriMesh = isTriMesh;
    	    ODEUtil::toODETransform(transform, gdata->p, gdata->rot);
    	    gdata->t3d = transform;
            triGeomDatas.push_back(gdata);
            gdata->geomId = geoId;
        }
	    // create geo
        RW_DEBUGS( "----- END buildTriGeom --------");
		return triGeomDatas;
	}

	void nearCallback(void *data, dGeomID o1, dGeomID o2)
	{
		if ( (dGeomIsSpace (o1) && !dGeomIsSpace (o2)) ||
	          (!dGeomIsSpace (o1) && dGeomIsSpace (o2))
	    ) {
	              // colliding a space with something
	              dSpaceCollide2 (o1,o2,data,&nearCallback);
	              // collide all geoms internal to the space(s)
	              //if (dGeomIsSpace (o1)) dSpaceCollide ((dSpaceID)o1,data,&nearCallback);
	              //if (dGeomIsSpace (o2)) dSpaceCollide ((dSpaceID)o2,data,&nearCallback);
	    } else {
	        reinterpret_cast<ODESimulator*>(data)->handleCollisionBetween(o1,o2);

	    }
	}
}

bool isInErrorGlobal = false;
bool badLCPSolution = false;

//const double CONTACT_SURFACE_LAYER = 0.0001;
//const double MAX_SEP_DISTANCE = 0.0005;
//const double MAX_PENETRATION  = 0.00045;

//const int CONTACT_SURFACE_LAYER = 0.006;
//const double MAX_SEP_DISTANCE = 0.008;
//const double MAX_PENETRATION  = 0.007;


double ODESimulator::getMaxSeperatingDistance(){
    return _maxSepDistance;
}

ODESimulator::ODESimulator(DynamicWorkCell::Ptr dwc):
	_dwc(dwc),_time(0.0),_render(new ODEDebugRender(this)),
    _contacts(INITIAL_MAX_CONTACTS),
    _filteredContacts(INITIAL_MAX_CONTACTS+10),
    _rwcontacts(INITIAL_MAX_CONTACTS),
    _rwClusteredContacts(INITIAL_MAX_CONTACTS+10),
    _srcIdx(INITIAL_MAX_CONTACTS+10),
    _dstIdx(INITIAL_MAX_CONTACTS+10),
    _nrOfCon(0),
    _enabledMap(20,1),
    _materialMap(dwc->getMaterialData()),
    _contactMap(dwc->getContactData()),
    _narrowStrategy(new ProximityStrategyPQP()),
    _sensorFeedbacks(5000),
    _nextFeedbackIdx(0),
    _excludeMap(0,100),
    _oldTime(0),
    _useRobWorkContactGeneration(true)
{

    // verify that the linked ode library has the correct
    int isDouble = dCheckConfiguration ("ODE_double_precision");
    //int isRelease = dCheckConfiguration ("ODE_EXT_no_debug");

    if(sizeof(dReal)==sizeof(double)){
        if(isDouble==0){
            RW_THROW("Current linked library does not support double! change dDOUBLE to dSINGLE or link agains ode lib with double support!");
        }
    } else {
        if(isDouble==1){
            RW_THROW("Current linked library does not support single! change dSINGLE to dDOUBLE or link agains ode lib with single support!");
        }
    }

}

void ODESimulator::setEnabled(Body* body, bool enabled){
    if(!body)
        RW_THROW("Body is NULL!");
    ODEBody *odebody = _rwFrameToODEBody[ body->getBodyFrame() ];
    Frame *frame = _rwODEBodyToFrame[ odebody ];
    if( enabled ) {
        dBodyEnable(odebody->getBodyID());
        _enabledMap[*frame] = 1;
    } else{
        dBodyDisable(odebody->getBodyID());
        _enabledMap[*frame] = 0;
    }

}

void ODESimulator::setDynamicsEnabled(dynamics::Body* body, bool enabled){
    ODEBody *odebody = _rwFrameToODEBody[ body->getBodyFrame() ];
	//std::cout << "1" << std::endl;
    if(odebody==NULL)
        return;
	//std::cout << "2" << std::endl;
    //if(odebody->getType()!=ODEBody::RIGID || odebody->getType()!=ODEBody::RIGIDJOINT)
    //    return;
	//std::cout << "3" << std::endl;
    if(enabled){
        //std::cout << "SET DYNAMIC BODY" << std::endl;
		dBodySetDynamic( odebody->getBodyID() );
	} else {
		//std::cout << "SET KINEMATIC BODY" << std::endl;
        dBodySetKinematic( odebody->getBodyID() );
	}
}

namespace {
	bool isClose(dReal *m1, const dReal *P, const dReal *R, double eps ){
		double rsum = 0;
		for(int i=0;i<12;i++){
			float val = fabs(m1[i]-R[i]);
			rsum += val*val;
		}
		double psum = fabs(m1[12]-P[0])*fabs(m1[12]-P[0])+
					  fabs(m1[14]-P[2])*fabs(m1[14]-P[2]);

		return rsum<eps && psum<eps;
	}
}


namespace {
	void drealCopy(const dReal *src, dReal *dst, int n){
		for(int i=0;i<n;i++)
			dst[i]=src[i];
	}
}
/*
    *  Reset each body's position - dBody[Get,Set]Position()
    * Reset each body's quaternion - dBody[Get,Set]Quaternion() ODE stores rotation in quaternions, so don't save/restore in euler angles because it will have to convert to/from quaternions and you won't get a perfect restoration.
    * Reset each body's linear velocity - dBody[Get,Set]LinearVel()
    * Reset each body's angular velocity - dBody[Get,Set]AngularVel()
    * In the quickstep solver, "warm starting" is enabled by default. This involves storing 6 dReal lambda values with each joint so that the previous solution can be applied towards achieving the next. These values must be stored and reset or warm starting disabled. There is no current method to retrieve or set these values, so one must be added manually. They are found at the bottom of the dxJoint struct in joint.h in the source. To disable warm starting, comment out the "#define WARM_STARTING" line in quickstep.cpp.
    * Reset "desired velocity" and "FMax" parameters for motorized joints
    * Reset the "enable" state of every body - If bodies are set to auto-disable, you may need to reset their associated variables (adis_timeleft,adis_stepsleft,etc) - there is no current api method for that.
    * Remove contact joints created during the previous step
    * You might want to assert that the force and torque accumulators are zero
    * Make sure the rest of your controller/simulation is also reset
 */



void ODESimulator::saveODEState(){
    _odeStateStuff.clear();
	// first run through all rigid bodies and set the velocity and force to zero
	// std::cout  << "- Resetting bodies: " << _bodies.size() << std::endl;
	BOOST_FOREACH(dBodyID odebody, _allbodies){
		ODEStateStuff res;
		res.body = odebody;
		dBodyID body = odebody;
		drealCopy( dBodyGetPosition(body), res.pos, 3);
		drealCopy( dBodyGetQuaternion(body), res.rot, 4);
		drealCopy( dBodyGetLinearVel  (body), res.lvel, 3);
		drealCopy( dBodyGetAngularVel (body), res.avel, 3);
		drealCopy( dBodyGetForce  (body), res.force, 3);
		drealCopy( dBodyGetTorque (body), res.torque, 3);
		_odeStateStuff.push_back(res);
	}

	/*
	BOOST_FOREACH(dJointID joint, _alljoints){
	    ODEStateStuff res;
	    res.joint = joint;
	    // test what joint type it is
	    dJointType type = dJointGetType(joint);
	    if( type == dHingeJoint ){
	        dJointGetHingeAngle(joint);
	    }


	    drealCopy( dBodyGetPosition(body), res.pos, 3);
    }
*/

	BOOST_FOREACH(ODEJoint* joint, _allODEJoints){
		ODEStateStuff res;
		res.joint = joint;
		//res.desvel = joint->getVelocity();
		//res.fmax = joint->getMaxForce();
		_odeStateStuff.push_back(res);
	}

}

void ODESimulator::restoreODEState(){
	BOOST_FOREACH(ODEStateStuff &res, _odeStateStuff){
		if(res.body!=NULL){
			dBodySetPosition(res.body, res.pos[0], res.pos[1], res.pos[2]);
			dBodySetQuaternion(res.body, res.rot);
			dBodySetLinearVel(res.body, res.lvel[0], res.lvel[1], res.lvel[2]);
			dBodySetAngularVel(res.body, res.avel[0], res.avel[1], res.avel[2]);
			dBodySetForce(res.body, res.force[0], res.force[1], res.force[2]);
			dBodySetTorque (res.body, res.torque[0], res.torque[1], res.torque[2]);
		} else if(res.joint!=NULL){
			//res.joint->setVelocity(res.desvel);
			//res.joint->setMaxForce(res.desvel);
		}
	}

	//BOOST_FOREACH(ODETactileSensor* sensor,_odeSensors){
	//	sensor->clear();
	//}
}

void ODESimulator::step(double dt, rw::kinematics::State& state)

{
	if(isInErrorGlobal)
		return;
	_stepState = &state;

	//std::cout << "-------------------------- STEP --------------------------------" << std::endl;
	//double dt = 0.001;
	_maxPenetration = 0;
    RW_DEBUGS("-------------------------- STEP --------------------------------");
    double lastDt = _time-_oldTime;
    if(lastDt<=0)
        lastDt = 0;

    RW_DEBUGS("------------- Collisions at " << _time << " :");
    // Detect collision
    _allcontacts.clear();
    if( _useRobWorkContactGeneration ){
        TIMING("Collision: ", detectCollisionsRW(state) );
        {
            boost::mutex::scoped_lock lock(_contactMutex);
            _allcontactsTmp = _allcontacts;
        }
    } else {
        try {
            TIMING("Collision: ", dSpaceCollide(_spaceId, this, &nearCallback) );
            _allcontactsTmp = _allcontacts;
        } catch ( ... ) {
            std::cout << "Collision ERROR";
            Log::errorLog() << "******************** Caught exeption in collision function!*******************" << std::endl;
        }
    }

    // we roll back to this point if there is any penetrations in the scene
    RW_DEBUGS("------------- Save state:");
    saveODEState();
    State tmpState = state;
    double dttmp = dt;
    int i;
    const int MAX_TIME_ITERATIONS = 10;
    badLCPSolution = false;
    int badLCPcount = 0;
    for(i=0;i<MAX_TIME_ITERATIONS;i++){

        if(isInErrorGlobal){
            dJointGroupEmpty(_contactGroupId);
            // and the joint feedbacks that where used is also destroyed
            _nextFeedbackIdx=0;
            RW_THROW("Collision error");
        }

        RW_DEBUGS("------------- Controller update :");
        Simulator::UpdateInfo conStepInfo;
        conStepInfo.dt = dttmp;
        conStepInfo.dt_prev = lastDt;
        conStepInfo.time = _time;
        conStepInfo.rollback = i>0;
        BOOST_FOREACH(SimulatedController::Ptr controller, _controllers ){
            controller->update(conStepInfo, tmpState);
        }

        RW_DEBUGS("------------- Device pre-update:");
        BOOST_FOREACH(ODEDevice *dev, _odeDevices){
            dev->update(dttmp, tmpState);
        }

        RW_DEBUGS("------------- Body pre-update:");
        BOOST_FOREACH(ODEBody *body, _odeBodies){
            body->update(dttmp, tmpState);
        }

        // Step world
        RW_DEBUGS("------------- Step dt=" << dttmp <<" at " << _time << " :");

		//TIMING("Step: ", dWorldStep(_worldId, dttmp));
	    try {
	        switch(_stepMethod){
	        case(WorldStep): TIMING("Step: ", dWorldStep(_worldId, dttmp)); break;
	        case(WorldQuickStep): TIMING("Step: ", dWorldQuickStep(_worldId, dttmp)); break;
	        //case(WorldFast1): TIMING("Step: ", dWorldStepFast1(_worldId, dt, _maxIter)); break;
	        default:
	            TIMING("Step: ", dWorldStep(_worldId, dttmp));
	        }
	    } catch ( ... ) {
	        std::cout << "ERROR";
	        Log::errorLog() << "******************** Caught exeption in step function!*******************" << std::endl;
	    }

	    // if the solution is bad then we need to reduce timestep
	    if(!badLCPSolution){
            // this is onlu done to check that the stepsize was not too big
            //TIMING("Collision: ", dSpaceCollide(_spaceId, this, &nearCallback) );
            bool inCollision = false;
            TIMING("Collision Resolution: ", inCollision = detectCollisionsRW(tmpState, true) );

            if(!inCollision){
                //std::cout << "THERE IS NO PENETRATION" << std::endl;
                break;
            }
	    } else {
	        badLCPcount++;
	        if( i>5 ){
	            bool inCollision = false;
	            TIMING("Collision Resolution: ", inCollision = detectCollisionsRW(tmpState, true) );
	            if(!inCollision){
	                //std::cout << "THERE IS NO PENETRATION" << std::endl;
	                break;
	            }
	        }
	    }

	    if( i==MAX_TIME_ITERATIONS-1){

            // TODO: register the objects that are penetrating such that we don't check them each time
            restoreODEState();
            // print all information available:
            std::cout << "----------------------- TOO LARGE PENETRATIONS --------------------\n";
            std::cout << "-- time    : " << _time << "\n";
            std::cout << "-- dt orig : " << dt << "\n";
            std::cout << "-- dt div  : " << dttmp << "\n";
            std::cout << "-- step divisions: " << i << "\n";
            std::cout << "-- bad lcp count : " << badLCPcount << "\n";
            std::cout << "-- Bodies: \n";
            BOOST_FOREACH(dBodyID body, _allbodies){
                dReal vec[4];
                ODEBody *data = (ODEBody*) dBodyGetData(body);
                if(data!=NULL){
                    std::cout << "--- Body: " << data->getRwBody()->getName();
                } else {
                    std::cout << "--- Body: NoRWBODY";
                }
                drealCopy( dBodyGetPosition(body), vec, 3);
                std::cout << "\n---- pos   : " << printArray(vec, 3);
                drealCopy( dBodyGetQuaternion(body), vec, 4);
                std::cout << "\n---- rot   : " << printArray(vec, 4);
                drealCopy( dBodyGetLinearVel  (body), vec, 3);
                std::cout << "\n---- linvel: " << printArray(vec, 3);
                drealCopy( dBodyGetAngularVel (body), vec, 3);
                std::cout << "\n---- angvel: " << printArray(vec, 3);
                drealCopy( dBodyGetForce  (body), vec, 3);
                std::cout << "\n---- force : " << printArray(vec, 3);
                drealCopy( dBodyGetTorque (body), vec, 3);
                std::cout << "\n---- torque: " << printArray(vec, 3);
                std::cout << "\n";
            }
            std::cout << "--\n-- contacts: \n";
            BOOST_FOREACH(ContactPoint p, _allcontactsTmp){
                std::cout << "-- pos: "<< p.p << "\n";
                std::cout << "-- normal: "<< p.n << "\n";
                std::cout << "-- depth: "<< p.penetration << "\n";
            }

            std::cout << "----------------------- TOO LARGE PENETRATIONS --------------------" << std::endl;
            RW_THROW("Too Large Penetrations!");
            break;
        }
        badLCPSolution = false;
        // max penetration was then we step back to the last configuration and we try again
		dttmp /= 2;
		restoreODEState();
		tmpState = state;
	}
    state = tmpState;
	_oldTime = _time;
	_time += dttmp;

	// if the solution is bad then throw an exception
	//if(badLCPSolution)
	//    RW_WARN("Possibly bad LCP Solution.");
	//std::cout << "dt:" << dttmp << " dt_p:" << lastDt << " time:"<< _time << std::endl;

    Simulator::UpdateInfo conStepInfo;
    conStepInfo.dt = dttmp;
    conStepInfo.dt_prev = lastDt;
    conStepInfo.time = _time;
    conStepInfo.rollback = false;

	RW_DEBUGS("------------- Device post update:");
	//std::cout << "Device post update:" << std::endl;
	BOOST_FOREACH(ODEDevice *dev, _odeDevices){
	    dev->postUpdate(state);
	}
	RW_DEBUGS("------------- Update robwork bodies:");
	//std::cout << "Update robwork bodies:" << std::endl;
    // now copy all state info into state/bodies (transform,vel,force)
    for(size_t i=0; i<_odeBodies.size(); i++){
        _odeBodies[i]->postupdate(state);
    }

    RW_DEBUGS("------------- Sensor update :");
    //std::cout << "Sensor update :" << std::endl;
    // update all sensors with the values of the joints
    BOOST_FOREACH(ODETactileSensor *odesensor, _odeSensors){
        odesensor->update(conStepInfo, state);
    }
    RW_DEBUGS("- removing joint group");
    // Remove all temporary collision joints now that the world has been stepped
    dJointGroupEmpty(_contactGroupId);
    // and the joint feedbacks that where used is also destroyed
    _nextFeedbackIdx=0;

	RW_DEBUGS("------------- Update trimesh prediction:");
	BOOST_FOREACH(TriGeomData *data, _triGeomDatas){
	    if(!data->isGeomTriMesh)
	        continue;
	    dGeomID geom = data->geomId;
	    //if( dGeomGetClass(geom) != dTriMeshClass )
	    //    continue;
	    const dReal* Pos = dGeomGetPosition(geom);
	    const dReal* Rot = dGeomGetRotation(geom);

	    // Fill in the (4x4) matrix.
        dReal* p_matrix = data->mBuff[data->mBuffIdx];

        //if( !isClose(p_matrix, Pos, Rot, 0.001) ){
			p_matrix[ 0]=Rot[0]; p_matrix[ 1]=Rot[1]; p_matrix[ 2]=Rot[ 2 ]; p_matrix[ 3]=0;
			p_matrix[ 4]=Rot[4]; p_matrix[ 5]=Rot[5]; p_matrix[ 6]=Rot[ 6 ]; p_matrix[ 7]=0;
			p_matrix[ 8]=Rot[8]; p_matrix[ 9]=Rot[9]; p_matrix[10]=Rot[10 ]; p_matrix[11]=0;
			p_matrix[12]=Pos[0]; p_matrix[13]=Pos[1]; p_matrix[14]=Pos[ 2 ]; p_matrix[15]=1;

			// Flip to other matrix.
			data->mBuffIdx = !data->mBuffIdx;
        //}

        dGeomTriMeshSetLastTransform( geom, data->mBuff[data->mBuffIdx]);

	}
	//std::cout << "e";
	RW_DEBUGS("----------------------- END STEP --------------------------------");
	//std::cout << "-------------------------- END STEP --------------------------------" << std::endl;
}

ODEBody* ODESimulator::createRigidBody(Body* rwbody,
                                      const rw::kinematics::State& state,
                                      dSpaceID spaceid)
{
    const BodyInfo& info = rwbody->getInfo();
    // create a triangle mesh for all staticly connected nodes
    // std::vector<Frame*> frames = DynamicUtil::getAnchoredFrames( *bframe, state);
    RW_DEBUGS( "- Create Rigid body: " << rwbody->getBodyFrame()->getName());

    std::vector<Geometry::Ptr> geoms = rwbody->getGeometry();
	std::vector<TriGeomData*> gdatas = buildTriGeom(geoms, state, spaceid, false);

	if(gdatas.size()==0){
		RW_WARN("Body: "<< rwbody->getBodyFrame()->getName() << " has no geometry!");
	}

    Vector3D<> mc = info.masscenter;
    dMass m;
    setODEBodyMass(&m, info.mass, Vector3D<>(0,0,0), info.inertia);


    //std::cout << "RW inertia: " << info.inertia << std::endl;
    //printMassInfo(m, *rwbody->getBodyFrame() );
    dMassCheck(&m);
    // create the body and initialize mass, inertia and stuff

    dBodyID bodyId = dBodyCreate(_worldId);
	ODEUtil::setODEBodyT3D(bodyId, rwbody->wTcom(state) );
	//ODEUtil::setODEBodyMass(bodyId, info.mass, Vector3D<>(0,0,0), info.inertia);

	dBodySetMass(bodyId, &m);

    int mid = _materialMap.getDataID( info.material );
    int oid = _contactMap.getDataID( info.objectType );

	ODEBody *odeBody=0;
    if(RigidBody *rbody = dynamic_cast<RigidBody*>(rwbody)){
        odeBody = new ODEBody(bodyId, rbody, info.masscenter, mid, oid);
        _odeBodies.push_back(odeBody);
        _allbodies.push_back(bodyId);
        dBodySetData (bodyId, (void*)odeBody);
    } else if(RigidJoint *rjbody = dynamic_cast<RigidJoint*>(rwbody)){
        odeBody = new ODEBody(bodyId, rjbody, info.masscenter, mid, oid);
        dBodySetData (bodyId, (void*)odeBody);
        //_odeBodies.push_back(odeBody);
        _allbodies.push_back(bodyId);
    }

    // check if body frame has any properties that relate to ODE
    if( rwbody->getBodyFrame()->getPropertyMap().has("LinearDamping") ){
        dReal linDamp = rwbody->getBodyFrame()->getPropertyMap().get<double>("LinearDamping");
        dBodySetLinearDamping(bodyId, linDamp);
    }
    if( rwbody->getBodyFrame()->getPropertyMap().has("AngularDamping") ){
        dReal angDamp = rwbody->getBodyFrame()->getPropertyMap().get<double>("AngularDamping");
        dBodySetAngularDamping(bodyId, angDamp);
    }

	// now associate all geometry with the body
	BOOST_FOREACH(TriGeomData* gdata, gdatas){
		_triGeomDatas.push_back(gdata);
		//Vector3D<> mc = gdata->t3d.R() * bmc;
		dGeomSetBody(gdata->geomId, bodyId);
		dGeomSetData(gdata->geomId, odeBody);
		_frameToOdeGeoms[rwbody->getBodyFrame()] = gdata->geomId;
		// the geom must be attached to body before offset is possible
		dGeomSetOffsetPosition(gdata->geomId, gdata->p[0]-mc[0], gdata->p[1]-mc[1], gdata->p[2]-mc[2]);
		dGeomSetOffsetQuaternion(gdata->geomId, gdata->rot);
	}
	dBodySetMaxAngularSpeed(bodyId, 10);
    _rwODEBodyToFrame[odeBody] = rwbody->getBodyFrame();
    _rwFrameToODEBody[rwbody->getBodyFrame()] = odeBody;
    BOOST_FOREACH(Frame* frame, rwbody->getFrames()){
        //std::cout  << "--> Adding frame: " << frame->getName() << std::endl;
        _rwFrameToODEBody[frame] = odeBody;
    }

    return odeBody;
}

rwsim::drawable::SimulatorDebugRender* ODESimulator::createDebugRender(){
    return _render;
}
// worlddimension -
// gravity

void ODESimulator::readProperties(){
    /*
    GLOBAL OPTIONS
    stepmethod - (WorldStep), QuickStep, FastStep1
    maxiterations - int (20)
    spacetype - simple, hash, (quad)
    quadtree depth - int
    WorldCFM - double [0-1]
    WorldERP - double [0-1]
    ContactClusteringAlg - Simple, Cube, ConvexHull, ConvexHullApprox


    ContactSurfaceLayer in meter default is 0.0001
    MaxSepDistance in meter default is 0.0005
    MaxPenetration in meter default is 0.00045

    PER BODY OPTIONS

    PER material-pair OPTIONS
        friction - coloumb,
        restitution -

     */
    _contactSurfaceLayer = _propertyMap.get<double>("ContactSurfaceLayer", 0.0001);
    _maxSepDistance = _propertyMap.get<double>("MaxSepDistance", 0.0005);
    _maxAllowedPenetration = _propertyMap.get<double>("MaxPenetration", _maxSepDistance);


	_maxIter = _propertyMap.get<int>("MaxIterations", 20);
	//std::string spaceTypeStr = _propertyMap.get<std::string>("SpaceType", "QuadTree");
	std::string spaceTypeStr = _propertyMap.get<std::string>("SpaceType", "Simple");
	//std::string stepStr = _propertyMap.get<std::string>("StepMethod", "WorldQuickStep");
	std::string stepStr = _propertyMap.get<std::string>("StepMethod", "WorldStep");
	if( stepStr=="WorldQuickStep" ){
		_stepMethod = WorldQuickStep;
	} else if( stepStr=="WorldStep" ) {
		_stepMethod = WorldStep;
	} else if( stepStr=="WorldFast1" ){
		_stepMethod = WorldFast1;
	} else {
		RW_THROW("ODE simulator property: Unknown step method!");
	}

	_worldCFM = _propertyMap.get<double>("WorldCFM", 0.0000001);
	_worldERP = _propertyMap.get<double>("WorldERP", 0.2);
	_clusteringAlgStr =  _propertyMap.get<std::string>("ContactClusteringAlg", "Box");

	if( spaceTypeStr == "QuadTree" )
		_spaceType = QuadTree;
	else if(spaceTypeStr == "Simple" )
		_spaceType = Simple;
	else if(spaceTypeStr == "HashTable" )
		_spaceType = HashTable;
	else
		_spaceType = QuadTree;
}

void ODESimulator::emitPropertyChanged(){
	readProperties();
}

static bool isODEInitialized = false;

ODEBody* ODESimulator::createKinematicBody(KinematicBody* kbody,
        const rw::kinematics::State& state,
        dSpaceID spaceid)
{
    BodyInfo info = kbody->getInfo();

    RW_ASSERT(kbody!=NULL);
    // create a triangle mesh for all statically connected nodes
    std::vector<Geometry::Ptr> geoms = kbody->getGeometry();
    std::vector<TriGeomData*> gdatas = buildTriGeom(geoms, state, _spaceId, false);
    // if no triangles was loaded then continue
    if( gdatas.size()==0 ){
        RW_WARN("No triangle mesh defined for this body: " << kbody->getBodyFrame()->getName());
    }

    Vector3D<> mc = info.masscenter;
    Transform3D<> wTb = Kinematics::worldTframe(kbody->getBodyFrame(), state);
    wTb.P() += wTb.R()*mc;

    dBodyID bodyId = dBodyCreate(_worldId);
    dBodySetKinematic(bodyId);
    ODEUtil::setODEBodyT3D(bodyId, wTb);

    int mid = _materialMap.getDataID( info.material );
    int oid = _contactMap.getDataID( info.objectType );

    ODEBody *odeBody = new ODEBody(bodyId, kbody, mid , oid);
    _odeBodies.push_back(odeBody);
    dBodySetData (bodyId, odeBody);
    _allbodies.push_back(bodyId);
    _rwODEBodyToFrame[odeBody] = kbody->getBodyFrame();
    _rwFrameToODEBody[kbody->getBodyFrame()] = odeBody;


    BOOST_FOREACH(TriGeomData* gdata, gdatas){
        _triGeomDatas.push_back(gdata);

        dGeomSetBody(gdata->geomId, bodyId);
        dGeomSetData(gdata->geomId, odeBody);

        _frameToOdeGeoms[kbody->getBodyFrame()] = gdata->geomId;

        // set position and rotation offset of the geometry relative to the body
        dGeomSetOffsetPosition(gdata->geomId, gdata->p[0]-mc[0], gdata->p[1]-mc[1], gdata->p[2]-mc[2]);
        dGeomSetOffsetQuaternion(gdata->geomId, gdata->rot);
    }


    BOOST_FOREACH(Frame* frame, kbody->getFrames()){
        RW_DEBUGS( "(KB) --> Adding frame: " << frame->getName() );
        _rwFrameToODEBody[frame] = odeBody;
    }
    return odeBody;
}


ODEBody* ODESimulator::createFixedBody(Body* rwbody,
        const rw::kinematics::State& state,
        dSpaceID spaceid)
{
    const BodyInfo& info = rwbody->getInfo();
	FixedBody *rbody = dynamic_cast<FixedBody*>( rwbody );
	if(rbody==NULL)
		RW_THROW("Not a fixed body!");
	// create a triangle mesh for all statically connected nodes
	std::vector<Geometry::Ptr> geoms = rwbody->getGeometry();
	std::vector<TriGeomData*> gdatas = buildTriGeom(geoms, state, _spaceId, false);
	// if no triangles was loaded then continue
	if( gdatas.size()==0 ){
		RW_WARN("No triangle mesh defined for this body: " << rwbody->getBodyFrame()->getName());
	}

    //Vector3D<> mc = info.masscenter;
    Transform3D<> wTb = Kinematics::worldTframe(rbody->getBodyFrame(), state);

    // create vector of geomids
    std::vector<dGeomID> geomids(gdatas.size());
    for(size_t i=0; i<gdatas.size(); i++ ){
        geomids[i] = gdatas[i]->geomId;
    }

    int mid = _materialMap.getDataID( info.material );
    int oid = _contactMap.getDataID( info.objectType );
    ODEBody *odeBody = new ODEBody(geomids, rbody, mid , oid);
    _rwODEBodyToFrame[odeBody] = rwbody->getBodyFrame();
    _rwFrameToODEBody[rwbody->getBodyFrame()] = odeBody;

	BOOST_FOREACH(TriGeomData* gdata, gdatas){
		_triGeomDatas.push_back(gdata);
		// set position and rotation of body
		dGeomSetData(gdata->geomId, odeBody);
		Transform3D<> gt3d = wTb*gdata->t3d;
		ODEUtil::setODEGeomT3D(gdata->geomId, gt3d);

		_frameToOdeGeoms[rbody->getBodyFrame()] = gdata->geomId;
	}

    BOOST_FOREACH(Frame* frame, rwbody->getFrames()){
        RW_DEBUGS( "(FB) --> Adding frame: " << frame->getName() );
        _rwFrameToODEBody[frame] = odeBody;
    }

	_odeBodies.push_back(odeBody);
	return odeBody;
}

namespace {

    void printMessage (int num, const char *msg1, const char *msg2, va_list ap)
    {
      fflush (stderr);
      fflush (stdout);
      if (num) fprintf (stderr,"\n%s %d: ",msg1,num);
      else fprintf (stderr,"\n%s: ",msg1);
      vfprintf (stderr,msg2,ap);
      fprintf (stderr,"\n");
      fflush (stderr);
    }

	void EmptyMessageFunction(int errnum, const char *msg, va_list ap){
		if(errnum==3){
		    // the LCP solution is bad. Try reducing the timestep
		    badLCPSolution = true;
		} else {
		    printMessage (errnum,"ODE Message",msg,ap);
		    //RW_WARN("ODE internal msg: errnum=" << errnum << " odemsg=\"" <<  msg<< "\"");
		}
	}

	void ErrorMessageFunction(int errnum, const char *msg, va_list ap){
	    printMessage (errnum,"ODE Error",msg,ap);
		isInErrorGlobal=true;
		//RW_THROW("ODE internal Error: errnum=" << errnum << " odemsg=\"" <<  msg<< "\"");
		RW_THROW("ODE ERROR");
	}

	void DebugMessageFunction(int errnum, const char *msg, va_list ap){
	    printMessage (errnum,"ODE INTERNAL ERROR",msg,ap);
		isInErrorGlobal=true;
		//dWorldCleanupWorkingMemory();
		RW_THROW("ODE INTERNAL ERROR");
	}




}

void ODESimulator::initPhysics(rw::kinematics::State& state)
{
    _propertyMap = _dwc->getEngineSettings();
    //CollisionSetup cSetup = Proximity::getCollisionSetup( *_dwc->getWorkcell() );

    //FramePairList excludeList = BasicFilterStrategy::getExcludePairList(*_dwc->getWorkcell(), cSetup);
    //BOOST_FOREACH(rw::kinematics::FramePair& pair, excludeList){
    //    _excludeMap[rw::kinematics::FramePair(pair.first,pair.second)] = 1;
    //}

    _bpstrategy = ownedPtr( new BasicFilterStrategy( _dwc->getWorkcell() ) );
    // build the frame map
    std::vector<Frame*> frames = Kinematics::findAllFrames(_dwc->getWorkcell()->getWorldFrame(), state);
    BOOST_FOREACH(Frame *frame, frames){
        _frameToModels[*frame] = _narrowStrategy->getModel(frame);
    }


    readProperties();

    if(!isODEInitialized){
        dInitODE2(0);
        isODEInitialized = true;
        dSetErrorHandler(ErrorMessageFunction);
        dSetDebugHandler(DebugMessageFunction);
        dSetMessageHandler(EmptyMessageFunction);
    }


	// Create the world

	_worldId = dWorldCreate();

	// Create the space for geometric collision geometries
	WorkCellDimension wcdim = _dwc->getWorldDimension();
	switch(_spaceType){
        case(Simple):{
            _spaceId = dSimpleSpaceCreate(0);
        }
        break;
        case(HashTable):{
            _spaceId = dHashSpaceCreate(0);
        }
        break;
        case(QuadTree):{
            dVector3 center, extends;
            ODEUtil::toODEVector(wcdim.center, center);
            ODEUtil::toODEVector(wcdim.boxDim, extends);
            _spaceId = dQuadTreeSpaceCreate (0, center, extends, 7);
        }
        break;
        default:{
            RW_THROW("UNSUPPORTED SPACE TYPE!");
        }
	}

	// Create joint group
    _contactGroupId = dJointGroupCreate(0);

	// add gravity
    Vector3D<> gravity = _dwc->getGravity();
	dWorldSetGravity ( _worldId, gravity(0), gravity(1), gravity(2) );
	dWorldSetCFM ( _worldId, _worldCFM );
	dWorldSetERP ( _worldId, _worldERP );

	dWorldSetContactSurfaceLayer(_worldId, _contactSurfaceLayer);
	//dWorldSetContactMaxCorrectingVel(_worldId, 0.05);
	//dWorldSetAngularDamping()
    State initState = state;
    // first set the initial state of all devices.
    BOOST_FOREACH(DynamicDevice* device, _dwc->getDynamicDevices() ){
        JointDevice *jdev = dynamic_cast<JointDevice*>( &(device->getModel()) );
        if(jdev==NULL)
            continue;
        Q offsets = Q::zero( jdev->getQ(state).size() );
        jdev->setQ( offsets , initState );
    }

    //dCreatePlane(_spaceId,0,0,1,0);

    RW_DEBUGS( "- ADDING BODIES " );
    BOOST_FOREACH(Body* body, _dwc->getBodies() ){
        // check if a body is part
        if(!_dwc->inDevice(body) )
            addBody(body, state);
    }

	Frame *wframe = _dwc->getWorkcell()->getWorldFrame();
	_rwODEBodyToFrame[0] = wframe;

	RW_DEBUGS( "- ADDING DEVICES " );
    BOOST_FOREACH(DynamicDevice* device, _dwc->getDynamicDevices() ){
        addDevice(device, state);
    }

    RW_DEBUGS( "- ADDING SENSORS " );
    BOOST_FOREACH(rwlibs::simulation::SimulatedSensor::Ptr sensor, _dwc->getSensors()){
    	addSensor(sensor, state);
	}

    RW_DEBUGS( "- ADDING CONTROLLERS " );
    BOOST_FOREACH(rwlibs::simulation::SimulatedController::Ptr controller, _dwc->getControllers()){
    	addController(controller);
	}

    RW_DEBUGS( "- CREATING MATERIAL MAP " );
    _odeMaterialMap = new ODEMaterialMap(_materialMap, _contactMap, _odeBodies);

    RW_DEBUGS( "- RESETTING SCENE " );
	resetScene(state);
}
ODEBody* ODESimulator::createBody(dynamics::Body* body, const rw::kinematics::State& state, dSpaceID spaceid)
{
    ODEBody *odeBody = NULL;
    if( RigidBody *rbody = dynamic_cast<RigidBody*>( body ) ){
        odeBody = createRigidBody(rbody, state, spaceid);
        dBodyID bodyId = odeBody->getBodyID();
        _bodies.push_back(bodyId);
        dBodySetAutoDisableFlag(bodyId, 0);
    } else if( KinematicBody *kbody = dynamic_cast<KinematicBody*>( body ) ) {
        odeBody = createKinematicBody(kbody, state, spaceid);
    } else if( FixedBody *fbody = dynamic_cast<FixedBody*>( body ) ) {
        odeBody = createFixedBody(fbody, state, spaceid);
    } else {
        RW_WARN("Unsupported body type, name: " << body->getName() );
    }
    return odeBody;
}

void ODESimulator::addBody(rwsim::dynamics::Body::Ptr body, rw::kinematics::State& state){
    createBody(body.get(), state, _spaceId );
    //body->getInfo().print();
}

void ODESimulator::addDevice(rwsim::dynamics::DynamicDevice::Ptr dev, rw::kinematics::State& nstate){
    Frame *wframe = _dwc->getWorkcell()->getWorldFrame();
    rwsim::dynamics::DynamicDevice *device = dev.get();
    State state = nstate;

    JointDevice *jdev = dynamic_cast<JointDevice*>( &(device->getModel()) );
    if(jdev!=NULL){
        Q offsets = Q::zero( jdev->getQ(state).size() );
        jdev->setQ( offsets , state );
    }

    if( dynamic_cast<RigidDevice*>( device ) ){
        RW_DEBUGS("RigidDevice")
         // we use hashspace here because devices typically have
         // relatively few bodies
         dSpaceID space = dHashSpaceCreate( _spaceId );

         // add kinematic constraints from base to joint1, joint1 to joint2 and so forth
         RigidDevice *fDev = dynamic_cast<RigidDevice*>( device );
         Body *baseBody = fDev->getBase();
         Frame *base = baseBody->getBodyFrame();

         // Check if the parent of base has been added to the ODE world,
         // if not create a fixed body whereto the base can be attached
         Frame *baseParent = base->getParent();
         dBodyID baseParentBodyID = 0;
         if(_rwFrameToODEBody.find(baseParent) == _rwFrameToODEBody.end()){
             //RW_WARN("No parent data available, connecting base to world!");
             dBodyID baseParentBodyId = dBodyCreate(_worldId);
             ODEUtil::setODEBodyT3D(baseParentBodyId, Kinematics::worldTframe(baseParent,state));
             baseParentBodyID = 0;
             //_rwFrameToODEBody[baseParent] = baseParentBodyId;
             _rwFrameToODEBody[baseParent] = 0;
         } else {
             baseParentBodyID = _rwFrameToODEBody[baseParent]->getBodyID();
         }

         // now create the base
         RW_DEBUGS( "Create base");

         ODEBody *baseODEBody = createBody(baseBody, state, space);
         /*
         if( dynamic_cast<FixedBody*>(baseBody) ){
             RW_DEBUGS("- Fixed");
             baseODEBody = createFixedBody(baseBody, state, space);
         } else if(KinematicBody *kbody = dynamic_cast<KinematicBody*>(baseBody)){
             RW_DEBUGS("- Kinematic");
             baseODEBody = createKinematicBody(kbody, state, space);
         } else if(dynamic_cast<RigidBody*>(baseBody)){
             RW_DEBUGS("- Rigid");
             baseODEBody = createRigidBody(baseBody, state, space);
         } else {
             RW_THROW("Unknown body type of robot \""<< device->getModel().getName()<<"\" base");
         }
         */
         dBodyID baseBodyID = baseODEBody->getBodyID();
         _rwFrameToODEBody[ base ] = baseODEBody;

         // and connect the base to the parent using a fixed joint if the base is rigid
         // we only do this if the parent is another body, NOT if its the world
         if( dynamic_cast<RigidBody*>(baseBody) ){
        	 if(_rwFrameToODEBody[baseParent]!=0){
                 dJointID baseJoint = dJointCreateFixed(_worldId, 0);
                 dJointAttach(baseJoint, baseBodyID, baseParentBodyID);
                 dJointSetFixed(baseJoint);
        	 }
         }

         std::vector<ODEJoint*> odeJoints;
         Q maxForce = fDev->getForceLimit();
         RW_DEBUGS("BASE:" << base->getName() << "<--" << base->getParent()->getName() );

         size_t i =0;
         BOOST_FOREACH(RigidJoint *rjoint, fDev->getBodies() ){
             Joint *joint = rjoint->getJoint();
             Frame *parent = joint->getParent(state);
             RW_DEBUGS(parent->getName() << "<--" << joint->getName());

             ODEBody *odeParent;// = _rwFrameToODEBody[parent];
             //Frame *parentFrame = NULL;
             if(_rwFrameToODEBody.find(parent) == _rwFrameToODEBody.end() ){
                 // could be that the reference is
                 RW_WARN("odeParent is NULL, " << joint->getName() << "-->"
                         << parent->getName()
                         << " Using WORLD as parent");

                 odeParent = _rwFrameToODEBody[wframe];
                 _rwFrameToODEBody[parent] = odeParent;
             }
             odeParent = _rwFrameToODEBody[parent];
             //parentFrame = _rwODEBodyToFrame[odeParent];

             ODEBody* odeChild = createRigidBody(rjoint, state, space ); //_rwFrameToBtBody[joint];
             if(odeChild==NULL){
                 RW_WARN("odeChild is NULL, " << joint->getName());
                 RW_ASSERT(0);
             }

             Transform3D<> wTchild = Kinematics::worldTframe(joint,state);
             Vector3D<> haxis = wTchild.R() * Vector3D<>(0,0,1);
             Vector3D<> hpos = wTchild.P();
             //Transform3D<> wTparent = Kinematics::WorldTframe(parentFrame,initState);

             std::pair<Q, Q> posBounds = joint->getBounds();

              if(RevoluteJoint *rwjoint = dynamic_cast<RevoluteJoint*>(joint)){
                  RW_DEBUGS("Revolute joint");
                  const double qinit = rwjoint->getData(state)[0];
                  dJointID hinge = dJointCreateHinge (_worldId, 0);
                  dJointAttach(hinge, odeChild->getBodyID(), odeParent->getBodyID());
                  dJointSetHingeAxis(hinge, haxis(0) , haxis(1), haxis(2));
                  dJointSetHingeAnchor(hinge, hpos(0), hpos(1), hpos(2));
                  dJointSetHingeParam(hinge, dParamCFM, 0.001);
                  // set the position limits
                  // TODO: these stops can only handle in interval [-Pi, Pi]
                  dJointSetHingeParam(hinge, dParamLoStop, posBounds.first[0] );
                  dJointSetHingeParam(hinge, dParamHiStop, posBounds.second[0] );

                  dJointID motor = NULL;
                  motor = dJointCreateAMotor (_worldId, 0);
                  dJointAttach(motor, odeChild->getBodyID(), odeParent->getBodyID());
                  dJointSetAMotorNumAxes(motor, 1);
                  dJointSetAMotorAxis(motor, 0, 1, haxis(0) , haxis(1), haxis(2));
                  dJointSetAMotorAngle(motor,0, qinit);
                  dJointSetAMotorParam(motor,dParamFMax, maxForce(i) );
                  dJointSetAMotorParam(motor,dParamVel,0);
                  //dJointSetAMotorParam(Amotor,dParamLoStop,-0);
                  //dJointSetAMotorParam(Amotor,dParamHiStop,0);


                  // we use motor to simulate friction
                  /*
                  dJointID motor2 = dJointCreateAMotor (_worldId, 0);
                  dJointAttach(motor2, odeChild, odeParent);
                  dJointSetAMotorNumAxes(motor2, 1);
                  dJointSetAMotorAxis(motor2, 0, 1, haxis(0) , haxis(1), haxis(2));
                  dJointSetAMotorAngle(motor2,0, qinit);
                  dJointSetAMotorParam(motor2,dParamFMax, maxForce(i)/50 );
                  dJointSetAMotorParam(motor2,dParamVel,0);
                  */
                  ODEJoint *odeJoint = new ODEJoint(ODEJoint::Revolute, hinge, motor, odeChild->getBodyID(), rjoint);
                  _jointToODEJoint[rwjoint] = odeJoint;
                  odeJoints.push_back(odeJoint);
                  _allODEJoints.push_back(odeJoint);
              } else if( dynamic_cast<DependentRevoluteJoint*>(joint)){
                  RW_DEBUGS("DependentRevolute");
                  DependentRevoluteJoint *rframe = dynamic_cast<DependentRevoluteJoint*>(joint);
                  Joint *owner = &rframe->getOwner();
                  const double qinit = owner->getData(state)[0]*rframe->getScale()+0;

                  dJointID hinge = dJointCreateHinge (_worldId, 0);
                  dJointAttach(hinge, odeChild->getBodyID(), odeParent->getBodyID());
                  dJointSetHingeAxis(hinge, haxis(0) , haxis(1), haxis(2));
                  dJointSetHingeAnchor(hinge, hpos(0), hpos(1), hpos(2));

                  dJointID motor = dJointCreateAMotor (_worldId, 0);
                  dJointAttach(motor, odeChild->getBodyID(), odeParent->getBodyID());
                  dJointSetAMotorNumAxes(motor, 1);
                  dJointSetAMotorAxis(motor, 0, 1, haxis(0) , haxis(1), haxis(2));
                  dJointSetAMotorAngle(motor,0, qinit);
                  dJointSetAMotorParam(motor,dParamFMax, 20/*maxForce(i)*/ );
                  dJointSetAMotorParam(motor,dParamVel,0);



                  ODEJoint *odeOwner = _jointToODEJoint[owner];
                  ODEJoint *odeJoint = new ODEJoint( ODEJoint::Revolute, hinge, motor,  odeChild->getBodyID(),
                                                     odeOwner, rframe ,
                                                     rframe->getScale(), 0 , rjoint);
                  odeJoints.push_back(odeJoint);
                  //dJointSetAMotorParam(Amotor,dParamLoStop,-0);
                  //dJointSetAMotorParam(Amotor,dParamHiStop,0);
                  _allODEJoints.push_back(odeJoint);
              } else if( PrismaticJoint *pjoint = dynamic_cast<PrismaticJoint*>(joint) ){

                  // test if another joint is dependent on this joint


                  //const double qinit = pjoint->getData(state)[0];
                  dJointID slider = dJointCreateSlider (_worldId, 0);
                  dJointAttach(slider, odeChild->getBodyID(), odeParent->getBodyID());
                  dJointSetSliderAxis(slider, haxis(0) , haxis(1), haxis(2));
                  //dJointSetHingeAnchor(slider, hpos(0), hpos(1), hpos(2));
                  dJointSetSliderParam(slider, dParamLoStop, posBounds.first[0] );
                  dJointSetSliderParam(slider, dParamHiStop, posBounds.second[0] );

                  dJointID motor = dJointCreateLMotor (_worldId, 0);
                  dJointAttach(motor, odeChild->getBodyID(), odeParent->getBodyID());
                  dJointSetLMotorNumAxes(motor, 1);
                  dJointSetLMotorAxis(motor, 0, 1, haxis(0) , haxis(1), haxis(2));
                  //dJointSetLMotorAngle(motor,0, qinit);

                  dJointSetLMotorParam(motor,dParamFMax, maxForce(i) );
                  dJointSetLMotorParam(motor,dParamVel,0);

                  //dJointSetAMotorParam(Amotor,dParamLoStop,-0);
                  //dJointSetAMotorParam(Amotor,dParamHiStop,0);
                  ODEJoint *odeJoint = new ODEJoint(ODEJoint::Prismatic, slider, motor, odeChild->getBodyID(), rjoint);
                  _jointToODEJoint[pjoint] = odeJoint;
                  odeJoints.push_back(odeJoint);
                  _allODEJoints.push_back(odeJoint);

              } else if( dynamic_cast<DependentPrismaticJoint*>(joint) ) {
                  RW_DEBUGS("DependentPrismaticJoint");
                  DependentPrismaticJoint *pframe = dynamic_cast<DependentPrismaticJoint*>(joint);
                  Joint *owner = &pframe->getOwner();
                  //const double qinit = owner->getData(state)[0]*pframe->getScale()+0;

                  ODEBody* ownerBody = _rwFrameToODEBody[owner];

                  dJointID slider = dJointCreateSlider (_worldId, 0);
                  //dJointAttach(slider, odeChild, odeParent);
                  dJointAttach(slider, odeChild->getBodyID(), ownerBody->getBodyID());
                  dJointSetSliderAxis(slider, haxis(0) , haxis(1), haxis(2));

                  dJointID motor = dJointCreateLMotor (_worldId, 0);
                  //dJointAttach(motor, odeChild, odeParent);
                  dJointAttach(motor, odeChild->getBodyID(), ownerBody->getBodyID());
                  dJointSetLMotorNumAxes(motor, 1);
                  dJointSetLMotorAxis(motor, 0, 1, haxis(0) , haxis(1), haxis(2));

                 // std::cout << "i:" << i << " mforce_len: " << maxForce.size() << std::endl;
                  // TODO: should take the maxforce value of the owner joint
                  dJointSetLMotorParam(motor,dParamFMax, maxForce(i) );
                  dJointSetLMotorParam(motor,dParamVel,0);

                  ODEJoint *odeOwner = _jointToODEJoint[owner];
                  ODEJoint *odeJoint = new ODEJoint( ODEJoint::Prismatic, slider, motor, odeChild->getBodyID(),
                                                     odeOwner, pframe,
                                                     pframe->getScale(), 0 , rjoint);
                  odeJoints.push_back(odeJoint);

                  _allODEJoints.push_back(odeJoint);
              } else {
                  RW_WARN("Joint type not supported!");
              }

              i++;

           }
         _odeDevices.push_back( new ODEVelocityDevice(fDev, odeJoints, maxForce) );


     } else  if( dynamic_cast<KinematicDevice*>( device ) ){
         RW_DEBUGS("KinematicDevice");
         // TODO: create all joints and make them kinematic
         KinematicDevice* kdev = dynamic_cast<KinematicDevice*>( device );
         dSpaceID space = dHashSpaceCreate( _spaceId );
         std::vector<dBodyID> kDevBodies;
         BOOST_FOREACH(KinematicBody *kbody, kdev->getBodies() ){
             dBodyID odeBodyID = createKinematicBody(kbody, state, space)->getBodyID();
             kDevBodies.push_back(odeBodyID);
         }

         ODEKinematicDevice *odekdev = new ODEKinematicDevice( kdev, kDevBodies);
         _odeDevices.push_back(odekdev);
     } else if( SuctionCup* scup = dynamic_cast<SuctionCup*>( device ) ) {
         RW_WARN("Creating suction cup!");
         //ODESuctionCupDevice *scup_ode = ODESuctionCupDevice::makeSuctionCup( scup , this, state);

         //BodyContactSensor::Ptr sensor = ownedPtr(new BodyContactSensor("SuctionCupSensor", scup->getEndBody()->getBodyFrame() ));
         //addSensor( sensor );
         // make ODE suction cup simulation
         ODESuctionCupDevice *scup_ode = new ODESuctionCupDevice(scup , this, state);
         _odeDevices.push_back(scup_ode);
     } else {
         RW_WARN("Controller not supported!");
     }
}

void ODESimulator::addSensor(rwlibs::simulation::SimulatedSensor::Ptr sensor, rw::kinematics::State& state){
	_sensors.push_back(sensor);

	SimulatedSensor *ssensor = sensor.get();
    if( dynamic_cast<SimulatedTactileSensor*>(ssensor) ){

        SimulatedTactileSensor *tsensor = dynamic_cast<SimulatedTactileSensor*>(sensor.get());
        Frame *bframe = tsensor->getSensorFrame();

        //std::cout << "Adding SimulatedTactileSensor: " << sensor->getSensor()->getName() << std::endl;
        //std::cout << "Adding SimulatedTactileSensor Frame: " << sensor->getSensor()->getFrame()->getName() << std::endl;
        if( _rwFrameToODEBody.find(bframe)== _rwFrameToODEBody.end()){
            RW_THROW("The frame that the sensor is being attached to is not in the simulator! Did you remember to run initphysics!");
        }

        ODETactileSensor *odesensor = new ODETactileSensor(tsensor);

        ODEBody* odeBody = _rwFrameToODEBody[bframe];
        // TODO: this should be a list of sensors for each body/frame
        //if(_odeBodyToSensor.find(odeBody->getBodyID())!=_odeBodyToSensor.end()){
        //	RW_ASSERT(0);
        //}
        _rwsensorToOdesensor[sensor] = odesensor;
        _odeBodyToSensor[odeBody->getBodyID()].push_back( odesensor );

        _odeSensors.push_back(odesensor);
    } else {
        _rwsensorToOdesensor[sensor] = NULL;
    }
}

void ODESimulator::removeSensor(rwlibs::simulation::SimulatedSensor::Ptr sensor)
{
    std::vector<rwlibs::simulation::SimulatedSensor::Ptr> newsensors;
    BOOST_FOREACH(SimulatedSensor::Ptr oldsen , _sensors){
        if(sensor != oldsen )
            newsensors.push_back(oldsen);
    }
    _sensors = newsensors;
    Frame *bframe = sensor->getSensorFrame();
    if( _rwFrameToODEBody.find(bframe)== _rwFrameToODEBody.end()){
        return;
    }

    ODEBody* odeBody = _rwFrameToODEBody[bframe];

    if(_rwsensorToOdesensor.find(sensor)!=_rwsensorToOdesensor.end()){
        ODETactileSensor *tsensor = _rwsensorToOdesensor[sensor];
        // the sensor has an equivalent ode sensor remove that too
        std::vector<ODETactileSensor*>& odesensors = _odeBodyToSensor[odeBody->getBodyID()];

        if(odesensors.size()==0)
            return;

        std::vector<ODETactileSensor*> nodesensors;
        for(size_t i=0;i<odesensors.size();i++){
            if(odesensors[i]!=tsensor)
                nodesensors.push_back(odesensors[i]);
        }

        _odeBodyToSensor[odeBody->getBodyID()] = nodesensors;

        std::vector<ODETactileSensor*> newOdeSensors;
        BOOST_FOREACH(ODETactileSensor* oldsen , _odeSensors){
            if(tsensor != oldsen )
                newOdeSensors.push_back(oldsen);
        }

        if(newOdeSensors.size()==0)
            _odeBodyToSensor.erase( _odeBodyToSensor.find(odeBody->getBodyID()) );

        _odeSensors = newOdeSensors;

        delete tsensor;
    }

};


using namespace rw::proximity;

bool ODESimulator::detectCollisionsRW(rw::kinematics::State& state, bool onlyTestPenetration){
    //
    //std::cout << "detectCollisionsRW" << onlyTestPenetration << std::endl;
    ProximityFilter::Ptr filter = _bpstrategy->update(state);
    FKTable fk(state);
    ProximityStrategyData data;

    // next we query the BP filter for framepairs that are possibly in collision
    while( !filter->isEmpty() ){
        const FramePair& pair = filter->frontAndPop();
        RW_DEBUGS(pair.first->getName() << " -- " << pair.second->getName());

        // and lastly we use the dispatcher to find the strategy the
        // is required to compute the narrowphase collision
        const ProximityModel::Ptr &a = _frameToModels[*pair.first];
        const ProximityModel::Ptr &b = _frameToModels[*pair.second];
        if(a==NULL || b==NULL)
            continue;

        // now find the "body" frame belonging to the frames
        //std::cout << "bodies" << std::endl;
        ODEBody *a_data = _rwFrameToODEBody[pair.first];
        ODEBody *b_data = _rwFrameToODEBody[pair.second];
        if(a_data==NULL || b_data==NULL)
            continue;

        //std::cout << "geoms" << std::endl;
        dGeomID a_geom = _frameToOdeGeoms[pair.first];
        dGeomID b_geom = _frameToOdeGeoms[pair.second];
        if(a_geom==NULL || b_geom==NULL)
            continue;
        /*
        if(a_geom == b_geom)
            continue;

        ODEBody *a_data;
        if( a_body==NULL ) {
            if(a_geom==NULL){
                continue;
            }
            a_data = (ODEBody*) dGeomGetData(a_geom);
        } else {
            a_data = (ODEBody*) dBodyGetData(a_body);
        }
        RW_DEBUGS("- get data2")
        ODEBody *b_data;
        if( b_body==NULL ) {
            if(b_geom==NULL)
                continue;
            b_data = (ODEBody*) dGeomGetData(b_geom);
        } else {
            b_data = (ODEBody*) dBodyGetData(b_body);
        }
        */

        if(a_data  == b_data)
            continue;

        //const Transform3D<> aT = fk.get(*pair.first);
        //const Transform3D<> bT = fk.get(*pair.second);

        //std::cout << pair.first->getName() << " " << pair.second->getName() << std::endl;
        //const Transform3D<> aT = ODEUtil::getODEGeomT3D(a_geom);
        //const Transform3D<> bT = ODEUtil::getODEGeomT3D(b_geom);
        const Transform3D<> aT = a_data->getTransform();
        const Transform3D<> bT = b_data->getTransform();

        // first make standard collision detection, if in collision then compute all contacts from dist query
        RW_DEBUGS( pair.first->getName() << " <--> " << pair.second->getName());
        //std::cout << pair.first->getName() << " <--> " << pair.second->getName() << std::endl;
        //std::cout << "bT" << bT << std::endl;
        //const double MAX_PENETRATION  = 0.0002;
        //const double MAX_SEP_DISTANCE = 0.0002;

        MultiDistanceResult *res;

        if(onlyTestPenetration){

            data.setCollisionQueryType(FirstContact);
            bool collides = _narrowStrategy->inCollision(a, aT, b, bT, data);
            if(collides){
                return true;
            }
            continue;
        }

        /*
        // there is a collision and we need to find the correct contact points/normals.
        BodyBodyContact& bcon = _lastNonCollidingTransform[pair];
        if( bcon.firstContact ){
            std::cout << "First contact " << std::endl;
            data.setCollisionQueryType(FirstContact);
            bool collides = _narrowStrategy->inCollision(a, aT, b, bT, data);

            if( !collides ){
                // save the last transform such that we are able to find the normal in case the bodies penetrate in the next timestep.
                _lastNonCollidingTransform[pair] = BodyBodyContact(aT, bT);
                return;
            }

            std::cout << "_aT" << bcon.aT << std::endl;
            std::cout << "_bT" << bcon.bT << std::endl;

            bcon.firstContact = false;
        } else {
            // update the contact normal and transforms
            // we use the old contact normal to project the poses of the object into a non-penetrating stance

            // TODO: here we should try and resolve the point of contact.
            bcon.aT = aT; // translated in the negative normal direction
            bcon.aT.P() += MAX_SEP_DISTANCE*bcon.cnormal;
            bcon.bT = bT;
            std::cout << "_aT" << bcon.aT << std::endl;
            std::cout << "_bT" << bcon.bT << std::endl;
            std::cout << "ASSERT if in collision" << std::endl;
            RW_ASSERT(_narrowStrategy->inCollision(a, bcon.aT, b, bcon.bT, data)==false);
            std::cout << "after ASSERT if in collision" << std::endl;
        }



        // calculate the contacts
        data.setCollisionQueryType(AllContacts);
        res = &_narrowStrategy->distances(a, bcon.aT, b, bcon.bT, MAX_SEP_DISTANCE, data);
        */

        // TODO: if the object is a soft object then we need to add more contacts
        bool softcontact = true;
        double softlayer = 0.0;
        if( softcontact ){
            // change MAX_SEP_DISTANCE
            softlayer = 0.0008;
        }

        data.setCollisionQueryType(AllContacts);
        res = &_narrowStrategy->distances(a, aT, b, bT, _maxSepDistance+softlayer, data);

        // create all contacts
        size_t numc = res->distances.size();
        if(_rwcontacts.size()<numc){
            _rwcontacts.resize(numc);
            _contacts.resize(numc);
        }
        int ni = 0;
        //std::cout << "--- {";
        //for(int i=0;i<numc;i++){
        //    std::cout << res.distances[i] << ", ";
        //}
        //std::cout << "}"<< std::endl;

        for(size_t i=0;i<numc;i++){

            if(res->distances[i]<0.00000001)
                continue;

            dContact &con = _contacts[ni];
            Vector3D<> p1 = aT * res->p1s[i];
            Vector3D<> p2 = aT * res->p2s[i];
            double len = (p2-p1).norm2();
            Vector3D<> n = (p2-p1)/(-len);
            //std::cout << "n: " << n << "\n";
            Vector3D<> p = n*(res->distances[i]/2) + p1;

            ODEUtil::toODEVector(n, con.geom.normal);
            ODEUtil::toODEVector(p, con.geom.pos);

            if( softcontact ){
                // scale the distances to fit into MAX_SEP_DISTANCE
                //std::cout << res->distances[i] << ";" << res->distances[i]*_maxSepDistance/(_maxSepDistance+softlayer) << std::endl;
                res->distances[i] *= _maxSepDistance/(_maxSepDistance+softlayer);
                //res->distances[i] *= _maxSepDistance/_maxSepDistance;
            }
            //double penDepth = MAX_SEP_DISTANCE-(res->distances[i]+(MAX_SEP_DISTANCE-MAX_PENETRATION));
            double penDepth = _maxAllowedPenetration - res->distances[i];
            con.geom.depth = penDepth;

            con.geom.g1 = a_geom;
            con.geom.g2 = b_geom;

            // friction direction between the bodies ...
            // Not necesary to calculate, unless we need explicit control
            ni++;
        }
        numc = ni;

        //bcon.cnormal =
            addContacts(numc, a_data, b_data, pair.first, pair.second);
        res->clear();
        // update the contact normal using the manifolds
    }

    if(onlyTestPenetration){
        return false;
    }
    return false;
}

void ODESimulator::attach(rwsim::dynamics::Body::Ptr b1, rwsim::dynamics::Body::Ptr b2){
    ODEBody *ob1 = _rwFrameToODEBody[ b1->getBodyFrame() ];
    ODEBody *ob2 = _rwFrameToODEBody[ b2->getBodyFrame() ];

    if(ob1==NULL )
        RW_THROW("Body b1 is not part of the simulation! "<< b1->getName());
    if(ob2==NULL )
        RW_THROW("Body b2 is not part of the simulation! "<< b2->getName());

    if(_attachConstraints.find(std::make_pair(b1->getBodyFrame(),b2->getBodyFrame()))!=_attachConstraints.end()){
        RW_THROW("Joints are allready attached!");
    }
    if(_attachConstraints.find(std::make_pair(b2->getBodyFrame(),b1->getBodyFrame()))!=_attachConstraints.end()){
        RW_THROW("Joints are allready attached!");
    }

    dJointID fjoint = dJointCreateFixed(_worldId, 0 );
    dJointAttach(fjoint, ob1->getBodyID(), ob2->getBodyID());
    _attachConstraints[std::make_pair(b1->getBodyFrame(),b2->getBodyFrame())] = fjoint;
    dJointSetFixed(fjoint);
}

void ODESimulator::detach(rwsim::dynamics::Body::Ptr b1, rwsim::dynamics::Body::Ptr b2){
    ODEBody *ob1 = _rwFrameToODEBody[ b1->getBodyFrame() ];
    ODEBody *ob2 = _rwFrameToODEBody[ b2->getBodyFrame() ];

    if(ob1==NULL )
        RW_THROW("Body b1 is not part of the simulation! "<< b1->getName());
    if(ob2==NULL )
        RW_THROW("Body b2 is not part of the simulation! "<< b2->getName());

    if(_attachConstraints.find(std::make_pair(b1->getBodyFrame(),b2->getBodyFrame()))!=_attachConstraints.end()){
        dJointID fjoint = _attachConstraints[std::make_pair(b1->getBodyFrame(),b2->getBodyFrame())];
        dJointDestroy(fjoint);
        _attachConstraints.erase(std::make_pair(b1->getBodyFrame(),b2->getBodyFrame()));
        return;
    }
    if(_attachConstraints.find(std::make_pair(b2->getBodyFrame(),b1->getBodyFrame()))!=_attachConstraints.end()){
        dJointID fjoint = _attachConstraints[std::make_pair(b2->getBodyFrame(),b1->getBodyFrame())];
        dJointDestroy(fjoint);
        _attachConstraints.erase(std::make_pair(b2->getBodyFrame(),b1->getBodyFrame()));
        return;
    }
    RW_THROW("There are no attachments between body b1 and body b2!");
}


void ODESimulator::handleCollisionBetween(dGeomID o1, dGeomID o2)
{
	RW_DEBUGS("********************handleCollisionBetween ************************** ")
    // Create an array of dContact objects to hold the contact joints
    dBodyID b1 = dGeomGetBody(o1);
    dBodyID b2 = dGeomGetBody(o2);
    //std::cout << "Collision: " << b1 << " " << b2 << std::endl;
    if (b1 && b2 && dAreConnectedExcluding (b1,b2,dJointTypeContact)){
    	RW_DEBUGS(b1 <<"&&" << b2 <<"&&" << dAreConnectedExcluding (b1,b2,dJointTypeContact));
        return;
    }

    ODEBody *dataB1;
    if( b1==NULL ) {
        dataB1 = (ODEBody*) dGeomGetData(o1);
    } else {
        dataB1 = (ODEBody*) dBodyGetData(b1);
    }
    //RW_DEBUGS("- get data2")
    ODEBody *dataB2;
    if( b2==NULL ) {
        dataB2 = (ODEBody*) dGeomGetData(o2);
    } else {
        dataB2 = (ODEBody*) dBodyGetData(b2);
    }

    if(dataB1 == NULL || dataB2==NULL ){
        //if(dataB1!=NULL)
        //	std::cout << "b1: " << dataB1->getFrame()->getName() << std::endl;
        //if(dataB2!=NULL)
        //	std::cout << "b2: " << dataB2->getFrame()->getName() << std::endl;
    	return;
    }

    RW_DEBUGS("- get data3 " << dataB1 << " " << dataB2)
    Frame *frameB1 = dataB1->getFrame();
    Frame *frameB2 = dataB2->getFrame();

    if(frameB1 == frameB2)
        return;
    RW_DEBUGS("- check enabled Map")
    // if any of the bodies are disabled then discard them
    if( _enabledMap[*frameB1]==0 || _enabledMap[*frameB2]==0 )
        return;
    RW_DEBUGS("- check exclude Map")
    // check if the frames are in the exclude list, if they are then return
    rw::kinematics::FramePair pair(frameB1,frameB2);
    if( _excludeMap.has( pair ) )
        return;
    //std::cout << frameB1->getName() << " " << frameB2->getName() << std::endl;
    // update the
    //std::vector<ContactManifold> &manifolds = _manifolds[pair];
    //BOOST_FOREACH(ContactManifold &manifold, manifolds){
        //manifold.update(); // TODO: use transform between objects to update manifold
    //}
    RW_DEBUGS("- do collide")
    int numc;
    // do the actual collision check

    numc = dCollide(o1, o2,
                        (_contacts.size()-1) , &_contacts[0].geom,
                        sizeof(dContact));

    if( numc >= (int)_contacts.size()-1 ){
        numc = _contacts.size()-2;
        RW_WARN( "------- Too small collision buffer ------" );
    }

    addContacts(numc, dataB1, dataB2, frameB1, frameB2);
}

rw::math::Vector3D<> ODESimulator::addContacts(int numc, ODEBody* dataB1, ODEBody* dataB2, Frame *f1, Frame *f2){
    RW_DEBUGS("Add Contacts");
    if(numc==0){
        RW_DEBUGS("No collisions detected!");
        return Vector3D<>(0,0,0);
    }

    //RW_DEBUGS("- detected: " << frameB1->getName() << " " << frameB2->getName());

    // check if any of the bodies are actually sensors
    std::vector<ODETactileSensor*>& odeSensorb1 = _odeBodyToSensor[dataB1->getBodyID()];
    std::vector<ODETactileSensor*>& odeSensorb2 = _odeBodyToSensor[dataB2->getBodyID()];

    // perform contact clustering
    //double threshold = std::min(dataB1->getCRThres(), dataB2->getCRThres());
    //std::cout << "Numc: " << numc << std::endl;



    int j=0;
    for(int i=0;i<numc;i++){
        ContactPoint &point = _rwcontacts[j];
        const dContact &con = _contacts[i];

        //if(con.geom.depth>0.01)
        //    continue;

        point.n = normalize( toVector3D(con.geom.normal) );
        point.p = toVector3D(con.geom.pos);
        point.penetration = con.geom.depth;
        point.userdata = (void*) &(_contacts[i]);
        RW_DEBUGS("-- Depth/Pos  : " << con.geom.depth << " " << printArray(con.geom.normal,3));
        RW_DEBUGS("-- Depth/Pos p: " << point.penetration << " p:" << point.p << " n:"<< point.n);

        //_allcontacts.push_back(point);
        j++;
        //std::cout << "n: " << point.n << " p:" << point.p << " dist: "
		//		  << MetricUtil::dist2(point.p,_rwcontacts[std::max(0,i-1)].p) << std::endl;
    }
    numc = j;
    if((int)_srcIdx.size()<numc)
        _srcIdx.resize(numc);
    if((int)_dstIdx.size()<numc)
        _dstIdx.resize(numc);

    int fnumc = ContactCluster::normalThresClustering(
									&_rwcontacts[0], numc,
                                    &_srcIdx[0], &_dstIdx[0],
                                    &_rwClusteredContacts[0],
                                    10*Deg2Rad);

    //std::cout << "Threshold: " << threshold << " numc:" << numc << " fnumc:" << fnumc << std::endl;
    RW_DEBUGS(" numc:" << numc << " fnumc:" << fnumc);
    //RW_DEBUGS("Nr of average contact points in cluster: " << numc/((double)fnumc));

    // we create a manyfold per cluster, and only use the most significant points

    std::vector<ContactPoint> &src = _rwcontacts;
    std::vector<ContactPoint> &dst = _rwClusteredContacts;

    //std::cout << "Number of contact clusters: " << fnumc << std::endl;
    /*
    for(int i=0;i<fnumc;i++)
        std::cout << "i:" << i << " -> " << dst[i].penetration << " " << dst[i].p<< " " << dst[i].n << std::endl;

    std::cout << "Dst idx array: " << std::endl;
    for(int i=0;i<_dstIdx.size();i++)
        std::cout << "i:" << i << " -> " << _dstIdx[i] << " " << src[_dstIdx[i] ].p<< " " << src[_dstIdx[i] ].n << std::endl;

    std::cout << "Src idx array: " << std::endl;
    for(int i=0;i<_srcIdx.size();i++)
        std::cout << "i:" << i << " -> " << _srcIdx[i] << std::endl;
    */
    // test manifold functionality
    std::vector< OBRManifold > manifolds;
    // for each cluster we fit a manifold
    for(int i=0;i<fnumc;i++){
        int idxFrom = _srcIdx[i];
        const int idxTo = _srcIdx[i+1];
        // locate the manifold that idxFrom is located in
        OBRManifold manifold(13*Deg2Rad,0.2);

        //std::cout << "Adding clustered points to manifold!" << std::endl;

        for(;idxFrom<idxTo; idxFrom++){
            ContactPoint &point = src[_dstIdx[idxFrom]];
            //std::cout << point.p << std::endl;

            if( !manifold.addPoint(point) ){
                // hmm, create a new manifold for this point
            }
        }
        manifolds.push_back(manifold);
    }
    RW_DEBUGS("Fibnd Contacts from manifolds");
    // run through all manifolds and get the contact points that will be used.
    int contactIdx = 0;
    rw::math::Vector3D<> contactNormalAvg(0,0,0);
    BOOST_FOREACH(OBRManifold& obr, manifolds){
        contactNormalAvg += obr.getNormal();
        int nrContacts = obr.getNrOfContacts();
        //std::cout << "Manifold: " << nrContacts << ";" << std::endl;
        for(int j=0;j<nrContacts; j++){
            _allcontacts.push_back( obr.getContact(j) );
            dst[contactIdx] = obr.getContact(j);
            contactIdx++;
        }
    }
    RW_DEBUGS("Add material map");
    // get the friction data and contact settings
    dContact conSettings;
    RW_ASSERT(_odeMaterialMap);
    _odeMaterialMap->setContactProperties(conSettings, dataB1, dataB2);

    // TODO: collisionMargin should also be specified per object
    //double collisionMargin = _dwc->getCollisionMargin();

    RW_DEBUGS("- Filtered contacts: " << numc << " --> " << fnumc);


    //std::cout << "- Filtered contacts: " << numc << " --> " << fnumc << std::endl;
    //Frame *world = _dwc->getWorkcell()->getWorldFrame();
    _nrOfCon = fnumc;
    std::vector<dJointFeedback*> feedbacks;
    std::vector<dContactGeom> feedbackContacts;
    bool enableFeedback = false;
    if(odeSensorb1.size()>0 || odeSensorb2.size()>0){
        //if( (dataB1->getType()!=ODEBody::FIXED) && (dataB2->getType()!=ODEBody::FIXED) ){
        //    if( (f1 != world) && (f2!=world) ){
                enableFeedback = true;
                //std::cout << "- detected: " << frameB1->getName() << " " << frameB2->getName() << std::endl;
        //    }
        //}
    }
    Vector3D<> cNormal(0,0,0);
    double maxPenetration = 0;
    // Run through all contacts and define contact constraints between them
    //std::vector<ContactPoint> &contactPointList = _rwClusteredContacts; int num = numc;
    std::vector<ContactPoint> &contactPointList = dst; int num = contactIdx;

    /*
    if( (dataB2->getFrame()->getName() == "SuctionGripper.Joint4" ||
            dataB1->getFrame()->getName() == "SuctionGripper.Joint4")
                    ){
        // adding suction force
        //con.geom.depth = -point->penetration;
        std::cout << "---------------------------------------------------" << std::endl;
        contactPointList[num] = contactPointList[0];
        contactPointList[num].n = -contactPointList[num].n;
        contactPointList[num].penetration = 0.0001;
        num++;
    }
    */

    for (int i = 0; i < num; i++) {
        ContactPoint *point = &contactPointList[i];
        point->n = normalize(point->n);
        dContact &con = *((dContact*)point->userdata);

        //std::cout << point->penetration << " " << point->n << "  " << point->p << "  " << std::endl;
        cNormal += point->n;
        double rwnlength = MetricUtil::norm2(point->n);
        if((0.9>rwnlength) || (rwnlength>1.1)){
        	//std::cout <<  "\n\n Normal not normalized _0_ !\n"<<std::endl;
        	continue;
        }
        ODEUtil::toODEVector(point->n, con.geom.normal);
        ODEUtil::toODEVector(point->p, con.geom.pos);

        //double odenlength = sqrt( con.geom.normal[0]*con.geom.normal[0] +
        //                          con.geom.normal[1]*con.geom.normal[1] +
        //                          con.geom.normal[2]*con.geom.normal[2] );


        //if( (0.9>odenlength) || (odenlength>1.1) )
        //	std::cout <<  "\n\n Normal not normalized _1_ !\n"<<std::endl;

        con.geom.depth = point->penetration;

        maxPenetration = std::max(point->penetration, maxPenetration);
        _maxPenetration = std::max(point->penetration, _maxPenetration);

        //if( con.geom.depth <= 0){
        //    continue;
        //}

        RW_DEBUGS("-- Depth/Pos  : " << con.geom.depth << " " << printArray(con.geom.pos,3));
        //RW_DEBUGS("-- Depth/Pos p: " << point.penetration << " " << point.p );

        // currently we use the best possible ode approximation to Coloumb friction
        con.surface = conSettings.surface;

        RW_DEBUGS("-- create contact ");
        dJointID c = dJointCreateContact (_worldId,_contactGroupId,&con);
        RW_DEBUGS("-- attach bodies");
        dJointAttach (c, dataB1->getBodyID(), dataB2->getBodyID() );
        RW_DEBUGS("--enable feedback");
        // We can only have one Joint feedback per joint so the sensors will have to share
        if( enableFeedback ){
            RW_ASSERT( ((size_t)_nextFeedbackIdx)<_sensorFeedbacks.size());
            dJointFeedback *feedback = &_sensorFeedbacks[_nextFeedbackIdx];
            _nextFeedbackIdx++;
            feedbacks.push_back(feedback);
            feedbackContacts.push_back(con.geom);
            dJointSetFeedback( c, feedback );
        }
    }
    //std::cout << "_maxPenetration: " << _maxPenetration << " meter" << std::endl;
    if(enableFeedback && odeSensorb1.size()>0){
    	//std::cout << "----------- ADD FEEDBACK\n";
        BOOST_FOREACH(ODETactileSensor* sen, odeSensorb1){
            sen->addFeedback(feedbacks, feedbackContacts, dataB2->getRwBody(), 0);
        }
        //odeSensorb1->setContacts(result,wTa,wTb);
    }
    if(enableFeedback && odeSensorb2.size()>0){
    	//std::cout << "----------- ADD FEEDBACK\n";
        BOOST_FOREACH(ODETactileSensor* sen, odeSensorb2){
    	        sen->addFeedback(feedbacks, feedbackContacts, dataB1->getRwBody(), 1);
        }
        //odeSensorb2->setContacts(result,wTa,wTb);
    }

    // if either body was a sensor then find all contacts within some small threshold
    /*
    if( enableFeedback ){
        MultiDistanceResult result;
        Transform3D<> wTa, wTb;
        if( !((dataB2->getType()==ODEBody::FIXED) || (dataB1->getType()==ODEBody::FIXED)) ){
            //std::cout << "MAX PENETRATION: " << maxPenetration << std::endl;
            Vector3D<> normal = normalize(cNormal);
            wTa = dataB1->getTransform();
            wTb = dataB2->getTransform();
            wTb.P() -= normal*(maxPenetration+0.001);
            bool res = _narrowStrategy->getDistances(result,
                                          frameB1,wTa,
                                          frameB2,wTb,
                                          0.002+maxPenetration);

            for(int i=0;i<result.distances.size();i++){
                ContactPoint cp;
                cp.n = wTa*result.p2s[i]-wTa*result.p1s[i];
                cp.p = wTa*result.p1s[i];
                _allcontacts.push_back(cp);
            }
        }
        if(odeSensorb1){
            odeSensorb1->addFeedback(feedbacks, feedbackContacts, 0);
            odeSensorb1->setContacts(result,wTa,wTb);
        }
        if(odeSensorb2){
            odeSensorb2->addFeedback(feedbacks, feedbackContacts, 1);
            odeSensorb2->setContacts(result,wTa,wTb);
        }
    }
    */

    //std::cout << "************ COL DONE *****************" << std::endl;

    return normalize( contactNormalAvg);
}

void ODESimulator::addContacts(std::vector<dContact>& contacts, size_t nr_con, ODEBody* dataB1, ODEBody* dataB2){

    RW_ASSERT(nr_con<=contacts.size());
    if(dataB1==NULL || dataB2==NULL){
        RW_DEBUGS("Bodies are NULL");
        RW_THROW("Bodies are NULL");
    }

    std::vector<ODETactileSensor*>& odeSensorb1 = _odeBodyToSensor[dataB1->getBodyID()];
    std::vector<ODETactileSensor*>& odeSensorb2 = _odeBodyToSensor[dataB2->getBodyID()];


    std::vector<dJointFeedback*> feedbacks;
    std::vector<dContactGeom> feedbackContacts;
    bool enableFeedback = false;
    if(odeSensorb1.size()>0 || odeSensorb2.size()>0){
        enableFeedback = true;
    }
    for (size_t i = 0; i < nr_con; i++) {
        dContact con = contacts[i];
        // add to all contacts
        ContactPoint point;
        point.n = normalize( toVector3D(con.geom.normal) );
        point.p = toVector3D(con.geom.pos);
        point.penetration = con.geom.depth;
        _allcontacts.push_back(point);


        RW_DEBUGS("-- create contact ");
        dJointID c = dJointCreateContact (_worldId,_contactGroupId,&con);
        RW_DEBUGS("-- attach bodies");
        dJointAttach (c, dataB1->getBodyID(), dataB2->getBodyID() );
        RW_DEBUGS("--enable feedback");
        // We can only have one Joint feedback per joint so the sensors will have to share
        if( enableFeedback ){
            RW_ASSERT( ((size_t)_nextFeedbackIdx)<_sensorFeedbacks.size());
            dJointFeedback *feedback = &_sensorFeedbacks[_nextFeedbackIdx];
            _nextFeedbackIdx++;
            feedbacks.push_back(feedback);
            feedbackContacts.push_back(con.geom);
            dJointSetFeedback( c, feedback );
        }
    }

    //std::cout << "_maxPenetration: " << _maxPenetration << " meter" << std::endl;
    if(enableFeedback && odeSensorb1.size()>0){
        //std::cout << "----------- ADD FEEDBACK\n";
        BOOST_FOREACH(ODETactileSensor* sen, odeSensorb1){
            sen->addFeedback(feedbacks, feedbackContacts, dataB2->getRwBody(), 0);
        }
        //odeSensorb1->setContacts(result,wTa,wTb);
    }
    if(enableFeedback && odeSensorb2.size()>0){
        //std::cout << "----------- ADD FEEDBACK\n";
        BOOST_FOREACH(ODETactileSensor* sen, odeSensorb2){
                sen->addFeedback(feedbacks, feedbackContacts, dataB1->getRwBody(), 1);
        }
        //odeSensorb2->setContacts(result,wTa,wTb);
    }


}


void ODESimulator::resetScene(rw::kinematics::State& state)
{
    if(isInErrorGlobal){
        // delete world and reinitialize everything

    }

	isInErrorGlobal = false;
	_time = 0.0;

	// first run through all rigid bodies and set the velocity and force to zero
	RW_DEBUGS("- Resetting bodies: " << _bodies.size());
	BOOST_FOREACH(dBodyID body, _allbodies){
	    dBodySetLinearVel  (body, 0, 0, 0);
	    dBodySetAngularVel (body, 0, 0, 0);
	    dBodySetForce  (body, 0, 0, 0);
	    dBodySetTorque (body, 0, 0, 0);
	}

	BOOST_FOREACH(ODEBody* body, _odeBodies){
	    body->reset(state);
	}

	// next the position need be reset to what is in state
	// run through all rw bodies and set the body position accordingly
	RW_DEBUGS("- Resetting sensors: " << _odeSensors.size());
	BOOST_FOREACH(ODETactileSensor* sensor, _odeSensors){
	    sensor->clear();
	}

	RW_DEBUGS("- Resetting devices: " << _odeDevices.size());
	// run through all devices and set the rigid bodies accoringly
	BOOST_FOREACH(ODEDevice *device, _odeDevices){
	    device->reset(state);
	}
	RW_DEBUGS("Finished reset!!");
}

void ODESimulator::exitPhysics()
{

}
