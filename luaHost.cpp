/**
 *
 * File: luaHost.cpp
 * Description: lua functions
 * Author: Yarygin Alexander <yarygin.alexander@gmail.com>
 *
 **/

#include <cassert>

#include "luaHost.h"
#include "eventHandler.h"
#include "simulator.h"

lua_State* LuaHost::m_lua = 0;
QString LuaHost::m_errorString;

ModuleID LuaHost::m_currentModule;
ModuleInstanceID LuaHost::m_currentModuleInstance;

QMap<ModuleID, ModuleInstanceID> LuaHost::m_modulesInstances;
QMap<ModuleID, QList<ModuleID> > LuaHost::m_moduleDeps;

QList<ModuleID> LuaHost::m_nodesModules;

QMap<ModuleID, QMap<ModuleInstanceID, int> > LuaHost::m_modulesRefs;
QMap<ModuleID, QMap<ModuleInstanceID, QMap<EventID, int> > > LuaHost::m_handlersRefs;
QMap<const void*, ModuleID> LuaHost::m_modulesPtrs;
QMap<const void*, ModuleInstanceID> LuaHost::m_modulesInstancesPtrs;

void LuaHost::open()
{
    // getting new state
    m_lua = luaL_newstate();

    luaL_openlibs(m_lua);

    lua_register(m_lua, "handleEvent", LuaHost::handleEvent);
    lua_register(m_lua, "postEvent", LuaHost::postEvent);

    // QString luaModules = "package.path = package.path .. \";"
    //     + currentProjectPath + "/?.lua\"";

    // luaL_dostring(L, luaModules.toUtf8().constData());

    // luaL_dostring(L, "jit.opt.start(3)");

    // return m_lua;
}

int LuaHost::loadFile(QString path, QString name)
{
    int ret = luaL_dofile(m_lua, path.toUtf8().constData());
    if (ret) {
        m_errorString = lua_tostring(m_lua, -1);
        return 0;
    }

    lua_getglobal(m_lua, name.toUtf8().constData());

    if (lua_istable(m_lua, -1)) {
        // set metatable
        lua_pushstring(m_lua, "__index");
        lua_getglobal(m_lua, name.toUtf8().constData());
        lua_settable(m_lua, -3);
        lua_pop(m_lua, 1);
    }
    else {
        m_errorString = "There is no module " + name + "in file " + path;
        return 0;
    }

    return 1;
}

void LuaHost::removeGlobalName(QString name)
{
    lua_pushnil(m_lua);
    lua_setglobal(m_lua, name.toUtf8().constData());
}

int LuaHost::createModule(ModuleID moduleID, ModuleInstanceID ID, QString name)
{
    lua_newtable(m_lua);
    lua_getglobal(m_lua, name.toUtf8().constData());
    lua_setmetatable(m_lua, -2);

    const void* p = lua_topointer(m_lua, -1);
    int ref = luaL_ref(m_lua, LUA_REGISTRYINDEX);

    m_modulesRefs[moduleID][ID] = ref;
    m_modulesPtrs[p] = moduleID;
    m_modulesInstancesPtrs[p] = ID;

    if (ID == 0)
        m_modulesInstances[moduleID] = 0;
    else
        m_modulesInstances[moduleID]++;

    return 1;
}

int LuaHost::initModule(ModuleID moduleID,
                        ModuleInstanceID ID,
                        ModuleType type,
                        QList<ModuleParameter> params,
                        QList<ModuleDependence> dependencies)
{
    setCurrentModule(moduleID, ID);

    // put on of the stack top module instance
    getInstance(moduleID, ID);

    lua_getfield(m_lua, -1, "init");
    if (!lua_isfunction(m_lua, -1)) {
        m_errorString = "module with ID " + QString::number(moduleID) + " has no init() method";
        return 0;
    }

    getInstance(moduleID, ID);

    // put on top of the stack module params
    createParams(params);

    if (type == ModuleType_Software
        || type == ModuleType_Hardware) {
        // FIXME: is ID always equals nodeID?
        lua_pushnumber(m_lua, ID);
        lua_setfield(m_lua, -2, "parentNode");

        // remember nodes modules for dispatch event params
        m_nodesModules += moduleID;
    }

    // put on top of the stack module interfaces
    createDependencies(ID, type, dependencies);

    // call init function
    if (lua_pcall(m_lua, 3, 0, 0)) {
        m_errorString = lua_tostring(m_lua, -1);

        return 0;
    }

    lua_pop(m_lua, 1);

    return 1;
}

