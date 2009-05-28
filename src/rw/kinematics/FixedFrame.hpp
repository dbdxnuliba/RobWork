/*********************************************************************
 * RobWork Version 0.3
 * Copyright (C) Robotics Group, Maersk Institute, University of Southern
 * Denmark.
 *
 * RobWork can be used, modified and redistributed freely.
 * RobWork is distributed WITHOUT ANY WARRANTY; including the implied
 * warranty of merchantability, fitness for a particular purpose and
 * guarantee of future releases, maintenance and bug fixes. The authors
 * has no responsibility of continuous development, maintenance, support
 * and insurance of backwards capability in the future.
 *
 * Notice that RobWork uses 3rd party software for which the RobWork
 * license does not apply. Consult the packages in the ext/ directory
 * for detailed information about these packages.
 *********************************************************************/

#ifndef RW_KINEMATICS_FIXEDFRAME_HPP
#define RW_KINEMATICS_FIXEDFRAME_HPP

/**
 * @file FixedFrame.hpp
 */

#include "Frame.hpp"

namespace rw { namespace kinematics {

    /** @addtogroup kinematics */
    /*@{*/

    /**
     * @brief FixedFrame is a frame for which the transform relative to the
     * parent is constant.
     *
     * A fixed frame can for example be used for attaching a camera, say, with a
     * fixed offset relative to the tool.
     */
    class FixedFrame: public Frame
    {
    public:
        /**
         * @brief A frame fixed to its parent with a constant relative transform
         * of \b transform.
         *
         * @param name [in] The name of the frame.
         * @param transform [in] The transform with which to attach the frame.
         */
        FixedFrame(const std::string& name,
                   const math::Transform3D<>& transform);


    private:
        void doMultiplyTransform(const math::Transform3D<>& parent,
                                 const State& state,
                                 math::Transform3D<>& result) const;

        math::Transform3D<> doGetTransform(const State& state) const;

    private:
        math::Transform3D<> _transform;
    };

    /*@}*/

}} // end namespaces

#endif // end include guard
