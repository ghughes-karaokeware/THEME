#include "CHTheme.h"
#include "CHPromoMarkup.h"
#include <windows.h>
#include <richedit.h>
#include <commctrl.h>
#include <algorithm>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace {
constexpr DWORD kVersion = 0x00010000;
constexpr wchar_t kClassName[] = L"CHThemePromoDesigner";
constexpr COLORREF kBg = RGB(5, 15, 25), kPanel = RGB(14, 31, 46), kBorder = RGB(43, 65, 84), kText = RGB(238, 245, 251), kAccent = RGB(0, 120, 215);
constexpr int ID_EDITOR = 100, ID_BOLD = 110, ID_ITALIC = 111, ID_UNDERLINE = 112, ID_UNDO = 113, ID_REDO = 114, ID_OK = 120, ID_CANCEL = 121, ID_COLOR0 = 200;
const COLORREF kColors[10] = { RGB(255,255,255), RGB(255,255,255), RGB(255,255,0), RGB(255,0,0), RGB(52,204,255), RGB(52,255,52), RGB(255,128,0), RGB(255,0,255), RGB(128,0,128), RGB(255,204,0) };
struct Completion { DWORD instance; LONG result; };
struct Data { HWND window{}, owner{}, notify{}, editor{}; CHPT_PROMO_DATA* caller{}; DWORD instance{}; std::string original; HFONT font{}; bool completed{}; };
std::mutex gLock; std::unordered_map<HWND, std::unique_ptr<Data>> gWindows; std::unordered_map<HWND, std::deque<Completion>> gCompletions; DWORD gNext = 1; HMODULE gRich{};
void Debug(const Data& d,const wchar_t* event,DWORD value=0){if(!(d.caller->flags&CHPT_FLAG_DEBUG_LOG))return;wchar_t line[180]{};wsprintfW(line,L"CHPT instance=%lu %s value=%lu input=%lu output=%lu\n",d.instance,event,value,d.caller->inputLength,d.caller->outputLength);OutputDebugStringW(line);}

void Notify(Data& d, LONG result)
{
    if (d.completed) return; d.completed = true;
    d.caller->instanceId = d.instance; d.caller->result = result;
    { std::lock_guard<std::mutex> lock(gLock); gCompletions[d.notify].push_back({d.instance, result}); }
    HWND parent = GetParent(d.notify);
    if (IsWindow(parent) && IsWindow(d.notify)) PostMessageW(parent, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(d.notify), BN_CLICKED), reinterpret_cast<LPARAM>(d.notify));
    Debug(d,result==CHPT_RESULT_OK?L"OK completion":L"Cancel completion",static_cast<DWORD>(result));
}

void ApplyFormat(HWND edit, DWORD mask, DWORD effects)
{
    CHARFORMAT2W cf{}; cf.cbSize = sizeof(cf); cf.dwMask = mask; cf.dwEffects = effects;
    SendMessageW(edit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf)); SetFocus(edit);
}
void Toggle(HWND edit, DWORD mask, DWORD effect)
{
    CHARFORMAT2W cf{}; cf.cbSize = sizeof(cf); cf.dwMask = mask; SendMessageW(edit, EM_GETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));
    ApplyFormat(edit, mask, (cf.dwEffects & effect) ? 0 : effect);
}
void SetColor(HWND edit, int index) { CHARFORMAT2W cf{}; cf.cbSize = sizeof(cf); cf.dwMask = CFM_COLOR; cf.crTextColor = kColors[index]; SendMessageW(edit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf)); SetFocus(edit); }
LRESULT CALLBACK EditorSubclass(HWND edit, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR)
{
    if (msg == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000)) {
        if (wp == 'B') { Toggle(edit, CFM_BOLD, CFE_BOLD); return 0; }
        if (wp == 'I') { Toggle(edit, CFM_ITALIC, CFE_ITALIC); return 0; }
        if (wp == 'U') { Toggle(edit, CFM_UNDERLINE, CFE_UNDERLINE); return 0; }
    }
    return DefSubclassProc(edit, msg, wp, lp);
}

