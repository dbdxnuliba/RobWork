/*
   Copyright [yyyy] [name of copyright owner]

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

#include "ModRusselBeam.hpp"

#include "rw/math/Math.hpp"


#include "rwlibs/softbody/numerics/TrapMethod.hpp"
#include "rwlibs/softbody/numerics/FdUtil.hpp"

using namespace std;
using namespace rw::math;
using namespace boost::numeric::ublas;
using namespace rwlibs::softbody;


#include "rwlibs/softbody/numerics/InteriorPointOptimizer.hpp"
#include "BeamGeometry.hpp"
#include "rwlibs/softbody/numerics/Interpolation.hpp"
#include "rwlibs/softbody/util/psplot.h"

static int NFCALLS;


const char DIVIDER[] = "--------------------------------------------------------------------------------";

// #define RW_ENABLE_ASSERT

ModRusselBeam::ModRusselBeam (
    boost::shared_ptr< rwlibs::softbody::BeamGeometry > geomPtr,
    boost::shared_ptr< rwlibs::softbody::BeamObstaclePlane > obstaclePtr,
    int M,
    double accuracy,
    bool useNoUpwardConstraint,
    int nIntegralConstraints
) : _geomPtr ( geomPtr ), _obstaclePtr ( obstaclePtr ), _M ( M ), _accuracy ( accuracy ), _a ( M ), _da ( M ), _useNoUpwardConstraint ( useNoUpwardConstraint ), _nIntegralConstraints ( nIntegralConstraints ) {
    RW_ASSERT ( _M >= 2 );
}


void ModRusselBeam::setUseNoUpwardConstraint ( bool val ) {
    _useNoUpwardConstraint = val;
}

void ModRusselBeam::setAccuracy ( double acc ) {
    _accuracy = acc;
}


void ModRusselBeam::set_nIntegralConstraints ( int nIntegralConstraints ) {
    _nIntegralConstraints = nIntegralConstraints;
}

int ModRusselBeam::get_nIntegralConstraints ( void ) const {
    return _nIntegralConstraints;
}





Transform3D< double > ModRusselBeam::get_planeTbeam ( void ) const {
    const rw::math::Transform3D<> &Tbeam = _geomPtr->getTransform();
    rw::math::Transform3D<> planeTbeam = _obstaclePtr->compute_planeTbeam ( Tbeam );

    return planeTbeam;
}


double ModRusselBeam::get_thetaTCP ( void ) const {
    const rw::math::Transform3D<> planeTbeam = get_planeTbeam();
    double thetaTCP = _obstaclePtr->get_thetaTCP ( planeTbeam );

    return thetaTCP;
}



double ModRusselBeam::get_yTCP ( void ) const {
    const rw::math::Transform3D<> planeTbeam = get_planeTbeam();
    double yTCP = _obstaclePtr->get_yTCP ( planeTbeam );

    return yTCP;
}


double ModRusselBeam::get_uxTCPy ( void ) const {
    const rw::math::Transform3D<> planeTbeam = get_planeTbeam();
    return planeTbeam.R() ( 1, 0 );
}


double ModRusselBeam::get_uyTCPy ( void ) const {
    const rw::math::Transform3D<> planeTbeam = get_planeTbeam();
    return planeTbeam.R() ( 1, 1 );
}




double ModRusselBeam::get_h ( void )  const {
    // TODO calculate once and set var
    double a = _geomPtr->get_a();
    double b = _geomPtr->get_b();

    return ( b-a ) / ( getM()-1 );
}


int ModRusselBeam::getM ( void )  const {
    return _M;
}


int ModRusselBeam::getN ( void )  const {
    return getM() -1;
}


struct RusselIntegrand {
    RusselIntegrand (
        const BeamGeometry &geom,
        const boost::numeric::ublas::vector<double>& a,
        const boost::numeric::ublas::vector<double>& da
    ) :
        _geom ( geom ),
        _a ( a ),
        _da ( da ) {

    };

    // gravitational energy per unit volume
    double eg ( const int i ) const {
        RW_ASSERT ( i < ( int ) _a.size() );
        const double &ax = _a[i];

        const double &g1 = _geom.g1()  * ( -1.0 );
        const double &g2 = _geom.g2() * ( -1.0 );
        const double b0 = _geom.b0 ( i );
        const double b1 = _geom.b1 ( i );
        const double B0 = _geom.B0 ( i );
        const double x = i * _geom.get_h();

        const double Eg = ( g1 * B0 + g2*b1 ) * cos ( ax ) + ( g2*B0 - g1*b1 ) * sin ( ax ) - g1*b0 * x - g2*b1;
        return Eg;
    };

    // elastic energy per unit volume
    double ee ( const int i ) const {
        RW_ASSERT ( i < ( int ) _da.size() );
        const double &dax = _da[i];

        const double c2 = _geom.c2 ( i );
        const double c3 = _geom.c3 ( i );
        const double c4 = _geom.c4 ( i );

        const double Ee = 4 * c2 * pow ( dax, 2.0 ) + 4 * c3 * pow ( dax, 3.0 ) + c4 * pow ( dax, 4.0 );
        return Ee;
    };

    // total energy per unit volume
    double operator() ( const int i ) const {
        return eg ( i ) + ee ( i );
    };

private:
    const BeamGeometry &_geom;
    const boost::numeric::ublas::vector<double>& _a;
    const boost::numeric::ublas::vector<double>& _da;
};





struct RusselIntegrandEonly : public RusselIntegrand {
    RusselIntegrandEonly (
        const BeamGeometry &geom,
        const boost::numeric::ublas::vector<double>& a,
        const boost::numeric::ublas::vector<double>& da
    ) :
        RusselIntegrand ( geom, a, da ) {
    };

    // only elastic energy
    double operator() ( const int i ) const {
        return ee ( i );
    };
};




//This is the object function
double ModRusselBeam::f ( const boost::numeric::ublas::vector<double>& x ) {
    //const int N = getN();
    const int M = getM();
    const double h = get_h();

    RW_ASSERT ( ( int ) x.size() == getN() );
    RW_ASSERT ( ( int ) _a.size() == getM() );

    _a[0] = 0.0;
    for ( int i = 1; i < M; i++ )
        _a[i] = x[i-1];

    FdUtil::vectorDerivative ( _a, _da, get_h() ) ;

    RusselIntegrand intgr ( *_geomPtr, _a, _da );
    double val = TrapMethod::trapezMethod<RusselIntegrand> ( intgr, M, h );

    NFCALLS++;

    return val;
}


double ModRusselBeam::f_elastic ( const boost::numeric::ublas::vector< double >& x ) {
    const int M = getM();
    const double h = get_h();

    RW_ASSERT ( ( int ) x.size() == getN() );
    RW_ASSERT ( ( int ) _a.size() == getM() );

    _a[0] = 0.0;
    for ( int i = 1; i < M; i++ )
        _a[i] = x[i-1];

    FdUtil::vectorDerivative ( _a, _da, get_h() ) ;

    RusselIntegrandEonly intgr ( *_geomPtr, _a, _da );
    double val = TrapMethod::trapezMethod<RusselIntegrandEonly> ( intgr, M, h );

    NFCALLS++;

    return val;
}



//Computing gradient for object function
void ModRusselBeam::df ( boost::numeric::ublas::vector<double> &res, const boost::numeric::ublas::vector<double>& x ) {
//     boost::numeric::ublas::vector<double> res ( x.size() );


    const double eps = 1e-6;
    //const double eps = _geom.get_h();

    boost::numeric::ublas::vector<double> xt;

    xt= x;

    for ( size_t i = 0; i< x.size(); i++ ) {
        xt ( i ) = x ( i )-eps;
        double d1 = f ( xt );

        xt ( i ) = x ( i ) +eps;
        double d2 = f ( xt );

        res ( i ) = ( d2-d1 ) / ( 2*eps );

        xt ( i ) = x ( i );
    }
    //std::cout<<"df = "<<res<<std::endl;
//     return res;
}



void ModRusselBeam::ddf_banded2 ( boost::numeric::ublas::matrix<double> &res, const boost::numeric::ublas::vector< double >& x ) {
//     boost::numeric::ublas::matrix<double> res ( x.size(), x.size() );
    res.clear();


    const double eps = 1e-6;

    for ( int i = 0; i< ( int ) x.size(); i++ ) {
        for ( int j = i; j< ( int ) x.size(); j++ ) {
            if ( ( ( i - j ) > 2 ) || ( ( j - i ) > 2 ) ) {
                res ( i, j ) = 0.0;
            } else if (
                ( ( ( i - j ) == 1 ) || ( ( j - i ) == 1 ) )
                &&
                ( i != ( int ) x.size() - 1 ) && ( j != ( int ) x.size() - 2 )
                &&
                ( i != ( int ) x.size() - 2 ) && ( j != ( int ) x.size() - 1 )
            ) {
                //if (  (i != x.size() - 1) && (i != x.size() - 2) )
                res ( i, j ) = 0.0;

            } else if ( i == j ) {
                boost::numeric::ublas::vector<double> xt;

                xt= x;

                xt ( i ) = x ( i ) + eps;
                const double ip = f ( xt );

                xt ( i ) = x ( i ) - eps;
                const double im = f ( xt );

                const double ic = f ( x );

                res ( i, j ) = ( ip - 2 * ic + im ) / ( eps * eps );
            } else {
                boost::numeric::ublas::vector<double> xt;

                xt= x;

                xt ( i ) = x ( i ) + eps;
                xt ( j ) = x ( j ) + eps;
                const double ipjp = f ( xt );

                xt ( i ) = x ( i ) + eps;
                xt ( j ) = x ( j ) - eps;
                const double ipjm = f ( xt );

                xt ( i ) = x ( i ) - eps;
                xt ( j ) = x ( j ) + eps;
                const double imjp = f ( xt );

                xt ( i ) = x ( i ) - eps;
                xt ( j ) = x ( j ) - eps;
                const double imjm = f ( xt );

                const double tmp = ( ipjp - ipjm - imjp + imjm ) / ( 4.0 * eps * eps );
                res ( i, j ) = tmp;
                res ( j, i ) = tmp;
            }
        }
    }

//     return res;
}




void ModRusselBeam::objective ( const boost::numeric::ublas::vector< double >& x, double& f, boost::numeric::ublas::vector< double >& df, boost::numeric::ublas::matrix< double >& ddf ) {
    //Implement the objective function here.
    //Input:	x: parameters
    //Output:	f: objective value
    //			df: gradient
    //			ddf: Hessian

    f = this->f ( x );
    this->df ( df, x );
    this->ddf_banded2 ( ddf, x );
}



/**
* The equality constraints. Require g(x)=0
*/
void ModRusselBeam::equalityConstraints ( const boost::numeric::ublas::vector< double >& x, size_t idx, double& g, boost::numeric::ublas::vector< double >& dg, boost::numeric::ublas::matrix< double >& ddg ) {

}


