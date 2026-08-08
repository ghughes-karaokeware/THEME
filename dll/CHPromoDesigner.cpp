#include "CHTheme.h"
#include "CHPromoMarkup.h"
#include <windows.h>
#include <richedit.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <algorithm>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace {
constexpr DWORD kVersion = 0x00010000;
constexpr wchar_t kClassName[] = L"CHThemePromoDesigner";
constexpr COLORREF kBg = RGB(5, 15, 25), kPanel = RGB(14, 31, 46), kBorder = RGB(43, 65, 84), kText = RGB(238, 245, 251), kAccent = RGB(0, 120, 215);
constexpr int ID_EDITOR = 100, ID_BOLD = 110, ID_ITALIC = 111, ID_UNDERLINE = 112, ID_UNDO = 113, ID_REDO = 114, ID_AUTOLOAD = 115, ID_LOAD = 116, ID_SAVE = 117, ID_CLEAR = 118, ID_OK = 120, ID_CANCEL = 121, ID_COLOR0 = 200;
const COLORREF kColors[10] = { RGB(255,255,255), RGB(255,255,255), RGB(255,255,0), RGB(255,0,0), RGB(52,204,255), RGB(52,255,52), RGB(255,128,0), RGB(255,0,255), RGB(128,0,128), RGB(255,204,0) };
struct Completion { DWORD instance; LONG result; };
struct Data { HWND window{}, owner{}, notify{}, editor{}; CHPT_PROMO_DATA* caller{}; DWORD instance{}; std::string original; std::wstring prmFolder; HFONT font{}, titleFont{}; HICON iconSmall{},iconLarge{}; bool completed{}, autoload{}; int hoverId{}; };
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

LRESULT CALLBACK ButtonSubclass(HWND button, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR ref)
{
    auto* d=reinterpret_cast<Data*>(ref);
    if(msg==WM_MOUSEMOVE&&d->hoverId!=GetDlgCtrlID(button)){d->hoverId=GetDlgCtrlID(button);TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,button,0};TrackMouseEvent(&track);InvalidateRect(button,nullptr,FALSE);}
    else if(msg==WM_MOUSELEAVE&&d->hoverId==GetDlgCtrlID(button)){d->hoverId=0;InvalidateRect(button,nullptr,FALSE);}
    return DefSubclassProc(button,msg,wp,lp);
}

HICON CreatePromoIcon(int size)
{
    HDC screen=GetDC(nullptr),colorDc=CreateCompatibleDC(screen),maskDc=CreateCompatibleDC(screen);HBITMAP color=CreateCompatibleBitmap(screen,size,size),mask=CreateBitmap(size,size,1,1,nullptr);ReleaseDC(nullptr,screen);HGDIOBJ oldColor=SelectObject(colorDc,color),oldMask=SelectObject(maskDc,mask);RECT bounds{0,0,size,size};FillRect(maskDc,&bounds,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));HBRUSH opaque=static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));HGDIOBJ oldMaskBrush=SelectObject(maskDc,opaque);Ellipse(maskDc,0,0,size,size);SelectObject(maskDc,oldMaskBrush);FillRect(colorDc,&bounds,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));HBRUSH blue=CreateSolidBrush(RGB(0,105,210));HPEN bluePen=CreatePen(PS_SOLID,1,RGB(55,170,255));HGDIOBJ oldBrush=SelectObject(colorDc,blue),oldPen=SelectObject(colorDc,bluePen);Ellipse(colorDc,0,0,size,size);HBRUSH gold=CreateSolidBrush(RGB(255,204,0));HPEN goldPen=CreatePen(PS_SOLID,std::max(1,size/16),RGB(255,235,120));SelectObject(colorDc,gold);SelectObject(colorDc,goldPen);POINT horn[4]{{size*3/16,size*7/16},{size*11/16,size*3/16},{size*11/16,size*13/16},{size*3/16,size*9/16}};Polygon(colorDc,horn,4);RECT mouth{size*10/16,size*3/16,size*14/16,size*13/16};RoundRect(colorDc,mouth.left,mouth.top,mouth.right,mouth.bottom,std::max(2,size/8),std::max(2,size/8));RECT handle{size*5/16,size*9/16,size*8/16,size*15/16};RoundRect(colorDc,handle.left,handle.top,handle.right,handle.bottom,std::max(2,size/10),std::max(2,size/10));SelectObject(colorDc,oldBrush);SelectObject(colorDc,oldPen);DeleteObject(blue);DeleteObject(bluePen);DeleteObject(gold);DeleteObject(goldPen);SelectObject(colorDc,oldColor);SelectObject(maskDc,oldMask);DeleteDC(colorDc);DeleteDC(maskDc);ICONINFO info{};info.fIcon=TRUE;info.hbmColor=color;info.hbmMask=mask;HICON icon=CreateIconIndirect(&info);DeleteObject(color);DeleteObject(mask);return icon;
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