void LoadDocument(Data& d)
{
    unsigned unknown{}; const auto doc = chpromo::Parse(d.original, &unknown); const auto plain = chpromo::PlainText(doc);
    SetWindowTextW(d.editor, plain.c_str());
    for (LONG i = 0; i < static_cast<LONG>(doc.characters.size()); ++i) {
        if (doc.characters[i].value == L'\n') continue;
        CHARRANGE range{i, i + 1}; SendMessageW(d.editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
        const auto& s = doc.characters[i].style; CHARFORMAT2W cf{}; cf.cbSize = sizeof(cf); cf.dwMask = CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_COLOR;
        cf.dwEffects = (s.bold ? CFE_BOLD : 0) | (s.italic ? CFE_ITALIC : 0) | (s.underline ? CFE_UNDERLINE : 0); cf.crTextColor = kColors[std::clamp(s.color, 0, 9)];
        SendMessageW(d.editor, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));
    }
    CHARRANGE end{static_cast<LONG>(plain.size()), static_cast<LONG>(plain.size())}; SendMessageW(d.editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&end));
}

bool SaveDocument(Data& d)
{
    const int length = GetWindowTextLengthW(d.editor); std::wstring text(static_cast<size_t>(length) + 1, L'\0'); GetWindowTextW(d.editor, text.data(), length + 1); text.resize(length);
    chpromo::Document doc; doc.characters.reserve(text.size());
    for (LONG i = 0; i < length; ++i) {
        if (text[i] != L'\r' && text[i] != L'\n') { char encoded{}; BOOL usedDefault=FALSE; if(!WideCharToMultiByte(1252,WC_NO_BEST_FIT_CHARS,&text[i],1,&encoded,1,"?",&usedDefault)||usedDefault){MessageBoxW(d.window,L"This character cannot be stored by the current CompuHost ANSI Promo Trailer format.",L"Promo Trailer Designer",MB_OK|MB_ICONWARNING);return false;} }
        chpromo::Style s{};
        if (text[i] != L'\r' && text[i] != L'\n') {
            CHARRANGE range{i, i + 1}; SendMessageW(d.editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range)); CHARFORMAT2W cf{}; cf.cbSize = sizeof(cf); cf.dwMask = CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_COLOR; SendMessageW(d.editor, EM_GETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));
            s.bold = (cf.dwEffects & CFE_BOLD) != 0; s.italic = (cf.dwEffects & CFE_ITALIC) != 0; s.underline = (cf.dwEffects & CFE_UNDERLINE) != 0;
            for (int c = 2; c < 10; ++c) if (cf.crTextColor == kColors[c]) { s.color = c; break; }
        }
        if (text[i] != L'\r') doc.characters.push_back({text[i], s});
    }
    const std::string encoded = chpromo::Serialize(doc); const DWORD limit = std::min<DWORD>(d.caller->maximumLength, d.caller->bufferCapacity - 1);
    if (encoded.size() > limit) { wchar_t message[160]; wsprintfW(message, L"The encoded Promo Trailer text is %u characters. The current limit is %u.", static_cast<unsigned>(encoded.size()), limit); MessageBoxW(d.window, message, L"Promo Trailer Designer", MB_OK | MB_ICONWARNING); return false; }
    memcpy(d.caller->text, encoded.data(), encoded.size()); d.caller->text[encoded.size()] = 0; d.caller->outputLength = static_cast<DWORD>(encoded.size()); Debug(d,L"serialized",static_cast<DWORD>(encoded.size())); return true;
}

