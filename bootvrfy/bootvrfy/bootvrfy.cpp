// ������ƹ�������һ��RPC ������������¶��һ��Ӧ�ñ�̽ӿڣ�����Ա���Է���ı�д����������
// ����Ϳ���Զ�̷������з������
// �������ͨ����д�ɿ���̨���͵�Ӧ�ó����ܵ���˵��һ�����ط�����ƹ������ӿ�Ҫ��ĳ���
// 
// ������������������
// 1�����������������main��������ϵͳ���� StartServiceCtrlDispatcher ���ӳ������̵߳�������ƹ������
// 2��������ڵ㺯����ServiceMain����ִ�з����ʼ������ͬʱִ�ж������ķ�������ж��������ں�����
// 3�����Ʒ������������Handler�����ڷ�������յ���������ʱ�ɿ��Ʒַ��߳����á����˴���Service_Ctrl����
// ������ϵͳ���д˷���֮ǰ��Ҫ��װ�ǼǷ������installService ������ɾ�������������Ҫ��ɾ������װ�Ǽǣ�removeService ������
#include <windows.h>
#include <winsvc.h>

SERVICE_STATUS_HANDLE BootVerificationStatusHandle=NULL;
SERVICE_STATUS BootVerificationStatus={0,0,0,0,0,0,0};
HANDLE BootVerificationDoneEvnet=NULL;

void WINAPI HandlerProc(DWORD dwControl)//���Ʒ����������
{
	if(dwControl == SERVICE_CONTROL_STOP)
	{
		BootVerificationStatus.dwWin32ExitCode=0;
		BootVerificationStatus.dwCurrentState=SERVICE_STOP_PENDING;//The service is stopping.
		SetEvent(BootVerificationDoneEvnet);//ʹ�¼������ź�
	}
	if(!SetServiceStatus(BootVerificationStatusHandle,&BootVerificationStatus))
		GetLastError();
}

void WINAPI ServiceProc(DWORD   dwNumServicesArgs,LPWSTR  *lpServiceArgVectors)//������ڵ㺯��
{
	SERVICE_STATUS ServiceStatus;

	BootVerificationDoneEvnet=CreateEvent(NULL,TRUE,FALSE,NULL);//��ʼ�����ź��¼�
	BootVerificationStatus.dwServiceType=SERVICE_WIN32;
	BootVerificationStatus.dwCurrentState=SERVICE_RUNNING;
	BootVerificationStatus.dwControlsAccepted=SERVICE_ACCEPT_STOP;
	//The service can be stopped. This control code allows the service to receive SERVICE_CONTROL_STOP notifications.
	BootVerificationStatus.dwWin32ExitCode=0;
	BootVerificationStatus.dwServiceSpecificExitCode=0;
	BootVerificationStatus.dwCheckPoint=0;
	// This value is not valid and should be zero when the service does not have a start, stop, pause, or continue operation pending
	BootVerificationStatus.dwWaitHint=0;
	BootVerificationStatusHandle=RegisterServiceCtrlHandlerW(L"BootVerification",HandlerProc);//ע����Ʒ����������
	if(!SetServiceStatus(BootVerificationStatusHandle,&BootVerificationStatus))//���÷���״̬
		GetLastError();
	NotifyBootConfigStatus(TRUE);//this function reports the boot status to the service control manager
	SC_HANDLE scm=OpenSCManagerW(NULL,NULL,SC_MANAGER_CONNECT);//Enables connecting to the service control manager.
	if(scm != NULL)
	{
		SC_HANDLE service=OpenServiceW(scm,L"BootVerification",SERVICE_STOP);
		if(service != NULL && ControlService(service,SERVICE_CONTROL_STOP,&ServiceStatus) != NULL/*sends a control code to a service.*/ )
		{
			WaitForSingleObject(BootVerificationDoneEvnet,INFINITE);//�ȴ��¼������ź�
			BootVerificationStatus.dwWin32ExitCode=0;
			BootVerificationStatus.dwCurrentState=SERVICE_STOPPED;
			if(!SetServiceStatus(BootVerificationStatusHandle,&BootVerificationStatus))
				GetLastError();
			ExitThread(0);
		}
	}

	BootVerificationStatus.dwWin32ExitCode=GetLastError();
	SetServiceStatus(BootVerificationStatusHandle,&BootVerificationStatus);
	ExitProcess(0);
}

void main()//�������������
{
	SERVICE_TABLE_ENTRYW ServiceStartTable[2];

	ServiceStartTable[1].lpServiceName=NULL;//��ʾTABLE_ENTRY����
	ServiceStartTable[1].lpServiceProc=NULL;//��ʾTABLE_ENTRY����
	ServiceStartTable[0].lpServiceName=L"BootVerification";//��������
	ServiceStartTable[0].lpServiceProc=ServiceProc;//������ں���
	int nret=StartServiceCtrlDispatcherW(ServiceStartTable);//��service control managerע��˷���
	nret=GetLastError();
	ExitProcess(0);
}