void ModRusselBeam::setInEqualityIntegralConstraintPoint ( const boost::numeric::ublas::vector< double >& x, std::size_t idx, boost::numeric::ublas::vector< double >& h, boost::numeric::ublas::matrix< double >& dh, boost::numeric::ublas::matrix< double >& ddh, int pIdx, int hBase ) {
    const rw::math::Transform3D<> planeTbeam = get_planeTbeam();
    double yTCP = _obstaclePtr->get_yTCP ( planeTbeam );
    const double hx = ( _geomPtr->get_b() - _geomPtr->get_a() ) / ( x.size() );

    RW_ASSERT ( pIdx < ( int ) x.size() );

    // V component
    const double f0V = sin ( 0.0 );
    const double fLV = sin ( x[pIdx] );

    double sumV = 0.0;
    for ( int i = 0; i < ( int ) pIdx; i++ ) {
        sumV += sin ( x[i] );
    }
    double resV = ( hx / 2.0 ) * ( f0V + fLV ) + hx * sumV;

    // U component
    const double f0U = cos ( 0.0 );
    const double fLU = cos ( x[pIdx] );

    double sumU = 0.0;
    for ( int i = 0; i < ( int ) pIdx; i++ ) {
        sumU += cos ( x[i] );
    }
    double resU = ( hx / 2.0 ) * ( f0U + fLU ) + hx * sumU;

    const double uxTCPy =  get_uxTCPy(); // u2
    const double uyTCPy = get_uyTCPy(); // v2

    /* tip constraint h
     *  endCon = UconEnd uxTCPy + VconEnd uyTCPy >= -yTCP;
     */
    h ( hBase ) = resU * uxTCPy        + resV * uyTCPy        + yTCP; // require h(x) > 0


    // TODO make b limit in integration, variable.
    // for dh and ddh we must remember that although we only integrate to a b <= L, we still must fill out the derivatives for the remaining interior points
    //  but they're just zero!

    // tip constraint dh
//     dh ( 0, 0 ) = 0.5 * hx * uyTCPy * cos ( x[0] ) - 0.5 * hx * uxTCPy * sin ( x[0] );
//     for ( int i = 1; i < ( int )  x.size() - 1; i++ ) {
//         dh ( 0, i ) = hx * uyTCPy * cos ( x[i] ) - hx * uxTCPy * sin ( x[i] );
//     }
//     dh ( 0, x.size() - 1 ) = 0.5 * hx * uyTCPy * cos ( x[x.size() -1] ) - 0.5 * hx * uxTCPy * sin ( x[x.size() -1] );

    for ( int i = 0; i < ( int ) x.size(); i++ ) {
        if ( pIdx == i ) {
            // BUG: wrong coefficient for i = 0,
            // also, we here use x[0] in both cases where it should be x[i]
            dh ( hBase, i ) = 0.5 * hx * uyTCPy * cos ( x[i] ) - 0.5 * hx * uxTCPy * sin ( x[i] );
        } else if ( i < pIdx ) {
            dh ( hBase, i ) = hx * uyTCPy * cos ( x[i] ) - hx * uxTCPy * sin ( x[i] );
        } else
            dh ( hBase, i ) = 0.0;
    }



    // tip constraint, ddh
    if ( hBase == ( int ) idx ) {
        ddh.clear();
        for ( int i = 0; i < ( int ) x.size(); i++ ) {
            for ( int j = 0; j < ( int ) x.size(); j++ ) {
                // only entries in the diagonal, i.e. d^2 f/ dx^2
                if ( i == j ) {
                    if ( pIdx == i ) {
                        // ddh(i, j) = -0.5 * hx * sin( x[i] );
                        // BUG: wrong coefficient for i = 0
                        ddh ( i, j ) = -0.5 * hx * uxTCPy * cos ( x[i] ) - 0.5 * hx * uyTCPy * sin ( x[i] );
                    } else if ( i < pIdx ) {
                        //ddh(i, j) = -1.0 * hx * sin( x[i] );
                        ddh ( i, j ) = -1.0 * hx * uxTCPy * cos ( x[i] ) - 1.0 * hx * uyTCPy * sin ( x[i] );
                    } else
                        ddh ( i, j ) = 0.0;
                }
            }
        }
    } /*else {
        cout << "constraint not active!" << endl;
        std::cout << "hBase: " << hBase << std::endl;
        std::cout << "idx: " << idx << std::endl;
    }*/
};



