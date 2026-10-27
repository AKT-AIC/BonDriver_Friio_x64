/*
 * BonDriver_Friio_x64 Ver.1.0
 * Copyright (C) 2026 AKT
 *
 * 64-bit BonDriver implementation for White Friio terrestrial tuners.
 * Developed through hardware testing and analysis of BonSDK materials and
 * BonDriver_Friio 3.1/3.2 behavior. IBonDriver.h and IBonDriver2.h originate
 * from BonSDK and retain their original notices.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "IBonDriver2.h"
#include <setupapi.h>
#include <winusb.h>
#include <vector>
#include <array>
#include <deque>
#include <algorithm>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>
namespace{
constexpr DWORD BAD=0xffffffffu,PS=188,PC=256,BS=PS*PC;
constexpr wchar_t TN[]=L"BonDriver_Friio_x64 Ver1.0",SN[]=L"UHF";
static const wchar_t* const CHANNEL_NAMES[50]={
 L"UHF 13ch",
 L"UHF 14ch",
 L"UHF 15ch",
 L"UHF 16ch",
 L"UHF 17ch",
 L"UHF 18ch",
 L"UHF 19ch",
 L"UHF 20ch",
 L"UHF 21ch",
 L"UHF 22ch",
 L"UHF 23ch",
 L"UHF 24ch",
 L"UHF 25ch",
 L"UHF 26ch",
 L"UHF 27ch",
 L"UHF 28ch",
 L"UHF 29ch",
 L"UHF 30ch",
 L"UHF 31ch",
 L"UHF 32ch",
 L"UHF 33ch",
 L"UHF 34ch",
 L"UHF 35ch",
 L"UHF 36ch",
 L"UHF 37ch",
 L"UHF 38ch",
 L"UHF 39ch",
 L"UHF 40ch",
 L"UHF 41ch",
 L"UHF 42ch",
 L"UHF 43ch",
 L"UHF 44ch",
 L"UHF 45ch",
 L"UHF 46ch",
 L"UHF 47ch",
 L"UHF 48ch",
 L"UHF 49ch",
 L"UHF 50ch",
 L"UHF 51ch",
 L"UHF 52ch",
 L"UHF 53ch",
 L"UHF 54ch",
 L"UHF 55ch",
 L"UHF 56ch",
 L"UHF 57ch",
 L"UHF 58ch",
 L"UHF 59ch",
 L"UHF 60ch",
 L"UHF 61ch",
 L"UHF 62ch"
};
constexpr GUID G[]={ {0x0b3e54a5,0x11a0,0x49a7,{0x89,0x9c,0x43,0xb2,0xa5,0x0c,0x38,0xde}}, {0x5a56d255,0xe23a,0x4b4d,{0x8c,0x78,0xc2,0x65,0x93,0x0b,0x6c,0x68}}, {0xae18aa60,0x7f6a,0x11d4,{0x97,0xdd,0x00,0x01,0x02,0x29,0xb9,0x59}}};
HMODULE gm{};SRWLOCK gl=SRWLOCK_INIT;
int ConfigInt(const wchar_t*section,const wchar_t*key,int def){wchar_t p[32768]{};GetModuleFileNameW(gm,p,_countof(p));if(auto*d=wcsrchr(p,L'\\'))*(d+1)=0;wcscat_s(p,L"BonDriver_Friio_x64.ini");wchar_t v[32]{},dv[16]{};_snwprintf_s(dv,_countof(dv),_TRUNCATE,L"%d",def);GetPrivateProfileStringW(section,key,dv,v,_countof(v),p);wchar_t*end=nullptr;long n=wcstol(v,&end,10);if(end==v||*end!=0)return def;return (int)n;}
int LogLevel(){int n=ConfigInt(L"Log",L"LogLevel",1);return n<0?0:n>2?2:n;}
void lgImpl(int level,const char*f,...){if(LogLevel()<level)return;char m[1000]{},l[1250]{};va_list a;va_start(a,f);_vsnprintf_s(m,_countof(m),_TRUNCATE,f,a);va_end(a);SYSTEMTIME st{};GetLocalTime(&st);_snprintf_s(l,_countof(l),_TRUNCATE,"%04u-%02u-%02u %02u:%02u:%02u.%03u [TID:%lu] %s\r\n",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,st.wMilliseconds,GetCurrentThreadId(),m);wchar_t p[32768]{};GetModuleFileNameW(gm,p,_countof(p));if(auto*d=wcsrchr(p,L'.'))*d=0;wcscat_s(p,L".log");AcquireSRWLockExclusive(&gl);HANDLE h=CreateFileW(p,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,0,nullptr);if(h!=INVALID_HANDLE_VALUE){DWORD n;WriteFile(h,l,(DWORD)strlen(l),&n,nullptr);CloseHandle(h);}ReleaseSRWLockExclusive(&gl);}
#define lg(...) lgImpl(1,__VA_ARGS__)
#define lgd(...) lgImpl(2,__VA_ARGS__)
class D final:public IBonDriver2{
 enum class NS{FREE,PENDING,READY};
 static constexpr int kNodeCount=344;
 static constexpr int kMaxPending=16;
 struct PumpNode{OVERLAPPED ov{};std::array<BYTE,BS> data{};ULONG bytes{};NS state{NS::FREE};ULONGLONG sequence{};DWORD generation{};DWORD completionError{};};
 SRWLOCK lk{};bool op{},tu{},diagnosticsDone{},pumpStop{},pumpRunning{};DWORD sp{BAD},ch{BAD},blocks{},controlCalls{},tsReadBytes{},switchCount{},generation{};BYTE realTs[BS]{};ULONGLONG due{},nextSequence{};BYTE cc{},buf[BS]{};HANDLE dev{INVALID_HANDLE_VALUE},pumpThread{},readyEvent{};WINUSB_INTERFACE_HANDLE usb{};std::array<PumpNode,kNodeCount> nodes{};int submitIndex{},completeIndex{},consumeIndex{};DWORD pendingCount{},readyCount{};
 static DWORD WINAPI PumpThunk(LPVOID q){return static_cast<D*>(q)->PumpLoop();}
 bool SubmitLocked(int i){auto&n=nodes[i];if(!usb||dev==INVALID_HANDLE_VALUE||pumpStop||n.state!=NS::FREE||pendingCount>=kMaxPending)return false;if(!n.ov.hEvent)n.ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!n.ov.hEvent){lg("TS ERROR generation=%lu stage=CREATE_EVENT index=%d error=%lu",generation,i,GetLastError());return false;}HANDLE e=n.ov.hEvent;ResetEvent(e);ZeroMemory(&n.ov,sizeof(n.ov));n.ov.hEvent=e;n.bytes=0;n.completionError=ERROR_SUCCESS;n.sequence=nextSequence++;n.generation=generation;SetLastError(ERROR_SUCCESS);BOOL ok=WinUsb_ReadPipe(usb,0x81,n.data.data(),BS,&n.bytes,&n.ov);DWORD er=ok?ERROR_SUCCESS:GetLastError();if(ok||er==ERROR_IO_PENDING){n.state=NS::PENDING;pendingCount++;submitIndex=(submitIndex+1)%kNodeCount;if(n.sequence<16||n.sequence%100==0)lgd("TS SUBMIT generation=%lu sequence=%llu index=%d pending=%lu result=%s error=%lu",n.generation,n.sequence,i,pendingCount,ok?"IMMEDIATE":"PENDING",er);return true;}lg("TS ERROR generation=%lu stage=SUBMIT index=%d error=%lu",generation,i,er);return false;}
 DWORD PumpLoop(){lg("TS WORKER START generation=%lu nodes=%d max_pending=%d",generation,kNodeCount,kMaxPending);for(;;){AcquireSRWLockExclusive(&lk);if(pumpStop){ReleaseSRWLockExclusive(&lk);break;}while(pendingCount<kMaxPending&&nodes[submitIndex].state==NS::FREE){if(!SubmitLocked(submitIndex))break;}int i=completeIndex;bool have=nodes[i].state==NS::PENDING;ReleaseSRWLockExclusive(&lk);if(!have){Sleep(1);continue;}DWORD got=0;SetLastError(ERROR_SUCCESS);BOOL gr=GetOverlappedResult(dev,&nodes[i].ov,&got,FALSE);DWORD ge=gr?ERROR_SUCCESS:GetLastError();if(!gr&&ge==ERROR_IO_INCOMPLETE){Sleep(10);continue;}AcquireSRWLockExclusive(&lk);auto&n=nodes[i];if(n.state==NS::PENDING){n.bytes=got;n.completionError=ge;n.state=NS::READY;pendingCount--;readyCount++;completeIndex=(completeIndex+1)%kNodeCount;SetEvent(readyEvent);if(!gr||got!=BS||n.sequence<16||n.sequence%100==0)lgd("TS COMPLETE generation=%lu sequence=%llu index=%d bytes=%lu completion=%s error=%lu pending=%lu ready=%lu",n.generation,n.sequence,i,got,gr?"SUCCESS":"ERROR_WITH_DATA",ge,pendingCount,readyCount);}ReleaseSRWLockExclusive(&lk);}lg("TS WORKER STOP generation=%lu",generation);return 0;}
 void StopPump(){AcquireSRWLockExclusive(&lk);bool active=pumpRunning;pumpStop=true;if(readyEvent)SetEvent(readyEvent);ReleaseSRWLockExclusive(&lk);if(!active)return;if(usb)WinUsb_AbortPipe(usb,0x81);for(auto&n:nodes)if(n.state==NS::PENDING)CancelIoEx(dev,&n.ov);if(pumpThread){WaitForSingleObject(pumpThread,5000);CloseHandle(pumpThread);pumpThread=nullptr;}DWORD drained=0,discarded=0;for(auto&n:nodes){if(n.state==NS::PENDING){DWORD got=0;GetOverlappedResult(dev,&n.ov,&got,TRUE);drained++;}if(n.state==NS::READY)discarded++;n.state=NS::FREE;n.bytes=0;n.completionError=ERROR_SUCCESS;}AcquireSRWLockExclusive(&lk);pendingCount=readyCount=0;submitIndex=completeIndex=consumeIndex=0;pumpRunning=false;if(readyEvent)ResetEvent(readyEvent);ReleaseSRWLockExclusive(&lk);lg("TS STOP generation=%lu pending_drained=%lu ready_discarded=%lu",generation,drained,discarded);}
 bool StartPumpLocked(){if(!usb||dev==INVALID_HANDLE_VALUE)return false;generation++;nextSequence=0;pumpStop=false;pumpRunning=true;pendingCount=readyCount=0;submitIndex=completeIndex=consumeIndex=0;if(readyEvent)ResetEvent(readyEvent);for(auto&n:nodes){n.state=NS::FREE;n.bytes=0;n.completionError=ERROR_SUCCESS;}ULONG timeout=0;UCHAR raw=TRUE;if(!WinUsb_SetPipePolicy(usb,0x81,PIPE_TRANSFER_TIMEOUT,sizeof(timeout),&timeout)||!WinUsb_SetPipePolicy(usb,0x81,RAW_IO,sizeof(raw),&raw)){pumpRunning=false;lg("TS ERROR generation=%lu stage=PIPE_POLICY error=%lu",generation,GetLastError());return false;}pumpThread=CreateThread(nullptr,0,PumpThunk,this,0,nullptr);if(!pumpThread){pumpRunning=false;lg("TS ERROR generation=%lu stage=CREATE_THREAD error=%lu",generation,GetLastError());return false;}lg("TS PUMP START generation=%lu nodes=%d max_pending=%d",generation,kNodeCount,kMaxPending);return true;}
 void closeusb(){StopPump();if(usb){BOOL r=WinUsb_Free(usb);lg("WinUsb_Free -> %s",r?"TRUE":"FALSE");usb=nullptr;}if(dev!=INVALID_HANDLE_VALUE){BOOL r=CloseHandle(dev);lg("CloseHandle -> %s",r?"TRUE":"FALSE");dev=INVALID_HANDLE_VALUE;}}
 BOOL openusb(){closeusb();lg("VER1.0 POLICY profile=WHITE_FRIIO_TERRESTRIAL configuration=INI expected_control_calls=3 max_transactions=6 two_channel_switch=TRUE real_ts_pump=TRUE pipe=0x81 read_size=48128 ring_nodes=344 max_pending=16 timeout_ms=0 timeout_policy=NO_TIMEOUT raw_io=TRUE real_ts_forwarded=TRUE");lg("Device enumeration begin");DWORD interfaces=0,openAttempts=0;for(const auto&g:G){HDEVINFO h=SetupDiGetClassDevsW(&g,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);if(h==INVALID_HANDLE_VALUE)continue;for(DWORD i=0;;i++){SP_DEVICE_INTERFACE_DATA x{};x.cbSize=sizeof(x);if(!SetupDiEnumDeviceInterfaces(h,nullptr,&g,i,&x))break;++interfaces;DWORD z=0;SetupDiGetDeviceInterfaceDetailW(h,&x,nullptr,0,&z,nullptr);if(!z)continue;std::vector<BYTE>v(z);auto*d=(SP_DEVICE_INTERFACE_DETAIL_DATA_W*)v.data();d->cbSize=sizeof(*d);if(!SetupDiGetDeviceInterfaceDetailW(h,&x,d,z,nullptr,nullptr))continue;++openAttempts;HANDLE fd=CreateFileW(d->DevicePath,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);if(fd==INVALID_HANDLE_VALUE){lg("CreateFileW -> FALSE error=%lu",GetLastError());continue;}WINUSB_INTERFACE_HANDLE u{};if(!WinUsb_Initialize(fd,&u)){lg("DEVICE_PROBE_RESULT = WINUSB_INITIALIZE_FAILED error=%lu",GetLastError());CloseHandle(fd);continue;}dev=fd;usb=u;SetupDiDestroyDeviceInfoList(h);lg("WinUsb_Initialize -> TRUE");lg("DEVICE_PROBE_RESULT = WHITE_FRIIO_FOUND interfaces=%lu open_attempts=%lu",interfaces,openAttempts);return TRUE;}SetupDiDestroyDeviceInfoList(h);}lg("DEVICE_PROBE_RESULT = NO_SUPPORTED_DEVICE interfaces=%lu open_attempts=%lu",interfaces,openAttempts);lg("DEVICE PATH: WinUsb_Initialize=NOT_REACHED");lg("CONTROL SUMMARY prepared=0 executed=0 blocked=0");return FALSE;}
 void diagnostics(){if(diagnosticsDone){lg("DIAGNOSTICS REJECTED: already prepared");return;}diagnosticsDone=true;struct C{const char*n;BYTE rt,rq;USHORT v,i,l;const char*e,*why;};static constexpr C c[]={
 {"TYPE5_PROBE_D000_0000",0xC0,0x02,0xD000,0x0000,1,"DRIVER_3_2_ONLY","WRITE prerequisite unresolved"},
 {"TYPE1_PROBE_3000_0100",0xC0,0x02,0x3000,0x0100,1,"DRIVER_3_2_ONLY","preceding OUT and mode dependency"},
 {"TYPE2_OR_4_PROBE_3000_0080",0xC0,0x02,0x3000,0x0080,1,"PARTIAL_MATCH","BonSDK match but standalone use unapproved"},
 {"INDIRECT_12_0380_OUT",0x40,0x03,0x1200,0x0100,2,"DRIVER_3_2_ONLY","sends 03 80; side effect unresolved"},
 {"INDIRECT_12_0380_IN",0xC0,0x02,0x1200,0x0100,2,"DRIVER_3_2_ONLY","depends on paired OUT; ID 01B3"},
 {"INDIRECT_90_0380_OUT",0x40,0x03,0x9000,0x0100,2,"DRIVER_3_2_ONLY","sends 03 80; side effect unresolved"},
 {"INDIRECT_90_0380_IN",0xC0,0x02,0x9000,0x0100,2,"DRIVER_3_2_ONLY","depends on paired OUT; ID 01B3"},
 {"BONSDK_WHITE_SELECT_3000_00FE",0x40,0x03,0x3000,0x00FE,5,"BONSDK_32_ONLY","payload and 3.2 order unresolved"},
 {"BONSDK_WHITE_STATUS_3000_00B0",0xC0,0x02,0x3000,0x00B0,1,"BONSDK_32_ONLY","selection sequence reference only"},
 {"BONSDK_WHITE_SIGNAL_3000_0089",0xC0,0x02,0x3000,0x0089,0x25,"BONSDK_32_ONLY","3.2 white signal path unresolved"}};lg("WHITE PATH CANDIDATES BEGIN");for(const auto&x:c)lg("CONTROL CANDIDATE name=%s RequestType=0x%02X Request=0x%02X Value=0x%04X Index=0x%04X Length=%u evidence=%s execution_allowed=FALSE status=BLOCKED reason=%s",x.n,x.rt,x.rq,x.v,x.i,x.l,x.e,x.why);lg("CONTROL SUMMARY prepared=%zu executed=0 blocked=%zu",_countof(c),_countof(c));lg("WHITE PATH CANDIDATES END");lg("CONTROL DIAGNOSTICS execution=DISABLED observation_only=TRUE");}
 void make(){for(DWORD i=0;i<PC;i++){BYTE*p=buf+i*PS;p[0]=0x47;p[1]=0x1f;p[2]=0xff;p[3]=0x10|(cc++&15);memset(p+4,0xff,PS-4);}}
enum class WhiteProbeStatus{WHITE,NOT_WHITE,FAILED};
 struct WhiteInitEntry{BYTE reg;BYTE value;};
 enum class WhiteInitExecStatus{DISABLED,NOT_WHITE,PRECONDITION_FAILED,PASS,FAIL};
 struct WhiteInitExecResult{WhiteInitExecStatus status{WhiteInitExecStatus::DISABLED};DWORD planned{34},attempted{},succeeded{},failedSequence{},lastError{};};
 static const std::array<WhiteInitEntry,34>& WhiteInitTable(){static const std::array<WhiteInitEntry,34> t={{{0x01,0x40},{0x04,0x38},{0x05,0x40},{0x07,0x40},{0x0F,0x4F},{0x11,0x21},{0x12,0x0B},{0x13,0x2F},{0x14,0x31},{0x16,0x02},{0x21,0xC4},{0x22,0x20},{0x2C,0x79},{0x2D,0x34},{0x2F,0x00},{0x30,0x28},{0x31,0x31},{0x32,0xDF},{0x38,0x01},{0x39,0x78},{0x48,0x90},{0x51,0x68},{0x5E,0x38},{0x71,0x00},{0x72,0x08},{0x77,0x00},{0xC0,0x21},{0xC1,0x10},{0xE4,0x1A},{0xEA,0x1F},{0x77,0x00},{0x71,0x00},{0x71,0x00},{0x76,0x0C}}};return t;}
 void ConfigPath(wchar_t(&p)[32768]){GetModuleFileNameW(gm,p,_countof(p));if(auto*d=wcsrchr(p,L'\\'))*(d+1)=0;wcscat_s(p,L"BonDriver_Friio_x64.ini");}
 bool ConfigEnabled(const wchar_t*section,const wchar_t*key){wchar_t p[32768]{};ConfigPath(p);wchar_t v[32]{};GetPrivateProfileStringW(section,key,L"1",v,_countof(v),p);return wcstol(v,nullptr,10)!=0;}
 bool IsWhiteProbeEnabled(){return ConfigEnabled(L"WhiteFriio",L"EnableWhiteProbe");}
 bool IsWhiteInitEnabled(){return ConfigEnabled(L"WhiteFriio",L"EnableWhiteInit");}
 bool IsWhiteInit34Enabled(){return ConfigEnabled(L"WhiteFriio",L"EnableWhiteInit34");}
 bool IsShiroPostProbe3Enabled(){return ConfigEnabled(L"WhiteFriio",L"EnableShiroPostProbe3");}
 bool IsPrecontrolEnabled(){return ConfigEnabled(L"WhiteFriio",L"EnablePrecontrol");}
 WhiteProbeStatus ProbeWhiteFriio(BYTE&observed){observed=0xFF;
  if(!usb){lg("WHITE PROBE RESULT status=FAILED stage=PRECHECK reason=WINUSB_HANDLE_NULL existing_path_unchanged=TRUE");return WhiteProbeStatus::FAILED;}
  BYTE command=0xC1;WINUSB_SETUP_PACKET out{};out.RequestType=0x40;out.Request=0x03;out.Value=0x3000;out.Index=0x00FE;out.Length=1;ULONG outTransferred=0;SetLastError(ERROR_SUCCESS);BOOL outOk=WinUsb_ControlTransfer(usb,out,&command,1,&outTransferred,nullptr);DWORD outError=outOk?ERROR_SUCCESS:GetLastError();
  lg("WHITE PROBE CONTROL stage=OUT request_type=0x%02X request=0x%02X value=0x%04X index=0x%04X data=0x%02X result=%s transferred=%lu expected=1 last_error=%lu",out.RequestType,out.Request,out.Value,out.Index,command,outOk?"TRUE":"FALSE",outTransferred,outError);
  if(!outOk||outTransferred!=1){lg("WHITE PROBE RESULT status=FAILED stage=OUT existing_path_unchanged=TRUE");return WhiteProbeStatus::FAILED;}
  BYTE response=0xFF;WINUSB_SETUP_PACKET in{};in.RequestType=0xC0;in.Request=0x02;in.Value=0x3000;in.Index=0x0100;in.Length=1;ULONG inTransferred=0;SetLastError(ERROR_SUCCESS);BOOL inOk=WinUsb_ControlTransfer(usb,in,&response,1,&inTransferred,nullptr);DWORD inError=inOk?ERROR_SUCCESS:GetLastError();observed=response;
  lg("WHITE PROBE CONTROL stage=IN request_type=0x%02X request=0x%02X value=0x%04X index=0x%04X response=0x%02X result=%s transferred=%lu expected=1 last_error=%lu",in.RequestType,in.Request,in.Value,in.Index,response,inOk?"TRUE":"FALSE",inTransferred,inError);
  if(!inOk||inTransferred!=1){lg("WHITE PROBE RESULT status=FAILED stage=IN response=0x%02X existing_path_unchanged=TRUE",response);return WhiteProbeStatus::FAILED;}
  WhiteProbeStatus result=(response!=0x00&&response!=0xFF)?WhiteProbeStatus::WHITE:WhiteProbeStatus::NOT_WHITE;
  lg("WHITE PROBE RESULT status=%s response=0x%02X rule=response_not_00_and_not_FF existing_path_unchanged=TRUE",result==WhiteProbeStatus::WHITE?"WHITE":"NOT_WHITE",response);return result;
 }
  struct ShiroPostProbe3Result{DWORD planned=3,attempted=0,succeeded=0,failedSequence=0,lastError=0;bool pass=false;};
 ShiroPostProbe3Result ExecuteShiroPostProbe3(){ShiroPostProbe3Result r{};struct Req{USHORT index;BYTE data;};static const Req q[3]={{0x0030,0x04},{0x0000,0x01},{0x0006,0x1F}};
  lg("SHIRO POST-PROBE3 BEGIN source=BonDriver_Friio_3.1_3.2 planned=3 order=AFTER_WHITE_PROBE_BEFORE_PRECONTROL");
  for(DWORD i=0;i<r.planned;++i){BYTE data=q[i].data;WINUSB_SETUP_PACKET z{};z.RequestType=0x40;z.Request=0x03;z.Value=0x0000;z.Index=q[i].index;z.Length=1;ULONG transferred=0;SetLastError(ERROR_SUCCESS);BOOL ok=WinUsb_ControlTransfer(usb,z,&data,1,&transferred,nullptr);DWORD error=ok?ERROR_SUCCESS:GetLastError();controlCalls++;r.attempted++;bool pass=ok&&transferred==1;lgd("SHIRO POST-PROBE3 RESULT seq=%lu/3 request_type=0x40 request=0x03 setup_value=0x0000 setup_index=0x%04X data=0x%02X result=%s transferred=%lu expected=1 last_error=%lu status=%s",i+1,z.Index,data,ok?"TRUE":"FALSE",transferred,error,pass?"PASS":"FAIL");if(!pass){r.failedSequence=i+1;r.lastError=error;break;}r.succeeded++;}
  r.pass=r.succeeded==r.planned;lg("SHIRO POST-PROBE3 FINAL planned=%lu attempted=%lu succeeded=%lu failed_sequence=%lu last_error=%lu status=%s",r.planned,r.attempted,r.succeeded,r.failedSequence,r.lastError,r.pass?"PASS":"FAIL");return r;}
 struct PrecontrolResult{DWORD planned=70,attempted=0,succeeded=0,failedSequence=0,lastError=0;BYTE finalState=0;bool pass=false;};
 PrecontrolResult ExecuteWhitePrecontrol_100099A0(){PrecontrolResult r{};BYTE state=0x03;
  auto emit=[&](const char*phase,int bit,BYTE value)->bool{WINUSB_SETUP_PACKET z{};z.RequestType=0x40;z.Request=0x03;z.Value=0;z.Index=0;z.Length=1;ULONG n=0;SetLastError(ERROR_SUCCESS);BOOL ok=WinUsb_ControlTransfer(usb,z,&value,1,&n,nullptr);DWORD e=ok?0:GetLastError();++r.attempted;bool pass=ok&&n==1;lg("WHITE PRECONTROL EXEC-B seq=%lu/%lu phase=%s bit=%d state=0x%02X result=%s transferred=%lu expected=1 last_error=%lu status=%s",r.attempted,r.planned,phase,bit,value,ok?"TRUE":"FALSE",n,e,pass?"PASS":"FAIL");if(!pass){r.failedSequence=r.attempted;r.lastError=e;return false;}++r.succeeded;return true;};
  auto sendBit=[&](const char*phase,int bit,bool one,bool first)->bool{state=(BYTE)((state&(first?0xF3:0xF7))|(one?0x08:0));state=(BYTE)(state&0xFB);if(!emit(phase,bit,state))return false;state=(BYTE)(state|0x04);return emit(phase,bit,state);};
  lg("WHITE PRECONTROL EXEC-B BEGIN source=3.2_address_0x100099A0 args=2,100,0,255,100 planned=%lu initial_state=0x%02X",r.planned,state);const BYTE a[5]={2,100,0,255,100};const char*n[5]={"ARG1_2BIT","ARG2_8BIT","ARG3_8BIT","ARG4_8BIT","ARG5_8BIT"};
  for(int b=1;b>=0;--b)if(!sendBit(n[0],b,(a[0]&(1u<<b))!=0,true))goto done;for(int i=1;i<5;++i)for(int b=7;b>=0;--b)if(!sendBit(n[i],b,(a[i]&(1u<<b))!=0,false))goto done;state=(BYTE)(state&0xF9);if(!emit("FINAL_LOW",-1,state))goto done;state=(BYTE)(state|4);if(!emit("FINAL_HIGH",-1,state))goto done;r.pass=true;
 done:r.finalState=state;lg("WHITE PRECONTROL EXEC-B FINAL planned=%lu attempted=%lu succeeded=%lu failed_sequence=%lu last_error=%lu final_state=0x%02X status=%s",r.planned,r.attempted,r.succeeded,r.failedSequence,r.lastError,r.finalState,r.pass?"PASS":"FAIL");return r;}
 void ObserveWhiteInitializationPlan(){const auto&t=WhiteInitTable();lg("WHITE INIT OBSERVE BEGIN mode=PLAN_ONLY execution_allowed=FALSE source=3.2_type1_branch table_entries=%zu",t.size());lg("WHITE INIT OBSERVE PRECONTROL source_address=0x1000A83F action=CALL_0x100099A0 args=2,100,0,255,100 execution_allowed=FALSE status=EXCLUDED_IN_EXEC_A");for(size_t i=0;i<t.size();++i){const auto&e=t[i];lgd("WHITE INIT OBSERVE REG seq=%zu/%zu reg=0x%02X value=0x%02X intended_out=RequestType_0x40_Request_0x03_Value_0x3000_Index_0x%04X_Length_1 execution_allowed=FALSE status=PLANNED",i+1,t.size(),e.reg,e.value,e.reg);}lg("WHITE INIT OBSERVE RESULT prepared=%zu executed=0 status=PASS existing_path_unchanged=TRUE",t.size());lg("WHITE INIT OBSERVE END");}
 WhiteInitExecResult ExecuteWhiteInitializationA(WhiteProbeStatus probeStatus,BYTE probeResponse){WhiteInitExecResult r{};const auto&t=WhiteInitTable();r.planned=(DWORD)t.size();const bool enabled=IsWhiteInitEnabled(),white=probeStatus==WhiteProbeStatus::WHITE&&probeResponse!=0x00&&probeResponse!=0xFF,usbValid=usb!=nullptr,pumpInactive=!pumpRunning;lg("WHITE INIT GATE probe_status=%s probe_response=0x%02X init_enabled=%s usb_valid=%s pump_inactive=%s table_count=%zu decision=%s",probeStatus==WhiteProbeStatus::WHITE?"WHITE":probeStatus==WhiteProbeStatus::NOT_WHITE?"NOT_WHITE":"FAILED",probeResponse,enabled?"TRUE":"FALSE",usbValid?"TRUE":"FALSE",pumpInactive?"TRUE":"FALSE",t.size(),enabled&&white&&usbValid&&pumpInactive?"EXECUTE":"SKIP");
  if(!enabled){r.status=WhiteInitExecStatus::DISABLED;lg("WHITE INIT EXEC-B FINAL planned=%lu attempted=0 succeeded=0 status=NOT_EXECUTED reason=INIT_ARM_NOT_FOUND existing_path_continued=TRUE",r.planned);return r;}if(!white){r.status=WhiteInitExecStatus::NOT_WHITE;lg("WHITE INIT EXEC-B FINAL planned=%lu attempted=0 succeeded=0 status=NOT_EXECUTED reason=WHITE_PROBE_NOT_WHITE existing_path_continued=TRUE",r.planned);return r;}if(!usbValid||!pumpInactive){r.status=WhiteInitExecStatus::PRECONDITION_FAILED;lg("WHITE INIT EXEC-B FINAL planned=%lu attempted=0 succeeded=0 status=NOT_EXECUTED reason=PRECONDITION_FAILED existing_path_continued=TRUE",r.planned);return r;}
  lg("WHITE INIT EXEC-B BEGIN precontrol=CONFIG_CONTROLLED planned=%lu",r.planned);const bool shiroPostProbe3Enabled=IsShiroPostProbe3Enabled();lg("SHIRO POST-PROBE3 CONFIG enabled=%s action=%s",shiroPostProbe3Enabled?"TRUE":"FALSE",shiroPostProbe3Enabled?"EXECUTE":"SKIP");if(shiroPostProbe3Enabled){ShiroPostProbe3Result post=ExecuteShiroPostProbe3();if(!post.pass){r.status=WhiteInitExecStatus::PRECONDITION_FAILED;r.lastError=post.lastError;lg("WHITE INIT EXEC-B FINAL planned=%lu attempted=0 succeeded=0 remaining_skipped=%lu last_error=%lu status=SHIRO_POST_PROBE3_FAILED existing_path_continued=TRUE",r.planned,r.planned,r.lastError);return r;}}else{lg("SHIRO POST-PROBE3 SKIPPED reason=CONFIG_DISABLED");}const bool precontrolEnabled=IsPrecontrolEnabled();lg("WHITE PRECONTROL CONFIG enabled=%s action=%s",precontrolEnabled?"TRUE":"FALSE",precontrolEnabled?"EXECUTE":"SKIP");if(precontrolEnabled){PrecontrolResult pre=ExecuteWhitePrecontrol_100099A0();if(!pre.pass){r.status=WhiteInitExecStatus::FAIL;r.lastError=pre.lastError;lg("WHITE INIT EXEC-B FINAL planned=%lu attempted=0 succeeded=0 remaining_skipped=%lu last_error=%lu status=PRECONTROL_FAILED existing_path_continued=TRUE",r.planned,r.planned,r.lastError);return r;}}else{lg("WHITE PRECONTROL SKIPPED reason=CONFIG_DISABLED");}const bool init34Enabled=IsWhiteInit34Enabled();lg("WHITE INIT34 CONFIG enabled=%s action=%s",init34Enabled?"TRUE":"FALSE",init34Enabled?"EXECUTE":"SKIP");if(!init34Enabled){r.status=WhiteInitExecStatus::DISABLED;lg("WHITE INIT34 SKIPPED reason=CONFIG_DISABLED");lg("WHITE INIT EXEC-B FINAL planned=%lu attempted=0 succeeded=0 remaining_skipped=%lu last_error=0 status=SKIPPED_34_DISABLED existing_path_continued=TRUE",r.planned,r.planned);return r;}for(DWORD i=0;i<r.planned;++i){const auto&e=t[i];BYTE data=e.value;WINUSB_SETUP_PACKET z{};z.RequestType=0x40;z.Request=0x03;z.Value=0x3000;z.Index=(USHORT)e.reg;z.Length=1;ULONG transferred=0;SetLastError(ERROR_SUCCESS);lgd("WHITE INIT EXEC-B SEND seq=%lu/%lu reg=0x%02X value=0x%02X request_type=0x40 request=0x03 setup_value=0x%04X setup_index=0x%04X length=1",i+1,r.planned,e.reg,e.value,z.Value,z.Index);BOOL ok=WinUsb_ControlTransfer(usb,z,&data,1,&transferred,nullptr);DWORD error=ok?ERROR_SUCCESS:GetLastError();controlCalls++;r.attempted++;bool pass=ok&&transferred==1;lgd("WHITE INIT EXEC-B RESULT seq=%lu/%lu result=%s transferred=%lu expected=1 last_error=%lu status=%s",i+1,r.planned,ok?"TRUE":"FALSE",transferred,error,pass?"PASS":"FAIL");if(!pass){r.status=WhiteInitExecStatus::FAIL;r.failedSequence=i+1;r.lastError=error;break;}r.succeeded++;}if(r.succeeded==r.planned)r.status=WhiteInitExecStatus::PASS;lg("WHITE INIT EXEC-B FINAL planned=%lu attempted=%lu succeeded=%lu failed_sequence=%lu remaining_skipped=%lu last_error=%lu status=%s existing_path_continued=TRUE",r.planned,r.attempted,r.succeeded,r.failedSequence,r.planned-r.attempted,r.lastError,r.status==WhiteInitExecStatus::PASS?"PASS":"FAIL");return r;}
 void RunWhiteProbeObservation(){if(!IsWhiteProbeEnabled()){lg("WHITE PROBE RESULT status=DISABLED control_executed=0 existing_path_unchanged=TRUE");lg("WHITE INIT EXEC-B FINAL planned=34 attempted=0 succeeded=0 status=NOT_EXECUTED reason=WHITE_PROBE_DISABLED existing_path_continued=TRUE");return;}lg("WHITE PROBE BEGIN mode=OBSERVATION_ONLY source=3.2_core_branch");BYTE response=0xFF;WhiteProbeStatus r=ProbeWhiteFriio(response);if(r==WhiteProbeStatus::WHITE)ObserveWhiteInitializationPlan();(void)ExecuteWhiteInitializationA(r,response);}
public:D(){InitializeSRWLock(&lk);readyEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);lg("Constructor");}~D(){closeusb();for(auto&n:nodes)if(n.ov.hEvent)CloseHandle(n.ov.hEvent);if(readyEvent)CloseHandle(readyEvent);lg("Destructor blocks=%lu",blocks);}const BOOL OpenTuner()override{BOOL r=openusb();AcquireSRWLockExclusive(&lk);op=r!=FALSE;tu=false;sp=ch=BAD;due=0;diagnosticsDone=false;ReleaseSRWLockExclusive(&lk);if(r){RunWhiteProbeObservation();diagnostics();}lg("OpenTuner -> %s",r?"TRUE":"FALSE");return r;}void CloseTuner()override{StopPump();AcquireSRWLockExclusive(&lk);op=tu=false;sp=ch=BAD;ReleaseSRWLockExclusive(&lk);closeusb();lg("CloseTuner");}const BOOL SetChannel(BYTE b)override{return SetChannel(0,b);}const float GetSignalLevel()override{
    // Ver1.0 candidate: white Friio only. Keep all existing initialization/TS paths unchanged.
    if(!usb||dev==INVALID_HANDLE_VALUE)return 0.0f;
    BYTE b[3]{};
    WINUSB_SETUP_PACKET z{};
    z.RequestType=0xC0; z.Request=0x02; z.Value=0x3000; z.Index=0x008B; z.Length=3;
    ULONG transferred=0;
    SetLastError(ERROR_SUCCESS);
    BOOL ok=WinUsb_ControlTransfer(usb,z,b,3,&transferred,nullptr);
    DWORD error=ok?ERROR_SUCCESS:GetLastError();
    if(!ok||transferred!=3){lgd("SIGNAL READ result=%s transferred=%lu expected=3 last_error=%lu status=FAIL",ok?"TRUE":"FALSE",transferred,error);return 0.0f;}
    const DWORD raw=(static_cast<DWORD>(b[0])<<16)|(static_cast<DWORD>(b[1])<<8)|static_cast<DWORD>(b[2]);
    if(raw==0){lgd("SIGNAL READ raw=0 status=ZERO");return 0.0f;}
    const double t=10.0*std::log10(5505024.0/static_cast<double>(raw));
    const double t2=t*t, t3=t2*t, t4=t2*t2;
    const double signal=0.000024*t4-0.0016*t3+0.0398*t2+0.5491*t+3.0965;
    const float result=static_cast<float>(signal);
    lgd("SIGNAL READ raw=%lu bytes=%02X%02X%02X level=%.6f status=PASS",raw,b[0],b[1],b[2],result);
    return result;
}const DWORD WaitTsStream(DWORD t)override{if(GetReadyCount())return WAIT_OBJECT_0;return WaitForSingleObject(readyEvent,t);}const DWORD GetReadyCount()override{AcquireSRWLockShared(&lk);DWORD n=readyCount;ReleaseSRWLockShared(&lk);return n;}const BOOL GetTsStream(BYTE*d,DWORD*z,DWORD*r)override{BYTE*p=nullptr;DWORD n=0,m=0;if(!d||!GetTsStream(&p,&n,&m)){if(z)*z=0;if(r)*r=0;return FALSE;}if(n)memcpy(d,p,n);if(z)*z=n;if(r)*r=m;return TRUE;}const BOOL GetTsStream(BYTE**p,DWORD*z,DWORD*r)override{if(p)*p=nullptr;if(z)*z=0;if(r)*r=0;AcquireSRWLockExclusive(&lk);if(!op||!tu){ReleaseSRWLockExclusive(&lk);return FALSE;}if(!readyCount){ResetEvent(readyEvent);ReleaseSRWLockExclusive(&lk);return TRUE;}int i=consumeIndex;auto&n=nodes[i];if(n.state!=NS::READY){lg("TS INVARIANT generation=%lu stage=CONSUME index=%d state=%d ready=%lu",generation,i,(int)n.state,readyCount);ReleaseSRWLockExclusive(&lk);return FALSE;}if(p)*p=n.data.data();if(z)*z=n.bytes;n.state=NS::FREE;consumeIndex=(consumeIndex+1)%kNodeCount;readyCount--;if(r)*r=readyCount;if(!readyCount)ResetEvent(readyEvent);DWORD b=++blocks;DWORD sz=n.bytes,ce=n.completionError;ULONGLONG seq=n.sequence;ReleaseSRWLockExclusive(&lk);if(ce!=ERROR_SUCCESS||sz!=BS||b<=10||b%100==0)lg("TS GET generation=%lu sequence=%llu index=%d size=%lu completion_error=%lu remain=%lu block=%lu",generation,seq,i,sz,ce,r?*r:0,b);return TRUE;}void PurgeTsStream()override{AcquireSRWLockExclusive(&lk);DWORD discarded=readyCount;while(readyCount&&nodes[consumeIndex].state==NS::READY){nodes[consumeIndex].state=NS::FREE;nodes[consumeIndex].bytes=0;consumeIndex=(consumeIndex+1)%kNodeCount;readyCount--;}consumeIndex=completeIndex;readyCount=0;ResetEvent(readyEvent);ReleaseSRWLockExclusive(&lk);lg("TS PURGE generation=%lu ready_discarded=%lu",generation,discarded);}LPCWSTR GetTunerName()override{return TN;}const BOOL IsTunerOpening()override{return op;}LPCWSTR EnumTuningSpace(DWORD x)override{return x?nullptr:SN;}LPCWSTR EnumChannelName(DWORD x,DWORD y)override{if(x||y>=50)return nullptr;return CHANNEL_NAMES[y];}const BOOL SetChannel(DWORD x,DWORD y)override{
 StopPump();AcquireSRWLockExclusive(&lk);BOOL result=FALSE;
 if(!op||x!=0||y>=50){lg("SWITCH BLOCKED reason=INVALID_STATE_OR_CHANNEL open=%s space=%lu channel=%lu",op?"TRUE":"FALSE",x,y);}
 else if(false&&tu&&sp==x&&ch==y){result=TRUE;}
 else{
  bool enabled=ConfigEnabled(L"TransportStream",L"EnableRealTS");
  if(!enabled){lg("SWITCH BLOCKED reason=CONFIG_REAL_TS_DISABLED control_executed=0 read_executed=0");}
  else if(!usb){lg("SWITCH BLOCKED reason=WINUSB_HANDLE_NULL");}
  else{
   const DWORD dwChannel=y,physical=y+13u,frequency=0x73837u+0x1770u*dwChannel;const WORD pll=(WORD)(0x0E7Fu+0x002Au*dwChannel);
   BYTE a[5]={0xC0,(BYTE)(pll>>8),(BYTE)pll,0xB2,0x08};BYTE b[5]={0xC0,(BYTE)(pll>>8),(BYTE)pll,0x9A,0x08};BYTE c[1]={0x40};
   struct R{USHORT v,i;ULONG n;BYTE*p;};R q[3]={{0x3000,0x00FE,5,a},{0x3000,0x00FE,5,b},{0x3000,0x0001,1,c}};
   const DWORD transaction=++switchCount;ULONGLONG started=GetTickCount64();DWORD succeeded=0,attempted=0;
   lg("SWITCH BEGIN transaction=%lu tvtest_channel=%lu physical_ch=%lu dwChannel=%lu frequency=%lu pll=0x%04X",transaction,y,physical,dwChannel,frequency,pll);
   for(DWORD k=0;k<3;k++){WINUSB_SETUP_PACKET z{};z.RequestType=0x40;z.Request=0x03;z.Value=q[k].v;z.Index=q[k].i;z.Length=(USHORT)q[k].n;ULONG transferred=0;SetLastError(ERROR_SUCCESS);BOOL ok=WinUsb_ControlTransfer(usb,z,q[k].p,q[k].n,&transferred,nullptr);DWORD error=ok?ERROR_SUCCESS:GetLastError();attempted++;controlCalls++;BOOL pass=ok&&transferred==q[k].n;lgd("CONTROL RESULT transaction=%lu seq=%lu/3 physical_ch=%lu result=%s transferred=%lu expected=%lu last_error=%lu status=%s",transaction,k+1,physical,ok?"TRUE":"FALSE",transferred,q[k].n,error,pass?"PASS":"FAIL");if(!pass)break;succeeded++;}
   if(succeeded==3){
    USB_INTERFACE_DESCRIPTOR id{};BOOL qi=WinUsb_QueryInterfaceSettings(usb,0,&id);WINUSB_PIPE_INFORMATION target{};bool found=false;
    if(qi){for(UCHAR pi=0;pi<id.bNumEndpoints;pi++){WINUSB_PIPE_INFORMATION info{};if(WinUsb_QueryPipe(usb,0,pi,&info)&&info.PipeId==0x81&&info.PipeType==UsbdPipeTypeBulk){target=info;found=true;}}}
    lgd("PIPE DISCOVERY transaction=%lu physical_ch=%lu found=%s direction=%s max_packet=%u",transaction,physical,found?"TRUE":"FALSE",found&&((target.PipeId&0x80)!=0)?"IN":"INVALID",found?target.MaximumPacketSize:0);
    bool pumpOk=false;if(found&&((target.PipeId&0x80)!=0))pumpOk=StartPumpLocked();
    if(!pumpOk){lg("SWITCH RESULT transaction=%lu physical_ch=%lu control_status=PASS pump_status=FAIL",transaction,physical);tu=false;sp=ch=BAD;due=0;result=FALSE;}else{
    tu=true;sp=x;ch=y;due=GetTickCount64();result=TRUE;
    lg("SWITCH RESULT transaction=%lu physical_ch=%lu control_status=PASS pump_status=PASS elapsed_ms=%llu setchannel_status=TRUE real_ts_forwarded=TRUE",transaction,physical,GetTickCount64()-started);}
   }else{tu=false;sp=ch=BAD;due=0;lg("SWITCH RESULT transaction=%lu physical_ch=%lu control_status=FAIL attempted=%lu succeeded=%lu elapsed_ms=%llu setchannel_status=FALSE",transaction,physical,attempted,succeeded,GetTickCount64()-started);}
  }
 }
 ReleaseSRWLockExclusive(&lk);lg("SetChannel(%lu,%lu) -> %s",x,y,result?"TRUE":"FALSE");return result;
}const DWORD GetCurSpace()override{return sp;}const DWORD GetCurChannel()override{return ch;}void Release()override{lg("Release");delete this;}};}
extern "C" __declspec(dllexport) IBonDriver* WINAPI CreateBonDriver(){lg("CreateBonDriver");try{return new D();}catch(...){return nullptr;}}BOOL APIENTRY DllMain(HMODULE m,DWORD r,LPVOID){if(r==DLL_PROCESS_ATTACH){gm=m;DisableThreadLibraryCalls(m);}return TRUE;}
