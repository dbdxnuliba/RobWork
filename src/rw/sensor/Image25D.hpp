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


#ifndef RW_SENSOR_IMAGE25D_HPP
#define RW_SENSOR_IMAGE25D_HPP

#include <vector>

namespace rw {
namespace sensor {

/** @addtogroup sensor */
/* @{ */

/**
 * @brief Data structure for 2.5D range data.
 */
class Image25D {

public:
    /**
     * @brief default constructor
     */
    Image25D():
        _width(0),
        _height(0),
        _depthData(new std::vector<float>())
    {};

    /**
     * @brief constructor
     * @param width [in] width of the image
     * @param height [in] height of the image
     */
    Image25D(int width, int height):
        _width(width),
        _height(height),
        _depthData(new std::vector<float>())
    {}

    /**
     * @brief constructor
     * @param imgData [in] char pointer that points to an array of chars with
     * length width*height*(bitsPerPixel/8)
     * @param width [in] width of the image
     * @param height [in] height of the image
     */
    Image25D(std::vector<float> *imgData,int width,int height):
        _width(width),
        _height(height),
        _depthData(imgData)
    {}

    /**
     * @brief destructor
     *
     */
    virtual ~Image25D();

    /**
     * @brief resizes the current image.
     * @param width
     * @param height
     */
    void resize(int width, int height){
        _width = width;
        _height = height;
        _depthData->resize(_width*_height);
    }

    /**
     * @brief returns a char pointer to the image data
     * @return char pointer to the image data
     */
    std::vector<float>& getImageData(){ return *_depthData; };

    /**
     * @brief returns a char pointer to the image data
     * @return const char pointer to the image data
     */
    const std::vector<float>& getImageData() const{ return *_depthData; };

    /**
     * @brief returns the width of this image
     * @return image width
     */
    unsigned int getWidth() const { return _width;};

    /**
     * @brief returns the height of this image
     * @return image height
     */
    unsigned int getHeight() const { return _height;};

private:
    unsigned int _width, _height;

protected:
    /**
     * @brief Float array of image data
     */
    std::vector<float> *_depthData;
};

/* @} */

}
}

#endif /*RW_SENSOR_IMAGE3D_HPP_*/
