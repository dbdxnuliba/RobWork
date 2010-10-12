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

#ifndef RW_GEOMETRY_GEOMETRY_HPP_
#define RW_GEOMETRY_GEOMETRY_HPP_

#include <rw/math/Transform3D.hpp>
#include <rw/kinematics/Frame.hpp>
#include <rw/common/Ptr.hpp>
#include "GeometryData.hpp"

namespace rw { namespace geometry {
	//! @addtogroup geometry
 	// @{

	/**
	 * @brief a class for representing a geometry that is scaled
	 * and transformed.
	 *
	 * Each geometry must have a unique ID. This is either auto
	 * generated or specified by user. The ids are used in collision
	 * detection and other algorithms where the object need an association
	 * other than its memory address.
	 */
	class Geometry {
	public:
        //! @brief smart pointer type to this class
        typedef rw::common::Ptr<Geometry> Ptr;

		/**
		 * @brief constructor - autogenerated id from geometry type.
		 * @param data
		 * @param scale
		 */
		Geometry(GeometryData::Ptr data, double scale=1.0);

		/**
		 * @brief constructor - autogenerated id from geometry type.
		 * @param data [in] pointer to geometry data
		 * @param t3d [in] transform
		 * @param scale [in] scaling factor
		 */
		Geometry(GeometryData::Ptr data,
				 const rw::math::Transform3D<>& t3d,
				 double scale=1.0);

		//! @brief destructor
		virtual ~Geometry();

		//! @brief gets the scaling factor applied when using this geometry
		double getScale() const {return _scale;}

		/**
		 * @brief set the scaling factor that should be applied to
		 * this geometry when used.
		 * @param scale [in] scale factor
		 */
		void setScale(double scale){_scale = scale;}

		//! @brief set transformation
		void setTransform(const rw::math::Transform3D<>& t3d){_transform = t3d;};

		//! @brief get transformation
		const rw::math::Transform3D<>& getTransform() const {return _transform;};

		//! @brief get geometry data
		GeometryData::Ptr getGeometryData(){return _data;};

		//! @brief get geometry data
		const GeometryData::Ptr getGeometryData() const {return _data;};

		//! @brief set transformation
		void setGeometryData(GeometryData::Ptr data){_data = data;};

		//! @brief get identifier of this geometry
		const std::string& getId() const {return _id; };

		//! @brief set identifier of this geometry
		void setId(const std::string& id) {_id = id; };


		//GeometryData* getBV(){return _bv;};
		//void setBV(GeometryData* bv){_bv = bv;};

	private:

		GeometryData::Ptr _data;
		//GeometryData *_bv;
		rw::math::Transform3D<> _transform;
		double _scale;
		std::string _id;

	};
	//! @brief Ptr to Geometry
	typedef rw::common::Ptr<Geometry> GeometryPtr;
	//! @}
}
}

#endif /* GEOMETRY_HPP_ */
