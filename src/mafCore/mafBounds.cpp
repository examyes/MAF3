/*
 *  mafBounds.cpp
 *  mafCore
 *
 *  Created by Paolo Quadrani on 10/02/12.
 *  Copyright 2012 B3C. All rights reserved.
 *
 *  See License at: http://tiny.cc/QXJ4D
 *
 */

#include "mafBounds.h"

using namespace mafCore;

mafBounds::mafBounds(const QString code_location) : mafReferenceCounted(), m_XMin(0), m_YMin(0), m_ZMin(0), m_XMax(-1), m_YMax(-1), m_ZMax(-1) {
}

// mafBounds::mafBounds(const mafBounds &p) : m_XMin(p.xMin()), m_YMin(p.yMin()), m_ZMin(p.zMin()), m_XMax(p.xMax()), m_YMax(p.yMax()), m_ZMax(p.zMax()) {
// }

mafBounds::mafBounds(double b[6], const QString code_location) : m_XMin(b[0]), m_XMax(b[1]), m_YMin(b[2]), m_YMax(b[3]), m_ZMin(b[4]), m_ZMax(b[5]) {
}

mafBounds::~mafBounds() {
}

mafBounds &mafBounds::operator =(const mafBounds& obj) {
    m_XMin = obj.xMin();
    m_XMax = obj.xMax();
    m_YMin = obj.yMin();
    m_YMax = obj.yMax();
    m_ZMin = obj.zMin();
    m_ZMax = obj.zMax();
    return *this;
}

bool mafBounds::isValid() {
	return m_XMin <= m_XMax && m_YMin <= m_YMax && m_ZMin <= m_ZMax;
}

void mafBounds::unite(const mafBounds &b, mafBounds &output) {
    double bounds[6];
    bounds[0] = MIN(this->xMin(), b.xMin());
    bounds[1] = MAX(this->xMax(), b.xMax());
    
    bounds[2] = MIN(this->yMin(), b.yMin());
    bounds[3] = MAX(this->yMax(), b.yMax());
    
    bounds[4] = MIN(this->zMin(), b.zMin());
    bounds[5] = MAX(this->zMax(), b.zMax());
    
    output.setBounds(bounds);
}

void mafBounds::intersect(const mafBounds &b, mafBounds &output) {
    
}

void mafBounds::setBounds(double b[6]) {
    m_XMin = b[0]; 
    m_XMax = b[1];
    m_YMin = b[2];
    m_YMax = b[3];
    m_ZMin = b[4];
    m_ZMax = b[5];
}

double mafBounds::length() {
    double b_diff[3];
    double temp;
    temp = xMax()- xMin();
    b_diff[0] = temp * temp;
    temp = yMax()- yMin();
    b_diff[1] = temp * temp;
    temp = zMax()- zMin();
    b_diff[2] = temp * temp;
    
    return sqrtl(b_diff[0] + b_diff[1] + b_diff[2]);
}

void mafBounds::center(double c[3]) {
    c[0] = (xMin() + xMax()) * .5;
    c[1] = (yMin() + yMax()) * .5;
    c[2] = (zMin() + zMax()) * .5;
}

bool mafBounds::isPointInBounds(mafPoint *p) {
    return p->x() >= m_XMin && p->x <= m_XMax &&
        p->y() >= m_YMin && p->y() <= m_YMax &&
        p->z() >= m_ZMin && p->z() <= m_ZMax;
}