void LuaHost::getInstance(ModuleID moduleID, ModuleInstanceID ID)
{
    // --- getting table for module
    int ref = m_modulesRefs[moduleID][ID];
    lua_rawgeti(m_lua, LUA_REGISTRYINDEX, ref);
    assert(lua_istable(m_lua, -1));
}

void LuaHost::createParams(QList<ModuleParameter> params)
{
    lua_newtable(m_lua);

    foreach(ModuleParameter param, params) {

        switch (param.type) {
        case ModuleParamType_INT:
            lua_pushnumber(m_lua, param.value.toInt());
            break;
        case ModuleParamType_UINT8:
        case ModuleParamType_UINT16:
        case ModuleParamType_UINT32:
        case ModuleParamType_UINT64:
            lua_pushnumber(m_lua, param.value.toUInt());
            break;
        case ModuleParamType_DOUBLE:
            lua_pushnumber(m_lua, param.value.toDouble());
            break;
        case ModuleParamType_STRING:
            lua_pushstring(m_lua, param.value.toString().toUtf8().constData());
            break;
        case ModuleParamType_NODES:
            lua_newtable(m_lua);
            lua_pushnumber(m_lua, param.value.toUInt());
            lua_setfield(m_lua, -2, "number");
            lua_newtable(m_lua);
            // FIXME: work for only one nodes generator
            for (quint16 i = 0; i < param.value.toUInt(); i++) {
                lua_pushnumber(m_lua, i);
                lua_rawseti(m_lua, -2, i);
            }
            lua_setfield(m_lua, -2, "nodes");
            break;
        case ModuleParamType_Undefined:
            break;
        }

        lua_setfield(m_lua, -2, param.name.toUtf8().constData());
    }
}

void LuaHost::createDependencies(ModuleInstanceID ID,
                                 ModuleType type,
                                 QList<ModuleDependence> dependencies)
{
    lua_newtable(m_lua);

    foreach(ModuleDependence dep, dependencies) {

        switch(dep.type) {

        case ModuleType_Environment:
            getInstance(dep.moduleID, 0);

            break;

        case ModuleType_Hardware:
        case ModuleType_Software:
            if (type == ModuleType_Hardware
                || type == ModuleType_Software) {

                getInstance(dep.moduleID, ID);

            }

            else if(type == ModuleType_Environment) {
                lua_newtable(m_lua);

                for (ModuleInstanceID i = 0; i <= m_modulesInstances[dep.moduleID]; i++) {
                    getInstance(dep.moduleID, i);

                    lua_rawseti(m_lua, -2, i);
                }
            }

            break;
        case ModuleType_Undefined:
            break;
        }

        lua_setfield(m_lua, -2, dep.name.toUtf8().constData());

        // remember module depends for event handling
        m_moduleDeps[m_currentModule] += dep.moduleID;
    }
}

void LuaHost::close()
{
    lua_close(m_lua);
}

