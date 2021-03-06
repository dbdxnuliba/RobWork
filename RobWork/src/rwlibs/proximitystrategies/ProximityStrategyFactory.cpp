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


#include "ProximityStrategyFactory.hpp"
#include <RobWorkConfig.hpp>
#include <rw/common/macros.hpp>
#include <rw/common/StringUtil.hpp>
#include <rw/proximity/rwstrategy/ProximityStrategyRW.hpp>

#ifdef RW_HAVE_PQP
#include "ProximityStrategyPQP.hpp"
#endif

#ifdef RW_HAVE_YAOBI
#include "ProximityStrategyYaobi.hpp"
#endif

#ifdef RW_HAVE_BULLET
#include "ProximityStrategyBullet.hpp"
#endif

namespace {
    const std::string RWStr("RWPROX");
    const std::string PQPStr("PQP");
    const std::string YAOBIStr("YAOBI");
    const std::string BulletStr("BULLET");
}

using namespace rwlibs::proximitystrategies;
using namespace rw::proximity;
using namespace rw;
using namespace rw::common;

rw::proximity::CollisionStrategy::Ptr ProximityStrategyFactory::makeDefaultCollisionStrategy() {

    #ifdef RW_HAVE_PQP
        return rw::common::ownedPtr<>(new ProximityStrategyPQP());
    #endif

    #ifdef RW_HAVE_YAOBI
        return rw::common::ownedPtr( new ProximityStrategyYaobi() );
    #endif

	#ifdef RW_HAVE_BULLET
        return rw::common::ownedPtr( new ProximityStrategyBullet() );
	#endif

    return rw::common::ownedPtr<>(new ProximityStrategyRW());
}

rw::proximity::CollisionStrategy::Ptr ProximityStrategyFactory::makeCollisionStrategy(const std::string& id){
    if(id==RWStr){
        return rw::common::ownedPtr<>(new ProximityStrategyRW());
    }
#ifdef RW_HAVE_PQP
    if(id==PQPStr){
        return rw::common::ownedPtr<>(new ProximityStrategyPQP());
    }
#endif

#ifdef RW_HAVE_YAOBI
    if(id==YAOBIStr){
        return rw::common::ownedPtr( new ProximityStrategyYaobi() );
    }
#endif

#ifdef RW_HAVE_BULLET
    if(id==BulletStr){
    	return rw::common::ownedPtr( new ProximityStrategyBullet() );
    }
#endif

    RW_THROW("No support for collision strategy with ID=" << StringUtil::quote(id));
    return NULL;
}

std::vector<std::string> ProximityStrategyFactory::getCollisionStrategyIDs(){
    std::vector<std::string> IDs;

#ifdef RW_HAVE_PQP
    IDs.push_back(PQPStr);
#endif

#ifdef RW_HAVE_YAOBI
    IDs.push_back(YAOBIStr);
#endif

#ifdef RW_HAVE_BULLET
    IDs.push_back(BulletStr);
#endif

    IDs.push_back(RWStr);

    return IDs;
}

std::vector<std::string> ProximityStrategyFactory::getDistanceStrategyIDs(){
    std::vector<std::string> IDs;

#ifdef RW_HAVE_PQP
    IDs.push_back(PQPStr);
#endif

    return IDs;
}

rw::proximity::DistanceStrategy::Ptr ProximityStrategyFactory::makeDefaultDistanceStrategy(){
#ifdef RW_HAVE_PQP
    return rw::common::ownedPtr<>(new ProximityStrategyPQP());
#endif

    RW_THROW("No default distance strategies available");
    return NULL;
}

rw::proximity::DistanceStrategy::Ptr ProximityStrategyFactory::makeDistanceStrategy(const std::string& id){
#ifdef RW_HAVE_PQP
    if(id==PQPStr){
        return rw::common::ownedPtr<>(new ProximityStrategyPQP());
    }
#endif

    RW_THROW("No support for distance strategy with ID=" << StringUtil::quote(id));
    return NULL;
}