/*
void ModRusselBeam:: setInEqualityIntegralConstraint (
    const boost::numeric::ublas::vector< double >& x,
    size_t idx,
    boost::numeric::ublas::vector< double >& h,
    boost::numeric::ublas::matrix< double >& dh,
    boost::numeric::ublas::matrix< double >& ddh
) {
    const rw::math::Transform3D<> planeTbeam = get_planeTbeam();
    double yTCP = _obstaclePtr->get_yTCP ( planeTbeam );
    const double hx = ( _geomPtr->get_b() - _geomPtr->get_a() ) / ( x.size() );

    // V component
    const double f0V = sin ( 0.0 ); // angle at x=0 is implictly 0.0
    const double fLV = sin ( x[x.size() - 1] );

    double sumV = 0.0;
    for ( int i = 0; i < ( int ) x.size() -1; i++ ) { // loop intentionally starts at i = 0
        sumV += sin ( x[i] );
    }
    double resV = ( hx / 2.0 ) * ( f0V + fLV ) + hx * sumV;

    // U component
    const double f0U = cos ( 0.0 ); // angle at x=0 is implictly 0.0
    const double fLU = cos ( x[x.size() - 1] );

    double sumU = 0.0;
    for ( int i = 0; i < ( int ) x.size() -1; i++ ) { // loop intentionally starts at i = 0
        sumU += cos ( x[i] );
    }
    double resU = ( hx / 2.0 ) * ( f0U + fLU ) + hx * sumU;

    const double uxTCPy =  get_uxTCPy(); // u2
    const double uyTCPy = get_uyTCPy(); // v2

    // tip constraint h
    // *  endCon = UconEnd uxTCPy + VconEnd uyTCPy >= -yTCP;
    ///
    h ( 0 ) = resU * uxTCPy        + resV * uyTCPy        + yTCP; // require h(x) > 0


    // TODO make b limit in integration, variable.
    // for dh and ddh we must remember that although we only integrate to a b <= L, we still must fill out the derivatives for the remaining interior points
    //  but they're just zero!

    // tip constraint dh
    // BUG: dh (0, 0) should have 1.0 coefficient! - as x[0] corresponds to interior point in integrand interval [a:b]
    //dh ( 0, 0 ) = 0.5 * hx * uyTCPy * cos ( x[0] ) - 0.5 * hx * uxTCPy * sin ( x[0] );
    for ( int i = 0; i < ( int )  x.size() - 1; i++ ) {
        dh ( 0, i ) = hx * uyTCPy * cos ( x[i] ) - hx * uxTCPy * sin ( x[i] );

        // if <= limitIdx
            // dh(0, i) = expr...
        // else
            // dh(0, i) = 0;
    }
    dh ( 0, x.size() - 1 ) = 0.5 * hx * uyTCPy * cos ( x[x.size() -1] ) - 0.5 * hx * uxTCPy * sin ( x[x.size() -1] );



    // tip constraint, ddh
    if ( 0 == idx ) {
        ddh.clear();
        for ( int i = 0; i < ( int ) x.size(); i++ ) {
            for ( int j = 0; j < ( int ) x.size(); j++ ) {
                // only entries in the diagonal, i.e. d^2 f/ dx^2
                if ( i == j ) {
                    if (( x.size() -1 ) == i ) { //  BUG: 0 == i should have 1.0 coefficient!
                        ddh ( i, j ) = -0.5 * hx * uxTCPy * cos ( x[i] ) - 0.5 * hx * uyTCPy * sin ( x[i] );
                    } else {
                        ddh ( i, j ) = -1.0 * hx * uxTCPy * cos ( x[i] ) - 1.0 * hx * uyTCPy * sin ( x[i] );
                    }
                }
            }
        }
    }
};*/


