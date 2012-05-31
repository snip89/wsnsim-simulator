/**
 *
 * IField.h
 *
 **/

#ifndef IFIELD_H
#define IFIELD_H

#include <QtCore>

#include "IEnvironment.h"

#include "types.h"

class IField : public IEnvironment
{
public:
    IField()
    {
        interfaceInfo.name = "IField";
    }

    virtual double measure(double* coord, VirtualTime time) = 0;
};

#endif // IFIELD_H