void Layout(Data& d)
{
    RECT r{}; GetClientRect(d.window, &r); const int m = 18, top = 100, bottom = 62;
    MoveWindow(d.editor, m, top, std::max(100, static_cast<int>(r.right) - m * 2), std::max(80, static_cast<int>(r.bottom) - top - bottom), TRUE);
    MoveWindow(GetDlgItem(d.window, ID_OK), r.right - 190, r.bottom - 44, 78, 30, TRUE); MoveWindow(GetDlgItem(d.window, ID_CANCEL), r.right - 102, r.bottom - 44, 78, 30, TRUE);
}
LRESULT CALLBACK Proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    Data* d = reinterpret_cast<Data*>(GetWindowLongPtrW(w, GWLP_USERDATA));
    if (msg == WM_NCCREATE) { d = static_cast<Data*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams); d->window = w; SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(d)); }
    if (!d) return DefWindowProcW(w, msg, wp, lp);
    switch (msg) {
    case WM_CREATE: {
        d->font = CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        auto button=[&](int id,const wchar_t* text,int x,int width){HWND h=CreateWindowExW(0,L"BUTTON",text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,x,62,width,30,w,reinterpret_cast<HMENU>(id),nullptr,nullptr);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(d->font),TRUE);};
        button(ID_BOLD,L"B",18,36); button(ID_ITALIC,L"I",58,36); button(ID_UNDERLINE,L"U",98,36); button(ID_UNDO,L"Undo",148,58); button(ID_REDO,L"Redo",210,58);
        button(ID_COLOR0,L"Default",278,62); for(int i=1;i<10;++i) button(ID_COLOR0+i,std::to_wstring(i).c_str(),344+(i-1)*38,34);
        button(ID_OK,L"OK",0,78); button(ID_CANCEL,L"Cancel",0,78);
        d->editor=CreateWindowExW(0,MSFTEDIT_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,18,100,700,350,w,reinterpret_cast<HMENU>(ID_EDITOR),nullptr,nullptr);
        SendMessageW(d->editor,WM_SETFONT,reinterpret_cast<WPARAM>(d->font),TRUE); SendMessageW(d->editor,EM_SETBKGNDCOLOR,0,kPanel); SendMessageW(d->editor,EM_SETLIMITTEXT,d->caller->maximumLength,0); SetWindowSubclass(d->editor,EditorSubclass,1,reinterpret_cast<DWORD_PTR>(d)); CHARFORMAT2W base{};base.cbSize=sizeof(base);base.dwMask=CFM_COLOR;base.crTextColor=kText;SendMessageW(d->editor,EM_SETCHARFORMAT,SCF_ALL,reinterpret_cast<LPARAM>(&base));LoadDocument(*d); Layout(*d); return 0; }
    case WM_GETMINMAXINFO: { auto* info=reinterpret_cast<MINMAXINFO*>(lp); info->ptMinTrackSize.x=760; info->ptMinTrackSize.y=480; return 0; }
    case WM_SIZE: Layout(*d); return 0;
    case WM_ERASEBKGND: { RECT r{}; GetClientRect(w,&r); HBRUSH brush=CreateSolidBrush(kBg); FillRect(reinterpret_cast<HDC>(wp),&r,brush); DeleteObject(brush); return 1; }
    case WM_PAINT: { PAINTSTRUCT ps{}; HDC dc=BeginPaint(w,&ps); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,kText); SelectObject(dc,d->font); TextOutW(dc,18,16,L"PROMO TRAILER DESIGNER",22); SetTextColor(dc,RGB(164,188,208)); TextOutW(dc,18,34,L"Each line represents a separate Promo Trailer. Press Enter to create another trailer.",82); EndPaint(w,&ps); return 0; }
    case WM_DRAWITEM: { auto* item=reinterpret_cast<DRAWITEMSTRUCT*>(lp); if(item->CtlType!=ODT_BUTTON)break; RECT r=item->rcItem; const bool pressed=(item->itemState&ODS_SELECTED)!=0; HBRUSH outside=CreateSolidBrush(kBg);FillRect(item->hDC,&r,outside);DeleteObject(outside);HBRUSH fill=CreateSolidBrush(item->CtlID==ID_OK?kAccent:(pressed?RGB(31,64,91):kPanel)); HPEN pen=CreatePen(PS_SOLID,1,item->CtlID==ID_OK?RGB(44,151,239):kBorder); HGDIOBJ oldBrush=SelectObject(item->hDC,fill),oldPen=SelectObject(item->hDC,pen); RoundRect(item->hDC,r.left,r.top,r.right,r.bottom,7,7); SelectObject(item->hDC,oldBrush);SelectObject(item->hDC,oldPen);DeleteObject(fill);DeleteObject(pen); wchar_t label[32]{};GetWindowTextW(item->hwndItem,label,32);SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,kText);SelectObject(item->hDC,d->font); if(item->CtlID>=ID_COLOR0&&item->CtlID<ID_COLOR0+10){int index=item->CtlID-ID_COLOR0;if(index){RECT sw=r;InflateRect(&sw,-8,-7);HBRUSH color=CreateSolidBrush(kColors[index]);FillRect(item->hDC,&sw,color);DeleteObject(color);}else DrawTextW(item->hDC,label,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}else DrawTextW(item->hDC,label,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);return TRUE; }
    case WM_COMMAND: switch(LOWORD(wp)) {
        case ID_BOLD: Toggle(d->editor,CFM_BOLD,CFE_BOLD); return 0; case ID_ITALIC: Toggle(d->editor,CFM_ITALIC,CFE_ITALIC); return 0; case ID_UNDERLINE: Toggle(d->editor,CFM_UNDERLINE,CFE_UNDERLINE); return 0;
        case ID_UNDO: SendMessageW(d->editor,EM_UNDO,0,0); return 0; case ID_REDO: SendMessageW(d->editor,EM_REDO,0,0); return 0;
        case ID_OK: if(SaveDocument(*d)){Notify(*d,CHPT_RESULT_OK);DestroyWindow(w);} return 0; case ID_CANCEL: Notify(*d,CHPT_RESULT_CANCEL);DestroyWindow(w);return 0;
        default: if(LOWORD(wp)>=ID_COLOR0&&LOWORD(wp)<ID_COLOR0+10){SetColor(d->editor,LOWORD(wp)-ID_COLOR0);return 0;} break; } break;
    case WM_CLOSE: Notify(*d,CHPT_RESULT_CANCEL); DestroyWindow(w); return 0;
    case WM_DESTROY: if(d->font) DeleteObject(d->font); { std::lock_guard<std::mutex> lock(gLock); gWindows.erase(w); } return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
}
bool EnsureClass()
{
    if (!gRich) gRich=LoadLibraryW(L"Msftedit.dll"); if(!gRich) return false; WNDCLASSEXW wc{}; if(GetClassInfoExW(GetModuleHandleW(nullptr),kClassName,&wc)) return true;
    wc.cbSize=sizeof(wc);wc.lpfnWndProc=Proc;wc.hInstance=GetModuleHandleW(nullptr);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=CreateSolidBrush(kBg);wc.lpszClassName=kClassName;return RegisterClassExW(&wc)!=0;
}
}