bool EncodeDocument(Data& d, std::string& encoded)
{
    const int length = GetWindowTextLengthW(d.editor); std::wstring text(static_cast<size_t>(length) + 1, L'\0'); GetWindowTextW(d.editor, text.data(), length + 1); text.resize(length);
    chpromo::Document doc; doc.characters.reserve(text.size());
    for (LONG i = 0; i < length; ++i) {
        if (text[i] != L'\r' && text[i] != L'\n') { char ansi{}; BOOL usedDefault=FALSE; if(!WideCharToMultiByte(1252,WC_NO_BEST_FIT_CHARS,&text[i],1,&ansi,1,"?",&usedDefault)||usedDefault){MessageBoxW(d.window,L"This character cannot be stored by the current CompuHost ANSI Promo Trailer format.",L"Promo Trailer Designer",MB_OK|MB_ICONWARNING);return false;} }
        chpromo::Style s{};
        if (text[i] != L'\r' && text[i] != L'\n') {
            CHARRANGE range{i, i + 1}; SendMessageW(d.editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range)); CHARFORMAT2W cf{}; cf.cbSize = sizeof(cf); cf.dwMask = CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_COLOR; SendMessageW(d.editor, EM_GETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));
            s.bold = (cf.dwEffects & CFE_BOLD) != 0; s.italic = (cf.dwEffects & CFE_ITALIC) != 0; s.underline = (cf.dwEffects & CFE_UNDERLINE) != 0;
            for (int c = 2; c < 10; ++c) if (cf.crTextColor == kColors[c]) { s.color = c; break; }
        }
        if (text[i] != L'\r') doc.characters.push_back({text[i], s});
    }
    encoded = chpromo::Serialize(doc); const DWORD limit = std::min<DWORD>(d.caller->maximumLength, d.caller->bufferCapacity - 1);
    if (encoded.size() > limit) { wchar_t message[160]; wsprintfW(message, L"The encoded Promo Trailer text is %u characters. The current limit is %u.", static_cast<unsigned>(encoded.size()), limit); MessageBoxW(d.window, message, L"Promo Trailer Designer", MB_OK | MB_ICONWARNING); return false; }
    return true;
}

bool SaveDocument(Data& d)
{
    std::string encoded; if (!EncodeDocument(d, encoded)) return false;
    memcpy(d.caller->text, encoded.data(), encoded.size()); d.caller->text[encoded.size()] = 0; d.caller->outputLength = static_cast<DWORD>(encoded.size());
    if (d.autoload) d.caller->flags |= CHPT_FLAG_AUTOLOAD; else d.caller->flags &= ~static_cast<DWORD>(CHPT_FLAG_AUTOLOAD);
    Debug(d,L"serialized",static_cast<DWORD>(encoded.size())); return true;
}

constexpr wchar_t kPromoRegistryKey[]=L"Software\\Karaokeware\\CHTheme\\PromoDesigner";
constexpr wchar_t kPromoFolderValue[]=L"LastPrmFolder";
constexpr wchar_t kPromoWindowValue[]=L"WindowRect";

