#include "MDL.h"
#include <cmath>

AxisAngle operator*(AxisAngle aa, const double & f){
    aa *= f;
    return aa;
}
Quaternion operator*(Quaternion q, const double & f){
    q *= f;
    return q;
}
Quaternion operator/(Quaternion q, const double & f){
    q /= f;
    return q;
}
Quaternion operator+(Quaternion q, const Quaternion & q2){
    q += q2;
    return q;
}
Quaternion operator-(Quaternion q, const Quaternion & q2){
    q -= q2;
    return q;
}
Quaternion operator*(Quaternion q, const Quaternion & q2){
    q *= q2;
    return q;
}
Quaternion operator*(Quaternion q, const Quaternion && q2){
    q *= q2;
    return q;
}

/// VECTOR
Vector operator*(Vector v, const Matrix22 & m){
    v *= m;
    return v;
}

Vector operator*(Vector v, const double & f){
    v *= f;
    return v;
}

Vector operator/(Vector v, const double & f){
    v /= f;
    return v;
}

Vector operator*(const double & f, Vector v){
    v *= f;
    return v;
}

double operator*(const Vector & v, const Vector & v2){ //dot product
    return (v.fX * v2.fX + v.fY * v2.fY + v.fZ * v2.fZ);
}
double dot(const Vector & v, const Vector & v2){ //dot product
    return (v.fX * v2.fX + v.fY * v2.fY + v.fZ * v2.fZ);
}

double Angle(const Vector & v, const Vector & v2){
    double fLength1 = v.GetLength();
    double fLength2 = v2.GetLength();
    double fDenominator = fLength1 * fLength2;
    if(fDenominator <= 0.000000000001 || !std::isfinite(fDenominator)) return 0.0;

    double fCos = (v * v2) / fDenominator;
    if(fCos > 1.0) fCos = 1.0;
    else if(fCos < -1.0) fCos = -1.0;

    return acos(fCos);
}

Vector operator/(Vector v, const Vector & v2){ //cross product
    v /= v2;
    return v;
}
Vector cross(Vector v, const Vector & v2){ //cross product
    v /= v2;
    return v;
}

Vector operator-(Vector v, const Vector & v2){
    v -= v2;
    return v;
}

Vector operator+(Vector v, const Vector & v2){
    v += v2;
    return v;
}

Vector Rotate(Vector v, const Quaternion & q){
    return v.Rotate(q);
}

double HeronFormulaEdge(const Vector & e1, const Vector & e2, const Vector & e3){
    double fA = e1.GetLength();
    double fB = e2.GetLength();
    double fC = e3.GetLength();
    double fS = (fA + fB + fC) / 2.0;
    if(fA <= 0.0 || fB <= 0.0 || fC <= 0.0){
        //std::cout << "Heron Formula error: One of the edges ("<</*fA<<", "<<fB<<", "<<fC<</**/") has zero or negative length.\n";
        return -1.0;
    }
    if(fA > fB + fC || fB > fA + fC || fC > fA + fB){
        //std::cout << "Heron Formula error: One of the edges ("<</**/fA<<", "<<fB<<", "<<fC<</**/") is longer than the other two combined.\n";
        return -1.0;
    }
    double fAreaSq = fS * (fS - fA) * (fS - fB) * (fS - fC);
    return sqrt(fAreaSq);
}

double HeronFormulaVert(const Vector & v1, const Vector & v2, const Vector & v3){
    Vector e1 = v1 - v2;
    Vector e2 = v1 - v3;
    Vector e3 = v2 - v3;
    double fA = e1.GetLength();
    double fB = e2.GetLength();
    double fC = e3.GetLength();
    double fS = (fA + fB + fC) / 2.0;
    if(fA <= 0.0 || fB <= 0.0 || fC <= 0.0){
        //std::cout << "Heron Formula error: One of the edges ("<</*fA<<", "<<fB<<", "<<fC<</**/") has zero or negative length.\n";
        return -1.0;
    }
    if(fA > fB + fC || fB > fA + fC || fC > fA + fB){
        //std::cout << "Heron Formula error: One of the edges ("<</**/fA<<", "<<fB<<", "<<fC<</**/") is longer than the other two combined.\n";
        return -1.0;
    }
    return sqrt(fS * (fS - fA) * (fS - fB) * (fS - fC));
}


