/**
 *
 * File: moduleAdapter.h
 * Description: module adapter functions
 * Author: Yarygin Alexander <yarygin.alexander@gmail.com>
 *
 **/

#ifndef MODULEADAPTER_H
#define MODULEADAPTER_H

#include "module.h"

class ModuleAdapter {
public:
    ModuleAdapter(ModuleInitData moduleInitData);

    virtual ~ModuleAdapter(){}
    virtual int load() = 0;
    virtual Module* create() = 0;
    virtual void init(Module* module, QList<Module*> dependencies) = 0;
    virtual QString errorString() = 0;

protected:
    ModuleInitData m_moduleInitData;
};

#endif // MODULEADAPTER_H