void ModRusselBeam::setInEqualityNoUpwardsEtaConstraint (
    const boost::numeric::ublas::vector< double >& x,
    size_t idx,
    boost::numeric::ublas::vector< double >& h,
    boost::numeric::ublas::matrix< double >& dh,
    boost::numeric::ublas::matrix< double >& ddh,
    int hBase
) {
    const double uxTCPy =  get_uxTCPy();
    const double uyTCPy = get_uyTCPy();
    // run through all points and formulate the 'no-pointing-upwards' constraint
    for ( int i = 0; i < ( int ) x.size(); i++ ) {
        h ( hBase + i ) = - uyTCPy * sin ( x[i] ) - uxTCPy * cos ( x[i] );

        // dh
        for ( int j = 0; j < ( int ) x.size(); j++ ) {
            if ( j == i ) {
                // - _uyTCPy * sin( x[i] ) - _uxTCPy * cos( x[i] )
                //dh( i+1, j) = -cos ( x[i] );
                dh ( hBase + i, j ) = -uyTCPy * cos ( x[i] ) + uxTCPy * sin ( x[i] );
            } else
                dh ( hBase + i, j ) = 0.0;
        }
    }
    
    for ( int i = 0; i < ( int ) x.size(); i++ ) {
        // ddh
        if ( hBase + i == ( int ) idx ) {
            // the 'no-pointing-upwards' constraint for point at xi = i*h, has idx=i+1

            // if idx=1 then we are dealing with the point at i=0

            // the constraint is there
            // -sin( x[0] )

            // only non-zero entry in the hessian is at i=j=0
            ddh.clear();
            //ddh(i, i) =  sin ( x[i] );
            // _uxTCPy Cos[x] + _uyTCPy Sin[x]
            ddh ( i, i ) =  uxTCPy * cos ( x[i] ) + uyTCPy * sin ( x[i] );
        }
    }
        
    
/*    
    if ( hBase + i == ( int ) idx ) {
        ddh.clear();
        for ( int i = 0; i < ( int ) x.size(); i++ ) {
        // ddh
     
            // the 'no-pointing-upwards' constraint for point at xi = i*h, has idx=i+1

            // if idx=1 then we are dealing with the point at i=0

            // the constraint is there
            // -sin( x[0] )

            // only non-zero entry in the hessian is at i=j=0
            
            //ddh(i, i) =  sin ( x[i] );
            // _uxTCPy Cos[x] + _uyTCPy Sin[x]
            ddh ( i, i ) =  uxTCPy * cos ( x[i] ) + uyTCPy * sin ( x[i] );
        }
    }*/

}