void LuaHost::eventHandler(Event* event,
                           ModuleID moduleID,
                           ModuleInstanceID ID)
{
    int handlerRef = m_handlersRefs[moduleID][ID][event->ID];
    lua_rawgeti(m_lua, LUA_REGISTRYINDEX, handlerRef);

    // TODO: errors handling
    // if (!lua_isfunction(m_lua, -1)) {
    //     return 0;
    // }

    getInstance(moduleID, ID);

    QVector<EventParam> params = event->params;

    // remove nodeID param for nodes modules
    if (m_nodesModules.contains(moduleID)) {

        // filter wrong nodes modules
        if (params[0].value.u16 == ID)
            params.pop_front();
        else {
            lua_pop(m_lua, 2);
            return;
        }
    }

    foreach(EventParam param, params) {
        switch (param.type) {
        case INT32_TYPE:
            lua_pushnumber(m_lua, param.value.i32);
            break;
        case BOOL_TYPE:
            lua_pushboolean(m_lua, param.value.b);
            break;
        case UINT8_TYPE:
            lua_pushnumber(m_lua, param.value.u8);
            break;
        case UINT16_TYPE:
            lua_pushnumber(m_lua, param.value.u16);
            break;
        case UINT32_TYPE:
            lua_pushnumber(m_lua, param.value.u32);
            break;
        case UINT64_TYPE:
            lua_pushnumber(m_lua, param.value.u64);
            break;
        case DOUBLE_TYPE:
            lua_pushnumber(m_lua, param.value.d);
            break;
        case STRING_TYPE:
            lua_pushstring(m_lua, param.value.string.data);
            break;
        case BYTE_ARRAY_TYPE:
            lua_createtable(m_lua, param.value.byteArray.size, 0);
            for (quint8 i = 0; i < param.value.byteArray.size;  i++) {
                lua_pushnumber(m_lua, param.value.byteArray.data[i]);
                lua_rawseti(m_lua, -2, i);
            }
            break;
        case UNKNOWN_TYPE:
            break;
        }
    }

    // TODO: errors handling
    int ret = lua_pcall(m_lua, 1 + params.size(), 0, 0);
    if (ret) {
        qDebug() << lua_tostring(m_lua, -1);
        lua_pop(m_lua, 1);
    }

}

QString LuaHost::errorString()
{
    return m_errorString;
}

int LuaHost::handleEvent(lua_State* lua)
{
    QString eventName = lua_tostring(lua, -2);
    const char* eventHandlerName = lua_tostring(lua, -1);

    getInstance(m_currentModule, m_currentModuleInstance);
    lua_getfield(m_lua, -1, eventHandlerName);
    int ref;
    if (lua_isfunction(m_lua, -1)) {
        ref = luaL_ref(m_lua, LUA_REGISTRYINDEX);
    }
    else {
        lua_pop(lua, 1);
    }

    lua_pop(lua, 1);

    // FIXME: memory leak
    EventHandler* handler = new EventHandler(m_currentModule,
                                             m_currentModuleInstance);

    QMap<ModuleID, EventID> events = Simulator::getEventID(eventName);

    // FIXME: wrong!
    ModuleID maybeEvent = events.value(m_currentModule, 255);
    if (maybeEvent != 255) {
        Simulator::registerEventHandler(maybeEvent, handler);
        m_handlersRefs[m_currentModule][m_currentModuleInstance][maybeEvent] = ref;
    }

    // handle events from depends modules
    foreach(ModuleID dep, events.keys()) {
        if (m_moduleDeps[m_currentModule].contains(dep)) {
            Simulator::registerEventHandler(events[dep], handler);
            m_handlersRefs[m_currentModule][m_currentModuleInstance][events[dep]] = ref;
        }
    }

    return 1;
}