void Vector::Set(const double & f1, const double & f2, const double & f3){
    fX = f1;
    fY = f2;
    fZ = f3;
}

std::string Vector::Print() const {
    std::stringstream ss;
    ss << "(" << PrepareFloat(fX) << ", " << PrepareFloat(fY) << ", " << PrepareFloat(fZ) << ")";
    return ss.str();
}

void Vector::print(const std::string & sMsg){
    std::cout << sMsg << " fX=" << fX << ", fY=" << fY << ".\n";
}
Vector & Vector::operator*=(const Matrix22 & m){
    double fOldX = fX;
    double fOldY = fY;
    fX = m.f11 * fOldX + m.f12 * fOldY;
    fY = m.f21 * fOldX + m.f22 * fOldY;
    return *this;
}
Vector & Vector::operator*=(const double & f){
    fX *= f;
    fY *= f;
    fZ *= f;
    return *this;
}
Vector & Vector::operator/=(const double & f){
    fX /= f;
    fY /= f;
    fZ /= f;
    return *this;
}
Vector & Vector::operator+=(const Vector & v){
    fX += v.fX;
    fY += v.fY;
    fZ += v.fZ;
    return *this;
}
Vector & Vector::operator-=(const Vector & v){
    fX -= v.fX;
    fY -= v.fY;
    fZ -= v.fZ;
    return *this;
}
Vector & Vector::operator/=(const Vector & v){ //cross product
    double fcrossx = fY * v.fZ - fZ * v.fY;
    double fcrossy = fZ * v.fX - fX * v.fZ;
    double fcrossz = fX * v.fY - fY * v.fX;
    fX = std::move(fcrossx);
    fY = std::move(fcrossy);
    fZ = std::move(fcrossz);
    return *this;
}
bool Vector::operator==(const Vector & v){
    //if(fX == v.fX && fY == v.fY && fZ == v.fZ) return true;
    if(abs(fX-v.fX) < 0.0001 && abs(fY-v.fY) < 0.0001 && abs(fZ-v.fZ) < 0.0001) return true;
    return false;
}
Vector & Vector::Rotate(const Quaternion & q){
    if(fX == 0.0 && fY == 0.0 && fZ == 0.0) return *this;

    /// Use quaternion sandwich rotation with q.inverse(); the closed-form
    /// unit-quaternion shortcut is not equivalent for non-unit data.
    Quaternion qVec (fX, fY, fZ, 0.0);
    qVec = (q * qVec) * q.inverse();
    *this = qVec.vAxis;
    return *this;
}
double Vector::GetLength() const {
    return sqrt(pow(fX, 2.0) + pow(fY, 2.0) + pow(fZ, 2.0));
}
void Vector::Normalize(){
    double fNorm = GetLength();
    if(fNorm < 0.000001 || !std::isfinite(fNorm)){
        /// Degenerate-vector handling.
        *this = Vector(0.0, 0.0, 0.0);
    }
    else *this /= fNorm;
}
bool Vector::Compare(const Vector & v1, double fDiff){
    return ((*this - v1).GetLength() < fDiff);
}
bool Vector::Null(double fDiff){
    if(abs(fX) <= fDiff &&
       abs(fY) <= fDiff &&
       abs(fZ) <= fDiff ) return true;
    else return false;
}
Vector DecompressVector(unsigned int nCompressed){
    Vector vAxis;
    unsigned int nCurrent;

    nCurrent = (nCompressed&0x07FF);
    if(nCurrent < 1024) vAxis.fX = nCurrent / 1023.0;
    else vAxis.fX = (2048 - nCurrent) / 1023.0 * -1.0;

    nCurrent = ((nCompressed>>11)&0x07FF);
    if(nCurrent < 1024) vAxis.fY = nCurrent / 1023.0;
    else vAxis.fY = (2048 - nCurrent) / 1023.0 * -1.0;

    nCurrent = (nCompressed>>22);
    if(nCurrent < 512) vAxis.fZ = nCurrent / 511.0;
    else vAxis.fZ = (1024 - nCurrent) / 511.0 * -1.0;
    return vAxis;
}

