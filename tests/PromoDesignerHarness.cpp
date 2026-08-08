#include "../dll/CHTheme.h"
#include <windows.h>
#include <cstdio>
#include <cstring>

template<class T>T Load(HMODULE m,const char* n){return reinterpret_cast<T>(GetProcAddress(m,n));}
LRESULT CALLBACK OwnerProc(HWND w,UINT m,WPARAM p,LPARAM l){return DefWindowProcW(w,m,p,l);}
bool PumpUntilClosed(HWND dialog)
{
    DWORD start=GetTickCount();MSG msg{};
    while(IsWindow(dialog)&&GetTickCount()-start<5000){while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(1);}
    return !IsWindow(dialog);
}
int main(int argc,char** argv)
{
    if(argc!=2)return 2;HMODULE dll=LoadLibraryA(argv[1]);if(!dll)return 3;
    using Open=LONG(__stdcall*)(HWND,CHPT_PROMO_DATA*,HWND);using Consume=LONG(__stdcall*)(HWND,DWORD*,LONG*);
    auto open=Load<Open>(dll,"CHPT_OpenDesigner");auto consume=Load<Consume>(dll,"CHPT_ConsumeCompletion");if(!open||!consume)return 4;
    WNDCLASSW wc{};wc.lpfnWndProc=OwnerProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"CHPromoHarnessOwner";RegisterClassW(&wc);
    HWND owner=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPED,0,0,200,100,nullptr,nullptr,wc.hInstance,nullptr);HWND notify=CreateWindowW(L"BUTTON",L"",WS_CHILD,0,0,1,1,owner,reinterpret_cast<HMENU>(900),wc.hInstance,nullptr);
    CHPT_PROMO_DATA data{};data.version=0x00010000;data.structureSize=sizeof(data);data.bufferCapacity=sizeof(data.text);data.maximumLength=1027;strcpy_s(data.text,"Welcome to <3><B>Friday Night Karaoke</B><0>!\r\nPlain trailer");
    LONG instance=open(owner,&data,notify);if(instance<=0)return 5;HWND dialog=FindWindowW(L"CHThemePromoDesigner",nullptr);if(!dialog)return 6;SendMessageW(dialog,WM_COMMAND,120,0);if(!PumpUntilClosed(dialog))return 7;DWORD completed{};LONG result{};if(!consume(notify,&completed,&result)||completed!=static_cast<DWORD>(instance)||result!=CHPT_RESULT_OK)return 8;if(!strstr(data.text,"Friday Night Karaoke")||!strstr(data.text,"Plain trailer"))return 9;
    char accepted[4096];strcpy_s(accepted,data.text);instance=open(owner,&data,notify);if(instance<=0)return 10;dialog=FindWindowW(L"CHThemePromoDesigner",nullptr);if(!dialog)return 11;SendMessageW(dialog,WM_COMMAND,121,0);if(!PumpUntilClosed(dialog))return 12;if(!consume(notify,&completed,&result)||result!=CHPT_RESULT_CANCEL||strcmp(data.text,accepted)!=0)return 13;
    DestroyWindow(owner);std::puts("Promo Designer modeless OK/Cancel transaction: PASS");return 0;
}
