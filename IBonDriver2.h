#pragma once
#include "IBonDriver.h"
class IBonDriver2:public IBonDriver{public:virtual LPCWSTR GetTunerName()=0;virtual const BOOL IsTunerOpening()=0;virtual LPCWSTR EnumTuningSpace(DWORD)=0;virtual LPCWSTR EnumChannelName(DWORD,DWORD)=0;virtual const BOOL SetChannel(DWORD,DWORD)=0;virtual const DWORD GetCurSpace()=0;virtual const DWORD GetCurChannel()=0;virtual void Release()=0;};
