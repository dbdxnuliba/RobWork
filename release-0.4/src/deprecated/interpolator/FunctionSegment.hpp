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


#ifndef rw_interpolator_FunctionSegment_HPP
#define rw_interpolator_FunctionSegment_HPP

/**
 * @file FunctionSegment.hpp
 */

#include <rw/math/Q.hpp>
namespace rw { namespace interpolator {

    /** @addtogroup interpolator */
    /*@{*/

    /**
     * @brief This Interface represents a pathsegment the most basic part of a
     * path description. The pathsegment is described by a function and an
     * interval [0;length]. Because the segment is described alone by a function,
     * random access methods are very efficient.
     *
     * @f$ f(t)=\bf{X(t)} @f$
     *
     * @note The length of the FunctionSegment is user defined and thereby has no predefined
     * real physical meaning or relation to the FunctionSegment. The length of all function
     * segments should default to 1, if a user finds it convinient to scale the function
     * segments a length !=1 can be defined.
     */
    class FunctionSegment
    {
    protected:
    	/**
    	 * @brief the length of the function segment
    	 */
        double _length;
        
        /**
         * brief The interval of the FunctionSegment
         */        
        //std::pair<double,double> _interval;

        /**
         * @brief Default constructor
         * @param length [in] The length of the segment
         */
        FunctionSegment(double length): 
            _length(length) 
        {            
        }

    public:

        /**
         * @brief Destroys object
         */
        virtual ~FunctionSegment() {}

        /**
         * @brief returns a vector X given some parametized value t
         * @param t [in] a double that specifies at what time or place the segment the
         * vector X is to be returned. @f$ t\in [0;length] @f$
         * @return a boost vector that is the result of the function
         */
        virtual rw::math::Q getX(double t) const = 0;

        /**
         * @brief returns the derivative or the slope of the function at the
         * parametized value t.
         * @param t [in] a parametized value @f$ t\in [0;length] @f$
         * @return a boost vector that represents the derived value in t
         */
        virtual rw::math::Q getXd(double t) const = 0;

        /**
         * @brief returns a vector X given some parametized value t
         * @param t [in] a double that specifies at what time or place the segment the
         * vector X is to be returned. @f$ t\in [0;length] @f$
         * @return a boost vector that is the result of the function
         */
        virtual rw::math::Q getXdd(double t) const = 0;


        /**
         * @brief Returns the length of the segment
         * @return the length 
         */
        double getLength() const {
            return _length;
        }

    private:
        FunctionSegment(const FunctionSegment&);
        FunctionSegment& operator=(const FunctionSegment&);
    };
    /*@}*/

}} // end namespaces

#endif // end include guard
