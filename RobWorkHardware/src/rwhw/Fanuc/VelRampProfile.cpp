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

#include "VelRampProfile.hpp"

#include <cmath>

using namespace rwhw::fanuc;
using namespace rw::math;

VelRampProfile::VelRampProfile(
    const std::vector<Range>& poslimits,
    const std::vector<Range>& vellimits,
    const std::vector<Range>& acclimits)
    :
    _poslimits(poslimits),
    _vellimits(vellimits),
    _acclimits(acclimits)
{}

VelRampProfile::~VelRampProfile()
{}

Q VelRampProfile::getVelocity(
    const Q& goalVec,
    const Q& posVec,
    const Q& velVec,
    double dt) const
{
    const std::size_t dof = goalVec.size();
    Q qtarget = goalVec;
    Q qcurrent = posVec;
    Q vcurrent = velVec;
    Q qdot(dof);

    for (std::size_t i = 0; i < dof; i++) {
        double dist = qtarget(i) - qcurrent(i);

        if (fabs(dist) < 0.01) // round close to zero values to zero
            dist = 0;

        double sign = (dist < 0)?-1:1;
        double vdist = sqrt(fabs(2*dist*_acclimits[i].first))*sign*0.8;
        double vmin = std::max(vcurrent(i)+_acclimits[i].second*dt, _vellimits[i].second);
        double vmax = std::min(vcurrent(i)+_acclimits[i].first*dt, _vellimits[i].first);

        qdot(i) = std::min(vmax, std::max(vmin, vdist));
    }
    return qdot;
}

/*

Q velRes(_poslimits.size());
for(size_t i=0; i<_poslimits.size(); i++){
double q = posVec[i];
double dq = velVec[i];
double velLowLimit = _vellimits[i].first;
double velUppLimit = _vellimits[i].second;
double accLowLimit = _acclimits[i].first;
double accUppLimit = _acclimits[i].second;
double goal = goalVec[i];
double x =  std::min(std::max(goal, _poslimits[i].first), _poslimits[i].second)-q;
if (x>0) { //Needs a positive velocity
//For qmax
double j_x = round(sqrt(1-8*x/(h*h*accLowLimit))/2-1);
double q_end_x = (x+h*h*accLowLimit*(j_x*(j_x+1))/2)/(h*(j_x+1));
double q_max_x = q_end_x-j_x*accLowLimit*h;
double X = x-h*q_max_x;
double posmax = 0;
if (X>0) {
double j_X = round(sqrt(1.-8*X/(h*h*accLowLimit))/2.-1);
double q_end_X = (X+h*h*accLowLimit*(j_X*(j_X+1))/2)/(h*(j_X+1));
posmax = q_end_X-j_X*accLowLimit*h;
}
velRes[i] = std::min(dq+h*accUppLimit, std::min(velUppLimit, posmax));
} else if (x<0) { //Needs a negative velocity
x = -x;
double j_x = round(sqrt(1+8*x/(h*h*accUppLimit))/2-1);
double q_end_x = (-x+h*h*accUppLimit*(j_x*(j_x+1))/2)/(h*(j_x+1));
double q_min_x = q_end_x-j_x*accUppLimit*h;
double X = x+h*q_min_x;
double posmin = 0;
if (X>0) {
double j_X = round(sqrt(1+8*X/(h*h*accUppLimit))/2-1);
double q_end_X = (-X+h*h*accUppLimit*(j_X*(j_X+1))/2)/(h*(j_X+1));
posmin = q_end_X-j_X*accUppLimit*h;
}
velRes[i] = std::max(dq+h*accLowLimit, std::max(velLowLimit, posmin));
} else {
velRes[i] = 0;
}
}
return velRes;
}
*/

/*

double VelRampProfile::getVelocity(double goal, double q, double dq, double h) const {
double x =  std::min(std::max(goal, _poslimits(0)), _poslimits(1))-q;
if (x>0) { //Needs a positive velocity

//For qmax
double j_x = round(sqrt(1-8*x/(h*h*_acclimits(0)))/2-1);
double q_end_x = (x+h*h*_acclimits(0)*(j_x*(j_x+1))/2)/(h*(j_x+1));
double q_max_x = q_end_x-j_x*_acclimits(0)*h;
double X = x-h*q_max_x;
double posmax = 0;
if (X>0) {
double j_X = round(sqrt(1.-8*X/(h*h*_acclimits(0)))/2.-1);
double q_end_X = (X+h*h*_acclimits(0)*(j_X*(j_X+1))/2)/(h*(j_X+1));
posmax = q_end_X-j_X*_acclimits(0)*h;
}

return std::min(dq+h*_acclimits(1), std::min(_vellimits(1), posmax));
} else if (x<0) { //Needs a negative velocity

x = -x;
double j_x = round(sqrt(1+8*x/(h*h*_acclimits(1)))/2-1);
double q_end_x = (-x+h*h*_acclimits(1)*(j_x*(j_x+1))/2)/(h*(j_x+1));
double q_min_x = q_end_x-j_x*_acclimits(1)*h;
double X = x+h*q_min_x;
double posmin = 0;
if (X>0) {
double j_X = round(sqrt(1+8*X/(h*h*_acclimits(1)))/2-1);
double q_end_X = (-X+h*h*_acclimits(1)*(j_X*(j_X+1))/2)/(h*(j_X+1));
posmin = q_end_X-j_X*_acclimits(1)*h;
}
return std::max(dq+h*_acclimits(0), std::max(_vellimits(0), posmin));
}
else
return 0;

}*/