RECT InitialPromoWindowRect()
{
    RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);const int workWidth=work.right-work.left,workHeight=work.bottom-work.top;RECT result{};DWORD bytes=sizeof(result);
    const bool stored=RegGetValueW(HKEY_CURRENT_USER,kPromoRegistryKey,kPromoWindowValue,RRF_RT_REG_BINARY,nullptr,&result,&bytes)==ERROR_SUCCESS&&bytes==sizeof(result)&&result.right>result.left&&result.bottom>result.top;
    int width=stored?result.right-result.left:920,height=stored?result.bottom-result.top:620;width=std::clamp(width,std::min(760,workWidth),workWidth);height=std::clamp(height,std::min(480,workHeight),workHeight);
    int left=stored?static_cast<int>(result.left):static_cast<int>(work.left)+(workWidth-width)/2,top=stored?static_cast<int>(result.top):static_cast<int>(work.top)+(workHeight-height)/2;left=std::clamp(left,static_cast<int>(work.left),static_cast<int>(work.right)-width);top=std::clamp(top,static_cast<int>(work.top),static_cast<int>(work.bottom)-height);return RECT{left,top,left+width,top+height};
}

void RememberPromoWindowRect(HWND window)
{
    WINDOWPLACEMENT placement{sizeof(placement)};if(!GetWindowPlacement(window,&placement))return;RECT rect=placement.rcNormalPosition;if(rect.right<=rect.left||rect.bottom<=rect.top)return;HKEY key{};if(RegCreateKeyExW(HKEY_CURRENT_USER,kPromoRegistryKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)==ERROR_SUCCESS){RegSetValueExW(key,kPromoWindowValue,0,REG_BINARY,reinterpret_cast<const BYTE*>(&rect),sizeof(rect));RegCloseKey(key);}
}

void InitializePrmFolder(Data& d)
{
    wchar_t folder[1024]{};DWORD bytes=sizeof(folder);
    if(RegGetValueW(HKEY_CURRENT_USER,kPromoRegistryKey,kPromoFolderValue,RRF_RT_REG_SZ,nullptr,folder,&bytes)==ERROR_SUCCESS&&folder[0]){d.prmFolder=folder;return;}
    wchar_t executable[MAX_PATH]{};DWORD length=GetModuleFileNameW(nullptr,executable,MAX_PATH);if(!length||length>=MAX_PATH)return;wchar_t* slash=wcsrchr(executable,L'\\');if(slash)*slash=0;d.prmFolder=executable;
}

void RememberPrmFolder(Data& d,const wchar_t* path)
{
    std::wstring folder=path;const auto slash=folder.find_last_of(L"\\/");if(slash==std::wstring::npos)return;folder.resize(slash);if(folder.empty())return;HKEY key{};if(RegCreateKeyExW(HKEY_CURRENT_USER,kPromoRegistryKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)==ERROR_SUCCESS){RegSetValueExW(key,kPromoFolderValue,0,REG_SZ,reinterpret_cast<const BYTE*>(folder.c_str()),static_cast<DWORD>((folder.size()+1)*sizeof(wchar_t)));RegCloseKey(key);}d.prmFolder=std::move(folder);
}

bool SelectPrmFile(Data& d, bool save, wchar_t* path, DWORD capacity)
{
    path[0]=0; OPENFILENAMEW ofn{}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=d.window; ofn.lpstrFilter=L"CompuHost Promo Trailer (*.prm)\0*.prm\0Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0"; ofn.lpstrFile=path; ofn.nMaxFile=capacity; ofn.lpstrInitialDir=d.prmFolder.empty()?nullptr:d.prmFolder.c_str();ofn.lpstrDefExt=L"prm"; ofn.lpstrTitle=save?L"Save Promo Trailer":L"Load Promo Trailer"; ofn.Flags=OFN_PATHMUSTEXIST|OFN_HIDEREADONLY|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    return save?GetSaveFileNameW(&ofn)!=FALSE:GetOpenFileNameW(&ofn)!=FALSE;
}

void SavePrm(Data& d)
{
    std::string encoded; if(!EncodeDocument(d,encoded))return; wchar_t path[MAX_PATH]{};if(!SelectPrmFile(d,true,path,MAX_PATH))return;HANDLE file=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE){MessageBoxW(d.window,L"The Promo Trailer file could not be created.",L"Promo Trailer Designer",MB_OK|MB_ICONERROR);return;}DWORD written{};BOOL ok=WriteFile(file,encoded.data(),static_cast<DWORD>(encoded.size()),&written,nullptr);CloseHandle(file);if(!ok||written!=encoded.size())MessageBoxW(d.window,L"The Promo Trailer file could not be written completely.",L"Promo Trailer Designer",MB_OK|MB_ICONERROR);else RememberPrmFolder(d,path);
}

