// LODCTR:���������ܼ�������ص�ע���ֵ��
// 
// �÷�:
// LODCTR <INI-FileName>
// 	INI-FileName ���������ĵĳ�ʼ�ļ������Ƽ����� DLL �ļ��������ƶ����˵�����֡�
// 
// 	LODCTR /S:<Backup-FileName> ����ǰ�� perf ע����ַ�������Ϣ���浽 <Backup-FileName>
// 
// 	LODCTR /R:<Backup-FileName> ʹ�� <Backup-FileName> ��ԭ perf ע����ַ�������Ϣ
// 
// 	LODCTR /R ���ڵ�ǰ�����ͷ��ʼ�ؽ� perf ע����ַ�������Ϣע������úͱ��� INI �ļ���
// 
// 	LODCTR /T:<Service-Name> �����ܼ�������������Ϊ�����Ρ�
// 
// 	LODCTR /E:<Service-Name> �������ܼ���������
// 
// 	LODCTR /D:<Service-Name> �������ܼ���������
// 
// 	LODCTR /Q
// 
// 	LODCTR /Q:<Service-Name> ͨ����ѯȫ�����������߲�ѯָ����һ������������ѯ���ܼ�����������Ϣ��
// 
// 	LODCTR /M:<Counter-Manifest> ��װ Windows Vista ���ܼ������ṩ������ XML �ļ���ϵͳ֪ʶ�⡣
// 
// ע��: �κ������д��ո�Ĳ������������˫���š�

#include <windows.h>
#include <loadperf.h>

#pragma comment(lib,"loadperf.lib")

char Buffer[520];
WCHAR szCommentString[2]=L"\0";

char* __fastcall restoreinfo(char* command)
{
	char* dest=NULL;

	if((command[0] == '-' || command[0] == '/') && (command[1] == 'r' || command[1] == 'R') && (command[2] == ':'))
	{//LODCTR /R:<Backup-FileName> ʹ�� <Backup-FileName> ��ԭ perf ע����ַ�������Ϣ
		if(!SearchPath(NULL,&command[3],NULL,sizeof(Buffer),Buffer,NULL))//û�ѵ�
			dest=Buffer;
		else
			dest=&command[3];		
	}
	return dest;
}

char* __fastcall unknownsub3(char* command)
{
	char* dest=NULL;

	if((command[0] == '-' || command[0] == '/') && (command[1] == 's' || command[1] == 'S') && (command[2] == ':'))
	{//LODCTR /S:<Backup-FileName> ����ǰ�� perf ע����ַ�������Ϣ���浽 <Backup-FileName>
		if(!SearchPath(NULL,&command[3],NULL,sizeof(Buffer),Buffer,NULL))
			dest=Buffer;
		else
			dest=&command[3];
	}
	return dest;
}

bool WINAPI saveinfo(char** argv,LPSTR* szNewCtrlFilePath,LPSTR* szNewHlpFilePath,LPSTR* szLanguageID)
{
	DWORD i,FLAG;
	for(i=1,FLAG=0;i<=3;i++)
	{
		if(argv[i][0] == '-' || argv[i][0] == '/')
		{
			switch(argv[i][1])
			{
				case 'c':
				case 'C':
					if(argv[i][2] == ':')
					{
						*szNewCtrlFilePath=&argv[i][3];
						FLAG |= 1;
					}
					break;

				case 'h':
				case 'H':
					if(argv[i][2] == ':')
					{
						*szNewHlpFilePath=&argv[i][3];
						FLAG |=2;
					}
					break;

				case 'l':
				case 'L':
					if(argv[i][2] == ':')
					{
						*szLanguageID=&argv[i][3];
						FLAG |=4;
					}
					break;

				default:
					break;
			}
		}
	}
	return FLAG == (1|2|4);
}



DWORD main(int argc,char** argv)
{
	LPSTR szNewCtrlFilePath,szNewHlpFilePath;

	if(argc >= 2)
	{
		if(argc >= 4)
		{
			szNewCtrlFilePath=NULL;
			szNewHlpFilePath=NULL;
			argc=0;
			LPSTR szLanguageID;
			if(saveinfo(argv,&szNewCtrlFilePath,&szNewHlpFilePath,&szLanguageID))
				return UpdatePerfNameFiles(szNewCtrlFilePath,szNewHlpFilePath,szLanguageID,0);
		}
		else//if(argc == 3)
		{
			DWORD DATA;
			szNewHlpFilePath=unknownsub3(argv[1]);
			if(szNewHlpFilePath)
			{
				int len=lstrlen(szNewHlpFilePath)+1;
				WCHAR* pchar=(WCHAR*)HeapAlloc(GetProcessHeap(),0,2*len);
				if(pchar)
				{
					mbstowcs(pchar,szNewHlpFilePath,len);
					DATA=BackupPerfRegistryToFileW(pchar,szCommentString);
					HeapFree(GetProcessHeap(),0,pchar);
					return DATA;
				}
			}

			szNewHlpFilePath=restoreinfo(argv[1]);
			if(szNewHlpFilePath)
			{
				int len=lstrlen(szNewHlpFilePath)+1;
				WCHAR* pchar=(WCHAR*)HeapAlloc(GetProcessHeap(),0,2*len);
				if(pchar)
				{
					mbstowcs(pchar,szNewHlpFilePath,len);
					DATA=RestorePerfRegistryFromFileW(pchar,NULL);
					HeapFree(GetProcessHeap(),0,pchar);
					return DATA;
				}
			}

			if((argv[1][0] == '-' || argv[1][0] == '/') && (argv[1][1] == 't' || argv[1][1] == 'T') && (argv[1][2] == ':'))
			{
				char* ptr=&argv[1][3];
				if(ptr)
				{
					int len=lstrlen(ptr)+1;
					WCHAR* pchar=(WCHAR*)HeapAlloc(GetProcessHeap(),0,2*len);
					if(pchar)
					{
						mbstowcs(pchar,ptr,len);
						DATA=SetServiceAsTrustedW(NULL,pchar);
						HeapFree(GetProcessHeap(),0,pchar);
						return DATA;
					}
				}
			}
		}
	}

	LoadPerfCounterTextStringsW(GetCommandLineW(),FALSE);
	return 0;
}
