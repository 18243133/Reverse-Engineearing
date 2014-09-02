#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#ifdef __cplusplus
extern "C" 
{
#endif

#include <ntddk.h>
#include <ntddstor.h>
#include <mountdev.h>
#include <ntddvol.h>
#include <ntddcdrm.h>
#include <ntdddisk.h>
#include <ntddscsi.h>
//#include <ntifs.h>

#ifdef __cplusplus
}
#endif

#pragma pack(push,1)

typedef struct _DEVICE_EXTENSION 
{//sizeof=48
	PDEVICE_OBJECT	DeviceObj;//�豸����
	PFILE_OBJECT	FileObj;//�ļ�����
	ULONGLONG		FileLength;//�ļ�����
	ULONG			ChangedTime;//�������״̬�ı����
	UNICODE_STRING	ustrSymLinkName;//����������
	UNICODE_STRING	FileName;//���̾����ļ���
	ULONG			FileType;//0 1:UDF 2:ISO
	ULONG			BytePerSec;//������С
	ULONG			SectorBits;//������С������BytePerSec=2^SectorBits
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

typedef struct _VCDROM_OPEN_DRIVE //sizeof=54
{
	USHORT 		Index;//�̷����
	WCHAR 		DriveBuffer[26];
} VCDROM_OPEN_DRIVE, *PVCDROM_OPEN_DRIVE;

typedef struct _VCDROM_OPEN_FILE //sizeof=516
{
	WCHAR  		FullPathFileName[255];
	USHORT 		NameLength;//in byte
	ULONG 		FileType;//0 1:UDF 2:ISO
} VCDROM_OPEN_FILE, *PVCDROM_OPEN_FILE;

typedef struct _VCDROM_GET_FILENAME //sizeof=514
{
	WCHAR  		FullPathFileName[255];
	USHORT 		NameLength;//in byte
	USHORT 		Loaded;//�Ƿ���ع��̾����ļ�
} VCDROM_GET_FILENAME, *PVCDROM_GET_FILENAME;

#pragma pack(pop)

#define FILE_TYPE_DEFAULT	0x0//Ĭ�ϸ�ʽ
#define FILE_TYPE_UDF		0x1//UDF��ʽ
#define FILE_TYPE_ISO		0x2//ISO��ʽ

#define SECTOR_SIZE			0x200//Ĭ��������С
#define TOC_DATA_TRACK		0x04

#define IOCTL_CDROM_CREATE_VIRTUALDEVICE	CTL_CODE(FILE_DEVICE_CD_ROM,0xCC0,METHOD_BUFFERED,FILE_ANY_ACCESS)//�����������
#define IOCTL_CDROM_DELETE_VIRTUALDEVICE	CTL_CODE(FILE_DEVICE_CD_ROM,0xCC1,METHOD_BUFFERED,FILE_ANY_ACCESS)//ɾ���������
#define IOCTL_CDROM_CDROM_LOADIMAGE			CTL_CODE(FILE_DEVICE_CD_ROM,0xCC2,METHOD_BUFFERED,FILE_ANY_ACCESS)//���ع��̾���
#define IOCTL_CDROM_GET_EXISTINGDEVICE		CTL_CODE(FILE_DEVICE_CD_ROM,0xCC3,METHOD_BUFFERED,FILE_ANY_ACCESS)//��ȡ����������������̷�
#define IOCTL_CDROM_GET_FILENAME			CTL_CODE(FILE_DEVICE_CD_ROM,0xCC4,METHOD_BUFFERED,FILE_ANY_ACCESS)//��ȡ����������Ѿ����ص�ȫ·���ļ���