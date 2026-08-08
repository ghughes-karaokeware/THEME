#include "../dll/CHTheme.h"
#include <windows.h>
#include <cstdio>
#include <cstring>

template<class T>T Load(HMODULE m,const char* n){return reinterpret_cast<T>(GetProcAddress(m,n));}
int main(int argc,char** argv)
{
    if(argc!=2)return 2; HMODULE dll=LoadLibraryA(argv[1]);if(!dll)return 3;
    using Get=DWORD(__stdcall*)();using Validate=LONG(__stdcall*)(const CHPT_PROMO_DATA*);
    auto version=Load<Get>(dll,"CHPT_GetAbiVersion");auto size=Load<Get>(dll,"CHPT_GetDataSize");auto validate=Load<Validate>(dll,"CHPT_ValidateData");
    if(!version||!size||!validate)return 4;if(version()!=0x00010000||size()!=4256||size()!=sizeof(CHPT_PROMO_DATA))return 5;
    CHPT_PROMO_DATA data{};data.version=version();data.structureSize=sizeof(data);data.bufferCapacity=sizeof(data.text);data.maximumLength=1027;strcpy_s(data.text,"Welcome to <3><B>Friday Night Karaoke</B><0>!");
    if(validate(&data)!=CHPT_STATUS_OK)return 6;data.structureSize--;if(validate(&data)!=CHPT_ERROR_SIZE)return 7;data.structureSize=sizeof(data);data.maximumLength=data.bufferCapacity;if(validate(&data)!=CHPT_ERROR_BUFFER)return 8;
    std::printf("Promo ABI=%08lX size=%lu validation=PASS\n",version(),size());return 0;
}
