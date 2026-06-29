#ifndef GEOMETRY_H_INCLUDED
#define GEOMETRY_H_INCLUDED

#include <string>

struct Matrix22;
struct Vector;
class Orientation;
struct Quaternion;
struct AxisAngle;

struct Vector{
    double fX;
    double fY;
    double fZ;

    //Constructors
    Vector(): fX(0.0), fY(0.0), fZ(0.0) {}
    Vector(const Vector & vec) = default;
    Vector & operator=(const Vector & vec) = default;
    Vector(const double & f1, const double & f2, const double & f3) : fX(f1), fY(f2), fZ(f3) {}

    //Operators
    Vector & operator*=(const Matrix22 & m);
    Vector & operator*=(const double & f);
    Vector & operator/=(const double & f);
    Vector & operator+=(const Vector & v);
    Vector & operator-=(const Vector & v);
    Vector & operator/=(const Vector & v);
    Vector & Rotate(const Quaternion & q);
    bool operator==(const Vector & v);
    double GetLength() const;
    void Normalize();
    bool Compare(const Vector & v1, double fDiff = 0.00001);
    bool Null(double fDiff = 0.0);
    void Set(const double & f1, const double & f2, const double & f3);
    std::string Print() const;
    void print(const std::string & sMsg);
};
Vector operator*(Vector v, const Matrix22 & m);
Vector operator*(Vector v, const double & f);
Vector operator/(Vector v, const double & f);
Vector operator*(const double & f, Vector v);
double operator*(const Vector & v, const Vector & v2); //dot product
double dot(const Vector & v, const Vector & v2); //dot product
Vector operator/(Vector v, const Vector & v2); //cross product
Vector cross(Vector v, const Vector & v2); //cross product
Vector operator+(Vector v, const Vector & v2);
Vector operator-(Vector v, const Vector & v2);
double Angle(const Vector & v, const Vector & v2);
double HeronFormulaVert(const Vector & v1, const Vector & v2, const Vector & v3);
double HeronFormulaEdge(const Vector & e1, const Vector & e2, const Vector & e3);
Vector Rotate(Vector v, const Quaternion & q);
Vector DecompressVector(unsigned int nCompressed);
unsigned int CompressVector(const Vector & v);

struct Quaternion{
    Vector vAxis;
    double fW;

    //Constructors
    Quaternion() : vAxis(0.0, 0.0, 0.0), fW(1.0) {}
    Quaternion(const double & fX, const double & fY, const double & fZ, const double & fW) : vAxis(fX, fY, fZ), fW(fW) {}
    Quaternion(const Vector & v, const double & fW) : vAxis(v), fW(fW) {}
    Quaternion(const AxisAngle & aa);

    //Operators
    Quaternion & operator+=(const Quaternion & q);
    Quaternion & operator-=(const Quaternion & q);
    Quaternion & operator*=(const Quaternion & q);
    //Quaternion & operator/=(const Quaternion & q);
    Quaternion & operator*=(const double & f);
    Quaternion & operator/=(const double & f);
    Quaternion & normalize();
    Quaternion conjugate() const;
    Quaternion reverse() const;
    Quaternion inverse() const;
    double norm();

    std::string Print() const {
        std::stringstream ss;
        ss << "(" << vAxis.fX << ", " << vAxis.fY << ", " << vAxis.fZ << ", " << fW << ")";
        return ss.str();
    }
};
Quaternion operator*(Quaternion q, const double & f);
Quaternion operator/(Quaternion q, const double & f);
Quaternion operator+(Quaternion q, const Quaternion & q2);
Quaternion operator-(Quaternion q, const Quaternion & q2);
Quaternion operator*(Quaternion q, const Quaternion & q2);
Quaternion operator*(Quaternion q, const Quaternion && q2);
Quaternion DecompressQuaternion(unsigned int nCompressed);
unsigned int CompressQuaternion(Quaternion qUncompressed);

struct AxisAngle{
    Vector vAxis;
    double fAngle;

    //Constructors
    AxisAngle() : vAxis(0.0, 0.0, 0.0), fAngle(0.0) {}
    AxisAngle(const double & fX, const double & fY, const double & fZ, const double & fAngle) : vAxis(fX, fY, fZ), fAngle(fAngle) {}
    AxisAngle(const Vector & v, const double & fAngle) : vAxis(v), fAngle(fAngle) {}
    AxisAngle(Quaternion q);

    AxisAngle & operator*=(const double & f);

    std::string Print() const {
        std::stringstream ss;
        ss << "(" << vAxis.fX << ", " << vAxis.fY << ", " << vAxis.fZ << "), " << fAngle;
        return ss.str();
    }
};
AxisAngle operator*(AxisAngle aa, const double & f);


struct Matrix22{
    double f11;
    double f12;
    double f21;
    double f22;

    //Constructors
    Matrix22() : f11(0.0), f12(0.0), f21(0.0), f22(0.0) {}
    Matrix22(const double & f1, const double & f2, const double & f3, const double & f4) : f11(f1), f12(f2), f21(f3), f22(f4) {}
};

class Orientation{
    Quaternion quaternion;
    AxisAngle axisangle;
    bool bQuaternion, bAxisAngle;

  public:
    Orientation(): quaternion(0.0, 0.0, 0.0, 1.0), axisangle(), bQuaternion(true), bAxisAngle(false) {}
    Orientation(const Quaternion & q) : quaternion(q), axisangle(), bQuaternion(true), bAxisAngle(false) {}
    Orientation(const AxisAngle & aa) : quaternion(), axisangle(aa), bQuaternion(false), bAxisAngle(true) {}
    Orientation(const double & f1, const double & f2, const double & f3, const double & f4): quaternion(f1, f2, f3, f4), bQuaternion(true), bAxisAngle(false) {}
    void SetQuaternion(const double & f1, const double & f2, const double & f3, const double & f4);
    void SetAxisAngle(const double & f1, const double & f2, const double & f3, const double & f4);
    void SetQuaternion(const Quaternion & q){
        quaternion = q;
        bQuaternion = true;
        bAxisAngle = false;
    }
    void SetAxisAngle(const AxisAngle & aa){
        axisangle = aa;
        bQuaternion = false;
        bAxisAngle = true;
    }
    const Quaternion & GetQuaternion();
    Quaternion GetQuaternionValue() const;
    const AxisAngle & GetAxisAngle();
};


#endif // GEOMETRY_H_INCLUDED