void LoadPrm(Data& d)
{
    wchar_t path[MAX_PATH]{};if(!SelectPrmFile(d,false,path,MAX_PATH))return;HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE){MessageBoxW(d.window,L"The Promo Trailer file could not be opened.",L"Promo Trailer Designer",MB_OK|MB_ICONERROR);return;}LARGE_INTEGER size{};if(!GetFileSizeEx(file,&size)||size.QuadPart<0||size.QuadPart>d.caller->maximumLength){CloseHandle(file);MessageBoxW(d.window,L"The Promo Trailer file exceeds the configured maximum length.",L"Promo Trailer Designer",MB_OK|MB_ICONWARNING);return;}std::string value(static_cast<size_t>(size.QuadPart),'\0');DWORD read{};BOOL ok=value.empty()||ReadFile(file,value.data(),static_cast<DWORD>(value.size()),&read,nullptr);CloseHandle(file);if(!ok||read!=value.size()){MessageBoxW(d.window,L"The Promo Trailer file could not be read completely.",L"Promo Trailer Designer",MB_OK|MB_ICONERROR);return;}if(value.size()>=3&&static_cast<unsigned char>(value[0])==0xEF&&static_cast<unsigned char>(value[1])==0xBB&&static_cast<unsigned char>(value[2])==0xBF)value.erase(0,3);d.original=value;LoadDocument(d);RememberPrmFolder(d,path);SetFocus(d.editor);
}

