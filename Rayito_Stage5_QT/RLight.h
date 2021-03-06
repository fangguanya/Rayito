////////////////////////////////////////////////////////////////////////////////
//
// Very simple ray tracing example
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __RLIGHT_H__
#define __RLIGHT_H__

#include "RMath.h"
#include "RMaterial.h"
#include "RScene.h"


namespace Rayito
{


// Light base class, making it easy to find all the lights in the scene.
class Light : public Shape
{
public:
    Light(const Color& c, float power) : m_color(c), m_power(power), m_material(c, power) { }
    
    virtual ~Light() { }
    
    // This *is* a light, so we put ourself on the list
    virtual void findLights(std::list<Shape*>& outLightList) { outLightList.push_back(this); }
    virtual bool isLight() const { return true; }
    
    virtual Color emitted() const { return m_color * m_power; }
    
    virtual float intersectPdf(const Intersection& isect) = 0;
    
protected:
    Color m_color;
    float m_power;
    Emitter m_material;
};


// Area light with a corner and two sides to define a rectangular/parallelipiped shape
class RectangleLight : public Light
{
public:
    RectangleLight(const Point& pos,
                   const Vector& side1,
                   const Vector& side2,
                   const Color& color,
                   float power)
        : Light(color, power), m_position(pos), m_side1(side1), m_side2(side2)
    {
        
    }
    
    virtual ~RectangleLight() { }
    
    virtual bool intersect(Intersection& intersection)
    {
        // This is much like a plane intersection, except we also range check it
        // to make sure it's within the rectangle.  Please see the plane shape
        // intersection method for a little more info.
        
        Vector normal = cross(m_side1, m_side2).normalized();
        float nDotD = dot(normal, intersection.m_ray.m_direction);
        if (nDotD == 0.0f)
        {
            return false;
        }
        
        float t = (dot(m_position, normal) - dot(intersection.m_ray.m_origin, normal)) / nDotD;
        
        // Make sure t is not behind the ray, and is closer than the current
        // closest intersection.
        if (t >= intersection.m_t || t < kRayTMin)
        {
            return false;
        }
        
        // Take the intersection point on the plane and transform it to a local
        // space where we can really easily check if it's in range or not.
        Vector side1Norm = m_side1;
        Vector side2Norm = m_side2;
        float side1Length = side1Norm.normalize();
        float side2Length = side2Norm.normalize();
        
        Point worldPoint = intersection.m_ray.calculate(t);
        Point worldRelativePoint = worldPoint - m_position;
        Point localPoint = Point(dot(worldRelativePoint, side1Norm),
                                 dot(worldRelativePoint, side2Norm),
                                 0.0f);
        
        // Do the actual range check
        if (localPoint.m_x < 0.0f || localPoint.m_x > side1Length ||
            localPoint.m_y < 0.0f || localPoint.m_y > side2Length)
        {
            return false;
        }
        
        // This intersection is the closest so far, so record it.
        intersection.m_t = t;
        intersection.m_pShape = this;
        intersection.m_pMaterial = &m_material;
        intersection.m_colorModifier = Color(1.0f, 1.0f, 1.0f);
        intersection.m_normal = normal;
        // Hit the back side of the light?  We'll count it, so flip the normal
        // to effectively make a double-sided light.
        if (dot(intersection.m_normal, intersection.m_ray.m_direction) > 0.0f)
        {
            intersection.m_normal *= -1.0f;
        }
        
        return true;
    }
    
    virtual bool doesIntersect(const Ray& ray)
    {
        // This is much like a plane intersection, except we also range check it
        // to make sure it's within the rectangle.  Please see the plane shape
        // intersection method for a little more info.
        
        Vector normal = cross(m_side1, m_side2).normalized();
        float nDotD = dot(normal, ray.m_direction);
        if (nDotD == 0.0f)
        {
            return false;
        }
        
        float t = (dot(m_position, normal) - dot(ray.m_origin, normal)) / nDotD;
        
        // Make sure t is not behind the ray, and is closer than the current
        // closest intersection.
        if (t >= ray.m_tMax || t < kRayTMin)
        {
            return false;
        }
        
        // Take the intersection point on the plane and transform it to a local
        // space where we can really easily check if it's in range or not.
        Vector side1Norm = m_side1;
        Vector side2Norm = m_side2;
        float side1Length = side1Norm.normalize();
        float side2Length = side2Norm.normalize();
        
        Point worldPoint = ray.calculate(t);
        Point worldRelativePoint = worldPoint - m_position;
        Point localPoint = Point(dot(worldRelativePoint, side1Norm),
                                 dot(worldRelativePoint, side2Norm),
                                 0.0f);
        
        // Do the actual range check
        if (localPoint.m_x < 0.0f || localPoint.m_x > side1Length ||
            localPoint.m_y < 0.0f || localPoint.m_y > side2Length)
        {
            return false;
        }
        
        return true;
    }
    
