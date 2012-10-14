/**
 *
 * File: project.h
 * Description: initialization of simulator's project
 * Author: Yarygin Alexander <yarygin.alexander@gmail.com>
 *
 **/

#ifndef PROJECT_H
#define PROJECT_H

#include <QtCore>

#include "types.h"
#include "projectParams.h"
#include "module.h"
#include "moduleAdapter.h"

#define MODULES_ENABLED
#define LUA_ENABLED

class Project {
public:
    Project(QString projectFileName);

    int load();

    int initSimulator();
    int initLog();

#ifdef MODULES_ENABLED

#ifdef LUA_ENABLED
    int initLua();
#endif

    int loadModules();
    int initModules();

#endif

    QString errorString();

private:
    ModuleType getType(QString type);

    QString m_projectFileName;
    ProjectParams m_projectParams;
    QString m_errorString;
    QList<ModuleAdapter*> m_envAdapters;
    QList<ModuleAdapter*> m_nodeAdapters;

    QHash<ModuleID, ModuleType> m_moduleType;

    QHash<ModuleID, ModuleInstanceID> m_envModules;
    // TODO: on future
    // QHash<ModuleID, QHash<SceneID, Module*> > m_envModules;
    QHash<ModuleID, QHash<NodeID, ModuleInstanceID> > m_nodeModules;
};

#endif // PROJECT_H