void Layout(Data& d)
{
    RECT r{}; GetClientRect(d.window, &r); const int m = 20, top = 150, bottom = 72;
    MoveWindow(d.editor, m, top, std::max(100, static_cast<int>(r.right) - m * 2), std::max(80, static_cast<int>(r.bottom) - top - bottom), TRUE);
    const int y=r.bottom-48;MoveWindow(GetDlgItem(d.window,ID_LOAD),20,y,92,32,TRUE);MoveWindow(GetDlgItem(d.window,ID_SAVE),120,y,92,32,TRUE);MoveWindow(GetDlgItem(d.window,ID_OK),r.right-380,y,260,32,TRUE);MoveWindow(GetDlgItem(d.window,ID_CANCEL),r.right-102,y,78,32,TRUE);
}
LRESULT CALLBACK Proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    Data* d = reinterpret_cast<Data*>(GetWindowLongPtrW(w, GWLP_USERDATA));
    if (msg == WM_NCCREATE) { d = static_cast<Data*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams); d->window = w; SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(d)); }
    if (!d) return DefWindowProcW(w, msg, wp, lp);
    switch (msg) {
    case WM_CREATE: {
        BOOL darkTitle=TRUE;DwmSetWindowAttribute(w,20,&darkTitle,sizeof(darkTitle));
        d->iconSmall=CreatePromoIcon(16);d->iconLarge=CreatePromoIcon(32);SendMessageW(w,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(d->iconSmall));SendMessageW(w,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(d->iconLarge));
        d->font = CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");d->titleFont=CreateFontW(-20,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        auto button=[&](int id,const wchar_t* text,int x,int y,int width){HWND h=CreateWindowExW(0,L"BUTTON",text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,x,y,width,30,w,reinterpret_cast<HMENU>(id),nullptr,nullptr);SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(d->font),TRUE);SetWindowSubclass(h,ButtonSubclass,1,reinterpret_cast<DWORD_PTR>(d));};
        button(ID_BOLD,L"B",20,72,36);button(ID_ITALIC,L"I",60,72,36);button(ID_UNDERLINE,L"U",100,72,36);
        for(int i=1;i<10;++i)button(ID_COLOR0+i,std::to_wstring(i).c_str(),150+(i-1)*38,72,34);
        button(ID_UNDO,L"Undo",510,72,58);button(ID_REDO,L"Redo",572,72,58);button(ID_CLEAR,L"Clear",646,72,70);
        button(ID_AUTOLOAD,L"Auto-load these Promo-Trailers on startup",20,110,410);
        button(ID_LOAD,L"Load",0,0,92);button(ID_SAVE,L"Save",0,0,92);
        button(ID_OK,L"Set Promo-Trailers and Exit",0,0,260);button(ID_CANCEL,L"Cancel",0,0,78);
        d->autoload=(d->caller->flags&CHPT_FLAG_AUTOLOAD)!=0;InitializePrmFolder(*d);
        d->editor=CreateWindowExW(0,MSFTEDIT_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,18,100,700,350,w,reinterpret_cast<HMENU>(ID_EDITOR),nullptr,nullptr);
        SendMessageW(d->editor,WM_SETFONT,reinterpret_cast<WPARAM>(d->font),TRUE); SendMessageW(d->editor,EM_SETBKGNDCOLOR,0,kPanel); SendMessageW(d->editor,EM_SETLIMITTEXT,d->caller->maximumLength,0); SetWindowSubclass(d->editor,EditorSubclass,1,reinterpret_cast<DWORD_PTR>(d)); CHARFORMAT2W base{};base.cbSize=sizeof(base);base.dwMask=CFM_COLOR;base.crTextColor=kText;SendMessageW(d->editor,EM_SETCHARFORMAT,SCF_ALL,reinterpret_cast<LPARAM>(&base));LoadDocument(*d); Layout(*d); return 0; }
    case WM_GETMINMAXINFO: { auto* info=reinterpret_cast<MINMAXINFO*>(lp); info->ptMinTrackSize.x=760; info->ptMinTrackSize.y=480; return 0; }
    case WM_SIZE: Layout(*d);InvalidateRect(w,nullptr,FALSE);return 0;
    case WM_ERASEBKGND: { RECT r{}; GetClientRect(w,&r); HBRUSH brush=CreateSolidBrush(kBg); FillRect(reinterpret_cast<HDC>(wp),&r,brush); DeleteObject(brush); return 1; }
    case WM_PAINT: { PAINTSTRUCT ps{};HDC dc=BeginPaint(w,&ps);RECT client{};GetClientRect(w,&client);for(int y=0;y<64;++y){const int blue=25+(y*18/64);HBRUSH shade=CreateSolidBrush(RGB(5,16,blue));RECT strip{0,y,client.right,y+1};FillRect(dc,&strip,shade);DeleteObject(shade);}HBRUSH band=CreateSolidBrush(RGB(8,23,36));RECT formatBand{14,66,client.right-14,104};FillRect(dc,&formatBand,band);RECT footer{0,client.bottom-64,client.right,client.bottom};FillRect(dc,&footer,band);HBRUSH accent=CreateSolidBrush(kAccent);RECT bar{14,14,18,54};FillRect(dc,&bar,accent);RECT line{18,59,client.right-18,61};FillRect(dc,&line,accent);RECT footerLine{18,client.bottom-65,client.right-18,client.bottom-64};FillRect(dc,&footerLine,accent);DeleteObject(accent);HPEN editorPen=CreatePen(PS_SOLID,1,RGB(36,74,102));HGDIOBJ oldPen=SelectObject(dc,editorPen);HGDIOBJ oldBrush=SelectObject(dc,GetStockObject(NULL_BRUSH));RECT editor{};GetWindowRect(d->editor,&editor);MapWindowPoints(HWND_DESKTOP,w,reinterpret_cast<POINT*>(&editor),2);Rectangle(dc,editor.left-1,editor.top-1,editor.right+1,editor.bottom+1);SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(editorPen);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,kText);SelectObject(dc,d->titleFont);RECT title{26,14,client.right-18,38};DrawTextW(dc,L"PROMO TRAILER DESIGNER",-1,&title,DT_LEFT|DT_SINGLELINE);SelectObject(dc,d->font);SetTextColor(dc,RGB(255,210,40));RECT help{26,38,client.right-18,57};DrawTextW(dc,L"ONE LINE = ONE PROMO TRAILER - EACH SCROLLER PASS SHOWS THE NEXT",-1,&help,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS);EndPaint(w,&ps);return 0; }
    case WM_DRAWITEM: {
        auto* item=reinterpret_cast<DRAWITEMSTRUCT*>(lp);if(item->CtlType!=ODT_BUTTON)break;RECT r=item->rcItem;
        const bool pressed=(item->itemState&ODS_SELECTED)!=0,hovered=d->hoverId==static_cast<int>(item->CtlID);
        const bool onBand=item->CtlID==ID_LOAD||item->CtlID==ID_SAVE||item->CtlID==ID_CLEAR||item->CtlID==ID_OK||item->CtlID==ID_CANCEL||(item->CtlID>=ID_BOLD&&item->CtlID<=ID_REDO)||(item->CtlID>=ID_COLOR0&&item->CtlID<ID_COLOR0+10);
        HBRUSH outside=CreateSolidBrush(onBand?RGB(8,23,36):kBg);FillRect(item->hDC,&r,outside);DeleteObject(outside);
        COLORREF fillColor=item->CtlID==ID_OK?(hovered?RGB(0,145,245):kAccent):(pressed?RGB(34,76,108):(hovered?RGB(24,58,83):kPanel));HBRUSH fill=CreateSolidBrush(fillColor);HPEN pen=CreatePen(PS_SOLID,1,(item->CtlID==ID_OK||hovered)?RGB(58,162,235):kBorder);HGDIOBJ oldBrush=SelectObject(item->hDC,fill),oldPen=SelectObject(item->hDC,pen);RoundRect(item->hDC,r.left,r.top,r.right,r.bottom,8,8);SelectObject(item->hDC,oldBrush);SelectObject(item->hDC,oldPen);DeleteObject(fill);DeleteObject(pen);
        wchar_t label[80]{};GetWindowTextW(item->hwndItem,label,80);SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,kText);SelectObject(item->hDC,d->font);
        if(item->CtlID>=ID_COLOR0&&item->CtlID<ID_COLOR0+10){int index=item->CtlID-ID_COLOR0;if(index){RECT sw=r;InflateRect(&sw,-8,-7);HBRUSH color=CreateSolidBrush(kColors[index]);HPEN swPen=CreatePen(PS_SOLID,1,RGB(108,137,158));HGDIOBJ ob=SelectObject(item->hDC,color),op=SelectObject(item->hDC,swPen);RoundRect(item->hDC,sw.left,sw.top,sw.right,sw.bottom,4,4);SelectObject(item->hDC,ob);SelectObject(item->hDC,op);DeleteObject(color);DeleteObject(swPen);}}
        else if(item->CtlID==ID_OK){RECT icon{r.left+12,r.top+6,r.left+32,r.top+26};HBRUSH iconBrush=CreateSolidBrush(RGB(45,190,85));HPEN iconPen=CreatePen(PS_SOLID,1,RGB(105,235,140));HGDIOBJ ob=SelectObject(item->hDC,iconBrush),op=SelectObject(item->hDC,iconPen);Ellipse(item->hDC,icon.left,icon.top,icon.right,icon.bottom);SelectObject(item->hDC,ob);SelectObject(item->hDC,op);DeleteObject(iconBrush);DeleteObject(iconPen);HPEN checkPen=CreatePen(PS_SOLID,2,RGB(255,255,255));HGDIOBJ old=SelectObject(item->hDC,checkPen);MoveToEx(item->hDC,icon.left+5,icon.top+10,nullptr);LineTo(item->hDC,icon.left+9,icon.top+14);LineTo(item->hDC,icon.right-4,icon.top+5);SelectObject(item->hDC,old);DeleteObject(checkPen);RECT textRect=r;textRect.left=r.left+40;DrawTextW(item->hDC,label,-1,&textRect,DT_LEFT|DT_VCENTER|DT_SINGLELINE);}
        else if(item->CtlID==ID_AUTOLOAD){RECT box{r.left+10,r.top+7,r.left+26,r.top+23};HBRUSH checkBg=CreateSolidBrush(RGB(8,22,34));FillRect(item->hDC,&box,checkBg);DeleteObject(checkBg);FrameRect(item->hDC,&box,GetSysColorBrush(COLOR_3DSHADOW));if(d->autoload){HPEN checkPen=CreatePen(PS_SOLID,2,RGB(77,190,255));HGDIOBJ old=SelectObject(item->hDC,checkPen);MoveToEx(item->hDC,box.left+3,box.top+8,nullptr);LineTo(item->hDC,box.left+7,box.bottom-3);LineTo(item->hDC,box.right-3,box.top+3);SelectObject(item->hDC,old);DeleteObject(checkPen);}RECT textRect=r;textRect.left=r.left+34;DrawTextW(item->hDC,label,-1,&textRect,DT_LEFT|DT_VCENTER|DT_SINGLELINE);}
        else DrawTextW(item->hDC,label,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);return TRUE;
    }
    case WM_COMMAND: switch(LOWORD(wp)) {
        case ID_BOLD: Toggle(d->editor,CFM_BOLD,CFE_BOLD); return 0; case ID_ITALIC: Toggle(d->editor,CFM_ITALIC,CFE_ITALIC); return 0; case ID_UNDERLINE: Toggle(d->editor,CFM_UNDERLINE,CFE_UNDERLINE); return 0;
        case ID_UNDO: SendMessageW(d->editor,EM_UNDO,0,0); return 0; case ID_REDO: SendMessageW(d->editor,EM_REDO,0,0); return 0;
        case ID_LOAD: LoadPrm(*d); return 0; case ID_SAVE: SavePrm(*d); return 0; case ID_CLEAR: SetWindowTextW(d->editor,L"");SetFocus(d->editor);return 0; case ID_AUTOLOAD: d->autoload=!d->autoload;InvalidateRect(GetDlgItem(w,ID_AUTOLOAD),nullptr,FALSE);return 0;
        case ID_OK: if(SaveDocument(*d)){Notify(*d,CHPT_RESULT_OK);DestroyWindow(w);} return 0; case ID_CANCEL: Notify(*d,CHPT_RESULT_CANCEL);DestroyWindow(w);return 0;
        default: if(LOWORD(wp)>=ID_COLOR0&&LOWORD(wp)<ID_COLOR0+10){SetColor(d->editor,LOWORD(wp)-ID_COLOR0);return 0;} break; } break;
    case WM_CLOSE: Notify(*d,CHPT_RESULT_CANCEL); DestroyWindow(w); return 0;
    case WM_DESTROY: RememberPromoWindowRect(w);if(d->font) DeleteObject(d->font);if(d->titleFont)DeleteObject(d->titleFont);if(d->iconSmall)DestroyIcon(d->iconSmall);if(d->iconLarge)DestroyIcon(d->iconLarge); { std::lock_guard<std::mutex> lock(gLock); gWindows.erase(w); } return 0;
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
extern "C" LONG __stdcall CHPT_OpenDesigner(HWND owner,CHPT_PROMO_DATA* p,HWND notify){LONG v=CHPT_ValidateData(p);if(v!=CHPT_STATUS_OK)return v;if(!IsWindow(owner)||!IsWindow(notify))return CHPT_ERROR_ARGUMENT;if(!EnsureClass())return CHPT_ERROR_WINDOW;DWORD instance{};{std::lock_guard<std::mutex> lock(gLock);if(!gWindows.empty())return CHPT_ERROR_ALREADY_OPEN;instance=gNext++;}auto data=std::make_unique<Data>();data->owner=owner;data->notify=notify;data->caller=p;data->instance=instance;data->original=p->text;p->inputLength=static_cast<DWORD>(data->original.size());p->outputLength=0;Data* raw=data.get();const RECT placement=InitialPromoWindowRect();HWND w=CreateWindowExW(WS_EX_APPWINDOW|WS_EX_CONTROLPARENT,kClassName,L"CompuHost V4 - Promo Trailer Designer",WS_OVERLAPPEDWINDOW|WS_VISIBLE,placement.left,placement.top,placement.right-placement.left,placement.bottom-placement.top,owner,nullptr,GetModuleHandleW(nullptr),raw);if(!w)return CHPT_ERROR_WINDOW;{std::lock_guard<std::mutex> lock(gLock);gWindows[w]=std::move(data);}p->instanceId=raw->instance;Debug(*raw,L"opened",p->inputLength);return static_cast<LONG>(raw->instance);}
extern "C" LONG __stdcall CHPT_ConsumeCompletion(HWND notify,DWORD* instance,LONG* result){if(!notify||!instance||!result)return FALSE;std::lock_guard<std::mutex> lock(gLock);auto it=gCompletions.find(notify);if(it==gCompletions.end()||it->second.empty())return FALSE;auto c=it->second.front();it->second.pop_front();*instance=c.instance;*result=c.result;if(it->second.empty())gCompletions.erase(it);return TRUE;}