unsigned int CompressVector(const Vector & v){
    if(abs(v.fX) > 1.0 || abs(v.fY) > 1.0 || abs(v.fZ) > 1.0){
        std::cout << "Warning! CompressVector() was given an unnormalized vector. Returning 0.\n";
        return 0;
    }
    unsigned int nReturn = 0;
    unsigned int nCurrent;
    if(v.fZ < 0.0) nCurrent = (int) round(v.fZ * 511.0) + 1024;
    else           nCurrent = (int) round(v.fZ * 511.0);
    nReturn = nReturn | nCurrent;
    if(v.fY < 0.0) nCurrent = (int) round(v.fY * 1023.0) + 2048;
    else           nCurrent = (int) round(v.fY * 1023.0);
    nReturn = (nReturn << 11) | nCurrent;
    if(v.fX < 0.0) nCurrent = (int) round(v.fX * 1023.0) + 2048;
    else           nCurrent = (int) round(v.fX * 1023.0);
    nReturn = (nReturn << 11) | nCurrent;
    return nReturn;
}

/// QUATERNION
//Constructors
Quaternion::Quaternion(const AxisAngle & aa){
    vAxis = aa.vAxis * sin(aa.fAngle / 2.0);
    fW = cos(aa.fAngle / 2.0);
}
Quaternion & Quaternion::operator+=(const Quaternion & q){
    vAxis += q.vAxis;
    fW += q.fW;
    return *this;
}
Quaternion & Quaternion::operator-=(const Quaternion & q){
    vAxis -= q.vAxis;
    fW -= q.fW;
    return *this;
}
Quaternion & Quaternion::operator*=(const Quaternion & q){
    //double fW2 = fW * q.fW - dot(vAxis, q.vAxis);
    //vAxis = fW * q.vAxis + q.fW * vAxis + cross(vAxis, q.vAxis);
    double fW2 = fW*q.fW - (vAxis.fX*q.vAxis.fX + vAxis.fY*q.vAxis.fY + vAxis.fZ*q.vAxis.fZ);
    double fX2 = fW*q.vAxis.fX + vAxis.fX*q.fW + vAxis.fY * q.vAxis.fZ - vAxis.fZ * q.vAxis.fY;
    double fY2 = fW*q.vAxis.fY + vAxis.fY*q.fW + vAxis.fZ * q.vAxis.fX - vAxis.fX * q.vAxis.fZ;
    double fZ2 = fW*q.vAxis.fZ + vAxis.fZ*q.fW + vAxis.fX * q.vAxis.fY - vAxis.fY * q.vAxis.fX;
    fW = std::move(fW2);
    vAxis.fX = std::move(fX2);
    vAxis.fY = std::move(fY2);
    vAxis.fZ = std::move(fZ2);
    return *this;
}
Quaternion & Quaternion::operator*=(const double & f){
    fW *= f;
    vAxis *= f;
    return *this;
}
Quaternion & Quaternion::operator/=(const double & f){
    fW /= f;
    vAxis /= f;
    return *this;
}
Quaternion Quaternion::conjugate() const{
    return Quaternion(vAxis * -1.0, fW);
}
Quaternion Quaternion::reverse() const{
    return Quaternion(vAxis, fW * -1.0);
}
double Quaternion::norm(){
    return sqrt(fW*fW + vAxis*vAxis);
}
Quaternion & Quaternion::normalize(){
    double fNorm = norm();
    if(fNorm <= 0.000000000001 || !std::isfinite(fNorm)){
        fW = 1.0;
        vAxis = Vector(0.0, 0.0, 0.0);
        return *this;
    }
    fW/=fNorm;
    vAxis/=fNorm;
    return *this;
}
Quaternion Quaternion::inverse() const{
    return conjugate()/(fW*fW + vAxis*vAxis);
}
Quaternion DecompressQuaternion(unsigned int nCompressed){
    Quaternion q;

    //Special compressed quaternion - from MDLOps
    /// Get only the first 11 bits (max value 0x07FF or 2047)
    /// Divide by half to get values in range from 0.0 to 2.0
    /// Subtract from 1.0 so we get values in range from 1.0 to -1.0
    /// This also seems to invert it, previously the highest number
    /// is now the lowest.
    q.vAxis.fX = ((double) (nCompressed&0x07FF) / 1023.0) - 1.0;
    /// Move the bits by 11 and repeat
    q.vAxis.fY = ((double) ((nCompressed>>11)&0x07FF) / 1023.0) - 1.0;
    /// Move the bits again and repeat
    /// This time, there are only 10 bits left, so our division number
    /// is smaller (511). Also since these are the only bits left
    /// there is no need to use & to clear higher bits,
    /// because they're 0 anyway, per >>.
    q.vAxis.fZ = ((double) (nCompressed>>22) / 511.0) - 1.0;
    /// Now we get the w from the other three through the formula:
    /// x^2 + y^2 + z^2 + w^2 == 1 (unit)
    double fSquares = pow(q.vAxis.fX, 2.0) + pow(q.vAxis.fY, 2.0) + pow(q.vAxis.fZ, 2.0);
    if(fSquares < 1.0) q.fW = sqrt(1.0 - fSquares);
    else{
        /// Apprently we dimply need to set q.fW to 0.0
        q.fW = 0.0;
        /*
        /// If the sum is more than 1.0, we'd get a complex number for w
        /// Instead, set w to 0.0, then recalculate the vector (renormalize the quaternion?) accordingly
        q.fW = 0.0;
        q.vAxis /= sqrt(fSquares);
        /// Now the equation still holds:
        /// (x / sqrt(fSq))^2 + (y / sqrt(fSq))^2 + (z / sqrt(fSq))^2 + 0.0^2 == 1
        /// x^2 / fSq + y^2 / fSq + z^2 / fSq + 0.0 == 1
        /// x^2 + y^2 + z^2 == fSq (which is the definition of fSquare)
        */
    }
    //std::cout << "Decompressed Q: " << q.vAxis.Print() << ", " << q.fW << "\n";
    return q;
}