/**
* The inequality constraints. Require h(x) > 0
//Implement the objective function here.
//Input:	x: parameters
//			idx: index of the active constraint
//Output:		h: constraint value
//			dh: constraint gradient
//			ddh: constraint Hessian
*/
void ModRusselBeam::inEqualityConstraints (
    const boost::numeric::ublas::vector< double >& x,
    size_t idx,
    boost::numeric::ublas::vector< double >& h,
    boost::numeric::ublas::matrix< double >& dh,
    boost::numeric::ublas::matrix< double >& ddh
) {
//     h.clear();
//     dh.clear();
//     ddh.clear();
//
//     cout << DIVIDER << endl;
//     cout << "old\n";
//     setInEqualityIntegralConstraint ( x, idx, h, dh, ddh ); // h[0]

//
//     h.clear();
//     dh.clear();
//     ddh.clear();



//     int pIdx = 1 * (x.size() -1) / 3 ;
//     int hBase = 0;
//     setInEqualityIntegralConstraintPoint( x, idx, h, dh, ddh, pIdx, hBase );
// //
//     pIdx = (x.size() -1) / 2 ;
//     hBase++;
//     setInEqualityIntegralConstraintPoint( x, idx, h, dh, ddh, pIdx, hBase );
// //
//     pIdx = 2 * (x.size() -1) / 3 ;
//     hBase++;
//     setInEqualityIntegralConstraintPoint( x, idx, h, dh, ddh, pIdx, hBase );

//      pIdx = (x.size() -1) ;
//      hBase++;
//     setInEqualityIntegralConstraintPoint( x, idx, h, dh, ddh, pIdx, hBase );
//      int pIdx = (x.size() -1) ;

//     setInEqualityIntegralConstraintPoint( x, idx, h, dh, ddh, pIdx, hBase );

//     const int hi = x.size() / get_nIntegralConstraints();

//     std::cout << "hi: " << hi << std::endl;
//     std::cout << "get_nIntegralConstraints: " << get_nIntegralConstraints() << std::endl;

//     for (int i = x.size() -1; i > 2; i -= hi) {
//         cout << "setting integral constraint at i = " << i << endl;
//         std::cout << "\thBase: " << hBase << std::endl;

//     }

//     std::cout << "x: " << x << std::endl;
//     std::cout << "idx: " << idx << std::endl;

    int hBase = 0;
    for ( int i = 0; i < ( int ) _integralConstraintIdxList.size(); i++ ) {
        int pIdx = _integralConstraintIdxList[i];
        setInEqualityIntegralConstraintPoint ( x, idx, h, dh, ddh, pIdx, hBase++ );
    }


    if ( _useNoUpwardConstraint ) {
        setInEqualityNoUpwardsEtaConstraint ( x, idx, h, dh, ddh, hBase ); // h[1] : h[x.size() + 1]
    }

//     std::cout << "h: " << h << std::endl;
//     std::cout << "dh: " << dh << std::endl;
//     std::cout << "ddh: " << ddh << std::endl;
}