extern "C" DWORD __stdcall CHPT_GetAbiVersion(){return kVersion;}
extern "C" DWORD __stdcall CHPT_GetDataSize(){return sizeof(CHPT_PROMO_DATA);}
extern "C" LONG __stdcall CHPT_ValidateData(const CHPT_PROMO_DATA* p){if(!p)return CHPT_ERROR_ARGUMENT;if(p->version!=kVersion)return CHPT_ERROR_VERSION;if(p->structureSize!=sizeof(*p))return CHPT_ERROR_SIZE;if(p->bufferCapacity<2||p->bufferCapacity>sizeof(p->text)||p->maximumLength>=p->bufferCapacity)return CHPT_ERROR_BUFFER;if(strnlen_s(p->text,p->bufferCapacity)>=p->bufferCapacity)return CHPT_ERROR_BUFFER;return CHPT_STATUS_OK;}
extern "C" LONG __stdcall CHPT_OpenDesigner(HWND owner,CHPT_PROMO_DATA* p,HWND notify){LONG v=CHPT_ValidateData(p);if(v!=CHPT_STATUS_OK)return v;if(!IsWindow(owner)||!IsWindow(notify))return CHPT_ERROR_ARGUMENT;if(!EnsureClass())return CHPT_ERROR_WINDOW;DWORD instance{};{std::lock_guard<std::mutex> lock(gLock);if(!gWindows.empty())return CHPT_ERROR_ALREADY_OPEN;instance=gNext++;}auto data=std::make_unique<Data>();data->owner=owner;data->notify=notify;data->caller=p;data->instance=instance;data->original=p->text;p->inputLength=static_cast<DWORD>(data->original.size());p->outputLength=0;Data* raw=data.get();HWND w=CreateWindowExW(WS_EX_APPWINDOW|WS_EX_CONTROLPARENT,kClassName,L"CompuHost V4 - Promo Trailer Designer",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,920,620,owner,nullptr,GetModuleHandleW(nullptr),raw);if(!w)return CHPT_ERROR_WINDOW;{std::lock_guard<std::mutex> lock(gLock);gWindows[w]=std::move(data);}p->instanceId=raw->instance;Debug(*raw,L"opened",p->inputLength);return static_cast<LONG>(raw->instance);}
extern "C" LONG __stdcall CHPT_ConsumeCompletion(HWND notify,DWORD* instance,LONG* result){if(!notify||!instance||!result)return FALSE;std::lock_guard<std::mutex> lock(gLock);auto it=gCompletions.find(notify);if(it==gCompletions.end()||it->second.empty())return FALSE;auto c=it->second.front();it->second.pop_front();*instance=c.instance;*result=c.result;if(it->second.empty())gCompletions.erase(it);return TRUE;}
