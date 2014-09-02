#include "stdafx.h"

#ifdef __cplusplus
extern "C" DRIVER_INITIALIZE DriverEntry;
//extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath);
#endif

NTSTATUS VCDRomDeleteDevice(PDEVICE_OBJECT pDevObj,PIRP pIrp);
NTSTATUS IoCommonComplete(NTSTATUS status,ULONG info,PIRP pIrp);
NTSTATUS VCDRomSetHardErrorOrVerifyDevice(PDEVICE_OBJECT pDevObj, PIRP pIrp);
VOID VCDRomGetTrackData(TRACK_DATA* pTrackData,ULONG Control,ULONG Adr,ULONG TrackNumber,ULONG AddressData);
NTSTATUS VCDRomIoControlCreate(PDRIVER_OBJECT DriverObject,PIRP pIrp);
NTSTATUS VCDRomCreateFile(PDEVICE_OBJECT pDevObj,PUNICODE_STRING FileName);
NTSTATUS VCDRomCreateDevice(PDRIVER_OBJECT DriverOjbect,int DriveName,PWSTR NameBuffer,PDEVICE_OBJECT* pOutpDevObj);

ULONG g_nVCDRomCount=0;
FAST_MUTEX g_VCDRomFastMutex={0};


ULONG VCDRomTrackDataAdress(ULONG InData)
{
	UCHAR data[4];
	data[0]=0;
	data[1]=UCHAR(InData/4500);
	data[2]=UCHAR((InData-4500*data[0])/75);
	data[3]=UCHAR(InData+108*data[1]-75*data[2]);
	return *(ULONG*)data;
}

NTSTATUS VCDRomReadTocEx(PDEVICE_OBJECT pDevObj,PIRP pIrp)
{
	PDEVICE_EXTENSION pDevExt=(PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	if(!pDevExt->FileObj)
	{
		return IoCommonComplete(STATUS_NO_MEDIA_IN_DEVICE,0,pIrp);
	}

	PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(pIrp);
	if((pDevObj->Flags & DO_VERIFY_VOLUME) && !(stack->Flags &  SL_OVERRIDE_VERIFY_VOLUME))
	{
		return VCDRomSetHardErrorOrVerifyDevice(pDevObj,pIrp);
	}
	if(stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(CDROM_READ_TOC_EX))
	{
		return IoCommonComplete(STATUS_INFO_LENGTH_MISMATCH,sizeof(CDROM_READ_TOC_EX),pIrp);
	}
	if(stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(CDROM_TOC))
	{
	}

	CDROM_READ_TOC_EX* tocex=(CDROM_READ_TOC_EX*)pIrp->AssociatedIrp.SystemBuffer;

	if(tocex->Reserved1 || tocex->Reserved2 || tocex->Reserved3 || tocex->Msf == 0)
		return IoCommonComplete(STATUS_INVALID_PARAMETER,0,pIrp);
	if(tocex->Format == CDROM_READ_TOC_EX_FORMAT_TOC || 
		tocex->Format == CDROM_READ_TOC_EX_FORMAT_FULL_TOC || 
		tocex->Format == CDROM_READ_TOC_EX_FORMAT_CDTEXT)
	{
	}
	else if(tocex->Format ==  CDROM_READ_TOC_EX_FORMAT_SESSION || 
			tocex->Format == CDROM_READ_TOC_EX_FORMAT_PMA || 
			tocex->Format == CDROM_READ_TOC_EX_FORMAT_ATIP)
	{
		if(tocex->SessionTrack)
		{
			return IoCommonComplete(STATUS_INVALID_PARAMETER,0,pIrp);
		}
	}
	else
	{
		return IoCommonComplete(STATUS_INVALID_PARAMETER,0,pIrp);
	}

	CDROM_TOC* toc=(CDROM_TOC*)pIrp->AssociatedIrp.SystemBuffer;
	toc->Length[0]=0;
	toc->Length[1]=sizeof(TRACK_DATA);
	toc->FirstTrack=1;
	toc->LastTrack=1;

	//������2�仰��ȫ����
	VCDRomGetTrackData(toc->TrackData,TOC_DATA_TRACK,0,1,150);
	VCDRomGetTrackData(toc->TrackData+1,TOC_DATA_TRACK,0,170,ULONG(pDevExt->FileLength>>pDevExt->SectorBits)+150);
	
	return IoCommonComplete(STATUS_SUCCESS,sizeof(CDROM_TOC),pIrp);
}

NTSTATUS VCDRomCreate(PDRIVER_OBJECT DriverObject,ULONG DriveIndex,PCWSTR FileName)
{
	UNICODE_STRING UFileName;
	WCHAR DriveName;
	PDEVICE_OBJECT pDevObj=NULL;
	RtlInitUnicodeString(&UFileName,FileName);
	VCDRomCreateDevice(DriverObject,DriveIndex,&DriveName,&pDevObj);
	VCDRomCreateFile(pDevObj,&UFileName);
	return STATUS_WAIT_1;
}

#pragma code_seg("PAGE")
NTSTATUS VCDRomQueryDevice(IN PDRIVER_OBJECT DriverObject, IN PCWSTR  RegistryPath)
{
	PAGED_CODE();
	NTSTATUS status;
	RTL_QUERY_REGISTRY_TABLE QueryTable[3];
	WCHAR wszFileName[100],wszQueryName[100],wszDriveName[2];;
	UNICODE_STRING FileName,QueryTableName;

	for(USHORT index='A';index <= 'Z';index++)
	{
		RtlZeroMemory(QueryTable,sizeof(QueryTable));
		RtlZeroMemory(wszQueryName,sizeof(wszQueryName));
		RtlInitEmptyUnicodeString(&QueryTableName,wszQueryName,sizeof(wszQueryName));
		RtlAppendUnicodeToString(&QueryTableName,L"Parameters\\Device");
		wszDriveName[0]=index;
		wszDriveName[1]=L'\0';//�����������ԣ�unicode�Ľ�����Ӧ����2��'\0'
		RtlAppendUnicodeToString(&QueryTableName,wszDriveName);
		RtlInitEmptyUnicodeString(&FileName,wszFileName,sizeof(wszFileName));
		QueryTable[0].Name=QueryTableName.Buffer;
		QueryTable[0].Flags=RTL_QUERY_REGISTRY_SUBKEY;
		QueryTable[1].Name=L"IMAGE";
		QueryTable[1].Flags=RTL_QUERY_REGISTRY_DIRECT;
		QueryTable[1].EntryContext=&FileName;//���ڴ洢��ѯ���
		status=RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE,RegistryPath,QueryTable,NULL,NULL);
		if(NT_SUCCESS(status))
			status=VCDRomCreate(DriverObject,index,FileName.Buffer);
	}

	return status;
}