void ModRusselBeam::solve ( boost::numeric::ublas::vector< double >& xinituser, boost::numeric::ublas::vector< double >& U, boost::numeric::ublas::vector< double >& V ) {
    cout << "ModRusselBeam::solve()" << endl;

    if ( get_nIntegralConstraints() < 0 )
        RW_THROW ( "Number of integral constraints cannot be negative" );

    if ( get_nIntegralConstraints() > getN() )
        RW_THROW ( "Number of integral constraints must be less or equal to M-1" );

    std::cout << "get_nIntegralConstraints(): " << get_nIntegralConstraints() << std::endl;
    _integralConstraintIdxList.clear();
    if ( get_nIntegralConstraints() > 0 ) {
        const double hi = getN() / get_nIntegralConstraints();
        for ( int i = 1; i < get_nIntegralConstraints() + 1; i++ ) {
            int idx = ( int ) ceil ( double ( i ) * hi ) - 1;
            _integralConstraintIdxList.push_back ( idx );
            std::cout << "idx: " << idx << std::endl;
        }
    }
    std::cout << "_integralConstraintIdxList.size(): " << _integralConstraintIdxList.size() << std::endl;

    const size_t N = getN(); //Dimensions of parameter space
//     const size_t L = 	_useNoUpwardConstraint == true 		?	 4 + N : 4;

    const size_t L =    _useNoUpwardConstraint == true      ?    get_nIntegralConstraints() + N : get_nIntegralConstraints();

    boost::numeric::ublas::vector<double> xinit ( N );
    for ( int i = 0; i < ( int ) N; i++ ) {
        xinit[i] = xinituser[i+1];
    }

    std::cout << "xinituser: " << xinituser << std::endl;
    std::cout << "DIVIDER: " << DIVIDER << std::endl;

    NFCALLS = 0;
    InteriorPointOptimizer iop ( N, L,
                                 boost::bind ( &ModRusselBeam::objective, this, _1, _2, _3, _4 ),
                                 boost::bind ( &ModRusselBeam::inEqualityConstraints, this, _1, _2, _3, _4, _5 )
                               );
    iop.setAccuracy ( _accuracy );
    boost::numeric::ublas::vector<double> res = iop.solve ( xinit );
    std::cout << "NFCALLS: " << NFCALLS << std::endl;

    const double Ee = f_elastic ( res );
    std::cout << "Ee: " << Ee << " [kg mm^2/s^2] = " << Ee * 1.0e-6 << " [J]" << std::endl;

    integrateAngleU ( U, res );
    integrateAngleV ( V, res );

    xinituser[0] = 0.0;
    for ( int i = 0; i < ( int ) res.size(); i++ )
        xinituser[i+1] = res[i];
}