int LuaHost::postEvent(lua_State* lua)
{
    QString name;
    VirtualTime delay = 0;
    QVector<EventParam> params;
    const void* table_p = NULL;
    ModuleID module;
    ModuleInstanceID id;

    // get args from table
    if (lua_istable(lua, -1)) {
        lua_getfield(lua, -1, "author");
        if (!lua_isnil(lua, -1))
            table_p = lua_topointer(lua, -1);
        lua_pop(lua, 1);

        lua_getfield(lua, -1, "event");
        if (!lua_isnil(lua, -1))
            name = lua_tostring(lua, -1);
        lua_pop(lua, 1);

        lua_getfield(lua, -1, "delay");
        if (!lua_isnil(lua, -1))
            delay = lua_tonumber(lua, -1);
        lua_pop(lua, 1);

        lua_getfield(lua, -1, "args");
        if (lua_istable(lua, -1)) {

            if (table_p != NULL) {
                module = m_modulesPtrs[table_p];
                id = m_modulesInstancesPtrs[table_p];
            }
            else {
                qDebug() << "no author of event";
                return 1;
            }

            params = Simulator::getEventParams(name, module);

            // params counter
            // begins from 0 for env modules
            // and from 1 for nodes modules
            int i = 0;

            if (m_nodesModules.contains(module)) {
                params[0].value.u16 = id;
                i = 1;
            }

            lua_pushnil(lua);

            for (; i < params.size(); i++) {
                lua_next(lua, -2);

                if (lua_isnil(lua, -1))
                    // TODO: error handling
                    break;

                switch (params[i].type) {
                case INT32_TYPE:
                    if (lua_isnumber(lua, -1))
                        params[i].value.i32 = lua_tonumber(lua, -1);
                    else
                        qDebug() << "got" << lua_typename(lua, -1) << "while expecting number";
                    break;
                case BOOL_TYPE:
                    if (lua_isboolean(lua, -1))
                        params[i].value.b = lua_toboolean(lua, -1);
                    else
                        qDebug() << "got" << lua_typename(lua, -1) << "while expecting bool";
                    break;
                case UINT8_TYPE:
                    if (lua_isnumber(lua, -1))
                        params[i].value.u8 = lua_tonumber(lua, -1);
                    else
                        qDebug() << "got" << lua_typename(lua, -1) << "while expecting number";
                    break;
                case UINT16_TYPE:
                    if (lua_isnumber(lua, -1))
                        params[i].value.u16 = lua_tonumber(lua, -1);
                    else
                        qDebug() << "got" << lua_typename(lua, -1) << "while expecting number";
                    break;
                case UINT32_TYPE:
                    if (lua_isnumber(lua, -1))
                        params[i].value.u32 = lua_tonumber(lua, -1);
                    else
                        qDebug() << "got" << lua_typename(lua, -1) << "while expecting number";
                    break;
                case UINT64_TYPE:
                    if (lua_isnumber(lua, -1))
                        params[i].value.u64 = lua_tonumber(lua, -1);
                    else
                        qDebug() << "got" << lua_typename(lua, -1) << "while expecting number";
                    break;
                case DOUBLE_TYPE:
                    if (lua_isnumber(lua, -1))
                        params[i].value.d = lua_tonumber(lua, -1);
                    else
                        qDebug() << "got" << lua_typename(lua, -1) << "while expecting number";
                    break;
                case STRING_TYPE:
                    if (lua_isstring(lua, -1)) {
                        const char* string = lua_tostring(lua, -1);
                        int length = strlen(string);
                        char* strparam = new char[length];
                        strcpy(strparam, string);
                        params[i].value.string.data = strparam;
                        params[i].value.string.length = length;
                    }
                    else
                        qDebug() << "got" << lua_typename(lua, -1) << "while expecting string";
                    break;
                case BYTE_ARRAY_TYPE:
                    if (lua_istable(lua, -1)) {
                        quint8 size = luaL_getn(lua, -1);
                        char* data = new char[size];
                        for (quint8 idx = 1; idx <= size; idx++) {
                            lua_rawgeti(lua, -1, idx);
                            data[idx-1] = lua_tonumber(lua, -1);
                            lua_pop(lua, 1);
                        }
                        params[i].value.byteArray.data = data;
                        params[i].value.byteArray.size = size;
                    }
                    else
                        qDebug() << "got" << lua_typename(lua, -1) << "while expecting table";
                    break;
                case UNKNOWN_TYPE:
                    break;
                }

                lua_pop(lua, 1);
            }
        }
        lua_pop(lua, 2);

        Simulator::postEvent(module, name, delay, params);

    }

    return 1;
}

void LuaHost::setCurrentModule(ModuleID moduleID, ModuleInstanceID ID)
{
    m_currentModule = moduleID;
    m_currentModuleInstance = ID;
}