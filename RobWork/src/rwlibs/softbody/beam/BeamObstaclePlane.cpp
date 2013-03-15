/*
    Copyright 2013 <copyright holder> <email>

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/


#include "BeamObstaclePlane.hpp"

using namespace rw::math;
using namespace rwlibs::softbody;

BeamObstaclePlane::BeamObstaclePlane ( const rw::geometry::Plane& plane, const rw::math::Transform3D< double >& trans ) :
    _plane(plane),
    _trans(trans)
{

}


Transform3D< double > BeamObstaclePlane::getTransform ( void ) const {
    return _trans;
}



void BeamObstaclePlane::setTransform ( const Transform3D< double >& trans ) {
    _trans = trans;
}







double BeamObstaclePlane::get_thetaTCP ( const rw::math::Transform3D< double >& planeTbeam ) const {
    EAA<> eaa(planeTbeam.R());
    
    return eaa[2];
}


// returns yTCP in millimeters
double BeamObstaclePlane::get_yTCP ( const rw::math::Transform3D< double >& planeTbeam ) const {
    return planeTbeam.P()[1] * 1.0e3;
}


rw::math::Transform3D< double > BeamObstaclePlane::compute_planeTbeam ( const rw::math::Transform3D< double >& Tbeam ) {
    Transform3D<> planeTbeam = _trans; // init to Tplane
    
    // compute beam to plane transform    
    Transform3D<>::invMult(planeTbeam, Tbeam);
    
    return planeTbeam;
}


const rw::geometry::Plane& BeamObstaclePlane::getPlane ( void ) const {
    return _plane;
}
