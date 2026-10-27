#pragma once
#include <Windows.h>
class IBonDriver{public:virtual const BOOL OpenTuner()=0;virtual void CloseTuner()=0;virtual const BOOL SetChannel(BYTE)=0;virtual const float GetSignalLevel()=0;virtual const DWORD WaitTsStream(DWORD=0)=0;virtual const DWORD GetReadyCount()=0;virtual const BOOL GetTsStream(BYTE*,DWORD*,DWORD*)=0;virtual const BOOL GetTsStream(BYTE**,DWORD*,DWORD*)=0;virtual void PurgeTsStream()=0;virtual void Release()=0;};
