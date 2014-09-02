#include <windows.h>
#pragma comment(lib,"ole32.lib")
extern "C" HRESULT WINAPI CoRegisterSurrogateEx(REFGUID rguidProcessID,void* reserved);
//������ԭʼ��������������Ӧc�����Ż������ή��Ӱ���������Ч��
int WINAPI GetCommandLineArguments(LPCSTR lpString,char** matrix,int MatrixWidth,int MatrixLength)
{//��δ��뾭������������֪���ǽ�lpString�е��ַ����Կո�Ϊ�ֽ�ֳ����ַ��������ַ�������matrix��
//matrix��СΪMarixWidth*MatrixLength
//����΢���ڲ��Ĵ��붼�Ǽ��õģ��ʺ��������������������������ֱ���ô˼��Ĵ��룬����Ĵ�ҿ��Է����������õط���������
	LPCSTR curpos=lpString;
	int len=lstrlenA(lpString);
	int i=0,j=0,somenum=0;
	if(len>0)
	{
		while(somenum<len)
		{
			if(j>MatrixLength) return 0;
			char curchar=*curpos;
			curpos++;
			if(curchar == ' ')
			{
				if(j!=0)
				{
					matrix[i][j]=0;
					i++;
					j=0;
					if(i == MatrixWidth) return i;
				}
			}
			else 
			{
				matrix[i][j]=curchar;
				j++;
			}
			somenum++;
		}

		if(j!=0) matrix[i][j]=0;
		i++;
	}
	return i;
}

int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nShowCmd)
{
#define WIDTH	1
#define LENGTH	260
	char command[LENGTH];

	if(GetCommandLineArguments(lpCmdLine,(char**)&command,WIDTH,LENGTH) < 1)
		return 0;

	char* lpMultiByteStr=command;
	if(command[0] != '\0')
	{
		while(*lpMultiByteStr != ':')
		{
			lpMultiByteStr++;
			if(*lpMultiByteStr == '\0') break;
		}
		if(*lpMultiByteStr != '\0')
		{
			*lpMultiByteStr = '\0';
			lpMultiByteStr++;
		}
		if(*lpMultiByteStr == '\0' || lstrcmpi(command,"/ProcessID")!=0)
			lpMultiByteStr=command;
	}


	WCHAR WideCharStr[41];
	CLSID clsid;
	if(MultiByteToWideChar(CP_ACP,0,lpMultiByteStr,lstrlen(lpMultiByteStr)+1,WideCharStr,41) !=0 &&
		CLSIDFromString(WideCharStr,&clsid) >= 0 && CoInitializeEx(NULL,COINIT_MULTITHREADED) >=0 )
	{
		CoRegisterSurrogateEx(clsid,NULL);
		CoUninitialize();
		TerminateProcess(GetCurrentProcess(),0);
	}
	return 0;
}