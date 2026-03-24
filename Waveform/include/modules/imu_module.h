#pragma once

#include "modules/math_utils.h"
#include "state/runtime_state.h"

bool imuModuleInit();
void imuModuleUpdate();
void imuModuleCaptureReference();

const MotionState &imuModuleState();
const MotionState &imuModuleDisplayState();
bool imuModuleIsReady();
Vec3 imuModuleReferenceDown();
Vec3 imuModuleReferenceAxisA();
Vec3 imuModuleReferenceAxisB();

bool imuModuleReferenceReady();
bool imuModuleReferenceCapturePending();
bool imuModuleDisplayValid();
bool imuModuleFilterReady();
float imuModuleDotPitch();
float imuModuleDotRoll();
void imuModuleSetDotPitch(float v);
void imuModuleSetDotRoll(float v);
void imuModuleSetFilterReady(bool v);
void imuModuleSetReferenceReady(bool v);
void imuModuleSetReferenceCapturePending(bool v);
void imuModuleSetReferenceDown(const Vec3 &v);
void imuModuleSetReferenceAxisA(const Vec3 &v);
void imuModuleSetReferenceAxisB(const Vec3 &v);

bool imuModuleConfigureWakeOnMotion();