unsigned int CompressQuaternion(Quaternion q){
    unsigned int nReturn = 0;
    unsigned int nCurrent;
    nCurrent = static_cast<unsigned>((1.0 + q.vAxis.fZ) * 511.0);
    nReturn = nReturn | nCurrent;
    nCurrent = static_cast<unsigned>((1.0 + q.vAxis.fY) * 1023.0);
    nReturn = (nReturn << 11) | nCurrent;
    nCurrent = static_cast<unsigned>((1.0 + q.vAxis.fX) * 1023.0);
    nReturn = (nReturn << 11) | nCurrent;
    return nReturn;
}

/// AXISANGLE
AxisAngle::AxisAngle(Quaternion q){
    q.normalize();

    //if(qW + 1.0 < 0.001) fAngle = 0.0;
    //else fAngle = 2.0 * acos(qW);
    fAngle = 2.0 * atan2(q.vAxis.GetLength(), q.fW);

    double s = sin(fAngle/2.0); //sqrt( (1.0 - qW * qW));
    if(s < 0.001) vAxis = Vector(1.0, 0.0, 0.0); //Make it zero.
    else vAxis = q.vAxis / s;
}
AxisAngle & AxisAngle::operator*=(const double & f){
    fAngle *= f;
    vAxis *= f;
    return *this;
}

/// ORIENTATION
const Quaternion & Orientation::GetQuaternion(){
    if(bQuaternion) return quaternion;
    else if(bAxisAngle){
        quaternion = Quaternion(axisangle);
        bQuaternion = true;
        return quaternion;
    }
    else{
        std::cout << "WARNING! Getting quaternion value of an undetermined orientation!\n";
        return quaternion;
    }
}

Quaternion Orientation::GetQuaternionValue() const{
    if(bQuaternion) return quaternion;
    if(bAxisAngle) return Quaternion(axisangle);
    std::cout << "WARNING! Getting quaternion value of an undetermined orientation!\n";
    return quaternion;
}

const AxisAngle & Orientation::GetAxisAngle(){
    if(bAxisAngle) return axisangle;
    else if(bQuaternion){
        axisangle = AxisAngle(quaternion);
        bAxisAngle = true;
        return axisangle;
    }
    else{
        std::cout << "WARNING! Getting axisangle value of an undetermined orientation!\n";
        return axisangle;
    }
}
void Orientation::SetQuaternion(const double & f1, const double & f2, const double & f3, const double & f4){
    quaternion = Quaternion(f1, f2, f3, f4);
    bQuaternion = true;
    bAxisAngle = false;
}

void Orientation::SetAxisAngle(const double & f1, const double & f2, const double & f3, const double & f4){
    axisangle = AxisAngle(f1, f2, f3, f4);
    bQuaternion = false;
    bAxisAngle = true;
}
