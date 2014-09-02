#include <windows.h>
#include "resource.h"
#include "commctrl.h"
#pragma comment(lib,"comctl32.lib")

HANDLE	hGlobalHeap=NULL;
HMODULE	hGlobalIns=NULL;
HICON	hGlobalIcon=NULL;
HMODULE hGlobalMod=NULL;
DWORD ProcMemSize=0;
DWORD ProcBaseAddr=0;
DWORD ProcEndAddr=0;

char SomeSaying[]=	"һ����Ҹ�ѧ���־����ϵ���������Ȼ����������ʵ����å\r\n�������ǰ������װϵͳ��װ��<ǧǧ���� V5.5.2 Build 0831>��\r\n"\
					"���边Ȼ�������½�ǿ�Ƶ���һ��FLASH��棬�������������\r\n����ѡ��û�з����ܹرյĵط�������������ж԰������ֻ��\r\n"\
					"���ᡣ\r\n\r\n"\
					"����ǧǧ������ΪʲôҲ��������å֮·������������������\r\n˵���������ǿ�ƹ�棬��Ҳ����д���ߣ�\r\n\r\n"\
					"�����������û���֪��������ǿ�Ƶ������͵�ȷ�е���ˡ�\r\n\r\n"\
					"����ʹ�ú��±��룬���ͼ�����ᴳ������ƣ��ٴθ�л�����\r\n�����ᴳ���ġ�";

LPVOID mymalloc(int size)//���ã�
{
	if(!hGlobalHeap)
		hGlobalHeap=GetProcessHeap();
	LPVOID mem=HeapAlloc(hGlobalHeap,HEAP_ZERO_MEMORY,size);
	if(!mem)
	{
		MessageBox(NULL,"�ڴ治��","error",MB_OK | MB_ICONERROR);
		ExitProcess(0);
	}
	return mem;
}

void GetParam()
{
	if(!hGlobalMod)
		hGlobalMod=GetModuleHandle(NULL);
	ProcBaseAddr=(DWORD)hGlobalMod;
	ProcMemSize=*(DWORD*)(*(DWORD*)(ProcBaseAddr+60)+80+ProcBaseAddr);//����pe��ʽ
	ProcEndAddr=ProcMemSize+ProcBaseAddr;
}

void myfree(void* mem)
{
	if((DWORD)mem <= 0xFFFF)
		return;
	if(!ProcMemSize)
		GetParam();
	if((DWORD)mem < ProcBaseAddr || (DWORD)mem > ProcEndAddr)
	{
		if(!IsBadReadPtr(mem,8))
			HeapFree(hGlobalHeap,0,mem);
	}
}

class ChangeData
{
public:
	virtual ~ChangeData(){};
	virtual ChangeData* CopySelf(ChangeData* src);
	virtual void HookItem(HWND hwndDlg,int nID);
	char*	UpdateText(char* str,BOOL Isset);//set or get

private:
	HWND	hwndDlg;
	int		nID;
};

ChangeData* ChangeData::CopySelf(ChangeData* src)
{
	ChangeData* temp=(ChangeData*)mymalloc(sizeof(ChangeData));
	memcpy(temp,src,sizeof(ChangeData));
	return temp;
}

void ChangeData::HookItem(HWND hwndDlg,int nID)
{
	this->hwndDlg=hwndDlg;
	this->nID=nID;
}

char* ChangeData::UpdateText(char* str,BOOL Isset)
{
	int cchMax=0;
	char* buf=NULL;
	if(Isset)
	{
		SetDlgItemText(hwndDlg,nID,str);
	}
	else
	{
		cchMax=1+SendDlgItemMessage(hwndDlg,nID,WM_GETTEXTLENGTH,0,0);
		buf=(char*)mymalloc(cchMax);
		GetDlgItemText(hwndDlg,nID,buf,cchMax);
	}
	return buf;
}

ChangeData GlobalData;


BOOL GotoVisit(char* str)
{
	if(ERROR_FILE_NOT_FOUND == (DWORD)ShellExecute(NULL,"open","explorer.exe",str,"",SW_SHOWNORMAL))
		return FALSE;
	return TRUE;
}

char* mygetenvironment(char* env)
{
	char* buf=(char*)mymalloc(260);
	DWORD size,originsize=260;
	while((size=GetEnvironmentVariable(env,buf,originsize)) > originsize)
	{
		myfree(buf);
		buf=(char*)mymalloc(size);
		originsize=size;
	}
	return buf;
}

char* mystrcat(char* buf1,char* buf2)
{
	int len[2]={strlen(buf1),strlen(buf2)};
	char* newp=(char*)mymalloc(len[0]+len[1]);
	strcpy(newp,buf1);
	strcat(newp+len[0],buf2);
	myfree(buf1);
	return newp;
}

int HasRemoved(char* str1,char* str2)
{//��str1�����Ӵ�str2     ����hosts�ļ������Ƿ����ӹ��Ǹ��ַ���
	//ԭ���߱���д�ıȽ���⣬�����������˴�����ָ��
	int len1=*(DWORD*)(str1+4)+8;
	int len2=strlen(str2);
	if(!len1 || !len2 || len2>len1)
		return -1;
	char* endpos=str1+len1-len2;
	char* beginpos=str1+8;
	while(strnicmp(beginpos,str2,len2))
	{//���û�ҵ�
		if(*beginpos >= 0)//����Ƿ�����ĸ����.

			beginpos++;
		else//����Ǻ���
			beginpos+=2;
		if(beginpos > endpos)
			return -1;
	}
	return beginpos-str1+1;
}