// integrates x-component of angle, assuming implictly a(0) = 0;
void ModRusselBeam::integrateAngleU ( boost::numeric::ublas::vector< double >& U, const boost::numeric::ublas::vector< double >& avec ) {
    const double h = ( _geomPtr->get_b() - _geomPtr->get_a() ) / avec.size();
    std::cout << "h: " << h << std::endl;
    std::cout << "avec.size(): " << avec.size() << std::endl;

    U[0] = 0.0;
    for ( int end = 0; end < ( int )  avec.size(); end++ ) {
        const double f0 = cos ( 0.0 );
        const double fL = cos ( avec[end] );

        double sum = 0.0;
        for ( int i = 0; i < ( int ) end; i++ )
            sum += cos ( avec[i] );

        U[end+1] = ( h / 2.0 ) * ( f0 + fL ) + h * sum;
    }
}


void ModRusselBeam::integrateAngleV ( boost::numeric::ublas::vector< double >& V, const boost::numeric::ublas::vector< double >& avec ) {
    const double h = ( _geomPtr->get_b() - _geomPtr->get_a() ) / avec.size();
    std::cout << "h: " << h << std::endl;
    std::cout << "avec.size(): " << avec.size() << std::endl;

    V[0] = 0.0;
    for ( int end = 0; end < ( int ) avec.size(); end++ ) {
        const double f0 = sin ( 0.0 );
        const double fL = sin ( avec[end] );

        double sum = 0.0;
        for ( int i = 0; i < ( int ) end; i++ )
            sum += sin ( avec[i] );

        V[end+1] = ( h / 2.0 ) * ( f0 + fL ) + h * sum;
    }
}