VOID VCDRomUnload(IN PDRIVER_OBJECT DriverObject)
{
	PAGED_CODE();
	PDEVICE_OBJECT pDevObj=DriverObject->pDeviceObject;
	while(pDevObj)
	{
		IRP Irp;
		PDEVICE_OBJECT next=pDevObj->NextDevice;
		VCDRomDeleteDevice(pDevObj,&Irp);
		pDevObj=next;
	}
}

NTSTATUS VCDRomCreateClose(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
	PAGED_CODE();
	IoCommonComplete(STATUS_SUCCESS,0,pIrp);
	IoCompleteRequest(pIrp,IO_CD_ROM_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS IoCommonComplete(NTSTATUS status,ULONG info,PIRP pIrp)
{
	PAGED_CODE();
	pIrp->IoStatus.Information=info;
	return pIrp->IoStatus.Status=status;
}

NTSTATUS InitDeviceExtension(PDEVICE_OBJECT pDevObj)
{
	PAGED_CODE();
	PDEVICE_EXTENSION pDevExt=(PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	pDevExt->DeviceObj=pDevObj;
	pDevExt->FileObj=NULL;
	pDevExt->ChangedTime=0;
	RtlInitEmptyUnicodeString(&pDevExt->ustrSymLinkName,NULL,0);
	RtlInitEmptyUnicodeString(&pDevExt->FileName,NULL,0);

	return STATUS_SUCCESS;
}

NTSTATUS AllocateUnicodeString(ULONG MaximumLength,PUNICODE_STRING UnicodeString)
{
	PAGED_CODE();
	LPWSTR pStr=(LPWSTR)ExAllocatePoolWithTag(NonPagedPool,MaximumLength,' dCV');
	RtlInitEmptyUnicodeString(UnicodeString,pStr,(USHORT)MaximumLength);
	if(pStr)
		return STATUS_SUCCESS;
	else
		return STATUS_NO_MEMORY;
}

VOID FreeUnicodeString(PUNICODE_STRING UnicodeString)
{
	PAGED_CODE();
	if(UnicodeString->Buffer)
	{
		ExFreePool(UnicodeString->Buffer);
		UnicodeString->Buffer=NULL;
	}
	UnicodeString->Length=0;
	UnicodeString->MaximumLength=0;
}

VOID VCDRomGetTrackData(TRACK_DATA* pTrackData,ULONG Control,ULONG Adr,ULONG TrackNumber,ULONG AddressData)
{
	pTrackData->Reserved=0;
	pTrackData->Reserved1=0;
	pTrackData->Control=Control;
	pTrackData->Adr=Adr;
	pTrackData->TrackNumber=(UCHAR)TrackNumber;
	*(ULONG*)pTrackData->Address=VCDRomTrackDataAdress(AddressData);
}

//If the driver detects that the media has changed and the volume is mounted (VPD_MOUNTED is set in the VPB),
//it must: set Information to zero, set Status to STATUS_VERIFY_REQUIRED, set the DO_VERIFY_VOLUME flag in the pDevObj,
//and call IoCompleteRequest with the input pIrp.

//If the driver detects that the media has changed, but the volume is not mounted, the driver must not set 
//the DO_VERIFY_VOLUME bit. The driver should set Status to STATUS_IO_DEVICE_ERROR, set Information to zero, 
//and call IoCompleteRequest with the pIrp.
NTSTATUS VCDRomSetHardErrorOrVerifyDevice(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
	PAGED_CODE();
	pDevObj->Flags |= DO_VERIFY_VOLUME;
	pIrp->IoStatus.Information=0;
	pIrp->IoStatus.Status=STATUS_VERIFY_REQUIRED;
	IoSetHardErrorOrVerifyDevice(pIrp,pDevObj);
	return STATUS_VERIFY_REQUIRED;
}

NTSTATUS CallFileSystemDriver(PFILE_OBJECT FileObj,PMDL pMdl,PLONGLONG pulReadOffset,PRKEVENT Event,
	ULONG ulReadLength,PIO_STATUS_BLOCK pIoStatus)
{
	PAGED_CODE();
	PDEVICE_OBJECT pTopDevObj=IoGetRelatedDeviceObject(FileObj);
	PIRP pNewIrp=IoAllocateIrp(pTopDevObj->StackSize,FALSE);
	if(!pNewIrp)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	pNewIrp->RequestorMode=KernelMode;
	pNewIrp->UserIosb=pIoStatus;
	pNewIrp->MdlAddress=pMdl;
	pNewIrp->Flags=IRP_SYNCHRONOUS_PAGING_IO | IRP_PAGING_IO | IRP_NOCACHE;
	pNewIrp->UserEvent=Event;
	pNewIrp->UserBuffer=MmGetMdlVirtualAddress(pMdl);
	pNewIrp->Tail.Overlay.OriginalFileObject=FileObj;
	pNewIrp->Tail.Overlay.Thread=PsGetCurrentThread();

	PIO_STACK_LOCATION stack=IoGetNextIrpStackLocation(pNewIrp);
	stack->Parameters.Read.Length=ulReadLength;
	stack->MajorFunction=IRP_MJ_READ;
	stack->FileObject=FileObj;
	stack->Parameters.Read.ByteOffset.QuadPart=*pulReadOffset;

	return IoCallDriver(pTopDevObj,pNewIrp);
}

NTSTATUS VCDRomRead(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
	PAGED_CODE();
	NTSTATUS status=STATUS_INVALID_PARAMETER;
	PDEVICE_EXTENSION pDevExt=(PDEVICE_EXTENSION)pDevObj->DeviceExtension;

	__try
	{
		if(!pDevExt->FileObj)
		{
			return IoCommonComplete(STATUS_NO_MEDIA_IN_DEVICE,0,pIrp);
		}

		PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(pIrp);
		if((pDevObj->Flags & DO_VERIFY_VOLUME) && !(stack->Flags &  SL_OVERRIDE_VERIFY_VOLUME))
		{
			status=VCDRomSetHardErrorOrVerifyDevice(pDevObj,pIrp);
			__leave;
		}
		
		PLONGLONG pulReadOffset=&stack->Parameters.Read.ByteOffset.QuadPart;
		ULONG ulReadLength=stack->Parameters.Read.Length;
		if(ulReadLength > MmGetMdlByteCount(pIrp->MdlAddress))
		{
			status=IoCommonComplete(STATUS_BUFFER_TOO_SMALL,0,pIrp);
			__leave;
		}
		KEVENT Event;
		IO_STATUS_BLOCK iostatus;
		KeInitializeEvent(&Event,NotificationEvent,FALSE);
		status=CallFileSystemDriver(pDevExt->FileObj,pIrp->MdlAddress,pulReadOffset,&Event,
			ulReadLength,&iostatus);
		if(!NT_SUCCESS(status))
		{
			status=IoCommonComplete(status,0,pIrp);
			__leave;
		}

		status=KeWaitForSingleObject(&Event,Executive,KernelMode,FALSE,NULL);
		if(!NT_SUCCESS(status))
		{
			status=IoCommonComplete(status,0,pIrp);
			__leave;
		}
		if(*pulReadOffset/pDevExt->BytePerSec > 32 || !(pIrp->MdlAddress->MdlFlags & MDL_MAPPED_TO_SYSTEM_VA) ||
			!(pIrp->MdlAddress->MdlFlags & MDL_PAGES_LOCKED) || !(pIrp->MdlAddress->MdlFlags & MDL_ALLOCATED_FIXED_SIZE))
		{
			status=IoCommonComplete(status,ulReadLength,pIrp);
			__leave;
		}

		//������Щ��֪������ʲô�����Ǽ򵥵��ж�!!!
		UCHAR* mdl_address=(UCHAR*)MmGetMdlVirtualAddress(pIrp->MdlAddress);
		if(pDevExt->FileType & FILE_TYPE_UDF)
		{
			if(mdl_address[0] == 0 && mdl_address[1] == 'N' && mdl_address[2] == 'S' && mdl_address[3] == 'R' && 
					mdl_address[4] == '0')
				mdl_address[5]='0';
		}
		if(pDevExt->FileType & FILE_TYPE_ISO)
		{
			if(mdl_address[0] == 2 && mdl_address[1] == 'C' && mdl_address[2] == 'D' && mdl_address[3] == '0' &&
					mdl_address[4] == '0' && mdl_address[5] == '1')
				mdl_address[5]='0';
		}
	}
	__finally
	{
		IoCompleteRequest(pIrp,IO_CD_ROM_INCREMENT);
	}

	return status;
}

NTSTATUS VCDRomGetExistingDevice(PDEVICE_OBJECT pDevObj,PIRP pIrp)
{
	PAGED_CODE();
	if(IoGetCurrentIrpStackLocation(pIrp)->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(VCDROM_OPEN_DRIVE))//Դ����д����
	{
		VCDROM_OPEN_DRIVE* VCDData=(VCDROM_OPEN_DRIVE*)pIrp->AssociatedIrp.SystemBuffer;
		VCDData->Index=0;
		PDEVICE_OBJECT pCurDevObj=pDevObj->DriverObject->pDeviceObject;//�ɵ�ǰ�豸�����ȡ��һ���豸����
		while(pCurDevObj)
		{
			PDEVICE_EXTENSION pDevExt=(PDEVICE_EXTENSION)pCurDevObj->DeviceExtension;
			if(pDevExt->ustrSymLinkName.Length != RtlCompareMemory(pDevExt->ustrSymLinkName.Buffer,
				L"\\??\\VirtualCdRom",pDevExt->ustrSymLinkName.Length))
			{
				VCDData->DriveBuffer[VCDData->Index++]=pDevExt->ustrSymLinkName.Buffer[4];//�պ���\\??\\��ĵ�һ����ĸ
			}
			pCurDevObj=pCurDevObj->NextDevice;
		}
		return IoCommonComplete(STATUS_SUCCESS,sizeof(VCDROM_OPEN_DRIVE),pIrp);
	}
	else
	{
		return IoCommonComplete(STATUS_BUFFER_TOO_SMALL,sizeof(VCDROM_OPEN_DRIVE),pIrp);
	}
}

NTSTATUS VCDRomGetFileName(PDEVICE_OBJECT pDevObj,PIRP pIrp)
{
	PAGED_CODE();
	PDEVICE_EXTENSION pDevExt=(PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	NTSTATUS status;
	if(IoGetCurrentIrpStackLocation(pIrp)->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(VCDROM_GET_FILENAME))//Դ������һ��д����
	{
		PVCDROM_GET_FILENAME pFileName=(PVCDROM_GET_FILENAME)pIrp->AssociatedIrp.SystemBuffer;
		pFileName->Loaded=pDevExt->FileObj != NULL;
		if(pDevExt->FileName.Length)
		{
			pFileName->NameLength=pDevExt->FileName.Length;
			RtlCopyMemory(pFileName->FullPathFileName,pDevExt->FileName.Buffer,pDevExt->FileName.Length);
		}
		else
		{
			pFileName->NameLength=0;
		}

		status=STATUS_SUCCESS;
	}
	else
	{
		status=STATUS_BUFFER_TOO_SMALL;
	}

	return IoCommonComplete(status,sizeof(VCDROM_GET_FILENAME),pIrp);
}

NTSTATUS VCDRomCheckVerify(PDEVICE_OBJECT pDevObj,PIRP pIrp)
{
	PAGED_CODE();
	PDEVICE_EXTENSION pDevExt=(PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	if(!pDevExt->FileObj)
		return IoCommonComplete(STATUS_NO_MEDIA_IN_DEVICE,0,pIrp);

	PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(pIrp);
	if((pDevObj->Flags & DO_VERIFY_VOLUME) && !(stack->Flags &  SL_OVERRIDE_VERIFY_VOLUME))
		return IoCommonComplete(STATUS_VERIFY_REQUIRED,0,pIrp);

	PULONG pChangedTime=(PULONG)pIrp->AssociatedIrp.SystemBuffer;
	if(pChangedTime)
	{
		*pChangedTime=pDevExt->ChangedTime;
		IoCommonComplete(STATUS_SUCCESS,sizeof(ULONG),pIrp);
	}
	else
	{
		IoCommonComplete(STATUS_SUCCESS,0,pIrp);
	}

	return STATUS_SUCCESS;
}

//VCDRomLoadImage��VCDRomEjectMedia���Ӧ��һ�����ؾ����ļ���һ��ж�ؾ����ļ�
NTSTATUS VCDRomLoadImage(PDEVICE_OBJECT pDevObj,PUNICODE_STRING FileName,PIRP pIrp)
{
	PAGED_CODE();
	NTSTATUS status;
	if(((PDEVICE_EXTENSION)pDevObj->DeviceExtension)->FileObj)
		status=STATUS_DEVICE_NOT_READY;//����δж��
	else
		status=VCDRomCreateFile(pDevObj,FileName);//�豸�仯���ڴ˺��������ñ�־λ

	return IoCommonComplete(status,0,pIrp);
}

NTSTATUS VCDRomEjectMedia(PDEVICE_OBJECT pDevObj,PIRP pIrp)
{
	PAGED_CODE();
	pDevObj->Flags |= DO_VERIFY_VOLUME;//�豸���������ñ�־λ
	PDEVICE_EXTENSION pDevExt=(PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	if(pDevExt->FileObj)
	{
		ObDereferenceObject(pDevExt->FileObj);//�ͷ�FileObj
		pDevExt->ChangedTime++;
		pDevExt->FileObj=NULL;
	}

	return IoCommonComplete(STATUS_SUCCESS,0,pIrp);
}

NTSTATUS VCDRomGetDriveGeometry(PDEVICE_OBJECT pDevObj,PIRP pIrp)
{
	PAGED_CODE();
	PDEVICE_EXTENSION pDevExt=(PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	if(!pDevExt->FileObj)
		return IoCommonComplete(STATUS_NO_MEDIA_IN_DEVICE,0,pIrp);

	PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(pIrp);
	if((pDevObj->Flags & DO_VERIFY_VOLUME) && !(stack->Flags &  SL_OVERRIDE_VERIFY_VOLUME))
		return VCDRomSetHardErrorOrVerifyDevice(pDevObj,pIrp);

	if(stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(DISK_GEOMETRY))
		return IoCommonComplete(STATUS_BUFFER_TOO_SMALL,sizeof(DISK_GEOMETRY),pIrp);

	PDISK_GEOMETRY pDisk=(PDISK_GEOMETRY)pIrp->AssociatedIrp.SystemBuffer;
	pDisk->MediaType=RemovableMedia;
	pDisk->BytesPerSector=pDevExt->BytePerSec;
	pDisk->SectorsPerTrack=ULONG(pDevExt->FileLength >> pDevExt->SectorBits);
	pDisk->Cylinders.QuadPart=1;
	pDisk->TracksPerCylinder=1;

	return IoCommonComplete(STATUS_SUCCESS,sizeof(DISK_GEOMETRY),pIrp);
}

NTSTATUS VCDRomReadToc(PDEVICE_OBJECT pDevObj,PIRP pIrp)
{
	PAGED_CODE();
	PDEVICE_EXTENSION pDevExt=(PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	if(!pDevExt->FileObj)
		return IoCommonComplete(STATUS_NO_MEDIA_IN_DEVICE,0,pIrp);

	PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(pIrp);
	if((pDevObj->Flags & DO_VERIFY_VOLUME) && !(stack->Flags &  SL_OVERRIDE_VERIFY_VOLUME))
		return VCDRomSetHardErrorOrVerifyDevice(pDevObj,pIrp);

	if(stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(CDROM_TOC))
		return IoCommonComplete(STATUS_BUFFER_TOO_SMALL,sizeof(CDROM_TOC),pIrp);

	PCDROM_TOC pToc=(PCDROM_TOC)pIrp->AssociatedIrp.SystemBuffer;
	pToc->Length[0]=0;
	pToc->Length[1]=sizeof(TRACK_DATA);
	pToc->FirstTrack=1;
	pToc->LastTrack=1;
	VCDRomGetTrackData(pToc->TrackData,TOC_DATA_TRACK,0,1,150);
	VCDRomGetTrackData(pToc->TrackData+1,TOC_DATA_TRACK,0,170,ULONG(pDevExt->FileLength >> pDevExt->SectorBits)+150);

	return IoCommonComplete(STATUS_SUCCESS,sizeof(CDROM_TOC),pIrp);
}

NTSTATUS VCDRomGetLastSession(PDEVICE_OBJECT pDevObj,PIRP pIrp)
{
	PAGED_CODE();
	if(!((PDEVICE_EXTENSION)pDevObj->DeviceExtension)->FileObj)
		return IoCommonComplete(STATUS_NO_MEDIA_IN_DEVICE,0,pIrp);

	PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(pIrp);
	if((pDevObj->Flags & DO_VERIFY_VOLUME) && !(stack->Flags &  SL_OVERRIDE_VERIFY_VOLUME))
		return VCDRomSetHardErrorOrVerifyDevice(pDevObj,pIrp);

	if(stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(CDROM_TOC_SESSION_DATA))
		return IoCommonComplete(STATUS_BUFFER_TOO_SMALL,sizeof(CDROM_TOC_SESSION_DATA),pIrp);

	PCDROM_TOC_SESSION_DATA pToc=(PCDROM_TOC_SESSION_DATA)pIrp->AssociatedIrp.SystemBuffer;
	pToc->Length[0]=0;
	pToc->Length[1]=sizeof(TRACK_DATA);
	pToc->FirstCompleteSession=1;
	pToc->LastCompleteSession=1;
	VCDRomGetTrackData(pToc->TrackData,TOC_DATA_TRACK,0,1,150);
	return IoCommonComplete(STATUS_SUCCESS,sizeof(CDROM_TOC_SESSION_DATA),pIrp);
}

NTSTATUS VCDRomLoadMedia(PDEVICE_OBJECT pDevObj,PIRP pIrp)
{
	PAGED_CODE();
	UNICODE_STRING FileName;
	PDEVICE_EXTENSION pDevExt=(PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	NTSTATUS status=AllocateUnicodeString(pDevExt->FileName.MaximumLength,&FileName);
	if(NT_SUCCESS(status))
	{
		__try
		{
			RtlCopyUnicodeString(&FileName,&pDevExt->FileName);
			VCDRomLoadImage(pDevObj,&FileName,pIrp);
		}
		__finally
		{
			FreeUnicodeString(&FileName);
		}
	}

	return status;
}

NTSTATUS VCDRomDeviceControl(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{//����DeviceIoControl�ĵ���
	PAGED_CODE();
	NTSTATUS status;
	__try
	{
		PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(pIrp);
		PDEVICE_EXTENSION pDevExt=(PDEVICE_EXTENSION)pDevObj->DeviceExtension;
		switch(stack->Parameters.DeviceIoControl.IoControlCode)
		{
			case IOCTL_CDROM_CHECK_VERIFY:
			case IOCTL_DISK_CHECK_VERIFY:
			case IOCTL_STORAGE_CHECK_VERIFY:
				status=VCDRomCheckVerify(pDevObj,pIrp);//����豸�仯
				break;

			case IOCTL_CDROM_GET_FILENAME:
				status=VCDRomGetFileName(pDevObj,pIrp);//��ȡ�����ļ���
				break;

			case IOCTL_CDROM_CREATE_VIRTUALDEVICE:
				status=VCDRomIoControlCreate(pDevObj->DriverObject,pIrp);//�����������
				break;

			case IOCTL_CDROM_DELETE_VIRTUALDEVICE:
				status=VCDRomDeleteDevice(pDevObj,pIrp);//ɾ���������
				break;

			case IOCTL_CDROM_CDROM_LOADIMAGE:
				{
					PVCDROM_OPEN_FILE pOpenFile=(PVCDROM_OPEN_FILE)pIrp->AssociatedIrp.SystemBuffer;
					UNICODE_STRING FullFileName={pOpenFile->NameLength,255,pOpenFile->FullPathFileName};
					pDevExt->FileType=pOpenFile->FileType;
					status=VCDRomLoadImage(pDevObj,&FullFileName,pIrp);//���ع��̾���
				}
				break;

			case IOCTL_CDROM_GET_EXISTINGDEVICE:
				status=VCDRomGetExistingDevice(pDevObj,pIrp);//��ȡ������������̷�
				break;

			case IOCTL_CDROM_READ_TOC:
				status=VCDRomReadToc(pDevObj,pIrp);//????
				break;

			case IOCTL_CDROM_GET_LAST_SESSION:
				status=VCDRomGetLastSession(pDevObj,pIrp);//????
				break;

			case IOCTL_CDROM_GET_DRIVE_GEOMETRY:
			case IOCTL_DISK_GET_DRIVE_GEOMETRY:
				status=VCDRomGetDriveGeometry(pDevObj,pIrp);//��ȡ�豸����ṹ
				break;

			case IOCTL_CDROM_READ_TOC_EX:
				status=VCDRomReadTocEx(pDevObj,pIrp);//???
				break;

			case IOCTL_CDROM_EJECT_MEDIA:
			case IOCTL_STORAGE_EJECT_MEDIA:
				status=VCDRomEjectMedia(pDevObj,pIrp);//����������������������ļ�
				break;

			case IOCTL_CDROM_LOAD_MEDIA:
			case IOCTL_STORAGE_LOAD_MEDIA:
				if(pDevExt->FileName.Buffer == NULL || pDevExt->FileObj)//������
				{
					status=IoCommonComplete(STATUS_SUCCESS,0,pIrp);
				}
				else
				{
					status=VCDRomLoadMedia(pDevObj,pIrp);
				}

				break;

			default:
				status=IoCommonComplete(STATUS_INVALID_DEVICE_REQUEST,0,pIrp);
				break;
		}
	}
	__finally
	{
		if(!NT_SUCCESS(status))
		{
			if(	status == STATUS_DEVICE_NOT_READY || status == STATUS_IO_TIMEOUT || status == STATUS_MEDIA_WRITE_PROTECTED || 
					status == STATUS_NO_MEDIA_IN_DEVICE || status == STATUS_VERIFY_REQUIRED || status == STATUS_UNRECOGNIZED_MEDIA ||
					status == STATUS_WRONG_VOLUME)
				IoSetHardErrorOrVerifyDevice(pIrp,pDevObj);
		}
		IoCompleteRequest(pIrp,IO_CD_ROM_INCREMENT);
	}

	return status;
}

NTSTATUS VCDRomCreateDevice(PDRIVER_OBJECT DriverOjbect,int DriveName,PWSTR NameBuffer,PDEVICE_OBJECT* pOutpDevObj)
{//�����豸
	PAGED_CODE();
	PDEVICE_OBJECT pDevObj=NULL;
	NTSTATUS status=STATUS_INVALID_PARAMETER;
	UNICODE_STRING DeviceName,Num;

	__try
	{
		AllocateUnicodeString(50,&DeviceName);
		AllocateUnicodeString(50,&Num);
		*pOutpDevObj=NULL;
		ExAcquireFastMutex(&g_VCDRomFastMutex);
		status=RtlIntegerToUnicodeString(g_nVCDRomCount++,10,&Num);//�����豸����
		ExReleaseFastMutex(&g_VCDRomFastMutex);

		if(NT_SUCCESS(status))
		{
			status=RtlAppendUnicodeToString(&DeviceName,L"\\Device\\VirtualCdRom");
			if(NT_SUCCESS(status))
			{
				status=RtlAppendUnicodeStringToString(&DeviceName,&Num);//�ϳ��µ��豸�� ��VirtualCdRom2
				if(NT_SUCCESS(status))
				{
					PDEVICE_EXTENSION pDevExt;
					status=IoCreateDevice(DriverOjbect,sizeof(DEVICE_EXTENSION),&DeviceName,FILE_DEVICE_CD_ROM,
						FILE_REMOVABLE_MEDIA | FILE_READ_ONLY_DEVICE,FALSE,&pDevObj);//�����豸
					if(NT_SUCCESS(status))
					{
						pDevExt=(PDEVICE_EXTENSION)pDevObj->DeviceExtension;
						InitDeviceExtension(pDevObj);
						AllocateUnicodeString(50,&pDevExt->ustrSymLinkName);
			
						BOOLEAN findDriveAvail=FALSE;
						for(USHORT i=(USHORT)DriveName;!findDriveAvail;i--)
						{//��'Z'��'A'�����̷����������̷�
							if(i < 'A')
							{//26����ĸ�̷����Ѿ�����!!
								if(!findDriveAvail)
									status=STATUS_INSUFFICIENT_RESOURCES;
								break;
							}
							status=STATUS_INSUFFICIENT_RESOURCES;
							WCHAR temp[5];
							temp[0]=i;
							*(ULONG*)(temp+1)=L':';
							pDevExt->ustrSymLinkName.Length=0;
							pDevExt->FileType=FILE_TYPE_DEFAULT;
							status=RtlAppendUnicodeToString(&pDevExt->ustrSymLinkName,L"\\??\\");
							if(!NT_SUCCESS(status))
								break;
							status=RtlAppendUnicodeToString(&pDevExt->ustrSymLinkName,temp);//����ȫ���̷����ƣ���\\??\\c:
							if(!NT_SUCCESS(status))
								break;

							//�����������Ƿ��Ѿ�ʹ��
							HANDLE Handle;
							OBJECT_ATTRIBUTES ObjectAttributes;
							InitializeObjectAttributes(&ObjectAttributes,NULL,OBJ_OPENIF,&pDevExt->ustrSymLinkName,NULL);
							status=ZwOpenSymbolicLinkObject(&Handle,GENERIC_READ,&ObjectAttributes);
							if(NT_SUCCESS(status))
							{//������̷��Ѿ����ڣ����������
								ZwClose(Handle);
								status=STATUS_OBJECT_NAME_EXISTS;
							}
							else
							{
								status=IoCreateSymbolicLink(&pDevExt->ustrSymLinkName,&DeviceName);//�����̷������豸��������
								if(NT_SUCCESS(status))
								{
									*NameBuffer=i;
									pDevObj->Flags &= ~DO_DEVICE_INITIALIZING;
									pDevObj->Flags |= DO_DIRECT_IO;
									*pOutpDevObj=pDevObj;
									findDriveAvail=TRUE;
								}
							}
						}
					}
					else
					{
						pDevObj=NULL;
					}
				}
			}
		}
	}
	__finally
	{
		FreeUnicodeString(&DeviceName);
		FreeUnicodeString(&Num);
		if(!NT_SUCCESS(status) && status != STATUS_VERIFY_REQUIRED)
		{
			if(pDevObj)
				IoDeleteDevice(pDevObj);
		}
	}

	return STATUS_SUCCESS;
}

NTSTATUS VCDRomDeleteDevice(PDEVICE_OBJECT pDevObj,PIRP pIrp)
{//ж���豸
	PAGED_CODE();
	PDEVICE_EXTENSION pDevExt=(PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	NTSTATUS status=IoDeleteSymbolicLink(&pDevExt->ustrSymLinkName);//ɾ���̷�
	FreeUnicodeString(&pDevExt->ustrSymLinkName);
	if(pDevExt->FileObj)
	{
		ObDereferenceObject(pDevExt->FileObj);
		pDevExt->FileObj=NULL;
	}
	FreeUnicodeString(&pDevExt->FileName);
	IoDeleteDevice(pDevObj);//ɾ���豸
	return IoCommonComplete(status,0,pIrp);
}

NTSTATUS VCDRomIoControlCreate(PDRIVER_OBJECT DriverObject,PIRP pIrp)
{//���������̷��������������̷�
	PAGED_CODE();
	NTSTATUS status;
	if(IoGetCurrentIrpStackLocation(pIrp)->Parameters.DeviceIoControl.OutputBufferLength >= 2)
	{//���������ص��̷���Ҫ��װ��WCHAR��С������
		PDEVICE_OBJECT pDevObj;
		//���������̷�
		status=VCDRomCreateDevice(DriverObject,'Z',(PWSTR)pIrp->AssociatedIrp.SystemBuffer,&pDevObj);
	}
	else
	{
		status=STATUS_BUFFER_TOO_SMALL;
	}
	return IoCommonComplete(status,2,pIrp);
}

NTSTATUS VCDRomCreateFile(PDEVICE_OBJECT pDevObj,PUNICODE_STRING FileName)
{//���ļ���ȡ��Ϣ
	PDEVICE_EXTENSION pDevExt=(PDEVICE_EXTENSION)pDevObj->DeviceExtension;
	OBJECT_ATTRIBUTES ObjectAttributes;
	InitializeObjectAttributes(&ObjectAttributes,FileName,OBJ_CASE_INSENSITIVE,NULL,NULL);
	HANDLE hFile=NULL;
	IO_STATUS_BLOCK IoStatusBlock;
	//�򿪹��̾����ļ�
	NTSTATUS status=ZwCreateFile(&hFile,FILE_READ_DATA | SYNCHRONIZE,&ObjectAttributes,&IoStatusBlock,
		NULL,FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ,FILE_OPEN,FILE_SYNCHRONOUS_IO_NONALERT,NULL,0);
	if(!NT_SUCCESS(status))
	{
		return status;
	}
	//��ȡ�ļ���Ϣ
	FILE_STANDARD_INFORMATION FileInformation;
	status=ZwQueryInformationFile(hFile,&IoStatusBlock,&FileInformation,sizeof(FileInformation),FileStandardInformation);
	if(!NT_SUCCESS(status))
	{
		ZwClose(hFile);
		return status;
	}

	ULONG Buffer[2];
	pDevExt->FileLength=FileInformation.EndOfFile.QuadPart;
	//BytePerSec����Ϊ0x200�ı�����Buffer[1]ΪҪ���õ�������С
	if(!NT_SUCCESS(ZwReadFile(hFile,NULL,NULL,NULL,&IoStatusBlock,Buffer,8,NULL,NULL)) || Buffer[1] == 0 || (Buffer[1] & 0x1FF))
		pDevExt->BytePerSec=2048;//���������������������512�ı���������������СΪ2048
	else
		pDevExt->BytePerSec=Buffer[1];//����ΪԤ��

	ULONG Number,Count;
	for(Number=2^9,Count=9;Number != pDevExt->BytePerSec;Count++)
	{
		if(Number >= pDevExt->BytePerSec)
			break;
		Number*=2;
	}

	//����SectorBits
	if(Number == pDevExt->BytePerSec)
	{
		pDevExt->SectorBits=Count;
	}
	else
	{
		pDevExt->SectorBits=11;
	}

	//�ɾ����ȡ�ļ�ָ��
	status=ObReferenceObjectByHandle(hFile,0,NULL,KernelMode,(PVOID*)&pDevExt->FileObj,NULL);
	if(!NT_SUCCESS(status))
	{
		ZwClose(hFile);
		pDevExt->FileObj=NULL;
		return status;
	}

	ZwClose(hFile);//����ObReferenceObjectByHandle��FileObj��δ�ͷţ���Ҫ��ObDereferenceObject�ͷ�
	pDevExt->ChangedTime++;//�������״̬�仯����
	pDevObj->Flags |= DO_VERIFY_VOLUME;//һ���豸�仯����λ
	//����3���滻DEVICE_EXTENSION�еľ��ļ���
	FreeUnicodeString(&pDevExt->FileName);
	AllocateUnicodeString(FileName->MaximumLength,&pDevExt->FileName);
	RtlCopyUnicodeString(&pDevExt->FileName,FileName);
	return STATUS_SUCCESS;
}

#pragma code_seg("INIT")
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath)
{
	UNICODE_STRING DeviceName;
	PDEVICE_OBJECT pDevObj = NULL;
	NTSTATUS status=STATUS_SUCCESS;

	__try
	{
		g_nVCDRomCount=0;//ȫ�ֱ������ڴ洢�����豸�ĸ��� (ȫ�ֱ���Ϊɶ���ŵ�DEVICE_EXTENSION����?)

		DriverObject->DriverUnload=VCDRomUnload;//ж���豸��ǲ����
		DriverObject->MajorFunction[IRP_MJ_CREATE]=VCDRomCreateClose;//�����豸���ر��豸��ǲ����
		DriverObject->MajorFunction[IRP_MJ_CLOSE]=VCDRomCreateClose;
		DriverObject->MajorFunction[IRP_MJ_CLEANUP]=VCDRomCreateClose;
		DriverObject->MajorFunction[IRP_MJ_READ]=VCDRomRead;//��ȡ�豸��ǲ����
		DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]=VCDRomDeviceControl;//�����豸��ǲ����

		RtlInitUnicodeString(&DeviceName,L"\\Device\\VirtualCdRom");
		status = IoCreateDevice(DriverObject,sizeof(DEVICE_EXTENSION),&DeviceName,FILE_DEVICE_CD_ROM,
			FILE_REMOVABLE_MEDIA | FILE_READ_ONLY_DEVICE,FALSE,&pDevObj);//�����豸
		if (!NT_SUCCESS(status))
		{
			pDevObj=NULL;
		}
		else
		{
			InitDeviceExtension(pDevObj);//��ʼ��DEVICE_EXTENSION�ṹ
			PUNICODE_STRING pde=&((PDEVICE_EXTENSION)pDevObj->DeviceExtension)->ustrSymLinkName;
			AllocateUnicodeString(50,pde);
			RtlAppendUnicodeToString(pde,L"\\??\\VirtualCdRom");
			status=IoCreateSymbolicLink(pde,&DeviceName);//������������
			if(NT_SUCCESS(status))
			{
				ExInitializeFastMutex(&g_VCDRomFastMutex);//��ʼ�����ٻ�����
				VCDRomQueryDevice(DriverObject,RegistryPath->Buffer);//��ѯisoʹ�ü�¼
			}	
		}
	}
	__finally
	{
		if(!NT_SUCCESS(status))
		{
			if(pDevObj)
			{//�����ϲ�������������ɾ���豸
				IoDeleteDevice(pDevObj);
			}
		}
	}

	return status;
}