    // Given two random numbers between 0.0 and 1.0, find a location + surface
    // normal on the surface of the *light*.
    virtual bool sampleSurface(const Point& surfPosition,
                               const Vector& surfNormal,
                               float u1, float u2,
                               Point& outPosition,
                               Vector& outNormal,
                               float& outPdf)
    {
        outPosition = m_position + m_side1 * u1 + m_side2 * u2;
        Vector outgoing = surfPosition - outPosition;
        float dist = outgoing.normalize();
        outNormal = cross(m_side1, m_side2);
        float area = outNormal.normalize();
        // Reference point out in back of the light?  That's okay, we'll flip
        // the normal to have a double-sided light.
        if (dot(outNormal, outgoing) < 0.0f)
        {
            outNormal *= -1.0f;
        }
        outPdf = dist * dist / (area * std::fabs(dot(outNormal, outgoing)));
        // Really big PDFs blow up power-heuristic MIS; detect it and don't
        // sample in that case
        if (outPdf > 1.0e10f)
        {
            outPdf = 0.0f;
            return false;
        }
        return true;
    }
    
    virtual float intersectPdf(const Intersection& isect)
    {
        if (isect.m_pShape == this)
        {
            float pdf = isect.m_t * isect.m_t /
                        (std::fabs(dot(isect.m_normal, -isect.m_ray.m_direction)) *
                         cross(m_side1, m_side2).length());
            // Really big PDFs blow up power-heuristic MIS; detect it and don't
            // sample in that case
            if (pdf > 1.0e10f)
            {
                return 0.0f;
            }
            return pdf;
        }
        return 0.0f;
    }
    
protected:
    Point m_position;
    Vector m_side1, m_side2;
};


// Area light based on an arbitrary shape from the scene
class ShapeLight : public Light
{
public:
    ShapeLight(Shape *pShape,
               const Color& color,
               float power)
        : Light(color, power), m_pShape(pShape)
    {
        
    }
    
    virtual ~ShapeLight() { }
    
    virtual bool intersect(Intersection& intersection)
    {
        // Forward intersection test on to the shape, but patch in the light material
        if (m_pShape->intersect(intersection))
        {
            intersection.m_pMaterial = &m_material;
            intersection.m_pShape = this;
            return true;
        }
        return false;
    }
    
    virtual bool doesIntersect(const Ray& ray)
    {
        // Forward intersection test on to the shape
        return m_pShape->doesIntersect(ray);
    }
    
    // Given two random numbers between 0.0 and 1.0, find a location + surface
    // normal on the surface of the *light*.
    virtual bool sampleSurface(const Point& surfPosition,
                               const Point& surfNormal,
                               float u1, float u2,
                               Point& outPosition,
                               Vector& outNormal,
                               float& outPdf)
    {
        // Forward surface sampling on to the shape
        if (!m_pShape->sampleSurface(surfPosition, surfNormal,
                                     u1, u2,
                                     outPosition, outNormal, outPdf))
        {
            outPdf = 0.0f;
            return false;
        }
        // Reference point out in back of the light or light on backside of sample?  Discard the sample.
        if (dot(outNormal, surfPosition - outPosition) < 0.0f)
        {
            return false;
        }
        return true;
    }
    
    virtual float intersectPdf(const Intersection& isect)
    {
        if (isect.m_pShape == this)
        {
            return m_pShape->pdfSA(isect.m_ray.m_origin,
                                   isect.m_ray.m_direction, // TODO: this isn't quite correct, but it's unused ATM
                                   isect.position(),
                                   isect.m_normal);
        }
        return 0.0f;
    }
    
protected:
    Shape *m_pShape;
};


} // namespace Rayito


#endif // __RLIGHT_H__