BOOL RemoveAdvertisement(char* filename,char* data)
{
	HANDLE hFile=CreateFile(filename,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_ARCHIVE,NULL);
	DWORD numofwrite;
	if(hFile != INVALID_HANDLE_VALUE)
	{
		BOOL ret=WriteFile(hFile,LPVOID(data+8),*(DWORD*)(data+4),&numofwrite,NULL);
		CloseHandle(hFile);
		return ret;
	}
	return FALSE;
}

void OnDoRemoveAdds()
{
	char* lpMem=mygetenvironment("Systemroot");
	lpMem=mystrcat(lpMem,"\\System32\\Drivers\\Etc\\hosts");
	HANDLE hFile=CreateFile(lpMem,GENERIC_READ,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,
		OPEN_EXISTING,FILE_ATTRIBUTE_ARCHIVE,NULL);
	BYTE* data=NULL;
	if(hFile != INVALID_HANDLE_VALUE)
	{
		DWORD size;
		DWORD filesizelow=GetFileSize(hFile,&size);
		data=(BYTE*)mymalloc(filesizelow+8+1);
		*(DWORD*)data=1;
		*(DWORD*)(data+4)=filesizelow;
		ReadFile(hFile,data+8,filesizelow,&size,NULL);
		data[filesizelow+8]='\0';
		CloseHandle(hFile);
	}
	char* filename=lpMem;
	lpMem=(char*)data;
	char* string1=	"\r\n------ ����ǧǧ������ʼ ------\r\n"
					"0.0.0.0   ttmsg.qianqian.com\r\n"
					"------ ����ǧǧ�������� ------\r\n";
	
	
	if(-1 == HasRemoved(lpMem,string1))
	{//���ûȥ���
		char* p1=(char*)mymalloc(*(DWORD*)(data+4)+8+strlen(string1)+1);
		memcpy(p1,lpMem,*(DWORD*)(data+4)+8);
		memcpy(p1+*(DWORD*)(data+4)+8,string1,strlen(string1)+1);
		*(DWORD*)(p1+4)=*(DWORD*)(data+4)+8+strlen(string1);
		BOOL ret=RemoveAdvertisement(filename,p1);
		if(p1)
			myfree(p1);
		if(ret)
		{
			HWND hwnd=GetActiveWindow();
			MessageBox(hwnd,"���γɹ������ڿ��Ծ��������޹���ǧǧ�����ˣ�","��ʾ",MB_OK | MB_ICONINFORMATION);
		}
		else
		{
			HWND hwnd=GetActiveWindow();
			MessageBox(hwnd,"����ʧ�ܣ�ԭ��δ֪��","��ʾ",MB_OK | MB_ICONWARNING);
		}
	}
	else
	{
		HWND hwnd=GetActiveWindow();
		MessageBox(hwnd,"����֮ǰ�Ѿ����ι�ǧǧ��������ˣ������ڼ���������","��ʾ",MB_OK | MB_ICONWARNING);	
	}

	if(lpMem)
		myfree(lpMem);
	if(data)
		myfree(data);
}

BOOL WINAPI DialogFunc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int nID=0;
	switch(uMsg)
	{
		case WM_INITDIALOG:
			SendMessage(hwndDlg,WM_SETICON,ICON_SMALL,(LPARAM)hGlobalIcon);
			SendMessage(hwndDlg,WM_SETICON,ICON_BIG,(LPARAM)hGlobalIcon);
			GlobalData.HookItem(hwndDlg,1002);
			char* ret;
			if(ret=GlobalData.UpdateText(SomeSaying,TRUE))
				myfree(ret);
			break;

		case WM_COMMAND:
			nID=LOWORD(wParam);
			if(nID == IDDOREMOVE)
				OnDoRemoveAdds();
			else if(nID == IDVISIT)
				GotoVisit("http://www.tudou8.cn");
			break;

		case WM_CLOSE:
			EndDialog(hwndDlg,0);
			break;

		case WM_SIZE:
			break;

		case WM_CTLCOLORSTATIC:
			{
				if(IDDESCRIB != GetDlgCtrlID((HWND)lParam))
					return FALSE;
				SetTextColor((HDC)wParam,RGB(255,0,0));
				SetBkMode((HDC)wParam,TRANSPARENT);
				return (BOOL)GetStockObject(NULL_BRUSH);//�����Ц�ˡ���������
			}
			break;

		default:
			return FALSE;
	}
	return TRUE;
}

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd )
{
	hGlobalIns=GetModuleHandle(NULL);
	hGlobalIcon=LoadIcon(hGlobalIns,MAKEINTRESOURCE(IDI_MAINICON));
	InitCommonControls();
	DialogBoxParam(hGlobalIns,MAKEINTRESOURCE(IDD_MAINDLG),NULL,(DLGPROC)DialogFunc,0);

	return 0;
}
